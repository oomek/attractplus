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

#include "fe_music.hpp"
#include "fe_settings.hpp"
#include "fe_present.hpp"
#include "fe_util.hpp"
#include "fe_file.hpp"

FeMusic::FeMusic( bool loop )
	: m_file_name( "" ),
	m_volume( 100.0 )
{
	m_music.setLoop( loop );
	m_music.setProcessingInterval(sf::milliseconds(1));

	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		m_music.setVolume( fep->get_fes()->get_play_volume( FeSoundInfo::Sound ));
}

FeMusic::~FeMusic()
{
}

void FeMusic::load( const std::string &fn )
{
	if ( !m_music.openFromFile( fn ))
	{
		FeLog() << "Error loading audio file: " << fn << std::endl;
        m_file_name = "";
		return;
	}
    m_file_name = fn;
}

void FeMusic::set_file_name( const char *n )
{
	std::string filename = clean_path( n );

	if ( filename.empty() )
	{
		m_file_name = "";
		m_music.stop();
		return;
	}

	if ( is_relative_path( filename ))
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
		if ( v > 100.0 ) v = 100.0;
		else if ( v < 0.0 ) v = 0.0;

		m_volume = v;

		FePresent *fep = FePresent::script_get_fep();
		if ( fep )
			v = v * fep->get_fes()->get_play_volume( FeSoundInfo::Sound ) / 100.0;

		m_music.setVolume( v );
	}
}

bool FeMusic::get_playing()
{
	return ( m_music.getStatus() == sf::SoundSource::Playing ) ? true : false;
}

void FeMusic::set_playing( bool state )
{
	m_music.stop();

	if ( state == true && m_file_name != "" )
		m_music.play();
}

float FeMusic::get_pitch()
{
	return m_music.getPitch();
}

void FeMusic::set_pitch( float p )
{
	m_music.setPitch( p );
}

bool FeMusic::get_loop()
{
	return m_music.getLoop();
}

void FeMusic::set_loop( bool loop )
{
	m_music.setLoop( loop );
}

float FeMusic::get_x()
{
	return m_music.getPosition().x;
}

float FeMusic::get_y()
{
	return m_music.getPosition().y;
}

float FeMusic::get_z()
{
	return m_music.getPosition().z;
}

void FeMusic::set_x( float v )
{
	m_music.setPosition( sf::Vector3f( v, get_y(), get_z() ) );
}

void FeMusic::set_y( float v )
{
	m_music.setPosition( sf::Vector3f( get_x(), v, get_z() ) );
}

void FeMusic::set_z( float v )
{
	m_music.setPosition( sf::Vector3f( get_x(), get_y(), v ) );
}

int FeMusic::get_duration()
{
	return m_music.getDuration().asMilliseconds();
}

int FeMusic::get_time()
{
	return m_music.getPlayingOffset().asMilliseconds();
}