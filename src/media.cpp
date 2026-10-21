/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013 Andrew Mickelson
 *
 *  This file is part of Attract-Mode.
 *
 *  Attract-Mode is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Attract-Mode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Attract-Mode.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "media.hpp"
#include "zip.hpp"
#include "fe_base.hpp"
#include "fe_file.hpp"
#include "fe_present.hpp"
#include "fe_audio_fx.hpp"
#include "fe_sdl3_gpu.hpp"
#include "fe_time.hpp"
#include <SDL3/SDL_thread.h>


extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>

#define FE_HWACCEL ( LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( 55, 78, 100 ))

#if FE_HWACCEL
#include <libavutil/hwcontext.h>
#endif
}

#include <queue>
#include <array>
#include <deque>
#include <chrono>
#include <limits>
#include <iostream>
#include <thread>
#include <condition_variable>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

#if ( LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT( 59, 0, 100 ))
typedef const AVCodec FeAVCodec;
#else
typedef AVCodec FeAVCodec;
#endif

#if ( LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT( 58, 7, 100 ))
  #define FORMAT_CTX_URL m_imp->m_format_ctx->url
#else
  #define FORMAT_CTX_URL m_imp->m_format_ctx->filename
#endif

#define HAVE_CH_LAYOUT ( LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( 57, 28, 100 ))
#define HAVE_DURATION ( LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( 58, 2, 100 ))

void try_hw_accel( AVCodecContext *&codec_ctx, FeAVCodec *&dec );

namespace
{
	constexpr int FE_MEDIA_AUDIO_QUEUE_TARGET_FRAMES = 4800;
	constexpr int FE_MEDIA_AUDIO_QUEUE_LOW_FRAMES = 2400;
	constexpr int FE_MEDIA_AUDIO_PROCESS_CHUNK_FRAMES = 480;

	// Producers keep independent I/O and deadlines, but simultaneous codec work
	// is bounded by CPU capacity. Audio refills take priority over video work.
	class FeMediaDecodeLock
	{
		static std::mutex mutex;
		static std::condition_variable audio_ready;
		static std::condition_variable video_ready;
		static unsigned int available;
		static unsigned int waiting_audio;
		bool m_acquired;

	public:
		FeMediaDecodeLock( const FeMediaDecodeLock & ) = delete;
		FeMediaDecodeLock &operator=( const FeMediaDecodeLock & ) = delete;

		FeMediaDecodeLock( const std::atomic<bool> &stop, bool audio )
			: m_acquired( false )
		{
			std::unique_lock<std::mutex> lock( mutex );
			if ( audio )
				++waiting_audio;
			std::condition_variable &ready = audio ? audio_ready : video_ready;
			while ( !stop && ( available == 0 || ( !audio && waiting_audio != 0 ) ) )
				ready.wait( lock );
			if ( audio )
				--waiting_audio;
			if ( !stop )
			{
				--available;
				m_acquired = true;
			}
			if ( available > 0 )
			{
				if ( waiting_audio > 0 )
					audio_ready.notify_one();
				else
					video_ready.notify_one();
			}
		}

		~FeMediaDecodeLock()
		{
			if ( m_acquired )
			{
				std::lock_guard<std::mutex> lock( mutex );
				++available;
				if ( waiting_audio > 0 )
					audio_ready.notify_one();
				else
					video_ready.notify_one();
			}
		}

		bool acquired() const { return m_acquired; }
		static void notify_stop()
		{
			std::lock_guard<std::mutex> lock( mutex );
			audio_ready.notify_all();
			video_ready.notify_all();
		}
	};

	std::mutex FeMediaDecodeLock::mutex;
	std::condition_variable FeMediaDecodeLock::audio_ready;
	std::condition_variable FeMediaDecodeLock::video_ready;
	unsigned int FeMediaDecodeLock::available = std::max( 1u, std::thread::hardware_concurrency() );
	unsigned int FeMediaDecodeLock::waiting_audio = 0;

	bool fe_is_attached_picture_stream( const AVStream *stream )
	{
		if ( !stream )
			return false;

		if ( stream->disposition & AV_DISPOSITION_ATTACHED_PIC )
			return true;

		return stream->attached_pic.size > 0;
	}

	std::string g_decoder;

#if FE_HWACCEL
	AVBufferRef *g_hw_device_ctx = NULL;
	AVHWDeviceType g_hw_device_type = AV_HWDEVICE_TYPE_NONE;
#endif
}

//
// As of Nov, 2017 RetroPie's default version of avcodec is old enough
// that it doesn't define AV_INPUT_PADDING_SIZE
//
#ifndef AV_INPUT_BUFFER_PADDING_SIZE
#define AV_INPUT_BUFFER_PADDING_SIZE FF_INPUT_BUFFER_PADDING_SIZE
#endif

void print_ffmpeg_version_info()
{
	FeLog() << "avcodec " << LIBAVCODEC_VERSION_MAJOR
		<< '.' << LIBAVCODEC_VERSION_MINOR
		<< '.' << LIBAVCODEC_VERSION_MICRO

		<< " / avformat " << LIBAVFORMAT_VERSION_MAJOR
		<< '.' << LIBAVFORMAT_VERSION_MINOR
		<< '.' << LIBAVFORMAT_VERSION_MICRO

		<< " / swscale " << LIBSWSCALE_VERSION_MAJOR
		<< '.' << LIBSWSCALE_VERSION_MINOR
		<< '.' << LIBSWSCALE_VERSION_MICRO

		<< " / avutil " << LIBAVUTIL_VERSION_MAJOR
		<< '.' << LIBAVUTIL_VERSION_MINOR
		<< '.' << LIBAVUTIL_VERSION_MICRO

		<< " / swresample " << LIBSWRESAMPLE_VERSION_MAJOR
		<< '.' << LIBSWRESAMPLE_VERSION_MINOR
		<< '.' << LIBSWRESAMPLE_VERSION_MICRO

		<< std::endl;
}

//
// One producer owns the demuxer and both codecs. Only completed video frames
// and playback controls cross the frontend/producer boundary.
//
class FeMediaImp
{
public:
	FeMediaImp( FeMedia::Type t );
	void close();

	FeMedia::Type m_type;
	AVFormatContext *m_format_ctx;
	AVIOContext *m_io_ctx;
	bool m_read_eof;
	bool m_rewind;
	bool m_restart;
	bool m_multiframe;
	FeTime m_duration;
	FeClock m_timer;
	std::thread m_thread;
	std::mutex m_mutex;
	std::condition_variable m_wake;
	std::atomic<bool> m_stop;
	std::atomic<bool> m_playing;
	std::atomic<std::int64_t> m_time;
	std::atomic<std::int64_t> m_audio_clock_offset;
	std::atomic<std::int64_t> m_audio_clock_end;
	std::uint64_t m_audio_frames;
};

class FeBaseStream
{
private:
	std::queue<AVPacket *> m_packetq;
	std::size_t m_packet_bytes;

public:
	bool at_end;
	bool draining;
	bool needs_packet;
	AVCodecContext *codec_ctx;
	FeAVCodec *codec;
	int stream_id;
	AVFrame *frame;

	FeBaseStream();
	virtual ~FeBaseStream();
	void stop();
	void push_packet( AVPacket *pkt );
	bool has_packet_room() const;
	int decode_frame( bool eof );
};

class FeAudioImp : public FeBaseStream
{
public:
	SwrContext *resample_ctx;
	std::vector<float> decoded_samples;
	std::vector<float> processed_samples;

	FeAudioImp();
	~FeAudioImp();

	bool process_frame( AVFrame *frame, std::vector<float> &samples );
};

class FeVideoImp : public FeBaseStream
{
	friend class FeMedia;

private:
	struct Frame
	{
		std::uint8_t *pixels = NULL;
		FeTime time;
	};

