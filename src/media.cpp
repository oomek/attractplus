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


extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/display.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>

#define FE_HWACCEL ( LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( 55, 78, 100 ))

#if FE_HWACCEL
#include <libavutil/hwcontext.h>
#endif
}

#include <queue>
#include <iostream>
#include <thread>
#include <condition_variable>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cmath>
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
	constexpr int FE_MEDIA_AUDIO_QUEUE_TARGET_FRAMES = 960;
	constexpr int FE_MEDIA_AUDIO_PROCESS_CHUNK_FRAMES = 480;

	bool fe_is_attached_picture_stream( const AVStream *stream )
	{
		if ( !stream )
			return false;

		if ( stream->disposition & AV_DISPOSITION_ATTACHED_PIC )
			return true;

		return stream->attached_pic.size > 0;
	}

	double normalize_degrees( double degrees )
	{
		while ( degrees < 0.0 )
			degrees += 360.0;
		while ( degrees >= 360.0 )
			degrees -= 360.0;
		return degrees;
	}

	int get_display_rotation_quadrants( const AVStream *stream )
	{
		if ( !stream || !stream->codecpar )
			return 0;

		const AVPacketSideData *side_data =
			av_packet_side_data_get(
				stream->codecpar->coded_side_data,
				stream->codecpar->nb_coded_side_data,
				AV_PKT_DATA_DISPLAYMATRIX );
		if ( !side_data || !side_data->data || side_data->size < ( sizeof( std::int32_t ) * 9u ) )
			return 0;

		double clockwise_degrees =
			-av_display_rotation_get( reinterpret_cast<const std::int32_t *>( side_data->data ) );
		if ( std::isnan( clockwise_degrees ) )
			return 0;

		clockwise_degrees = normalize_degrees( clockwise_degrees );
		return static_cast<int>( std::floor( ( clockwise_degrees + 45.0 ) / 90.0 ) ) % 4;
	}

	void copy_rotated_rgba(
		const std::uint8_t *source,
		int source_width,
		int source_height,
		int clockwise_quadrants,
		std::uint8_t *destination )
	{
		if ( !source || !destination || source_width <= 0 || source_height <= 0 )
			return;

		if ( clockwise_quadrants == 0 )
		{
			const std::size_t byte_count =
				static_cast<std::size_t>( source_width ) *
				static_cast<std::size_t>( source_height ) * 4u;
			std::memcpy( destination, source, byte_count );
			return;
		}

		const int destination_width =
			( clockwise_quadrants % 2 ) ? source_height : source_width;
		for ( int source_y = 0; source_y < source_height; ++source_y )
		{
			for ( int source_x = 0; source_x < source_width; ++source_x )
			{
				int destination_x = source_x;
				int destination_y = source_y;
				switch ( clockwise_quadrants )
				{
				case 1:
					destination_x = source_height - 1 - source_y;
					destination_y = source_x;
					break;

				case 2:
					destination_x = source_width - 1 - source_x;
					destination_y = source_height - 1 - source_y;
					break;

				case 3:
					destination_x = source_y;
					destination_y = source_width - 1 - source_x;
					break;

				default:
					break;
				}

				const std::size_t source_index =
					( static_cast<std::size_t>( source_y ) * static_cast<std::size_t>( source_width )
						+ static_cast<std::size_t>( source_x ) ) * 4u;
				const std::size_t destination_index =
					( static_cast<std::size_t>( destination_y ) * static_cast<std::size_t>( destination_width )
						+ static_cast<std::size_t>( destination_x ) ) * 4u;
				std::memcpy( destination + destination_index, source + source_index, 4u );
			}
		}
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

#define MAX_AUDIO_FRAME_SIZE 256000

//
// Container for our general implementation
//
class FeMediaImp
{
public:
	FeMediaImp( FeMedia::Type t );
	void close();

	FeMedia::Type m_type;
	AVFormatContext *m_format_ctx;
	AVIOContext *m_io_ctx;
	std::recursive_mutex m_read_mutex;
	bool m_read_eof;
};

//
// Base class for our implementation of the audio and video components
//
class FeBaseStream
{
private:
	//
	// Queue containing the next packet to process for this stream
	//
	std::queue <AVPacket *> m_packetq;
	std::mutex m_packetq_mutex;

public:
	virtual ~FeBaseStream();

	bool at_end;					// set when at the end of our input
	bool far_behind;
	AVCodecContext *codec_ctx;
	FeAVCodec *codec;
	int stream_id;

	FeBaseStream();
	virtual void stop();
	AVPacket *pop_packet();
	void push_packet( AVPacket *pkt );
	void clear_packet_queue();
};

//
// Container for our implementation of the audio component
//
class FeAudioImp : public FeBaseStream
{
public:
	SwrContext *resample_ctx;

	FeAudioImp();
	~FeAudioImp();

	bool process_frame( AVFrame *frame, std::vector<float> &samples );
};

class FeVideoImp : public FeBaseStream
{
	friend class FeMedia;

private:
	//
	// Video decoding and colour conversion runs on a dedicated thread.
	// The main thread consumes decoded RGBA frame data.
	//
	std::thread m_video_thread;
	FeMedia *m_parent;
	std::uint8_t *rgba_buffer[4];
	int rgba_linesize[4];
	FeTime half_frame_offset;
	int rgba_width;
	int rgba_height;
#if FE_HWACCEL
	AVPixelFormat hwaccel_output_format;
	bool hw_retrieve_data( AVFrame *f );
#endif

public:
	std::atomic<bool> run_video_thread;
	FeTime time_base;
	FeTime max_sleep;
	FeClock video_timer;
	int disptex_width;
	int disptex_height;
	int display_rotation_quadrants;

	//
	// The video thread sets display_frame when the next image frame is decoded.
	// The main thread then marks that frame as consumed.
	//
	std::recursive_mutex image_swap_mutex;
	std::uint8_t *display_frame;
	std::condition_variable_any frame_displayed;
	std::atomic<unsigned long long> frame_serial;

	FeVideoImp( FeMedia *parent );
	~FeVideoImp();

	void play();
	void stop();

	void signal_stop(); // signal the bg thread we are stopping, without blocking

	bool get_rgba_frame_dimensions( unsigned int &width, unsigned int &height );
	void init_rgba_buffer();
	bool copy_rgba_frame( std::vector<unsigned char> &pixels, unsigned int &width, unsigned int &height );
	bool copy_rgba_frame_to( void *pixels, std::size_t pixel_count, unsigned int &width, unsigned int &height );
	void video_thread();
};

FeMediaImp::FeMediaImp( FeMedia::Type t )
	: m_type( t ),
	m_format_ctx( NULL ),
	m_io_ctx( NULL ),
	m_read_eof( false )
{
}

void FeMediaImp::close()
{
	std::lock_guard<std::recursive_mutex> l( m_read_mutex );

	if ( m_format_ctx )
	{
		avformat_close_input( &m_format_ctx );
		m_format_ctx = NULL;
	}

	if ( m_io_ctx )
	{
		if ( m_io_ctx->opaque )
			delete static_cast<FeInputStream *>( m_io_ctx->opaque );

		av_free( m_io_ctx->buffer );
		av_free( m_io_ctx );
		m_io_ctx=NULL;
	}

	m_read_eof=false;
}

FeBaseStream::FeBaseStream()
	: at_end( false ),
	far_behind( false ),
	codec_ctx( NULL ),
	codec( NULL ),
	stream_id( -1 )
{
}

FeBaseStream::~FeBaseStream()
{
	if ( codec_ctx )
		avcodec_free_context( &codec_ctx );

	clear_packet_queue();

	codec = NULL;
	at_end = false;
	far_behind = false;
	stream_id = -1;
}

void FeBaseStream::stop()
{
	clear_packet_queue();
	at_end=false;
	far_behind = false;
}

AVPacket *FeBaseStream::pop_packet()
{
	std::lock_guard<std::mutex> l( m_packetq_mutex );

	if ( m_packetq.empty() )
		return NULL;

	AVPacket *p = m_packetq.front();
	m_packetq.pop();
	return p;
}

void FeBaseStream::clear_packet_queue()
{
	std::lock_guard<std::mutex> l( m_packetq_mutex );

	while ( !m_packetq.empty() )
	{
		AVPacket *p = m_packetq.front();
		m_packetq.pop();
		av_packet_free( &p );
	}
}

void FeBaseStream::push_packet( AVPacket *pkt )
{
	std::lock_guard<std::mutex> l( m_packetq_mutex );
	m_packetq.push( pkt );
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

	const int in_sample_rate = frame->sample_rate > 0 ? frame->sample_rate : codec_ctx->sample_rate;
	const int max_out_samples = av_rescale_rnd(
		swr_get_delay( resample_ctx, in_sample_rate ) + frame->nb_samples,
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
		const_cast<const uint8_t **>( frame->extended_data ),
		frame->nb_samples );

	if ( out_samples < 0 )
	{
		FeLog() << "Error performing media audio conversion." << std::endl;
		return false;
	}

	converted_samples.resize( static_cast<std::size_t>( out_samples ) * FeSdlAudioBackend::get().channel_count() );
	samples.insert( samples.end(), converted_samples.begin(), converted_samples.end() );
	return true;
}


FeVideoImp::FeVideoImp( FeMedia *p )
		: FeBaseStream(),
		m_video_thread(),
		m_parent( p ),
		rgba_buffer(),
		rgba_linesize(),
		rgba_width( 0 ),
		rgba_height( 0 ),
#if FE_HWACCEL
		hwaccel_output_format( AV_PIX_FMT_NONE ),
#endif
		run_video_thread( false ),
		disptex_width( 0 ),
		disptex_height( 0 ),
		display_rotation_quadrants( 0 ),
		display_frame( NULL ),
		frame_serial( 0 )
{
	video_timer.reset();
	FePresent *fep = FePresent::script_get_fep();
	half_frame_offset = fe_milliseconds( 500 / fep->get_refresh_rate() );
}

FeVideoImp::~FeVideoImp()
{
	stop();

	if ( rgba_buffer[0] )
		av_freep( &rgba_buffer[0] );
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
	if ( hwaccel_output_format == AV_PIX_FMT_NONE )
	{
		hwaccel_output_format = hw_get_output_format( codec_ctx->hw_frames_ctx );

		FeDebug() << "HWAccel output pixel format: "
			<< av_pix_fmt_desc_get( hwaccel_output_format )->name << std::endl;
	}

	sw_frame->format = hwaccel_output_format;

	int err = av_hwframe_transfer_data( sw_frame, f, 0 );
	if ( err < 0 )
		FeLog() << "Error transferring hardware frame data." << std::endl;

	err = av_frame_copy_props( sw_frame, f );
	if ( err < 0 )
		FeLog() << "Error copying hardware frame properties." << std::endl;

	av_frame_unref( f );
	av_frame_move_ref( f, sw_frame );
	av_frame_free( &sw_frame );

	return true;
}

#endif

void FeVideoImp::play()
{
	run_video_thread = true;
	video_timer.restart();
	m_video_thread = std::thread( &FeVideoImp::video_thread, this );
}

void FeVideoImp::stop()
{
	if ( run_video_thread )
	{
		run_video_thread = false;
		video_timer.stop();
	}

	if ( m_video_thread.joinable() )
	{
		frame_displayed.notify_one();
		m_video_thread.join();
	}

	FeBaseStream::stop();
}

void FeVideoImp::signal_stop()
{
	if ( run_video_thread )
	{
		run_video_thread = false;
		video_timer.stop();
		frame_displayed.notify_one();
	}
}

namespace
{
	void set_avdiscard_from_qscore( AVCodecContext *c, int qscore )
	{
		AVDiscard d = AVDISCARD_DEFAULT;

		// Note: we aren't ever setting AVDISCARD_ALL
		if ( qscore <= 2 )
				d = AVDISCARD_NONKEY;
		else if ( qscore <= 4 )
				d = AVDISCARD_BIDIR;
		else if ( qscore <= 8 )
			d = AVDISCARD_NONREF;

		c->skip_loop_filter = d;
		c->skip_idct = d;
		c->skip_frame = d;
	}
}

void FeVideoImp::init_rgba_buffer()
{
	std::lock_guard<std::recursive_mutex> l( image_swap_mutex );

	if ( rgba_buffer[0] )
		av_freep( &rgba_buffer[0] );

	int ret = av_image_alloc( rgba_buffer, rgba_linesize,
			rgba_width, rgba_height,
			AV_PIX_FMT_RGBA, 32 );

	if ( ret < 0 )
		FeLog() << "Error allocating rgba buffer" << std::endl;

	// Override linesize with original video width
	// to remove stride padding with sws_scale
	rgba_linesize[0] = rgba_width * 4;
}

bool FeVideoImp::copy_rgba_frame( std::vector<unsigned char> &pixels, unsigned int &width, unsigned int &height )
{
	std::lock_guard<std::recursive_mutex> l( image_swap_mutex );
	if ( !rgba_buffer[0] || disptex_width <= 0 || disptex_height <= 0 )
		return false;

	width = static_cast<unsigned int>( disptex_width );
	height = static_cast<unsigned int>( disptex_height );
	const std::size_t data_size = static_cast<std::size_t>( width ) * static_cast<std::size_t>( height ) * 4;
	pixels.resize( data_size );
	copy_rotated_rgba(
		rgba_buffer[0],
		rgba_width,
		rgba_height,
		display_rotation_quadrants,
		pixels.data() );
	return true;
}

bool FeVideoImp::get_rgba_frame_dimensions( unsigned int &width, unsigned int &height )
{
	std::lock_guard<std::recursive_mutex> l( image_swap_mutex );
	if ( !rgba_buffer[0] || disptex_width <= 0 || disptex_height <= 0 )
		return false;

	width = static_cast<unsigned int>( disptex_width );
	height = static_cast<unsigned int>( disptex_height );
	return true;
}

bool FeVideoImp::copy_rgba_frame_to( void *pixels, std::size_t pixel_count, unsigned int &width, unsigned int &height )
{
	std::lock_guard<std::recursive_mutex> l( image_swap_mutex );
	if ( !pixels || !rgba_buffer[0] || disptex_width <= 0 || disptex_height <= 0 )
		return false;

	width = static_cast<unsigned int>( disptex_width );
	height = static_cast<unsigned int>( disptex_height );
	const std::size_t data_size = static_cast<std::size_t>( width ) * static_cast<std::size_t>( height ) * 4;
	if ( pixel_count < data_size )
		return false;

	copy_rotated_rgba(
		rgba_buffer[0],
		rgba_width,
		rgba_height,
		display_rotation_quadrants,
		static_cast<std::uint8_t *>( pixels ) );
	return true;
}

void FeVideoImp::video_thread()
{
	const int QMAX = 16;
	const int QMIN = 0;
	int qscore( 10 ); // quality scoring
	int displayed( 0 ), qscore_accum( 0 );

	AVFrame *detached_frame = NULL;
	bool degrading = false;
	bool do_flush = false;
	bool flush_packet_sent = false;

	int64_t prev_pts = 0;
	int64_t prev_duration = 0;
	SwsContext *sws_ctx = NULL;

	FeTime wait_time;

	if ( !rgba_buffer[0] )
	{
		FeLog() << "Error initializing video thread" << std::endl;
		goto the_end;
	}

	while ( run_video_thread )
	{
		bool do_process = true;

		//
		// If we are falling behind for more than 2 seconds
		// it can only mean that we are in suspend/hibernation state,
		// so we flag the video to be restarted on the next tick.
		// This prevents displaying only keyframes for several seconds on wake.
		//
		if ( wait_time < fe_seconds( -5.0 ) )
		{
			wait_time = FeTime();
			far_behind = true;
			run_video_thread = false;
			video_timer.stop();
		}

		//
		// First, display queued frame
		//
		if ( detached_frame )
		{

			FeTime frame_time;
			{
				std::lock_guard<std::recursive_mutex> l( m_parent->m_imp->m_read_mutex );
				if ( m_parent->m_imp->m_format_ctx && stream_id >= 0 )
				{
					frame_time = fe_seconds( detached_frame->pts
						* av_q2d( m_parent->m_imp->m_format_ctx->streams[stream_id]->time_base ));
				}
				else
				{
					frame_time = FeTime();
				}
			}
			wait_time = frame_time - m_parent->get_video_time() + half_frame_offset;

			if ( wait_time < max_sleep )
			{
				if ( wait_time < -time_base )

				{
					// If we are falling behind, we may need to start discarding
					// frames to catch up
					//
					if ( qscore > QMIN )
						qscore--;

					set_avdiscard_from_qscore( codec_ctx, qscore );
					degrading = true;
				}
				else if ( wait_time >= FeTime() )
				{
					//
					// We are ahead and can sleep until presentation time
					//
					FeClock clock;
					while ( run_video_thread && clock.getElapsedTime() < wait_time )
						fe_sleep( fe_milliseconds( 1 ) );

					if ( !run_video_thread )
						goto the_end;

					degrading = false;

				}

				qscore_accum += qscore;

#if FE_HWACCEL
				hw_retrieve_data( detached_frame );
#endif

				if ( !sws_ctx )
				{
					enum AVPixelFormat pfmt = codec_ctx->pix_fmt;
#if FE_HWACCEL
					if ( hwaccel_output_format != AV_PIX_FMT_NONE )
						pfmt = hwaccel_output_format;
#endif
					int sws_flags( SWS_BILINEAR );
					if (( codec_ctx->width & 0x7 ) || ( codec_ctx->height & 0x7 ))
						sws_flags |= SWS_ACCURATE_RND;

					sws_ctx = sws_getCachedContext( NULL,
						codec_ctx->width, codec_ctx->height, pfmt,
						rgba_width, rgba_height, AV_PIX_FMT_RGBA,
						sws_flags, NULL, NULL, NULL );

					if ( !sws_ctx )
					{
						FeLog() << "Error allocating SwsContext" << std::endl;
						goto the_end;
					}
				}

				std::lock_guard<std::recursive_mutex> l( image_swap_mutex );
				displayed++;

				sws_scale( sws_ctx, detached_frame->data, detached_frame->linesize,
							0, codec_ctx->height, rgba_buffer,
							rgba_linesize );

				display_frame = rgba_buffer[0];

				av_frame_free( &detached_frame );
				detached_frame = NULL;

				do_process = false;
			}
			//
			// if we didn't do anything above, then we go into the queue
			// management process below
			//
		}

		if ( do_flush && flush_packet_sent && !detached_frame )
		{
			// We've sent the flush packet and have no more frames to display
			// Try to get any remaining buffered frames
			AVFrame *raw_frame = av_frame_alloc();
			int r = avcodec_receive_frame( codec_ctx, raw_frame );

			if ( r == 0 )
			{
				raw_frame->pts = raw_frame->best_effort_timestamp;
				if ( raw_frame->pts == AV_NOPTS_VALUE )
					raw_frame->pts = prev_pts + prev_duration;

#if ( LIBAVUTIL_VERSION_MICRO >= 100 )
				if ( raw_frame->pts < prev_pts )
					raw_frame->pts = prev_pts + prev_duration;

				prev_pts = raw_frame->pts;
#if HAVE_DURATION
				prev_duration = raw_frame->duration;
#else
				prev_duration = raw_frame->pkt_duration;
#endif
#endif
				detached_frame = raw_frame;
			}
			else
			{
				av_frame_free( &raw_frame );
				if ( r == AVERROR_EOF )
				{
					// Decoder is fully drained so now we can goto the_end
					// Wait for the main thread to display the last frame
					std::unique_lock<std::recursive_mutex> lock( image_swap_mutex );
					frame_displayed.wait( lock, [this]{ return !display_frame || !run_video_thread; });

					goto the_end;
				}
			}
		}

		if ( do_process )
		{
			if ( !detached_frame )
			{
				//
				// get next packet
				//
				AVPacket *packet = pop_packet();
				if ( packet == NULL )
				{
					if ( !m_parent->end_of_file() )
						m_parent->read_packet();
					else
						do_flush = true; // NULL packet will be fed to avcodec_send_packet()
				}

				if (( packet != NULL ) || ( do_flush && !flush_packet_sent ))
				{
					//
					// decompress packet and put it in our frame queue
					//
					int r = avcodec_send_packet( codec_ctx, packet );
					if (( r < 0 ) && ( r != AVERROR( EAGAIN )))
					{
						char buff[256];
						av_strerror( r, buff, 256 );
						FeLog() << "Error decoding video (sending packet): "
							<< buff << std::endl;
					}

					if ( do_flush && !packet )
						flush_packet_sent = true;

					AVFrame *raw_frame = av_frame_alloc();
					r = avcodec_receive_frame( codec_ctx, raw_frame );

					if ( r != 0 )
					{
						if (( r != AVERROR( EAGAIN )) && ( r != AVERROR_EOF ))
						{
							char buff[256];
							av_strerror( r, buff, 256 );
							FeLog() << "Error decoding video (receiving frame): "
								<< buff << std::endl;
						}
						av_frame_free( &raw_frame );
					}
					else
					{
						raw_frame->pts = raw_frame->best_effort_timestamp;

						if ( raw_frame->pts == AV_NOPTS_VALUE )
							raw_frame->pts = packet ? packet->dts : prev_pts + prev_duration;

#if ( LIBAVUTIL_VERSION_MICRO >= 100 )
						// This only works on FFmpeg, exclude libav (it doesn't have pkt_duration
						// Correct for out of bounds pts
						if ( raw_frame->pts < prev_pts )
							raw_frame->pts = prev_pts + prev_duration;

						// Track pts and duration if we need to correct next frame
						prev_pts = raw_frame->pts;
#if HAVE_DURATION
						prev_duration = raw_frame->duration;
#else
						prev_duration = raw_frame->pkt_duration;
#endif
#endif

						detached_frame = raw_frame;

					}

					if ( packet )
						av_packet_free( &packet );
				}
			}
			else if ( !degrading )
			{
				if ( qscore < QMAX )
					qscore++;

				set_avdiscard_from_qscore( codec_ctx, qscore );

				//
				// full frame queue and nothing to display yet, so sleep
				//
				FeClock clock;
				while ( run_video_thread && clock.getElapsedTime() < max_sleep )
					fe_sleep( fe_milliseconds( 1 ) );
			}
		}
	}

the_end:
	//
	// shutdown the thread
	//
	at_end=true;
	video_timer.stop();

	{
		std::lock_guard<std::recursive_mutex> l( image_swap_mutex );
		if ( display_frame )
			display_frame=NULL;
	}

	if ( detached_frame )
		av_frame_free( &detached_frame );

	if ( sws_ctx )
		sws_freeContext( sws_ctx );

	int average = ( displayed == 0 ) ? qscore_accum : ( qscore_accum / displayed );

	FeDebug() << "End Video Thread - " << m_parent->FORMAT_CTX_URL << std::endl
				<< " - bit_rate=" << codec_ctx->bit_rate
				<< ", width=" << codec_ctx->width << ", height=" << codec_ctx->height << std::endl
				<< " - displayed=" << displayed << std::endl
				<< " - average qscore=" << average
				<< std::endl;
}

FeMedia::FeMedia( Type t, FeAudioEffectsManager &effects_manager )
	: m_audio( NULL ),
	m_video( NULL ),
	m_audio_stream(),
	m_audio_effects( effects_manager ),
	m_aspect_ratio( 1.0f ),
	m_volume( 100.0f ),
	m_pan( 0.0f ),
	m_audio_playing( false ),
	m_audio_pending_samples(),
	m_audio_pending_offset( 0 )
{
	m_imp = new FeMediaImp( t );

	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
	{
		auto* normaliser = m_audio_effects.get_effect<FeAudioNormaliser>();
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
	if ( !m_audio )
		return false;

	if ( !m_audio_stream.ensure_open() )
		return false;

	std::ignore = m_audio_stream.set_gain( m_volume / 100.0f );
	return true;
}

void FeMedia::clear_pending_audio()
{
	m_audio_pending_samples.clear();
	m_audio_pending_offset = 0;
}

void FeMedia::append_pending_audio( const std::vector<float> &samples )
{
	if ( samples.empty() )
		return;

	if ( m_audio_pending_offset >= m_audio_pending_samples.size() )
		clear_pending_audio();

	m_audio_pending_samples.insert( m_audio_pending_samples.end(), samples.begin(), samples.end() );
}

bool FeMedia::queue_pending_audio( std::vector<float> &processed_samples )
{
	const std::size_t channel_count = static_cast<std::size_t>( FeSdlAudioBackend::get().channel_count() );

	while (( m_audio_pending_offset < m_audio_pending_samples.size() )
		&& ( m_audio_stream.queued_frames() < FE_MEDIA_AUDIO_QUEUE_TARGET_FRAMES ))
	{
		const std::size_t remaining_frames = ( m_audio_pending_samples.size() - m_audio_pending_offset ) / channel_count;
		const int queue_headroom = FE_MEDIA_AUDIO_QUEUE_TARGET_FRAMES - m_audio_stream.queued_frames();
		const unsigned int frame_count = static_cast<unsigned int>( std::min<std::size_t>(
			FE_MEDIA_AUDIO_PROCESS_CHUNK_FRAMES,
			std::min<std::size_t>( remaining_frames, static_cast<std::size_t>( std::max( 1, queue_headroom ) ) ) ) );

		if ( frame_count == 0 )
			break;

		processed_samples.resize( static_cast<std::size_t>( frame_count ) * channel_count );
		m_audio_effects.process_all( m_audio_pending_samples.data() + m_audio_pending_offset, processed_samples.data(), frame_count, FeSdlAudioBackend::get().channel_count() );
		fe_audio_apply_balance( processed_samples, m_pan, false, Vec3f() );

		if ( !m_audio_stream.queue_frames( processed_samples.data(), static_cast<int>( frame_count ) ) )
			return false;

		m_audio_pending_offset += static_cast<std::size_t>( frame_count ) * channel_count;
	}

	if ( m_audio_pending_offset >= m_audio_pending_samples.size() )
		clear_pending_audio();

	return true;
}

FeTime FeMedia::get_video_time()
{
	if ( m_video )
		return m_video->at_end
			? get_duration()
			: m_video->video_timer.getElapsedTime();

	return FeTime();
}

void FeMedia::play()
{
	if ( !is_playing() )
	{
		if ( m_video )
			m_video->play();

		if ( m_audio )
		{
			m_audio_playing = true;
			std::ignore = ensure_audio_stream();
			std::ignore = pump_audio();
		}
	}
}

void FeMedia::signal_stop()
{
	m_audio_playing = false;
	m_audio_stream.clear();

	if ( m_video )
		m_video->signal_stop();
}

void FeMedia::stop()
{
	if ( m_audio )
	{
		m_audio_playing = false;
		m_audio_stream.clear();
		m_audio_stream.destroy();
		m_audio->stop();
		m_audio->at_end = false;
		clear_pending_audio();
		m_audio_effects.reset_all();

		{
			std::lock_guard<std::recursive_mutex> l( m_imp->m_read_mutex );
			if ( m_imp->m_format_ctx )
			{
				av_seek_frame( m_imp->m_format_ctx, m_audio->stream_id, 0, AVSEEK_FLAG_BACKWARD );
			}
		}

		avcodec_flush_buffers( m_audio->codec_ctx );
	}

	if ( m_video )
	{
		m_video->stop();

		{
			std::lock_guard<std::recursive_mutex> l( m_imp->m_read_mutex );
			if ( m_imp->m_format_ctx )
			{
				av_seek_frame( m_imp->m_format_ctx, m_video->stream_id, 0, AVSEEK_FLAG_BACKWARD );
			}
		}

		avcodec_flush_buffers( m_video->codec_ctx );
	}

	{
		std::lock_guard<std::recursive_mutex> l( m_imp->m_read_mutex );
		m_imp->m_read_eof = false;
	}
}

void FeMedia::close()
{
	stop();

	if ( m_audio )
	{
		delete m_audio;
		m_audio = NULL;
	}

	if ( m_video )
	{
		delete m_video;
		m_video = NULL;
	}

	m_audio_stream.destroy();
	m_imp->close();
}

bool FeMedia::is_playing()
{
	if (( m_video ) && ( m_video->far_behind ) )
		return false;

	if (( m_video ) && ( !m_video->at_end ) )
		return ( m_video->run_video_thread );

	return m_audio && m_audio_playing && (( m_audio_stream.queued_frames() > 0 ) || ( !m_audio->at_end && !end_of_file() ));
}

float FeMedia::getVolume() const
{
	return m_volume;
}

void FeMedia::setVolume( float volume )
{
	m_volume = volume;

	if ( m_audio )
	{
		AVDiscard d =( volume <= 0.f ) ? AVDISCARD_ALL : AVDISCARD_DEFAULT;

		m_audio->codec_ctx->skip_loop_filter = d;
		m_audio->codec_ctx->skip_idct = d;
		m_audio->codec_ctx->skip_frame = d;
	}

	if ( m_audio_stream.is_open() )
		std::ignore = m_audio_stream.set_gain( volume / 100.0f );

	auto* normaliser = m_audio_effects.get_effect<FeAudioNormaliser>();
	if ( normaliser )
		normaliser->set_media_volume( volume / 100.0f );
}

float FeMedia::getPan() const
{
	return m_pan;
}

void FeMedia::setPan( float pan )
{
	m_pan = std::clamp( pan, -1.0f, 1.0f );
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

	m_audio_effects.reset_all();

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
				m_video->time_base = fe_seconds( av_q2d( m_imp->m_format_ctx->streams[stream_id]->time_base ) );

				m_video->max_sleep = fe_seconds( 0.5 / av_q2d( m_imp->m_format_ctx->streams[stream_id]->r_frame_rate ) );

				if ( codec_ctx->sample_aspect_ratio.num != 0 )
					m_aspect_ratio = av_q2d( codec_ctx->sample_aspect_ratio );
				if ( m_imp->m_format_ctx->streams[stream_id]->sample_aspect_ratio.num != 0 )
					m_aspect_ratio = av_q2d( m_imp->m_format_ctx->streams[stream_id]->sample_aspect_ratio );

				m_video->rgba_width = codec_ctx->width;
				m_video->rgba_height = codec_ctx->height;
				m_video->display_rotation_quadrants =
					get_display_rotation_quadrants( m_imp->m_format_ctx->streams[stream_id] );
				if ( m_video->display_rotation_quadrants == 1 || m_video->display_rotation_quadrants == 3 )
				{
					m_video->disptex_width = codec_ctx->height;
					m_video->disptex_height = codec_ctx->width;
				}
				else
				{
					m_video->disptex_width = codec_ctx->width;
					m_video->disptex_height = codec_ctx->height;
				}

				m_video->init_rgba_buffer();
			}
		}
	}

	if (( !m_video ) && ( !m_audio ))
		return false;

	return true;
}

