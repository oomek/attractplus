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
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include "nowide/fstream.hpp"

#include <SFML/System/Sleep.hpp>

#ifdef USE_SDL3_GPU
namespace
{
	char32_t decode_utf8_first_codepoint( const char *text )
	{
		if ( !text )
			return 0;

		const unsigned char *bytes = reinterpret_cast<const unsigned char *>( text );
		if ( bytes[0] == 0 )
			return 0;

		if ( ( bytes[0] & 0x80 ) == 0 )
			return bytes[0];

		if ( ( bytes[0] & 0xE0 ) == 0xC0 && bytes[1] != 0 )
			return ( ( bytes[0] & 0x1F ) << 6 ) | ( bytes[1] & 0x3F );

		if ( ( bytes[0] & 0xF0 ) == 0xE0 && bytes[1] != 0 && bytes[2] != 0 )
			return ( ( bytes[0] & 0x0F ) << 12 ) | ( ( bytes[1] & 0x3F ) << 6 ) | ( bytes[2] & 0x3F );

		if ( ( bytes[0] & 0xF8 ) == 0xF0 && bytes[1] != 0 && bytes[2] != 0 && bytes[3] != 0 )
			return ( ( bytes[0] & 0x07 ) << 18 ) | ( ( bytes[1] & 0x3F ) << 12 ) | ( ( bytes[2] & 0x3F ) << 6 ) | ( bytes[3] & 0x3F );

		return 0;
	}

