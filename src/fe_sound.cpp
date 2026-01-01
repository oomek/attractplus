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
	m_ambient_sound.tick();
}

void FeSoundSystem::release_audio( bool state )
{
	m_event_sound.release_audio( state );
}

FeSound::FeSound( bool loop )
	: m_buffer(),
	m_sound( m_buffer ),
	m_file_name( "" ),
	m_play_state( false ),
	m_volume( 100.0 ),
	m_pan( 0.0 ),
	m_pitch( 1.0 ),
	m_loop( loop ),
	m_position( 0.0, 0.0, 0.0 )
{
	m_sound.setSpatializationEnabled( false );
}

FeSound::~FeSound()
{
}

void FeSound::release_audio( bool state )
{
	// fix our state if sound is being stopped...
	if ( state )
		set_playing( false );
}

void FeSound::tick()
{
	float vol = m_volume;

	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		vol = vol * fep->get_fes()->get_play_volume( FeSoundInfo::Sound ) / 100.0;

	if ( vol != m_sound.getVolume() )
		m_sound.setVolume( vol );

	if ( m_pan != m_sound.getPan() )
		m_sound.setPan( m_pan );
}

void FeSound::load( const std::string &fn )
{
	if ( !m_buffer.loadFromFile( fn ))
	{
		FeLog() << "Error loading sound file: " << fn << std::endl;
		m_file_name = "";
		return;
	}
	m_file_name = fn;
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
		if ( v > 100.0 ) v = 100.0;
		else if ( v < 0.0 ) v = 0.0;

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
	m_play_state = flag;

	if ( m_play_state == true && m_file_name != "" )
	{
		m_sound.stop();
		m_sound.setLooping( m_loop );
		m_sound.setPosition( m_position );
		m_sound.setPitch( m_pitch );

		float vol = m_volume;
		FePresent *fep = FePresent::script_get_fep();
		if ( fep )
			vol = vol * fep->get_fes()->get_play_volume( FeSoundInfo::Sound ) / 100.0;

		m_sound.setVolume( vol );
		m_sound.setPan( m_pan );
		m_sound.play();
	}
	else
	{
		m_sound.stop();
	}
}

bool FeSound::get_playing()
{
	return ( m_sound.getStatus() == sf::SoundSource::Status::Playing ) ? true : false;
}

float FeSound::get_pitch()
{
	return m_pitch;
}

void FeSound::set_pitch( float p )
{
	if ( p != m_pitch )
	{
		m_pitch = p;

		m_sound.setPitch( p );
	}
}

bool FeSound::get_loop()
{
	return m_loop;
}

void FeSound::set_loop( bool l )
{
	if ( l != m_loop )
	{
		m_loop = l;

		m_sound.setLooping( l );
	}
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

		m_sound.setPosition( m_position );
		m_sound.setSpatializationEnabled( true );
	}
}

void FeSound::set_y( float p )
{
	if ( p != m_position.y )
	{
		m_position.y = p;

		m_sound.setPosition( m_position );
		m_sound.setSpatializationEnabled( true );
	}
}

void FeSound::set_z( float p )
{
	if ( p != m_position.z )
	{
		m_position.z = p;

		m_sound.setPosition( m_position );
		m_sound.setSpatializationEnabled( true );
	}
}

int FeSound::get_duration()
{
	return m_buffer.getDuration().asMilliseconds();
}

int FeSound::get_time()
{
	return m_sound.getPlayingOffset().asMilliseconds();
}
