#include "fe_resource_cache.hpp"
#include "fe_base.hpp"
#include "image_loader.hpp"
#include "fe_present.hpp"
#include "fe_util.hpp"

#include <condition_variable>
#include <atomic>
#include <deque>
#include <list>
#include <map>
#include <mutex>
#include <thread>
#include <utility>

const FeFont &FeFontContainer::get_font() const
{
	if ( !m_cache_key.empty() ) FeResourceCache::get().update( FeResourceCache::Font, m_cache_key, this,
		m_font.memory_bytes() + m_font_binary_data.capacity() + sizeof( FeFontContainer ) );
	return m_font;
}

std::shared_ptr<FeFontContainer> FeFontContainer::acquire( const std::string &filename, bool wait )
{
	auto &cache = FeResourceCache::get();
	const std::string key = filename + "|" + std::to_string( get_file_mtime( filename ) );
	auto font = cache.find<FeFontContainer>( FeResourceCache::Font, key );
	if ( !font ) font = std::static_pointer_cast<FeFontContainer>( cache.acquire(
		FeResourceCache::Font, key, std::make_shared<FeFontContainer>() ) );
	if ( !font->m_started.exchange( true ) ) cache.submit( [font, filename, key]
	{
		font->m_cache_key = key;
		try { font->set_font( filename ); }
		catch ( const std::exception &error ) { FeLog() << "Error preparing font: " << filename << ": " << error.what() << std::endl; }
		{
			std::lock_guard<std::mutex> lock( font->m_load_mutex );
			font->m_loaded.store( true, std::memory_order_release );
		}
		font->m_load_ready.notify_all();
		FeResourceCache::get().update( FeResourceCache::Font, key, font.get(), sizeof( FeFontContainer ) );
	} );
	if ( wait )
	{
		std::unique_lock<std::mutex> lock( font->m_load_mutex );
		font->m_load_ready.wait( lock, [&]{ return font->m_loaded.load( std::memory_order_acquire ); } );
	}
	return font;
}

void FeAudioResource::wait()
{
	std::unique_lock<std::mutex> lock( mutex );
	loaded.wait( lock, [this]{ return ready.load( std::memory_order_acquire ); } );
}

std::shared_ptr<FeAudioResource> fe_get_audio_resource( const std::string &filename )
{
	auto &cache = FeResourceCache::get();
	std::string key = filename + "|" + std::to_string( get_file_mtime( filename ) );
	auto resource = cache.find<FeAudioResource>( FeResourceCache::Audio, key );
	if ( resource && resource->ready.load( std::memory_order_acquire ) && resource->failed )
	{
		resource = std::make_shared<FeAudioResource>();
		cache.put( FeResourceCache::Audio, key, resource, 0 );
	}
	if ( !resource ) resource = std::static_pointer_cast<FeAudioResource>(
		cache.acquire( FeResourceCache::Audio, key, std::make_shared<FeAudioResource>() ) );
	if ( !resource->started.exchange( true ) ) cache.submit( [resource, filename, key]
	{
		try
		{
			resource->failed = !fe_decode_audio_file_to_buffer( filename, resource->buffer );
		}
		catch ( const std::exception &error )
		{
			resource->failed = true;
			FeLog() << "Error preparing audio file: " << filename << ": " << error.what() << std::endl;
		}
		if ( resource->failed ) FeLog() << "Error loading audio file: " << filename << std::endl;
		{
			std::lock_guard<std::mutex> lock( resource->mutex );
			resource->ready.store( true, std::memory_order_release );
		}
		resource->loaded.notify_all();
		FeResourceCache::get().update( FeResourceCache::Audio, key, resource.get(),
			resource->buffer.samples.capacity() * sizeof( float ) + sizeof( FeAudioResource ) );
	} );
	return resource;
}

FeImageCache::FeImageCache( std::size_t bytes ) { FeResourceCache::get().resize( bytes ); }
FeImageCache::~FeImageCache() { FeResourceCache::get().clear( FeResourceCache::Image ); }

void FeImageCache::put( const std::string &key, const std::shared_ptr<FeImageLoaderEntry> &value )
{
	std::weak_ptr<FeImageLoaderEntry> weak = value;
	FeResourceCache::get().put( FeResourceCache::Image, key, value, value->gpu_bytes(), [weak]
	{
		auto entry = weak.lock();
		return entry ? entry->trim_textures() : std::size_t( 0 );
	} );
}

bool FeImageCache::get( const std::string &key, std::shared_ptr<FeImageLoaderEntry> *value )
{
	auto entry = FeResourceCache::get().find<FeImageLoaderEntry>( FeResourceCache::Image, key );
	if ( !entry || entry->m_canceled ) return false;
	*value = std::move( entry );
	return true;
}

void FeImageCache::resize( std::size_t bytes ) { FeResourceCache::get().resize( bytes ); }
std::size_t FeImageCache::get_max_size() { return FeResourceCache::get().limit(); }
std::size_t FeImageCache::get_count() { return FeResourceCache::get().items( FeResourceCache::Image ).size(); }
bool FeImageCache::contains( const std::string &key ) { return bool( FeResourceCache::get().find( FeResourceCache::Image, key ) ); }
void FeImageCache::prune( bool unused ) { FeResourceCache::get().prune( unused ); }