	sf::Keyboard::Key sdl_scancode_to_sf_key( SDL_Scancode scancode )
	{
		switch ( scancode )
		{
		case SDL_SCANCODE_A: return sf::Keyboard::Key::A;
		case SDL_SCANCODE_B: return sf::Keyboard::Key::B;
		case SDL_SCANCODE_C: return sf::Keyboard::Key::C;
		case SDL_SCANCODE_D: return sf::Keyboard::Key::D;
		case SDL_SCANCODE_E: return sf::Keyboard::Key::E;
		case SDL_SCANCODE_F: return sf::Keyboard::Key::F;
		case SDL_SCANCODE_G: return sf::Keyboard::Key::G;
		case SDL_SCANCODE_H: return sf::Keyboard::Key::H;
		case SDL_SCANCODE_I: return sf::Keyboard::Key::I;
		case SDL_SCANCODE_J: return sf::Keyboard::Key::J;
		case SDL_SCANCODE_K: return sf::Keyboard::Key::K;
		case SDL_SCANCODE_L: return sf::Keyboard::Key::L;
		case SDL_SCANCODE_M: return sf::Keyboard::Key::M;
		case SDL_SCANCODE_N: return sf::Keyboard::Key::N;
		case SDL_SCANCODE_O: return sf::Keyboard::Key::O;
		case SDL_SCANCODE_P: return sf::Keyboard::Key::P;
		case SDL_SCANCODE_Q: return sf::Keyboard::Key::Q;
		case SDL_SCANCODE_R: return sf::Keyboard::Key::R;
		case SDL_SCANCODE_S: return sf::Keyboard::Key::S;
		case SDL_SCANCODE_T: return sf::Keyboard::Key::T;
		case SDL_SCANCODE_U: return sf::Keyboard::Key::U;
		case SDL_SCANCODE_V: return sf::Keyboard::Key::V;
		case SDL_SCANCODE_W: return sf::Keyboard::Key::W;
		case SDL_SCANCODE_X: return sf::Keyboard::Key::X;
		case SDL_SCANCODE_Y: return sf::Keyboard::Key::Y;
		case SDL_SCANCODE_Z: return sf::Keyboard::Key::Z;
		case SDL_SCANCODE_1: return sf::Keyboard::Key::Num1;
		case SDL_SCANCODE_2: return sf::Keyboard::Key::Num2;
		case SDL_SCANCODE_3: return sf::Keyboard::Key::Num3;
		case SDL_SCANCODE_4: return sf::Keyboard::Key::Num4;
		case SDL_SCANCODE_5: return sf::Keyboard::Key::Num5;
		case SDL_SCANCODE_6: return sf::Keyboard::Key::Num6;
		case SDL_SCANCODE_7: return sf::Keyboard::Key::Num7;
		case SDL_SCANCODE_8: return sf::Keyboard::Key::Num8;
		case SDL_SCANCODE_9: return sf::Keyboard::Key::Num9;
		case SDL_SCANCODE_0: return sf::Keyboard::Key::Num0;
		case SDL_SCANCODE_ESCAPE: return sf::Keyboard::Key::Escape;
		case SDL_SCANCODE_LCTRL: return sf::Keyboard::Key::LControl;
		case SDL_SCANCODE_LSHIFT: return sf::Keyboard::Key::LShift;
		case SDL_SCANCODE_LALT: return sf::Keyboard::Key::LAlt;
		case SDL_SCANCODE_LGUI: return sf::Keyboard::Key::LSystem;
		case SDL_SCANCODE_RCTRL: return sf::Keyboard::Key::RControl;
		case SDL_SCANCODE_RSHIFT: return sf::Keyboard::Key::RShift;
		case SDL_SCANCODE_RALT: return sf::Keyboard::Key::RAlt;
		case SDL_SCANCODE_RGUI: return sf::Keyboard::Key::RSystem;
		case SDL_SCANCODE_MENU: return sf::Keyboard::Key::Menu;
		case SDL_SCANCODE_LEFTBRACKET: return sf::Keyboard::Key::LBracket;
		case SDL_SCANCODE_RIGHTBRACKET: return sf::Keyboard::Key::RBracket;
		case SDL_SCANCODE_SEMICOLON: return sf::Keyboard::Key::Semicolon;
		case SDL_SCANCODE_COMMA: return sf::Keyboard::Key::Comma;
		case SDL_SCANCODE_PERIOD: return sf::Keyboard::Key::Period;
		case SDL_SCANCODE_APOSTROPHE: return sf::Keyboard::Key::Apostrophe;
		case SDL_SCANCODE_SLASH: return sf::Keyboard::Key::Slash;
		case SDL_SCANCODE_BACKSLASH: return sf::Keyboard::Key::Backslash;
		case SDL_SCANCODE_GRAVE: return sf::Keyboard::Key::Grave;
		case SDL_SCANCODE_EQUALS: return sf::Keyboard::Key::Equal;
		case SDL_SCANCODE_MINUS: return sf::Keyboard::Key::Hyphen;
		case SDL_SCANCODE_SPACE: return sf::Keyboard::Key::Space;
		case SDL_SCANCODE_RETURN: return sf::Keyboard::Key::Enter;
		case SDL_SCANCODE_BACKSPACE: return sf::Keyboard::Key::Backspace;
		case SDL_SCANCODE_TAB: return sf::Keyboard::Key::Tab;
		case SDL_SCANCODE_PAGEUP: return sf::Keyboard::Key::PageUp;
		case SDL_SCANCODE_PAGEDOWN: return sf::Keyboard::Key::PageDown;
		case SDL_SCANCODE_END: return sf::Keyboard::Key::End;
		case SDL_SCANCODE_HOME: return sf::Keyboard::Key::Home;
		case SDL_SCANCODE_INSERT: return sf::Keyboard::Key::Insert;
		case SDL_SCANCODE_DELETE: return sf::Keyboard::Key::Delete;
		case SDL_SCANCODE_KP_PLUS: return sf::Keyboard::Key::Add;
		case SDL_SCANCODE_KP_MINUS: return sf::Keyboard::Key::Subtract;
		case SDL_SCANCODE_KP_MULTIPLY: return sf::Keyboard::Key::Multiply;
		case SDL_SCANCODE_KP_DIVIDE: return sf::Keyboard::Key::Divide;
		case SDL_SCANCODE_LEFT: return sf::Keyboard::Key::Left;
		case SDL_SCANCODE_RIGHT: return sf::Keyboard::Key::Right;
		case SDL_SCANCODE_UP: return sf::Keyboard::Key::Up;
		case SDL_SCANCODE_DOWN: return sf::Keyboard::Key::Down;
		case SDL_SCANCODE_KP_0: return sf::Keyboard::Key::Numpad0;
		case SDL_SCANCODE_KP_1: return sf::Keyboard::Key::Numpad1;
		case SDL_SCANCODE_KP_2: return sf::Keyboard::Key::Numpad2;
		case SDL_SCANCODE_KP_3: return sf::Keyboard::Key::Numpad3;
		case SDL_SCANCODE_KP_4: return sf::Keyboard::Key::Numpad4;
		case SDL_SCANCODE_KP_5: return sf::Keyboard::Key::Numpad5;
		case SDL_SCANCODE_KP_6: return sf::Keyboard::Key::Numpad6;
		case SDL_SCANCODE_KP_7: return sf::Keyboard::Key::Numpad7;
		case SDL_SCANCODE_KP_8: return sf::Keyboard::Key::Numpad8;
		case SDL_SCANCODE_KP_9: return sf::Keyboard::Key::Numpad9;
		case SDL_SCANCODE_F1: return sf::Keyboard::Key::F1;
		case SDL_SCANCODE_F2: return sf::Keyboard::Key::F2;
		case SDL_SCANCODE_F3: return sf::Keyboard::Key::F3;
		case SDL_SCANCODE_F4: return sf::Keyboard::Key::F4;
		case SDL_SCANCODE_F5: return sf::Keyboard::Key::F5;
		case SDL_SCANCODE_F6: return sf::Keyboard::Key::F6;
		case SDL_SCANCODE_F7: return sf::Keyboard::Key::F7;
		case SDL_SCANCODE_F8: return sf::Keyboard::Key::F8;
		case SDL_SCANCODE_F9: return sf::Keyboard::Key::F9;
		case SDL_SCANCODE_F10: return sf::Keyboard::Key::F10;
		case SDL_SCANCODE_F11: return sf::Keyboard::Key::F11;
		case SDL_SCANCODE_F12: return sf::Keyboard::Key::F12;
		case SDL_SCANCODE_F13: return sf::Keyboard::Key::F13;
		case SDL_SCANCODE_F14: return sf::Keyboard::Key::F14;
		case SDL_SCANCODE_F15: return sf::Keyboard::Key::F15;
		case SDL_SCANCODE_PAUSE: return sf::Keyboard::Key::Pause;
		default: return sf::Keyboard::Key::Unknown;
		}
	}