	FeMedia *m_parent;
	std::array<Frame, 4> m_frames;
	std::deque<int> m_ready_frames;
	std::deque<int> m_free_frames;
	int m_displayed_frame;
	std::mutex m_image_mutex;
	FeTime refresh_interval;
	FeTime half_frame_offset;
	FeTime presentation_time;
	FeTime previous_time;
	AVRational time_base;
	int64_t start_time;
	FeTime frame_duration;
	FeTime next_frame_time;
	SwsContext *sws_ctx;
	int source_width = 0;
	int source_height = 0;
	AVPixelFormat source_format = AV_PIX_FMT_NONE;
#if FE_HWACCEL
	AVPixelFormat hwaccel_output_format;
	bool hw_retrieve_data( AVFrame *f );
#endif

public:
	int disptex_width;
	int disptex_height;
	std::atomic<unsigned long long> frame_serial;

	FeVideoImp( FeMedia *parent );
	~FeVideoImp();

	void stop();
	void init_rgba_buffer();
	bool get_rgba_frame_dimensions( unsigned int &width, unsigned int &height );
	bool copy_rgba_frame( std::vector<unsigned char> &pixels, unsigned int &width, unsigned int &height );
	bool copy_rgba_frame_to( void *pixels, std::size_t pixel_count, unsigned int &width, unsigned int &height );
	void clear_rgba_frame();
	bool tick();
	bool queue_frame();
	bool has_frame_room();
	bool frames_pending();
	FeTime next_frame_delay();
};

FeMediaImp::FeMediaImp( FeMedia::Type t )
	: m_type( t ),
	m_format_ctx( NULL ),
	m_io_ctx( NULL ),
	m_read_eof( false ),
	m_rewind( false ),
	m_restart( false ),
	m_multiframe( false ),
	m_stop( true ),
	m_playing( false ),
	m_time( 0 ),
	m_audio_clock_offset( 0 ),
	m_audio_clock_end( 0 ),
	m_audio_frames( 0 )
{
	m_timer.reset();
}

void FeMediaImp::close()
{
	if ( m_format_ctx )
		avformat_close_input( &m_format_ctx );

	if ( m_io_ctx )
	{
		delete static_cast<FeInputStream *>( m_io_ctx->opaque );
		av_free( m_io_ctx->buffer );
		av_free( m_io_ctx );
		m_io_ctx = NULL;
	}

	m_read_eof = false;
}

FeBaseStream::FeBaseStream()
	: m_packet_bytes( 0 ),
	at_end( false ),
	draining( false ),
	needs_packet( true ),
	codec_ctx( NULL ),
	codec( NULL ),
	stream_id( -1 ),
	frame( av_frame_alloc() )
{
}

FeBaseStream::~FeBaseStream()
{
	stop();
	av_frame_free( &frame );
	avcodec_free_context( &codec_ctx );
}

void FeBaseStream::stop()
{
	while ( !m_packetq.empty() )
	{
		AVPacket *packet = m_packetq.front();
		m_packetq.pop();
		av_packet_free( &packet );
	}
	m_packet_bytes = 0;
	at_end = false;
	draining = false;
	needs_packet = true;
	if ( frame )
		av_frame_unref( frame );
}

void FeBaseStream::push_packet( AVPacket *pkt )
{
	m_packet_bytes += pkt->size;
	m_packetq.push( pkt );
	needs_packet = false;
}

bool FeBaseStream::has_packet_room() const
{
	return m_packetq.size() < 64 && m_packet_bytes < 4 * 1024 * 1024;
}

int FeBaseStream::decode_frame( bool eof )
{
	av_frame_unref( frame );
	for (;;)
	{
		int result = avcodec_receive_frame( codec_ctx, frame );
		if ( result != AVERROR( EAGAIN ) )
		{
			needs_packet = false;
			if ( result == AVERROR_EOF )
				at_end = true;
			return result;
		}

		if ( m_packetq.empty() && !eof )
		{
			needs_packet = true;
			return result;
		}
		if ( draining )
			return AVERROR_INVALIDDATA;

		AVPacket *packet = m_packetq.empty() ? NULL : m_packetq.front();
		result = avcodec_send_packet( codec_ctx, packet );
		// A receive and send cannot both return EAGAIN. Keep unaccepted input.
		if ( result == AVERROR( EAGAIN ) )
			return AVERROR_INVALIDDATA;
		if ( result < 0 )
			return result;

		if ( packet )
		{
			m_packet_bytes -= packet->size;
			m_packetq.pop();
			av_packet_free( &packet );
		}
		else
			draining = true;
	}
}

FeAudioImp::FeAudioImp()
	: FeBaseStream(),
	resample_ctx( NULL )
{
}

FeAudioImp::~FeAudioImp()
{
	if ( resample_ctx )
	{
		swr_free( &resample_ctx );
		resample_ctx = NULL;
	}
}

bool FeAudioImp::process_frame( AVFrame *frame, std::vector<float> &samples )
{
	if ( !resample_ctx )
	{
		if ( !frame )
			return true;
		resample_ctx = swr_alloc();
		if ( !resample_ctx )
		{
			FeLog() << "Error allocating audio format converter." << std::endl;
			return false;
		}

#if HAVE_CH_LAYOUT
		AVChannelLayout layout = AV_CHANNEL_LAYOUT_MASK( 0, 0 );
		av_channel_layout_copy( &layout, &codec_ctx->ch_layout );
		if ( !av_channel_layout_check( &layout ) )
			av_channel_layout_default( &layout, codec_ctx->ch_layout.nb_channels );

		AVChannelLayout out_layout = AV_CHANNEL_LAYOUT_STEREO;
		av_opt_set_chlayout( resample_ctx, "in_chlayout", &layout, 0 );
		av_opt_set_chlayout( resample_ctx, "out_chlayout", &out_layout, 0 );
		av_channel_layout_uninit( &layout );
#else
		int64_t channel_layout = codec_ctx->channel_layout;
		if ( !channel_layout )
			channel_layout = av_get_default_channel_layout( codec_ctx->channels );
		av_opt_set_int( resample_ctx, "in_channel_layout", channel_layout, 0 );
		av_opt_set_int( resample_ctx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0 );
#endif

		av_opt_set_int( resample_ctx, "in_sample_fmt", codec_ctx->sample_fmt, 0 );
		av_opt_set_int( resample_ctx, "in_sample_rate", codec_ctx->sample_rate, 0 );
		av_opt_set_int( resample_ctx, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0 );
		av_opt_set_int( resample_ctx, "out_sample_rate", FeSdlAudioBackend::get().sample_rate(), 0 );

		if ( swr_init( resample_ctx ) < 0 )
		{
			FeLog() << "Error initializing media audio converter." << std::endl;
			swr_free( &resample_ctx );
			return false;
		}
	}

	const int in_sample_rate = frame && frame->sample_rate > 0 ? frame->sample_rate : codec_ctx->sample_rate;
	const int max_out_samples = av_rescale_rnd(
		swr_get_delay( resample_ctx, in_sample_rate ) + ( frame ? frame->nb_samples : 0 ),
		FeSdlAudioBackend::get().sample_rate(),
		in_sample_rate,
		AV_ROUND_UP );
	if ( max_out_samples <= 0 )
		return true;

	std::vector<float> converted_samples( static_cast<std::size_t>( max_out_samples ) * FeSdlAudioBackend::get().channel_count() );
	uint8_t *output_data[1] =
	{
		reinterpret_cast<uint8_t *>( converted_samples.data() )
	};

	const int out_samples = swr_convert(
		resample_ctx,
		output_data,
		max_out_samples,
		frame ? const_cast<const uint8_t **>( frame->extended_data ) : NULL,
		frame ? frame->nb_samples : 0 );

	if ( out_samples < 0 )
	{
		FeLog() << "Error performing media audio conversion." << std::endl;
		return false;
	}

	converted_samples.resize( static_cast<std::size_t>( out_samples ) * FeSdlAudioBackend::get().channel_count() );
	samples.insert( samples.end(), converted_samples.begin(), converted_samples.end() );
	return true;
}


