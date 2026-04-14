/*
 *
 *  Attract-Mode Plus frontend
 *  Copyright (C) 2026 Radek Dutkiewicz
 *
 *  This file is part of Attract-Mode Plus
 *
 *  Attract-Mode Plus is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Attract-Mode Plus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Attract-Mode Plus.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "fe_audio_sdl.hpp"

#include "fe_base.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#if ( LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT( 59, 0, 100 ))
typedef const AVCodec FeAVCodec;
#else
typedef AVCodec FeAVCodec;
#endif

#define HAVE_CH_LAYOUT ( LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( 57, 28, 100 ))

namespace
{
	constexpr int FE_AUDIO_MIX_RATE = 48000;
	constexpr int FE_AUDIO_MIX_CHANNELS = 2;

	std::string fe_ffmpeg_error_string( int error_code )
	{
		char buffer[256] = {};
		av_strerror( error_code, buffer, sizeof( buffer ) );
		return std::string( buffer );
	}

	int fe_codec_channel_count( const AVCodecContext *codec_ctx )
	{
#if HAVE_CH_LAYOUT
		return codec_ctx->ch_layout.nb_channels;
#else
		return codec_ctx->channels;
#endif
	}

	bool fe_setup_resampler( SwrContext *&resample_ctx, AVCodecContext *codec_ctx )
	{
		if ( resample_ctx )
			return true;

		resample_ctx = swr_alloc();
		if ( !resample_ctx )
		{
			FeLog() << "Error allocating SDL audio resampler." << std::endl;
			return false;
		}

#if HAVE_CH_LAYOUT
		AVChannelLayout in_layout = AV_CHANNEL_LAYOUT_MASK( 0, 0 );
		av_channel_layout_copy( &in_layout, &codec_ctx->ch_layout );
		if ( !av_channel_layout_check( &in_layout ) )
			av_channel_layout_default( &in_layout, std::max( 1, codec_ctx->ch_layout.nb_channels ) );

		AVChannelLayout out_layout = AV_CHANNEL_LAYOUT_STEREO;
		av_opt_set_chlayout( resample_ctx, "in_chlayout", &in_layout, 0 );
		av_opt_set_chlayout( resample_ctx, "out_chlayout", &out_layout, 0 );
		av_channel_layout_uninit( &in_layout );
#else
		const int64_t in_layout = codec_ctx->channel_layout
			? codec_ctx->channel_layout
			: av_get_default_channel_layout( std::max( 1, codec_ctx->channels ) );
		av_opt_set_int( resample_ctx, "in_channel_layout", in_layout, 0 );
		av_opt_set_int( resample_ctx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0 );
#endif

		av_opt_set_int( resample_ctx, "in_sample_fmt", codec_ctx->sample_fmt, 0 );
		av_opt_set_int( resample_ctx, "in_sample_rate", codec_ctx->sample_rate, 0 );
		av_opt_set_int( resample_ctx, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0 );
		av_opt_set_int( resample_ctx, "out_sample_rate", FE_AUDIO_MIX_RATE, 0 );

		const int init_result = swr_init( resample_ctx );
		if ( init_result < 0 )
		{
			FeLog() << "Error initializing SDL audio resampler: "
				<< fe_ffmpeg_error_string( init_result ) << std::endl;
			swr_free( &resample_ctx );
			return false;
		}

		return true;
	}

	bool fe_append_resampled_audio( SwrContext *resample_ctx, AVCodecContext *codec_ctx,
		AVFrame *frame, std::vector<float> &out_samples, std::size_t max_frames )
	{
		const int in_sample_rate = frame->sample_rate > 0 ? frame->sample_rate : codec_ctx->sample_rate;
		const int delayed_samples = swr_get_delay( resample_ctx, in_sample_rate );
		const int max_out_samples = av_rescale_rnd( delayed_samples + frame->nb_samples, FE_AUDIO_MIX_RATE, in_sample_rate, AV_ROUND_UP );
		if ( max_out_samples <= 0 )
			return true;

		const std::size_t old_size = out_samples.size();
		out_samples.resize( old_size + static_cast<std::size_t>( max_out_samples ) * FE_AUDIO_MIX_CHANNELS );

		uint8_t *output_data[1] =
		{
			reinterpret_cast<uint8_t *>( out_samples.data() + old_size )
		};

		const int converted_samples = swr_convert(
			resample_ctx,
			output_data,
			max_out_samples,
			const_cast<const uint8_t **>( frame->extended_data ),
			frame->nb_samples );

		if ( converted_samples < 0 )
		{
			FeLog() << "Error performing SDL audio conversion: "
				<< fe_ffmpeg_error_string( converted_samples ) << std::endl;
			out_samples.resize( old_size );
			return false;
		}

		out_samples.resize( old_size + static_cast<std::size_t>( converted_samples ) * FE_AUDIO_MIX_CHANNELS );
		const std::size_t max_output_size = max_frames * FE_AUDIO_MIX_CHANNELS;
		if ( out_samples.size() > max_output_size )
			out_samples.resize( max_output_size );

		return true;
	}
}

struct FeAudioFileDecoder::Impl
{
	AVFormatContext *format_ctx = nullptr;
	AVCodecContext *codec_ctx = nullptr;
	SwrContext *resample_ctx = nullptr;
	int stream_index = -1;
	bool sent_eof = false;
	bool decoder_eof = false;
	std::vector<float> pending_samples;
	std::size_t pending_offset = 0;
};

int FeAudioSampleBuffer::duration_ms() const
{
	return fe_audio_frames_to_ms( frame_count() );
}

FeSdlAudioBackend::FeSdlAudioBackend()
	: m_mix_spec(),
	m_generation( 1 )
{
	m_mix_spec.format = SDL_AUDIO_F32;
	m_mix_spec.channels = FE_AUDIO_MIX_CHANNELS;
	m_mix_spec.freq = FE_AUDIO_MIX_RATE;
}

FeSdlAudioBackend &FeSdlAudioBackend::get()
{
	static FeSdlAudioBackend backend;
	return backend;
}

bool FeSdlAudioBackend::ensure_audio()
{
	if ( SDL_WasInit( SDL_INIT_AUDIO ) & SDL_INIT_AUDIO )
		return true;

	if ( !SDL_InitSubSystem( SDL_INIT_AUDIO ) )
	{
		FeLog() << "ERROR: Failed to initialize SDL audio: " << SDL_GetError() << std::endl;
		return false;
	}

	return true;
}

void FeSdlAudioBackend::note_system_resume()
{
	ensure_audio();
	m_generation.fetch_add( 1, std::memory_order_release );
}

FeSdlAudioStream::FeSdlAudioStream()
	: m_stream( nullptr ),
	m_generation( 0 )
{
}

FeSdlAudioStream::~FeSdlAudioStream()
{
	destroy();
}

bool FeSdlAudioStream::ensure_open()
{
	if ( m_stream && !is_stale() )
		return true;

	destroy();

	FeSdlAudioBackend &backend = FeSdlAudioBackend::get();
	if ( !backend.ensure_audio() )
		return false;

	m_stream = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &backend.mix_spec(), nullptr, nullptr );
	if ( !m_stream )
	{
		FeLog() << "ERROR: Failed to open SDL audio stream: " << SDL_GetError() << std::endl;
		return false;
	}

	if ( !SDL_ResumeAudioStreamDevice( m_stream ) )
	{
		FeLog() << "ERROR: Failed to resume SDL audio stream device: " << SDL_GetError() << std::endl;
		SDL_DestroyAudioStream( m_stream );
		m_stream = nullptr;
		return false;
	}

	m_generation = backend.generation();
	return true;
}

void FeSdlAudioStream::destroy()
{
	if ( m_stream )
	{
		SDL_DestroyAudioStream( m_stream );
		m_stream = nullptr;
	}
	m_generation = 0;
}

void FeSdlAudioStream::clear()
{
	if ( m_stream )
		std::ignore = SDL_ClearAudioStream( m_stream );
}

bool FeSdlAudioStream::is_stale() const
{
	return m_stream && ( m_generation != FeSdlAudioBackend::get().generation() );
}

bool FeSdlAudioStream::queue_frames( const float *frames, int frame_count )
{
	if ( !ensure_open() || !frames || frame_count <= 0 )
		return false;

	const int len = frame_count * FeSdlAudioBackend::get().channel_count() * static_cast<int>( sizeof( float ) );
	return SDL_PutAudioStreamData( m_stream, frames, len );
}

int FeSdlAudioStream::queued_frames() const
{
	if ( !m_stream )
		return 0;

	const int queued_bytes = SDL_GetAudioStreamQueued( m_stream );
	if ( queued_bytes <= 0 )
		return 0;

	const int bytes_per_frame = FeSdlAudioBackend::get().channel_count() * static_cast<int>( sizeof( float ) );
	return queued_bytes / bytes_per_frame;
}

bool FeSdlAudioStream::set_gain( float gain )
{
	return m_stream ? SDL_SetAudioStreamGain( m_stream, gain ) : false;
}

bool FeSdlAudioStream::set_pitch( float pitch )
{
	return m_stream ? SDL_SetAudioStreamFrequencyRatio( m_stream, pitch ) : false;
}

FeAudioFileDecoder::FeAudioFileDecoder()
	: m_impl( std::make_unique<Impl>() ),
	m_duration_ms( 0 )
{
}

FeAudioFileDecoder::~FeAudioFileDecoder()
{
	close();
}

bool FeAudioFileDecoder::open( const std::string &filename )
{
	close();

	if ( avformat_open_input( &m_impl->format_ctx, filename.c_str(), nullptr, nullptr ) < 0 )
		return false;

	if ( avformat_find_stream_info( m_impl->format_ctx, nullptr ) < 0 )
	{
		close();
		return false;
	}

	FeAVCodec *decoder = nullptr;
	m_impl->stream_index = av_find_best_stream( m_impl->format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0 );
	if ( m_impl->stream_index < 0 || !decoder )
	{
		close();
		return false;
	}

	m_impl->codec_ctx = avcodec_alloc_context3( nullptr );
	if ( !m_impl->codec_ctx )
	{
		close();
		return false;
	}

	if ( avcodec_parameters_to_context( m_impl->codec_ctx, m_impl->format_ctx->streams[m_impl->stream_index]->codecpar ) < 0 )
	{
		close();
		return false;
	}

	if ( avcodec_open2( m_impl->codec_ctx, decoder, nullptr ) < 0 )
	{
		close();
		return false;
	}

	if ( !fe_setup_resampler( m_impl->resample_ctx, m_impl->codec_ctx ) )
	{
		close();
		return false;
	}

	m_impl->sent_eof = false;
	m_impl->decoder_eof = false;
	m_impl->pending_samples.clear();
	m_impl->pending_offset = 0;
	m_duration_ms = 0;

	AVStream *stream = m_impl->format_ctx->streams[m_impl->stream_index];
	if ( stream && stream->duration > 0 )
		m_duration_ms = static_cast<int>( av_rescale_q( stream->duration, stream->time_base, AVRational{ 1, 1000 } ) );
	else if ( m_impl->format_ctx->duration > 0 )
		m_duration_ms = static_cast<int>( m_impl->format_ctx->duration / ( AV_TIME_BASE / 1000 ) );

	return true;
}

void FeAudioFileDecoder::close()
{
	if ( !m_impl )
		return;

	if ( m_impl->resample_ctx )
		swr_free( &m_impl->resample_ctx );

	if ( m_impl->codec_ctx )
		avcodec_free_context( &m_impl->codec_ctx );

	if ( m_impl->format_ctx )
		avformat_close_input( &m_impl->format_ctx );

	m_impl->stream_index = -1;
	m_impl->sent_eof = false;
	m_impl->decoder_eof = false;
	m_impl->pending_samples.clear();
	m_impl->pending_offset = 0;
	m_duration_ms = 0;
}

bool FeAudioFileDecoder::seek_ms( int milliseconds )
{
	if ( !m_impl || !m_impl->format_ctx || m_impl->stream_index < 0 )
		return false;

	const int64_t timestamp = av_rescale_q( std::max( 0, milliseconds ), AVRational{ 1, 1000 }, m_impl->format_ctx->streams[m_impl->stream_index]->time_base );
	const int seek_result = av_seek_frame( m_impl->format_ctx, m_impl->stream_index, timestamp, AVSEEK_FLAG_BACKWARD );
	if ( seek_result < 0 )
		return false;

	avcodec_flush_buffers( m_impl->codec_ctx );
	m_impl->sent_eof = false;
	m_impl->decoder_eof = false;
	m_impl->pending_samples.clear();
	m_impl->pending_offset = 0;

	if ( m_impl->resample_ctx )
	{
		swr_free( &m_impl->resample_ctx );
		if ( !fe_setup_resampler( m_impl->resample_ctx, m_impl->codec_ctx ) )
			return false;
	}

	return true;
}

bool FeAudioFileDecoder::decode_frames( std::vector<float> &out_samples, std::size_t max_frames, bool &reached_end )
{
	out_samples.clear();
	reached_end = false;

	if ( !m_impl || !m_impl->codec_ctx || max_frames == 0 )
		return false;

	const std::size_t max_output_samples = max_frames * FE_AUDIO_MIX_CHANNELS;

	auto consume_pending_samples = [&]()
	{
		if ( m_impl->pending_offset >= m_impl->pending_samples.size() )
		{
			m_impl->pending_samples.clear();
			m_impl->pending_offset = 0;
			return;
		}

		const std::size_t remaining = m_impl->pending_samples.size() - m_impl->pending_offset;
		const std::size_t count = std::min( remaining, max_output_samples - out_samples.size() );
		out_samples.insert( out_samples.end(),
			m_impl->pending_samples.begin() + static_cast<std::ptrdiff_t>( m_impl->pending_offset ),
			m_impl->pending_samples.begin() + static_cast<std::ptrdiff_t>( m_impl->pending_offset + count ) );
		m_impl->pending_offset += count;

		if ( m_impl->pending_offset >= m_impl->pending_samples.size() )
		{
			m_impl->pending_samples.clear();
			m_impl->pending_offset = 0;
		}
	};

	consume_pending_samples();

	while ( out_samples.size() < max_output_samples )
	{
		AVFrame *frame = av_frame_alloc();
		if ( !frame )
			break;

		const int receive_result = avcodec_receive_frame( m_impl->codec_ctx, frame );
		if ( receive_result == 0 )
		{
			std::vector<float> decoded_frame_samples;
			const bool append_ok = fe_append_resampled_audio( m_impl->resample_ctx, m_impl->codec_ctx, frame, decoded_frame_samples, std::numeric_limits<std::size_t>::max() / FE_AUDIO_MIX_CHANNELS );
			av_frame_free( &frame );
			if ( !append_ok )
				return false;

			if ( !decoded_frame_samples.empty() )
			{
				if ( m_impl->pending_offset >= m_impl->pending_samples.size() )
				{
					m_impl->pending_samples = std::move( decoded_frame_samples );
					m_impl->pending_offset = 0;
				}
				else
				{
					m_impl->pending_samples.insert( m_impl->pending_samples.end(), decoded_frame_samples.begin(), decoded_frame_samples.end() );
				}
				consume_pending_samples();
			}
			continue;
		}

		av_frame_free( &frame );

		if ( receive_result == AVERROR_EOF )
		{
			m_impl->decoder_eof = true;
			reached_end = true;
			break;
		}

		if ( receive_result != AVERROR( EAGAIN ) )
		{
			FeLog() << "Error decoding audio frame: " << fe_ffmpeg_error_string( receive_result ) << std::endl;
			m_impl->decoder_eof = true;
			reached_end = true;
			break;
		}

		AVPacket *packet = av_packet_alloc();
		if ( !packet )
			break;

		const int read_result = av_read_frame( m_impl->format_ctx, packet );
		if ( read_result < 0 )
		{
			av_packet_free( &packet );
			if ( !m_impl->sent_eof )
			{
				avcodec_send_packet( m_impl->codec_ctx, nullptr );
				m_impl->sent_eof = true;
				continue;
			}

			m_impl->decoder_eof = true;
			reached_end = true;
			break;
		}

		if ( packet->stream_index != m_impl->stream_index )
		{
			av_packet_free( &packet );
			continue;
		}

		const int send_result = avcodec_send_packet( m_impl->codec_ctx, packet );
		av_packet_free( &packet );
		if ( send_result < 0 && send_result != AVERROR( EAGAIN ) )
		{
			FeLog() << "Error sending audio packet: " << fe_ffmpeg_error_string( send_result ) << std::endl;
			m_impl->decoder_eof = true;
			reached_end = true;
			break;
		}
	}

	if ( out_samples.empty() && m_impl->decoder_eof && m_impl->pending_samples.empty() )
		reached_end = true;

	return !out_samples.empty();
}

bool fe_decode_audio_file_to_buffer( const std::string &filename, FeAudioSampleBuffer &buffer )
{
	buffer.samples.clear();
	buffer.sample_rate = FE_AUDIO_MIX_RATE;
	buffer.channel_count = FE_AUDIO_MIX_CHANNELS;

	FeAudioFileDecoder decoder;
	if ( !decoder.open( filename ) )
		return false;

	std::vector<float> chunk;
	bool reached_end = false;
	while ( decoder.decode_frames( chunk, 8192, reached_end ) )
		buffer.samples.insert( buffer.samples.end(), chunk.begin(), chunk.end() );

	return !buffer.samples.empty();
}

void fe_audio_apply_balance( std::vector<float> &samples, float pan, bool spatialization_enabled, const Vec3f &position )
{
	if ( samples.size() < 2 )
		return;

	float effective_pan = std::clamp( pan, -1.0f, 1.0f );
	float gain = 1.0f;

	if ( spatialization_enabled )
	{
		const float distance = std::sqrt( position.x * position.x + position.y * position.y + position.z * position.z );
		const float spatial_pan = std::clamp( position.x / std::max( 1.0f, std::fabs( position.x ) + std::fabs( position.z ) + 1.0f ), -1.0f, 1.0f );
		effective_pan = std::clamp( effective_pan + spatial_pan, -1.0f, 1.0f );
		gain = 1.0f / std::max( 1.0f, distance );
	}

	if ( !spatialization_enabled && std::fabs( effective_pan ) < 0.0001f )
		return;

	const float left_gain = gain * ( effective_pan <= 0.0f ? 1.0f : 1.0f - effective_pan );
	const float right_gain = gain * ( effective_pan >= 0.0f ? 1.0f : 1.0f + effective_pan );

	for ( std::size_t i = 0; i + 1 < samples.size(); i += 2 )
	{
		samples[i] *= left_gain;
		samples[i + 1] *= right_gain;
	}
}

int fe_audio_frames_to_ms( std::uint64_t frame_count )
{
	return static_cast<int>( ( frame_count * 1000ull ) / FE_AUDIO_MIX_RATE );
}

std::uint64_t fe_audio_ms_to_frames( int milliseconds )
{
	if ( milliseconds <= 0 )
		return 0;

	return ( static_cast<std::uint64_t>( milliseconds ) * FE_AUDIO_MIX_RATE ) / 1000ull;
}