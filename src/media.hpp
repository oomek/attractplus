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

#ifndef MEDIA_HPP
#define MEDIA_HPP

#include <cstddef>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <functional>
#include <memory>
#include "fe_time.hpp"
#include "fe_audio_fx.hpp"

class FeMediaImp;
class FeAudioImp;
class FeVideoImp;
class FeMediaSoundStream;

class FeMedia
{
friend class FeVideoImp;
friend class FeMediaSoundStream;

public:
	enum Type
	{
		Audio=0x01,
		Video=0x02,
		AudioVideo=0x03
	};

	FeMedia( Type t, FeAudioEffectsManager &effects_manager );
	~FeMedia();

	bool open( const std::string &archive,
			const std::string &name );

	void play();
	void stop();
	void signal_stop();
	void close();

	// tick() needs to be called regularly on video media to update the display
	// texture. Returns true if display refresh required.  false if no update
	//
	bool tick();

	float getVolume() const;
	void setVolume( float volume );

	float getPan() const;
	void setPan( float pan );

	bool is_playing();
	bool is_multiframe() const;
	float get_aspect_ratio() const;
	bool get_video_frame_dimensions( unsigned int &width, unsigned int &height );
	bool copy_video_frame_rgba( std::vector<unsigned char> &pixels, unsigned int &width, unsigned int &height );
	bool copy_video_frame_rgba_to( void *pixels, std::size_t pixel_count, unsigned int &width, unsigned int &height );
	unsigned long long get_video_frame_serial() const;

	FeTime get_video_time();
	FeTime get_duration() const;

	// return true if the given filename is a media file that can be opened
	//	by FeMedia
	//
	static bool is_supported_media_file( const std::string &filename );

	//
	static void get_decoder_list( std::vector < std::string > &l );

	// get/set video decoder to be used (if available)
	//
	static std::string get_current_decoder();
	static void set_current_decoder( const std::string & );

	float get_vu_mono();
	float get_vu_left();
	float get_vu_right();

	const std::vector<float> *get_fft_mono_ptr();
	const std::vector<float> *get_fft_left_ptr();
	const std::vector<float> *get_fft_right_ptr();

	void set_fft_bands( int count );
	int get_fft_bands() const;

	FeAudioVisualiser* get_audio_visualiser() const;

private:
	bool read_packet();
	bool end_of_file();

	FeMediaImp *m_imp;
	FeAudioImp *m_audio;
	FeVideoImp *m_video;
	std::unique_ptr<FeMediaSoundStream> m_stream;
	std::atomic<bool> m_alive{ true };
	std::atomic<bool> m_ready{ false };
	std::mutex m_callback_mutex;
	std::mutex m_closing_mutex;

	FeAudioEffectsManager &m_audio_effects;
	void setup_effect_processor();

	FeMedia( const FeMedia & );
	FeMedia &operator=( const FeMedia & );
	float m_aspect_ratio;
};

#endif
