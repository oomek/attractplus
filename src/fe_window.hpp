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

#ifdef SFML_SYSTEM_WINDOWS
#include <windows.h>
#endif

#include <SFML/Graphics.hpp>

class FeSettings;

class FeWindowPosition : public FeBaseConfigurable
{
public:
	sf::Vector2i m_pos;
	sf::Vector2u m_size;
	bool m_temporary;

	FeWindowPosition();
	FeWindowPosition(
		const sf::Vector2i &pos,
		const sf::Vector2u &size
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
	sf::RenderWindow *m_window;
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
	FeWindowPosition m_win_pos;

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

	void clear();
	void draw( const sf::Drawable &d, const sf::RenderStates &t=sf::RenderStates::Default );
	const std::optional<sf::Event> pollEvent();

	sf::RenderWindow &get_win();
	int get_window_mode() { return m_win_mode; }
};

#endif
