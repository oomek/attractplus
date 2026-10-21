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

#include <list>
#include <map>
#include <queue>
#include <string>
#include <mutex>
#include <thread>
#include <vector>
#include <condition_variable>
#include <chrono>
#include <algorithm>
#include <SDL3/SDL_thread.h>

#include "fe_base.hpp" // logging
#include "fe_file.hpp"
#include "fe_util.hpp"

#include "media.hpp"

#include "image_loader.hpp"
#include "fe_present.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace
{
	// stb_image callbacks that operate on a FeInputStream
	int read( void* user, char* data, int size )
	{
		FeInputStream* stream = static_cast<FeInputStream*>( user );
		auto result = stream->read( data, size );
		return result.value_or(0);
	}

	void skip( void* user, int size )
	{
		FeInputStream* stream = static_cast<FeInputStream*>( user );
		auto position = stream->tell().value_or(0);
		stream->seek( position + static_cast<std::size_t>( size ));
	}

	int eof( void* user )
	{
		FeInputStream* stream = static_cast<FeInputStream*>( user );
		auto pos = stream->tell().value_or(0);
		auto size = stream->getSize().value_or(0);
		return ( pos >= size ) ? 1 : 0;
	}
	std::recursive_mutex g_mutex;

#ifdef FE_DEBUG
	std::atomic<int> g_entry_count( 0 );

	class EntryCountReporter
	{
	public:
		~EntryCountReporter()
		{
			FeLog() << "Image Loader Entry Count on exit: " << g_entry_count << std::endl;
		}
	};

	EntryCountReporter g_e_rep;
#endif
}

class FeImageLRUCache
{
public:
	typedef std::pair< std::string, std::shared_ptr<FeImageLoaderEntry> > kvp_t;
	typedef std::list<kvp_t>::iterator list_iterator_t;
	typedef std::map< std::string, list_iterator_t >::iterator map_iterator_t;

	FeImageLRUCache( size_t max_bytes )
		: m_max_bytes( max_bytes ),
		m_current_bytes( 0 )
	{
	}
	~FeImageLRUCache()
	{
		for ( auto &item : m_items )
			FeImageLoader::get_ref().release_entry( &item.second );
	}

	void put( const std::string &key, const std::shared_ptr<FeImageLoaderEntry> &value )
	{
		std::lock_guard<std::recursive_mutex> l( g_mutex );
		m_items.push_front( kvp_t( key, value ));

		m_items_map[key] = m_items.begin();

		m_current_bytes += value->get_bytes();
		prune();
	}

	bool get( const std::string &key, std::shared_ptr<FeImageLoaderEntry> *val )
	{
		std::lock_guard<std::recursive_mutex> l( g_mutex );
		map_iterator_t it = m_items_map.find( key );
		if ( it != m_items_map.end() )
		{
			// promote
			m_items.splice( m_items.begin(), m_items, it->second );
			*val = it->second->second;
			return true;
		}

		return false;
	}

	void resize( size_t new_size )
	{
		std::lock_guard<std::recursive_mutex> l( g_mutex );
		m_max_bytes = new_size;
		prune();
	}

	size_t get_max_size() { return m_max_bytes; };
	size_t get_size() { return m_current_bytes; };
	size_t get_count() { return m_items.size(); };

	const char *get_name_at( int pos )
	{
		std::list< kvp_t >::iterator it = m_items.begin();
		std::advance( it, pos );

		return it->first.c_str();
	}

	int get_size_at( int pos )
	{
		std::list< kvp_t >::iterator it = m_items.begin();
		std::advance( it, pos );

		return it->second->get_bytes();
	}

	bool contains( const std::string &key )
	{
		std::lock_guard<std::recursive_mutex> l( g_mutex );
		return m_items_map.find( key ) != m_items_map.end();
	}

private:
	void prune()
	{
		while ( !m_items.empty() && ( m_current_bytes > m_max_bytes ))
		{
			list_iterator_t last = m_items.end();
			--last;

			size_t lsize = last->second->get_bytes();

			if ( lsize >= m_current_bytes )
				m_current_bytes = 0;
			else
				m_current_bytes -= lsize;


			m_items_map.erase( last->first );
			FeImageLoader::get_ref().release_entry( &last->second );
			m_items.pop_back();
		}
	}

