// #include <chrono> // sleep_for
#include <iostream>
#include <chrono>
#include <unistd.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include "stb_image.h"

#include "fe_base.hpp" // logging
#include "async_loader.hpp"

#include <fstream>
#include <iostream>
#include <cstdio>
#include <thread>

#include<SFML/System/FileInputStream.hpp>

FeAsyncLoaderEntry::FeAsyncLoaderEntry()
	: m_ref_count( 0 ),
	m_texture_size( 0, 0 )
	// m_loaded( false )
{
}

FeAsyncLoaderEntry::~FeAsyncLoaderEntry()
{
}

size_t FeAsyncLoaderEntry::get_bytes()
{
	return m_texture_size.x * m_texture_size.y * 4;
}

sf::Texture &FeAsyncLoaderEntry::get_texture()
{
	return m_texture;
}

sf::Vector2u FeAsyncLoaderEntry::get_texture_size()
{
	return m_texture_size;
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

FeAsyncLoader::FeAsyncLoader()
	: m_running( true ),
	m_done( true ),
	m_active{},
	m_cached{},
	m_map{},
	m_in{}
{
	dummy_texture.loadFromFile( "dummy_texture.png" );
	// FeLog() << "AL: Constructor" << std::endl;
	m_thread = std::thread( &thread_loop, this );
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
	std::ios::sync_with_stdio(false);
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
			lock.unlock();

			if ( m_map.find( tmp_file ) == m_map.end() )
			{
				// FeLog() << "AL: LOAD " << tmp_file << std::endl;

				// FeAsyncLoaderEntry tmp_entry;
				FeAsyncLoaderEntry *tmp_entry_ptr( NULL );
				tmp_entry_ptr = new FeAsyncLoaderEntry();
				// sf::Image img;
// auto start = std::chrono::steady_clock::now();
				// img.loadFromFile( tmp_file );
				// ctx.setActive(true);
				// tmp_entry_ptr->get_texture().loadFromImage( img );
				tmp_entry_ptr->get_texture().loadFromFile( tmp_file );
				// ctx.setActive(false);
// auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::steady_clock::now() - start ).count();
// FeLog() << elapsed << std::endl;

				tmp_entry_ptr->m_texture_size = tmp_entry_ptr->get_texture().getSize();

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
			lock.lock();
			m_in.pop();
			lock.unlock();
		}
	}

	// FeLog() << "AL: worker_END" << std::endl;
}

#define _CRT_DISABLE_PERFCRIT_LOCKS

sf::Vector2u FeAsyncLoader::add_texture( const std::string file )
{
	// std::ios::sync_with_stdio(false);
	if ( file.empty() ) return sf::Vector2u( 0, 0 );

	int w = 128;
	int h = 128;
	int comp;

	ulock_t lock( m_mutex );
	map_iterator_t it = m_map.find( file );
	if ( it == m_map.end() )
	{
		stbi_info( file.c_str(), &w, &h, &comp );
		// FeLog() << "S:" << w << "x" << h << std::endl;
	}
	else
	{
		w = it->second->second->m_texture_size.x;
		h = it->second->second->m_texture_size.y;
		// FeLog() << "C:" << w << "x" << h << std::endl;
	}


	// sf::Image img;

	// auto start = std::chrono::steady_clock::now();
	// stbi_info( file.c_str(), &w, &h, &comp );
	// auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::steady_clock::now() - start ).count();
	// FeLog() << w << "x"<< h << " " << elapsed << std::endl;

	// img.loadFromFile( file );
	// w = img.getSize().x;
	// h = img.getSize().y;



	// auto start = std::chrono::steady_clock::now();

	// std::ifstream in( file.c_str(), std::ifstream::binary );
	// if ( in.is_open() )
	// {
	// 	in.seekg(16);
	// 	in.read((char *)&w, 4);
	// 	in.read((char *)&h, 4);
	// 	w = ntohl(w);
	// 	h = ntohl(h);
	// }

	// auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::steady_clock::now() - start ).count();
	// FeLog() << w << "x"<< h << " " << elapsed << std::endl;



	// auto start = std::chrono::steady_clock::now();

	// unsigned char buf[8];
	// sf::FileInputStream s;
	// if ( s.open( file ))
	// {
	// 	s.seek(16);
	// 	s.read(reinterpret_cast<char*>(&buf), 8);
	// 	w = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + (buf[3] << 0);
	// 	h = (buf[4] << 24) + (buf[5] << 16) + (buf[6] << 8) + (buf[7] << 0);
	// }

	// auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::steady_clock::now() - start ).count();
	// FeLog() << w << "x"<< h << " " << elapsed << std::endl;





	// auto start = std::chrono::steady_clock::now();

	// unsigned char buf[8];
	// FILE *s;
	// s = fopen(file.c_str(), "r");
	// if ( s )
	// {
	// 	fseek( s, 16, 0 );
	// 	for ( int i = 0; i < 8; i++ ) buf[i] = getc(s);
	// 	fclose(s);
	// }

	// w = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + (buf[3] << 0);
	// h = (buf[4] << 24) + (buf[5] << 16) + (buf[6] << 8) + (buf[7] << 0);

	// auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::steady_clock::now() - start ).count();
	// FeLog() << w << "x"<< h << " " << elapsed << std::endl;




	// auto start = std::chrono::steady_clock::now();
	// {
	// 	unsigned char buf[8];
	// 	sf::FileInputStream s;
	// 	s.open(file);
	// 	s.seek(16);
	// 	s.read(reinterpret_cast<char*>(&buf), 8);
	// 	w = (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + (buf[3] << 0);
	// 	h = (buf[4] << 24) + (buf[5] << 16) + (buf[6] << 8) + (buf[7] << 0);
	// }
	// auto end = std::chrono::steady_clock::now();

	// if ( elapsed > 100 ) FeLog() << w << " " << h << " " << elapsed << std::endl;


	// return;

	// ulock_t lock( m_mutex );
	m_done = false;
	m_in.push( file );
	lock.unlock();

	m_cond.notify_one();

	return sf::Vector2u( w, h );
}

sf::Texture *FeAsyncLoader::get_dummy_texture()
{
	return &dummy_texture;
}

sf::Texture *FeAsyncLoader::get_texture( const std::string file )
{
	// FeLog() << "get_texture " << file << std::endl;
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

void FeAsyncLoader::release_texture( const std::string file )
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