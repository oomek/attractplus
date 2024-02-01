// #include <chrono> // sleep_for
// auto start = std::chrono::steady_clock::now();
// auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::steady_clock::now() - start ).count();
// FeLog() << elapsed << std::endl;

#include <iostream>
#include <chrono>
#include <unistd.h>

#include "fe_base.hpp" // logging
#include "fe_async_loader.hpp"

#include <fstream>
#include <iostream>
#include <cstdio>
#include <thread>



// FeAsyncLoaderEntryBase
int FeAsyncLoaderEntryBase::get_ref()
{
	return m_ref_count;
}

void FeAsyncLoaderEntryBase::inc_ref()
{
	m_ref_count++;
}

bool FeAsyncLoaderEntryBase::dec_ref()
{
	if ( m_ref_count > 0 ) m_ref_count--;
	return ( m_ref_count == 0 );
}



// FeAsyncLoader
FeAsyncLoader &FeAsyncLoader::get_ref()
{
	static FeAsyncLoader loader;
	return loader;
}

int FeAsyncLoader::get_cached_size()
{
	ulock_t lock( m_mutex );
	return m_cached.size();
}

int FeAsyncLoader::get_active_size()
{
	ulock_t lock( m_mutex );
	return m_active.size();
}

int FeAsyncLoader::get_queue_size()
{
	ulock_t lock( m_mutex );
	return m_in.size();
}

int FeAsyncLoader::get_cached_ref_count( int pos )
{
	ulock_t lock( m_mutex );
	list_iterator_t it = m_cached.begin();
	std::advance( it, pos );
	return it->second->get_ref();
}

int FeAsyncLoader::get_active_ref_count( int pos )
{
	ulock_t lock( m_mutex );
	list_iterator_t it = m_active.begin();
	std::advance( it, pos );
	return (*it).second->get_ref();
}

bool FeAsyncLoader::done()
{
	if ( !m_done )
	{
		ulock_t lock( m_mutex );
		if ( m_in.size() == 0 )
		{
			m_done = true;
			return true;
		}
	}

	return false;
}

void FeAsyncLoader::notify()
{
	m_cond.notify_one();
}

FeAsyncLoader::FeAsyncLoader()
	: m_running( true ),
	m_done( true ),
	m_active{},
	m_cached{},
	m_map{},
	m_in{}
{
	dummy_texture.loadFromFile( "dummy_texture.png" );
	m_thread = std::thread( &FeAsyncLoader::thread_loop, this );
}

FeAsyncLoader::~FeAsyncLoader()
{
	m_running = false;
	m_cond.notify_one();
	m_thread.join();

	while ( !m_active.empty() )
	{
		ulock_t lock( m_mutex );
		list_iterator_t last = --m_active.end();
		delete last->second;
		m_active.pop_back();
	}

	while ( !m_cached.empty() )
	{
		ulock_t lock( m_mutex );
		list_iterator_t last = --m_cached.end();
		delete last->second;
		m_cached.pop_back();
	}

	m_map.clear();

}

void FeAsyncLoader::thread_loop()
{
	sf::Context ctx;

	while ( m_running )
	{
		ulock_t lock( m_mutex );

		if ( m_in.size() == 0 )
			m_cond.wait( lock );
		else
		{
			lock.unlock();

			if ( m_map.find( m_in.front().first ) == m_map.end() )
				load_resource( m_in.front().first, m_in.front().second );

			lock.lock();
			m_in.pop();
			lock.unlock();
		}
	}
}

#define _CRT_DISABLE_PERFCRIT_LOCKS

void FeAsyncLoader::load_resource( const std::string file, const EntryType type )
{
	FeAsyncLoaderEntryBase *tmp_entry_ptr = nullptr;

	if ( type == TextureType )
		tmp_entry_ptr = new FeAsyncLoaderEntryTexture();
	else if ( type == FontType )
		tmp_entry_ptr = new FeAsyncLoaderEntryFont();
	else if ( type == SoundBufferType )
		tmp_entry_ptr = new FeAsyncLoaderEntrySoundBuffer();

	if ( tmp_entry_ptr->load_from_file( file ))
	{
		m_cached.push_front( kvp_t( file, tmp_entry_ptr ) );
		m_map[file] = m_cached.begin();
	}
	else
		delete tmp_entry_ptr;
}

sf::Texture *FeAsyncLoader::get_dummy_texture()
{
	return &dummy_texture;
}

void FeAsyncLoader::release_texture( const std::string file )
{
	if ( file.empty() ) return;

	ulock_t lock( m_mutex );
	map_iterator_t it = m_map.find( file );

	if ( it != m_map.end() )
		if ( it->second->second->get_ref() > 0 )
		{
			// Move to cache list if ref count is 0
			if ( it->second->second->dec_ref() )
				m_cached.splice( m_cached.begin(), m_active, it->second );
		}
}
