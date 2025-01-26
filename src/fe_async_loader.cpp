#include <iostream>
#include <chrono>
#include <unistd.h>

#include "fe_base.hpp" // logging
#include "fe_util.hpp"
#include "fe_async_loader.hpp"
#include "fe_present.hpp"

#include <fstream>
#include <iostream>
#include <cstdio>
#include <thread>

const EntryType FeAsyncLoaderEntryTexture::type = TextureType;
const EntryType FeAsyncLoaderEntryVideo::type = VideoType;
const EntryType FeAsyncLoaderEntryFont::type = FontType;
const EntryType FeAsyncLoaderEntrySoundBuffer::type = SoundBufferType;

size_t FeAsyncLoader::s_cache_max_bytes = 0;

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
FeAsyncLoader *FeAsyncLoader::m_loader = nullptr;

FeAsyncLoader &FeAsyncLoader::get_al()
{
	if ( !m_loader )
		m_loader = new FeAsyncLoader();

	return *m_loader;
}

void FeAsyncLoader::clear()
{
	if ( m_loader )
	{
		delete m_loader;
		m_loader = nullptr;
	}
}

size_t FeAsyncLoader::get_cache_current_bytes()
{
	return m_cache_current_bytes;
}

void FeAsyncLoader::inc_cache_bytes( size_t size )
{
	// ulock_t lock( m_loader_mutex );
	m_cache_current_bytes += size;
}

void FeAsyncLoader::dec_cache_bytes( size_t size )
{
	// ulock_t lock( m_loader_mutex );
	m_cache_current_bytes -= size;
}

int FeAsyncLoader::get_cached_size()
{
	return m_resources_cached.size();
}

int FeAsyncLoader::get_active_size()
{
	return m_resources_active.size();
}

int FeAsyncLoader::get_queue_size()
{
	return m_loader_queue_size.load();
}

int FeAsyncLoader::get_cleanup_size()
{
	return m_cleanup_size.load();
}

int FeAsyncLoader::get_cached_ref_count( int pos )
{
	ulock_t lock( m_loader_mutex );
	list_iterator_t it = m_resources_cached.begin();
	std::advance( it, pos );
	return it->second->get_ref();
}

int FeAsyncLoader::get_active_ref_count( int pos )
{
	ulock_t lock( m_loader_mutex );
	list_iterator_t it = m_resources_active.begin();
	std::advance( it, pos );
	return (*it).second->get_ref();
}

bool FeAsyncLoader::done()
{
	if ( !m_done )
	{
		ulock_t lock( m_loader_mutex );
		if ( m_loader_queue.empty() )
		{
			m_done = true;
			return true;
		}
	}

	return false;
}

void FeAsyncLoader::notify()
{
	m_loader_condition.notify_one();
}

FeAsyncLoader::FeAsyncLoader()
	: m_loader_running( true ),
	m_cleanup_running( true ),
	m_done( true ),
	m_resources_active{},
	m_resources_cached{},
	m_resources_cleanup{},
	m_resources_map{},
	m_loader_queue{},
	m_cache_current_bytes( 0 )
{
	// setLogHandler(nullptr);
	m_loader_thread = std::thread( &FeAsyncLoader::loader_thread_loop, this );
	m_cleanup_thread = std::thread( &FeAsyncLoader::cleanup_thread_loop, this );
}

FeAsyncLoader::~FeAsyncLoader()
{
	while ( !m_resources_active.empty() )
	{
		ulock_t lock( m_loader_mutex );
		list_iterator_t last = --m_resources_active.end();
		delete last->second;
		m_resources_active.pop_back();
	}

	while ( !m_resources_cached.empty() )
	{
		ulock_t lock( m_loader_mutex );
		list_iterator_t last = --m_resources_cached.end();
		delete last->second;
		m_resources_cached.pop_back();
	}

	while ( !m_resources_cleanup.empty() )
	{
		ulock_t lock( m_loader_mutex );
		list_iterator_t last = --m_resources_cleanup.end();
		delete last->second;
		m_resources_cleanup.pop_back();
	}

	m_resources_map.clear();

	// Stop and join the cleanup thread
	m_cleanup_running = false;
	m_cleanup_condition.notify_one();
	if ( m_cleanup_thread.joinable() )
		m_cleanup_thread.join();

	// Stop and join the loader thread
	m_loader_running = false;
	m_loader_condition.notify_one();
	if ( m_loader_thread.joinable() )
		m_loader_thread.join();
}

