/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-2018 Andrew Mickelson
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

#ifndef SFML_SYSTEM_MACOS
#include "attractplus_icon.hpp"
#endif
#include "fe_util.hpp"
#include "fe_settings.hpp"
#include "fe_window.hpp"
#include "fe_present.hpp"
#include "base64.hpp"

#ifdef SFML_SYSTEM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#endif // SFML_SYSTEM_WINDOWS

#ifdef SFML_SYSTEM_MACOS
#include "fe_util_osx.hpp"
#endif // SFM_SYSTEM_MACOS

#include <iostream>
#include "nowide/fstream.hpp"

#include <SFML/System/Sleep.hpp>

#ifdef SFML_SYSTEM_WINDOWS
void set_win32_foreground_window( HWND hwnd, HWND order )
{
	HWND hCurWnd = GetForegroundWindow();
	DWORD dwMyID = GetCurrentThreadId();
	DWORD dwCurID = GetWindowThreadProcessId(hCurWnd, NULL);
	AttachThreadInput(dwCurID, dwMyID, true);
	SetWindowPos(hwnd, order, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	SetForegroundWindow(hwnd);
	SetFocus(hwnd);
	SetActiveWindow(hwnd);
	AttachThreadInput(dwCurID, dwMyID, false);
}
#endif

FeWindowPosition::FeWindowPosition():
	m_pos({ 0, 0 }),
	m_size({ 0, 0 })
{}

FeWindowPosition::FeWindowPosition(
	const sf::Vector2i &pos,
	const sf::Vector2u &size
):
	m_pos( pos ),
	m_size( size )
{}

int FeWindowPosition::process_setting(
	const std::string &setting,
	const std::string &value,
	const std::string &filename )
{
	size_t pos=0;
	std::string token;
	if ( setting.compare( "position" ) == 0 )
	{
		token_helper( value, pos, token, "," );
		m_pos.x = as_int( token );

		token_helper( value, pos, token );
		m_pos.y = as_int( token );
	}
	else if ( setting.compare( "size" ) == 0 )
	{
		token_helper( value, pos, token, "," );
		m_size.x = as_int( token );

		token_helper( value, pos, token );
		m_size.y = as_int( token );
	}
	return 1;
};

void FeWindowPosition::save( const std::string &filename )
{
	nowide::ofstream outfile( filename.c_str() );
	if ( outfile.is_open() )
	{
		outfile << "position " << m_pos.x << "," << m_pos.y << std::endl;
		outfile << "size " << m_size.x << "," << m_size.y << std::endl;
	}
	outfile.close();
}

bool is_multimon_config( FeSettings &fes )
{
	return fes.get_multimon() && !is_windowed_mode( fes.get_window_mode() );
}

#ifdef SFML_SYSTEM_WINDOWS
LRESULT CALLBACK FeWindow::CustomWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	if ( msg == WM_POWERBROADCAST )
		if ( wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND )
			FeWindow::s_system_resumed = true;

	return CallWindowProc( s_sfml_wnd_proc, hwnd, msg, wParam, lParam );
}
#endif

FeWindow::FeWindow( FeSettings &fes )
	: m_window( NULL ),
	m_fes( fes ),
	m_running_pid( 0 ),
	m_running_wnd( NULL ),
	m_win_mode( 0 )
{
}

FeWindow::~FeWindow()
{
	if ( m_running_pid && process_exists( m_running_pid ) )
		kill_program( m_running_pid );

	if ( m_window )
		delete m_window;
}

void FeWindow::display()
{
	m_window->display();

	// Starting from Windows Vista non fullscreen window modes
	// should be synced by DWM, instead of v-sync
#ifdef SFML_SYSTEM_WINDOWS
	if ( m_win_mode != FeSettings::Fullscreen )
		DwmFlush();
	check_for_sleep();
#endif
}

