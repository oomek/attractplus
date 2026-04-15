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

#define _USE_MATH_DEFINES

#include "fe_music.hpp"
#include "fe_settings.hpp"
#include "fe_present.hpp"
#include "fe_util.hpp"
#include "fe_file.hpp"

#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <cmath>
#include <memory>

// Undefine Windows GetObject macro that conflict with Sqrat
#ifdef GetObject
#undef GetObject
#endif

namespace
{
	constexpr int FE_MUSIC_QUEUE_TARGET_FRAMES = 1920;
	constexpr int FE_MUSIC_DECODE_CHUNK_FRAMES = 4096;
	constexpr int FE_MUSIC_PROCESS_CHUNK_FRAMES = 480;
}

FeMusic::FeMusic( bool loop, FeSoundInfo::SoundType st )
	: m_stream(),
	m_decoder( std::make_unique<FeAudioFileDecoder>() ),
	m_file_name( "" ),
	m_rewind( true ),
	m_loop( loop ),
	m_volume( 100.0f ),
	m_pan( 0.0f ),
	m_pitch( 1.0f ),
	m_position( 0.0f, 0.0f, 0.0f ),
	m_spatialization_enabled( false ),
	m_sound_type( st ),
	m_audio_effects(),
	m_status( FePlaybackStatusStopped ),
	m_current_frame( 0 ),
	m_total_frames_written( 0 ),
	m_duration_ms( 0 ),
	m_pending_samples(),
	m_pending_offset( 0 ),
	m_applied_pan( 0.0f ),
	m_applied_position( 0.0f, 0.0f, 0.0f ),
	m_applied_spatialization_enabled( false ),
	m_fft_data_zero( FeAudioVisualiser::FFT_BANDS_MAX, 0.0f ),
	m_fft_zero_wrapper( &m_fft_data_zero ),
	m_fft_array_wrapper( &m_fft_data_zero )
{
	m_audio_effects.add_effect( std::make_unique<FeAudioDCFilter>() );
	m_audio_effects.add_effect( std::make_unique<FeAudioNormaliser>() );
	m_audio_effects.add_effect( std::make_unique<FeAudioVisualiser>() );

	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
	{
		auto* normaliser = m_audio_effects.get_effect<FeAudioNormaliser>();
		if ( normaliser )
			normaliser->set_enabled( fep->get_fes()->get_loudness() );
	}

	m_audio_effects.set_ready_for_processing();
}

FeMusic::~FeMusic()
{
	release_audio( true );
}

bool FeMusic::ensure_stream()
{
	if ( !m_stream.ensure_open() )
		return false;

	float vol = m_volume;
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		vol = vol * fep->get_fes()->get_play_volume( m_sound_type ) / 100.0f;

	std::ignore = m_stream.set_gain( vol / 100.0f );
	std::ignore = m_stream.set_pitch( m_pitch );
	return true;
}

void FeMusic::clear_pending_audio()
{
	m_pending_samples.clear();
	m_pending_offset = 0;
}

void FeMusic::append_pending_audio( const std::vector<float> &samples )
{
	if ( samples.empty() )
		return;

	if ( m_pending_offset >= m_pending_samples.size() )
		clear_pending_audio();

	m_pending_samples.insert( m_pending_samples.end(), samples.begin(), samples.end() );
}

bool FeMusic::queue_pending_audio( std::vector<float> &processed_samples )
{
	const std::size_t channel_count = static_cast<std::size_t>( FeSdlAudioBackend::get().channel_count() );

	while (( m_pending_offset < m_pending_samples.size() )
		&& ( m_stream.queued_frames() < FE_MUSIC_QUEUE_TARGET_FRAMES ))
	{
		const std::size_t remaining_frames = ( m_pending_samples.size() - m_pending_offset ) / channel_count;
		const int queue_headroom = FE_MUSIC_QUEUE_TARGET_FRAMES - m_stream.queued_frames();
		const unsigned int frame_count = static_cast<unsigned int>( std::min<std::size_t>(
			FE_MUSIC_PROCESS_CHUNK_FRAMES,
			std::min<std::size_t>( remaining_frames, static_cast<std::size_t>( std::max( 1, queue_headroom ) ) ) ) );

		if ( frame_count == 0 )
			break;

		processed_samples.resize( static_cast<std::size_t>( frame_count ) * channel_count );
		m_audio_effects.process_all( m_pending_samples.data() + m_pending_offset, processed_samples.data(), frame_count, FeSdlAudioBackend::get().channel_count() );
		fe_audio_apply_balance( processed_samples, m_pan, m_spatialization_enabled, m_position );

		if ( !m_stream.queue_frames( processed_samples.data(), static_cast<int>( frame_count ) ) )
			return false;

		m_pending_offset += static_cast<std::size_t>( frame_count ) * channel_count;
		m_total_frames_written += frame_count;
		m_applied_pan = m_pan;
		m_applied_position = m_position;
		m_applied_spatialization_enabled = m_spatialization_enabled;
	}

	if ( m_pending_offset >= m_pending_samples.size() )
		clear_pending_audio();

	return true;
}

