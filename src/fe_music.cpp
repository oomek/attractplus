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


FeMusic::FeMusic( bool loop )
	: m_file_name( "" ),
	m_volume( 100.0 ),
	m_pan( 0.0 ),
	m_fft_data_zero( FeAudioVisualiser::FFT_BANDS_MAX, 0.0f ),
	m_fft_zero_wrapper( &m_fft_data_zero ),
	m_fft_array_wrapper( &m_fft_data_zero )
{
	m_music.setLooping( loop );
	m_music.setSpatializationEnabled( false );

	m_audio_effects.add_effect( std::make_unique<FeAudioDCFilter>() );
	m_audio_effects.add_effect( std::make_unique<FeAudioNormaliser>() );
	m_audio_effects.add_effect( std::make_unique<FeAudioVisualiser>() );

	// Mark effects manager as ready for processing after all effects are constructed
	m_audio_effects.set_ready_for_processing();
	m_music.setEffectProcessor( [this]( const float *input_frames, unsigned int &input_frame_count,
	                                    float *output_frames, unsigned int &output_frame_count,
	                                    unsigned int frame_channel_count )
	{
		if ( input_frames && input_frame_count > 0 )
		{
			m_audio_effects.process_all( input_frames, output_frames, input_frame_count, frame_channel_count );
		}
		else
		{
			m_audio_effects.reset_all();

			if ( input_frames && output_frames && input_frame_count > 0 )
			{
				const unsigned int total_samples = input_frame_count * frame_channel_count;
				std::memcpy( output_frames, input_frames, total_samples * sizeof(float) );
			}
		}

		output_frame_count = input_frame_count;
	});
}

FeMusic::~FeMusic()
{
	m_music.setEffectProcessor( [this]( const float *input_frames, unsigned int &input_frame_count,
	                                    float *output_frames, unsigned int &output_frame_count,
	                                    unsigned int frame_channel_count )
	{
		if ( input_frames && output_frames && input_frame_count > 0 )
		{
			const unsigned int total_samples = input_frame_count * frame_channel_count;
			std::memcpy( output_frames, input_frames, total_samples * sizeof(float) );
		}
		output_frame_count = input_frame_count;
	});
	m_music.stop();
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
	return ( m_music.getStatus() == sf::SoundSource::Status::Playing ) ? true : false;
}

void FeMusic::set_playing( bool state )
{
	m_music.stop();

	if ( state == true && m_file_name != "" )
	{
		float vol = m_volume;
		FePresent *fep = FePresent::script_get_fep();
		if ( fep )
			vol = vol * fep->get_fes()->get_play_volume( FeSoundInfo::Sound ) / 100.0;

		m_music.setVolume( vol );
		m_music.setPan( m_pan );

		auto* normaliser = m_audio_effects.get_effect<FeAudioNormaliser>();
		if ( normaliser )
			normaliser->set_media_volume( vol / 100.0f );

		m_music.play();
	}
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
	return m_music.isLooping();
}

void FeMusic::set_loop( bool loop )
{
	m_music.setLooping( loop );
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
	m_music.setSpatializationEnabled( true );
}

void FeMusic::set_y( float v )
{
	m_music.setPosition( sf::Vector3f( get_x(), v, get_z() ) );
	m_music.setSpatializationEnabled( true );
}

void FeMusic::set_z( float v )
{
	m_music.setPosition( sf::Vector3f( get_x(), get_y(), v ) );
	m_music.setSpatializationEnabled( true );
}

int FeMusic::get_duration()
{
	return m_music.getDuration().asMilliseconds();
}

int FeMusic::get_time()
{
	return m_music.getPlayingOffset().asMilliseconds();
}

const char *FeMusic::get_metadata( const char* tag )
{
	if ( m_file_name.empty() || !tag )
		return "";

	std::ifstream file( m_file_name, std::ios::binary );

	if ( !file.is_open() )
		return "";

	// Read the last 128 bytes for ID3v1 metadata
	file.seekg( -128, std::ios::end );
	char id3v1[128];
	file.read( id3v1, 128 );
	file.close();

	// Check for ID3v1 tag
	if ( std::strncmp( id3v1, "TAG", 3 ) != 0 )
		return "";

	// Map ID3v1 tags to their respective positions
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
	float vol = m_volume;

	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		vol = vol * fep->get_fes()->get_play_volume( FeSoundInfo::Sound ) / 100.0;

	if ( vol != m_music.getVolume() )
	{
		m_music.setVolume( vol );

		auto* normaliser = m_audio_effects.get_effect<FeAudioNormaliser>();
		if ( normaliser )
			normaliser->set_media_volume( vol / 100.0f );
	}

	if ( m_pan != m_music.getPan() )
		m_music.setPan( m_pan );

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