FeVideoImp::FeVideoImp( FeMedia *parent )
	: m_parent( parent ),
	m_displayed_frame( -1 ),
	start_time( 0 ),
	frame_duration( fe_seconds( 1.0 / 30.0 ) ),
	sws_ctx( NULL ),
#if FE_HWACCEL
	hwaccel_output_format( AV_PIX_FMT_NONE ),
#endif
	disptex_width( 0 ),
	disptex_height( 0 ),
	frame_serial( 0 )
{
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
	{
		double refresh_rate = std::max( 1.0, fep->get_refresh_rate() );
		if ( SDL_Window *window = fep->get_gpu_context().get_window() )
			if ( const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode( SDL_GetDisplayForWindow( window ) ) )
				if ( mode->refresh_rate > 0.0f )
					refresh_rate = mode->refresh_rate;
		refresh_interval = fe_seconds( 1.0 / refresh_rate );
		half_frame_offset = fe_seconds( 0.5 / refresh_rate );
	}
}

FeVideoImp::~FeVideoImp()
{
	stop();
	for ( Frame &frame : m_frames )
		av_freep( &frame.pixels );
}

#if FE_HWACCEL

enum AVPixelFormat hw_get_output_format( AVBufferRef *hw_frames_ctx )
{
	enum AVPixelFormat retval = AV_PIX_FMT_NONE;

	enum AVPixelFormat *p=NULL;
	int e = av_hwframe_transfer_get_formats(
			hw_frames_ctx,
			AV_HWFRAME_TRANSFER_DIRECTION_FROM,
			&p, 0 );

	if ( e < 0 )
		FeLog() << "Error getting supported hardware formats." << std::endl;
	else
		retval = *p;

	av_free( p );

	return retval;
}

bool FeVideoImp::hw_retrieve_data( AVFrame *f )
{
	if ( f->format == AV_PIX_FMT_NONE )
		return false;

	if ( !( av_pix_fmt_desc_get( (AVPixelFormat)f->format )->flags & AV_PIX_FMT_FLAG_HWACCEL ))
		return false;

	AVFrame *sw_frame = av_frame_alloc();
	if ( !sw_frame )
	{
		FeLog() << "Error allocating hardware video transfer frame." << std::endl;
		return false;
	}
	if ( hwaccel_output_format == AV_PIX_FMT_NONE )
	{
		hwaccel_output_format = hw_get_output_format( codec_ctx->hw_frames_ctx );

		FeDebug() << "HWAccel output pixel format: "
			<< av_pix_fmt_desc_get( hwaccel_output_format )->name << std::endl;
	}

	sw_frame->format = hwaccel_output_format;

	int err = av_hwframe_transfer_data( sw_frame, f, 0 );
	if ( err < 0 )
	{
		FeLog() << "Error transferring hardware frame data." << std::endl;
		av_frame_free( &sw_frame );
		return false;
	}

	err = av_frame_copy_props( sw_frame, f );
	if ( err < 0 )
		FeLog() << "Error copying hardware frame properties." << std::endl;

	av_frame_unref( f );
	av_frame_move_ref( f, sw_frame );
	av_frame_free( &sw_frame );

	return true;
}

#endif

void FeVideoImp::stop()
{
	FeBaseStream::stop();
	std::lock_guard<std::mutex> lock( m_image_mutex );
	m_ready_frames.clear();
	m_free_frames.clear();
	for ( int i = 0; i < static_cast<int>( m_frames.size() ); ++i )
		if ( i != m_displayed_frame )
			m_free_frames.push_back( i );
	next_frame_time = FeTime();
	if ( sws_ctx )
	{
		sws_freeContext( sws_ctx );
		sws_ctx = NULL;
	}
}

void FeVideoImp::init_rgba_buffer()
{
	stop();
	frame_serial.store( 0, std::memory_order_release );
}

bool FeVideoImp::get_rgba_frame_dimensions( unsigned int &width, unsigned int &height )
{
	if ( disptex_width <= 0 || disptex_height <= 0 )
		return false;
	width = static_cast<unsigned int>( disptex_width );
	height = static_cast<unsigned int>( disptex_height );
	return true;
}

bool FeVideoImp::copy_rgba_frame( std::vector<unsigned char> &pixels, unsigned int &width, unsigned int &height )
{
	// The frontend pins this buffer until its next tick. The producer only
	// writes free buffers, so a display copy never needs its queue lock.
	if ( m_displayed_frame < 0 || !get_rgba_frame_dimensions( width, height ) )
		return false;
	const std::uint8_t *source = m_frames[m_displayed_frame].pixels;
	pixels.assign( source, source + static_cast<std::size_t>( width ) * height * 4 );
	return true;
}

bool FeVideoImp::copy_rgba_frame_to( void *pixels, std::size_t pixel_count, unsigned int &width, unsigned int &height )
{
	if ( !pixels || m_displayed_frame < 0 || !get_rgba_frame_dimensions( width, height ) )
		return false;
	const std::size_t size = static_cast<std::size_t>( width ) * height * 4;
	if ( pixel_count < size )
		return false;
	std::memcpy( pixels, m_frames[m_displayed_frame].pixels, size );
	return true;
}

void FeVideoImp::clear_rgba_frame()
{
	std::lock_guard<std::mutex> lock( m_image_mutex );
	if ( m_displayed_frame >= 0 )
		m_free_frames.push_back( m_displayed_frame );
	m_displayed_frame = -1;
	while ( !m_ready_frames.empty() )
	{
		m_free_frames.push_back( m_ready_frames.front() );
		m_ready_frames.pop_front();
	}
	frame_serial.store( 0, std::memory_order_release );
}

bool FeVideoImp::tick()
{
	const FeTime time = m_parent->get_video_time() + half_frame_offset;
	const FeTime next = presentation_time + refresh_interval;
	// Vsync paces presentation, but CPU wakeups can straddle a frame deadline.
	// Keep a regular refresh cadence while it remains within one refresh of
	// the media clock, including when queued GPU frames cause CPU ticks to
	// arrive in bursts. Stalls, restarts and a stopped clock reanchor.
	FeTime now = time;
	if ( previous_time > half_frame_offset && time > previous_time && refresh_interval > FeTime() &&
		std::abs( ( next - time ).asMicroseconds() ) < refresh_interval.asMicroseconds() )
		now = next;
	previous_time = time;
	presentation_time = now;
	bool changed = false;
	{
		std::unique_lock<std::mutex> lock( m_image_mutex, std::try_to_lock );
		if ( !lock.owns_lock() )
			return false;
		while ( !m_ready_frames.empty() && m_frames[m_ready_frames.front()].time <= now )
		{
			if ( m_displayed_frame >= 0 )
				m_free_frames.push_back( m_displayed_frame );
			m_displayed_frame = m_ready_frames.front();
			m_ready_frames.pop_front();
			changed = true;
		}
		if ( changed )
			frame_serial.fetch_add( 1, std::memory_order_release );
	}
	if ( changed )
		m_parent->m_imp->m_wake.notify_one();
	return changed;
}

bool FeVideoImp::has_frame_room()
{
	const FeTime now = m_parent->get_video_time() - half_frame_offset;
	std::lock_guard<std::mutex> lock( m_image_mutex );
	// Keep a refresh of history for the frontend's presentation phase, even
	// when its CPU wakeup comes after the next media deadline.
	while ( m_ready_frames.size() > 1 && m_frames[m_ready_frames[1]].time <= now )
	{
		m_free_frames.push_back( m_ready_frames.front() );
		m_ready_frames.pop_front();
	}
	return m_ready_frames.size() < 3 && !m_free_frames.empty();
}

