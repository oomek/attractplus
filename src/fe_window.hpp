/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-2014 Andrew Mickelson
 *
 *  This file is part of Attract-Mode.
 *
 *  Attract-Mode is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Attract-Mode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Attract-Mode.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef FE_WINDOW_HPP
#define FE_WINDOW_HPP

#include <SFML/Config.hpp>

#ifdef SFML_SYSTEM_WINDOWS
#include <windows.h>
#endif

#include <unordered_map>
#include <SFML/Graphics.hpp>
#include "fe_event.hpp"
#include "fe_joystick.hpp"
#include "fe_types.hpp"
#include "fe_sdl3_gpu.hpp"

class FeSettings;
class FeTextPrimitive;
class FeListBox;
class FeText;
class FeRectangle;
struct SDL_Surface;

struct FeOverlayRect
{
	Vec2f position;
	Vec2f size;
	Color color;

	FeOverlayRect()
		: position( 0.0f, 0.0f ),
		size( 0.0f, 0.0f ),
		color( Color::Transparent )
	{
	}
};

struct FeOverlayDrawItem
{
	enum Type
	{
		OverlayRect,
		TextPrimitive,
		ListBox,
		Text,
		Rectangle
	};

	Type type;
	const void *item;

	explicit FeOverlayDrawItem( const FeOverlayRect &rect )
		: type( OverlayRect ), item( &rect ) {}
	explicit FeOverlayDrawItem( const FeTextPrimitive &text )
		: type( TextPrimitive ), item( &text ) {}
	explicit FeOverlayDrawItem( const FeListBox &listbox )
		: type( ListBox ), item( &listbox ) {}
	explicit FeOverlayDrawItem( const FeText &text )
		: type( Text ), item( &text ) {}
	explicit FeOverlayDrawItem( const FeRectangle &rect )
		: type( Rectangle ), item( &rect ) {}
};

class FeWindowPosition : public FeBaseConfigurable
{
public:
	Vec2i m_pos;
	Vec2u m_size;
	bool m_temporary;

	FeWindowPosition();
	FeWindowPosition(
		const Vec2i &pos,
		const Vec2u &size
	);
	int process_setting(
		const std::string &setting,
		const std::string &value,
		const std::string &filename
	);
	void save(
		const std::string &filename
	);
};

class FeWindow
{
	friend void launch_callback( void *o );
	friend void wait_callback( void *o );

protected:
	FeSettings &m_fes;
	unsigned int m_running_pid;
	void *m_running_wnd;

private:
#ifdef SFML_SYSTEM_WINDOWS
	sf::RenderWindow m_blackout;
	static inline bool s_system_resumed = false;
	static inline WNDPROC s_sfml_wnd_proc = nullptr;
	static LRESULT CALLBACK CustomWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );
	void check_for_sleep();
#endif
	int m_win_mode;
	bool m_mouse_outside = true;
	struct OverlayTextureEntry
	{
		std::vector<unsigned char> pixels;
		FeRenderRawTextureSource source;
	};
	std::vector<FeRenderGeometry> m_overlay_geometry;
	std::unordered_map<const SDL_Surface *, OverlayTextureEntry> m_overlay_images;
	FeWindowPosition m_win_pos;
	FeSdl3GpuContext m_gpu_context;
	bool m_key_repeat_enabled = false;

	const FeRenderRawTextureSource *cache_overlay_image( const SDL_Surface &image );
	bool append_native_overlay_item( const FeOverlayDrawItem &item, const sf::RenderStates &r );

public:
	FeWindow( FeSettings &fes );
	~FeWindow();

	void set_window_position( const FeWindowPosition &pos );
	void initial_create();		// first time window creation
	bool run();						// run the currently selected game (blocking). returns false if window closed in the interim
	void on_exit();				// called before exiting frontend

	// return true if there is another process (i.e. a paused emulator) that we are currently running
	bool has_running_process();

	void display();
	void close();
	void save();
	bool hasFocus();
	bool isOpen();
	Vec2u get_size() const;
	Vec2i get_position() const;
	Vec2i get_mouse_position() const;
	void set_mouse_position( const Vec2i &pos );
	void set_key_repeat_enabled( bool enabled );
	void set_text_input_enabled( bool enabled );
	void set_mouse_cursor_visible( bool visible );
	void on_resize( const Vec2u &size );
	bool save_screenshot( const std::string &filename );
	bool owns_sdl_window() const;

	void clear();
	void draw( const FeOverlayDrawItem &item, const sf::RenderStates &t=sf::RenderStates::Default );
	void draw( const FeOverlayRect &rect, const sf::RenderStates &t=sf::RenderStates::Default );
	void draw( const FeTextPrimitive &text, const sf::RenderStates &t=sf::RenderStates::Default );
	void draw( const FeListBox &listbox, const sf::RenderStates &t=sf::RenderStates::Default );
	void draw( const FeText &text, const sf::RenderStates &t=sf::RenderStates::Default );
	void draw( const FeRectangle &rect, const sf::RenderStates &t=sf::RenderStates::Default );
	void draw_overlay_image( const SDL_Surface &image, const FloatRect &bounds, bool smooth = true, const Color &color = Color::White );
	std::optional<FeEvent> pollEvent();

	FeSdl3GpuContext &get_gpu_context() { return m_gpu_context; }
	const FeSdl3GpuContext &get_gpu_context() const { return m_gpu_context; }
	int get_window_mode() { return m_win_mode; }
};

void fe_joystick_update();
void fe_joystick_refresh_devices();
void fe_joystick_shutdown();
bool fe_joystick_is_connected( unsigned int joystick_id );
bool fe_joystick_is_button_pressed( unsigned int joystick_id, unsigned int button );
float fe_joystick_get_axis_position( unsigned int joystick_id, FeJoystick::Axis axis );
std::string fe_joystick_get_name( unsigned int joystick_id );
int fe_joystick_translate_sdl_instance_id( int instance_id );

#endif