#ifdef SFML_SYSTEM_WINDOWS
void FeWindow::check_for_sleep()
{
	if ( s_system_resumed )
	{
		if ( m_window )
		{
			m_window->clear();
			m_window->display();
		}

		FeDebug() << "! NOTE: Resume from sleep detected. Resetting audio device" << std::endl;
		sf::sleep( sf::milliseconds( 2000 ) ); // Wait 2 seconds to allow audio devices to wake up

		std::optional<std::string> current_device = sf::PlaybackDevice::getDevice();
		if ( current_device.has_value() )
		{
			bool success = false;
			for ( int attempt = 0; attempt < 20; ++attempt )
			{
				success = sf::PlaybackDevice::setDevice( *current_device );
				if ( success )
					break;
				else
					sf::sleep( sf::milliseconds( 100 ) );
			}
			if ( !success )
				FeLog() << "ERROR: Failed to reinitialize audio" << std::endl;
		}
		else
			FeLog() << "ERROR: No current audio device" << std::endl;

		s_system_resumed = false;
	}
}
#endif

void FeWindow::set_window_position( const FeWindowPosition &pos )
{
	m_win_pos = pos;
}

void FeWindow::initial_create()
{
	// If re-initialising save its position first
	if ( m_window )
		save();
	else
		m_window = new sf::RenderWindow();

	int style_map[4] =
	{
		sf::Style::None,       // FeSettings::Fillscreen
		sf::Style::None,       // FeSettings::Fullscreen
		sf::Style::Default,    // FeSettings::Window
		sf::Style::None        // FeSettings::WindowNoBorder
	};

	sf::State state_map[4] =
	{
		sf::State::Windowed,   // FeSettings::Fillscreen
		sf::State::Fullscreen, // FeSettings::Fullscreen
		sf::State::Windowed,   // FeSettings::Window
		sf::State::Windowed    // FeSettings::WindowNoBorder
	};

	sf::VideoMode vm = sf::VideoMode::getDesktopMode(); // width/height/bpp of OpenGL surface to create

	sf::Vector2i wpos( 0, 0 );  // position to set window to

	bool do_multimon = is_multimon_config( m_fes );
	m_win_mode = m_fes.get_window_mode();

#if defined(USE_XLIB)

	if ( !do_multimon && ( m_win_mode != FeSettings::Fullscreen ))
	{
		// If we aren't doing multimonitor mode (it isn't configured or we are in a window)
		// then use the primary screen size as our OpenGL surface size and 'fillscreen' window
		// size.
		//
		// We don't do this on "Fullscreen", which has to be set to a valid videomode
		// returned by SFML.

		// Known issue: Linux Mint 18.3 Cinnamon w/ SFML 2.5.1, w/ fullscreen and multimon disabled:
		// SFML fullscreen is extended across all monitors but positioned incorrectly
		// (it is positioned to accomodate window decoration that isn't there).  setPosition()
		// doesn't work
		//
		get_x11_primary_screen_size( vm.size.x, vm.size.y );
	}
	else
	{
		// In testing on Linux Mint Cinnamon 18.3 w/ SFML 2.5.1, this call to get_x11_multimon_geometry()
		// isn't needed and multimon works without any further repositioning of our window.  I'm
		// keeping it though because it has been needed historically (earlier versions of SFML,
		// other window managers etc) and it seems to lead to the same results
		//
		get_x11_multimon_geometry( wpos.x, wpos.y, vm.size.x, vm.size.y );
	}

#elif defined(SFML_SYSTEM_WINDOWS)

	//
	// Windows General Notes:
	//
	// Out present strategy with Windows is to stick with the WS_POPUP window style for our
	// window.  SFML seems to always create windows with this style
	//
	// In previous FE versions, the WS_POPUP style was causing grief switching to MAME.
	// It also looked clunky/flickery when transitioning between frontend and emulator.
	// More recent tests suggest these WS_POPUP problems have gone away (fingers crossed)
	//
	// With Windows 10 v1607, it seems that the WS_POPUP style is required in order for a
	// window to be drawn over the taskbar.
	//
	// Windows API call to undo the WS_POPUP Style.  Seems to require a ShowWindow() call
	// afterwards to take effect:
	//
	//		SetWindowLongPtr( getNativeHandle(), GWL_STYLE,
	//			WS_BORDER | WS_CLIPCHILDREN | WS_CLIPSIBLINGS );
	//

	//
	//
	if ( do_multimon
			&& ( m_win_mode == FeSettings::Fullscreen )
			&& ( GetSystemMetrics( SM_CMONITORS ) > 1 ) )
	{
		//
		// Tested on Windows 10 w/ SFML 2.5.1 - SFML seems to be forcing the window to being the primary
		// monitor size notwithstanding that we tell it to use bigger (i.e. full multimon desktop) dimensions.
		//
		// As a workaround we force 'Fill Screen' mode here, since the user seems to want multimon to work and we
		// have detected that multiple monitors are available
		//
		FeLog() << " ! NOTE: Switching to 'Fill Screen' window mode (required for multiple monitor support)." << std::endl;
		m_win_mode = FeSettings::Fillscreen;
	}

	// Cover all available monitors with our window in multimonitor config
	//
	if ( do_multimon )
	{
		wpos.x = GetSystemMetrics( SM_XVIRTUALSCREEN );
		wpos.y = GetSystemMetrics( SM_YVIRTUALSCREEN );

		vm.size.x = GetSystemMetrics( SM_CXVIRTUALSCREEN );
		vm.size.y = GetSystemMetrics( SM_CYVIRTUALSCREEN );
	}

	sf::Vector2i screen_size( vm.size.x, vm.size.y );

	// Some Windows users are reporting emulators hanging/failing to get focus when launched
	// from 'fullscreen' (fullscreen, fillscreen where window dimensions = screen dimensions)
	// They also report that the same emulator does work when the frontend is in one of its windowed
	// modes.
	//
	// We work around this issue for these users by having the default "fillscreen" mode actually
	// extend 1 pixel offscreen in each direction (-1, -1, scr_width+2, scr_height+2).  The expectation
	// is that this will prevent Windows from giving the frontend window the "fullscreen" treatment
	// which seems to be the cause of this issue.  This is actually the same behaviour that earlier
	// versions had (first by design, then by accident).
	//
	if ( m_win_mode == FeSettings::Fillscreen )
	{
		wpos.x -= 1;
		wpos.y -= 1;
		vm.size.x += 2;
		vm.size.y += 2;
	}

#endif

	//
	// If in windowed mode
	//
	if ( is_windowed_mode( m_win_mode ) )
	{
		// Default to a centered 4:3 window (3:4 on vertical monitors) at 3/4 screen height
		int monitor_width = (int)vm.size.x;
		int monitor_height = (int)vm.size.y;
		bool vert = monitor_height > monitor_width;
		int h = std::max( monitor_height * 3 / 4, 240 );
		int w = std::max( vert ? h * 3 / 4 : h * 4 / 3, 320 );
		int x = (monitor_width - w) / 2;
		int y = (monitor_height - h) / 2;

		// Load the parameters from the window.am file (uses given args if none)
		if ( !m_win_pos.m_temporary )
		{
			m_win_pos.m_pos = sf::Vector2i( x, y );
			m_win_pos.m_size = sf::Vector2u( w, h );
			m_win_pos.load_from_file( m_fes.get_config_dir() + FE_CFG_SUBDIR + FE_WINDOW_FILE );
		}

		sf::Rect<int> window_rect( m_win_pos.m_pos, sf::Vector2i( m_win_pos.m_size ) );
#if !defined(NO_MULTIMON)
		// The window can be positioned anywhere in the virtual screen
		sf::Rect<int> vm_rect(
			{ GetSystemMetrics( SM_XVIRTUALSCREEN ), GetSystemMetrics( SM_YVIRTUALSCREEN ) },
			{ GetSystemMetrics( SM_CXVIRTUALSCREEN ), GetSystemMetrics( SM_CYVIRTUALSCREEN ) }
		);
#else
		// No multi-monitor support
		sf::Rect<int> vm_rect( wpos, sf::Vector2i( vm.size ) );
#endif

		// Reset the window position if it's completely outside the virtual screen
		// - Usually occurs when the hardware has changed, ie: Un-docking a laptop
		// - NOTE: Window may still hide in part of the virtual screen thats off-monitor
		// - TODO: Enumerate and check all individual monitors
		if ( !window_rect.findIntersection( vm_rect ) )
		{
			FeLog() << "Warning: Window " << window_rect.size.x << "x" << window_rect.size.y << " @ " << window_rect.position.x << "," << window_rect.position.y
				<< " is outside desktop "
				<< vm_rect.size.x << "x" << vm_rect.size.y << " @ " << vm_rect.position.x << "," << vm_rect.position.y
				<< " reset to " << window_rect.size.x << "x" << window_rect.size.y << " @ 0,0" << std::endl;
			m_win_pos.m_pos = { 0, 0 };
		}

		// Populate our local variables from m_win_pos
		wpos = m_win_pos.m_pos;
		vm.size.x = m_win_pos.m_size.x;
		vm.size.y = m_win_pos.m_size.y;
	}

	sf::Vector2i wsize( vm.size.x, vm.size.y );

#if defined(SFML_SYSTEM_WINDOWS)

	// If the window mode is set to Borderless Window and the values in window.am
	// match the display resolution we treat it as if it was Fullscreen
	// to properly handle borderless fulscreen optimizations.
	// Required on Vista and above.
	//
	// On Windows 10/11 there is an issue with WindowNoBorder.
	// When the window size matches the display resolution and the x,y position
	// in window.am is set to be > 0 Windows will move this window to 0,0.
	// In this case we simply set the window mode to Fullscreen to correctly trigger other logic.
	//
	if (( m_win_mode == FeSettings::WindowNoBorder )
		&& ( screen_size.x == (int)vm.size.x )
		&& ( screen_size.y == (int)vm.size.y ))
		{
			FeLog() << "Borderless window size matches the display resolution. Switching to Fullscreen." << std::endl;
			m_win_mode = FeSettings::Fullscreen;
			wpos.x = 0;
			wpos.y = 0;
		}

	// To avoid problems with black screen on launching games when window mode is set to Fullscreen
	// we hide the main renderwindow and show this m_blackout window instead
	// which has the extended size by 1 pixel in each direction to stop Windows
	// from treating it as exclusive borderless.
	//
	if ( m_win_mode == FeSettings::Fullscreen )
	{
		m_blackout.create( sf::VideoMode({ 16, 16 }, 24 ), "", sf::Style::None );
		m_blackout.setSize( sf::Vector2u( vm.size.x + 2, vm.size.y + 2 ));
		m_blackout.setPosition( sf::Vector2i( -1, -1 ));
		m_blackout.setVerticalSyncEnabled(true);
		m_blackout.setKeyRepeatEnabled(false);
		m_blackout.setMouseCursorVisible(false);


		// We hide the black window from the task bar and the alt+tab switcher
		int style = GetWindowLongPtr(m_blackout.getNativeHandle(), GWL_EXSTYLE );
		SetWindowLongPtr( m_blackout.getNativeHandle(), GWL_EXSTYLE, style | WS_EX_TOOLWINDOW );
		m_blackout.clear();
		m_blackout.display();
	}
#endif

	//
	// Create window
	//
	sf::ContextSettings ctx;
	ctx.antiAliasingLevel = m_fes.get_antialiasing();

	m_window->create( vm, FE_NAME, style_map[ m_win_mode ], state_map[ m_win_mode ], ctx );

	// On Windows Vista and above all non fullscreen window modes
	// go through DWM. We have to disable vsync
	// when we rely solely on DwmFlush()
#if defined(SFML_SYSTEM_WINDOWS)
	m_window->setVerticalSyncEnabled( m_win_mode == FeSettings::Fullscreen );
#else
	m_window->setVerticalSyncEnabled(true);
#endif
	m_window->setKeyRepeatEnabled(false);
	m_window->setMouseCursorVisible( is_windowed_mode( m_win_mode ));
	m_window->setJoystickThreshold( 1.0 );

#ifndef SFML_SYSTEM_MACOS
    sf::Image icon;
    if ( icon.loadFromMemory( attractplus_icon, sizeof( attractplus_icon )))
    	m_window->setIcon({ icon.getSize().y, icon.getSize().y }, icon.getPixelsPtr() );
#endif
	// We need to clear and display here before calling setSize and setPosition
	// to avoid a white window flash on launch.
	clear();
	display();

#ifdef SFML_SYSTEM_MACOS
	if ( m_win_mode == FeSettings::Fillscreen )
	{
		// note ordering req: pretty sure this needs to be before the setPosition() call below
		osx_hide_menu_bar();
	}
#endif

#if defined(USE_XLIB)
	if ( m_win_mode == FeSettings::Fillscreen )
		set_x11_fullscreen_state( m_window->getNativeHandle() );
#endif

	// Known issue: Linux Mint 18.3 Cinnamon w/ SFML 2.5.1, position isn't being set
	// (Window always winds up at 0,0)
	// There is currently no way to create window at position, or create hidden then reveal
	m_window->setPosition( wpos );

	FeDebug() << "Created " << FE_NAME << " Window: " << wsize.x << "x" << wsize.y << " @ "
		<< wpos.x << "," << wpos.y << " [OpenGL surface: "
		<< vm.size.x << "x" << vm.size.y << " bpp=" << vm.bitsPerPixel << "]" << std::endl;

#if defined(SFML_SYSTEM_WINDOWS)
	set_win32_foreground_window( m_window->getNativeHandle(), m_fes.get_window_topmost() ? HWND_TOPMOST : HWND_TOP );
	HWND hwnd = static_cast<HWND>( m_window->getNativeHandle() );

	// Enable dark mode titlebar on Windows
	BOOL value = TRUE;
	DwmSetWindowAttribute( hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof( value ));

	s_sfml_wnd_proc = reinterpret_cast<WNDPROC>( GetWindowLongPtr( hwnd, GWLP_WNDPROC ));
	SetWindowLongPtr( hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( CustomWndProc ));
#endif

	m_fes.init_mouse_capture( m_window );

	// Only mess with the mouse position if mouse moves mapped
	if ( m_fes.has_mouse_moves() )
		sf::Mouse::setPosition( wsize / 2, *m_window );
}