bool FeVideoImp::frames_pending()
{
	std::lock_guard<std::mutex> lock( m_image_mutex );
	return !m_ready_frames.empty();
}

FeTime FeVideoImp::next_frame_delay()
{
	const FeTime now = m_parent->get_video_time() + half_frame_offset;
	std::lock_guard<std::mutex> lock( m_image_mutex );
	for ( int index : m_ready_frames )
		if ( m_frames[index].time > now )
			return m_frames[index].time - now;
	return fe_milliseconds( 20 );
}

bool FeVideoImp::queue_frame()
{
	int64_t pts = frame->best_effort_timestamp;
	FeTime frame_time = ( pts == AV_NOPTS_VALUE )
		? next_frame_time
		: fe_seconds( ( pts - start_time ) * av_q2d( time_base ) );
#if HAVE_DURATION
	const int64_t duration = frame->duration;
#else
	const int64_t duration = frame->pkt_duration;
#endif
	const FeTime span = duration > 0 ? fe_seconds( duration * av_q2d( time_base ) ) : frame_duration;
	// Some containers round constant-rate timestamps to whole milliseconds.
	// Keep their nominal cadence without flattening variable frame durations
	// or larger timestamp discontinuities.
	const bool regular_span = std::abs( ( span - frame_duration ).asMicroseconds() ) <= 1000;
	if ( regular_span && std::abs( ( frame_time - next_frame_time ).asMicroseconds() ) <= 1000 )
		frame_time = next_frame_time;
	next_frame_time = frame_time + ( regular_span ? frame_duration : span );

	// Catch up without spending conversion/upload work on expired frames.
	if ( next_frame_time < m_parent->get_video_time() - half_frame_offset && frame_serial.load( std::memory_order_acquire ) != 0 )
		return true;

#if FE_HWACCEL
	hw_retrieve_data( frame );
	if ( av_pix_fmt_desc_get( static_cast<AVPixelFormat>( frame->format ) )->flags & AV_PIX_FMT_FLAG_HWACCEL )
		return false;
#endif
	if ( !sws_ctx || source_width != frame->width || source_height != frame->height || source_format != frame->format )
	{
		int sws_flags = SWS_BILINEAR;
		if ( ( frame->width & 0x7 ) || ( frame->height & 0x7 ) )
			sws_flags |= SWS_ACCURATE_RND;
		sws_ctx = sws_getCachedContext( sws_ctx,
			frame->width, frame->height, static_cast<AVPixelFormat>( frame->format ),
			disptex_width, disptex_height, AV_PIX_FMT_RGBA, sws_flags, NULL, NULL, NULL );
		source_width = frame->width;
		source_height = frame->height;
		source_format = static_cast<AVPixelFormat>( frame->format );
	}
	if ( !sws_ctx )
	{
		FeLog() << "Error allocating video colour converter." << std::endl;
		return false;
	}

	int index;
	{
		std::lock_guard<std::mutex> lock( m_image_mutex );
		index = m_free_frames.front();
		m_free_frames.pop_front();
	}
	Frame &output = m_frames[index];
	if ( !output.pixels )
		output.pixels = static_cast<std::uint8_t *>( av_malloc( static_cast<std::size_t>( disptex_width ) * disptex_height * 4 ) );
	if ( !output.pixels )
	{
		std::lock_guard<std::mutex> lock( m_image_mutex );
		m_free_frames.push_back( index );
		FeLog() << "Error allocating video frame." << std::endl;
		return false;
	}
	std::uint8_t *buffers[4] = { output.pixels, NULL, NULL, NULL };
	int linesize[4] = { disptex_width * 4, 0, 0, 0 };
	const int rows = sws_scale( sws_ctx, frame->data, frame->linesize,
		0, frame->height, buffers, linesize );
	{
		std::lock_guard<std::mutex> lock( m_image_mutex );
		if ( rows != disptex_height )
		{
			m_free_frames.push_back( index );
			FeLog() << "Error converting video frame." << std::endl;
			return false;
		}
		output.time = frame_time;
		m_ready_frames.push_back( index );
	}
	return true;
}

FeMedia::FeMedia( Type t, std::shared_ptr<FeAudioEffectsManager> effects_manager )
	: m_audio( NULL ),
	m_video( NULL ),
	m_audio_effects( std::move( effects_manager ) ),
	m_aspect_ratio( 1.0f ),
	m_volume( 100.0f ),
	m_pan( 0.0f ),
	m_audio_pending_offset( 0 )
{
	m_imp = new FeMediaImp( t );
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
	{
		auto* normaliser = m_audio_effects->get_effect<FeAudioNormaliser>();
		if ( normaliser )
			normaliser->set_enabled( fep->get_fes()->get_loudness() );
	}
}

FeMedia::~FeMedia()
{
	close();
	delete m_imp;
}

bool FeMedia::ensure_audio_stream()
{
	if ( !m_audio || !m_audio_stream.ensure_open() )
		return false;
	std::ignore = m_audio_stream.set_gain( m_volume.load( std::memory_order_relaxed ) / 100.0f );
	return true;
}

void FeMedia::clear_pending_audio()
{
	m_audio_pending_samples.clear();
	m_audio_pending_offset = 0;
}

void FeMedia::append_pending_audio( const std::vector<float> &samples )
{
	if ( m_audio_pending_offset >= m_audio_pending_samples.size() )
		clear_pending_audio();
	m_audio_pending_samples.insert( m_audio_pending_samples.end(), samples.begin(), samples.end() );
}

bool FeMedia::queue_pending_audio( std::vector<float> &processed_samples )
{
	const int channel_count = FeSdlAudioBackend::get().channel_count();
	while ( m_audio_pending_offset < m_audio_pending_samples.size() && !m_imp->m_stop )
	{
		const int queue_headroom = FE_MEDIA_AUDIO_QUEUE_TARGET_FRAMES - m_audio_stream.queued_frames();
		if ( queue_headroom <= 0 )
			break;
		const std::size_t remaining_frames = ( m_audio_pending_samples.size() - m_audio_pending_offset ) / channel_count;
		const unsigned int frame_count = static_cast<unsigned int>( std::min<std::size_t>(
			remaining_frames, std::min( FE_MEDIA_AUDIO_PROCESS_CHUNK_FRAMES, queue_headroom ) ) );
		if ( frame_count == 0 )
			break;

		processed_samples.resize( static_cast<std::size_t>( frame_count ) * channel_count );
		m_audio_effects->process_all( m_audio_pending_samples.data() + m_audio_pending_offset,
			processed_samples.data(), frame_count, channel_count );
		fe_audio_apply_balance( processed_samples, m_pan.load( std::memory_order_relaxed ), false, Vec3f() );
		if ( !m_audio_stream.queue_frames( processed_samples.data(), frame_count ) )
			return false;
		m_imp->m_audio_frames += frame_count;
		m_audio_pending_offset += static_cast<std::size_t>( frame_count ) * channel_count;
	}
	if ( m_audio_pending_offset >= m_audio_pending_samples.size() )
		clear_pending_audio();
	return true;
}

FeTime FeMedia::get_video_time()
{
	FeTime time = m_imp->m_duration;
	if ( m_imp->m_playing || m_imp->m_stop )
	{
		time = m_imp->m_timer.getElapsedTime();
		const std::int64_t audio_end = m_imp->m_audio_clock_end.load( std::memory_order_acquire );
		if ( audio_end == 0 )
			time = FeTime(); // Wait for the first PCM before advancing audio/video time.
		else
		{
			time += fe_microseconds( m_imp->m_audio_clock_offset.load( std::memory_order_relaxed ) );
			if ( audio_end > 0 )
				time = std::min( time, fe_microseconds( audio_end ) );
		}
	}
	// SDL consumes input in device-sized blocks. Updating its clock estimate
	// must not move script time or presentation deadlines backwards.
	const std::int64_t current = std::max<std::int64_t>( 0, time.asMicroseconds() );
	std::int64_t previous = m_imp->m_time.load( std::memory_order_relaxed );
	while ( previous < current && !m_imp->m_time.compare_exchange_weak( previous, current, std::memory_order_relaxed ) )
	{
	}
	return fe_microseconds( std::max( previous, current ) );
}