std::size_t FeImageCache::get_size()
{
	std::size_t bytes = 0;
	for ( const auto &item : FeResourceCache::get().items( FeResourceCache::Image ) )
		bytes += std::static_pointer_cast<FeImageLoaderEntry>( item.value )->gpu_bytes();
	return bytes;
}

const char *FeImageCache::get_name_at( int pos )
{
	static thread_local std::string name;
	auto entries = FeResourceCache::get().items( FeResourceCache::Image );
	name = pos >= 0 && pos < static_cast<int>( entries.size() )
		? std::static_pointer_cast<FeImageLoaderEntry>( entries[pos].value )->m_filename : "";
	return name.c_str();
}

std::size_t FeImageCache::get_size_at( int pos )
{
	auto entries = FeResourceCache::get().items( FeResourceCache::Image );
	return pos >= 0 && pos < static_cast<int>( entries.size() )
		? std::static_pointer_cast<FeImageLoaderEntry>( entries[pos].value )->gpu_bytes() : 0;
}

struct FeResourceCache::Impl
{
	struct Entry
	{
		Item item;
		std::function<std::size_t()> trim;
		unsigned long long version = 0;
	};
	using Key = std::pair<Type, std::string>;
	mutable std::mutex mutex;
	std::mutex pruning;
	std::condition_variable ready, idle;
	std::list<std::shared_ptr<Entry>> lru;
	std::map<Key, decltype(lru)::iterator> entries;
	std::deque<std::function<void()>> jobs;
	std::vector<std::thread> workers;
	std::size_t max_bytes = 0, bytes = 0, running = 0;
	bool stopping = false;
	std::atomic<unsigned long long> version{ 0 };

	void run()
	{
		for ( ;; )
		{
			std::function<void()> job;
			{
				std::unique_lock<std::mutex> lock( mutex );
				ready.wait( lock, [this]{ return stopping || !jobs.empty(); } );
				if ( jobs.empty() ) return;
				job = std::move( jobs.front() );
				jobs.pop_front();
				++running;
			}
			try { job(); }
			catch ( const std::exception &error ) { FeLog() << "Resource worker: " << error.what() << std::endl; }
			job = {};
			FeResourceCache::get().prune();
			{
				std::lock_guard<std::mutex> lock( mutex );
				--running;
				if ( jobs.empty() && !running ) idle.notify_all();
			}
		}
	}
};

FeResourceCache &FeResourceCache::get()
{
	static FeResourceCache cache;
	return cache;
}

FeResourceCache::FeResourceCache() : m_impl( new Impl )
{
	for ( int i = 0; i < 2; ++i ) m_impl->workers.emplace_back( &Impl::run, m_impl.get() );
}

FeResourceCache::~FeResourceCache()
{
	wait();
	clear( Any );
	{
		std::lock_guard<std::mutex> lock( m_impl->mutex );
		m_impl->stopping = true;
	}
	m_impl->ready.notify_all();
	for ( auto &worker : m_impl->workers ) worker.join();
}

std::shared_ptr<void> FeResourceCache::find( Type type, const std::string &key )
{
	std::lock_guard<std::mutex> lock( m_impl->mutex );
	auto found = m_impl->entries.find( { type, key } );
	if ( found == m_impl->entries.end() ) return {};
	m_impl->lru.splice( m_impl->lru.begin(), m_impl->lru, found->second );
	return ( *found->second )->item.value;
}

std::shared_ptr<void> FeResourceCache::acquire( Type type, const std::string &key, std::shared_ptr<void> value )
{
	std::lock_guard<std::mutex> lock( m_impl->mutex );
	auto found = m_impl->entries.find( { type, key } );
	if ( found != m_impl->entries.end() )
	{
		m_impl->lru.splice( m_impl->lru.begin(), m_impl->lru, found->second );
		return ( *found->second )->item.value;
	}
	auto entry = std::make_shared<Impl::Entry>();
	entry->item = { type, key, std::move( value ), 0 };
	m_impl->lru.push_front( entry );
	m_impl->entries[{ type, key }] = m_impl->lru.begin();
	return entry->item.value;
}

void FeResourceCache::update( Type type, const std::string &key, const void *value, std::size_t bytes )
{
	{
		std::lock_guard<std::mutex> lock( m_impl->mutex );
		auto found = m_impl->entries.find( { type, key } );
		if ( found == m_impl->entries.end() ) return;
		auto &entry = *found->second;
		if ( entry->item.value.get() != value ) return;
		if ( entry->item.bytes == bytes ) return;
		m_impl->bytes -= entry->item.bytes;
		entry->item.bytes = bytes;
		m_impl->bytes += bytes;
		++entry->version;
		++m_impl->version;
	}
	prune();
}