FeAsyncLoaderEntryVideo::FeAsyncLoaderEntryVideo()
	: m_texture_size( 0, 0 ),
	m_media( FeMedia::AudioVideo )
{
	// m_player.setDecoders( MediaType::Video, {"MFT:d3d=0:fallback=1:copy=2"} );
	// m_player.setDecoders( MediaType::Video, {"VAAPI:copy=0"} );
	// m_player.setDecoders( MediaType::Video, {"FFmpeg:hwaccel=cuda:hwcontext=cuda"} );
	// m_player.setDecoders( MediaType::Video, {"FFmpeg:threads=1:hwaccel=dxva2:sw_fallback=0"} );
};

void FeAsyncLoader::loader_thread_loop()
{
	sf::Context ctx;

	while ( m_loader_running )
	{
		ulock_t lock( m_loader_mutex );

		if ( m_loader_queue.empty() )
			m_loader_condition.wait( lock );
		else
		{
			lock.unlock();

			if ( m_resources_map.find( m_loader_queue.front().first ) == m_resources_map.end() )
				load_resource( m_loader_queue.front().first, m_loader_queue.front().second );

			lock.lock();
			m_loader_queue.pop();
			m_loader_queue_size.fetch_sub(1);
			lock.unlock();
		}
	}
}

void FeAsyncLoader::cleanup_thread_loop()
{
	sf::Context ctx;

	while ( m_cleanup_running )
	{
		ulock_t lock( m_cleanup_mutex );

		if ( m_resources_cleanup.empty() )
			m_cleanup_condition.wait( lock );
		else
		{
            // Create a copy of cleanup list to minimize mutex lock time
            list_t cleanup_copy;
            cleanup_copy.splice(cleanup_copy.begin(), m_resources_cleanup);
            m_resources_cleanup.clear();
            lock.unlock();

			for ( auto it = cleanup_copy.begin(); it != cleanup_copy.end(); ++it )
			{
				delete it->second;
				m_cleanup_size.fetch_sub(1);
			}
		}
	}
}

#define _CRT_DISABLE_PERFCRIT_LOCKS

bool FeAsyncLoader::load_resource( const std::string file, const EntryType type )
{
	if ( file.empty() ) return false;
	FeAsyncLoaderEntryBase *tmp_entry_ptr = nullptr;

	if ( type == TextureType )
		tmp_entry_ptr = static_cast<FeAsyncLoaderEntryBase*>( new FeAsyncLoaderEntryTexture() );
	else if ( type == VideoType )
		tmp_entry_ptr = static_cast<FeAsyncLoaderEntryBase*>( new FeAsyncLoaderEntryVideo() );
	else if ( type == FontType )
		tmp_entry_ptr = static_cast<FeAsyncLoaderEntryBase*>( new FeAsyncLoaderEntryFont() );
	else if ( type == SoundBufferType )
		tmp_entry_ptr = static_cast<FeAsyncLoaderEntryBase*>( new FeAsyncLoaderEntrySoundBuffer() );

	if ( tmp_entry_ptr->load_from_file( file ))
	{
		m_resources_cached.push_front( kvp_t( file, tmp_entry_ptr ) );
		m_resources_map[file] = m_resources_cached.begin();
		return true;
	}
	else
	{
		delete tmp_entry_ptr;
		return false;
	}
}

template <typename T>
bool FeAsyncLoader::add_resource( const std::string input_file, bool async )
{
	std::string file = clean_path( input_file );
	std::replace( file.begin(), file.end(), '\\', '/' );

	if ( is_relative_path( file ))
		file = FePresent::script_get_base_path() + file;

	ulock_t lock( m_loader_mutex );
	map_iterator_t it = m_resources_map.find( file );
	if ( it == m_resources_map.end() )
	{
		if ( !async )
		{
			if ( load_resource( file, T::type ))
				return true;
			else
				return false;
		}
		m_done = false;
		m_loader_queue.push( std::make_pair( file, T::type ));
		lock.unlock();
		m_loader_queue_size.fetch_add(1);
		m_loader_condition.notify_one();
	}
	return true;
}

