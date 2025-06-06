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

#ifndef FE_AUDIO_FX_HPP
#define FE_AUDIO_FX_HPP

#include <SFML/System.hpp>
#include <vector>
#include <sqrat.h>
#include <cmath>

class FeAudioVisualiser
{
public:
	FeAudioVisualiser();
	~FeAudioVisualiser();

	void process( const float *input_frames , unsigned int input_frame_count ,
	                            unsigned int frame_channel_count , float sample_rate );
	void update();
	void reset();

	float get_vu_mono() const;
	float get_vu_left() const;
	float get_vu_right() const;

	// Sqrat bindings for FFT arrays
	Sqrat::Array get_fft_array_mono() const;
	Sqrat::Array get_fft_array_left() const;
	Sqrat::Array get_fft_array_right() const;

	static const int FFT_BANDS = 32;
	static const int FFT_BUFFER_SIZE = 480;
	static const int ROLLING_BUFFER_SIZE = 960;
	static const int FFT_FREQ_MIN = 20;
	static const int FFT_FREQ_MAX = 16000;
	static constexpr float FFT_LINEARITY = 0.9f; // 0.0 = linear, 1.0 = logarithmic
	static constexpr float AMPLITUDE_LINEARITY = 0.5f; // 0.0 = linear, 1.0 = logarithmic
	static constexpr float DB_SCALE = 60.0f;
	static constexpr float VU_FALL_SPEED = 2.4f;
	static constexpr float FFT_FALL_SPEED = 1.2f;
	static const int RESAMPLE_RATE = 48000;

private:
	// VU meter data
	mutable float m_vu_mono_in;
	mutable float m_vu_mono_out;
	mutable float m_vu_left_in;
	mutable float m_vu_left_out;
	mutable float m_vu_right_in;
	mutable float m_vu_right_out;

	// FFT analysis data
	mutable std::vector<float> m_fft_mono_in;
	mutable std::vector<float> m_fft_mono_out;
	mutable std::vector<float> m_fft_left_in;
	mutable std::vector<float> m_fft_left_out;
	mutable std::vector<float> m_fft_right_in;
	mutable std::vector<float> m_fft_right_out;

	mutable sf::Time m_last_frame_time;
	mutable sf::Clock m_system_clock;
	mutable bool m_reset_done;

	// Usage tracking for performance optimization
	mutable bool m_vu_requested;
	mutable bool m_fft_requested;

	// Rolling buffer and resampling for consistent FFT
	mutable std::vector<float> m_rolling_buffer_mono;
	mutable std::vector<float> m_rolling_buffer_left;
	mutable std::vector<float> m_rolling_buffer_right;
	mutable std::vector<float> m_window_lut;
	mutable size_t m_buffer_write_pos;
	mutable size_t m_buffer_samples_count;

	void calculate_fft_channel( const float *samples , unsigned int sample_count ,
	                            std::vector<float> &fft_bands , float sample_rate ) const;
	void update_fall() const;
	float convert_to_log_scale( float linear_value ) const;
	bool resample_and_buffer_audio( const float* input_frames, unsigned int input_frame_count,
	                                unsigned int frame_channel_count, float input_sample_rate );
};

#endif // FE_AUDIO_FX_HPP
