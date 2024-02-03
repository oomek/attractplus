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

// Destructor needs to be in the same compilation unit
// so tmp_entry_ptr static_cast to derived will work
FeAsyncLoaderEntryBase::~FeAsyncLoaderEntryBase() {};

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
FeAsyncLoader &FeAsyncLoader::get_al()
{
	static FeAsyncLoader loader;
	return loader;
}

int FeAsyncLoader::get_cached_size()
{
	ulock_t lock( m_mutex );
	return m_resources_cached.size();
}

int FeAsyncLoader::get_active_size()
{
	ulock_t lock( m_mutex );
	return m_resources_active.size();
}

int FeAsyncLoader::get_queue_size()
{
	ulock_t lock( m_mutex );
	return m_queue.size();
}

int FeAsyncLoader::get_cached_ref_count( int pos )
{
	ulock_t lock( m_mutex );
	list_iterator_t it = m_resources_cached.begin();
	std::advance( it, pos );
	return it->second->get_ref();
}

int FeAsyncLoader::get_active_ref_count( int pos )
{
	ulock_t lock( m_mutex );
	list_iterator_t it = m_resources_active.begin();
	std::advance( it, pos );
	return (*it).second->get_ref();
}

bool FeAsyncLoader::done()
{
	if ( !m_done )
	{
		ulock_t lock( m_mutex );
		if ( m_queue.size() == 0 )
		{
			m_done = true;
			return true;
		}
	}

	return false;
}

void FeAsyncLoader::notify()
{
	m_condition.notify_one();
}

FeAsyncLoader::FeAsyncLoader()
	: m_running( true ),
	m_done( true ),
	m_resources_active{},
	m_resources_cached{},
	m_resources_map{},
	m_queue{}
{
	m_thread = std::thread( &FeAsyncLoader::thread_loop, this );
}

FeAsyncLoader::~FeAsyncLoader()
{
	m_running = false;
	m_condition.notify_one();
	m_thread.join();

	while ( !m_resources_active.empty() )
	{
		ulock_t lock( m_mutex );
		list_iterator_t last = --m_resources_active.end();
		delete last->second;
		m_resources_active.pop_back();
	}

	while ( !m_resources_cached.empty() )
	{
		ulock_t lock( m_mutex );
		list_iterator_t last = --m_resources_cached.end();
		delete last->second;
		m_resources_cached.pop_back();
	}

	m_resources_map.clear();

}

void FeAsyncLoader::thread_loop()
{
	sf::Context ctx;

	while ( m_running )
	{
		ulock_t lock( m_mutex );

		if ( m_queue.size() == 0 )
			m_condition.wait( lock );
		else
		{
			lock.unlock();

			if ( m_resources_map.find( m_queue.front().first ) == m_resources_map.end() )
				load_resource( m_queue.front().first, m_queue.front().second );

			lock.lock();
			m_queue.pop();
			lock.unlock();
		}
	}
}

#define _CRT_DISABLE_PERFCRIT_LOCKS

void FeAsyncLoader::load_resource( const std::string file, const EntryType type )
{
	FeAsyncLoaderEntryBase *tmp_entry_ptr = nullptr;

	if ( type == TextureType )
		tmp_entry_ptr = static_cast<FeAsyncLoaderEntryBase*>( new FeAsyncLoaderEntryTexture() );
	else if ( type == FontType )
		tmp_entry_ptr = static_cast<FeAsyncLoaderEntryBase*>( new FeAsyncLoaderEntryFont() );
	else if ( type == SoundBufferType )
		tmp_entry_ptr = static_cast<FeAsyncLoaderEntryBase*>( new FeAsyncLoaderEntrySoundBuffer() );

	if ( tmp_entry_ptr->load_from_file( file ))
	{
		m_resources_cached.push_front( kvp_t( file, tmp_entry_ptr ) );
		m_resources_map[file] = m_resources_cached.begin();
	}
	else
		delete tmp_entry_ptr;
}

template <typename T>
void FeAsyncLoader::add_resource( const std::string file, bool async )
{
	if ( file.empty() ) return;

	ulock_t lock( m_mutex );
	map_iterator_t it = m_resources_map.find( file );
	if ( it == m_resources_map.end() )
	{
		if ( !async )
		{
			load_resource( file, T::type );
			return;
		}
	}

	m_done = false;
	m_queue.push( std::make_pair( file, T::type ));
	lock.unlock();
	m_condition.notify_one();
	return;
}

void FeAsyncLoader::add_resource_texture( const std::string file, bool async )
{
    add_resource<FeAsyncLoaderEntryTexture>( file, async );
}

void FeAsyncLoader::add_resource_font( const std::string file, bool async )
{
    add_resource<FeAsyncLoaderEntryFont>( file, async );
}

void FeAsyncLoader::add_resource_sound( const std::string file, bool async )
{
    add_resource<FeAsyncLoaderEntrySoundBuffer>( file, async );
}

template <typename T>
T *FeAsyncLoader::get_resource( const std::string file )
{
	ulock_t lock( m_mutex );
	map_iterator_t it = m_resources_map.find( file );

	if ( it != m_resources_map.end() )
	{
		if ( it->second->second->get_ref() > 0 )
			// Promote in active list
			m_resources_active.splice( m_resources_active.begin(), m_resources_active, it->second );
		else
			// Move from cached list to active list
			m_resources_active.splice( m_resources_active.begin(), m_resources_cached, it->second );

		it->second->second->inc_ref();
		return static_cast<T*>( it->second->second->get_resource_pointer() );
	}
	else
		return nullptr;
}

sf::Texture *FeAsyncLoader::get_resource_texture( const std::string file )
{
	return get_resource<sf::Texture>( file );
}

sf::Font *FeAsyncLoader::get_resource_font( const std::string file )
{
	return get_resource<sf::Font>( file );
}

sf::SoundBuffer *FeAsyncLoader::get_resource_sound( const std::string file )
{
	return get_resource<sf::SoundBuffer>( file );
}

void FeAsyncLoader::release_resource( const std::string file )
{
	if ( file.empty() ) return;

	ulock_t lock( m_mutex );
	map_iterator_t it = m_resources_map.find( file );

	if ( it != m_resources_map.end() )
		if ( it->second->second->get_ref() > 0 )
			if ( it->second->second->dec_ref() )
				// Move to cache list if ref count is 0
				m_resources_cached.splice( m_resources_cached.begin(), m_resources_active, it->second );
}
