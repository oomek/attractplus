#include "fe_resource_cache.hpp"
#include "fe_base.hpp"
#include "image_loader.hpp"
#include "fe_present.hpp"
#include "fe_util.hpp"
#include "fe_glslang.hpp"
#include <fstream>

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

FeImageCache::FeImageCache( std::size_t bytes ) : m_max_bytes( bytes ) {}
FeImageCache::~FeImageCache()
{
	for ( auto &item : m_items ) FeImageLoader::get_ref().release_entry( &item.second );
}

void FeImageCache::put( const std::string &key, const std::shared_ptr<FeImageLoaderEntry> &value )
{
	auto found = m_entries.find( key );
	if ( found != m_entries.end() )
	{
		if ( found->second->second != value )
		{
			FeImageLoader::get_ref().release_entry( &found->second->second );
			found->second->second = value;
		}
		m_items.splice( m_items.begin(), m_items, found->second );
	}
	else
	{
		m_items.emplace_front( key, value );
		m_entries[key] = m_items.begin();
	}
	prune();
}

bool FeImageCache::get( const std::string &key, std::shared_ptr<FeImageLoaderEntry> *value )
{
	auto found = m_entries.find( key );
	if ( found == m_entries.end() || found->second->second->m_canceled ) return false;
	m_items.splice( m_items.begin(), m_items, found->second );
	*value = found->second->second;
	return true;
}

void FeImageCache::resize( std::size_t bytes ) { m_max_bytes = bytes; prune(); }
std::size_t FeImageCache::get_max_size() { return m_max_bytes; }
std::size_t FeImageCache::get_count() { return m_items.size(); }
bool FeImageCache::contains( const std::string &key ) { return m_entries.find( key ) != m_entries.end(); }

void FeImageCache::prune( bool unused )
{
	auto bytes = get_size();
	for ( auto it = m_items.end(); it != m_items.begin() && ( unused || !m_max_bytes || bytes > m_max_bytes ); )
	{
		--it;
		auto &textures = it->second->m_textures;
		for ( auto texture = textures.begin(); texture != textures.end() && ( unused || !m_max_bytes || bytes > m_max_bytes ); )
		{
			if ( texture->second.use_count() != 2 || texture->second->status == FeImageTexture::Pending ) { ++texture; continue; }
			if ( texture->second->status == FeImageTexture::Ready ) bytes -= texture->second->bytes;
			texture = textures.erase( texture );
		}
		if ( it->second->gpu_bytes() ) continue;
		m_entries.erase( it->first );
		FeImageLoader::get_ref().release_entry( &it->second );
		it = m_items.erase( it );
	}
}

std::size_t FeImageCache::get_size()
{
	std::size_t bytes = 0;
	for ( const auto &item : m_items ) bytes += item.second->gpu_bytes();
	return bytes;
}

const char *FeImageCache::get_name_at( int pos )
{
	if ( pos < 0 || pos >= static_cast<int>( m_items.size() ) ) return "";
	auto item = m_items.begin();
	std::advance( item, pos );
	return item->second->m_filename.c_str();
}