bool FeMedia::end_of_file()
{
	std::lock_guard<std::recursive_mutex> l( m_imp->m_read_mutex );

	bool retval = ( m_imp->m_read_eof );
	return retval;
}

bool FeMedia::read_packet()
{
	std::lock_guard<std::recursive_mutex> l( m_imp->m_read_mutex );

	if ( m_imp->m_read_eof )
		return false;

	if ( !m_imp->m_format_ctx )
	{
		m_imp->m_read_eof = true;
		FeLog() << "Error: Invalid format context: " << FORMAT_CTX_URL << std::endl;
		return false;
	}

	AVPacket *pkt = av_packet_alloc();
	if ( !pkt )
	{
		m_imp->m_read_eof = true;
		FeLog() << "Error: Failed to allocate packet: " << FORMAT_CTX_URL << std::endl;
		return false;
	}

	if ( !m_imp->m_format_ctx )
	{
		FeLog() << "Error: Format context was closed during read operation" << std::endl;
		av_packet_free( &pkt );
		m_imp->m_read_eof = true;
		return false;
	}

	int r = av_read_frame( m_imp->m_format_ctx, pkt );

	if ( r < 0 )
	{
		m_imp->m_read_eof=true;
		av_packet_free( &pkt );
		return false;
	}

	if (( m_audio ) && ( pkt->stream_index == m_audio->stream_id ))
		m_audio->push_packet( pkt );
	else if (( m_video ) && (pkt->stream_index == m_video->stream_id ))
		m_video->push_packet( pkt );
	else
		av_packet_free( &pkt );

	return true;
}