	sf::Mouse::Button sdl_mouse_button_to_sf( Uint8 button )
	{
		switch ( button )
		{
		case SDL_BUTTON_LEFT: return sf::Mouse::Button::Left;
		case SDL_BUTTON_RIGHT: return sf::Mouse::Button::Right;
		case SDL_BUTTON_MIDDLE: return sf::Mouse::Button::Middle;
		case SDL_BUTTON_X1: return sf::Mouse::Button::Extra1;
		case SDL_BUTTON_X2: return sf::Mouse::Button::Extra2;
		default: return sf::Mouse::Button::Left;
		}
	}

	sf::Joystick::Axis sdl_joystick_axis_to_sf( Uint8 axis )
	{
		switch ( axis )
		{
		case 0: return sf::Joystick::Axis::X;
		case 1: return sf::Joystick::Axis::Y;
		case 2: return sf::Joystick::Axis::Z;
		case 3: return sf::Joystick::Axis::R;
		case 4: return sf::Joystick::Axis::U;
		case 5: return sf::Joystick::Axis::V;
		case 6: return sf::Joystick::Axis::PovX;
		case 7: return sf::Joystick::Axis::PovY;
		default: return sf::Joystick::Axis::X;
		}
	}

	bool sdl_event_targets_window( const SDL_Event &event, Uint32 window_id )
	{
		switch ( event.type )
		{
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		case SDL_EVENT_WINDOW_FOCUS_GAINED:
		case SDL_EVENT_WINDOW_FOCUS_LOST:
			return event.window.windowID == window_id;
		case SDL_EVENT_TEXT_INPUT:
			return event.text.windowID == window_id;
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
			return event.key.windowID == window_id;
		case SDL_EVENT_MOUSE_MOTION:
			return event.motion.windowID == window_id;
		case SDL_EVENT_MOUSE_WHEEL:
			return event.wheel.windowID == window_id;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			return event.button.windowID == window_id;
		default:
			return true;
		}
	}

