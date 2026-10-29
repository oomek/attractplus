/* Shared ownership and unused-resource retention for frontend assets. */
#ifndef FE_RESOURCE_CACHE_HPP
#define FE_RESOURCE_CACHE_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <list>
#include <map>
#include <string>
#include <vector>
#include "fe_audio_sdl.hpp"

struct FeAudioResource
{
	std::atomic<bool> started{ false }, ready{ false };
	std::mutex mutex;
	std::condition_variable loaded;
	FeAudioSampleBuffer buffer;
	bool failed = false;
	void wait();
};

std::shared_ptr<FeAudioResource> fe_get_audio_resource( const std::string &filename );

struct ShaderBlob
{
	SDL_GPUShaderFormat format;
	std::vector<Uint8> code;
	const char *entrypoint;
};


bool compile_shader_blob( const std::string &source_id, const std::string &source,
	bool vertex, ShaderBlob &blob, std::shared_ptr<void> *request = nullptr, bool *pending = nullptr );

class FeImageLoaderEntry;

class FeImageCache
{
public:
	explicit FeImageCache( std::size_t bytes );
	~FeImageCache();
	void put( const std::string &, const std::shared_ptr<FeImageLoaderEntry> & );
	bool get( const std::string &, std::shared_ptr<FeImageLoaderEntry> * );
	void resize( std::size_t bytes );
	std::size_t get_max_size();
	std::size_t get_size();
	std::size_t get_count();
	const char *get_name_at( int );
	std::size_t get_size_at( int );
	bool contains( const std::string & );
	void prune( bool unused = false );

private:
	using Item = std::pair<std::string, std::shared_ptr<FeImageLoaderEntry>>;
	std::list<Item> m_items;
	std::map<std::string, std::list<Item>::iterator> m_entries;
	std::size_t m_max_bytes;
};

class FeResourceCache
{
public:
	enum Type { Image, Audio, Font, Shader, Any };
	struct Item
	{
		Type type;
		std::string key;
		std::shared_ptr<void> value;
		std::size_t bytes;
	};

	static FeResourceCache &get();
	~FeResourceCache();
	std::shared_ptr<void> find( Type, const std::string &key );
	std::shared_ptr<void> acquire( Type, const std::string &key, std::shared_ptr<void> );
	void update( Type, const std::string &key, const void *value, std::size_t bytes );
	unsigned long long version() const;
	void put( Type, const std::string &key, std::shared_ptr<void>, std::size_t bytes,
		std::function<std::size_t()> trim = {} );
	void resize( std::size_t bytes );
	std::size_t limit() const;
	std::size_t size() const;
	bool get_background_loading();
	void set_background_loading( bool );
	void add_image( const char * );
	std::vector<Item> items( Type = Any ) const;
	void prune( bool unused = false );
	void clear( Type );
	void submit( std::function<void()> );
	void retire( std::shared_ptr<void> );
	void wait();

	// Resource loaders never run while holding the registry lock.
	template<class T> std::shared_ptr<T> find( Type type, const std::string &key )
	{
		return std::static_pointer_cast<T>( find( type, key ) );
	}

private:
	FeResourceCache();
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

#endif