void FeMedia::play()
{
	std::unique_lock<std::mutex> lock( m_imp->m_mutex );
	if ( m_imp->m_playing || ( !m_video && !m_audio ) )
		return;
	if ( m_imp->m_thread.joinable() && m_imp->m_stop )
	{
		lock.unlock();
		stop();
		lock.lock();
	}

	// Open the SDL stream on the caller before launching producers. Subsystem
	// initialization stays serialized; only the producer uses the open stream.
	const bool restart = m_imp->m_thread.joinable();
	if ( !restart && m_audio && !ensure_audio_stream() )
		m_audio->at_end = true;
	m_imp->m_time = 0;
	m_imp->m_audio_clock_offset = 0;
	m_imp->m_audio_clock_end = m_audio && ( restart || !m_audio->at_end ) ? 0 : -1;
	m_imp->m_timer.restart();
	m_imp->m_stop = false;
	m_imp->m_playing = true;
	if ( restart )
	{
		// At EOF the producer waits under this mutex. It can rewind without
		// joining a thread or rebuilding an SDL stream on the frontend.
		m_imp->m_restart = true;
		m_imp->m_wake.notify_one();
	}
	else
	{
		m_imp->m_restart = false;
		m_imp->m_audio_frames = 0;
		m_imp->m_thread = std::thread( &FeMedia::media_thread, this );
	}
}

void FeMedia::signal_stop()
{
	{
		std::lock_guard<std::mutex> lock( m_imp->m_mutex );
		m_imp->m_stop = true;
	}
	m_imp->m_wake.notify_one();
	FeMediaDecodeLock::notify_stop();
}

void FeMedia::stop()
{
	signal_stop();
	if ( m_imp->m_thread.joinable() )
	{
		m_imp->m_thread.join();
		m_imp->m_rewind = true;
	}
	m_imp->m_timer.stop();
	m_imp->m_playing = false;
	m_audio_stream.clear();
	m_audio_stream.destroy();
	reset_decoder();
}

void FeMedia::reset_decoder()
{
	clear_pending_audio();
	m_imp->m_audio_frames = 0;

	if ( m_audio )
	{
		m_audio->stop();
		avcodec_flush_buffers( m_audio->codec_ctx );
		swr_free( &m_audio->resample_ctx );
	}
	if ( m_video )
	{
		m_video->stop();
		avcodec_flush_buffers( m_video->codec_ctx );
	}
	m_audio_effects->reset_all();
	m_imp->m_read_eof = false;
}

void FeMedia::close()
{
	stop();
	delete m_audio;
	m_audio = NULL;
	delete m_video;
	m_video = NULL;
	m_imp->close();
}

bool FeMedia::is_playing()
{
	return !m_imp->m_stop && m_imp->m_playing;
}

float FeMedia::getVolume() const
{
	return m_volume.load( std::memory_order_relaxed );
}

void FeMedia::setVolume( float volume )
{
	m_volume.store( volume, std::memory_order_relaxed );
	m_imp->m_wake.notify_one();
	auto* normaliser = m_audio_effects->get_effect<FeAudioNormaliser>();
	if ( normaliser )
		normaliser->set_media_volume( volume / 100.0f );
}

float FeMedia::getPan() const
{
	return m_pan.load( std::memory_order_relaxed );
}

void FeMedia::setPan( float pan )
{
	m_pan.store( std::clamp( pan, -1.0f, 1.0f ), std::memory_order_relaxed );
}

int fe_media_read( void *opaque, uint8_t *buff, int buff_size )
{
	FeInputStream *z = static_cast<FeInputStream *>( opaque );

	size_t bytes_read = z->read( buff, buff_size ).value_or(0);

	if ( bytes_read == 0 )
		return AVERROR_EOF;

	return bytes_read;
}

// whence: SEEK_SET, SEEK_CUR, SEEK_END, and AVSEEK_SIZE
size_t fe_media_seek( void *opaque, int64_t offset, int whence )
{
	FeInputStream *z = static_cast<FeInputStream *>( opaque );

	switch ( whence )
	{
	case AVSEEK_SIZE:
		return z->getSize().value_or(0);

	case SEEK_CUR:
		if ( auto pos = z->tell() )
			return z->seek( *pos + static_cast<size_t>( offset )).value_or(0);
		else
			return 0;

	case SEEK_END:
		if ( auto size = z->getSize() )
			return z->seek( *size + static_cast<size_t>( offset )).value_or(0);
		else
			return 0;

	case SEEK_SET:
	default:
		return z->seek( static_cast<size_t>( offset )).value_or(0);
	}
}

