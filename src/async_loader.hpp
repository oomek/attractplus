#ifndef ASYNC_LOADER_HPP
#define ASYNC_LOADER_HPP

#include <map>
#include <list>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <SFML/Graphics.hpp>

class FeAsyncLoaderEntry
{

friend class FeAsyncLoader;

public:
	~FeAsyncLoaderEntry();

	int get_width();
	int get_height();
	size_t get_bytes();

	sf::Texture &get_texture();

private:

	int m_ref_count;
	int m_width;
	int m_height;
	sf::Texture m_texture;
	// bool m_loaded;

	FeAsyncLoaderEntry();

	int get_ref();
	void inc_ref();
	bool dec_ref();
};

class FeAsyncLoader
{
public:
	typedef std::pair< std::string, FeAsyncLoaderEntry * > kvp_t;

	typedef std::list< kvp_t > list_t;
	typedef list_t::iterator list_iterator_t;

	typedef std::map< std::string, list_iterator_t > map_t;
	typedef map_t::iterator map_iterator_t;

	typedef std::unique_lock<std::mutex> ulock_t;

	~FeAsyncLoader();
	static FeAsyncLoader &get_ref();

	sf::Texture *get_dummy_texture();

	void add_texture( const std::string& );
	sf::Texture *get_texture( const std::string& );
	void release_texture( const std::string& );

	bool pending_updates();

	// Debug
	int get_cached_size();
	int get_active_size();
	int get_queue_size();
	int get_cached_ref( int );
	int get_active_ref( int );

private:
	FeAsyncLoader();
	std::thread m_thread;
	std::mutex m_mutex;
	std::condition_variable m_cond;

	// sf::Context m_ctx;

	list_t m_active;
	list_t m_cached;
	map_t m_map;
	std::queue< std::string > m_in;

	sf::Texture dummy_texture;

	bool m_pending;
	bool m_running;
	void thread_loop();
};

#endif