void launch_callback( void *o )
{
#if defined(SFML_SYSTEM_LINUX)
	FeWindow *win = (FeWindow *)o;
	if ( win->m_fes.get_window_mode() == FeSettings::Fullscreen )
	{
		//
		// On X11 Linux, fullscreen is confirmed to block the emulator
		// from running on some systems...
		//
#if defined(USE_XLIB)
		sf::sleep( sf::milliseconds( 1000 ) );
#endif
		FeDebug() << "Closing Attract-Mode Plus window" << std::endl;
		win->close(); // this fixes raspi version (w/sfml-pi) obscuring daphne (and others?)
	}
#endif
}

void wait_callback( void *o )
{
	FeWindow *win = (FeWindow *)o;

	if ( win->isOpen() )
	{
		while ( const std::optional ev = win->pollEvent() )
		{
			if ( ev.has_value() && ev->is<sf::Event::Closed>() )
				return;
		}
	}
}

bool FeWindow::run()
{
	sf::Vector2i reset_pos;
	bool windowed = is_windowed_mode( m_fes.get_window_mode() );

	if ( !windowed && m_fes.get_info_bool( FeSettings::MoveMouseOnLaunch ) )
	{
		// Move the mouse to the bottom right corner so it isn't visible
		// when the emulator launches.
		//
		reset_pos = sf::Mouse::getPosition();

		sf::Vector2i hide_pos = m_window->getPosition();
		hide_pos.x += get_win().getSize().x - 1;
		hide_pos.y += get_win().getSize().y - 1;

		sf::Mouse::setPosition( hide_pos );
	}

	sf::Clock timer;

	// We need to get this variable before calling m_fes.prep_for_launch(),
	// which goes and resets the last launch tracking to the current selection
	bool is_relaunch = m_fes.is_last_launch( 0, 0 );

	std::string command, args, work_dir;
	FeEmulatorInfo *emu = NULL;
	m_fes.prep_for_launch( command, args, work_dir, emu );
	FePresent::script_process_magic_strings( args, 0, 0 );

	if ( !emu )
	{
		FeLog() << "Error getting emulator info for launch" << std::endl;
		return true;
	}

	// non-blocking mode wait time (in seconds)
	int nbm_wait = as_int( emu->get_info( FeEmulatorInfo::NBM_wait ) );

	run_program_options_class opt;
	opt.exit_hotkey = emu->get_info( FeEmulatorInfo::Exit_hotkey );
	opt.pause_hotkey = emu->get_info( FeEmulatorInfo::Pause_hotkey );
	opt.joy_thresh = m_fes.get_joy_thresh();
#if defined (USE_DRM)
	opt.launch_cb = NULL; // In DRM we're closing the window before launching the emulator
#else
	opt.launch_cb = (( nbm_wait <= 0 ) ? launch_callback : NULL );
#endif
	opt.wait_cb = wait_callback;
	opt.launch_opaque = this;

#if defined(USE_DRM)
	FePresent *fep = FePresent::script_get_fep();
	fep->clear_resources();

	close();
	delete m_window;
	m_window = NULL;
#endif

	bool have_paused_prog = m_running_pid && process_exists( m_running_pid );

#if defined(SFML_SYSTEM_WINDOWS)
	if ( m_win_mode == FeSettings::Fullscreen )
	{
		set_win32_foreground_window( m_window->getNativeHandle(), HWND_BOTTOM );
		m_blackout.display();
		m_window->setVisible( false );
		set_win32_foreground_window( m_blackout.getNativeHandle(), HWND_TOP );
	}
	else
	{
		set_win32_foreground_window( m_window->getNativeHandle(), HWND_TOP );
		if ( !is_multimon_config( m_fes ))
			clear();
		display();
	}
#endif

	// check if we need to resume a previously paused game
	if ( have_paused_prog && is_relaunch )
	{
		FeLog() << "*** Resuming previously paused program, pid: " << m_running_pid << std::endl;
		resume_program( m_running_pid, m_running_wnd, &opt );
	}
	else
	{
		if ( have_paused_prog )
		{
			FeLog() << "*** Killing previously paused program, pid: " << m_running_pid << std::endl;
			kill_program( m_running_pid );

			m_running_pid = 0;
			m_running_wnd = NULL;
		}

		if ( !work_dir.empty() )
			FeLog() << " - Working directory: " << work_dir << std::endl;

		FeLog() << "*** Running: " << command << " " << args << std::endl;

		run_program(
			command,
			args,
			work_dir,
			NULL,
			NULL,
			( nbm_wait <= 0 ), // don't block if nbm_wait > 0
			&opt );
	}

	if ( opt.running_pid != 0 )
	{
		// User has paused the progam and it is still running in the background
		// (Note that this only can happen when run_program is blocking (i.e. nbm_wait <= 0)
		m_running_pid = opt.running_pid;
		m_running_wnd = opt.running_wnd;
	}
	else
	{
		m_running_pid = 0;
		m_running_wnd = NULL;
	}

	//
	// If nbm_wait > 0, then m_fes.run() above is non-blocking and we need
	// to wait at most nbm_wait seconds to lose focus to the launched program.
	// If it loses focus, we continue waiting until it returns
	//
	if ( nbm_wait > 0 )
	{
		FeDebug() << "Non-Blocking Wait Mode: nb_mode_wait=" << nbm_wait << " seconds, waiting..." << std::endl;
		bool done_wait=false, has_focus=false;

		while ( !done_wait && isOpen() )
		{
			while ( const std::optional ev = pollEvent() )
			{
				if ( ev.has_value() && ev->is<sf::Event::Closed>() )
					return false;
			}

#if defined(SFML_SYSTEM_WINDOWS)
			has_focus = hasFocus() || m_blackout.hasFocus();
#else
			has_focus = hasFocus();
#endif

			if (( timer.getElapsedTime() >= sf::seconds( nbm_wait ) )
				&& ( has_focus ))
			{
				FeDebug() << "Attract-Mode Plus has focus, stopped non-blocking wait after "
					<< timer.getElapsedTime().asSeconds() << "s" << std::endl;

				done_wait = true;
			}
			else
				sf::sleep( sf::milliseconds( 25 ) );
		}
	}

	if ( m_fes.update_stats_current( 1, timer.getElapsedTime().asSeconds() ) )
	{
		FePresent *fep = FePresent::script_get_fep();
		fep->update_to( ToNewList, false );
		fep->on_transition( ToNewList, 0 );
	}

#if defined(SFML_SYSTEM_LINUX)
	if ( m_fes.get_window_mode() == FeSettings::Fullscreen )
	{
 #if defined(USE_XLIB)
		//
		// On X11 Linux fullscreen we might have forcibly closed our window after launching the
		// emulator. Recreate it now if we did.
		//
		// Note that simply hiding and then showing the window again doesn't work right... focus
		// doesn't come back
		//
		if ( !isOpen() )
			initial_create();
 #else
		initial_create(); // On raspberry pi or with DRM, we have forcibly closed the window, so recreate it now
 #endif
	}
 #if defined(USE_XLIB)
	set_x11_foreground_window( m_window->getNativeHandle() );
 #endif

#elif defined(SFML_SYSTEM_MACOS)
	osx_take_focus();
#elif defined(SFML_SYSTEM_WINDOWS)
	if ( m_win_mode == FeSettings::Fullscreen )
	{
		m_blackout.display();
		m_window->setVisible( true );

		// Since we are double/triple buffering in fullscreen
		// we need to clear the frames rendered ahead
		// to avoid back buffer flashing on game launch/exit
		for ( int i = 0; i < 3; i++ )
		{
			clear();
			display();
		}
		set_win32_foreground_window( m_window->getNativeHandle(), HWND_TOP );
	}
	else
		set_win32_foreground_window( m_window->getNativeHandle(), HWND_TOP );
#endif

	if ( !windowed && m_fes.get_info_bool( FeSettings::MoveMouseOnLaunch ) )
		sf::Mouse::setPosition( reset_pos );

	// Empty the window event queue, so we don't go triggering other
	// right away after running an emulator

	while ( const std::optional ev = pollEvent() )
	{
		if ( ev->is<sf::Event::Closed>() )
			return false;
	}

	FeDebug() << "Resuming frontend after game launch" << std::endl;

	return true;
}

