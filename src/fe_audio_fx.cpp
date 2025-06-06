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

static const int FFT_BUFFER_SIZE = 1024;
static const int ROLLING_BUFFER_SIZE = 2048;
static const int FFT_FREQ_MIN = 20;
static const int FFT_FREQ_MAX = 16000;
static const int RESAMPLE_RATE = 48000;
static constexpr float FFT_FREQUENCY_LINEARITY = 0.9f; // 0.0 = linear, 1.0 = logarithmic
static constexpr float FFT_AMPLITUDE_LINEARITY = 1.0f; // 0.0 = linear, 1.0 = logarithmic
static constexpr float VU_AMPLITUDE_LINEARITY = 0.5f; // 0.0 = linear, 1.0 = logarithmic
static constexpr float VU_FALL_SPEED = 2.4f;
static constexpr float FFT_FALL_SPEED = 1.2f;
static constexpr float DB_SCALE = 60.0f;
static constexpr float MAX_GAIN = 128.0f;

#define _USE_MATH_DEFINES

#include "fe_audio_fx.hpp"
#include "fe_settings.hpp"
#include "fe_present.hpp"

#include <SFML/Audio/PlaybackDevice.hpp>
#include <algorithm>
#include <cstring>
#include <cstdio>

// Undefine Windows GetObject macro that conflicts with Sqrat
#ifdef GetObject
#undef GetObject
#endif

FeAudioEffect::FeAudioEffect()
{
	const std::optional<std::uint32_t> device_sample_rate = sf::PlaybackDevice::getDeviceSampleRate();
	m_device_sample_rate = static_cast<float>( device_sample_rate.value_or( 48000.0f ));
}

FeAudioEffectsManager::FeAudioEffectsManager()
{
}

FeAudioEffectsManager::~FeAudioEffectsManager()
{
}

void FeAudioEffectsManager::add_effect( std::unique_ptr<FeAudioEffect> effect )
{
	if ( effect )
		m_effects.push_back( std::move( effect ));
}

bool FeAudioEffectsManager::process_all( const float *input_frames, float *output_frames,
                                         unsigned int frame_count, unsigned int channel_count,
                                         float media_sample_rate )
{
	bool audio_modified = false;

	// First, copy input to output
	if ( input_frames && output_frames && frame_count > 0 )
	{
		const unsigned int total_samples = frame_count * channel_count;
		std::memcpy( output_frames, input_frames, total_samples * sizeof( float ));
	}

	// Create a temporary buffer for chaining effects
	std::vector<float> temp_buffer;
	const unsigned int total_samples = frame_count * channel_count;
	temp_buffer.resize( total_samples );

	float* current_input = output_frames;
	float* current_output = temp_buffer.data();
	bool use_temp_as_output = true;

	for ( size_t i = 0; i < m_effects.size(); ++i )
	{
		auto& effect = m_effects[i];
		if ( effect && effect->is_enabled() )
		{
			if ( effect->process( current_input, current_output, frame_count, channel_count, media_sample_rate ))
				audio_modified = true;
		}
		else if ( effect )
		{
			// Pass-through
			std::memcpy( current_output, current_input, frame_count * channel_count * sizeof( float ) );
		}

		std::swap( current_input, current_output );
		use_temp_as_output = !use_temp_as_output;
	}

	// Ensure final result is in output_frames
	if ( current_input != output_frames )
	{
		std::memcpy( output_frames, current_input, total_samples * sizeof( float ));
	}

	return audio_modified;
}

void FeAudioEffectsManager::update_all()
{
	for ( auto& effect : m_effects )
	{
		if ( effect && effect->is_enabled() )
			effect->update();
	}
}

