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

#include "fe_sound.hpp"
#include "fe_settings.hpp"
#include "fe_present.hpp"
#include "fe_util.hpp"
#include "fe_file.hpp"
#include "zip.hpp"

#include <algorithm>
#include <iostream>
#include <cstring>

namespace
{
	constexpr int FE_SOUND_QUEUE_TARGET_FRAMES = 960;
	constexpr int FE_SOUND_QUEUE_CHUNK_FRAMES = 480;
}

FeSoundSystem::FeSoundSystem( FeSettings *fes )
	: m_event_sound( false ),
	m_ambient_sound( true, FeSoundInfo::Ambient ),
	m_fes( fes ),
	m_current_sound( FeInputMap::LAST_COMMAND )
{
}

FeSoundSystem::~FeSoundSystem()
{
}

FeMusic &FeSoundSystem::get_ambient_sound()
{
	return m_ambient_sound;
}

void FeSoundSystem::sound_event( FeInputMap::Command c )
{
	if ( m_fes->get_play_volume( FeSoundInfo::Sound ) <= 0 )
		return;

	std::string sound;
	if ( !m_fes->get_sound_file( c, sound ) )
		return;

	if ( sound.compare( m_event_sound.get_file_name() ) != 0 )
		m_event_sound.load( sound );

	m_current_sound = c;
	m_event_sound.set_playing( true );
}

bool FeSoundSystem::is_sound_event_playing( FeInputMap::Command c )
{
	return (( m_current_sound == c ) && m_event_sound.get_playing() );
}

void FeSoundSystem::play_ambient()
{
	if ( m_fes->get_play_volume( FeSoundInfo::Ambient ) <= 0 )
		return;

	std::string sound;
	if ( !m_fes->get_sound_file( FeInputMap::AmbientSound, sound ) )
		return;

	if ( sound.compare( m_ambient_sound.get_file_name() ) != 0 )
		m_ambient_sound.load( sound );

	m_ambient_sound.set_playing( true );
}

void FeSoundSystem::stop()
{
	m_ambient_sound.set_playing( false );
}

void FeSoundSystem::tick()
{
	m_event_sound.tick();
	m_ambient_sound.tick();
}

void FeSoundSystem::release_audio( bool state )
{
	m_event_sound.release_audio( state );
	m_ambient_sound.release_audio( state );
}

FeSound::FeSound( bool loop )
	: m_buffer(),
	m_stream(),
	m_file_name( "" ),
	m_rewind( true ),
	m_volume( 100.0f ),
	m_pan( 0.0f ),
	m_pitch( 1.0f ),
	m_loop( loop ),
	m_position( 0.0f, 0.0f, 0.0f ),
	m_spatialization_enabled( false ),
	m_status( FePlaybackStatusStopped ),
	m_current_frame( 0 ),
	m_source_frame( 0 ),
	m_total_frames_written( 0 ),
	m_applied_pan( 0.0f ),
	m_applied_position( 0.0f, 0.0f, 0.0f ),
	m_applied_spatialization_enabled( false )
{
}

FeSound::~FeSound()
{
	release_audio( true );
}

bool FeSound::ensure_stream()
{
	if ( !m_stream.ensure_open() )
		return false;

	float vol = m_volume;
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		vol = vol * fep->get_fes()->get_play_volume( FeSoundInfo::Sound ) / 100.0f;

	std::ignore = m_stream.set_gain( vol / 100.0f );
	std::ignore = m_stream.set_pitch( m_pitch );
	return true;
}

