/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2020 Andrew Mickelson
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

#ifndef IMAGE_LOADER_HPP
#define IMAGE_LOADER_HPP

#include <atomic>
#include <memory>
#include <map>
#include <mutex>
#include <condition_variable>
#include <SDL3/SDL_gpu.h>
#include "fe_renderer.hpp"
#include "fe_input_stream.hpp"

class FeImageLoader;
class FeImageLoaderThread;
class FeImageCache;
class FeImageUploadThread;
class FeImageLoaderImp;
class FeImageMemory;
class FeMedia;

struct FeImageTexture
{
	enum Status { Pending, Ready, Failed };
	std::atomic<Status> status{ Pending };
	SDL_GPUTexture *texture = nullptr;
	SDL_GPUTransferBuffer *stream_buffer = nullptr;
	Uint32 stream_size = 0;
	SDL_GPUTextureCreateInfo info = {};
	std::size_t bytes = 0;
	unsigned long long generation = 0;
	unsigned long long content_version = 0;
	bool prefetch = false;
	bool from_texture = false;
	bool staged = false;
};

class FeImageLoaderEntry : public std::enable_shared_from_this<FeImageLoaderEntry>
{
friend class FeImageLoader;
friend class FeImageLoaderThread;
friend class FeImageCache;
friend class FeImageUploadThread;

public:
	~FeImageLoaderEntry();

	int get_width();
	int get_height();
	bool is_loaded() const;
	bool failed() const { return is_loaded() && m_width == 0; }
	bool has_pixels() const;
	bool copy_pixels( std::vector<unsigned char> &pixels ) const;
	unsigned long long get_version() const { return m_version; }
	size_t get_bytes();

private:

   void load( const std::string &filename );
   void release_pixels();
   std::shared_ptr<unsigned char> take_pixels();
   std::shared_ptr<unsigned char> pixels() const;
   std::size_t gpu_bytes() const;
   std::size_t trim_textures();
   std::string m_filename;
   std::string m_key;
   bool m_masked = false;
   bool m_legacy_cache = true;
   bool m_prefetch;
   bool m_mipmap = false;
   bool m_decode_pending = false;
   std::atomic<bool> m_canceled{ false };
   std::map<std::pair<bool, bool>, std::shared_ptr<FeImageTexture>> m_textures;
   mutable std::mutex m_pixel_mutex;
   std::condition_variable m_pixel_ready;
   unsigned long long m_version;
   std::atomic<int> m_width;
   std::atomic<int> m_height;
   std::shared_ptr<unsigned char> m_data;
   std::shared_ptr<FeImageMemory> m_memory;
   std::atomic<bool> m_loaded;

   FeImageLoaderEntry();
   FeImageLoaderEntry( const FeImageLoaderEntry & );
   const FeImageLoaderEntry &operator=( const FeImageLoaderEntry & );


};

class FeImageLoader
{
public:
	~FeImageLoader();

	// Return whether decoded metadata is available. GPU readiness is separate.
	//
	// Caller becomes responsible for *e and must release it by calling release_entry() when done with it
	//
	bool load_image_from_file( const std::string &fn, std::shared_ptr<FeImageLoaderEntry> *e, bool background=false, bool mipmap=false,
		std::shared_ptr<FeImageTexture> *texture=nullptr, bool legacy=true );
	void set_gpu_device( SDL_GPUDevice *device );
	void cancel_pending();
	void wait_for_texture( const std::shared_ptr<FeImageTexture> &texture );
	void image_loaded( const std::shared_ptr<FeImageLoaderEntry> & );
	std::shared_ptr<FeImageTexture> prepare_image( const std::shared_ptr<FeImageLoaderEntry> &, bool mipmap, bool cube_map, bool blocking=false );
	std::shared_ptr<FeImageTexture> prepare_video( std::shared_ptr<const unsigned char> pixels,
		unsigned int width, unsigned int height, bool mipmap, bool cube_map, unsigned long long version );
	std::shared_ptr<FeImageLoaderEntry> mask_image( const std::shared_ptr<FeImageLoaderEntry> &, bool mipmap );

	// release *e. Caller must do this for any *e returned by load_image()
	void release_entry( std::shared_ptr<FeImageLoaderEntry> *e );

	// Check whether decoding has completed, including a failed decode.
	bool check_loaded( FeImageLoaderEntry *e );

	// Get a reference to the image loader
	static FeImageLoader &get_ref();

	// Set the retention limit for uploaded image textures (in bytes).
	static void set_cache_size( size_t cache_size );
	void prune_cache( bool unused=false );

	// destroy vid (on our background thread which will wait on the video threads to stop)
	void reap_video( FeMedia *vid );
	void reap( std::shared_ptr<void> object );
	void notify_loaded();
	unsigned long long load_version();
	void wait_for_load( unsigned long long version );

	//
	// Script interface functions
	//
	void cache_image( const char * );
	void prefetch_image( const char *, bool legacy );
	long long cache_max();
	long long cache_size();
	int cache_count();
	const char *cache_get_name_at( int );
	long long cache_get_size_at( int );

	void set_background_loading( bool flag );
	bool get_background_loading();
	bool image_in_cache( const std::string &filename );
	void add_to_cache( const std::string &key, const std::shared_ptr<FeImageLoaderEntry> &entry );
private:
	bool get_cached_image( const std::string &, std::shared_ptr<FeImageLoaderEntry> *, bool legacy );
	FeImageLoader();
	FeImageLoader( const FeImageLoader & );
	const FeImageLoader &operator=( const FeImageLoader & );

	FeImageLoaderImp *m_imp;
};

#endif