	std::optional<sf::Event> translate_sdl_event( const SDL_Event &event, SDL_Window *window )
	{
		if ( !window )
			return {};

		const Uint32 window_id = SDL_GetWindowID( window );
		if ( window_id == 0 )
			return {};

		if ( !sdl_event_targets_window( event, window_id ) )
			return {};

		switch ( event.type )
		{
		case SDL_EVENT_QUIT:
			return sf::Event::Closed{};

		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			return sf::Event::Closed{};

		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			return sf::Event::FocusGained{};

		case SDL_EVENT_WINDOW_FOCUS_LOST:
			return sf::Event::FocusLost{};

		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			return sf::Event::Resized{ sf::Vector2u( static_cast<unsigned int>( event.window.data1 ),
				static_cast<unsigned int>( event.window.data2 ) ) };

		case SDL_EVENT_TEXT_INPUT:
		{
			const char32_t unicode = decode_utf8_first_codepoint( event.text.text );
			if ( unicode != 0 )
				return sf::Event::TextEntered{ unicode };
			return {};
		}

		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
		{
			if ( event.key.repeat )
				return {};

			const SDL_Keymod mods = SDL_GetModState();
			const auto key = sdl_scancode_to_sf_key( event.key.scancode );
			const bool alt = ( mods & SDL_KMOD_ALT ) != 0;
			const bool control = ( mods & SDL_KMOD_CTRL ) != 0;
			const bool shift = ( mods & SDL_KMOD_SHIFT ) != 0;
			const bool system = ( mods & SDL_KMOD_GUI ) != 0;

			if ( event.type == SDL_EVENT_KEY_DOWN )
				return sf::Event::KeyPressed{ key, sf::Keyboard::Scancode::Unknown, alt, control, shift, system };

			return sf::Event::KeyReleased{ key, sf::Keyboard::Scancode::Unknown, alt, control, shift, system };
		}

		case SDL_EVENT_MOUSE_MOTION:
			return sf::Event::MouseMoved{ sf::Vector2i( static_cast<int>( event.motion.x ), static_cast<int>( event.motion.y ) ) };

		case SDL_EVENT_MOUSE_WHEEL:
		{
			const sf::Mouse::Wheel wheel = ( event.wheel.x != 0.0f ) ? sf::Mouse::Wheel::Horizontal : sf::Mouse::Wheel::Vertical;
			const float delta = ( event.wheel.x != 0.0f ) ? event.wheel.x : event.wheel.y;
			return sf::Event::MouseWheelScrolled{
				wheel,
				delta,
				sf::Vector2i( static_cast<int>( event.wheel.mouse_x ), static_cast<int>( event.wheel.mouse_y ) )
			};
		}

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			return sf::Event::MouseButtonPressed{
				sdl_mouse_button_to_sf( event.button.button ),
				sf::Vector2i( static_cast<int>( event.button.x ), static_cast<int>( event.button.y ) )
			};

		case SDL_EVENT_MOUSE_BUTTON_UP:
			return sf::Event::MouseButtonReleased{
				sdl_mouse_button_to_sf( event.button.button ),
				sf::Vector2i( static_cast<int>( event.button.x ), static_cast<int>( event.button.y ) )
			};

		case SDL_EVENT_JOYSTICK_AXIS_MOTION:
			return sf::Event::JoystickMoved{
				static_cast<unsigned int>( event.jaxis.which ),
				sdl_joystick_axis_to_sf( event.jaxis.axis ),
				static_cast<float>( event.jaxis.value ) * ( 100.0f / 32767.0f )
			};

		case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
			return sf::Event::JoystickButtonPressed{
				static_cast<unsigned int>( event.jbutton.which ),
				static_cast<unsigned int>( event.jbutton.button )
			};

		case SDL_EVENT_JOYSTICK_BUTTON_UP:
			return sf::Event::JoystickButtonReleased{
				static_cast<unsigned int>( event.jbutton.which ),
				static_cast<unsigned int>( event.jbutton.button )
			};

		case SDL_EVENT_JOYSTICK_ADDED:
			return sf::Event::JoystickConnected{ static_cast<unsigned int>( event.jdevice.which ) };

		case SDL_EVENT_JOYSTICK_REMOVED:
			return sf::Event::JoystickDisconnected{ static_cast<unsigned int>( event.jdevice.which ) };

		default:
			return {};
		}
	}

	std::string get_gpu_log_path()
	{
		const std::string base_path = get_program_path();
		if ( base_path.empty() )
			return "fe_sdl3_gpu.log";

		std::string log_path( base_path );
		const char last = log_path[ log_path.size() - 1 ];
		if ( last != '/' && last != '\\' )
			log_path += '/';
		log_path += "fe_sdl3_gpu.log";
		return log_path;
	}

	void append_gpu_probe_log( const std::string &message )
	{
		static bool s_truncated_gpu_log = false;
		const std::ios_base::openmode mode = s_truncated_gpu_log ? std::ios::app : std::ios::trunc;
		std::ofstream log_file( get_gpu_log_path().c_str(), mode );
		if ( log_file )
		{
			log_file << message << std::endl;
			s_truncated_gpu_log = true;
		}
	}

	bool has_gpu_present_marker()
	{
		if ( std::ifstream( "fe_sdl3_gpu_present.txt" ) )
			return true;

		const std::string base_path = get_program_path();
		if ( !base_path.empty() )
		{
			std::string marker_path( base_path );
			const char last = marker_path[ marker_path.size() - 1 ];
			if ( last != '/' && last != '\\' )
				marker_path += '/';
			marker_path += "fe_sdl3_gpu_present.txt";
			return static_cast<bool>( std::ifstream( marker_path.c_str() ) );
		}

		return false;
	}

	bool gpu_present_requested()
	{
		return ( std::getenv( "FE_SDL3_GPU_PRESENT" ) != nullptr ) || has_gpu_present_marker();
	}
}
#endif

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
	m_size({ 0, 0 }),
	m_temporary( false )
{}

FeWindowPosition::FeWindowPosition(
	const sf::Vector2i &pos,
	const sf::Vector2u &size
):
	m_pos( pos ),
	m_size( size ),
	m_temporary( false )
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

sf::RenderWindow *FeWindow::ensure_legacy_window()
{
	if ( m_window )
		return m_window;

	if ( !m_sdl_window_owned )
		return nullptr;

#ifdef USE_SDL3_GPU
	void *native_handle = m_gpu_context.get_native_window_handle();
	if ( !native_handle )
		return nullptr;

	m_window = new sf::RenderWindow();
	m_window->create( static_cast<sf::WindowHandle>( native_handle ), m_legacy_window_context );
	if ( m_legacy_view_set )
		m_window->setView( m_legacy_view );
	return m_window;
#else
	return nullptr;
#endif
}

