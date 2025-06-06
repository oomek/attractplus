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
#include <memory>
#include <sqrat.h>
#include <cmath>

class FeAudioEffect
{
public:
	FeAudioEffect();
	virtual ~FeAudioEffect() = default;

	// Pure virtual methods that all effects must implement
	virtual bool process( const float *input_frames, float *output_frames,
	                           unsigned int frame_count, unsigned int channel_count,
	                           float media_sample_rate ) = 0;
	virtual void update() = 0;
	virtual void reset() = 0;

	// Optional methods that effects can override
	virtual bool is_enabled() const { return m_enabled; }
	virtual void set_enabled( bool enabled ) { m_enabled = enabled; }

protected:
	bool m_enabled = true;
	float m_device_sample_rate;
};


class FeAudioEffectsManager
{
public:
	FeAudioEffectsManager();
	~FeAudioEffectsManager();

	void add_effect( std::unique_ptr<FeAudioEffect> effect );
	bool process_all( const float *input_frames, float *output_frames,
	                            unsigned int frame_count, unsigned int channel_count,
	                            float media_sample_rate );

	void update_all();
	void reset_all();

	// Get specific effect (returns nullptr if not found or wrong type)
	template<typename T>
	T* get_effect() const;

private:
	std::vector<std::unique_ptr<FeAudioEffect>> m_effects;
};


class FeAudioDCFilter : public FeAudioEffect
{
public:
	FeAudioDCFilter( float cutoff_freq = 20.0f );
	~FeAudioDCFilter() override = default;

	bool process( const float *input_frames, float *output_frames,
	                   unsigned int frame_count, unsigned int channel_count,
	                   float media_sample_rate ) override;

	void update() override;
	void reset() override;

private:
	float m_cutoff_freq;
	mutable float m_coefficient;
	mutable std::vector<float> m_prev_input;  // Previous input samples per channel
	mutable std::vector<float> m_prev_output; // Previous output samples per channel
};


class FeAudioVisualiser : public FeAudioEffect
{
public:
	FeAudioVisualiser();
	~FeAudioVisualiser() override;

	bool process( const float *input_frames, float *output_frames,
	                   unsigned int frame_count, unsigned int channel_count,
	                   float media_sample_rate ) override;

	void update() override;
	void reset() override;

	float get_vu_mono() const;
	float get_vu_left() const;
	float get_vu_right() const;

	// Sqrat bindings for FFT arrays
	Sqrat::Array get_fft_array_mono() const;
	Sqrat::Array get_fft_array_left() const;
	Sqrat::Array get_fft_array_right() const;

	static const int FFT_BANDS = 32;

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

	mutable bool m_vu_requested;
	mutable bool m_fft_requested;

	// Rolling buffer and resampling for FFT
	mutable std::vector<float> m_rolling_buffer_mono;
	mutable std::vector<float> m_rolling_buffer_left;
	mutable std::vector<float> m_rolling_buffer_right;
	mutable std::vector<float> m_window_lut;
	mutable size_t m_buffer_write_pos;
	mutable size_t m_buffer_samples_count;
	mutable double m_phase_accumulator;

	void calculate_fft_channel( const float *samples , unsigned int sample_count ,
	                            std::vector<float> &fft_bands , float sample_rate ) const;
	void update_fall() const;
	float convert_to_log_scale( float linear_value, float amplitude_linearity ) const;
	bool resample_and_buffer_audio( const float* input_frames, unsigned int input_frame_count,
	                                unsigned int frame_channel_count );
};

class FeAudioNormaliser : public FeAudioEffect
{
public:
	FeAudioNormaliser();
	~FeAudioNormaliser() override = default;

	bool process( const float *input_frames, float *output_frames,
	                   unsigned int frame_count, unsigned int channel_count,
	                   float media_sample_rate ) override;

	void update() override;
	void reset() override;

	void set_media_volume( float volume );

private:
	mutable float m_current_gain = 1.0f;
	mutable bool m_target_reached = false;
	mutable size_t m_startup_delay = 0;
	float m_media_volume = 1.0f;
};

template<typename T>
T* FeAudioEffectsManager::get_effect() const
{
	for ( const auto& effect : m_effects )
	{
		if ( T* casted = dynamic_cast<T*>( effect.get() ) )
			return casted;
	}
	return nullptr;
}

#endif // FE_AUDIO_FX_HPP