void FeAudioEffectsManager::reset_all()
{
	for ( auto& effect : m_effects )
	{
		if ( effect )
			effect->reset();
	}
}

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
	m_buffer_samples_count( 0 ),
	m_phase_accumulator( 0.0 )
{
	// Pre-calculate Blackman window for FFT
	const float a0 = 0.42f;
	const float a1 = 0.5f;
	const float a2 = 0.08f;
	float coherent_gain_sum = 0.0f;
	std::vector<float> temp_window( FFT_BUFFER_SIZE );

	for ( unsigned int i = 0; i < FFT_BUFFER_SIZE; ++i )
	{
		temp_window[i] = a0 - a1 * std::cos( 2.0f * M_PI * i / ( FFT_BUFFER_SIZE - 1 ))
		                    + a2 * std::cos( 4.0f * M_PI * i / ( FFT_BUFFER_SIZE - 1 ));
		coherent_gain_sum += temp_window[i];
	}
	const float blackman_coherent_gain = coherent_gain_sum / FFT_BUFFER_SIZE;
	const float window_scale = ( 2.0f / FFT_BUFFER_SIZE ) * ( 1.0f / blackman_coherent_gain ) * 2.0f; // multiply by 2.0 for SFML's -0.5 to 0.5 range

	for ( unsigned int i = 0; i < FFT_BUFFER_SIZE; ++i )
		m_window_lut[i] = temp_window[i] * window_scale;
}

FeAudioVisualiser::~FeAudioVisualiser()
{
}

float FeAudioVisualiser::convert_to_log_scale( float linear_value, float amplitude_linearity ) const
{
	if ( linear_value <= 1e-8f )
		return 0.0f;

	float linear_scaled = std::min( 1.0f, linear_value );
	float db_value = 20.0f * std::log2( linear_value + 1e-10f ) * 0.30103f;
	float log_scaled = std::max( 0.0f, std::min( 1.0f, ( db_value + DB_SCALE ) / DB_SCALE ));

	// Blend between linear and logarithmic
	return linear_scaled * ( 1.0f - amplitude_linearity ) + log_scaled * amplitude_linearity;
}

bool FeAudioVisualiser::process( const float* input_frames, float* output_frames,
                                 unsigned int frame_count, unsigned int channel_count,
                                 float media_sample_rate )
{
	// It's a passthrough effect
	if ( input_frames && output_frames && frame_count > 0 && channel_count > 0 )
	{
		const unsigned int total_samples = frame_count * channel_count;
		std::memcpy( output_frames, input_frames, total_samples * sizeof( float ));
	}

	bool buffer_ready = resample_and_buffer_audio( input_frames, frame_count, channel_count );

	if ( !buffer_ready )
		return false;

	if ( !m_vu_requested && !m_fft_requested )
		return false;

	// Process VU meters only if requested
	if ( m_vu_requested )
	{
		float peak_mono = 0.0f;
		float peak_left = 0.0f;
		float peak_right = 0.0f;

		if ( channel_count == 1 )
		{
			for ( size_t i = 0; i < FFT_BUFFER_SIZE; ++i )
			{
				size_t read_pos = ( m_buffer_write_pos + ROLLING_BUFFER_SIZE - FFT_BUFFER_SIZE + i ) % ROLLING_BUFFER_SIZE;
				float sample = std::abs( m_rolling_buffer_mono[read_pos] );
				if ( sample > peak_mono ) peak_mono = sample;
			}
			peak_left = peak_right = peak_mono;
		}
		else if ( channel_count >= 2 )
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
		m_vu_mono_in = convert_to_log_scale( peak_mono * 2.0f, VU_AMPLITUDE_LINEARITY ); // multiply by 2.0 for SFML's -0.5 to 0.5 range
		m_vu_left_in = convert_to_log_scale( peak_left * 2.0f, VU_AMPLITUDE_LINEARITY );
		m_vu_right_in = convert_to_log_scale( peak_right * 2.0f, VU_AMPLITUDE_LINEARITY );
	}

	// Process FFT only if requested
	if ( m_fft_requested && channel_count >= 1 )
	{
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

		if ( channel_count == 1 )
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

	return false;
}

void FeAudioVisualiser::reset()
{
	if ( m_reset_done )
		return;

	m_vu_mono_in = 0.0f;
	m_vu_left_in = 0.0f;
	m_vu_right_in = 0.0f;

	std::fill( m_fft_mono_in.begin(), m_fft_mono_in.end(), 0.0f );
	std::fill( m_fft_left_in.begin(), m_fft_left_in.end(), 0.0f );
	std::fill( m_fft_right_in.begin(), m_fft_right_in.end(), 0.0f );

	std::fill( m_rolling_buffer_mono.begin(), m_rolling_buffer_mono.end(), 0.0f );
	std::fill( m_rolling_buffer_left.begin(), m_rolling_buffer_left.end(), 0.0f );
	std::fill( m_rolling_buffer_right.begin(), m_rolling_buffer_right.end(), 0.0f );

	m_buffer_write_pos = 0;
	m_buffer_samples_count = 0;
	m_phase_accumulator = 0.0;
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
		float centre_freq = linear_freq * ( 1.0f - FFT_FREQUENCY_LINEARITY ) + log_freq * FFT_FREQUENCY_LINEARITY;

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

		// Fast approximation of frequency weighting
		if ( centre_freq > freq_min )
			magnitude *= 1.0f + ( centre_freq / freq_max ) * 6.0f;

		fft_bands[band] = convert_to_log_scale( magnitude, FFT_AMPLITUDE_LINEARITY );
	}
}