void FeWindow::display()
{
	bool used_sdl_gpu_present = false;
#ifdef USE_SDL3_GPU
	const bool s_enable_gpu_present = gpu_present_requested();
	static bool s_logged_gpu_present_probe = false;
	if ( !s_logged_gpu_present_probe )
	{
		std::ostringstream stream;
		stream
			<< "display: gpu_present_requested=" << ( s_enable_gpu_present ? 1 : 0 )
			<< " env=" << ( std::getenv( "FE_SDL3_GPU_PRESENT" ) ? 1 : 0 )
			<< " marker=" << ( has_gpu_present_marker() ? 1 : 0 )
			<< " program_path=" << get_program_path();
		append_gpu_probe_log( stream.str() );
		s_logged_gpu_present_probe = true;
	}
	if ( s_enable_gpu_present )
	{
		static int s_logged_submitted = -1;
		static int s_logged_content = -1;
		static int s_logged_available = -1;
		const int submitted = m_gpu_context.has_submitted_frame() ? 1 : 0;
		const int content = m_gpu_context.has_frame_content() ? 1 : 0;
		const int available = m_gpu_context.is_available() ? 1 : 0;
		if ( submitted != s_logged_submitted || content != s_logged_content || available != s_logged_available )
		{
			std::ostringstream stream;
			stream
				<< "display: gpu_state"
				<< " submitted=" << submitted
				<< " content=" << content
				<< " available=" << available;
			append_gpu_probe_log( stream.str() );
			s_logged_submitted = submitted;
			s_logged_content = content;
			s_logged_available = available;
		}
#if defined(SFML_SYSTEM_WINDOWS)
		if ( !m_sdl_window_owned && !m_gpu_context.is_available() && m_window && m_gpu_context.has_submitted_frame() && m_gpu_context.has_frame_content() )
		{
			const sf::Vector2u size = m_window->getSize();
			if ( !m_gpu_context.wrap_native_window(
					reinterpret_cast<void *>( m_window->getNativeHandle() ),
					static_cast<int>( size.x ),
					static_cast<int>( size.y ) ) )
			{
				FeLog() << "WARNING: SDL3 GPU backend initialization failed: " << SDL_GetError() << std::endl;
			}
			else
			{
				FeDebug() << "SDL3 GPU backend initialized for the frontend window." << std::endl;
			}
		}
#endif
		if ( m_gpu_context.should_present() )
			used_sdl_gpu_present = m_gpu_context.execute_frame();
	}
#endif

	if ( m_legacy_frame_drawn && m_window )
	{
		m_window->display();
		used_sdl_gpu_present = true;
	}
	else if ( !used_sdl_gpu_present && m_window )
		m_window->display();

	m_legacy_clear_requested = false;
	m_legacy_frame_drawn = false;
	m_deferred_drawable = nullptr;
	m_deferred_drawable_states = sf::RenderStates::Default;

	// Starting from Windows Vista non fullscreen window modes
	// should be synced by DWM, instead of v-sync
#ifdef SFML_SYSTEM_WINDOWS
	if ( !used_sdl_gpu_present && ( m_win_mode != FeSettings::Fullscreen ) )
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

#ifdef USE_SDL3_GPU
	if ( m_sdl_window_owned )
	{
		delete m_window;
		m_window = NULL;
		m_gpu_context.release_window();
		m_sdl_window_owned = false;
	}
#endif

	if ( !m_window )
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

	sf::Vector2u screen_size = vm.size;

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
		bool vert = vm.size.y > vm.size.x;
		int h = std::max( (int)vm.size.y * 3 / 4, 240 );
		int w = std::max( vert ? h * 3 / 4 : h * 4 / 3, 320 );
		int x = (vm.size.x - w) / 2;
		int y = (vm.size.y - h) / 2;

		// Load the parameters from the window.am file (uses given args if none)
		if ( !m_win_pos.m_temporary )
		{
			m_win_pos.m_pos = sf::Vector2i( x, y );
			m_win_pos.m_size = sf::Vector2u( w, h );
			m_win_pos.load_from_file( m_fes.get_config_dir() + FE_CFG_SUBDIR + FE_WINDOW_FILE );
		}

		// Minimum window size (1x1) prevents (0x0) crash (may be set in window.am or commandline)
		m_win_pos.m_size.x = std::max( m_win_pos.m_size.x, (unsigned int)1 );
		m_win_pos.m_size.y = std::max( m_win_pos.m_size.y, (unsigned int)1 );

		sf::Rect<int> window_rect( m_win_pos.m_pos, sf::Vector2i( m_win_pos.m_size ) );
#if !defined(NO_MULTIMON) && defined(SFML_SYSTEM_WINDOWS)
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
	if (( m_win_mode == FeSettings::WindowNoBorder ) && ( screen_size == vm.size ))
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
	m_legacy_window_context = ctx;

	bool use_sdl_owned_window = false;
#if defined(USE_SDL3_GPU) && defined(SFML_SYSTEM_WINDOWS)
	use_sdl_owned_window = gpu_present_requested() && ( m_win_mode != FeSettings::Fullscreen );
#endif

	if ( use_sdl_owned_window )
	{
#if defined(USE_SDL3_GPU) && defined(SFML_SYSTEM_WINDOWS)
		delete m_window;
		m_window = nullptr;

		if ( !m_gpu_context.initialize() )
			FeLog() << "WARNING: SDL3 GPU backend initialization failed before SDL window creation: " << SDL_GetError() << std::endl;
		else
		{
			SDL_WindowFlags flags = SDL_WINDOW_HIDDEN;
			if ( m_win_mode == FeSettings::Window )
				flags = static_cast<SDL_WindowFlags>( flags | SDL_WINDOW_RESIZABLE );
			if ( m_win_mode != FeSettings::Window )
				flags = static_cast<SDL_WindowFlags>( flags | SDL_WINDOW_BORDERLESS );

			SDL_Window *sdl_window = SDL_CreateWindow( FE_NAME, static_cast<int>( vm.size.x ), static_cast<int>( vm.size.y ), flags );
			if ( !sdl_window )
			{
				FeLog() << "WARNING: SDL3 GPU SDL window creation failed: " << SDL_GetError() << std::endl;
			}
			else
			{
				SDL_SetWindowPosition( sdl_window, wpos.x, wpos.y );
				if ( !m_gpu_context.claim_window( sdl_window ) )
				{
					FeLog() << "WARNING: SDL3 GPU window claim failed: " << SDL_GetError() << std::endl;
					SDL_DestroyWindow( sdl_window );
				}
				else
				{
					void *native_handle = m_gpu_context.get_native_window_handle();
					if ( !native_handle )
					{
						FeLog() << "WARNING: SDL3 GPU native window handle lookup failed." << std::endl;
						m_gpu_context.release_window();
					}
					else
					{
						m_sdl_window_owned = true;
						SDL_ShowWindow( sdl_window );
					}
				}
			}
		}
#endif
	}

	if ( !m_sdl_window_owned )
		m_window->create( vm, FE_NAME, style_map[ m_win_mode ], state_map[ m_win_mode ], ctx );

	// On Windows Vista and above all non fullscreen window modes
	// go through DWM. We have to disable vsync
	// when we rely solely on DwmFlush()
#if defined(SFML_SYSTEM_WINDOWS)
	if ( m_window )
		m_window->setVerticalSyncEnabled( m_win_mode == FeSettings::Fullscreen );
#else
	if ( m_window )
		m_window->setVerticalSyncEnabled(true);
#endif
	if ( m_window )
	{
		m_window->setKeyRepeatEnabled(false);
		m_window->setMouseCursorVisible( is_windowed_mode( m_win_mode ));
		m_window->setJoystickThreshold( 1.0 );
	}

#ifndef SFML_SYSTEM_MACOS
    sf::Image icon;
    if ( m_window && icon.loadFromMemory( attractplus_icon, sizeof( attractplus_icon )))
    	m_window->setIcon({ icon.getSize().y, icon.getSize().y }, icon.getPixelsPtr() );
#endif
	// We need to clear and display here before calling setSize and setPosition
	// to avoid a white window flash on launch.
	if ( m_window )
	{
		clear();
		display();
	}

#ifdef SFML_SYSTEM_MACOS
	if ( m_win_mode == FeSettings::Fillscreen )
	{
		// note ordering req: pretty sure this needs to be before the setPosition() call below
		osx_hide_menu_bar();
	}
#endif

#if defined(USE_XLIB)
	if ( m_win_mode == FeSettings::Fillscreen )
	{
		if ( m_window )
			set_x11_fullscreen_state( m_window->getNativeHandle() );
	}
#endif

	// Known issue: Linux Mint 18.3 Cinnamon w/ SFML 2.5.1, position isn't being set
	// (Window always winds up at 0,0)
	// There is currently no way to create window at position, or create hidden then reveal
	if ( m_window )
		m_window->setPosition( wpos );

	FeDebug() << "Created " << FE_NAME << " Window: " << wsize.x << "x" << wsize.y << " @ "
		<< wpos.x << "," << wpos.y << " [OpenGL surface: "
		<< vm.size.x << "x" << vm.size.y << " bpp=" << vm.bitsPerPixel << "]" << std::endl;

#if defined(SFML_SYSTEM_WINDOWS)
	HWND hwnd = nullptr;
	if ( m_sdl_window_owned )
		hwnd = static_cast<HWND>( m_gpu_context.get_native_window_handle() );
	else if ( m_window )
		hwnd = static_cast<HWND>( m_window->getNativeHandle() );
	if ( hwnd )
	{
		set_win32_foreground_window( hwnd, m_fes.get_window_topmost() ? HWND_TOPMOST : HWND_TOP );

		// Enable dark mode titlebar on Windows
		BOOL value = TRUE;
		DwmSetWindowAttribute( hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof( value ));

		s_sfml_wnd_proc = reinterpret_cast<WNDPROC>( GetWindowLongPtr( hwnd, GWLP_WNDPROC ));
		SetWindowLongPtr( hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( CustomWndProc ));

		// Trigger title bar redraw to fix Win10 dark-mode (initially draws in light-mode)
		if ( m_window )
			display();
		SendMessage(hwnd, WM_NCACTIVATE, FALSE, 0);
		SendMessage(hwnd, WM_NCACTIVATE, TRUE, 0);
	}
#endif

	m_fes.init_mouse_capture( this );

	// Only mess with the mouse position if mouse moves mapped
	if ( m_fes.has_mouse_moves() )
		set_mouse_position( sf::Vector2i( wsize / 2 ) );
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

		sf::Vector2i hide_pos = get_position();
		hide_pos.x += static_cast<int>( get_size().x ) - 1;
		hide_pos.y += static_cast<int>( get_size().y ) - 1;

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
		if ( m_window )
			set_win32_foreground_window( m_window->getNativeHandle(), HWND_BOTTOM );
		m_blackout.display();
		if ( m_window )
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
	if ( m_window )
		set_x11_foreground_window( m_window->getNativeHandle() );
 #endif

#elif defined(SFML_SYSTEM_MACOS)
	osx_take_focus();
#elif defined(SFML_SYSTEM_WINDOWS)
	if ( m_win_mode == FeSettings::Fullscreen )
	{
		m_blackout.display();
		if ( m_window )
			m_window->setVisible( true );

		// Since we are double/triple buffering in fullscreen
		// we need to clear the frames rendered ahead
		// to avoid back buffer flashing on game launch/exit
		for ( int i = 0; i < 3; i++ )
		{
			clear();
			display();
		}
		if ( m_window )
			set_win32_foreground_window( m_window->getNativeHandle(), HWND_TOP );
	}
	else
	{
		if ( m_window )
			set_win32_foreground_window( m_window->getNativeHandle(), HWND_TOP );
	}
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
	if ( !ensure_legacy_window() )
		FeLog() << "FeWindow::get_win() on NULL window!" << std::endl;

	return *m_window;
}

