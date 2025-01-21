#ifndef ASYNC_LOADER_HPP
#define ASYNC_LOADER_HPP

#include <map>
#include <list>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "fe_settings.hpp"
#include "media.hpp"

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

enum EntryType
{
	TextureType,
	VideoType,
	FontType,
	SoundBufferType
};

class FeAsyncLoaderEntryBase
{
	friend class FeAsyncLoader;

protected:
	FeAsyncLoaderEntryBase() : m_ref_count(0) {}; // Protected constructor

private:
	int m_ref_count;

	int get_ref();
	void inc_ref();
	bool dec_ref();

public:
	// Virtual to correctly call Derived's destructor, then Base's destructor
	// Definition moved to cpp
	virtual ~FeAsyncLoaderEntryBase();

	// FeAsyncLoaderEntryTexture
	// virtual sf::Vector2u get_texture_size() { return sf::Vector2u( 0, 0 ); };
	// virtual size_t get_bytes() { return 0; };

	virtual bool load_from_file( const std::string ) { return false; };
	virtual void *get_resource_pointer() { return nullptr; };
};



class FeAsyncLoaderEntryTexture : public FeAsyncLoaderEntryBase
{
	friend class FeAsyncLoader;

public:
	static const EntryType type;
	FeAsyncLoaderEntryTexture() : m_texture_size(0, 0) {};
	~FeAsyncLoaderEntryTexture() {};

	void *get_resource_pointer() override { return &m_texture; };
	bool load_from_file( const std::string file ) override { return m_texture.loadFromFile( file ); };

	// sf::Vector2u get_texture_size() override{ return m_texture.getSize(); };
	// size_t get_bytes() override { return m_texture.getSize().x * m_texture.getSize().y * 4; };

private:
	sf::Texture m_texture;
	sf::Vector2u m_texture_size;
};


class FeAsyncLoaderEntryVideo : public FeAsyncLoaderEntryBase
{
	friend class FeAsyncLoader;

public:
	static const EntryType type;
	FeAsyncLoaderEntryVideo();
	~FeAsyncLoaderEntryVideo() {};

	void *get_resource_pointer() override { return &m_texture; };
	bool load_from_file( const std::string file ) override;
	FeMedia* get_player() { return &m_media; };
	FeMedia* get_player( const std::string& file );

private:
	sf::Texture m_texture;
	sf::Vector2u m_texture_size;
	FeMedia m_media;
};

class FeAsyncLoaderEntryFont : public FeAsyncLoaderEntryBase
{
	friend class FeAsyncLoader;

public:
	static const EntryType type;
	FeAsyncLoaderEntryFont() {};
	~FeAsyncLoaderEntryFont() {};

	void *get_resource_pointer() override { return &m_font; };
	bool load_from_file( const std::string file ) override { return m_font.loadFromFile( file ); };

private:
	sf::Font m_font;
};



class FeAsyncLoaderEntrySoundBuffer : public FeAsyncLoaderEntryBase
{
	friend class FeAsyncLoader;

public:
	static const EntryType type;
	FeAsyncLoaderEntrySoundBuffer() {};
	~FeAsyncLoaderEntrySoundBuffer() {};

	void *get_resource_pointer() override { return &m_sound_buffer; };
	bool load_from_file( const std::string file ) override { return m_sound_buffer.loadFromFile( file ); };

private:
	sf::SoundBuffer m_sound_buffer;
};



class FeAsyncLoader
{
public:
	typedef std::pair<std::string, FeAsyncLoaderEntryBase*> kvp_t;

	typedef std::list< kvp_t > list_t;
	typedef list_t::iterator list_iterator_t;

	typedef std::map<std::string, list_iterator_t> map_t;
	typedef map_t::iterator map_iterator_t;

	typedef std::unique_lock<std::mutex> ulock_t;

	std::atomic<int> m_loader_queue_size{0};
	std::atomic<int> m_cleanup_size{0};

	~FeAsyncLoader();
	static FeAsyncLoader &get_al();
	static void clear();

	bool load_resource( const std::string, const EntryType );

	template <typename T>
	bool add_resource( const std::string file, bool async );

	template <typename T>
	T *get_resource( const std::string file );

	void release_resource( const std::string );

	void stop_cached_videos();

	bool add_to_cache( const std::string file );

	sf::Texture *get_resource_texture( const std::string file );
	sf::Texture *get_resource_video( const std::string file );
	sf::Font *get_resource_font( const std::string file );
	sf::SoundBuffer *get_resource_sound( const std::string file );

	FeMedia* get_player(const std::string& file);

	bool done();

	void notify();

	// Debug
	int get_cached_size();
	int get_active_size();
	int get_queue_size();
	int get_cleanup_size();
	int get_cached_ref_count( int );
	int get_active_ref_count( int );

	static void set_cache_size( size_t size );

private:
	FeAsyncLoader();
	static FeAsyncLoader* m_loader;
	std::thread m_loader_thread;
	std::thread m_cleanup_thread;
	std::mutex m_loader_mutex;
	std::mutex m_cleanup_mutex;
	std::condition_variable m_loader_condition;
	std::condition_variable m_cleanup_condition;

	std::atomic<bool> m_loader_running;
	std::atomic<bool> m_cleanup_running;
	bool m_done;
	list_t m_resources_active;
	list_t m_resources_cached;
	list_t m_resources_cleanup;
	map_t m_resources_map;
	std::queue< std::pair< std::string, EntryType >> m_loader_queue;
	static size_t m_cache_size;

	void loader_thread_loop();
	void cleanup_thread_loop();
};

#endif