unsigned long long FeResourceCache::version() const { return m_impl->version.load(); }

void FeResourceCache::put( Type type, const std::string &key, std::shared_ptr<void> value,
	std::size_t bytes, std::function<std::size_t()> trim )
{
	std::shared_ptr<void> old;
	{
		std::lock_guard<std::mutex> lock( m_impl->mutex );
		auto found = m_impl->entries.find( { type, key } );
		if ( found != m_impl->entries.end() )
		{
			auto &entry = *found->second;
			m_impl->bytes -= entry->item.bytes;
			old = std::move( entry->item.value );
			entry->item.value = std::move( value );
			entry->item.bytes = bytes;
			entry->trim = std::move( trim );
			++entry->version;
			m_impl->lru.splice( m_impl->lru.begin(), m_impl->lru, found->second );
		}
		else
		{
			auto entry = std::make_shared<Impl::Entry>();
			entry->item = { type, key, std::move( value ), bytes };
			entry->trim = std::move( trim );
			m_impl->lru.push_front( std::move( entry ) );
			m_impl->entries[{ type, key }] = m_impl->lru.begin();
		}
		m_impl->bytes += bytes;
	}
	retire( std::move( old ) );
	prune();
}

void FeResourceCache::resize( std::size_t bytes )
{
	{
		std::lock_guard<std::mutex> lock( m_impl->mutex );
		m_impl->max_bytes = bytes;
	}
	prune();
}

std::size_t FeResourceCache::limit() const
{
	std::lock_guard<std::mutex> lock( m_impl->mutex );
	return m_impl->max_bytes;
}

std::vector<FeResourceCache::Item> FeResourceCache::items( Type type ) const
{
	std::lock_guard<std::mutex> lock( m_impl->mutex );
	std::vector<Item> result;
	for ( const auto &entry : m_impl->lru )
		if ( type == Any || type == entry->item.type ) result.push_back( entry->item );
	return result;
}

void FeResourceCache::prune( bool unused )
{
	// Concurrent insertions may request another trim; none wait behind IO or destruction.
	std::unique_lock<std::mutex> pruning( m_impl->pruning, std::try_to_lock );
	if ( !pruning.owns_lock() ) return;
	std::vector<std::shared_ptr<Impl::Entry>> entries;
	{
		std::lock_guard<std::mutex> lock( m_impl->mutex );
		if ( !unused && m_impl->max_bytes && m_impl->bytes <= m_impl->max_bytes ) return;
		entries.assign( m_impl->lru.rbegin(), m_impl->lru.rend() );
	}
	for ( const auto &entry : entries )
	{
		std::function<std::size_t()> trim;
		std::size_t bytes;
		unsigned long long version;
		{
			std::lock_guard<std::mutex> lock( m_impl->mutex );
			if ( !unused && m_impl->max_bytes && m_impl->bytes <= m_impl->max_bytes ) break;
			trim = entry->trim;
			bytes = entry->item.bytes;
			version = entry->version;
		}
		const std::size_t remaining = trim ? trim() : bytes;
		std::shared_ptr<void> retired;
		{
			std::lock_guard<std::mutex> lock( m_impl->mutex );
			auto found = m_impl->entries.find( { entry->item.type, entry->item.key } );
			if ( found == m_impl->entries.end() || *found->second != entry || entry->version != version ) continue;
			m_impl->bytes -= entry->item.bytes;
			entry->item.bytes = remaining;
			if ( ( trim && !remaining ) || ( !trim && entry->item.value.use_count() == 1 ) )
			{
				retired = std::move( entry->item.value );
				m_impl->lru.erase( found->second );
				m_impl->entries.erase( found );
			}
			else m_impl->bytes += remaining;
		}
		retire( std::move( retired ) );
	}
}

void FeResourceCache::clear( Type type )
{
	std::vector<std::shared_ptr<void>> retired;
	{
		std::lock_guard<std::mutex> lock( m_impl->mutex );
		for ( auto it = m_impl->lru.begin(); it != m_impl->lru.end(); )
		{
			auto &entry = *it;
			if ( type != Any && type != entry->item.type ) { ++it; continue; }
			m_impl->bytes -= entry->item.bytes;
			m_impl->entries.erase( { entry->item.type, entry->item.key } );
			retired.push_back( std::move( entry->item.value ) );
			it = m_impl->lru.erase( it );
		}
	}
	for ( auto &value : retired ) retire( std::move( value ) );
}

void FeResourceCache::submit( std::function<void()> job )
{
	{
		std::lock_guard<std::mutex> lock( m_impl->mutex );
		m_impl->jobs.push_back( std::move( job ) );
	}
	m_impl->ready.notify_one();
}

void FeResourceCache::retire( std::shared_ptr<void> value )
{
	if ( value ) submit( [value = std::move( value )]() mutable { value.reset(); } );
}

void FeResourceCache::wait()
{
	std::unique_lock<std::mutex> lock( m_impl->mutex );
	m_impl->idle.wait( lock, [this]{ return m_impl->jobs.empty() && !m_impl->running; } );
}
