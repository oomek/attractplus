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

static const int FFT_BUFFER_SIZE = 1024 * 8;
static const int ROLLING_BUFFER_SIZE = FFT_BUFFER_SIZE * 2;
static const int FFT_FREQ_MIN = 10;
static const int FFT_FREQ_MAX = 16000;
static const int RESAMPLE_RATE = 48000;
static constexpr float FFT_FREQUENCY_LINEARITY = 0.95f; // 0.0 = linear, 1.0 = logarithmic
static constexpr float FFT_AMPLITUDE_LINEARITY = 1.0f; // 0.0 = linear, 1.0 = logarithmic
static constexpr float VU_AMPLITUDE_LINEARITY = 0.25f; // 0.0 = linear, 1.0 = logarithmic
static constexpr float VU_FALL_SPEED = 2.4f;
static constexpr float FFT_FALL_SPEED = 1.2f;
static constexpr float DB_SCALE = 70.0f;
static constexpr float MAX_GAIN = 128.0f;
static constexpr float NORMALISE_MAX_GAIN = 128.0f;
static constexpr float NORMALISE_INCREASE_TIME_MS = 1000.0f;
static constexpr float NORMALISE_DECREASE_TIME_MS = 2.0f;

#define _USE_MATH_DEFINES

#define MEOW_FFT_IMPLEMENTATION
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
                                         unsigned int frame_count, unsigned int channel_count )
{
	// Don't process until all effects are fully constructed
	if ( !m_ready_for_processing )
	{
		// Pass-through during construction
		const unsigned int total_samples = frame_count * channel_count;
		std::memcpy( output_frames, input_frames, total_samples * sizeof( float ));
		return false;
	}

	bool audio_modified = false;
	m_reset_fx = true;

	// First, copy input to output
	const unsigned int total_samples = frame_count * channel_count;
	std::memcpy( output_frames, input_frames, total_samples * sizeof( float ));

	// Create a temporary buffer for chaining effects
	std::vector<float> temp_buffer;
	temp_buffer.resize( total_samples );

	float* current_input = output_frames;
	float* current_output = temp_buffer.data();
	bool use_temp_as_output = true;

	for ( size_t i = 0; i < m_effects.size(); ++i )
	{
		auto& effect = m_effects[i];
		if ( effect )
		{
			bool enabled = effect->is_enabled();

			if ( enabled && effect->process( current_input, current_output, frame_count, channel_count ))
				audio_modified = true;
			else
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
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
	{
		auto* normaliser = get_effect<FeAudioNormaliser>();
		if ( normaliser )
			normaliser->set_enabled( fep->get_fes()->get_loudness() );
	}

	for ( auto& effect : m_effects )
	{
		if ( effect && effect->is_enabled() )
			effect->update();
	}
}

void FeAudioEffectsManager::reset_all()
{
	if ( m_reset_fx )
	{
		for ( auto& effect : m_effects )
		{
			if ( effect )
				effect->reset();
		}
		m_reset_fx = false;
	}
}

void FeAudioEffectsManager::set_ready_for_processing()
{
	m_ready_for_processing = true;
}

std::vector<float> FeAudioVisualiser::m_window_lut;

FeAudioVisualiser::FeAudioVisualiser()
	: m_vu_mono_in( 0.0f ),
	m_vu_mono_out( 0.0f ),
	m_vu_left_in( 0.0f ),
	m_vu_left_out( 0.0f ),
	m_vu_right_in( 0.0f ),
	m_vu_right_out( 0.0f ),
	m_fft_mono_in( FFT_BANDS_MAX, 0.0f ),
	m_fft_mono_out( FFT_BANDS_MAX, 0.0f ),
	m_fft_left_in( FFT_BANDS_MAX, 0.0f ),
	m_fft_left_out( FFT_BANDS_MAX, 0.0f ),
	m_fft_right_in( FFT_BANDS_MAX, 0.0f ),
	m_fft_right_out( FFT_BANDS_MAX, 0.0f ),
	m_last_frame_time( sf::Time::Zero ),
	m_vu_requested( false ),
	m_fft_requested( false ),
	m_vu_request_time( sf::Time::Zero ),
	m_fft_request_time( sf::Time::Zero ),
	m_fft_bands( 32 ),
	m_rolling_buffer_mono( ROLLING_BUFFER_SIZE, 0.0f ),
	m_rolling_buffer_left( ROLLING_BUFFER_SIZE, 0.0f ),
	m_rolling_buffer_right( ROLLING_BUFFER_SIZE, 0.0f ),
	m_buffer_write_pos( 0 ),
	m_buffer_samples_count( 0 ),
	m_phase_accumulator( 0.0 ),
	m_fft_workset( nullptr )
{
	// Initialise Blackman window
	if ( m_window_lut.empty() )
		initialise_window_lut();

	// Initialise MEOW_FFT
	size_t workset_size = meow_fft_generate_workset_real( FFT_BUFFER_SIZE, nullptr );
	m_fft_workset_storage.resize( workset_size );
	m_fft_workset = reinterpret_cast<Meow_FFT_Workset_Real*>( m_fft_workset_storage.data() );
	meow_fft_generate_workset_real( FFT_BUFFER_SIZE, m_fft_workset );

	// Allocate FFT output buffer
	m_fft_output_buffer.resize( FFT_BUFFER_SIZE / 2 + 1 );
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
                                 unsigned int frame_count, unsigned int channel_count )
{
	// It's a passthrough effect
	const unsigned int total_samples = frame_count * channel_count;
	std::memcpy( output_frames, input_frames, total_samples * sizeof( float ));

	std::lock_guard<std::mutex> lock( m_mutex );

	if ( !m_fft_requested && !m_vu_requested )
		return false;

	resample_and_buffer_audio( input_frames, frame_count, channel_count );

	const size_t MIN_FFT_SIZE = 1024;
	bool should_process_fft = m_fft_requested && ( m_buffer_samples_count >= MIN_FFT_SIZE );

	// Process VU meters only if requested
	if ( m_vu_requested )
	{
		float peak_mono = 0.0f;
		float peak_left = 0.0f;
		float peak_right = 0.0f;

		if ( channel_count == 1 )
		{
			for ( unsigned int i = 0; i < frame_count; ++i )
			{
				float sample = std::abs( input_frames[i] );
				if ( sample > peak_mono ) peak_mono = sample;
			}
			peak_left = peak_right = peak_mono;
		}
		else if ( channel_count >= 2 )
		{
			for ( unsigned int i = 0; i < frame_count; ++i )
			{
				float left_sample = std::abs( input_frames[i * channel_count] );
				if ( left_sample > peak_left ) peak_left = left_sample;

				float right_sample = std::abs( input_frames[i * channel_count + 1] );
				if ( right_sample > peak_right ) peak_right = right_sample;

				float max_sample = std::max( left_sample, right_sample );
				if ( max_sample > peak_mono ) peak_mono = max_sample;
			}
		}

		// Hold the maximum peak until fall processing
		float new_mono = convert_to_log_scale( peak_mono, VU_AMPLITUDE_LINEARITY );
		float new_left = convert_to_log_scale( peak_left, VU_AMPLITUDE_LINEARITY );
		float new_right = convert_to_log_scale( peak_right, VU_AMPLITUDE_LINEARITY );

		if ( new_mono > m_vu_mono_in ) m_vu_mono_in = new_mono;
		if ( new_left > m_vu_left_in ) m_vu_left_in = new_left;
		if ( new_right > m_vu_right_in ) m_vu_right_in = new_right;
	}

	// Process FFT only if requested and we have enough data
	if ( should_process_fft && channel_count >= 1 )
	{
		static int fft_call_counter = 0;
		fft_call_counter++;

		float sample_rate_factor = m_device_sample_rate / 48000.0f; // Normalise to 48kHz baseline
		float band_factor = static_cast<float>(m_fft_bands) / 64.0f; // Normalise to 64 bands baseline
		float computational_load = sample_rate_factor * band_factor;

		int throttle_factor = 1;
		if ( computational_load > 2.0f )
			throttle_factor = static_cast<int>( std::min( 4.0f, 1.0f + ( computational_load - 2.0f ) * 1.5f ));

		if ( throttle_factor > 1 && ( fft_call_counter % throttle_factor ) != 0 )
			return false;

		// Only process if we have enough samples
		if ( m_buffer_samples_count < FFT_BUFFER_SIZE )
			return false;

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

		// Use temporary vectors for FFT calculation to avoid race conditions
		std::vector<float> temp_fft_mono( FFT_BANDS_MAX, 0.0f );
		std::vector<float> temp_fft_left( FFT_BANDS_MAX, 0.0f );
		std::vector<float> temp_fft_right( FFT_BANDS_MAX, 0.0f );

		if ( channel_count == 1 )
		{
			calculate_fft_channel( temp_mono.data(), FFT_BUFFER_SIZE, temp_fft_mono, RESAMPLE_RATE );

			std::copy( temp_fft_mono.begin(), temp_fft_mono.begin() + m_fft_bands, m_fft_mono_in.begin() );
			std::copy( temp_fft_mono.begin(), temp_fft_mono.begin() + m_fft_bands, m_fft_left_in.begin() );
			std::copy( temp_fft_mono.begin(), temp_fft_mono.begin() + m_fft_bands, m_fft_right_in.begin() );
		}
		else
		{
			calculate_fft_channel( temp_left.data(), FFT_BUFFER_SIZE, temp_fft_left, RESAMPLE_RATE );
			calculate_fft_channel( temp_right.data(), FFT_BUFFER_SIZE, temp_fft_right, RESAMPLE_RATE );

			for ( int i = 0; i < m_fft_bands; ++i )
				temp_fft_mono[i] = ( temp_fft_left[i] + temp_fft_right[i] ) * 0.5f;

			std::copy( temp_fft_left.begin(), temp_fft_left.begin() + m_fft_bands, m_fft_left_in.begin() );
			std::copy( temp_fft_right.begin(), temp_fft_right.begin() + m_fft_bands, m_fft_right_in.begin() );
			std::copy( temp_fft_mono.begin(), temp_fft_mono.begin() + m_fft_bands, m_fft_mono_in.begin() );
		}
	}

	return false;
}

void FeAudioVisualiser::reset()
{
	std::lock_guard<std::mutex> lock( m_mutex );
	m_vu_mono_in = 0.0f;
	m_vu_left_in = 0.0f;
	m_vu_right_in = 0.0f;

	std::fill( m_fft_mono_in.begin(), m_fft_mono_in.end(), 0.0f );
	std::fill( m_fft_left_in.begin(), m_fft_left_in.end(), 0.0f );
	std::fill( m_fft_right_in.begin(), m_fft_right_in.end(), 0.0f );

	std::fill( m_rolling_buffer_mono.begin(), m_rolling_buffer_mono.end(), 0.0f );
	std::fill( m_rolling_buffer_left.begin(), m_rolling_buffer_left.end(), 0.0f );
	std::fill( m_rolling_buffer_right.begin(), m_rolling_buffer_right.end(), 0.0f );

	std::fill( m_fft_output_buffer.begin(), m_fft_output_buffer.end(), Meow_FFT_Complex{0.0f, 0.0f} );

	m_buffer_write_pos = 0;
	m_buffer_samples_count = 0;
	m_phase_accumulator = 0.0;

	m_vu_request_time = sf::Time::Zero;
	m_fft_request_time = sf::Time::Zero;
}

void FeAudioVisualiser::update()
{
	std::lock_guard<std::mutex> lock( m_mutex );
	update_fall();
}

float FeAudioVisualiser::get_vu_mono() const
{
	m_vu_requested = true;
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		m_vu_request_time = fep->get_layout_time();

	return m_vu_mono_out;
}

float FeAudioVisualiser::get_vu_left() const
{
	m_vu_requested = true;
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		m_vu_request_time = fep->get_layout_time();

	return m_vu_left_out;
}

float FeAudioVisualiser::get_vu_right() const
{
	m_vu_requested = true;
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		m_vu_request_time = fep->get_layout_time();

	return m_vu_right_out;
}

const std::vector<float> *FeAudioVisualiser::get_fft_mono_ptr() const
{
	m_fft_requested = true;
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		m_fft_request_time = fep->get_layout_time();

	return &m_fft_mono_out;
}

const std::vector<float> *FeAudioVisualiser::get_fft_left_ptr() const
{
	m_fft_requested = true;
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		m_fft_request_time = fep->get_layout_time();

	return &m_fft_left_out;
}

const std::vector<float> *FeAudioVisualiser::get_fft_right_ptr() const
{
	m_fft_requested = true;
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		m_fft_request_time = fep->get_layout_time();

	return &m_fft_right_out;
}

void FeAudioVisualiser::calculate_fft_channel( const float* samples, unsigned int sample_count,
                                               std::vector<float>& fft_bands, float sample_rate ) const
{
	// Create a temporary input buffer
	std::vector<float> fft_input( FFT_BUFFER_SIZE, 0.0f );

	// Apply Blackman window
	for ( unsigned int i = 0; i < sample_count; ++i )
		fft_input[i] = samples[i] * m_window_lut[i];

	meow_fft_real( m_fft_workset, fft_input.data(), m_fft_output_buffer.data() );

	// Convert FFT bins to frequency bands
	// Accumulate energy across frequency ranges
	float freq_min = static_cast<float>( FFT_FREQ_MIN );
	float freq_max = static_cast<float>( FFT_FREQ_MAX );
	float nyquist_freq = sample_rate * 0.5f;
	float bin_width = sample_rate / FFT_BUFFER_SIZE;

	freq_max = std::min( freq_max, nyquist_freq );

	for ( int band = 0; band < m_fft_bands; ++band )
	{
		// Calculate frequency range for this band
		float band_start_ratio = static_cast<float>( band ) / m_fft_bands;
		float band_end_ratio = static_cast<float>( band + 1 ) / m_fft_bands;

		// Apply linear/logarithmic blending to band edges
		float linear_start = freq_min + band_start_ratio * ( freq_max - freq_min );
		float linear_end = freq_min + band_end_ratio * ( freq_max - freq_min );
		float log_start = freq_min * std::pow( freq_max / freq_min, band_start_ratio );
		float log_end = freq_min * std::pow( freq_max / freq_min, band_end_ratio );

		float start_freq = linear_start * ( 1.0f - FFT_FREQUENCY_LINEARITY ) + log_start * FFT_FREQUENCY_LINEARITY;
		float end_freq = linear_end * ( 1.0f - FFT_FREQUENCY_LINEARITY ) + log_end * FFT_FREQUENCY_LINEARITY;

		// Convert to bin indices
		int start_bin = std::max( 1, static_cast<int>( start_freq / bin_width ) ); // Skip DC bin
		int end_bin = std::min( static_cast<int>( end_freq / bin_width ), static_cast<int>( m_fft_output_buffer.size() ) - 1 );

		// Ensure we have at least one bin
		if ( end_bin <= start_bin ) end_bin = start_bin + 1;

		// Accumulate energy across the frequency range
		float max_energy = 0.0f;
		float total_energy = 0.0f;
		int bin_count = 0;

		for ( int bin = start_bin; bin <= end_bin; ++bin )
		{
			float real_part, imag_part;

			if ( bin == 0 )
			{
				real_part = m_fft_output_buffer[0].r;
				imag_part = 0.0f;
			}
			else if ( bin == FFT_BUFFER_SIZE / 2 )
			{
				real_part = m_fft_output_buffer[0].j;
				imag_part = 0.0f;
			}
			else
			{
				real_part = m_fft_output_buffer[bin].r;
				imag_part = m_fft_output_buffer[bin].j;
			}

			float magnitude = std::sqrt( real_part * real_part + imag_part * imag_part );
			float energy = magnitude * magnitude;

			// Track both maximum and total energy
			if ( energy > max_energy ) max_energy = energy;
			total_energy += energy;
			bin_count++;
		}

		// Use a blend of maximum and average energy for better responsiveness
		float avg_energy = total_energy / bin_count;
		float blend_factor = 0.7f;
		float final_energy = max_energy * blend_factor + avg_energy * ( 1.0f - blend_factor );

		// Convert back to magnitude
		float band_magnitude = std::sqrt( final_energy );

		// Scale the output
		band_magnitude *= 1.0f / FFT_BUFFER_SIZE;

		// Frequency weighting
		float center_freq = ( start_freq + end_freq ) * 0.5f;
		if ( center_freq > freq_min )
		{
			float octaves = std::log2( center_freq / freq_min );
			float db_per_octave = 1.0f;
			float db_compensation = octaves * db_per_octave;
			float linear_compensation = std::pow( 10.0f, db_compensation / 20.0f );
			band_magnitude *= linear_compensation / db_per_octave;
		}

		float scaled_result = convert_to_log_scale( band_magnitude, FFT_AMPLITUDE_LINEARITY );
		fft_bands[band] = scaled_result;
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
	sf::Time current_time =  sf::Time::Zero;
	float frame_time = 0.0;

	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
	{
		current_time = fep->get_layout_time();
		frame_time = fep->get_layout_frame_time() * 0.001f;
	}

	float vu_fall_amount = VU_FALL_SPEED * frame_time;
	float fft_fall_amount = FFT_FALL_SPEED * frame_time;

	// Lambda for fall-off calculation ( compiler inlined, faster than a function call )
	auto apply_vu_fall = []( float input_value, float& output_value, float fall_amount )
	{
		if ( input_value > output_value ) output_value = input_value;
		else output_value = std::max( input_value, std::max( 0.0f, output_value - fall_amount ));
	};

	apply_vu_fall( m_vu_mono_in, m_vu_mono_out, vu_fall_amount );
	apply_vu_fall( m_vu_left_in, m_vu_left_out, vu_fall_amount );
	apply_vu_fall( m_vu_right_in, m_vu_right_out, vu_fall_amount );

	m_vu_mono_in = 0.0f;
	m_vu_left_in = 0.0f;
	m_vu_right_in = 0.0f;

	// Clear flags if 100ms has passed since last request
	const float REQUEST_TIMEOUT_MS = 100.0f;
	float vu_time_since_request = ( current_time - m_vu_request_time ).asMilliseconds();
	float fft_time_since_request = ( current_time - m_fft_request_time ).asMilliseconds();

	if ( vu_time_since_request > REQUEST_TIMEOUT_MS )
		m_vu_requested = false;

	if ( fft_time_since_request > REQUEST_TIMEOUT_MS )
		m_fft_requested = false;

	for ( int i = 0; i < m_fft_bands; ++i )
	{
		apply_vu_fall( m_fft_mono_in[i], m_fft_mono_out[i], fft_fall_amount );
		apply_vu_fall( m_fft_left_in[i], m_fft_left_out[i], fft_fall_amount );
		apply_vu_fall( m_fft_right_in[i], m_fft_right_out[i], fft_fall_amount );
	}

	// Reset FFT input values so they can fall properly when no audio is present
	m_fft_mono_in.assign( m_fft_bands, 0.0f );
	m_fft_left_in.assign( m_fft_bands, 0.0f );
	m_fft_right_in.assign( m_fft_bands, 0.0f );
}

FeAudioNormaliser::FeAudioNormaliser()
	: m_current_gain( 1.0f ),
	m_target_reached( false ),
	m_startup_delay( 0 ),
	m_media_volume( 1.0f )
{
}

bool FeAudioNormaliser::process( const float *input_frames, float *output_frames,
                                 unsigned int frame_count, unsigned int channel_count )
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

	float current_gain;
	float previous_gain;
	bool target_reached;
	size_t startup_delay;
	float media_volume;
	float max_peak;

	// Read shared variables with minimal lock
	{
		std::lock_guard<std::mutex> lock( m_mutex );
		current_gain = m_current_gain;
		previous_gain = m_current_gain;
		target_reached = m_target_reached;
		startup_delay = m_startup_delay;
		media_volume = m_media_volume;
		max_peak = m_max_peak;
	}

	// Ignore the first 100ms to allow the DC filter to settle down
	size_t startup_delay_threshold = static_cast<size_t>( 0.1f * m_device_sample_rate );
	if ( startup_delay < startup_delay_threshold )
	{
		startup_delay += frame_count;
		for ( unsigned int i = 0; i < frame_count; ++i )
		{
			for ( unsigned int ch = 0; ch < channel_count; ++ch )
			{
				size_t idx = i * channel_count + ch;
				output_frames[idx] = input_frames[idx];
			}
		}

		std::lock_guard<std::mutex> lock( m_mutex );
		m_startup_delay = startup_delay;

		return true;
	}

	const float target_level = media_volume;

	if ( current_peak > max_peak )
		max_peak = current_peak;

	if ( max_peak > 0.0001f )
	{
		float output_level = current_peak * current_gain;

		if ( !target_reached && output_level >= target_level )
			target_reached = true;

		if ( target_reached )
		{
			if ( output_level > target_level )
			{
				// Over the target, fast gain decrease
				const float buffers_per_second = m_device_sample_rate / frame_count;
				const float alpha = 1.0f - std::exp( -1.0f / ( NORMALISE_DECREASE_TIME_MS * 0.001f * buffers_per_second ));
				float needed_gain = target_level / max_peak;
				current_gain = current_gain * ( 1.0f - alpha ) + needed_gain * alpha;
			}
		}
		else if ( !target_reached && output_level < target_level )
		{
			// Slow gain increase
			const float buffers_per_second = m_device_sample_rate / frame_count;
			const float alpha = 1.0f - std::exp( -1.0f / ( NORMALISE_INCREASE_TIME_MS * 0.001f * buffers_per_second ));
			float needed_gain = target_level / max_peak;
			current_gain = current_gain * ( 1.0f - alpha ) + needed_gain * alpha;
		}
		// Otherwise, maintain current gain
	}

	if ( current_gain > NORMALISE_MAX_GAIN )
		current_gain = NORMALISE_MAX_GAIN;

	if ( current_gain < 1.0f )
		current_gain = 1.0f;

	// Apply gain ramp to output using weighted average
	for ( unsigned int i = 0; i < frame_count; ++i )
	{
		float weight;
		if ( frame_count <= 1 ) // Sanity check to prevent div 0
			weight = 1.0f;
		else
			weight = (float)i / (float)( frame_count - 1 );

		float interpolated_gain = previous_gain * ( 1.0f - weight ) + current_gain * weight;

		for ( unsigned int ch = 0; ch < channel_count; ++ch )
		{
			size_t idx = i * channel_count + ch;
			output_frames[idx] = input_frames[idx] * interpolated_gain;
		}
	}

	std::lock_guard<std::mutex> lock( m_mutex );
	m_current_gain = current_gain;
	m_target_reached = target_reached;
	m_max_peak = max_peak;

	return true;
}

void FeAudioNormaliser::update()
{
	// Nothing to update per frame
}

void FeAudioNormaliser::reset()
{
	std::lock_guard<std::mutex> lock( m_mutex );
	m_current_gain = 1.0f;
	m_target_reached = false;
	m_startup_delay = 0;
	m_max_peak = 0.0f;
}

void FeAudioNormaliser::set_media_volume( float volume )
{
	std::lock_guard<std::mutex> lock( m_mutex );
	m_target_reached = false;
	m_media_volume = volume;
}

FeAudioDCFilter::FeAudioDCFilter( float cutoff_freq )
	: m_cutoff_freq( cutoff_freq ),
	m_coefficient( 0.0f )
{
}

bool FeAudioDCFilter::process( const float *input_frames, float *output_frames,
                               unsigned int frame_count, unsigned int channel_count )
{
	float coefficient;
	std::vector<float> prev_input;
	std::vector<float> prev_output;

	// Read shared variables with minimal lock
	{
		std::lock_guard<std::mutex> lock( m_mutex );
		coefficient = m_coefficient;
		prev_input = m_prev_input;
		prev_output = m_prev_output;
	}

	if ( coefficient == 0.0f )
	{
		// Calculate one-pole one-zero DC filter coefficient
		const float omega = 2.0f * M_PI * m_cutoff_freq / m_device_sample_rate;
		coefficient = std::exp( -omega );
	}

	if ( prev_input.size() != channel_count )
	{
		prev_input.resize( channel_count, 0.0f );
		prev_output.resize( channel_count, 0.0f );
	}

	// Apply one-pole one-zero DC filter
	// y[n] = x[n] - x[n-1] + coefficient * y[n-1]
	for ( unsigned int frame = 0; frame < frame_count; ++frame )
	{
		for ( unsigned int ch = 0; ch < channel_count; ++ch )
		{
			const unsigned int sample_idx = frame * channel_count + ch;
			const float input_sample = input_frames[sample_idx];
			const float output_sample = input_sample - prev_input[ch] + coefficient * prev_output[ch];
			prev_input[ch] = input_sample;
			prev_output[ch] = output_sample;
			output_frames[sample_idx] = output_sample;
		}
	}

	std::lock_guard<std::mutex> lock( m_mutex );
	m_coefficient = coefficient;
	m_prev_input = prev_input;
	m_prev_output = prev_output;

	return true;
}

void FeAudioDCFilter::update()
{
	// Nothing to update per frame
}

void FeAudioDCFilter::reset()
{
	std::lock_guard<std::mutex> lock( m_mutex );
	std::fill( m_prev_input.begin(), m_prev_input.end(), 0.0f );
	std::fill( m_prev_output.begin(), m_prev_output.end(), 0.0f );
	m_coefficient = 0.0f;
}

void FeAudioVisualiser::initialise_window_lut()
{
	// Pre-calculate Blackman window
	const float a0 = 0.42f;
	const float a1 = 0.5f;
	const float a2 = 0.08f;
	float coherent_gain_sum = 0.0f;
	std::vector<float> temp_window( FFT_BUFFER_SIZE );

	for ( int i = 0; i < FFT_BUFFER_SIZE; ++i )
	{
		temp_window[i] = a0 - a1 * std::cos( 2.0f * M_PI * i / ( FFT_BUFFER_SIZE - 1 ))
		                    + a2 * std::cos( 4.0f * M_PI * i / ( FFT_BUFFER_SIZE - 1 ));
		coherent_gain_sum += temp_window[i];
	}
	const float blackman_coherent_gain = coherent_gain_sum / FFT_BUFFER_SIZE;
	const float window_scale = 1.0f / blackman_coherent_gain;

	m_window_lut.resize( FFT_BUFFER_SIZE );
	for ( int i = 0; i < FFT_BUFFER_SIZE; ++i )
		m_window_lut[i] = temp_window[i] * window_scale;
}

void FeAudioVisualiser::set_fft_bands( int count )
{
	count = std::max( 2, std::min( count, FFT_BANDS_MAX ));
	m_fft_bands = count;
}