	std::list< kvp_t > m_items;
	std::map< std::string, list_iterator_t > m_items_map;
	size_t m_max_bytes;
	size_t m_current_bytes;
};



class FeImageLoaderThread
{
public:
	FeImageLoaderThread() : m_run( true ), m_next_image( m_images.end() )
	{
		const unsigned int count = std::min( 4u, std::max( 1u, std::thread::hardware_concurrency() / 4 ) );
		for ( unsigned int i = 0; i < count; ++i )
		{
			m_threads.emplace_back( &FeImageLoaderThread::run_thread, this );
			m_video_threads.emplace_back( &FeImageLoaderThread::reap_videos, this );
		}
	}

	~FeImageLoaderThread() { stop(); }

	void stop()
	{
		{
			std::lock_guard<std::recursive_mutex> lock( g_mutex );
			m_run = false;
		}
		m_ready.notify_all();
		m_video_ready.notify_all();
		for ( auto &thread : m_threads )
			if ( thread.joinable() ) thread.join();
		for ( auto &thread : m_video_threads )
			if ( thread.joinable() ) thread.join();
	}

	void request( const std::string &name, std::shared_ptr<FeImageLoaderEntry> *entry, bool prefetch=false )
	{
		{
			std::lock_guard<std::recursive_mutex> lock( g_mutex );
			*entry = m_images[name].lock();
			if ( !*entry )
			{
				entry->reset( new FeImageLoaderEntry() );
				m_images[name] = *entry;
				m_in.emplace( name, *entry );
			}
			(*entry)->m_prefetch |= prefetch;
		}
		m_ready.notify_one();
	}

	void reap_video( FeMedia *vid )
	{
		{
			std::lock_guard<std::recursive_mutex> lock( g_mutex );
			m_vid.push( vid );
		}
		m_video_ready.notify_one();
	}

	void reap( std::shared_ptr<void> object )
	{
		std::unique_lock<std::recursive_mutex> lock( g_mutex );
		if ( m_run )
		{
			m_retired.push( std::move( object ) );
			lock.unlock();
			m_ready.notify_one();
		}
	}

private:
	void run_thread()
	{
		SDL_SetCurrentThreadPriority( SDL_THREAD_PRIORITY_NORMAL );
		for (;;)
		{
			std::unique_lock<std::recursive_mutex> lock( g_mutex );
			m_ready.wait( lock, [this]{ return !m_run || !m_in.empty() || !m_retired.empty(); } );
			if ( !m_retired.empty() )
			{
				auto entry = std::move( m_retired.front() );
				m_retired.pop();
				lock.unlock();
				entry.reset();
				continue;
			}
			if ( m_in.empty() ) return;
			auto job = std::move( m_in.front() );
			m_in.pop();
			auto &entry = job.second;
			const bool wanted = m_run && ( entry->m_prefetch || entry.use_count() > 1 );
			if ( !wanted )
			{
				// A new request must not attach to a job we are discarding.
				m_images.erase( job.first );
				m_next_image = m_images.end();
			}
			lock.unlock();
			if ( wanted ) entry->load( job.first );
			lock.lock();
			if ( wanted && entry->m_data )
				FeImageLoader::get_ref().add_to_cache( job.first, entry );
			// Bound stale weak entries without a full registry scan on navigation.
			for ( int i = 0; i < 4 && !m_images.empty(); ++i )
			{
				if ( m_next_image == m_images.end() ) m_next_image = m_images.begin();
				auto current = m_next_image++;
				if ( current->second.expired() ) m_images.erase( current );
			}
			lock.unlock();
		}
	}

	void reap_videos()
	{
		for (;;)
		{
			std::unique_lock<std::recursive_mutex> lock( g_mutex );
			m_video_ready.wait( lock, [this]{ return !m_run || !m_vid.empty(); } );
			if ( m_vid.empty() ) return;
			FeMedia *vid = m_vid.front();
			m_vid.pop();
			lock.unlock();
			delete vid;
		}
	}

	bool m_run;
	std::vector<std::thread> m_threads;
	std::vector<std::thread> m_video_threads;
	std::condition_variable_any m_ready;
	std::condition_variable_any m_video_ready;
	std::map<std::string, std::weak_ptr<FeImageLoaderEntry>> m_images;
	std::map<std::string, std::weak_ptr<FeImageLoaderEntry>>::iterator m_next_image;
	std::queue<std::pair<std::string, std::shared_ptr<FeImageLoaderEntry>>> m_in;
	std::queue<std::shared_ptr<void>> m_retired;
	std::queue<FeMedia *> m_vid;
};