void FeWindow::save()
{
	if ( is_windowed_mode( m_win_mode ) && !m_win_pos.m_temporary )
	{
		if ( m_sdl_window_owned )
		{
#ifdef USE_SDL3_GPU
			if ( SDL_Window *window = m_gpu_context.get_window() )
			{
				int x = 0;
				int y = 0;
				int w = 0;
				int h = 0;
				SDL_GetWindowPosition( window, &x, &y );
				SDL_GetWindowSize( window, &w, &h );
				FeWindowPosition win_pos( sf::Vector2i( x, y ), sf::Vector2u( static_cast<unsigned int>( w ), static_cast<unsigned int>( h ) ) );
				win_pos.save( m_fes.get_config_dir() + FE_CFG_SUBDIR + FE_WINDOW_FILE );
			}
#endif
		}
		else if ( m_window )
		{
			m_window->display(); // Crashing on Linux workaround
			FeWindowPosition win_pos( m_window->getPosition(), m_window->getSize() );
			win_pos.save( m_fes.get_config_dir() + FE_CFG_SUBDIR + FE_WINDOW_FILE );
		}
	}
}

void FeWindow::close()
{
	if ( m_sdl_window_owned )
	{
		save();
		if ( m_window )
		{
			delete m_window;
			m_window = NULL;
		}

#ifdef USE_SDL3_GPU
		m_gpu_context.release_window();
#endif
		m_sdl_window_owned = false;
		return;
	}

	if ( m_window )
	{
		m_window->display(); // Crashing on Linux workaround
		save();
		m_window->close();
	}

#ifdef USE_SDL3_GPU
	m_gpu_context.release_window();
#endif
}