bool FeMedia::open( const std::string &archive,
	const std::string &name )
{
	FeInputStream *s = NULL;

	if ( !archive.empty() )
	{
		FeZipStream *z = new FeZipStream( archive );

		if ( !z->open( name ))
		{
			// Error opening specified filename. Try to correct
			// in case filname is in a subdir of the archive
			std::string temp;
			if ( get_archive_filename_with_base( temp, archive, name ))
			{
				z->open( temp );
			}
			else
			{
				delete s;
				return false;
			}
		}

		s = z;
	}
	else
		s = new FeFileInputStream( name );

	m_imp->m_format_ctx = avformat_alloc_context();

	size_t avio_ctx_buffer_size = 4096;
	uint8_t *avio_ctx_buffer = (uint8_t *)av_malloc( avio_ctx_buffer_size
			+ AV_INPUT_BUFFER_PADDING_SIZE );

	memset( avio_ctx_buffer + avio_ctx_buffer_size,
		0,
		AV_INPUT_BUFFER_PADDING_SIZE );

	m_imp->m_io_ctx = avio_alloc_context( avio_ctx_buffer,
		avio_ctx_buffer_size, 0, s, &fe_media_read,
		NULL, reinterpret_cast<int64_t (*)( void*, int64_t, int )>( fe_media_seek ));

	m_imp->m_format_ctx->pb = m_imp->m_io_ctx;

	if ( avformat_open_input( &m_imp->m_format_ctx, name.c_str(), NULL, NULL ) < 0 )
	{
		FeLog() << "Error opening input file: " << name << std::endl;
		return false;
	}

	if ( avformat_find_stream_info( m_imp->m_format_ctx, NULL ) < 0 )
	{
		FeLog() << "Error finding stream information in input file: "
				<< FORMAT_CTX_URL << std::endl;
		return false;
	}

	m_audio_effects->reset_all();

	if ( m_imp->m_type & Audio )
	{
		int stream_id( -1 );
		FeAVCodec *dec;

		stream_id = av_find_best_stream( m_imp->m_format_ctx, AVMEDIA_TYPE_AUDIO,
			-1, -1, &dec, 0 );

		if ( stream_id >= 0 )
		{

			AVCodecContext *codec_ctx;
			codec_ctx = avcodec_alloc_context3( NULL );

			avcodec_parameters_to_context( codec_ctx, m_imp->m_format_ctx->streams[stream_id]->codecpar );

			if ( avcodec_open2( codec_ctx, dec, NULL ) < 0 )
			{
				FeLog() << "Could not open audio decoder for file: "
					<< FORMAT_CTX_URL << std::endl;
				avcodec_free_context( &codec_ctx );
			}
			else
			{
				m_audio = new FeAudioImp();
				m_audio->stream_id = stream_id;
				m_audio->codec_ctx = codec_ctx;
				m_audio->codec = dec;
				m_audio->at_end = false;
				clear_pending_audio();
			}
		}
	}

	if ( m_imp->m_type & Video )
	{
		std::string prev_dec_name;
		int av_result( -1 );
		int stream_id( -1 );
		FeAVCodec *dec;

		stream_id = av_find_best_stream( m_imp->m_format_ctx, AVMEDIA_TYPE_VIDEO,
					-1, -1, &dec, 0 );

		// Force libvpx decoders for VP8/VP9 content to get alpha support
		if ( stream_id >= 0 )
		{
			AVCodecID codec_id = m_imp->m_format_ctx->streams[stream_id]->codecpar->codec_id;

			if ( codec_id == AV_CODEC_ID_VP8 )
			{
				FeAVCodec *libvpx_dec = avcodec_find_decoder_by_name( "libvpx" );
				if ( libvpx_dec ) dec = libvpx_dec;
			}
			else if ( codec_id == AV_CODEC_ID_VP9 )
			{
				FeAVCodec *libvpx_dec = avcodec_find_decoder_by_name( "libvpx-vp9" );
				if ( libvpx_dec ) dec = libvpx_dec;
			}

		}

		if (( stream_id >= 0 ) && fe_is_attached_picture_stream( m_imp->m_format_ctx->streams[stream_id] ))
		{
			stream_id = -1;
		}

		if ( stream_id < 0 )
		{
			FeLog() << "No video stream found, file: "
				<< FORMAT_CTX_URL << std::endl;
		}
		else
		{

			AVCodecContext *codec_ctx;
			codec_ctx = avcodec_alloc_context3( NULL );

			avcodec_parameters_to_context( codec_ctx, m_imp->m_format_ctx->streams[stream_id]->codecpar );

			codec_ctx->workaround_bugs = FF_BUG_AUTODETECT;

			// Note also: http://trac.ffmpeg.org/ticket/4404
			codec_ctx->thread_count=1;

			if ( dec )
				prev_dec_name = std::string( dec->name );

			try_hw_accel( codec_ctx, dec );

			av_result = avcodec_open2( codec_ctx, dec, NULL );
			if ( av_result < 0 )
			{
				if ( !prev_dec_name.empty() && ( g_decoder.compare( "mmal" ) == 0 ))
				{
					switch( dec->id )
					{


					case AV_CODEC_ID_VC1:
					case AV_CODEC_ID_MPEG2VIDEO:
					case AV_CODEC_ID_H264:
					case AV_CODEC_ID_MPEG4:
						FeLog() << "mmal video decoding (" << dec->name
							<< ") not supported for file (trying software): "
							<< FORMAT_CTX_URL << std::endl;

						dec = avcodec_find_decoder_by_name( prev_dec_name.c_str() );

						av_result = avcodec_open2( codec_ctx, dec, NULL );
						break;

					default:
						break;
					}
				}

				if ( av_result < 0 )
				{
					FeLog() << "Could not open video decoder for file: "
							<< FORMAT_CTX_URL << std::endl;
					avcodec_free_context( &codec_ctx );
				}
			}

			if ( av_result >=0 )
			{
				m_video = new FeVideoImp( this );

				m_video->stream_id = stream_id;
				m_video->codec_ctx = codec_ctx;

				m_video->codec = dec;
				AVStream *stream = m_imp->m_format_ctx->streams[stream_id];
				m_video->time_base = stream->time_base;
				m_video->start_time = stream->start_time == AV_NOPTS_VALUE ? 0 : stream->start_time;
				const double frame_rate = av_q2d( av_guess_frame_rate( m_imp->m_format_ctx, stream, NULL ) );
				if ( frame_rate > 0 )
					m_video->frame_duration = fe_seconds( 1.0 / frame_rate );
				m_imp->m_multiframe = stream->nb_frames > 1 ||
					stream->codecpar->codec_id == AV_CODEC_ID_APNG ||
					stream->codecpar->codec_id == AV_CODEC_ID_GIF;

				if ( codec_ctx->sample_aspect_ratio.num != 0 )
					m_aspect_ratio = av_q2d( codec_ctx->sample_aspect_ratio );
				if ( m_imp->m_format_ctx->streams[stream_id]->sample_aspect_ratio.num != 0 )
					m_aspect_ratio = av_q2d( m_imp->m_format_ctx->streams[stream_id]->sample_aspect_ratio );

				m_video->disptex_width = codec_ctx->width;
				m_video->disptex_height = codec_ctx->height;

				m_video->init_rgba_buffer();
			}
		}
	}

	if ( !m_video && !m_audio )
		return false;

	const AVStream *stream = m_imp->m_format_ctx->streams[m_video ? m_video->stream_id : m_audio->stream_id];
	if ( stream->duration != AV_NOPTS_VALUE && stream->duration > 0 )
		m_imp->m_duration = fe_seconds( stream->duration * av_q2d( stream->time_base ) );
	else if ( m_imp->m_format_ctx->duration != AV_NOPTS_VALUE && m_imp->m_format_ctx->duration > 0 )
		m_imp->m_duration = fe_seconds( static_cast<double>( m_imp->m_format_ctx->duration ) / AV_TIME_BASE );
	return true;
}

bool FeMedia::read_packet()
{
	if ( m_imp->m_read_eof || m_imp->m_stop )
		return false;

	AVPacket *packet = av_packet_alloc();
	if ( !packet )
	{
		FeLog() << "Error allocating media packet: " << FORMAT_CTX_URL << std::endl;
		m_imp->m_read_eof = true;
		return false;
	}
	const int result = av_read_frame( m_imp->m_format_ctx, packet );
	if ( result < 0 )
	{
		if ( result != AVERROR_EOF && !m_imp->m_stop )
		{
			char error[256];
			av_strerror( result, error, sizeof( error ) );
			FeLog() << "Error reading media: " << FORMAT_CTX_URL << ": " << error << std::endl;
		}
		m_imp->m_read_eof = true;
		av_packet_free( &packet );
		return false;
	}

	if ( m_audio && !m_audio->at_end && packet->stream_index == m_audio->stream_id )
		m_audio->push_packet( packet );
	else if ( m_video && !m_video->at_end && packet->stream_index == m_video->stream_id )
		m_video->push_packet( packet );
	else
		av_packet_free( &packet );
	return true;
}

bool FeMedia::pump_audio()
{
	if ( !m_audio )
		return false;
	if ( m_audio_stream.is_stale() )
	{
		m_audio_stream.destroy();
		FeDebug() << "Reopening media audio stream after device change." << std::endl;
	}
	if ( !ensure_audio_stream() )
	{
		m_audio->at_end = true;
		clear_pending_audio();
		m_imp->m_audio_clock_end = -1;
		return false;
	}

	bool progress = false;
	while ( !m_imp->m_stop && m_audio_stream.queued_frames() < FE_MEDIA_AUDIO_QUEUE_TARGET_FRAMES )
	{
		if ( !m_audio_pending_samples.empty() )
		{
			if ( !queue_pending_audio( m_audio->processed_samples ) )
			{
				FeLog() << "Error queueing media audio: " << SDL_GetError() << std::endl;
				m_audio->at_end = true;
				clear_pending_audio();
				m_imp->m_audio_clock_end = -1;
				return false;
			}
			progress = true;
			if ( !m_audio_pending_samples.empty() )
				break;
		}
		if ( m_audio->at_end )
			break;

		const int result = m_audio->decode_frame( m_imp->m_read_eof );
		if ( result == AVERROR( EAGAIN ) )
			break;
		m_audio->decoded_samples.clear();
		if ( result == 0 || result == AVERROR_EOF )
		{
			if ( !m_audio->process_frame( result == 0 ? m_audio->frame : NULL, m_audio->decoded_samples ) )
			{
				m_audio->at_end = true;
				break;
			}
			append_pending_audio( m_audio->decoded_samples );
		}
		else
		{
			char error[256];
			av_strerror( result, error, sizeof( error ) );
			FeLog() << "Error decoding audio: " << FORMAT_CTX_URL << ": " << error << std::endl;
			m_audio->at_end = true;
		}
		progress = true;
	}
	return progress;
}