void FeMusic::sync_playback_position()
{
	const std::uint64_t total_frames = fe_audio_ms_to_frames( m_duration_ms );
	if ( total_frames == 0 )
	{
		m_current_frame = 0;
		return;
	}

	const int queued_frames = m_stream.queued_frames();
	const std::uint64_t played_frames = ( m_total_frames_written >= static_cast<std::uint64_t>( queued_frames ) )
		? ( m_total_frames_written - static_cast<std::uint64_t>( queued_frames ) )
		: 0;

	if ( m_loop )
		m_current_frame = played_frames % total_frames;
	else
		m_current_frame = std::min<std::uint64_t>( played_frames, total_frames );

	if ( !m_loop && ( m_status == FePlaybackStatusPlaying ) && ( m_total_frames_written >= total_frames ) && ( queued_frames == 0 ) )
		m_status = FePlaybackStatusEnded;
}

void FeMusic::restart_stream()
{
	if ( m_status != FePlaybackStatusPlaying )
		return;

	sync_playback_position();
	if ( !m_decoder || !m_decoder->seek_ms( fe_audio_frames_to_ms( m_current_frame ) ) )
	{
		m_status = FePlaybackStatusStopped;
		return;
	}

	m_stream.clear();
	m_total_frames_written = m_current_frame;
	pump_audio();
}

void FeMusic::pump_audio()
{
	if (( m_status != FePlaybackStatusPlaying ) || !m_decoder )
		return;

	if ( !ensure_stream() )
		return;

	std::vector<float> decoded;
	std::vector<float> processed;
	while ( m_stream.queued_frames() < FE_MUSIC_QUEUE_TARGET_FRAMES )
	{
		if ( !m_pending_samples.empty() )
		{
			if ( !queue_pending_audio( processed ) )
			{
				m_status = FePlaybackStatusStopped;
				return;
			}

			if ( m_stream.queued_frames() >= FE_MUSIC_QUEUE_TARGET_FRAMES )
				break;
		}

		bool reached_end = false;
		if ( !m_decoder->decode_frames( decoded, FE_MUSIC_DECODE_CHUNK_FRAMES, reached_end ) )
		{
			if ( reached_end )
			{
				if ( m_loop )
				{
					m_audio_effects.reset_all();
					clear_pending_audio();
					if ( !m_decoder->seek_ms( 0 ) )
					{
						m_status = FePlaybackStatusStopped;
						return;
					}
					continue;
				}

				if ( m_stream.queued_frames() == 0 )
					m_status = FePlaybackStatusEnded;
			}
			break;
		}

		if ( !decoded.empty() )
		{
			append_pending_audio( decoded );
			if ( !queue_pending_audio( processed ) )
			{
				m_status = FePlaybackStatusStopped;
				return;
			}
		}
	}

	sync_playback_position();
}

void FeMusic::release_audio( bool state )
{
	if ( state )
	{
		sync_playback_position();
		m_stream.destroy();
	}
}

void FeMusic::load( const std::string &fn )
{
	if ( !m_decoder )
		m_decoder = std::make_unique<FeAudioFileDecoder>();

	if ( !m_decoder->open( fn ) )
	{
		FeLog() << "Error loading audio file: " << fn << std::endl;
		m_file_name = "";
		m_status = FePlaybackStatusStopped;
		m_duration_ms = 0;
		m_current_frame = 0;
		m_total_frames_written = 0;
		clear_pending_audio();
		m_stream.clear();
		return;
	}

	m_file_name = fn;
	m_status = FePlaybackStatusStopped;
	m_duration_ms = m_decoder->duration_ms();
	m_current_frame = 0;
	m_total_frames_written = 0;
	clear_pending_audio();
	m_stream.clear();
	m_audio_effects.reset_all();
}