bool FeWindow::hasFocus()
{
#ifdef USE_SDL3_GPU
	if ( m_sdl_window_owned )
	{
		SDL_Window *window = m_gpu_context.get_window();
		if ( window )
			return SDL_GetKeyboardFocus() == window || SDL_GetMouseFocus() == window;
	}
#endif

	if ( m_window )
		return m_window->hasFocus();

	return false;
}

bool FeWindow::isOpen()
{
	if ( m_sdl_window_owned )
		return m_gpu_context.get_window() != nullptr;

	if ( m_window )
		return m_window->isOpen();

	return false;
}

sf::Vector2u FeWindow::get_size() const
{
#ifdef USE_SDL3_GPU
	if ( m_sdl_window_owned )
	{
		if ( SDL_Window *window = m_gpu_context.get_window() )
		{
			int width = 0;
			int height = 0;
			SDL_GetWindowSize( window, &width, &height );
			return sf::Vector2u( static_cast<unsigned int>( width ), static_cast<unsigned int>( height ) );
		}
	}
#endif

	if ( m_window )
		return m_window->getSize();

	return {};
}

sf::Vector2i FeWindow::get_position() const
{
#ifdef USE_SDL3_GPU
	if ( m_sdl_window_owned )
	{
		if ( SDL_Window *window = m_gpu_context.get_window() )
		{
			int x = 0;
			int y = 0;
			SDL_GetWindowPosition( window, &x, &y );
			return sf::Vector2i( x, y );
		}
	}
#endif

	if ( m_window )
		return m_window->getPosition();

	return {};
}