int fe_media_interrupt( void *opaque )
{
	return static_cast<FeMediaImp *>( opaque )->m_stop ? 1 : 0;
}

void FeMedia::media_thread()
{
	// Producers can use every idle core, but must yield to presentation and
	// the audio device when their frame deadlines arrive together.
	if ( !SDL_SetCurrentThreadPriority( SDL_THREAD_PRIORITY_LOW ) )
		FeDebug() << "Unable to lower media thread priority: " << SDL_GetError() << std::endl;
	m_imp->m_format_ctx->interrupt_callback.callback = &fe_media_interrupt;
	m_imp->m_format_ctx->interrupt_callback.opaque = m_imp;
	while ( !m_imp->m_stop )
	{
		if ( m_imp->m_rewind )
		{
			const int stream_id = m_video ? m_video->stream_id : m_audio->stream_id;
			if ( av_seek_frame( m_imp->m_format_ctx, stream_id, 0, AVSEEK_FLAG_BACKWARD ) < 0 )
			{
				if ( !m_imp->m_stop )
					FeLog() << "Error rewinding media: " << FORMAT_CTX_URL << std::endl;
				m_imp->m_stop = true;
				break;
			}
			m_imp->m_rewind = false;
		}

		float applied_volume = getVolume();
		std::ignore = m_audio_stream.set_gain( applied_volume / 100.0f );
		unsigned int video_frames = 0;
		std::int64_t audio_clock_time = 0;
		while ( !m_imp->m_stop )
		{
			bool progress = false;
			if ( applied_volume != getVolume() )
			{
				applied_volume = getVolume();
				std::ignore = m_audio_stream.set_gain( applied_volume / 100.0f );
			}

			const int queued = m_audio_stream.queued_frames();
			const bool need_audio = m_audio && queued <= FE_MEDIA_AUDIO_QUEUE_LOW_FRAMES &&
				( !m_audio->at_end || !m_audio_pending_samples.empty() );
			if ( need_audio && ( !m_audio->needs_packet || !m_audio_pending_samples.empty() || m_imp->m_read_eof ) )
			{
				FeMediaDecodeLock decode_lock( m_imp->m_stop, true );
				if ( !decode_lock.acquired() )
					break;
				progress = pump_audio();
			}

			// SDL consumes PCM in blocks. Follow its long-term clock without
			// turning every block boundary into a video timing discontinuity.
			if ( m_imp->m_audio_frames > 0 && m_imp->m_audio_clock_end >= 0 )
			{
				const std::uint64_t remaining = static_cast<std::uint64_t>( m_audio_stream.queued_frames() );
				const std::uint64_t played = m_imp->m_audio_frames - std::min( remaining, m_imp->m_audio_frames );
				const bool ended = m_audio->at_end && m_audio_pending_samples.empty() && remaining == 0;
				const int rate = FeSdlAudioBackend::get().sample_rate();
				const std::int64_t elapsed = m_imp->m_timer.getElapsedTime().asMicroseconds();
				const std::int64_t offset = static_cast<std::int64_t>( played * 1000000 / rate ) - elapsed;
				const std::int64_t previous = m_imp->m_audio_clock_offset.load( std::memory_order_relaxed );
				const std::int64_t interval = std::min<std::int64_t>( elapsed - audio_clock_time, 1000000 );
				// Average block-sized measurement noise over one second. Only
				// startup or an actual underrun needs an immediate new anchor.
				m_imp->m_audio_clock_offset = ( m_imp->m_audio_clock_end == 0 || ( remaining == 0 && !ended ) )
					? offset : previous + ( offset - previous ) * interval / 1000000;
				audio_clock_time = elapsed;
				// Keep advancing any video tail after the last PCM has drained.
				m_imp->m_audio_clock_end.store( ended ? -1 :
					static_cast<std::int64_t>( m_imp->m_audio_frames * 1000000 / rate ), std::memory_order_release );
			}
			else if ( m_audio && m_audio->at_end )
				m_imp->m_audio_clock_end = -1;

			const bool frame_room = m_video && m_video->has_frame_room();
			const bool need_video = frame_room && !m_video->at_end;
			if ( need_video && ( !m_video->needs_packet || m_imp->m_read_eof ) )
			{
				FeMediaDecodeLock decode_lock( m_imp->m_stop, false );
				if ( !decode_lock.acquired() )
					break;
				const int result = m_video->decode_frame( m_imp->m_read_eof );
				if ( result == 0 )
				{
					if ( !m_video->queue_frame() )
						m_video->at_end = true;
					++video_frames;
					progress = true;
				}
				else if ( result != AVERROR( EAGAIN ) )
				{
					if ( result != AVERROR_EOF )
					{
						char error[256];
						av_strerror( result, error, sizeof( error ) );
						FeLog() << "Error decoding video: " << FORMAT_CTX_URL << ": " << error << std::endl;
					}
					m_video->at_end = true;
					progress = true;
				}
			}

			const bool need_packet = ( need_audio && m_audio->needs_packet && !m_audio->at_end ) ||
				( need_video && m_video->needs_packet && !m_video->at_end );
			if ( need_packet && !m_imp->m_read_eof &&
				( !m_audio || m_audio->at_end || m_audio->has_packet_room() ) &&
				( !m_video || m_video->at_end || m_video->has_packet_room() ) )
			{
				std::ignore = read_packet();
				progress = true;
			}

			const bool audio_done = !m_audio ||
				( m_audio->at_end && m_audio_pending_samples.empty() && m_audio_stream.queued_frames() == 0 );
			const bool video_done = !m_video || ( m_video->at_end && !m_video->frames_pending() );
			if ( audio_done && video_done )
				break;
			if ( progress )
				continue;

			FeTime delay = m_video ? m_video->next_frame_delay() : fe_milliseconds( 50 );
			if ( m_audio )
			{
				const int headroom = m_audio_stream.queued_frames() - FE_MEDIA_AUDIO_QUEUE_LOW_FRAMES;
				const FeTime audio_delay = headroom > 0
					? fe_seconds( static_cast<double>( headroom ) / FeSdlAudioBackend::get().sample_rate() )
					: fe_milliseconds( 10 );
				delay = std::min( delay, audio_delay );
			}
			std::unique_lock<std::mutex> lock( m_imp->m_mutex );
			if ( !m_imp->m_stop )
				m_imp->m_wake.wait_for( lock, std::chrono::microseconds( std::max<std::int64_t>( 1000, delay.asMicroseconds() ) ) );
		}

		if ( m_imp->m_stop )
			break;
		FeDebug() << "End media playback - " << FORMAT_CTX_URL
			<< " - decoded video frames=" << video_frames << std::endl;
		// Publish EOF only after all producer work is finished. play() resets
		// the clock under this mutex before releasing us to rewind.
		std::unique_lock<std::mutex> lock( m_imp->m_mutex );
		m_imp->m_playing = false;
		while ( !m_imp->m_stop && !m_imp->m_restart )
			m_imp->m_wake.wait( lock );
		if ( m_imp->m_stop )
			break;
		m_imp->m_restart = false;
		lock.unlock();
		reset_decoder();
		m_imp->m_rewind = true;
	}

	if ( m_imp->m_stop )
		m_audio_stream.clear();
	m_imp->m_playing = false;
	FeDebug() << "End Media Thread - " << FORMAT_CTX_URL << std::endl;
}

bool FeMedia::tick()
{
	return m_video && m_video->tick();
}

bool FeMedia::get_video_frame_dimensions( unsigned int &width, unsigned int &height )
{
	return m_video && m_video->get_rgba_frame_dimensions( width, height );
}

bool FeMedia::copy_video_frame_rgba( std::vector<unsigned char> &pixels, unsigned int &width, unsigned int &height )
{
	return m_video && m_video->copy_rgba_frame( pixels, width, height );
}