bool FeAudioVisualiser::resample_and_buffer_audio( const float* input_frames, unsigned int input_frame_count,
                                                   unsigned int frame_channel_count )
{
	if ( !input_frames || input_frame_count == 0 || frame_channel_count == 0 )
		return false;

	unsigned int samples_added = 0;

	if ( static_cast<int>( m_device_sample_rate ) == RESAMPLE_RATE )
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
		return ( m_buffer_samples_count >= FFT_BUFFER_SIZE && samples_added > 0 );
	}

	const double step_size = static_cast<double>( m_device_sample_rate ) / static_cast<double>( RESAMPLE_RATE );

	for ( unsigned int i = 0; i < input_frame_count; ++i )
	{
		float sample_left = 0.0f;
		float sample_right = 0.0f;
		float sample_mono = 0.0f;

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

		m_phase_accumulator += 1.0;

		// Output sample when we've accumulated enough input
		while ( m_phase_accumulator >= step_size )
		{
			// Always use the most recent sample (simple hold)
			m_rolling_buffer_mono[m_buffer_write_pos] = sample_mono;
			m_rolling_buffer_left[m_buffer_write_pos] = sample_left;
			m_rolling_buffer_right[m_buffer_write_pos] = sample_right;

			m_buffer_write_pos = ( m_buffer_write_pos + 1 ) % ROLLING_BUFFER_SIZE;
			samples_added++;

			if ( m_buffer_samples_count < ROLLING_BUFFER_SIZE )
				m_buffer_samples_count++;

			m_phase_accumulator -= step_size;
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

	apply_vu_fall( m_vu_mono_in, m_vu_mono_out, vu_fall_amount );
	apply_vu_fall( m_vu_left_in, m_vu_left_out, vu_fall_amount );
	apply_vu_fall( m_vu_right_in, m_vu_right_out, vu_fall_amount );

	for ( int i = 0; i < FFT_BANDS; ++i )
	{
		apply_vu_fall( m_fft_mono_in[i], m_fft_mono_out[i], fft_fall_amount );
		apply_vu_fall( m_fft_left_in[i], m_fft_left_out[i], fft_fall_amount );
		apply_vu_fall( m_fft_right_in[i], m_fft_right_out[i], fft_fall_amount );	}
}

FeAudioNormaliser::FeAudioNormaliser()
	: m_current_gain( 1.0f ),
	m_target_reached( false ),
	m_startup_delay( 0 ),
	m_media_volume( 1.0f )
{
}

bool FeAudioNormaliser::process( const float *input_frames, float *output_frames,
                                 unsigned int frame_count, unsigned int channel_count,
                                 float media_sample_rate )
{
	float current_peak = 0.0f;
	for ( unsigned int i = 0; i < frame_count; ++i )
	{
		for ( unsigned int ch = 0; ch < channel_count; ++ch )
		{
			size_t idx = i * channel_count + ch;
			float sample = std::abs(input_frames[idx]);
			if ( sample > current_peak )
				current_peak = sample;
		}
	}

	// Ignore the first 100ms to allow the DC filter to settle down
	size_t startup_delay = static_cast<size_t>( 0.1f * m_device_sample_rate );
	if ( m_startup_delay < startup_delay )
	{
		m_startup_delay += frame_count;
		for ( unsigned int i = 0; i < frame_count; ++i )
		{
			for ( unsigned int ch = 0; ch < channel_count; ++ch )
			{
				size_t idx = i * channel_count + ch;
				output_frames[idx] = input_frames[idx];
			}
		}
		return true;
	}

	const float target_level = 0.5f * m_media_volume;

	if ( current_peak > 0.0001f )
	{
		float output_level = current_peak * m_current_gain;

		if ( !m_target_reached && output_level >= target_level )
			m_target_reached = true;

		if ( m_target_reached )
		{
			if ( output_level > target_level )
			{
				// Over the target, fast gain decrease
				const float decrease_time_ms = 10.0f;
				const float buffers_per_second = m_device_sample_rate / frame_count;
				const float fast_response = 1.0f / ( decrease_time_ms * 0.001f * buffers_per_second );
				float needed_gain = target_level / current_peak;
				m_current_gain += ( needed_gain - m_current_gain ) * fast_response;
			}
		}
		else if ( !m_target_reached && output_level < target_level )
		{
			// Slow gain increase
			const float increase_time_ms = 1000.0f;
			const float buffers_per_second = m_device_sample_rate / frame_count;
			const float slow_response = 1.0f / ( increase_time_ms * 0.001f * buffers_per_second );
			float needed_gain = target_level / current_peak;
			m_current_gain += (needed_gain - m_current_gain) * slow_response;
		}
		// Otherwise, maintain current gain
	}

	if ( m_current_gain > MAX_GAIN )
		m_current_gain = MAX_GAIN;

	if ( m_current_gain < 1.0f )
		m_current_gain = 1.0f;

	// Apply gain to output
	for ( unsigned int i = 0; i < frame_count; ++i )
	{
		for ( unsigned int ch = 0; ch < channel_count; ++ch )
		{
			size_t idx = i * channel_count + ch;
			output_frames[idx] = input_frames[idx] * m_current_gain;
		}
	}

	return true;
}

void FeAudioNormaliser::update()
{
	// Nothing to update per frame
}

void FeAudioNormaliser::reset()
{
	m_current_gain = 1.0f;
	m_target_reached = false;
	m_startup_delay = 0;
}

void FeAudioNormaliser::set_media_volume( float volume )
{
	m_target_reached = false;
	m_media_volume = volume;
}

FeAudioDCFilter::FeAudioDCFilter( float cutoff_freq )
	: m_cutoff_freq( cutoff_freq ),
	m_coefficient( 0.0f )
{
}

bool FeAudioDCFilter::process( const float *input_frames, float *output_frames,
                               unsigned int frame_count, unsigned int channel_count,
                               float media_sample_rate )
{
	if ( m_coefficient == 0.0f )
	{
		const float rc = 1.0f / ( 2.0f * M_PI * m_cutoff_freq );
		const float dt = 1.0f / m_device_sample_rate;
		m_coefficient = rc / ( rc + dt );
	}

	if ( m_prev_input.size() != channel_count )
	{
		m_prev_input.resize( channel_count, 0.0f );
		m_prev_output.resize( channel_count, 0.0f );
	}

	// Apply 1-pole highpass filter
	for ( unsigned int frame = 0; frame < frame_count; ++frame )
	{
		for ( unsigned int ch = 0; ch < channel_count; ++ch )
		{
			const unsigned int sample_idx = frame * channel_count + ch;
			const float input_sample = input_frames[sample_idx];
			const float output_sample = m_coefficient * ( m_prev_output[ch] + input_sample - m_prev_input[ch] );
			m_prev_input[ch] = input_sample;
			m_prev_output[ch] = output_sample;
			output_frames[sample_idx] = output_sample;
		}
	}

	return true;
}

void FeAudioDCFilter::update()
{
	// Nothing to update per frame
}

void FeAudioDCFilter::reset()
{
	bool needs_reset = false;

	for ( size_t i = 0; i < m_prev_input.size() && !needs_reset; ++i )
		if ( std::abs( m_prev_input[i] ) > 0.1f || std::abs( m_prev_output[i] ) > 0.1f )
			needs_reset = true;

	if ( needs_reset )
	{
		std::fill( m_prev_input.begin(), m_prev_input.end(), 0.0f );
		std::fill( m_prev_output.begin(), m_prev_output.end(), 0.0f );
	}
}
