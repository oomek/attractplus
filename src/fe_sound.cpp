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
#include <iostream>
#include <cstring>

FeSoundSystem::FeSoundSystem( FeSettings *fes )
	: m_event_sound( false ),
	m_ambient_sound( true ),
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
}

void FeSoundSystem::update_volumes()
{
	m_ambient_sound.set_volume( m_fes->get_play_volume( FeSoundInfo::Ambient ) );
	m_event_sound.set_volume( m_fes->get_play_volume( FeSoundInfo::Sound ) );
}

void FeSoundSystem::release_audio( bool state )
{
	m_event_sound.release_audio( state );
}

FeSound::FeSound( bool loop )
	: m_buffer(),
	m_sounds(),
	m_voices( 3 ),
	m_file_name( "" ),
	m_play_state( false ),
	m_volume( 100.0 ),
	m_pitch( 1.0 ),
	m_loop( loop ),
	m_position( 0.0, 0.0, 0.0 )
{
	m_sounds.emplace_back( sf::Sound( m_buffer ));
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
	std::string filename = clean_path( n );

	if ( filename.empty() )
	{
		m_file_name = "";
		return;
	}

	if ( is_relative_path( filename ) )
		filename = FePresent::script_get_base_path() + filename;

	load( filename );
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

		FePresent *fep = FePresent::script_get_fep();
		if ( fep )
			v = v * fep->get_fes()->get_play_volume( FeSoundInfo::Sound ) / 100.0;

		for ( auto itr=m_sounds.begin(); itr != m_sounds.end(); ++itr )
			itr->setVolume( v );
	}
}

void FeSound::set_playing( bool flag )
{
	m_play_state = flag;

	if ( m_play_state == true )
	{
		if ( m_sounds.size() >= m_voices )
		{
			m_sounds.front().stop();
			m_sounds.pop_front();
		}

		m_sounds.emplace_back( sf::Sound( m_buffer ));
		FePresent *fep = FePresent::script_get_fep();
		float vol = m_volume;
		if ( fep )
			vol = vol * fep->get_fes()->get_play_volume( FeSoundInfo::Sound ) / 100.0;
		m_sounds.back().setVolume( vol );
		m_sounds.back().setLoop( m_loop );
		m_sounds.back().setPosition( m_position );
		m_sounds.back().setPitch( m_pitch );
		m_sounds.back().play();
	}
	else
	{
		for ( auto itr=m_sounds.begin(); itr != m_sounds.end(); ++itr )
			itr->stop();
	}
}

bool FeSound::get_playing()
{
	return ( m_sounds.back().getStatus() == sf::SoundSource::Playing ) ? true : false;
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

		for ( auto itr=m_sounds.begin(); itr != m_sounds.end(); ++itr )
			itr->setPitch( p );
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

		for ( auto itr=m_sounds.begin(); itr != m_sounds.end(); ++itr )
			itr->setLoop( l );
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

		for ( auto itr=m_sounds.begin(); itr != m_sounds.end(); ++itr )
			itr->setPosition( m_position );
	}
}

void FeSound::set_y( float p )
{
	if ( p != m_position.y )
	{
		m_position.y = p;

		for ( auto itr=m_sounds.begin(); itr != m_sounds.end(); ++itr )
			itr->setPosition( m_position );
	}
}

void FeSound::set_z( float p )
{
	if ( p != m_position.z )
	{
		m_position.z = p;

		for ( auto itr=m_sounds.begin(); itr != m_sounds.end(); ++itr )
			itr->setPosition( m_position );
	}
}

int FeSound::get_duration()
{
	return m_buffer.getDuration().asMilliseconds();
}

int FeSound::get_time()
{
	return m_sounds.back().getPlayingOffset().asMilliseconds();
}

int FeSound::get_voices()
{
	return m_voices;
}

void FeSound::set_voices( int v )
{
	m_voices = v;
}