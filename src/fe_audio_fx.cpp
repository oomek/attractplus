/*
 *
 *  Attract-Mode Plus frontend
 *  Copyright (C) 2025 Andrew Mickelson & Radek Dutkiewicz
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

#define _USE_MATH_DEFINES

#include "fe_audio_fx.hpp"
#include "fe_settings.hpp"
#include "fe_present.hpp"

#include <SFML/Audio/PlaybackDevice.hpp>

// Undefine Windows GetObject macro that conflicts with Sqrat
#ifdef GetObject
#undef GetObject
#endif

FeAudioVisualiser::FeAudioVisualiser()
	: m_vu_mono_in( 0.0f ),
	m_vu_mono_out( 0.0f ),
	m_vu_left_in( 0.0f ),
	m_vu_left_out( 0.0f ),
	m_vu_right_in( 0.0f ),
	m_vu_right_out( 0.0f ),
	m_fft_mono_in( FFT_BANDS, 0.0f ),
	m_fft_mono_out( FFT_BANDS, 0.0f ),
	m_fft_left_in( FFT_BANDS, 0.0f ),
	m_fft_left_out( FFT_BANDS, 0.0f ),
	m_fft_right_in( FFT_BANDS, 0.0f ),
	m_fft_right_out( FFT_BANDS, 0.0f ),
	m_last_frame_time( sf::Time::Zero ),
	m_reset_done( false ),
	m_vu_requested( false ),
	m_fft_requested( false ),
	m_rolling_buffer_mono( ROLLING_BUFFER_SIZE, 0.0f ),
	m_rolling_buffer_left( ROLLING_BUFFER_SIZE, 0.0f ),
	m_rolling_buffer_right( ROLLING_BUFFER_SIZE, 0.0f ),
	m_window_lut( FFT_BUFFER_SIZE ),
	m_buffer_write_pos( 0 ),
	m_buffer_samples_count( 0 )
{
	// Pre-calculate Blackman window for FFT
	const float window_scale = 2.0f / FFT_BUFFER_SIZE;
	for ( unsigned int i = 0; i < FFT_BUFFER_SIZE; ++i )
	{
		float a0 = 0.42f;
		float a1 = 0.5f;
		float a2 = 0.08f;
		float window = a0 - a1 * std::cos( 2.0f * M_PI * i / ( FFT_BUFFER_SIZE - 1 ))
		                  + a2 * std::cos( 4.0f * M_PI * i / ( FFT_BUFFER_SIZE - 1 ));
		m_window_lut[i] = window * window_scale;
	}
}

FeAudioVisualiser::~FeAudioVisualiser()
{
}

float FeAudioVisualiser::convert_to_log_scale( float linear_value ) const
{
	if ( linear_value <= 1e-8f )
		return 0.0f;

	// SFML 3.0.0 is outputting 50% of the amplitude? Multiply by 2.0
	linear_value *= 2.0f;

	float linear_scaled = std::min( 1.0f, linear_value );
	float db_value = 20.0f * std::log2( linear_value + 1e-10f ) * 0.30103f;
	float log_scaled = std::max( 0.0f, std::min( 1.0f, ( db_value + DB_SCALE ) / DB_SCALE ));

	// Blend between linear and logarithmic scaling
	return linear_scaled * ( 1.0f - AMPLITUDE_LINEARITY ) + log_scaled * AMPLITUDE_LINEARITY;
}

void FeAudioVisualiser::process( const float* input_frames, unsigned int input_frame_count,
                                               unsigned int frame_channel_count, float sample_rate )
{
	bool buffer_ready = resample_and_buffer_audio( input_frames, input_frame_count, frame_channel_count, sample_rate );

	if ( !buffer_ready )
		return;

	if ( !m_vu_requested && !m_fft_requested )
		return;

	// Process VU meters only if requested
	if ( m_vu_requested )
	{
		float peak_mono = 0.0f;
		float peak_left = 0.0f;
		float peak_right = 0.0f;

		if ( frame_channel_count == 1 )
		{
			for ( size_t i = 0; i < FFT_BUFFER_SIZE; ++i )
			{
				size_t read_pos = ( m_buffer_write_pos + ROLLING_BUFFER_SIZE - FFT_BUFFER_SIZE + i ) % ROLLING_BUFFER_SIZE;
				float sample = std::abs( m_rolling_buffer_mono[read_pos] );
				if ( sample > peak_mono ) peak_mono = sample;
			}
			peak_left = peak_right = peak_mono;
		}
		else if ( frame_channel_count >= 2 )
		{
			for ( size_t i = 0; i < FFT_BUFFER_SIZE; ++i )
			{
				size_t read_pos = ( m_buffer_write_pos + ROLLING_BUFFER_SIZE - FFT_BUFFER_SIZE + i ) % ROLLING_BUFFER_SIZE;
				float left_sample = std::abs( m_rolling_buffer_left[read_pos] );
				if ( left_sample > peak_left ) peak_left = left_sample;

				float right_sample = std::abs( m_rolling_buffer_right[read_pos] );
				if ( right_sample > peak_right ) peak_right = right_sample;

				float max_sample = std::max( left_sample, right_sample );
				if ( max_sample > peak_mono ) peak_mono = max_sample;
			}
		}

		m_vu_mono_in = convert_to_log_scale( peak_mono );
		m_vu_left_in = convert_to_log_scale( peak_left );
		m_vu_right_in = convert_to_log_scale( peak_right );
	}

		// Process FFT only if requested
		if ( m_fft_requested && frame_channel_count >= 1 )
		{
			// Create temporary buffers with the most recent 480 samples
			std::vector<float> temp_mono( FFT_BUFFER_SIZE );
			std::vector<float> temp_left( FFT_BUFFER_SIZE );
			std::vector<float> temp_right( FFT_BUFFER_SIZE );

			for ( size_t i = 0; i < FFT_BUFFER_SIZE; ++i )
			{
				size_t read_pos = ( m_buffer_write_pos + ROLLING_BUFFER_SIZE - FFT_BUFFER_SIZE + i ) % ROLLING_BUFFER_SIZE;
				temp_mono[i] = m_rolling_buffer_mono[read_pos];
				temp_left[i] = m_rolling_buffer_left[read_pos];
				temp_right[i] = m_rolling_buffer_right[read_pos];
			}

			if ( frame_channel_count == 1 )
			{
				calculate_fft_channel( temp_mono.data(), FFT_BUFFER_SIZE, m_fft_mono_in, RESAMPLE_RATE );
				m_fft_left_in = m_fft_mono_in;
				m_fft_right_in = m_fft_mono_in;
			}
			else
			{
				calculate_fft_channel( temp_left.data(), FFT_BUFFER_SIZE, m_fft_left_in, RESAMPLE_RATE );
				calculate_fft_channel( temp_right.data(), FFT_BUFFER_SIZE, m_fft_right_in, RESAMPLE_RATE );

				for ( int i = 0; i < FFT_BANDS; ++i )
					m_fft_mono_in[i] = ( m_fft_left_in[i] + m_fft_right_in[i] ) * 0.5f;
			}
		}

	m_vu_requested = false;
	m_fft_requested = false;
	m_reset_done = false;
}

void FeAudioVisualiser::reset()
{
	if ( m_reset_done )
		return;

	m_vu_mono_in = 0.0f;
	m_vu_left_in = 0.0f;
	m_vu_right_in = 0.0f;
	m_vu_mono_out = 0.0f;
	m_vu_left_out = 0.0f;
	m_vu_right_out = 0.0f;

	std::fill( m_fft_mono_in.begin(), m_fft_mono_in.end(), 0.0f );
	std::fill( m_fft_left_in.begin(), m_fft_left_in.end(), 0.0f );
	std::fill( m_fft_right_in.begin(), m_fft_right_in.end(), 0.0f );

	std::fill( m_rolling_buffer_mono.begin(), m_rolling_buffer_mono.end(), 0.0f );
	std::fill( m_rolling_buffer_left.begin(), m_rolling_buffer_left.end(), 0.0f );
	std::fill( m_rolling_buffer_right.begin(), m_rolling_buffer_right.end(), 0.0f );

	m_buffer_write_pos = 0;
	m_buffer_samples_count = 0;
	m_reset_done = true;
}

void FeAudioVisualiser::update()
{
	update_fall();
}

float FeAudioVisualiser::get_vu_mono() const
{
	m_vu_requested = true;
	return m_vu_mono_out;
}

float FeAudioVisualiser::get_vu_left() const
{
	m_vu_requested = true;
	return m_vu_left_out;
}

float FeAudioVisualiser::get_vu_right() const
{
	m_vu_requested = true;
	return m_vu_right_out;
}

Sqrat::Array FeAudioVisualiser::get_fft_array_mono() const
{
	m_fft_requested = true;
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();

	Sqrat::Table fft_table( vm );
	for ( size_t i = 0; i < m_fft_mono_out.size(); ++i )
		fft_table.SetValue( static_cast<int>( i ), m_fft_mono_out[i] );

	return Sqrat::Array( fft_table.GetObject(), vm );
}

Sqrat::Array FeAudioVisualiser::get_fft_array_left() const
{
	m_fft_requested = true;
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();

	Sqrat::Table fft_table( vm );
	for ( size_t i = 0; i < m_fft_left_out.size(); ++i )
		fft_table.SetValue( static_cast<int>( i ), m_fft_left_out[i] );

	return Sqrat::Array( fft_table.GetObject(), vm );
}

Sqrat::Array FeAudioVisualiser::get_fft_array_right() const
{
	m_fft_requested = true;
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();

	Sqrat::Table fft_table( vm );
	for ( size_t i = 0; i < m_fft_right_out.size(); ++i )
		fft_table.SetValue( static_cast<int>( i ), m_fft_right_out[i] );

	return Sqrat::Array( fft_table.GetObject(), vm );
}

void FeAudioVisualiser::calculate_fft_channel( const float* samples, unsigned int sample_count,
                                               std::vector<float>& fft_bands, float sample_rate ) const
{
	std::fill( fft_bands.begin(), fft_bands.end(), 0.0f );

	if ( sample_count < 256 || sample_count != FFT_BUFFER_SIZE ) return;
	if ( sample_rate <= 0.0f ) return;

	float inv_sample_rate = 1.0f / sample_rate;
	float freq_min = static_cast<float>( FFT_FREQ_MIN );
	float freq_max = static_cast<float>( FFT_FREQ_MAX );

	for ( int band = 0; band < FFT_BANDS; ++band )
	{
		float freq_ratio = static_cast<float>( band ) / ( FFT_BANDS - 1 );
		float linear_freq = freq_min + freq_ratio * ( freq_max - freq_min );
		float log_freq = freq_min * std::pow( freq_max / freq_min, freq_ratio );
		float centre_freq = linear_freq * ( 1.0f - FFT_LINEARITY ) + log_freq * FFT_LINEARITY;

		centre_freq = std::min( centre_freq, freq_max );

		const float angle_step = 2.0f * M_PI * centre_freq * inv_sample_rate;

		// Pre-calculate cos/sin step values for better performance
		const float cos_step = std::cos( angle_step );
		const float sin_step = std::sin( angle_step );
		float real_sum = 0.0f;
		float imag_sum = 0.0f;
		float cos_angle = 1.0f; // cos(0)
		float sin_angle = 0.0f; // sin(0)

		for ( unsigned int i = 0; i < FFT_BUFFER_SIZE; ++i )
		{
			float sample_val = samples[i] * m_window_lut[i];
			real_sum += sample_val * cos_angle;
			imag_sum += sample_val * sin_angle;

			// Update angle using recurrence relation
			float new_cos = cos_angle * cos_step - sin_angle * sin_step;
			float new_sin = sin_angle * cos_step + cos_angle * sin_step;

			cos_angle = new_cos;
			sin_angle = new_sin;
		}

		float magnitude = std::sqrt( real_sum * real_sum + imag_sum * imag_sum );
		magnitude *= 2.0f; // Compensate for windowing function

		// Fast approximation of frequency weighting
		if ( centre_freq > freq_min )
			magnitude *= 1.0f + ( centre_freq / freq_max ) * 10.0f;

		fft_bands[band] = convert_to_log_scale( magnitude );
	}
}

bool FeAudioVisualiser::resample_and_buffer_audio( const float* input_frames, unsigned int input_frame_count,
                                                   unsigned int frame_channel_count, float input_sample_rate )
{
	if ( !input_frames || input_frame_count == 0 || frame_channel_count == 0 )
		return false;

	const std::optional<std::uint32_t> device_sample_rate = sf::PlaybackDevice::getDeviceSampleRate();
	if ( !device_sample_rate )
		return false;

	unsigned int samples_added = 0;

	if ( device_sample_rate.value() == RESAMPLE_RATE )
	{
		for ( unsigned int i = 0; i < input_frame_count; ++i )
		{
			float sample_mono = 0.0f;
			float sample_left = 0.0f;
			float sample_right = 0.0f;

			if ( frame_channel_count == 1 )
			{
				sample_mono = input_frames[i];
				sample_left = sample_right = sample_mono;
			}
			else if ( frame_channel_count >= 2 )
			{
				sample_left = input_frames[i * frame_channel_count];
				sample_right = input_frames[i * frame_channel_count + 1];
				sample_mono = ( sample_left + sample_right ) * 0.5f;
			}

			m_rolling_buffer_mono[m_buffer_write_pos] = sample_mono;
			m_rolling_buffer_left[m_buffer_write_pos] = sample_left;
			m_rolling_buffer_right[m_buffer_write_pos] = sample_right;

			m_buffer_write_pos = ( m_buffer_write_pos + 1 ) % ROLLING_BUFFER_SIZE;
			samples_added++;

			if ( m_buffer_samples_count < ROLLING_BUFFER_SIZE )
				m_buffer_samples_count++;
		}
	}
	else
	{
		const unsigned int divide_factor = static_cast<unsigned int>( device_sample_rate.value() / RESAMPLE_RATE + 0.5f );

		if ( divide_factor > 0 )
		{
			const unsigned int decimated_samples = input_frame_count / divide_factor;

			for ( unsigned int i = 0; i < decimated_samples; ++i )
			{
				unsigned int src_idx = i * divide_factor;
				if ( src_idx >= input_frame_count ) break;

				float sample_left = 0.0f;
				float sample_right = 0.0f;
				float sample_mono = 0.0f;

				if ( frame_channel_count == 1 )
				{
					sample_mono = input_frames[src_idx];
					sample_left = sample_right = sample_mono;
				}
				else if ( frame_channel_count >= 2 )
				{
					sample_left = input_frames[src_idx * frame_channel_count];
					sample_right = input_frames[src_idx * frame_channel_count + 1];
					sample_mono = ( sample_left + sample_right ) * 0.5f;
				}

				m_rolling_buffer_mono[m_buffer_write_pos] = sample_mono;
				m_rolling_buffer_left[m_buffer_write_pos] = sample_left;
				m_rolling_buffer_right[m_buffer_write_pos] = sample_right;

				m_buffer_write_pos = ( m_buffer_write_pos + 1 ) % ROLLING_BUFFER_SIZE;
				samples_added++;

				if ( m_buffer_samples_count < ROLLING_BUFFER_SIZE )
					m_buffer_samples_count++;
			}
		}
	}
	return ( m_buffer_samples_count >= FFT_BUFFER_SIZE && samples_added > 0 );
}

void FeAudioVisualiser::update_fall() const
{
	sf::Time current_time = m_system_clock.getElapsedTime();

	float delta_time = ( current_time - m_last_frame_time ).asSeconds();
	m_last_frame_time = current_time;

	float refresh_rate = 60.0f;
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		refresh_rate = static_cast<float>( fep->get_refresh_rate() );

	float vu_fall_amount = VU_FALL_SPEED * delta_time;
	float fft_fall_amount = FFT_FALL_SPEED * delta_time;

	// Lambda for fall-off calculation ( compiler inlined, faster than a function call )
	auto apply_vu_fall = []( float input_value, float& output_value, float fall_amount )
	{
		if ( input_value > output_value ) output_value = input_value;
		else output_value = std::max( input_value, std::max( 0.0f, output_value - fall_amount ));
	};

	// VU fall
	apply_vu_fall( m_vu_mono_in, m_vu_mono_out, vu_fall_amount );
	apply_vu_fall( m_vu_left_in, m_vu_left_out, vu_fall_amount );
	apply_vu_fall( m_vu_right_in, m_vu_right_out, vu_fall_amount );

	// FFT fall
	for ( int i = 0; i < FFT_BANDS; ++i )
	{
		apply_vu_fall( m_fft_mono_in[i], m_fft_mono_out[i], fft_fall_amount );
		apply_vu_fall( m_fft_left_in[i], m_fft_left_out[i], fft_fall_amount );
		apply_vu_fall( m_fft_right_in[i], m_fft_right_out[i], fft_fall_amount );
	}
}