void FeMusic::set_file_name( const char *n )
{
	std::string filename = clean_path( n );

	if ( filename.empty() )
	{
		m_file_name = "";
		m_status = FePlaybackStatusStopped;
		m_stream.clear();
		return;
	}

	if ( is_relative_path( filename ) )
		filename = FePresent::script_get_base_path() + filename;

	load( filename );
}

const char *FeMusic::get_file_name()
{
	return m_file_name.c_str();
}

float FeMusic::get_volume()
{
	return m_volume;
}

void FeMusic::set_volume( float v )
{
	if ( v != m_volume )
	{
		if ( v > 100.0f ) v = 100.0f;
		else if ( v < 0.0f ) v = 0.0f;

		m_volume = v;
	}
}

float FeMusic::get_pan()
{
	return m_pan;
}

void FeMusic::set_pan( float p )
{
	if ( p == m_pan ) return;
	m_pan = std::clamp( p, -1.0f, 1.0f );
}

bool FeMusic::get_playing()
{
	return get_status() == FePlaybackStatusPlaying;
}

void FeMusic::set_playing( bool state )
{
	if ( !state )
	{
		if ( m_rewind )
		{
			m_stream.clear();
			m_current_frame = 0;
			m_total_frames_written = 0;
			m_status = FePlaybackStatusStopped;
		}
		else
		{
			sync_playback_position();
			m_stream.clear();
			m_total_frames_written = m_current_frame;
			m_pending_samples.clear();
			m_pending_offset = 0;
			m_status = FePlaybackStatusPaused;
		}

		return;
	}

	if ( m_file_name.empty() || !m_decoder )
	{
		m_status = FePlaybackStatusStopped;
		return;
	}

	const std::uint64_t duration_frames = fe_audio_ms_to_frames( m_duration_ms );
	if (( m_status == FePlaybackStatusEnded ) || ( m_current_frame >= duration_frames ))
		m_current_frame = 0;

	if ( m_status == FePlaybackStatusPlaying )
		return;

	if ( !ensure_stream() )
	{
		m_status = FePlaybackStatusStopped;
		return;
	}

	if ( !m_decoder->seek_ms( fe_audio_frames_to_ms( m_current_frame ) ) )
	{
		m_status = FePlaybackStatusStopped;
		return;
	}

	m_stream.clear();
	m_total_frames_written = m_current_frame;
	clear_pending_audio();
	m_status = FePlaybackStatusPlaying;
	pump_audio();
}

bool FeMusic::get_rewind()
{
	return m_rewind;
}

void FeMusic::set_rewind( bool rewind )
{
	m_rewind = rewind;
}

FePlaybackStatus FeMusic::get_status()
{
	sync_playback_position();
	return m_status;
}

std::string FeMusic::get_status_msg()
{
	switch ( get_status() )
	{
	case FePlaybackStatusPlaying:
		return _( "Playing" );
	case FePlaybackStatusPaused:
		return _( "Paused" );
	case FePlaybackStatusEnded:
		return _( "Ended" );
	default:
		return _( "Stopped" );
	}
}

float FeMusic::get_pitch()
{
	return m_pitch;
}

void FeMusic::set_pitch( float p )
{
	m_pitch = p;
}

bool FeMusic::get_loop()
{
	return m_loop;
}

void FeMusic::set_loop( bool loop )
{
	m_loop = loop;
}

float FeMusic::get_x()
{
	return m_position.x;
}

float FeMusic::get_y()
{
	return m_position.y;
}

float FeMusic::get_z()
{
	return m_position.z;
}

void FeMusic::set_x( float v )
{
	m_position.x = v;
	m_spatialization_enabled = true;
}

void FeMusic::set_y( float v )
{
	m_position.y = v;
	m_spatialization_enabled = true;
}

void FeMusic::set_z( float v )
{
	m_position.z = v;
	m_spatialization_enabled = true;
}

int FeMusic::get_duration()
{
	return m_duration_ms;
}

int FeMusic::get_time()
{
	sync_playback_position();
	return fe_audio_frames_to_ms( m_current_frame );
}

void FeMusic::set_time( int time )
{
	const int clamped_time = std::clamp( time, 0, get_duration() );
	m_current_frame = fe_audio_ms_to_frames( clamped_time );
	m_total_frames_written = m_current_frame;
	clear_pending_audio();

	if ( m_decoder )
		std::ignore = m_decoder->seek_ms( clamped_time );

	if ( m_status == FePlaybackStatusPlaying )
		restart_stream();
	else if (( get_duration() > 0 ) && ( clamped_time >= get_duration() ))
		m_status = FePlaybackStatusEnded;
	else if ( m_status != FePlaybackStatusPaused )
		m_status = FePlaybackStatusStopped;
}