std::size_t FeImageCache::get_size_at( int pos )
{
	if ( pos < 0 || pos >= static_cast<int>( m_items.size() ) ) return 0;
	auto item = m_items.begin();
	std::advance( item, pos );
	return item->second->gpu_bytes();
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

std::size_t FeResourceCache::size() const
{
	std::lock_guard<std::mutex> lock( m_impl->mutex );
	return m_impl->bytes;
}

bool FeResourceCache::get_background_loading() { return FeImageLoader::get_ref().get_background_loading(); }
void FeResourceCache::set_background_loading( bool value ) { FeImageLoader::get_ref().set_background_loading( value ); }
void FeResourceCache::add_image( const char *filename ) { FeImageLoader::get_ref().prefetch_image( filename, false ); }

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

namespace
{
	std::string join_path( const std::string &base, const std::string &suffix )
	{
		if ( base.empty() )
			return suffix;

		const char last = base[ base.size() - 1 ];
		if ( last == '/' || last == '\\' )
			return base + suffix;

		return base + "/" + suffix;
	}

	std::string get_base_path()
	{
		return get_program_path();
	}

	std::string get_shader_cache_base_path()
	{
#if !defined( SDL_PLATFORM_WINDOWS )
		return absolute_path( clean_path( std::string( FE_DEFAULT_CFG_PATH ) + FE_CACHE_SUBDIR ) );
#else
		return join_path( get_base_path(), FE_CACHE_SUBDIR );
#endif
	}

	struct ShaderCompileCacheEntry
	{
		std::string key;
		std::atomic<bool> started{ false }, ready{ false };
		std::mutex mutex;
		std::condition_variable loaded;
		bool compile_failed;
		ShaderBlob blob;

		ShaderCompileCacheEntry()
			: compile_failed( false )
		{
		}
	};

	bool file_exists( const std::string &path )
	{
		std::ifstream stream( path.c_str(), std::ios::binary );
		return stream.good();
	}

	bool read_binary_file( const std::string &path, std::vector<Uint8> &code )
	{
		std::ifstream stream( path.c_str(), std::ios::binary );
		if ( !stream )
			return false;

		stream.seekg( 0, std::ios::end );
		const std::streamoff size = stream.tellg();
		stream.seekg( 0, std::ios::beg );

		if ( size <= 0 )
			return false;

		code.resize( static_cast<std::size_t>( size ) );
		stream.read( reinterpret_cast<char *>( code.data() ), size );
		return stream.good();
	}

	bool write_binary_file( const std::string &path, const std::vector<Uint8> &code )
	{
		if ( code.empty() )
			return false;

		std::ofstream stream( path.c_str(), std::ios::binary | std::ios::trunc );
		if ( !stream )
			return false;

		stream.write( reinterpret_cast<const char *>( code.data() ), static_cast<std::streamsize>( code.size() ) );
		return stream.good();
	}

	bool compile_shader_blob_uncached(
		const std::string &source_id,
		const std::string &translated_source,
		bool vertex_stage,
		ShaderBlob &blob, const std::string &root )
	{
		std::string cache_root = root;
		confirm_directory( cache_root, "" );
		confirm_directory( cache_root, "shaders/" );
		cache_root = join_path( cache_root, "shaders/" );
		confirm_directory( cache_root, "sdl3/" );
		cache_root = join_path( cache_root, "sdl3/" );

		std::string shader_name = path_filename( source_id );
		if ( shader_name.empty() )
			shader_name = source_id;

		const std::string cache_name =
			sanitize_filename( shader_name ) + "-" + std::to_string( std::hash<std::string>{}( translated_source ) );
		const std::string stage_name = vertex_stage ? "vert" : "frag";
		const std::string spirv_path = join_path( cache_root, cache_name + "." + stage_name + ".spv" );
		bool compiled = false;
		if ( !file_exists( spirv_path ) || !read_binary_file( spirv_path, blob.code ) || blob.code.empty() )
		{
			std::string diagnostics;
			blob.code.clear();
			compiled = true;

			if ( !fe_glslang_compile_to_spirv( shader_name, translated_source, vertex_stage, blob.code, diagnostics ) || blob.code.empty() )
			{
				FeLog() << "SDL: shader_compile: error " << shader_name << std::endl;
				if ( !diagnostics.empty() )
					FeLog() << "SDL: " << diagnostics << std::endl;
				return false;
			}

			if ( !diagnostics.empty() )
				FeDebug() << "SDL: " << diagnostics << std::endl;

			write_binary_file( spirv_path, blob.code );
		}

		if ( compiled )
			FeDebug() << "SDL: shader_compile: ok " << shader_name << std::endl;

		blob.format = SDL_GPU_SHADERFORMAT_SPIRV;
		blob.entrypoint = "main";

		return true;
	}

}

bool compile_shader_blob( const std::string &source_id, const std::string &source,
	bool vertex, ShaderBlob &blob, std::shared_ptr<void> *request, bool *pending )
{
	auto &cache = FeResourceCache::get();
	const std::string key = std::string( vertex ? "vert|" : "frag|" ) + std::to_string( std::hash<std::string>{}( source ) );
	auto entry = request ? std::static_pointer_cast<ShaderCompileCacheEntry>( *request ) : nullptr;
	if ( entry && entry->key != key ) entry.reset();
	if ( !entry ) entry = cache.find<ShaderCompileCacheEntry>( FeResourceCache::Shader, key );
	if ( !entry )
	{
		auto candidate = std::make_shared<ShaderCompileCacheEntry>();
		candidate->key = key;
		entry = std::static_pointer_cast<ShaderCompileCacheEntry>( cache.acquire( FeResourceCache::Shader, key, candidate ) );
	}
	if ( request ) *request = entry;
	if ( !entry->started.exchange( true ) )
	{
		const auto root = get_shader_cache_base_path();
		auto compile = [entry, source_id, source, vertex, root, key]
		{
			try { entry->compile_failed = !compile_shader_blob_uncached( source_id, source, vertex, entry->blob, root ); }
			catch ( const std::exception &error )
			{
				entry->compile_failed = true;
				FeLog() << "Error preparing shader: " << source_id << ": " << error.what() << std::endl;
			}
			{
				std::lock_guard<std::mutex> lock( entry->mutex );
				entry->ready.store( true, std::memory_order_release );
			}
			entry->loaded.notify_all();
			FeResourceCache::get().update( FeResourceCache::Shader, key, entry.get(), entry->blob.code.capacity() + sizeof( ShaderCompileCacheEntry ) );
		};
		if ( request ) cache.submit( std::move( compile ) );
		else compile();
	}
	if ( pending )
	{
		*pending = !entry->ready.load( std::memory_order_acquire );
		if ( *pending ) return false;
	}
	else
	{
		std::unique_lock<std::mutex> lock( entry->mutex );
		entry->loaded.wait( lock, [&]{ return entry->ready.load( std::memory_order_acquire ); } );
	}
	if ( entry->compile_failed ) return false;
	blob = entry->blob;
	return !blob.code.empty();
}
