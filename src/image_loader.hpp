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

#include <SFML/System/InputStream.hpp>
#include <SFML/System/Vector2.hpp>

#include <atomic>

class FeImageLoader;
class FeImageLoaderThread;
class FeImageLRUCache;
class FeImageLoaderImp;

class FeImageLoaderEntry
{
friend class FeImageLoader;
friend class FeImageLoaderThread;
friend class FeImageLRUCache;

public:
   ~FeImageLoaderEntry();

	int get_width();
	int get_height();

   unsigned char *get_data();

   size_t get_bytes();

private:

   sf::InputStream *m_stream;
   std::atomic<int> m_ref_count;
   std::atomic<int> m_width;
   std::atomic<int> m_height;
   unsigned char *m_data;
   std::atomic<bool> m_loaded;

   FeImageLoaderEntry( sf::InputStream *s );
   FeImageLoaderEntry( const FeImageLoaderEntry & );
   const FeImageLoaderEntry &operator=( const FeImageLoaderEntry & );

   void add_ref();
   bool dec_ref();
};

class FeImageLoader
{
public:
	~FeImageLoader();

	// Return true if image is already loaded and entry contains the pixel data, false if followup
	// check_loaded is required
	//
	// Caller becomes responsible for *e and must release it by calling release_entry() when done with it
	//
	bool load_image_from_file( const std::string &fn, FeImageLoaderEntry **e );

	// release *e. Caller must do this for any *e returned by load_image()
	void release_entry( FeImageLoaderEntry **e );

	// check if the image pointed to by *e has been loaded yet such that get_data() will return its pixel
	// data
	bool check_loaded( FeImageLoaderEntry *e );

	// Get a reference to the image loader
	static FeImageLoader &get_ref();

	// set the cache size for the image loader's cache of uncompressed images (in bytes)
	static void set_cache_size( size_t cache_size );

#ifndef NO_MOVIE
	// destroy vid (on our background thread which will wait on the video threads to stop)
	void reap_video( FeMedia *vid );
#endif

	//
	// Script interface functions
	//
	void cache_image( const char * );
	int cache_max();
	int cache_size();
	int cache_count();
	const char *cache_get_name_at( int );
	int cache_get_size_at( int );

	void set_background_loading( bool flag );
	bool get_background_loading();
	bool image_in_cache( const std::string &filename );
	void add_to_cache(const std::string &key, FeImageLoaderEntry *entry);
private:
	FeImageLoader();
	FeImageLoader( const FeImageLoader & );
	const FeImageLoader &operator=( const FeImageLoader & );

	bool internal_load_image( const std::string &fn, sf::InputStream *stream, FeImageLoaderEntry **e );

	FeImageLoaderImp *m_imp;
};

#endif