const char *FeMusic::get_metadata( const char* tag )
{
	if ( m_file_name.empty() || !tag )
		return "";

	std::ifstream file( m_file_name, std::ios::binary );

	if ( !file.is_open() )
		return "";

	file.seekg( -128, std::ios::end );
	char id3v1[128];
	file.read( id3v1, 128 );
	file.close();

	if ( std::strncmp( id3v1, "TAG", 3 ) != 0 )
		return "";

	std::unordered_map< std::string, std::string > metadata =
	{
		{ "title", std::string( id3v1 + 3, 30 )},
		{ "artist", std::string( id3v1 + 33, 30 )},
		{ "album", std::string( id3v1 + 63, 30 )},
		{ "year", std::string( id3v1 + 93, 4 )},
		{ "comment", std::string( id3v1 + 97, 30 )}
	};

	std::unordered_map< std::string, std::string >::iterator it = metadata.find( tag );

	return it != metadata.end() ? it->second.c_str() : "";
}

void FeMusic::tick()
{
	if ( m_stream.is_stale() )
	{
		sync_playback_position();
		m_stream.destroy();
		if ( m_decoder )
			std::ignore = m_decoder->seek_ms( fe_audio_frames_to_ms( m_current_frame ) );
		m_total_frames_written = m_current_frame;
		clear_pending_audio();
	}

	float vol = m_volume;

	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		vol = vol * fep->get_fes()->get_play_volume( m_sound_type ) / 100.0f;

	if ( m_stream.is_open() )
	{
		std::ignore = m_stream.set_gain( vol / 100.0f );
		std::ignore = m_stream.set_pitch( m_pitch );

		auto* normaliser = m_audio_effects.get_effect<FeAudioNormaliser>();
		if ( normaliser )
			normaliser->set_media_volume( vol / 100.0f );
	}

	if (( m_status == FePlaybackStatusPlaying )
		&& (( m_pan != m_applied_pan )
			|| ( m_spatialization_enabled != m_applied_spatialization_enabled )
			|| ( m_position.x != m_applied_position.x )
			|| ( m_position.y != m_applied_position.y )
			|| ( m_position.z != m_applied_position.z )))
	{
		restart_stream();
	}
	else
		pump_audio();

	m_audio_effects.update_all();
}

FeAudioVisualiser* FeMusic::get_audio_visualiser() const
{
	return m_audio_effects.get_effect<FeAudioVisualiser>();
}

float FeMusic::get_vu_mono()
{
	return get_audio_visualiser()->get_vu_mono();
}

float FeMusic::get_vu_left()
{
	return get_audio_visualiser()->get_vu_left();
}

float FeMusic::get_vu_right()
{
	return get_audio_visualiser()->get_vu_right();
}

const SqratArrayWrapper& FeMusic::get_fft_array_mono() const
{
	if ( get_audio_visualiser() )
	{
		if ( get_audio_visualiser()->get_fft_mono_ptr() )
			{
				m_fft_array_wrapper.set_data( get_audio_visualiser()->get_fft_mono_ptr() );
				return m_fft_array_wrapper;
			}
	}

	return m_fft_zero_wrapper;
}

const SqratArrayWrapper& FeMusic::get_fft_array_left() const
{
	if ( get_audio_visualiser() )
	{
		if ( get_audio_visualiser()->get_fft_left_ptr() )
			{
				m_fft_array_wrapper.set_data( get_audio_visualiser()->get_fft_left_ptr() );
				return m_fft_array_wrapper;
			}
	}

	return m_fft_zero_wrapper;
}

const SqratArrayWrapper& FeMusic::get_fft_array_right() const
{
	if ( get_audio_visualiser() )
	{
		if ( get_audio_visualiser()->get_fft_right_ptr() )
			{
				m_fft_array_wrapper.set_data( get_audio_visualiser()->get_fft_right_ptr() );
				return m_fft_array_wrapper;
			}
	}

	return m_fft_zero_wrapper;
}

void FeMusic::set_fft_bands( int count )
{
	get_audio_visualiser()->set_fft_bands( count );
}

int FeMusic::get_fft_bands() const
{
	return get_audio_visualiser()->get_fft_bands();
}