sf::Vector2i FeWindow::get_mouse_position() const
{
#ifdef USE_SDL3_GPU
	if ( m_sdl_window_owned )
	{
		if ( SDL_Window *window = m_gpu_context.get_window() )
		{
			float x = 0.0f;
			float y = 0.0f;
			SDL_GetMouseState( &x, &y );
			return sf::Vector2i( static_cast<int>( x ), static_cast<int>( y ) );
		}
	}
#endif

	if ( m_window )
		return sf::Mouse::getPosition( *m_window );

	return {};
}

void FeWindow::set_mouse_position( const sf::Vector2i &pos )
{
#ifdef USE_SDL3_GPU
	if ( m_sdl_window_owned )
	{
		if ( SDL_Window *window = m_gpu_context.get_window() )
		{
			SDL_WarpMouseInWindow( window, static_cast<float>( pos.x ), static_cast<float>( pos.y ) );
			return;
		}
	}
#endif

	if ( m_window )
		sf::Mouse::setPosition( pos, *m_window );
}

void FeWindow::set_key_repeat_enabled( bool enabled )
{
	if ( m_window )
		m_window->setKeyRepeatEnabled( enabled );
}

void FeWindow::set_mouse_cursor_visible( bool visible )
{
#ifdef USE_SDL3_GPU
	if ( m_sdl_window_owned )
	{
		if ( SDL_Window *window = m_gpu_context.get_window() )
		{
			visible ? SDL_ShowCursor() : SDL_HideCursor();
			return;
		}
	}
#endif

	if ( m_window )
		m_window->setMouseCursorVisible( visible );
}

void FeWindow::set_view( const sf::View &view )
{
	m_legacy_view = view;
	m_legacy_view_set = true;
	if ( m_window )
		m_window->setView( view );
}

bool FeWindow::save_screenshot( const std::string &filename )
{
#ifdef USE_SDL3_GPU
	if ( m_sdl_window_owned && m_gpu_context.save_screenshot( filename ) )
		return true;
#endif

	if ( sf::RenderWindow *window = ensure_legacy_window() )
	{
		sf::Texture texture;
		if ( texture.resize({ window->getSize().x, window->getSize().y }) )
		{
			texture.update( *window );
			return texture.copyToImage().saveToFile( filename );
		}
	}

	return false;
}

void FeWindow::clear()
{
	if ( m_sdl_window_owned )
	{
		m_legacy_clear_requested = true;
		if ( m_window )
			m_window->clear();
		return;
	}

	if ( m_window )
		m_window->clear();
}

void FeWindow::draw( const sf::Drawable &d, const sf::RenderStates &r )
{
	if ( m_sdl_window_owned )
	{
		if ( !m_window && dynamic_cast<const FePresent *>( &d ) )
		{
			m_deferred_drawable = &d;
			m_deferred_drawable_states = r;
			return;
		}

		if ( sf::RenderWindow *window = ensure_legacy_window() )
		{
			if ( m_legacy_clear_requested )
			{
				window->clear();
				m_legacy_clear_requested = false;
			}

			if ( m_deferred_drawable )
			{
				window->draw( *m_deferred_drawable, m_deferred_drawable_states );
				m_legacy_frame_drawn = true;
				m_deferred_drawable = nullptr;
				m_deferred_drawable_states = sf::RenderStates::Default;
			}

			window->draw( d, r );
			m_legacy_frame_drawn = true;
		}
		return;
	}

	if ( m_window )
		m_window->draw( d, r );
}

const std::optional<sf::Event> FeWindow::pollEvent()
{
#ifdef USE_SDL3_GPU
	if ( m_sdl_window_owned )
	{
		SDL_Event event;
		while ( SDL_PollEvent( &event ) )
		{
			if ( const auto translated = translate_sdl_event( event, m_gpu_context.get_window() ) )
				return translated;
		}

		return {};
	}
#endif

	if ( m_window )
		return m_window->pollEvent();

	return {};
}
