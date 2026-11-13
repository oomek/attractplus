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
#include <atomic>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include "fe_time.hpp"
#include "fe_audio_fx.hpp"
#include "fe_audio_sdl.hpp"

class FeMediaImp;
class FeAudioImp;
class FeVideoImp;

class FeMedia
{
friend class FeVideoImp;

public:
	enum Type
	{
		Audio=0x01,
		Video=0x02,
		AudioVideo=0x03
	};

	FeMedia( Type t, std::shared_ptr<FeAudioEffectsManager> effects_manager );
	~FeMedia();

	bool open( const std::string &archive,
			const std::string &name );
	void open_async( const std::string &archive, const std::string &name, bool new_effects=false );
	// Background open result: 0=pending, 1=opened, -1=failed.
	int get_open_result() const;
	unsigned long long get_id() const { return m_id; }
	std::shared_ptr<FeAudioEffectsManager> get_audio_effects() const { return m_audio_effects; }

	void play();
	void stop();
	// close_input retires the media; it must not be played again.
	void signal_stop( bool close_input=false );
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
	std::shared_ptr<const unsigned char> pin_video_frame( unsigned int &width, unsigned int &height, bool blocking=false );
	void clear_video_frame();
	unsigned long long get_video_frame_serial() const;

	FeTime get_video_time();
	FeTime get_duration() const;

	static bool is_supported_media_file( const std::string &filename );
	static void get_decoder_list( std::vector < std::string > &l );
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
	bool ensure_audio_stream();
	void clear_pending_audio();
	void append_pending_audio( const std::vector<float> &samples );
	bool queue_pending_audio( std::vector<float> &processed_samples );
	bool pump_audio();
	void reset_decoder();
	void media_thread();
	void open_thread( std::string archive, std::string name );
	void await_play_thread();

	const unsigned long long m_id;
	FeMediaImp *m_imp;
	FeAudioImp *m_audio;
	FeVideoImp *m_video;
	FeSdlAudioStream m_audio_stream;
	std::shared_ptr<FeAudioEffectsManager> m_audio_effects;
	float m_aspect_ratio;
	std::atomic<float> m_volume;
	std::atomic<float> m_pan;
	std::vector<float> m_audio_pending_samples;
	std::size_t m_audio_pending_offset;

	FeMedia( const FeMedia & );
	FeMedia &operator=( const FeMedia & );
};

#endif