bool FeMedia::copy_video_frame_rgba_to( void *pixels, std::size_t pixel_count, unsigned int &width, unsigned int &height )
{
	return m_video && m_video->copy_rgba_frame_to( pixels, pixel_count, width, height );
}

void FeMedia::clear_video_frame()
{
	if ( m_video )
		m_video->clear_rgba_frame();
}

unsigned long long FeMedia::get_video_frame_serial() const
{
	return m_video
		? m_video->frame_serial.load( std::memory_order_acquire )
		: 0;
}




bool FeMedia::is_supported_media_file( const std::string &filename )
{
	// Work around for FFmpeg not recognizing certain file extensions
	// that it supports (xmv reported as of Dec 2015)
	//
	size_t pos = filename.find_last_of( '.' );
	if ( pos != std::string::npos )
	{
		std::string f = filename.substr( pos + 1 );
		std::transform( f.begin(), f.end(), f.begin(), ::tolower );
		return ( av_guess_format( f.c_str(), filename.c_str(), NULL ) != NULL );
	}

	return ( av_guess_format( NULL, filename.c_str(), NULL ) != NULL );
}

bool FeMedia::is_multiframe() const
{
	return m_video && m_imp->m_multiframe;
}

float FeMedia::get_aspect_ratio() const
{
	return m_aspect_ratio;
}

FeTime FeMedia::get_duration() const
{
	return m_imp->m_duration;
}

#if FE_HWACCEL
//
// A list of the 'HWDEVICE' ffmpeg hwaccels that we support
//
enum AVHWDeviceType fe_hw_accels[] =
{
#ifdef FE_HWACCEL_VAAPI
	AV_HWDEVICE_TYPE_VAAPI,
#endif
#ifdef FE_HWACCEL_VDPAU
	AV_HWDEVICE_TYPE_VDPAU,
#endif

#ifdef SDL_PLATFORM_WINDOWS
	AV_HWDEVICE_TYPE_DXVA2,
 #if ( LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( 56, 67, 100 ))
	AV_HWDEVICE_TYPE_D3D11VA,
 #endif
#endif

#ifdef SDL_PLATFORM_MACOS
 #if ( LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( 57, 63, 100 ))
	AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
 #endif
#endif

	AV_HWDEVICE_TYPE_NONE
};
#endif

void FeMedia::get_decoder_list( std::vector< std::string > &l )
{
	l.clear();

	l.push_back( "software" );

#if defined( USE_MMAL )
	//
	// Raspberry Pi specific - check for mmal
	//
	if ( avcodec_find_decoder_by_name( "mpeg4_mmal" ))
		l.push_back( "mmal" );
#endif

#if FE_HWACCEL
	for ( int i=0; fe_hw_accels[i] != AV_HWDEVICE_TYPE_NONE; i++ )
	{
		const char *name = av_hwdevice_get_type_name( fe_hw_accels[i] );
		if ( name != NULL )
			l.push_back( name );
	}
#endif
}

std::string FeMedia::get_current_decoder()
{
	return g_decoder;
}

void FeMedia::set_current_decoder( const std::string &l )
{
	g_decoder = l;

#if FE_HWACCEL
	if ( g_hw_device_ctx )
	{
		av_buffer_unref( &g_hw_device_ctx );
		FeDebug() << "Closed hardware video decoder context: "
			<< av_hwdevice_get_type_name( g_hw_device_type ) << std::endl;
		g_hw_device_ctx = NULL;
		g_hw_device_type = AV_HWDEVICE_TYPE_NONE;
	}

	if ( !l.empty() && ( l.compare( "software" ) != 0 ) && ( l.compare( "mmal" ) != 0 ))
	{
		for ( int i=0; fe_hw_accels[i] != AV_HWDEVICE_TYPE_NONE; i++ )
		{
			if ( l.compare( av_hwdevice_get_type_name( fe_hw_accels[i] )) != 0 )
				continue;

			int ret = av_hwdevice_ctx_create( &g_hw_device_ctx, fe_hw_accels[i], NULL, NULL, 0 );

			if ( ret < 0 )
			{
				FeLog() << "Error creating hardware video decoder context: "
					<< av_hwdevice_get_type_name( fe_hw_accels[i] ) << std::endl;
				g_hw_device_ctx = NULL;
				g_hw_device_type = AV_HWDEVICE_TYPE_NONE;
				return;
			}

			g_hw_device_type = fe_hw_accels[i];

			FeDebug() << "Created hardware video decoder context: "
				<< av_hwdevice_get_type_name( fe_hw_accels[i] ) << std::endl;
			break;
		}
	}
#endif
}


//
// Try to use a hardware accelerated decoder where readily available...
//
void try_hw_accel( AVCodecContext *&codec_ctx, FeAVCodec *&dec )
{
	if ( g_decoder.empty() || ( g_decoder.compare( "software" ) == 0 ))
		return;

#if defined( USE_MMAL )
	if ( g_decoder.compare( "mmal" ) == 0 )
	{
		switch( dec->id )
		{

		case AV_CODEC_ID_MPEG4:
			dec = avcodec_find_decoder_by_name( "mpeg4_mmal" );
			return;


		case AV_CODEC_ID_H264:
			dec = avcodec_find_decoder_by_name( "h264_mmal" );
			return;


		case AV_CODEC_ID_MPEG2VIDEO:
			dec = avcodec_find_decoder_by_name( "mpeg2_mmal" );
			return;

		case AV_CODEC_ID_VC1:
			dec = avcodec_find_decoder_by_name( "vc1_mmal" );
			return;

		default:
			break;
		};

		return;
	}
#endif

#if FE_HWACCEL
	if ( g_hw_device_ctx && g_hw_device_type != AV_HWDEVICE_TYPE_NONE )
	{
		if ( g_decoder.compare( av_hwdevice_get_type_name( g_hw_device_type )) == 0 )
		{
			codec_ctx->hw_device_ctx = av_buffer_ref( g_hw_device_ctx );
			codec_ctx->hwaccel_flags = AV_HWACCEL_FLAG_IGNORE_LEVEL;
		}
	}
#endif
}

FeAudioVisualiser* FeMedia::get_audio_visualiser() const
{
	return m_audio_effects->get_effect<FeAudioVisualiser>();
}

float FeMedia::get_vu_left()
{
	auto* visualiser = get_audio_visualiser();
	return visualiser ? visualiser->get_vu_left() : 0.0f;
}

float FeMedia::get_vu_right()
{
	auto* visualiser = get_audio_visualiser();
	return visualiser ? visualiser->get_vu_right() : 0.0f;
}

float FeMedia::get_vu_mono()
{
	auto* visualiser = get_audio_visualiser();
	return visualiser ? visualiser->get_vu_mono() : 0.0f;
}

const std::vector<float> *FeMedia::get_fft_mono_ptr()
{
	auto* visualiser = get_audio_visualiser();
	if ( visualiser )
		return visualiser->get_fft_mono_ptr();
	return nullptr;
}

const std::vector<float> *FeMedia::get_fft_left_ptr()
{
	auto* visualiser = get_audio_visualiser();
	if ( visualiser )
		return visualiser->get_fft_left_ptr();
	return nullptr;
}

const std::vector<float> *FeMedia::get_fft_right_ptr()
{
	auto* visualiser = get_audio_visualiser();
	if ( visualiser )
		return visualiser->get_fft_right_ptr();
	return nullptr;
}

void FeMedia::set_fft_bands( int count )
{
	auto* visualiser = get_audio_visualiser();
	if ( visualiser )
		visualiser->set_fft_bands( count );
}

int FeMedia::get_fft_bands() const
{
	auto* visualiser = get_audio_visualiser();
	if ( visualiser )
		return visualiser->get_fft_bands();

	return 32;
}
