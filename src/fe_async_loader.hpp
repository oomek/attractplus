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
#include <SFML/Audio.hpp>


enum EntryType
{
	TextureType,
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
	virtual ~FeAsyncLoaderEntryBase() {};

	// FeAsyncLoaderEntryTexture
	virtual sf::Texture *get_texture() { return nullptr; };
	virtual sf::Vector2u get_texture_size() { return sf::Vector2u( 0, 0 ); };
	virtual size_t get_bytes() { return 0; };

	// FeAsyncLoaderEntryFont
	virtual sf::Font *get_font() { return nullptr; };

	// FeAsyncLoaderEntrySoundBuffer
	virtual sf::SoundBuffer *get_soundbuffer() { return nullptr; };

	virtual bool load_from_file( const std::string );
};



class FeAsyncLoaderEntryTexture : public FeAsyncLoaderEntryBase
{
	friend class FeAsyncLoader;

public:
	static const EntryType type = TextureType;
	FeAsyncLoaderEntryTexture() : m_texture_size(0, 0) {};
	~FeAsyncLoaderEntryTexture() {};

	sf::Texture *get_texture() override { return &m_texture; };
	bool load_from_file( const std::string file ) override { return m_texture.loadFromFile( file ); };
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
	static const EntryType type = FontType;
	FeAsyncLoaderEntryFont() {};
	~FeAsyncLoaderEntryFont() {};

	sf::Font *get_font() override { return &m_font; };
	bool load_from_file( const std::string file ) override { return m_font.loadFromFile( file ); };

private:
	sf::Font m_font;
};



class FeAsyncLoaderEntrySoundBuffer : public FeAsyncLoaderEntryBase
{
	friend class FeAsyncLoader;

public:
	static const EntryType type = SoundBufferType;
	FeAsyncLoaderEntrySoundBuffer() {};
	~FeAsyncLoaderEntrySoundBuffer() {};

	sf::SoundBuffer *get_soundbuffer() override { return &m_sound_buffer; };
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

	~FeAsyncLoader();
	static FeAsyncLoader &get_ref();

	sf::Texture *get_dummy_texture();

	void load_resource( const std::string, const EntryType );

	void add_texture( const std::string, bool async = true );
	void add_font( const std::string, bool async = true );
	void add_soundbuffer( const std::string, bool async = true );

	template <typename T>
	void add_resource( const std::string file, bool async );

	sf::Texture *get_texture( const std::string );
	void release_texture( const std::string );

	bool done();

	void notify();

	// Debug
	int get_cached_size();
	int get_active_size();
	int get_queue_size();
	int get_cached_ref_count( int );
	int get_active_ref_count( int );

private:
	FeAsyncLoader();
	std::thread m_thread;
	std::mutex m_mutex;
	std::condition_variable m_cond;

	list_t m_active;
	list_t m_cached;
	map_t m_map;
	std::queue< std::pair< std::string, EntryType >> m_in;

	sf::Texture dummy_texture;

	bool m_done;
	bool m_running;
	void thread_loop();
};

template <typename T>
void FeAsyncLoader::add_resource( const std::string file, bool async )
{
	if ( file.empty() ) return;

	ulock_t lock( m_mutex );
	map_iterator_t it = m_map.find( file );
	if ( it == m_map.end() )
	{
		if ( !async )
		{
			load_resource( file, T::type );
			return;
		}
	}

	m_done = false;
	m_in.push( std::make_pair( file, T::type ));
	lock.unlock();
	m_cond.notify_one();
	return;
}

#endif
