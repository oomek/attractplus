#include <chrono> // sleep_for

#include "fe_base.hpp" // logging

#include "async_loader.hpp"

FeAsyncLoaderEntry::FeAsyncLoaderEntry()
	: m_ref_count( 0 ),
	m_width( 0 ),
	m_height( 0 )
	// m_loaded( false )
{
}

FeAsyncLoaderEntry::~FeAsyncLoaderEntry()
{
}

size_t FeAsyncLoaderEntry::get_bytes()
{
	return m_width * m_height * 4;
}

sf::Texture &FeAsyncLoaderEntry::get_texture()
{
	return m_texture;
}

int FeAsyncLoaderEntry::get_width()
{
	return m_width;
}

int FeAsyncLoaderEntry::get_height()
{
	return m_height;
}

int FeAsyncLoaderEntry::get_ref()
{
	return m_ref_count;
}

void FeAsyncLoaderEntry::inc_ref()
{
	m_ref_count++;
}

bool FeAsyncLoaderEntry::dec_ref()
{
	if ( m_ref_count > 0 ) m_ref_count--;
	// FeLog() << m_ref_count << std::endl;
	return ( m_ref_count == 0 );
}

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
	// FeLog() << m_active.size() << std::endl;
	return m_active.size();
}

int FeAsyncLoader::get_queue_size()
{
	ulock_t lock( m_mutex );
	return m_in.size();
}

int FeAsyncLoader::get_cached_ref( int pos )
{
	ulock_t lock( m_mutex );
	list_iterator_t it = m_cached.begin();
	std::advance( it, pos );
	// FeLog() << it->second->get_ref() << std::endl;
	return it->second->get_ref();
}

int FeAsyncLoader::get_active_ref( int pos )
{
	ulock_t lock( m_mutex );
	list_iterator_t it = m_active.begin();
	std::advance( it, pos );
	// FeLog() << (*it).second->get_ref() << std::endl;
	return (*it).second->get_ref();
}

bool FeAsyncLoader::pending_updates()
{
	if ( m_pending )
	{
		ulock_t lock( m_mutex );
		if ( m_in.size() == 0 )
		{
			m_pending = false;
			return true;
		}
	}

	return false;
}

FeAsyncLoader::FeAsyncLoader()
	: m_running( true ),
	m_pending( false ),
	m_active{},
	m_cached{},
	m_map{},
	m_in{}
{
	dummy_texture.loadFromFile( "dummy_texture.png" );
	// FeLog() << "AL: Constructor" << std::endl;
	m_thread = std::thread( thread_loop, this );
}

FeAsyncLoader::~FeAsyncLoader()
{
	// FeLog() << "AL: Destructor" << std::endl;

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

	// FeLog() << "AL: Destructor END" << std::endl;
}

void FeAsyncLoader::thread_loop()
{
	sf::Context ctx;

	while ( m_running )
	{
		ulock_t lock( m_mutex );

		if ( m_in.size() == 0 )
		{
			m_cond.wait( lock );
		}
		else
		{
			std::string tmp_file = m_in.front();
			m_in.pop();
			lock.unlock();

			if ( m_map.find( tmp_file ) == m_map.end() )
			{
				// FeLog() << "AL: LOAD " << tmp_file << std::endl;

				// ctx.setActive(true);
				// FeAsyncLoaderEntry tmp_entry;
				FeAsyncLoaderEntry *tmp_entry_ptr( NULL );
				tmp_entry_ptr = new FeAsyncLoaderEntry();

				tmp_entry_ptr->get_texture().loadFromFile( tmp_file );
				// ctx.setActive(false);
				tmp_entry_ptr->m_width = tmp_entry_ptr->get_texture().getSize().x;
				tmp_entry_ptr->m_height = tmp_entry_ptr->get_texture().getSize().y;

				lock.lock();
				m_cached.push_front( kvp_t( tmp_file, tmp_entry_ptr ));
				m_map[ tmp_file ] = m_cached.begin();
				lock.unlock();

				// FeLog() << "AL: A:" << get_active_size() << " C:" << get_cached_size() << std::endl;
			}
			else
			{
				// FeLog() << "AL: FOUND " << tmp_file << std::endl;
			}
		}
	}

	// FeLog() << "AL: worker_END" << std::endl;
}

void FeAsyncLoader::add_texture( const std::string &file )
{
	if ( file.empty() ) return;

	m_pending = true;
	ulock_t lock( m_mutex );
	m_in.push( file );
	lock.unlock();

	m_cond.notify_one();
}

sf::Texture *FeAsyncLoader::get_dummy_texture()
{
	return &dummy_texture;
}

sf::Texture *FeAsyncLoader::get_texture( const std::string &file )
{
	ulock_t lock( m_mutex );
	map_iterator_t it = m_map.find( file );

	if ( it != m_map.end() )
	{
		if ( it->second->second->get_ref() > 0 )
		{
			// Promote in active list
			// FeLog() << "promot: " << file << " "  << m_active.size() << " " << m_cached.size() << std::endl;
			m_active.splice( m_active.begin(), m_active, it->second );
			// FeLog() << "        " << file << " "  << m_active.size() << " " << m_cached.size() << std::endl;
		}
		else
		{
			// Move from cached list to active list
			// FeLog() << "active: " << file << " "  << m_active.size() << " " << m_cached.size() << std::endl;
			m_active.splice( m_active.begin(), m_cached, it->second );
			// FeLog() << "      : " << file << " "  << m_active.size() << " " << m_cached.size() << std::endl;
		}

		it->second->second->inc_ref();

		return &it->second->second->get_texture();
	}
	else
		return &dummy_texture;
}

void FeAsyncLoader::release_texture( const std::string &file )
{
	if ( file.empty() ) return;

	ulock_t lock( m_mutex );
	map_iterator_t it = m_map.find( file );

	// FeLog() << "Ref: " << it->second->second->get_ref() << file << std::endl;
	if ( it != m_map.end() )
		if ( it->second->second->get_ref() > 0 )
		{
			// FeLog() << "cached: " << file << " " << m_active.size() << " " << m_cached.size() << std::endl;
			// Move to cache list if ref count is 0

			if ( it->second->second->dec_ref() )
			{
				// FeLog() << "moved2c: " << file << std::endl;
				m_cached.splice( m_cached.begin(), m_active, it->second );
			}
			// FeLog() << "      : " << file << " " << m_active.size() << " " << m_cached.size() << std::endl;
		}
}

// Release: D:/emu/mamedata/snaps/org/esha.png
// cached: D:/emu/mamedata/snaps/org/esha.png 0 5
//       : D:/emu/mamedata/snaps/org/esha.png 18446744073709551615 6