void FeWindow::on_exit()
{
#if defined(SFML_SYSTEM_WINDOWS)
	m_blackout.close();
#endif
}

bool FeWindow::has_running_process()
{
	return ( m_running_pid != 0 );
}

sf::RenderWindow &FeWindow::get_win()
{
	if ( !m_window )
		FeLog() << "FeWindow::get_win() on NULL window!" << std::endl;

	return *m_window;
}

void FeWindow::save()
{
	if ( m_window && is_windowed_mode( m_win_mode ) && !m_win_pos.m_temporary )
	{
		m_window->display(); // Crashing on Linux workaround

		FeWindowPosition win_pos( m_window->getPosition(), m_window->getSize() );
		win_pos.save( m_fes.get_config_dir() + FE_CFG_SUBDIR + FE_WINDOW_FILE );
	}
}

void FeWindow::close()
{
	if ( m_window )
	{
		m_window->display(); // Crashing on Linux workaround
		save();
		m_window->close();
	}
}

bool FeWindow::hasFocus()
{
	if ( m_window )
		return m_window->hasFocus();

	return false;
}

bool FeWindow::isOpen()
{
	if ( m_window )
		return m_window->isOpen();

	return false;
}

void FeWindow::clear()
{
	if ( m_window )
		m_window->clear();
}

void FeWindow::draw( const sf::Drawable &d, const sf::RenderStates &r )
{
	if ( m_window )
		m_window->draw( d, r );
}

const std::optional<sf::Event> FeWindow::pollEvent()
{
	return m_window->pollEvent();
}