bool FeMedia::pump_audio()
{
	if ( !m_audio || !m_audio_playing )
		return false;

	if ( m_audio_stream.is_stale() )
		m_audio_stream.destroy();

	if ( !ensure_audio_stream() )
		return false;

	std::vector<float> processed_samples;

	while ( m_audio_stream.queued_frames() < FE_MEDIA_AUDIO_QUEUE_TARGET_FRAMES )
	{
		if ( !m_audio_pending_samples.empty() )
		{
			if ( !queue_pending_audio( processed_samples ) )
				return false;

			if ( m_audio_stream.queued_frames() >= FE_MEDIA_AUDIO_QUEUE_TARGET_FRAMES )
				break;
		}

		AVPacket *packet = m_audio->pop_packet();
		while (( packet == NULL ) && ( !end_of_file() ))
		{
			if ( !read_packet() )
				break;
			packet = m_audio->pop_packet();
		}

		if ( packet == NULL )
		{
			m_audio->at_end = true;
			break;
		}

		int send_result = avcodec_send_packet( m_audio->codec_ctx, packet );
		av_packet_free( &packet );
		if (( send_result < 0 ) && ( send_result != AVERROR( EAGAIN ) ))
		{
			char buff[256];
			av_strerror( send_result, buff, 256 );
			FeLog() << "Error decoding audio (sending packet): " << buff << std::endl;
			break;
		}

		AVFrame *frame = av_frame_alloc();
		if ( !frame )
			break;

		int receive_result = AVERROR( EAGAIN );
		do
		{
			receive_result = avcodec_receive_frame( m_audio->codec_ctx, frame );
			if ( receive_result == 0 )
			{
				std::vector<float> decoded_samples;
				if ( !m_audio->process_frame( frame, decoded_samples ) )
				{
					av_frame_free( &frame );
					return false;
				}

				if ( !decoded_samples.empty() )
				{
					append_pending_audio( decoded_samples );
					if ( !queue_pending_audio( processed_samples ) )
					{
						av_frame_free( &frame );
						return false;
					}
				}
			}
			else if ( receive_result != AVERROR( EAGAIN ) )
			{
				char buff[256];
				av_strerror( receive_result, buff, 256 );
				FeLog() << "Error decoding audio (receiving frame): " << buff << std::endl;
			}

			av_frame_unref( frame );
		}
		while ( receive_result != AVERROR( EAGAIN ) );

		av_frame_free( &frame );
	}

	if ( m_audio->at_end && m_audio_pending_samples.empty() && ( m_audio_stream.queued_frames() == 0 ) )
		m_audio_playing = false;

	return m_audio_stream.queued_frames() > 0;
}

