/*
 *
 *  Attract-Mode Plus frontend
 *  Copyright (C) 2026 Radek Dutkiewicz
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

#ifndef FE_AUDIO_SDL_HPP
#define FE_AUDIO_SDL_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "fe_types.hpp"

class FeAudioFileDecoder;

struct FeAudioSampleBuffer
{
	std::vector<float> samples;
	int sample_rate = 0;
	int channel_count = 0;

	bool empty() const { return samples.empty(); }
	std::uint64_t frame_count() const
	{
		return ( channel_count > 0 ) ? ( samples.size() / static_cast<std::size_t>( channel_count ) ) : 0;
	}
	int duration_ms() const;
};

class FeSdlAudioBackend
{
public:
	static FeSdlAudioBackend &get();

	bool ensure_audio();
	int sample_rate() const { return m_mix_spec.freq; }
	int channel_count() const { return m_mix_spec.channels; }
	const SDL_AudioSpec &mix_spec() const { return m_mix_spec; }
	std::uint64_t generation() const { return m_generation.load( std::memory_order_acquire ); }
	void note_system_resume();

private:
	FeSdlAudioBackend();

	SDL_AudioSpec m_mix_spec;
	std::atomic<std::uint64_t> m_generation;
};

class FeSdlAudioStream
{
public:
	FeSdlAudioStream();
	~FeSdlAudioStream();

	FeSdlAudioStream( const FeSdlAudioStream & ) = delete;
	FeSdlAudioStream &operator=( const FeSdlAudioStream & ) = delete;

	bool ensure_open();
	void destroy();
	void clear();

	bool is_open() const { return m_stream != nullptr; }
	bool is_stale() const;

	bool queue_frames( const float *frames, int frame_count );
	int queued_frames() const;

	bool set_gain( float gain );
	bool set_pitch( float pitch );

private:
	SDL_AudioStream *m_stream;
	std::uint64_t m_generation;
};

bool fe_decode_audio_file_to_buffer( const std::string &filename, FeAudioSampleBuffer &buffer );
void fe_audio_apply_balance( std::vector<float> &samples, float pan, bool spatialization_enabled, const Vec3f &position );
int fe_audio_frames_to_ms( std::uint64_t frame_count );
std::uint64_t fe_audio_ms_to_frames( int milliseconds );

class FeAudioFileDecoder
{
public:
	FeAudioFileDecoder();
	~FeAudioFileDecoder();

	FeAudioFileDecoder( const FeAudioFileDecoder & ) = delete;
	FeAudioFileDecoder &operator=( const FeAudioFileDecoder & ) = delete;

	bool open( const std::string &filename );
	void close();
	bool seek_ms( int milliseconds );
	bool decode_frames( std::vector<float> &out_samples, std::size_t max_frames, bool &reached_end );
	int duration_ms() const { return m_duration_ms; }

private:
	struct Impl;

	std::unique_ptr<Impl> m_impl;
	int m_duration_ms;
};

#endif