class FeImageLoaderImp
{
public:
	FeImageLoaderImp()
		: m_cache( NULL ),
		m_load_images_in_bg( false )
	{
	};

	~FeImageLoaderImp()
	{
		m_bg_loader.stop();
		if ( m_cache )
			delete m_cache;
	}

	FeImageLRUCache *m_cache;
	FeImageLoaderThread m_bg_loader;
	bool m_load_images_in_bg;
	std::mutex m_load_mutex;
	std::condition_variable m_load_ready;
	unsigned long long m_load_version = 0;
};

FeImageLoaderEntry::FeImageLoaderEntry()
	: m_prefetch( false ), m_width( 0 ), m_height( 0 ),
	m_data( NULL ), m_loaded( false )
{
	static std::atomic<unsigned long long> version( 1 );
	m_version = version.fetch_add( 1, std::memory_order_relaxed );
#ifdef FE_DEBUG
	g_entry_count++;
#endif
}

FeImageLoaderEntry::~FeImageLoaderEntry()
{
#ifdef FE_DEBUG
	g_entry_count--;
#endif
	stbi_image_free( m_data );
}

void FeImageLoaderEntry::load( const std::string &filename )
{
	FeFileInputStream stream( filename );
	int width = 0, height = 0, ignored = 0;
	if ( stream.isOpen() )
	{
		stbi_io_callbacks cb = { &read, &skip, &eof };
		m_data = stbi_load_from_callbacks( &cb, &stream, &width, &height, &ignored, STBI_rgb_alpha );
		if ( !m_data )
			FeLog() << "Error loading image: " << filename << " - " << stbi_failure_reason() << std::endl;
	}
	else
		FeLog() << "Failed to open file: " << filename << std::endl;
	m_width = m_data ? width : 0;
	m_height = m_data ? height : 0;
	m_texture = { m_data, static_cast<unsigned int>( m_width.load() ), static_cast<unsigned int>( m_height.load() ) };
	m_loaded.store( true, std::memory_order_release );
	FeImageLoader::get_ref().notify_loaded();
}

size_t FeImageLoaderEntry::get_bytes()
{
	return m_width * m_height * 4;
}

unsigned char *FeImageLoaderEntry::get_data()
{
	return m_data;
}

int FeImageLoaderEntry::get_width()
{
	return m_width;
}

int FeImageLoaderEntry::get_height()
{
	return m_height;
}

bool FeImageLoaderEntry::is_loaded() const
{
	return m_loaded.load( std::memory_order_acquire );
}

FeImageLoader::FeImageLoader()
	: m_imp( new FeImageLoaderImp() )
{
}

FeImageLoader::~FeImageLoader()
{
	if ( m_imp )
		delete m_imp;
}

bool FeImageLoader::load_image_from_file( const std::string &fn, std::shared_ptr<FeImageLoaderEntry> *e, bool background )
{
	std::string file = fn;
	std::replace( file.begin(), file.end(), '\\', '/' );
	{
		std::lock_guard<std::recursive_mutex> lock( g_mutex );
		if ( m_imp->m_cache && m_imp->m_cache->get( file, e ) )
			return (*e)->is_loaded();
	}
	if ( background || ( !FePresent::script_get_fep() && m_imp->m_load_images_in_bg ) )
	{
		m_imp->m_bg_loader.request( file, e );
		return false;
	}
	e->reset( new FeImageLoaderEntry() );
	(*e)->load( file );
	add_to_cache( file, *e );
	return true;
}

void FeImageLoader::release_entry( std::shared_ptr<FeImageLoaderEntry> *e )
{
	if ( e ) reap( std::move( *e ) );
}

void FeImageLoader::reap( std::shared_ptr<void> object )
{
	if ( object ) m_imp->m_bg_loader.reap( std::move( object ) );
}

void FeImageLoader::notify_loaded()
{
	{
		std::lock_guard<std::mutex> lock( m_imp->m_load_mutex );
		++m_imp->m_load_version;
	}
	m_imp->m_load_ready.notify_one();
}

unsigned long long FeImageLoader::load_version()
{
	std::lock_guard<std::mutex> lock( m_imp->m_load_mutex );
	return m_imp->m_load_version;
}

