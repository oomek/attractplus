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

#include <fstream>
#include <unordered_map>
#include <cstring>
#include <cmath>

// Undefine Windows GetObject macro that conflict with Sqrat
#ifdef GetObject
#undef GetObject
#endif


FeMusic::FeMusic( bool loop )
	: m_file_name( "" ),
	m_volume( 100.0 ),
	m_vu( 0.0f ),
	m_vu_left( 0.0f ),
	m_vu_right( 0.0f ),
	m_vu_display( 0.0f ),
	m_vu_left_display( 0.0f ),
	m_vu_right_display( 0.0f ),
	m_fft_bands( FFT_BANDS, 0.0f ),
	m_fft_display_values( FFT_BANDS, 0.0f ),
	m_fft_bands_left( FFT_BANDS, 0.0f ),
	m_fft_display_values_left( FFT_BANDS, 0.0f ),
	m_fft_bands_right( FFT_BANDS, 0.0f ),
	m_fft_display_values_right( FFT_BANDS, 0.0f ),	m_fft_buffer( FFT_SIZE ),
	m_last_frame_time( 0.0f ),
	m_fft_linearity( 0.5f )
{
	m_music.setLooping( loop );

	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		m_music.setVolume( fep->get_fes()->get_play_volume( FeSoundInfo::Sound ));

	m_music.setEffectProcessor( [this]( const float* input_frames, unsigned int& input_frame_count,
	                                  float* output_frames, unsigned int& output_frame_count,
	                                  unsigned int frame_channel_count )
	{
		if ( input_frames && input_frame_count > 0 )
		{
			float peak = 0.0f;
			float peak_left = 0.0f;
			float peak_right = 0.0f;
			const unsigned int total_samples = input_frame_count * frame_channel_count;

			if ( frame_channel_count == 1 )
			{
				for ( unsigned int i = 0; i < total_samples; ++i )
				{
					float sample = std::abs( input_frames[i] );
					if ( sample > peak ) peak = sample;
				}
				peak_left = peak_right = peak;
			}
			else if ( frame_channel_count >= 2 )
			{
				for ( unsigned int i = 0; i < input_frame_count; ++i )
				{
					float left_sample = std::abs( input_frames[i * frame_channel_count] );
					if ( left_sample > peak_left ) peak_left = left_sample;

					float right_sample = std::abs( input_frames[i * frame_channel_count + 1] );
					if ( right_sample > peak_right ) peak_right = right_sample;

					float max_sample = std::max( left_sample, right_sample );
					if ( max_sample > peak ) peak = max_sample;
				}
			}

			m_vu = peak * 2.0f;
			m_vu_left = peak_left * 2.0f;
			m_vu_right = peak_right * 2.0f;

			if ( m_vu > 1.0f ) m_vu = 1.0f;
			if ( m_vu_left > 1.0f ) m_vu_left = 1.0f;
			if ( m_vu_right > 1.0f ) m_vu_right = 1.0f;
			if ( frame_channel_count >= 1 )
			{
				if ( frame_channel_count == 1 )
				{
					calculate_fft( input_frames, input_frame_count );
					m_fft_bands_left = m_fft_bands;
					m_fft_bands_right = m_fft_bands;
				}
				else
				{
					calculate_fft_stereo( input_frames, input_frame_count, frame_channel_count );

					std::vector<float> mono_samples( input_frame_count );
					for ( unsigned int i = 0; i < input_frame_count; ++i )
						mono_samples[i] = ( input_frames[i * frame_channel_count] + input_frames[i * frame_channel_count + 1] ) * 0.5f;

					calculate_fft( mono_samples.data(), input_frame_count );
				}
			}
		}
	else
		{
			m_vu = 0.0f;
			m_vu_left = 0.0f;
			m_vu_right = 0.0f;
			m_vu_display = 0.0f;
			m_vu_left_display = 0.0f;
			m_vu_right_display = 0.0f;

			std::fill( m_fft_bands.begin(), m_fft_bands.end(), 0.0f );
			std::fill( m_fft_bands_left.begin(), m_fft_bands_left.end(), 0.0f );
			std::fill( m_fft_bands_right.begin(), m_fft_bands_right.end(), 0.0f );
		}

		if ( input_frames && output_frames && input_frame_count > 0 )
		{
			const unsigned int total_samples = input_frame_count * frame_channel_count;
			std::memcpy( output_frames, input_frames, total_samples * sizeof(float) );
		}

		output_frame_count = input_frame_count;
	});
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
	return ( m_music.getStatus() == sf::SoundSource::Status::Playing ) ? true : false;
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

float FeMusic::get_vu()
{
	update_bands_fall();
	return m_vu_display;
}

float FeMusic::get_vu_left()
{
	update_bands_fall();
	return m_vu_left_display;
}

float FeMusic::get_vu_right()
{
	update_bands_fall();
	return m_vu_right_display;
}

const std::vector<float>& FeMusic::get_fft()
{
	update_bands_fall();
	return m_fft_display_values;
}

const std::vector<float>& FeMusic::get_fft_left()
{
	update_bands_fall();
	return m_fft_display_values_left;
}

const std::vector<float>& FeMusic::get_fft_right()
{
	update_bands_fall();
	return m_fft_display_values_right;
}

Sqrat::Array FeMusic::get_fft_array()
{
	update_bands_fall();
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();

	Sqrat::Table fft_table( vm );
	for ( size_t i = 0; i < m_fft_display_values.size(); ++i )
		fft_table.SetValue( static_cast<int>(i), m_fft_display_values[i] );

	return Sqrat::Array( fft_table.GetObject(), vm );
}

Sqrat::Array FeMusic::get_fft_left_array()
{
	update_bands_fall();
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();

	Sqrat::Table fft_table( vm );
	for ( size_t i = 0; i < m_fft_display_values_left.size(); ++i )
		fft_table.SetValue( static_cast<int>(i), m_fft_display_values_left[i] );

	return Sqrat::Array( fft_table.GetObject(), vm );
}

Sqrat::Array FeMusic::get_fft_right_array()
{
	update_bands_fall();
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();

	Sqrat::Table fft_table( vm );
	for ( size_t i = 0; i < m_fft_display_values_right.size(); ++i )
		fft_table.SetValue( static_cast<int>(i), m_fft_display_values_right[i] );

	return Sqrat::Array( fft_table.GetObject(), vm );
}

void FeMusic::calculate_fft( const float* samples, unsigned int sample_count ) const
{
	calculate_fft_channel( samples, sample_count, m_fft_bands );
}

void FeMusic::calculate_fft_stereo( const float* samples, unsigned int sample_count, unsigned int frame_channel_count ) const
{
	std::fill( m_fft_bands_left.begin(), m_fft_bands_left.end(), 0.0f );
	std::fill( m_fft_bands_right.begin(), m_fft_bands_right.end(), 0.0f );

	if ( sample_count < 256 || frame_channel_count < 2 ) return;

	std::vector<float> left_samples( sample_count );
	std::vector<float> right_samples( sample_count );

	for ( unsigned int i = 0; i < sample_count; ++i )
	{
		left_samples[i] = samples[i * frame_channel_count]; // Left channel
		right_samples[i] = samples[i * frame_channel_count + 1]; // Right channel
	}

	calculate_fft_channel( left_samples.data(), sample_count, m_fft_bands_left );
	calculate_fft_channel( right_samples.data(), sample_count, m_fft_bands_right );
}

void FeMusic::calculate_fft_channel( const float* samples, unsigned int sample_count, std::vector<float>& fft_bands ) const
{
	std::fill( fft_bands.begin(), fft_bands.end(), 0.0f );

	if ( sample_count < 256 ) return;

	const float sample_rate = static_cast<float>( m_music.getSampleRate() );

	if ( sample_rate <= 0.0f )
		return;

	const unsigned int analysis_size = std::min( 2048u, sample_count );

	const float nyquist_freq = sample_rate * 0.5f;
	const float freq_min = 40.0f;
	const float freq_max = std::min( 16000.0f, nyquist_freq );

	std::vector<float> windowed_samples( analysis_size );
	for ( unsigned int i = 0; i < analysis_size; ++i )
	{
		// Apply Hann window to reduce spectral leakage
		float window = 0.5f * ( 1.0f - std::cos( 2.0f * M_PI * i / ( analysis_size - 1 )));
		windowed_samples[i] = samples[i] * window;
	}

	for ( int band = 0; band < FFT_BANDS; ++band )
	{
		float freq_ratio = static_cast<float>( band ) / ( FFT_BANDS - 1 );

		// Blend between linear and logarithmic using m_fft_linearity
		float linear_freq = freq_min + freq_ratio * ( freq_max - freq_min );
		float log_freq = freq_min * std::pow( freq_max / freq_min, freq_ratio );
		float centre_freq = linear_freq * ( 1.0f - m_fft_linearity ) + log_freq * m_fft_linearity;

		float real_sum = 0.0f;
		float imag_sum = 0.0f;

		const unsigned int step = std::max( 1u, analysis_size / 512u );

		for ( unsigned int i = 0; i < analysis_size; i += step )
		{
			float angle = 2.0f * M_PI * centre_freq * i / sample_rate;
			float cos_val = std::cos( angle );
			float sin_val = std::sin( angle );

			real_sum += windowed_samples[i] * cos_val;
			imag_sum += windowed_samples[i] * sin_val;
		}

		float magnitude = std::sqrt( real_sum * real_sum + imag_sum * imag_sum );
		magnitude /= ( analysis_size / step );

		// Apply +6dB per octave amplitude fall-off
		float octaves = std::log2( centre_freq / freq_min );
		float amplitude_boost = std::pow( 10.0f, ( 6.0f * octaves ) / 20.0f );
		magnitude *= amplitude_boost;

		// Apply logarithmic scaling
		if ( magnitude > 0.0f )
		{
			// Convert to decibels and normalize
			float db_value = 20.0f * std::log10( magnitude + 1e-10f );

			// Map from -60dB - 0dB to 0-1 range
			float normalized = ( db_value + 60.0f ) / 60.0f;

			if ( normalized > 1.0f ) normalized = 1.0f;
			if ( normalized < 0.0f ) normalized = 0.0f;

			fft_bands[band] = normalized;
		}
		else
			fft_bands[band] = 0.0f;
	}
}

void FeMusic::update_bands_fall() const
{
	float current_time = m_system_clock.getElapsedTime().asSeconds();

	float delta_time = current_time - m_last_frame_time;
	m_last_frame_time = current_time;

	if ( delta_time > 0.1f ) delta_time = 0.1f;
	if ( delta_time < 0.0f ) delta_time = 0.0f;

	float refresh_rate = 60.0f;
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		refresh_rate = static_cast<float>( fep->get_refresh_rate() );

	float vu_fall_speed_per_second = 0.04f * refresh_rate;  // Faster fall for VU
	float fft_fall_speed_per_second = 0.02f * refresh_rate; // Slower fall for FFT
	float vu_fall_amount = vu_fall_speed_per_second * delta_time;
	float fft_fall_amount = fft_fall_speed_per_second * delta_time;

	// VU fall
	if ( m_vu > m_vu_display )
		m_vu_display = m_vu;
	else
	{
		m_vu_display -= vu_fall_amount;
		if ( m_vu_display < m_vu )
			m_vu_display = m_vu;
		if ( m_vu_display < 0.0f )
			m_vu_display = 0.0f;
	}

	if ( m_vu_left > m_vu_left_display )
		m_vu_left_display = m_vu_left;
	else
	{
		m_vu_left_display -= vu_fall_amount;
		if ( m_vu_left_display < m_vu_left )
			m_vu_left_display = m_vu_left;
		if ( m_vu_left_display < 0.0f )
			m_vu_left_display = 0.0f;
	}

	if ( m_vu_right > m_vu_right_display )
		m_vu_right_display = m_vu_right;
	else
	{
		m_vu_right_display -= vu_fall_amount;
		if ( m_vu_right_display < m_vu_right )
			m_vu_right_display = m_vu_right;
		if ( m_vu_right_display < 0.0f )
			m_vu_right_display = 0.0f;
	}

	// FFT fall
	for ( int i = 0; i < FFT_BANDS; ++i )
	{
		float current_fft_value = m_fft_bands[i];

		if ( current_fft_value > m_fft_display_values[i] )
			m_fft_display_values[i] = current_fft_value;
		else
		{
			m_fft_display_values[i] -= fft_fall_amount;

			if ( m_fft_display_values[i] < current_fft_value )
				m_fft_display_values[i] = current_fft_value;

			if ( m_fft_display_values[i] < 0.0f )
				m_fft_display_values[i] = 0.0f;
		}

		// Left channel
		float current_fft_left = m_fft_bands_left[i];
		if ( current_fft_left > m_fft_display_values_left[i] )
			m_fft_display_values_left[i] = current_fft_left;
		else
		{
			m_fft_display_values_left[i] -= fft_fall_amount;

			if ( m_fft_display_values_left[i] < current_fft_left )
				m_fft_display_values_left[i] = current_fft_left;

			if ( m_fft_display_values_left[i] < 0.0f )
				m_fft_display_values_left[i] = 0.0f;
		}

		// Right channel
		float current_fft_right = m_fft_bands_right[i];
		if ( current_fft_right > m_fft_display_values_right[i] )
			m_fft_display_values_right[i] = current_fft_right;
		else
		{
			m_fft_display_values_right[i] -= fft_fall_amount;

			if ( m_fft_display_values_right[i] < current_fft_right )
				m_fft_display_values_right[i] = current_fft_right;

			if ( m_fft_display_values_right[i] < 0.0f )
				m_fft_display_values_right[i] = 0.0f;
		}
	}
}

float FeMusic::get_fft_linearity()
{
	return m_fft_linearity;
}

void FeMusic::set_fft_linearity( float linearity )
{
	if ( linearity == m_fft_linearity )
		return;

	if ( linearity < 0.0f ) linearity = 0.0f;
	else if ( linearity > 1.0f ) linearity = 1.0f;

	m_fft_linearity = linearity;
}