bool FeMedia::tick()
{
	if (( !m_video ) && ( !m_audio ))
		return false;

	if ( m_audio )
		pump_audio();

	if ( m_video )
	{
		std::lock_guard<std::recursive_mutex> l( m_video->image_swap_mutex );
		if ( m_video->display_frame )
		{
			m_video->display_frame = NULL;
			m_video->frame_serial.fetch_add( 1, std::memory_order_release );
			m_video->frame_displayed.notify_one();
			return true;
		}
	}

	return false;
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
	if ( m_video && m_imp->m_format_ctx )
	{
		std::lock_guard<std::recursive_mutex> l( m_imp->m_read_mutex );
		if ( m_imp->m_format_ctx && m_video->stream_id >= 0 )
		{
			AVStream *s = m_imp->m_format_ctx->streams[ m_video->stream_id ];

#if ( LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 56, 13, 0 ))
		if (( s->nb_frames > 1 )
				|| ( s->id == AV_CODEC_ID_APNG )
				|| ( s->id == AV_CODEC_ID_GIF ))
			return true;
#else
			if ( s->nb_frames > 1 )
				return true;
#endif
		}
	}

	return false;
}

float FeMedia::get_aspect_ratio() const
{
	return m_aspect_ratio;
}

FeTime FeMedia::get_duration() const
{
	if ( m_video && m_imp->m_format_ctx )
	{
		std::lock_guard<std::recursive_mutex> l( m_imp->m_read_mutex );
		if ( m_imp->m_format_ctx && m_video->stream_id >= 0 )
		{
			return fe_seconds(
					av_q2d( m_imp->m_format_ctx->streams[m_video->stream_id]->time_base ) *
							m_imp->m_format_ctx->streams[ m_video->stream_id ]->duration );
		}
	}

	return FeTime();
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
	return m_audio_effects.get_effect<FeAudioVisualiser>();
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
