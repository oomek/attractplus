#ifndef ASYNC_LOADER_HPP
#define ASYNC_LOADER_HPP

#include <map>
#include <list>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics.hpp>
// #include <SFML/Audio.hpp>


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
	virtual ~FeAsyncLoaderEntryBase() {};

	// FeAsyncLoaderEntryTexture
	virtual sf::Texture *get_texture() { return nullptr; };
	virtual sf::Vector2u get_texture_size() { return sf::Vector2u( 0, 0 ); };
	virtual size_t get_bytes() { return 0; };

	// FeAsyncLoaderEntryFont
	virtual sf::Font *get_font() { return nullptr; };

	// FeAsyncLoaderEntrySoundBuffer
	// virtual sf::SoundBuffer *get_sound_buffer() { return nullptr; };
};



class FeAsyncLoaderEntryTexture : public FeAsyncLoaderEntryBase
{
	friend class FeAsyncLoader;

public:
	FeAsyncLoaderEntryTexture() : m_texture_size(0, 0) {};
	virtual ~FeAsyncLoaderEntryTexture() {};

	sf::Texture *get_texture() override { return &m_texture; };
	sf::Vector2u get_texture_size() override{ return m_texture.getSize(); };
	size_t get_bytes() override { return m_texture.getSize().x * m_texture.getSize().y * 4; };

private:
	sf::Texture m_texture;
	sf::Vector2u m_texture_size;
};



class FeAsyncLoaderEntryFont : public FeAsyncLoaderEntryBase
{
	friend class FeAsyncLoader;

public:
	FeAsyncLoaderEntryFont() {};
	virtual ~FeAsyncLoaderEntryFont() {};

	sf::Font *get_font() override { return &m_font; };

private:
	sf::Font m_font;
};



// class FeAsyncLoaderEntrySoundBuffer : public FeAsyncLoaderEntryBase
// {
// 	friend class FeAsyncLoader;

// public:
// 	FeAsyncLoaderEntrySoundBuffer() {};
// 	virtual ~FeAsyncLoaderEntrySoundBuffer() {};

// 	sf::SoundBuffer *get_sound_buffer() override { return &m_sound_buffer; };

// private:
// 	sf::SoundBuffer m_sound_buffer;
// };



class FeAsyncLoader
{
public:
	typedef std::pair<std::string, FeAsyncLoaderEntryBase*> kvp_t;

	typedef std::list< kvp_t > list_t;
	typedef list_t::iterator list_iterator_t;

	typedef std::map<std::string, list_iterator_t> map_t;
	typedef map_t::iterator map_iterator_t;

	typedef std::unique_lock<std::mutex> ulock_t;

	~FeAsyncLoader();
	static FeAsyncLoader &get_ref();

	sf::Texture *get_dummy_texture();

	void add_texture( const std::string, bool async = true );
	void load_texture( const std::string );
	sf::Texture *get_texture( const std::string );
	void release_texture( const std::string );

	bool done();

	void notify();

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

	list_t m_active;
	list_t m_cached;
	map_t m_map;
	std::queue< std::string > m_in;

	sf::Texture dummy_texture;

	bool m_done;
	bool m_running;
	void thread_loop();
};

#endif