void FeSound::sync_playback_position()
{
	const std::uint64_t total_frames = m_buffer.frame_count();
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

void FeSound::restart_stream()
{
	if ( m_status != FePlaybackStatusPlaying )
		return;

	sync_playback_position();
	m_stream.clear();
	m_source_frame = m_current_frame;
	m_total_frames_written = m_current_frame;
	pump_audio();
}

void FeSound::pump_audio()
{
	if ( m_status != FePlaybackStatusPlaying || m_buffer.empty() )
		return;

	if ( !ensure_stream() )
		return;

	const std::uint64_t total_frames = m_buffer.frame_count();
	std::vector<float> chunk;

	while ( m_stream.queued_frames() < FE_SOUND_QUEUE_TARGET_FRAMES )
	{
		if ( !m_loop && ( m_source_frame >= total_frames ) )
			break;

		const std::uint64_t start_frame = m_loop ? ( m_source_frame % total_frames ) : m_source_frame;
		const std::uint64_t remaining_frames = total_frames - start_frame;
		const std::uint64_t chunk_frames = std::min<std::uint64_t>( FE_SOUND_QUEUE_CHUNK_FRAMES, remaining_frames );
		if ( chunk_frames == 0 )
		{
			if ( m_loop )
			{
				m_source_frame = 0;
				continue;
			}
			break;
		}

		const std::size_t first_sample = static_cast<std::size_t>( start_frame ) * m_buffer.channel_count;
		const std::size_t sample_count = static_cast<std::size_t>( chunk_frames ) * m_buffer.channel_count;
		chunk.assign( m_buffer.samples.begin() + first_sample, m_buffer.samples.begin() + first_sample + sample_count );
		fe_audio_apply_balance( chunk, m_pan, m_spatialization_enabled, m_position );

		if ( !m_stream.queue_frames( chunk.data(), static_cast<int>( chunk_frames ) ) )
		{
			m_status = FePlaybackStatusStopped;
			return;
		}

		m_source_frame += chunk_frames;
		m_total_frames_written += chunk_frames;
		m_applied_pan = m_pan;
		m_applied_position = m_position;
		m_applied_spatialization_enabled = m_spatialization_enabled;

		if ( m_loop && ( m_source_frame >= total_frames ) )
			m_source_frame %= total_frames;
	}

	sync_playback_position();
}

void FeSound::release_audio( bool state )
{
	if ( state )
	{
		sync_playback_position();
		m_stream.destroy();
	}
}

void FeSound::tick()
{
	if ( m_stream.is_stale() )
	{
		sync_playback_position();
		m_stream.destroy();
		m_source_frame = m_current_frame;
		m_total_frames_written = m_current_frame;
	}

	float vol = m_volume;

	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		vol = vol * fep->get_fes()->get_play_volume( FeSoundInfo::Sound ) / 100.0f;

	if ( m_stream.is_open() )
	{
		std::ignore = m_stream.set_gain( vol / 100.0f );
		std::ignore = m_stream.set_pitch( m_pitch );
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
}

void FeSound::load( const std::string &fn )
{
	if ( !fe_decode_audio_file_to_buffer( fn, m_buffer ) )
	{
		FeLog() << "Error loading sound file: " << fn << std::endl;
		m_file_name = "";
		m_status = FePlaybackStatusStopped;
		m_current_frame = 0;
		m_source_frame = 0;
		m_total_frames_written = 0;
		m_stream.clear();
		return;
	}

	m_file_name = fn;
	m_status = FePlaybackStatusStopped;
	m_current_frame = 0;
	m_source_frame = 0;
	m_total_frames_written = 0;
	m_stream.clear();
}

void FeSound::set_file_name( const char *n )
{
	FeLog() << "sound.file_name is deprecated. Go to Layouts.md for more info." << std::endl;
}

const char *FeSound::get_file_name()
{
	return m_file_name.c_str();
}

float FeSound::get_volume()
{
	return m_volume;
}

void FeSound::set_volume( float v )
{
	if ( v != m_volume )
	{
		if ( v > 100.0f ) v = 100.0f;
		else if ( v < 0.0f ) v = 0.0f;

		m_volume = v;
	}
}

float FeSound::get_pan()
{
	return m_pan;
}

void FeSound::set_pan( float p )
{
	if ( p == m_pan ) return;
	m_pan = std::clamp( p, -1.0f, 1.0f );
}

void FeSound::set_playing( bool flag )
{
	if ( !flag )
	{
		if ( m_rewind )
		{
			m_stream.clear();
			m_current_frame = 0;
			m_source_frame = 0;
			m_total_frames_written = 0;
			m_status = FePlaybackStatusStopped;
		}
		else
		{
			sync_playback_position();
			m_stream.clear();
			m_source_frame = m_current_frame;
			m_total_frames_written = m_current_frame;
			m_status = FePlaybackStatusPaused;
		}

		return;
	}

	if ( m_file_name.empty() || m_buffer.empty() )
	{
		m_status = FePlaybackStatusStopped;
		return;
	}

	const bool ended = ( get_status() == FePlaybackStatusEnded );
	if (( m_status == FePlaybackStatusPlaying ) || ended || ( m_current_frame >= m_buffer.frame_count() ))
		m_current_frame = 0;

	if ( !ensure_stream() )
	{
		m_status = FePlaybackStatusStopped;
		return;
	}

	m_stream.clear();
	m_source_frame = m_current_frame;
	m_total_frames_written = m_current_frame;
	m_status = FePlaybackStatusPlaying;
	pump_audio();
}

bool FeSound::get_playing()
{
	return get_status() == FePlaybackStatusPlaying;
}

bool FeSound::get_rewind()
{
	return m_rewind;
}

void FeSound::set_rewind( bool rewind )
{
	m_rewind = rewind;
}

FePlaybackStatus FeSound::get_status()
{
	sync_playback_position();
	return m_status;
}

std::string FeSound::get_status_msg()
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

float FeSound::get_pitch()
{
	return m_pitch;
}

void FeSound::set_pitch( float p )
{
	if ( p != m_pitch )
		m_pitch = p;
}

bool FeSound::get_loop()
{
	return m_loop;
}

void FeSound::set_loop( bool l )
{
	m_loop = l;
}

float FeSound::get_x()
{
	return m_position.x;
}

float FeSound::get_y()
{
	return m_position.y;
}

float FeSound::get_z()
{
	return m_position.z;
}

void FeSound::set_x( float p )
{
	if ( p != m_position.x )
	{
		m_position.x = p;
		m_spatialization_enabled = true;
	}
}

void FeSound::set_y( float p )
{
	if ( p != m_position.y )
	{
		m_position.y = p;
		m_spatialization_enabled = true;
	}
}

void FeSound::set_z( float p )
{
	if ( p != m_position.z )
	{
		m_position.z = p;
		m_spatialization_enabled = true;
	}
}

int FeSound::get_duration()
{
	return m_buffer.duration_ms();
}

int FeSound::get_time()
{
	sync_playback_position();
	return fe_audio_frames_to_ms( m_current_frame );
}