void FeImageLoader::wait_for_load( unsigned long long version )
{
	std::unique_lock<std::mutex> lock( m_imp->m_load_mutex );
	m_imp->m_load_ready.wait_for( lock, std::chrono::milliseconds( 1 ),
		[this, version] { return m_imp->m_load_version != version; } );
}

bool FeImageLoader::check_loaded( FeImageLoaderEntry *e )
{
	return e && e->is_loaded();
}

void FeImageLoader::reap_video( FeMedia *vid )
{
	vid->signal_stop( true );

	FeImageLoader &il = get_ref();
	if ( il.m_imp )
		il.m_imp->m_bg_loader.reap_video( vid );
}

FeImageLoader &FeImageLoader::get_ref()
{
	static FeImageLoader g_image_loader;

	return g_image_loader;
}

void FeImageLoader::set_cache_size( size_t s )
{
	FeImageLoader &il = get_ref();
	std::lock_guard<std::recursive_mutex> lock( g_mutex );

	if ( s == 0 )
	{
		if ( il.m_imp->m_cache )
		{
			delete il.m_imp->m_cache;
			il.m_imp->m_cache = NULL;
		}

		return;
	}

	// If cache size is > 0 clamp it to a minimum of 100MB as a workaround
	// for the situation when loaded image is larger than the cache size
	s = std::max( static_cast<size_t>( 100 * 1024 * 1024 ), s );

	if ( !il.m_imp->m_cache )
		il.m_imp->m_cache = new FeImageLRUCache( s );
	else
		il.m_imp->m_cache->resize( s );
}

void FeImageLoader::set_background_loading( bool flag )
{
	if ( auto *present = FePresent::script_get_fep() )
	{
		present->set_script_background_loading( flag );
		return;
	}
	FeImageLoader &il = get_ref();
	il.m_imp->m_load_images_in_bg = flag;

	FeDebug() << "Set background image loading: " << flag << std::endl;
}

bool FeImageLoader::get_background_loading()
{
	if ( auto *present = FePresent::script_get_fep() )
		return present->get_script_background_loading( present->get_script_id() );
	FeImageLoader &il = get_ref();
	return il.m_imp->m_load_images_in_bg;
}

//
//
void FeImageLoader::cache_image( const char *fn )
{
	if ( !m_imp || !m_imp->m_cache )
		return;

	// Queue the filename for background loading
	std::string file = fn;
	std::replace( file.begin(), file.end(), '\\', '/' );

	if ( image_in_cache( file ) ) return;
	std::shared_ptr<FeImageLoaderEntry> entry;
	m_imp->m_bg_loader.request( file, &entry, true );
	release_entry( &entry );
}

bool FeImageLoader::image_in_cache( const std::string &filename )
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	if ( !m_imp || !m_imp->m_cache )
		return false;

	return m_imp->m_cache->contains( filename );
}

void FeImageLoader::add_to_cache( const std::string &key, const std::shared_ptr<FeImageLoaderEntry> &entry )
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	if ( !m_imp || !m_imp->m_cache || !entry )
		return;

	if ( !entry->is_loaded() || !entry->m_data || entry->get_bytes() > m_imp->m_cache->get_max_size() )
		return;

	// Check if already in cache
	if ( m_imp->m_cache->contains( key ))
		return;

	m_imp->m_cache->put(key, entry);
}

int FeImageLoader::cache_max()
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	if ( !m_imp->m_cache )
		return 0;

	return m_imp->m_cache->get_max_size();
}

int FeImageLoader::cache_size()
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	if ( !m_imp->m_cache )
		return 0;

	return m_imp->m_cache->get_size();
}

int FeImageLoader::cache_count()
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	if ( !m_imp->m_cache )
		return 0;

	return m_imp->m_cache->get_count();
}

const char *FeImageLoader::cache_get_name_at( int pos )
{
	static thread_local std::string name;
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	name = ( m_imp->m_cache && pos >= 0 && pos < m_imp->m_cache->get_count() )
		? m_imp->m_cache->get_name_at( pos ) : "";
	return name.c_str();
}

int FeImageLoader::cache_get_size_at( int pos )
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	if ( !m_imp->m_cache || ( pos < 0 ) || ( pos >= (int)m_imp->m_cache->get_count() ))
		return 0;

	return m_imp->m_cache->get_size_at( pos );
}