bool FeAsyncLoader::add_to_cache( const std::string file )
{
	if ( tail_compare( file, FE_ART_EXTENSIONS ))
		return add_resource<FeAsyncLoaderEntryTexture>( file, true );
	else if ( tail_compare(file, FE_VID_EXTENSIONS ))
		return add_resource<FeAsyncLoaderEntryVideo>( file, true );
	else if ( tail_compare(file, FE_FONT_EXTENSIONS ))
		return add_resource<FeAsyncLoaderEntryFont>( file, true );
	else if ( tail_compare(file, FE_SOUND_EXTENSIONS ))
		return add_resource<FeAsyncLoaderEntrySoundBuffer>( file, true );
	else
		return false;
}

template <typename T>
T *FeAsyncLoader::get_resource( const std::string input_file )
{
	std::string file = input_file;
	std::replace( file.begin(), file.end(), '\\', '/' );

	map_iterator_t it = m_resources_map.find( file );

	if ( it != m_resources_map.end() )
	{
		ulock_t lock( m_loader_mutex );
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
	{
		bool added = false;
		if ( tail_compare( file, FE_ART_EXTENSIONS ))
			added = add_resource<FeAsyncLoaderEntryTexture>(file, false);
		else if ( tail_compare( file, FE_VID_EXTENSIONS ))
			added = add_resource<FeAsyncLoaderEntryVideo>(file, false);
		else if ( tail_compare(file, FE_SOUND_EXTENSIONS ))
			added = add_resource<FeAsyncLoaderEntrySoundBuffer>(file, false);
		else if ( tail_compare( file, FE_FONT_EXTENSIONS ))
			added = add_resource<FeAsyncLoaderEntryFont>(file, false);
		else
			FeLog() << "Error: unsupported file type" << std::endl;

		if (added)
			return get_resource<T>(file);
		else
			return nullptr;
	}
}

sf::Texture *FeAsyncLoader::get_resource_texture( const std::string file )
{
	return get_resource<sf::Texture>( file );
}

sf::Texture *FeAsyncLoader::get_resource_video( const std::string file )
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

	ulock_t lock( m_loader_mutex );
	map_iterator_t it = m_resources_map.find( file );

	if ( it != m_resources_map.end() )
		if ( it->second->second->get_ref() > 0 )
			if ( it->second->second->dec_ref() )
				// Move to cache list if ref count is 0
				m_resources_cached.splice( m_resources_cached.begin(), m_resources_active, it->second );
}

void FeAsyncLoader::stop_cached_videos()
{
	for ( auto it = m_resources_cached.begin(); it != m_resources_cached.end(); )
	{
		if ( dynamic_cast<FeAsyncLoaderEntryVideo*>( it->second ) != nullptr )
		{
			if ( FeMedia* player = get_player( it->first ))
			{
				if ( player->is_playing() )
					player->signal_stop();
			}
			m_resources_map.erase( it->first );
			m_resources_cleanup.splice( m_resources_cleanup.end(), m_resources_cached, it++ );
			m_cleanup_size.fetch_add(1);
			m_cleanup_condition.notify_one();
		}
		else if ( m_cache_current_bytes > s_cache_max_bytes && dynamic_cast<FeAsyncLoaderEntryTexture*>( it->second ) != nullptr )
		{
			FeAsyncLoader::get_al().dec_cache_bytes( it->second->get_bytes() );
			m_resources_map.erase( it->first );
			m_resources_cleanup.splice( m_resources_cleanup.end(), m_resources_cached, it++ );
			m_cleanup_size.fetch_add(1);
			m_cleanup_condition.notify_one();
		}
		else
			++it;
	}
}

bool FeAsyncLoaderEntryTexture::load_from_file( const std::string file )
{
	bool ret = m_texture.loadFromFile( file );
	FeAsyncLoader::get_al().inc_cache_bytes( m_texture.getSize().x * m_texture.getSize().y * 4 );
	return ret;
}

bool FeAsyncLoaderEntryVideo::load_from_file( const std::string file )
{
	bool ret = m_media.open( "", file, &m_texture );
	sf::Image img;
	img.create(	m_texture.getSize().x, m_texture.getSize().y, sf::Color( 0, 0, 0 ));
	m_texture.update( img );
	return ret;
}

FeMedia* FeAsyncLoader::get_player( const std::string& file )
{
	map_iterator_t it = m_resources_map.find ( file );
	if ( it != m_resources_map.end() )
	{
		FeAsyncLoaderEntryBase* entry = it->second->second;
		if ( entry != nullptr )
		{
			FeAsyncLoaderEntryVideo* video_entry = dynamic_cast<FeAsyncLoaderEntryVideo*>( entry );
			if ( video_entry )
				return video_entry->get_player();
		}
	}
	return nullptr;
}
