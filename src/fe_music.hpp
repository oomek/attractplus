/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2022 Andrew Mickelson
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

#ifndef FE_MUSIC_HPP
#define FE_MUSIC_HPP

#include <SFML/Audio.hpp>
#include <cstring>
#include <vector>
#include <complex>
#include <sqrat.h>

class FeMusic
{
private:
	FeMusic( const FeMusic & );
	FeMusic &operator=( const FeMusic & );

	sf::Music m_music;

	std::string m_file_name;
	bool m_play_state;
	float m_volume;	mutable float m_vu;
	mutable float m_vu_left;
	mutable float m_vu_right;
	mutable float m_vu_display;
	mutable float m_vu_left_display;
	mutable float m_vu_right_display;
	mutable std::vector<float> m_fft_bands;
	mutable std::vector<float> m_fft_display_values;
	mutable std::vector<float> m_fft_bands_left;
	mutable std::vector<float> m_fft_display_values_left;
	mutable std::vector<float> m_fft_bands_right;
	mutable std::vector<float> m_fft_display_values_right;
	mutable std::vector<std::complex<float>> m_fft_buffer;
	mutable float m_last_frame_time;
	mutable sf::Clock m_system_clock;
	mutable float m_fft_linearity; // 0.0 = linear, 1.0 = logarithmic
	static const int FFT_BANDS = 32;
	static const int FFT_SIZE = 16384;

public:
	FeMusic( bool loop=false );
	~FeMusic();

	void load( const std::string &fn );

    void set_file_name( const char * );
	const char *get_file_name();

	float get_volume();
	void set_volume( float );

	bool get_playing();
	void set_playing( bool );

    float get_pitch();
	void set_pitch( float );

	bool get_loop();
	void set_loop( bool );

	float get_x();
	float get_y();
	float get_z();
	void set_x( float );
	void set_y( float );
	void set_z( float );

	int get_duration();
	int get_time();
	const char *get_metadata( const char * );
	float get_vu();
	float get_vu_left();
	float get_vu_right();
	const std::vector<float>& get_fft();
	const std::vector<float>& get_fft_left();
	const std::vector<float>& get_fft_right();
	float get_fft_linearity();
	void set_fft_linearity( float linearity );

	Sqrat::Array get_fft_array();
	Sqrat::Array get_fft_left_array();
	Sqrat::Array get_fft_right_array();

private:
	void calculate_fft(const float* samples, unsigned int sampleCount) const;
	void calculate_fft_stereo(const float* samples, unsigned int sampleCount, unsigned int frameChannelCount) const;
	void calculate_fft_channel(const float* samples, unsigned int sampleCount, std::vector<float>& fft_bands) const;
	void update_bands_fall() const;
};

#endif // FE_MUSIC_HPP