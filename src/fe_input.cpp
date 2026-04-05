/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-16 Andrew Mickelson
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

#include "fe_input.hpp"
#include "fe_util.hpp"
#include "fe_settings.hpp"
#include <iostream>
#include <algorithm>
#include <array>
#include <iomanip>
#include <limits.h>
#include <cstring>
#include <cmath>
#include <set>
#include <SDL3/SDL.h>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Touch.hpp>

namespace
{
	struct FeKeyInfo
	{
		SDL_Scancode scancode;
		const char *name;
	};

	const FeKeyInfo g_key_info[] =
	{
		{ SDL_SCANCODE_A, "A" },
		{ SDL_SCANCODE_B, "B" },
		{ SDL_SCANCODE_C, "C" },
		{ SDL_SCANCODE_D, "D" },
		{ SDL_SCANCODE_E, "E" },
		{ SDL_SCANCODE_F, "F" },
		{ SDL_SCANCODE_G, "G" },
		{ SDL_SCANCODE_H, "H" },
		{ SDL_SCANCODE_I, "I" },
		{ SDL_SCANCODE_J, "J" },
		{ SDL_SCANCODE_K, "K" },
		{ SDL_SCANCODE_L, "L" },
		{ SDL_SCANCODE_M, "M" },
		{ SDL_SCANCODE_N, "N" },
		{ SDL_SCANCODE_O, "O" },
		{ SDL_SCANCODE_P, "P" },
		{ SDL_SCANCODE_Q, "Q" },
		{ SDL_SCANCODE_R, "R" },
		{ SDL_SCANCODE_S, "S" },
		{ SDL_SCANCODE_T, "T" },
		{ SDL_SCANCODE_U, "U" },
		{ SDL_SCANCODE_V, "V" },
		{ SDL_SCANCODE_W, "W" },
		{ SDL_SCANCODE_X, "X" },
		{ SDL_SCANCODE_Y, "Y" },
		{ SDL_SCANCODE_Z, "Z" },
		{ SDL_SCANCODE_0, "Num0" },
		{ SDL_SCANCODE_1, "Num1" },
		{ SDL_SCANCODE_2, "Num2" },
		{ SDL_SCANCODE_3, "Num3" },
		{ SDL_SCANCODE_4, "Num4" },
		{ SDL_SCANCODE_5, "Num5" },
		{ SDL_SCANCODE_6, "Num6" },
		{ SDL_SCANCODE_7, "Num7" },
		{ SDL_SCANCODE_8, "Num8" },
		{ SDL_SCANCODE_9, "Num9" },
		{ SDL_SCANCODE_ESCAPE, "Escape" },
		{ SDL_SCANCODE_LCTRL, "LControl" },
		{ SDL_SCANCODE_LSHIFT, "LShift" },
		{ SDL_SCANCODE_LALT, "LAlt" },
		{ SDL_SCANCODE_LGUI, "LSystem" },
		{ SDL_SCANCODE_RCTRL, "RControl" },
		{ SDL_SCANCODE_RSHIFT, "RShift" },
		{ SDL_SCANCODE_RALT, "RAlt" },
		{ SDL_SCANCODE_RGUI, "RSystem" },
		{ SDL_SCANCODE_MENU, "Menu" },
		{ SDL_SCANCODE_LEFTBRACKET, "LBracket" },
		{ SDL_SCANCODE_RIGHTBRACKET, "RBracket" },
		{ SDL_SCANCODE_SEMICOLON, "Semicolon" },
		{ SDL_SCANCODE_COMMA, "Comma" },
		{ SDL_SCANCODE_PERIOD, "Period" },
		{ SDL_SCANCODE_APOSTROPHE, "Quote" },
		{ SDL_SCANCODE_SLASH, "Slash" },
		{ SDL_SCANCODE_BACKSLASH, "Backslash" },
		{ SDL_SCANCODE_GRAVE, "Tilde" },
		{ SDL_SCANCODE_EQUALS, "Equal" },
		{ SDL_SCANCODE_MINUS, "Dash" },
		{ SDL_SCANCODE_SPACE, "Space" },
		{ SDL_SCANCODE_RETURN, "Return" },
		{ SDL_SCANCODE_BACKSPACE, "Backspace" },
		{ SDL_SCANCODE_TAB, "Tab" },
		{ SDL_SCANCODE_PAGEUP, "PageUp" },
		{ SDL_SCANCODE_PAGEDOWN, "PageDown" },
		{ SDL_SCANCODE_END, "End" },
		{ SDL_SCANCODE_HOME, "Home" },
		{ SDL_SCANCODE_INSERT, "Insert" },
		{ SDL_SCANCODE_DELETE, "Delete" },
		{ SDL_SCANCODE_KP_PLUS, "Add" },
		{ SDL_SCANCODE_KP_MINUS, "Subtract" },
		{ SDL_SCANCODE_KP_MULTIPLY, "Multiply" },
		{ SDL_SCANCODE_KP_DIVIDE, "Divide" },
		{ SDL_SCANCODE_LEFT, "Left" },
		{ SDL_SCANCODE_RIGHT, "Right" },
		{ SDL_SCANCODE_UP, "Up" },
		{ SDL_SCANCODE_DOWN, "Down" },
		{ SDL_SCANCODE_KP_0, "Numpad0" },
		{ SDL_SCANCODE_KP_1, "Numpad1" },
		{ SDL_SCANCODE_KP_2, "Numpad2" },
		{ SDL_SCANCODE_KP_3, "Numpad3" },
		{ SDL_SCANCODE_KP_4, "Numpad4" },
		{ SDL_SCANCODE_KP_5, "Numpad5" },
		{ SDL_SCANCODE_KP_6, "Numpad6" },
		{ SDL_SCANCODE_KP_7, "Numpad7" },
		{ SDL_SCANCODE_KP_8, "Numpad8" },
		{ SDL_SCANCODE_KP_9, "Numpad9" },
		{ SDL_SCANCODE_F1, "F1" },
		{ SDL_SCANCODE_F2, "F2" },
		{ SDL_SCANCODE_F3, "F3" },
		{ SDL_SCANCODE_F4, "F4" },
		{ SDL_SCANCODE_F5, "F5" },
		{ SDL_SCANCODE_F6, "F6" },
		{ SDL_SCANCODE_F7, "F7" },
		{ SDL_SCANCODE_F8, "F8" },
		{ SDL_SCANCODE_F9, "F9" },
		{ SDL_SCANCODE_F10, "F10" },
		{ SDL_SCANCODE_F11, "F11" },
		{ SDL_SCANCODE_F12, "F12" },
		{ SDL_SCANCODE_F13, "F13" },
		{ SDL_SCANCODE_F14, "F14" },
		{ SDL_SCANCODE_F15, "F15" },
		{ SDL_SCANCODE_PAUSE, "Pause" },
		{ SDL_SCANCODE_UNKNOWN, nullptr }
	};

	// globals to track when last touch event "began"
	//
	std::optional<FeEvent> g_last_touch;
	bool g_touch_moved=false;

	static std::vector< int > g_joyfemap( FeJoystick::Count, 0 );
	int joymap2feid( int raw_id )
	{
		return g_joyfemap[ raw_id ];
	}

	int joymap2raw( int config_id )
	{
		for ( size_t i=0; i<FeJoystick::Count; i++ )
		{
			if ( g_joyfemap[i] == config_id )
				return i;
		}

		return 0;
	}

	void joy_raw_map_init( const std::vector < std::pair < int, std::string > > &joy_config )
	{
		size_t i=0;
		std::vector < std::pair < int, std::string > >::iterator itr;
		std::vector < std::pair < int, std::string  > > wl = joy_config;
		std::array<bool, FeJoystick::Count> mapped = {};

		// clear map structure
		for ( i=0; i< FeJoystick::Count; i++ )
		{
			g_joyfemap[i] = static_cast<int>( FeJoystick::Count ) - 1;
		}

		// initialize structure telling us whether a slot is already mapped by the user
		for ( itr = wl.begin(); itr != wl.end(); ++itr )
			mapped[ (*itr).first ] = true;

		fe_joystick_update();

		// update the map structure using supplied joy_config
		//
		for ( i=0; i< FeJoystick::Count; i++ )
		{
			if ( fe_joystick_is_connected( i ) )
			{
				std::string name = fe_joystick_get_name( i );

				for ( itr = wl.begin(); itr != wl.end(); ++itr )
				{
					if ( name.compare( (*itr).second ) == 0 )
					{
						g_joyfemap[i] = (*itr).first;
						wl.erase( itr );
						break;
					}
				}
			}
		}

		// Put any remaining unmapped joysticks in the lowest available "Default" slot
		for ( i=0; i< FeJoystick::Count; i++ )
		{
			if (( g_joyfemap[i] == static_cast<int>( FeJoystick::Count ) - 1 ) && ( fe_joystick_is_connected( i ) ))
			{
				for ( size_t j=0; j< FeJoystick::Count; j++ )
				{
					if ( !mapped[j] )
					{
						g_joyfemap[i] = j;
						mapped[j] = true;
						break;
					}
				}
			}
		}

		FeDebug() << "Joysticks after mapping: " << std::endl;
		for ( i=0; i<FeJoystick::Count; i++ )
			FeDebug() << "ID: " << i << " => Joy" << g_joyfemap[i] << "("
				<< fe_joystick_get_name( i )
				<< ")" << std::endl;
	}

	bool is_press_like_event( const FeEvent &e )
	{
		return e.is<FeEvent::KeyPressed>()
			|| e.is<FeEvent::MouseButtonPressed>()
			|| e.is<FeEvent::JoystickButtonPressed>()
			|| e.is<FeEvent::JoystickMoved>()
			|| e.is<FeEvent::TouchEnded>();
	}

	bool is_release_like_event( const FeEvent &e )
	{
		return e.is<FeEvent::KeyReleased>()
			|| e.is<FeEvent::MouseButtonReleased>()
			|| e.is<FeEvent::JoystickButtonReleased>();
	}

	const char *fe_key_to_config_string_internal( int code )
	{
		for ( const FeKeyInfo &entry : g_key_info )
		{
			if ( entry.name == nullptr )
				break;

			if ( static_cast<int>( entry.scancode ) == code )
				return entry.name;
		}

		return nullptr;
	}

	int fe_key_from_config_string_internal( const std::string &value )
	{
		for ( const FeKeyInfo &entry : g_key_info )
		{
			if ( entry.name == nullptr )
				break;

			if ( value == entry.name )
				return static_cast<int>( entry.scancode );
		}

		return static_cast<int>( SDL_SCANCODE_UNKNOWN );
	}

	std::optional<FeInputSingle> get_release_match_input(
		const FeEvent &e,
		const sf::IntRect &mc_rect,
		int joy_thresh,
		bool has_focus )
	{
		FeEvent te = e;

		if ( const auto *key = e.getIf<FeEvent::KeyReleased>() )
			te = FeEvent::KeyPressed{ key->code };
		else if ( const auto *joy = e.getIf<FeEvent::JoystickButtonReleased>() )
			te = FeEvent::JoystickButtonPressed{ joy->joystickId, joy->button };
		else if ( const auto *mouse = e.getIf<FeEvent::MouseButtonReleased>() )
			te = FeEvent::MouseButtonPressed{ mouse->button };
		else
			return std::nullopt;

		FeInputSingle input( te, mc_rect, joy_thresh, has_focus );
		if ( input.get_type() == FeInputSingle::Unsupported )
			return std::nullopt;

		return input;
	}
};

FeEvent::Vector2i FeInputMouse::m_pos_last = { INT_MAX, INT_MAX };
FeEvent::Vector2i FeInputMouse::m_pos_delta = { 0, 0 };
int FeInputMouse::m_wheel_delta = 0;

bool fe_key_is_pressed( int code )
{
	if ( code == static_cast<int>( SDL_SCANCODE_UNKNOWN ) )
		return false;

	SDL_PumpEvents();

	int key_count = 0;
	const bool *keyboard_state = SDL_GetKeyboardState( &key_count );
	return keyboard_state && code >= 0 && code < key_count && keyboard_state[code];
}

bool fe_mouse_is_button_pressed( int button )
{
	if ( button <= 0 || button > 32 )
		return false;

	SDL_PumpEvents();
	return ( SDL_GetMouseState( nullptr, nullptr ) & SDL_BUTTON_MASK( button ) ) != 0;
}

int fe_key_from_legacy_sfml_code( int legacy_code )
{
	if ( legacy_code < 0 )
		return static_cast<int>( SDL_SCANCODE_UNKNOWN );

	const std::size_t index = static_cast<std::size_t>( legacy_code );
	const std::size_t count = std::size( g_key_info ) - 1;
	if ( index >= count )
		return static_cast<int>( SDL_SCANCODE_UNKNOWN );

	return static_cast<int>( g_key_info[index].scancode );
}

void FeInputMouse::clear()
{
	m_wheel_delta = 0;
	m_pos_delta = { 0, 0 };
}

void FeInputMouse::set_wheel_delta( int delta )
{
	m_wheel_delta = delta;
}

int FeInputMouse::get_wheel_delta()
{
	return m_wheel_delta;
}

void FeInputMouse::set_pos_delta( FeEvent::Vector2i p )
{
	if ( m_pos_last.x != INT_MAX && m_pos_last.y != INT_MAX )
	{
		m_pos_delta.x = p.x - m_pos_last.x;
		m_pos_delta.y = p.y - m_pos_last.y;
	}
	m_pos_last = { p.x, p.y };
}

FeEvent::Vector2i FeInputMouse::get_pos_delta()
{
	return m_pos_delta;
}

const char *FeInputSingle::mouseStrings[] =
{
	"Up",
	"Down",
	"Left",
	"Right",
	"WheelUp",
	"WheelDown",
	"LeftButton",
	"RightButton",
	"MiddleButton",
	"ExtraButton1",
	"ExtraButton2",
	NULL
};

const char *FeInputSingle::touchStrings[] =
{
	"Up",
	"Down",
	"Left",
	"Right",
	"Tap",
	NULL
};

const char *FeInputSingle::joyStrings[] =
{
	"Zpos",
	"Zneg",
	"Rpos",
	"Rneg",
	"Upos",
	"Uneg",
	"Vpos",
	"Vneg",
	"PovXpos",
	"PovXneg",
	"PovYpos",
	"PovYneg",
	"Up",
	"Down",
	"Left",
	"Right",
	"Button",
	NULL
};

FeInputSingle::FeInputSingle()
	: m_type( Unsupported ),
	m_code( 0 )
{
}

FeInputSingle::FeInputSingle( Type t, int c )
	: m_type( t ),
	m_code( c )
{
}

FeInputSingle::FeInputSingle( const FeEvent &e, const sf::IntRect &mc_rect, const int joy_thresh, bool has_focus )
	: m_type( Unsupported ),
	m_code( 0 )
{
	if ( e.is<FeEvent::KeyPressed>() )
	{
		const auto* event = e.getIf<FeEvent::KeyPressed>();
		if ( event && event->code != static_cast<int>( SDL_SCANCODE_UNKNOWN ) )
		{
			m_type = Keyboard;
			m_code = event->code;
		}
	}

	else if ( e.is<FeEvent::JoystickButtonPressed>() )
	{
		const auto* event = e.getIf<FeEvent::JoystickButtonPressed>();
		if ( event )
		{
			m_type = static_cast<Type>( Joystick0 + joymap2feid( event->joystickId ));
			m_code = JoyButton0 + static_cast<int>( event->button );
		}
	}

	else if ( e.is<FeEvent::JoystickMoved>() )
	{
		const auto* event = e.getIf<FeEvent::JoystickMoved>();
		if ( event && std::abs( event->position ) > joy_thresh )
		{
			switch ( static_cast<FeJoystick::Axis>( event->axis ) )
			{
				case FeJoystick::Axis::X:
					m_code = ( event->position > 0 ) ? JoyRight : JoyLeft;
					break;

				case FeJoystick::Axis::Y:
					m_code = ( event->position > 0 ) ? JoyDown : JoyUp;
					break;

#ifdef SFML_SYSTEM_LINUX
				case FeJoystick::Axis::Z:
				case FeJoystick::Axis::R:
					if ( event->position < 0 )
						return;

					m_code = ( static_cast<FeJoystick::Axis>( event->axis ) == FeJoystick::Axis::Z )
						? JoyZPos : JoyRPos;
					break;
#else
				case FeJoystick::Axis::Z:
					m_code = ( event->position > 0 ) ? JoyZPos : JoyZNeg;
					break;

				case FeJoystick::Axis::R:
					m_code = ( event->position > 0 ) ? JoyRPos : JoyRNeg;
					break;
#endif

				case FeJoystick::Axis::U:
					m_code = ( event->position > 0 ) ? JoyUPos : JoyUNeg;
					break;

				case FeJoystick::Axis::V:
					m_code = ( event->position > 0 ) ? JoyVPos : JoyVNeg;
					break;

				case FeJoystick::Axis::PovX:
					m_code = ( event->position > 0 ) ? JoyPOVXPos : JoyPOVXNeg;
					break;

				case FeJoystick::Axis::PovY:
					m_code = ( event->position > 0 ) ? JoyPOVYPos : JoyPOVYNeg;
					break;

				default:
					ASSERT( 0 ); // unhandled joystick axis encountered
					return;
			}

			m_type = static_cast<Type>(Joystick0 + joymap2feid( event->joystickId ));
		}
	}

	else if ( has_focus && e.is<FeEvent::MouseMoved>() )
	{
		const auto* event = e.getIf<FeEvent::MouseMoved>();
		if ( event )
		{
			sf::Vector2i p( event->position.x, event->position.y );
			sf::Vector2i r1 = mc_rect.position;
			sf::Vector2i r2 = r1 + mc_rect.size;
			FeInputMouse::set_pos_delta( { p.x, p.y } );

			if ( p.x < r1.x )
			{
				m_type = Mouse;
				m_code = MouseLeft;
			}
			else if ( p.y < r1.y )
			{
				m_type = Mouse;
				m_code = MouseUp;
			}
			else if ( p.x >= r2.x )
			{
				m_type = Mouse;
				m_code = MouseRight;
			}
			else if ( p.y >= r2.y )
			{
				m_type = Mouse;
				m_code = MouseDown;
			}
		}
	}

	else if ( e.is<FeEvent::MouseWheelScrolled>() )
	{
		const auto* event = e.getIf<FeEvent::MouseWheelScrolled>();
		if ( event )
		{
			m_type = Mouse;
			m_code = event->delta > 0 ? MouseWheelUp : MouseWheelDown;
			FeInputMouse::set_wheel_delta( event->delta );
		}
	}

	else if ( e.is<FeEvent::MouseButtonPressed>() )
	{
		const auto* event = e.getIf<FeEvent::MouseButtonPressed>();
		if ( event )
		{
			switch ( event->button )
			{
				case SDL_BUTTON_LEFT:   m_code = MouseBLeft;   break;
				case SDL_BUTTON_RIGHT:  m_code = MouseBRight;  break;
				case SDL_BUTTON_MIDDLE: m_code = MouseBMiddle; break;
				case SDL_BUTTON_X1:     m_code = MouseBX1;     break;
				case SDL_BUTTON_X2:     m_code = MouseBX2;     break;
				default:
					ASSERT( 0 ); // unhandled mouse button encountered
					return;
			}
			m_type = Mouse;
		}
	}

	else if ( e.is<FeEvent::TouchMoved>() )
	{
		const auto* event = e.getIf<FeEvent::TouchMoved>();
		if ( event && g_last_touch.has_value() )
		{
			const auto* last = g_last_touch->getIf<FeEvent::TouchMoved>();
			if ( !last )
			{
				g_last_touch = e;
				return;
			}
			const int THRESH = 100;
			int diff_x = event->position.x - last->position.x;
			int diff_y = event->position.y - last->position.y;

			if ( abs( diff_y ) > THRESH )
			{
				if ( diff_y < 0 )
					m_code = SwipeUp;
				else
					m_code = SwipeDown;

				m_type = Touch;

				g_touch_moved = true;
				g_last_touch = e;
			}
			else if ( abs( diff_x ) > THRESH )
			{
				if ( diff_x < 0 )
					m_code = SwipeRight;
				else
					m_code = SwipeLeft;

				m_type = Touch;

				g_touch_moved = true;
				g_last_touch = e;
			}
		}
		else if ( event )
		{
			g_last_touch = e;
		}
	}

	else if ( e.is<FeEvent::TouchBegan>() )
	{
		const auto* event = e.getIf<FeEvent::TouchBegan>();
		g_touch_moved = false;
		if ( event )
		{
			g_last_touch = e;
		}
	}

	else if ( e.is<FeEvent::TouchEnded>() )
	{
		if ( !g_touch_moved )
		{
			m_code = Tap;
			m_type = Touch;
		}
	}
}

FeInputSingle::FeInputSingle( const std::string &str )
	: m_type( Unsupported ),
	m_code( 0 )
{
	if ( str.empty() )
		return;

	size_t pos=0;
	std::string val;

	token_helper( str, pos, val, FE_WHITESPACE );
	int i=0;

	if ( val.compare( "Mouse" ) == 0 )
	{
		m_type = Mouse;

		token_helper( str, pos, val, FE_WHITESPACE );
		while ( mouseStrings[i] != NULL )
		{
			if ( val.compare( mouseStrings[i] ) == 0 )
			{
				m_code = i;
				break;
			}
			i++;
		}
	}
	else if ( val.compare( "Touch" ) == 0 )
	{
		m_type = Touch;

		token_helper( str, pos, val, FE_WHITESPACE );
		while ( touchStrings[i] != NULL )
		{
			if ( val.compare( touchStrings[i] ) == 0 )
			{
				m_code = i;
				break;
			}
			i++;
		}
	}
	else if ( val.compare( 0, 3, "Joy" ) == 0 )
	{
		int num = as_int( val.substr( 3 ) );
		m_type = (Type)(Joystick0 + num);

		token_helper( str, pos, val, FE_WHITESPACE );
		while ( joyStrings[i] != NULL )
		{
			if ( val.compare( 0, strlen(joyStrings[i]), joyStrings[i] ) == 0 )
			{
				if ( i == JoyButton0 )
				{
					int temp = as_int( val.substr( strlen( joyStrings[i] ) ) );
					m_code = i + temp;
				}
				else
				{
					m_code = i;
				}
				break;
			}
			i++;
		}
	}
	else
	{
		const int key_code = fe_key_from_config_string_internal( val );
		if ( key_code != static_cast<int>( SDL_SCANCODE_UNKNOWN ) )
		{
			m_type = Keyboard;
			m_code = key_code;
		}
	}
}

std::string FeInputSingle::as_string() const
{
	std::string temp;

	if ( m_type == Keyboard )
	{
		if ( const char *name = fe_key_to_config_string_internal( m_code ) )
			temp = name;
	}
	else if ( m_type == Mouse )
	{
		temp = "Mouse ";
		temp += mouseStrings[ m_code ];
	}
	else if ( m_type == Touch )
	{
		temp = "Touch ";
		temp += touchStrings[ m_code ];
	}
	else if ( m_type >= Joystick0 )
	{
		temp = "Joy";
		temp += as_str( m_type - Joystick0 );
		temp += " ";
		if ( m_code >= JoyButton0 )
		{
			temp += joyStrings[ JoyButton0 ];
			temp += as_str( m_code - JoyButton0 );
		}
		else
		{
			temp += joyStrings[ m_code ];
		}
	}

	return temp;
}

bool FeInputSingle::is_mouse_move() const
{
	return (( m_type == Mouse )
		&& ( m_code <= MouseRight )
		&& ( m_code >= MouseUp ));
}

bool FeInputSingle::operator< ( const FeInputSingle &o ) const
{
	if ( m_type == o.m_type )
		return ( m_code < o.m_code );

	return ( m_type < o.m_type );
}

bool FeInputSingle::get_current_state( int joy_thresh ) const
{
	if ( m_type == Unsupported )
		return false;
	else if ( m_type == Keyboard )
		return fe_key_is_pressed( m_code );
	else if ( m_type == Mouse )
	{
		switch ( m_code )
		{
		case MouseUp: return FeInputMouse::get_pos_delta().y < 0;
		case MouseDown: return FeInputMouse::get_pos_delta().y > 0;
		case MouseLeft: return FeInputMouse::get_pos_delta().x < 0;
		case MouseRight: return FeInputMouse::get_pos_delta().x > 0;
		case MouseWheelUp: return FeInputMouse::get_wheel_delta() > 0;
		case MouseWheelDown: return FeInputMouse::get_wheel_delta() < 0;
		case MouseBLeft: return fe_mouse_is_button_pressed( SDL_BUTTON_LEFT );
		case MouseBRight: return fe_mouse_is_button_pressed( SDL_BUTTON_RIGHT );
		case MouseBMiddle: return fe_mouse_is_button_pressed( SDL_BUTTON_MIDDLE );
		case MouseBX1: return fe_mouse_is_button_pressed( SDL_BUTTON_X1 );
		case MouseBX2: return fe_mouse_is_button_pressed( SDL_BUTTON_X2 );
		default: return false;
		}
	}
	else if ( m_type == Touch )
		return sf::Touch::isDown( 0 );
	else // Joysticks
	{
		fe_joystick_update();

		int id = joymap2raw( m_type - Joystick0 );

		if ( m_code < JoyButton0 )
		{
			switch ( m_code )
			{
				case JoyLeft: return ( -fe_joystick_get_axis_position( id, FeJoystick::Axis::X ) > joy_thresh );
				case JoyRight: return ( fe_joystick_get_axis_position( id, FeJoystick::Axis::X ) > joy_thresh );
				case JoyUp: return ( -fe_joystick_get_axis_position( id, FeJoystick::Axis::Y ) > joy_thresh );
				case JoyDown: return ( fe_joystick_get_axis_position( id, FeJoystick::Axis::Y ) > joy_thresh );
				case JoyZPos: return ( fe_joystick_get_axis_position( id, FeJoystick::Axis::Z ) > joy_thresh );
				case JoyZNeg: return ( -fe_joystick_get_axis_position( id, FeJoystick::Axis::Z ) > joy_thresh );
				case JoyRPos: return ( fe_joystick_get_axis_position( id, FeJoystick::Axis::R ) > joy_thresh );
				case JoyRNeg: return ( -fe_joystick_get_axis_position( id, FeJoystick::Axis::R ) > joy_thresh );
				case JoyUPos: return ( fe_joystick_get_axis_position( id, FeJoystick::Axis::U ) > joy_thresh );
				case JoyUNeg: return ( -fe_joystick_get_axis_position( id, FeJoystick::Axis::U ) > joy_thresh );
				case JoyVPos: return ( fe_joystick_get_axis_position( id, FeJoystick::Axis::V ) > joy_thresh );
				case JoyVNeg: return ( -fe_joystick_get_axis_position( id, FeJoystick::Axis::V ) > joy_thresh );
				case JoyPOVXPos: return ( fe_joystick_get_axis_position( id, FeJoystick::Axis::PovX ) > joy_thresh );
				case JoyPOVXNeg: return ( -fe_joystick_get_axis_position( id, FeJoystick::Axis::PovX ) > joy_thresh );
				case JoyPOVYPos: return ( fe_joystick_get_axis_position( id, FeJoystick::Axis::PovY ) > joy_thresh );
				case JoyPOVYNeg: return ( -fe_joystick_get_axis_position( id, FeJoystick::Axis::PovY ) > joy_thresh );
				default: return false;
			}
		}
		else
			return fe_joystick_is_button_pressed( id, m_code - JoyButton0 );
	}
}

int FeInputSingle::get_current_pos( FeWindow &wnd ) const
{
	if ( m_type == Mouse )
	{
		if (( m_code == MouseUp ) || ( m_code == MouseDown ))
			return wnd.get_mouse_position().y;
		else if (( m_code == MouseLeft ) || ( m_code == MouseRight ))
			return wnd.get_mouse_position().x;
		else if (( m_code == MouseWheelUp ) || ( m_code == MouseWheelDown ))
			return FeInputMouse::get_wheel_delta();
	}
	else if (( m_type >= Joystick0 ) && ( m_code < JoyButton0 ))
	{
		// return the joystick position on the specified axis
		fe_joystick_update();

		int temp = 0;
		int id = joymap2raw( m_type - Joystick0 );

		switch ( m_code )
		{
			case JoyLeft: temp = -fe_joystick_get_axis_position( id, FeJoystick::Axis::X ); break;
			case JoyRight: temp = fe_joystick_get_axis_position( id, FeJoystick::Axis::X ); break;
			case JoyUp: temp = -fe_joystick_get_axis_position( id, FeJoystick::Axis::Y ); break;
			case JoyDown: temp = fe_joystick_get_axis_position( id, FeJoystick::Axis::Y ); break;
			case JoyZPos: temp = fe_joystick_get_axis_position( id, FeJoystick::Axis::Z ); break;
			case JoyZNeg: temp = -fe_joystick_get_axis_position( id, FeJoystick::Axis::Z ); break;
			case JoyRPos: temp = fe_joystick_get_axis_position( id, FeJoystick::Axis::R ); break;
			case JoyRNeg: temp = -fe_joystick_get_axis_position( id, FeJoystick::Axis::R ); break;
			case JoyUPos: temp = fe_joystick_get_axis_position( id, FeJoystick::Axis::U ); break;
			case JoyUNeg: temp = -fe_joystick_get_axis_position( id, FeJoystick::Axis::U ); break;
			case JoyVPos: temp = fe_joystick_get_axis_position( id, FeJoystick::Axis::V ); break;
			case JoyVNeg: temp = -fe_joystick_get_axis_position( id, FeJoystick::Axis::V ); break;
			case JoyPOVXPos: temp = fe_joystick_get_axis_position( id, FeJoystick::Axis::PovX ); break;
			case JoyPOVXNeg: temp = -fe_joystick_get_axis_position( id, FeJoystick::Axis::PovX ); break;
			case JoyPOVYPos: temp = fe_joystick_get_axis_position( id, FeJoystick::Axis::PovY ); break;
			case JoyPOVYNeg: temp = -fe_joystick_get_axis_position( id, FeJoystick::Axis::PovY ); break;
			default: break;
		}

		return ( temp < 0 ) ? 0 : temp;
	}

	return 0;
}

std::string FeInputSingle::get_joy_name() const
{
	if ( m_type >= Joystick0 )
	{
		int raw_id = joymap2raw( m_type - Joystick0 );

		if ( fe_joystick_is_connected( raw_id ) )
			return fe_joystick_get_name( raw_id );
	}
	return "";
}

bool FeInputSingle::operator==( const FeInputSingle &o ) const
{
	return (( m_type == o.m_type ) && ( m_code == o.m_code ));
}

bool FeInputSingle::operator!=( const FeInputSingle &o ) const
{
	return (( m_type != o.m_type ) || ( m_code != o.m_code ));
}

FeInputMapEntry::FeInputMapEntry()
	: command( FeInputMap::LAST_COMMAND )
{
}

FeInputMapEntry::FeInputMapEntry( FeInputSingle::Type t, int code, FeInputMap::Command c )
	: command( c )
{
	inputs.insert( FeInputSingle( t, code ) );
}

FeInputMapEntry::FeInputMapEntry( const std::string &s, FeInputMap::Command c )
	: command( c )
{
	size_t pos=0;

	do
	{
		std::string tok;
		token_helper( s, pos, tok, "+" );
		if ( !tok.empty() )
			inputs.insert( FeInputSingle( tok ) );

	} while ( pos < s.size() );
}

bool FeInputMapEntry::get_current_state( int joy_thresh, const FeInputSingle &trigger ) const
{
	if ( inputs.empty() )
		return false;

	for ( std::set < FeInputSingle >::const_iterator it=inputs.begin(); it != inputs.end(); ++it )
	{
		if (( (*it) != trigger ) && ( !(*it).get_current_state( joy_thresh ) ))
			return false;
	}

	return true;
}

std::string FeInputMapEntry::as_string() const
{
	std::string retval;

	for ( std::set < FeInputSingle >::const_iterator it=inputs.begin(); it != inputs.end(); ++it )
	{
		if ( !retval.empty() )
			retval += "+";

		retval += (*it).as_string();
	}

	return retval;
}

bool FeInputMapEntry::has_mouse_move() const
{
	for ( std::set < FeInputSingle >::const_iterator it=inputs.begin(); it != inputs.end(); ++it )
	{
		if ( (*it).is_mouse_move() )
			return true;
	}

	return false;
}

FeMapping::FeMapping( FeInputMap::Command cmd )
	: command( cmd )
{
}

bool FeMapping::operator< ( const FeMapping &o ) const
{
	return ( command < o.command );
}

// NOTE: This needs to be kept in alignment with fe_input.hpp `enum Command`
//
const char *FeInputMap::commandStrings[] =
{
	"back",
	"up",
	"down",
	"left",
	"right",
	"select", // ^^^^ Cannot change order here on up
	"prev_game",
	"next_game",
	"prev_page", // was page_up
	"next_page", // was page_down
	"prev_letter",
	"next_letter",
	"prev_favourite",
	"next_favourite",
	"prev_filter",
	"next_filter",
	"prev_display", // was prev_list
	"next_display", // was next_list
	"random_game",
	"replay_last_game",
	"filters_menu",
	"displays_menu", // was lists_menu
	"add_tags",
	"add_favourite",
	"configure",
	"layout_options",
	"plugin_options",
	"toggle_layout",
	"reload_layout",
	"reload_config",
	"reset_window",
	"exit",
	"exit_to_desktop",
	"toggle_fullscreen",
	"toggle_movie",
	"toggle_mute",
	"toggle_rotate_left",
	"toggle_rotate_right",
	"toggle_flip",
	"intro",
	"screen_saver",
	"screenshot",
	"insert_game",
	"edit_game",
	"custom1",
	"custom2",
	"custom3",
	"custom4",
	"custom5",
	"custom6",
	"custom7",
	"custom8",
	"custom9",
	"custom10",
	NULL, // LAST_COMMAND... NULL required here
	"ambient",
	"startup",
	"game_return",
	NULL
};

const char *FeInputMap::commandDispStrings[] =
{
	"Back",
	"Up",
	"Down",
	"Left",
	"Right",
	"Select", // ^^^^ Cannot change order here on up
	"Previous Game",
	"Next Game",
	"Previous Page",
	"Next Page",
	"Previous Letter",
	"Next Letter",
	"Previous Favourite",
	"Next Favourite",
	"Previous Filter",
	"Next Filter",
	"Previous Display",
	"Next Display",
	"Random Game",
	"Replay Last Game",
	"Filters Menu",
	"Displays Menu",
	"Add/Remove Tags",
	"Add/Remove Favourite",
	"Configure",
	"Layout Options",
	"Plugin Options",
	"Toggle Layout",
	"Reload Layout",
	"Reload Config",
	"Reset Window",
	"Exit",
	"Exit to Desktop",
	"Toggle Fullscreen",
	"Toggle Movie",
	"Toggle Mute",
	"Toggle Rotate Left",
	"Toggle Rotate Right",
	"Toggle Flip",
	"Intro",
	"Screen Saver",
	"Screenshot",
	"Insert Game",
	"Game Options",
	"Custom1",
	"Custom2",
	"Custom3",
	"Custom4",
	"Custom5",
	"Custom6",
	"Custom7",
	"Custom8",
	"Custom9",
	"Custom10",
	NULL, // LAST_COMMAND... NULL required here
	"Ambient Soundtrack",
	"Startup Sound",
	"Game Return Sound",
	NULL
};

FeInputMap::FeInputMap()
	: m_defaults( (int)Select ),
	  m_suppress_pressed_inputs( false ),
	  m_mmove_count( 0 )
{
	// Set default actions for the "UI" commands (Back, Up, Down, Left, Right)
	//
	m_defaults[ Back ]  = Exit;
	m_defaults[ Up ]    = PrevGame;
	m_defaults[ Down ]  = NextGame;
	m_defaults[ Left ]  = PrevDisplay;
	m_defaults[ Right ] = NextDisplay;
}

bool my_sort_fn( FeInputMapEntry *a, FeInputMapEntry *b )
{
	return ( a->inputs.size() > b->inputs.size() );
}

void FeInputMap::initialize_mappings()
{
	if ( m_inputs.empty() )
	{
		//
		// Default controls
		// Set some convenient initial mappings (user can always choose to unmap)...
		//
		struct InitialMappings { const char *label; Command command; } imap[] =
		{
			{ "Tab",                      Configure },
			{ "Up+LControl",              PrevLetter },
			{ "Down+LControl",            NextLetter },
			{ "Left+LControl",            FiltersMenu },
			{ "Right+LControl",           NextFilter },
			{ "Escape+Up",                Configure },
			{ "Escape+Down",              EditGame },
			{ "Escape+LControl",          ToggleFavourite },
			{ "F11",                      ToggleFullscreen },
			{ "RAlt+Return",              ToggleFullscreen },
			{ "PageUp",              		PrevPage },
			{ "PageDown",              	NextPage },
			{ "F5",                       ReloadLayout },
			{ "LShift+F5",                ReloadConfig },

			{ "Joy0 Up+Joy0 Button0",     PrevLetter },
			{ "Joy0 Down+Joy0 Button0",   NextLetter },
			{ "Joy0 Left+Joy0 Button0",   FiltersMenu },
			{ "Joy0 Right+Joy0 Button0",  NextFilter },
			{ "Joy0 Up+Joy0 Button1",     Configure },
			{ "Joy0 Down+Joy0 Button1",   EditGame },
			{ "Joy0 Button0+Joy0 Button1",ToggleFavourite },

			{ NULL,                       LAST_COMMAND }
		};

		int i=0;
		while ( imap[i].label != NULL )
		{
			m_inputs.push_back( FeInputMapEntry( imap[i].label, imap[i].command ) );
			i++;
		}
	}

	//
	// Now ensure that the various 'UI' commands and 'Select' are mapped
	//
	std::vector < bool > ui_mapped( (int)Select+1, false );

	std::vector< FeInputMapEntry >::iterator it;
	for ( it = m_inputs.begin(); it != m_inputs.end(); ++it )
	{
		if ( it->command <= Select )
			ui_mapped[ it->command ] = true;
	}

	bool fix = false;
	for ( unsigned int i=0; i<ui_mapped.size(); i++ )
	{
		if ( !ui_mapped[i] )
		{
			fix=true;
			break;
		}
	}

	if ( fix )
	{
		//
		// The default UI command mappings:
		//
		struct DefaultMappings { FeInputSingle::Type type; int code; Command comm; };
		DefaultMappings dmap[] =
		{
			{ FeInputSingle::Keyboard,    static_cast<int>( SDL_SCANCODE_ESCAPE ), Back },
			{ FeInputSingle::Joystick0,   FeInputSingle::JoyButton0+1, Back },
			{ FeInputSingle::Keyboard,    static_cast<int>( SDL_SCANCODE_UP ), Up },
			{ FeInputSingle::Joystick0,   FeInputSingle::JoyUp, Up },
			{ FeInputSingle::Keyboard,    static_cast<int>( SDL_SCANCODE_DOWN ), Down },
			{ FeInputSingle::Joystick0,   FeInputSingle::JoyDown, Down },
			{ FeInputSingle::Keyboard,    static_cast<int>( SDL_SCANCODE_LEFT ), Left },
			{ FeInputSingle::Joystick0,   FeInputSingle::JoyLeft, Left },
			{ FeInputSingle::Keyboard,    static_cast<int>( SDL_SCANCODE_RIGHT ), Right },
			{ FeInputSingle::Joystick0,   FeInputSingle::JoyRight, Right },
			{ FeInputSingle::Keyboard,    static_cast<int>( SDL_SCANCODE_RETURN ), Select },
			{ FeInputSingle::Keyboard,    static_cast<int>( SDL_SCANCODE_LCTRL ), Select },
			{ FeInputSingle::Joystick0,   FeInputSingle::JoyButton0, Select },

#ifdef SFML_SYSTEM_ANDROID
			{ FeInputSingle::Touch,       FeInputSingle::SwipeUp,      Up },
			{ FeInputSingle::Touch,       FeInputSingle::SwipeDown,    Down },
			{ FeInputSingle::Touch,       FeInputSingle::SwipeLeft,    Left },
			{ FeInputSingle::Touch,       FeInputSingle::SwipeRight,   Right },
			{ FeInputSingle::Touch,       FeInputSingle::Tap,          Select },
#endif

			{ FeInputSingle::Unsupported, static_cast<int>( SDL_SCANCODE_UNKNOWN ), LAST_COMMAND }	// keep as last
		};

		int i=0;
		while ( dmap[i].comm != LAST_COMMAND )
		{
			// This will overwrite any conflicting input mapping
			if ( !ui_mapped[ dmap[i].comm ] )
				m_inputs.push_back( FeInputMapEntry( dmap[i].type, dmap[i].code, dmap[i].comm ) );

			i++;
		}
	}

	//
	// Now setup m_single_map
	//
	std::map< FeInputSingle, std::vector< FeInputMapEntry * > >::iterator itm;
	m_single_map.clear();

	for ( std::vector < FeInputMapEntry >::iterator ite=m_inputs.begin(); ite != m_inputs.end(); ++ite )
	{
		for ( std::set < FeInputSingle >::iterator its=(*ite).inputs.begin(); its != (*ite).inputs.end(); ++its )
		{
			itm = m_single_map.find( *its );
			if ( itm != m_single_map.end() )
				(*itm).second.push_back( &( *ite ) );
			else
			{
				std::vector< FeInputMapEntry *> temp;
				temp.push_back( &( *ite ) );
				m_single_map[ *its ] = temp;
			}
		}
	}

	for ( itm = m_single_map.begin(); itm != m_single_map.end(); ++itm )
		std::sort( (*itm).second.begin(), (*itm).second.end(), my_sort_fn );

	joy_raw_map_init( m_joy_config );
}

void FeInputMap::on_joystick_connect()
{
	joy_raw_map_init( m_joy_config );
}

FeInputMap::Command FeInputMap::get_command_from_tracked_keys() const
{
	FeInputMap::Command retval = FeInputMap::LAST_COMMAND;

	// Rank is the number of inputs a command requires (ie: key combos)
	// - Commands with higher ranks will be chosen over lower ones
	size_t rank = 0;

	for ( std::set<FeInputSingle>::iterator itt = m_tracked_keys.begin(); itt != m_tracked_keys.end(); ++itt )
	{
		std::map<FeInputSingle, std::vector<FeInputMapEntry*>>::const_iterator it;
		it = m_single_map.find( *itt );

		if ( it == m_single_map.end() )
		{
			// this should not happen!
			// it means an un-mapped key has been added to m_tracked_keys
			ASSERT( 0 );
			continue;
		}

		// Multiple commands may be mapped to a single key
		std::vector<FeInputMapEntry*>::const_iterator itv;
		for ( itv = (*it).second.begin(); itv != (*it).second.end(); ++itv )
		{
			bool found = true;
			size_t it_rank = (*itv)->inputs.size();

			// Exit early if this command is outranked
			if ( it_rank <= rank ) continue;

			// Check if all keys for this command are tracked
			for ( std::set<FeInputSingle>::const_iterator its = (*itv)->inputs.begin(); its != (*itv)->inputs.end(); ++its )
			{
				if ( m_tracked_keys.find( *its ) == m_tracked_keys.end() )
				{
					found = false;
					break;
				}
			}

			// Select this command for return
			if ( found ) {
				retval = (*itv)->command;
				rank = it_rank;
			}
		}
	}

	return retval;
}

void FeInputMap::clear_tracked_keys()
{
	m_tracked_keys.clear();
}

void FeInputMap::suppress_pressed_inputs( int joy_thresh )
{
	m_tracked_keys.clear();
	m_suppressed_inputs.clear();

	for ( const auto &entry : m_single_map )
	{
		const FeInputSingle &input = entry.first;
		if ( input.get_current_state( joy_thresh ) )
			m_suppressed_inputs.insert( input );
	}

	m_suppress_pressed_inputs = !m_suppressed_inputs.empty();
}

void FeInputMap::clear()
{
	m_single_map.clear();
	m_inputs.clear();
	m_defaults.clear();
	m_tracked_keys.clear();
	m_suppressed_inputs.clear();
	m_suppress_pressed_inputs = false;
	m_joy_config.clear();
	m_mmove_count = 0;
}

FeInputMap::Command FeInputMap::map_input( const FeEvent &e, const sf::IntRect &mc_rect, const int joy_thresh, bool has_focus )
{
	FeInputSingle index( e, mc_rect, joy_thresh, has_focus );

	// Window focus has changed
	if ( e.is<FeEvent::FocusLost>() || e.is<FeEvent::FocusGained>() )
	{
		clear_tracked_keys();
		if ( e.is<FeEvent::FocusGained>() )
			suppress_pressed_inputs( joy_thresh );
		else
		{
			m_suppressed_inputs.clear();
			m_suppress_pressed_inputs = false;
		}
	}

	// Window has closed
	if ( e.is<FeEvent::Closed>() )
	{
		clear_tracked_keys();
		return ExitToDesktop;
	}

	if ( m_suppress_pressed_inputs )
	{
		if ( e.is<FeEvent::JoystickMoved>() && ( index.get_type() == FeInputSingle::Unsupported ) )
		{
			const auto *event = e.getIf<FeEvent::JoystickMoved>();
			if ( event )
			{
				FeEvent pos = FeEvent::JoystickMoved{ event->joystickId, event->axis, (float)joy_thresh * 2 };
				FeEvent neg = FeEvent::JoystickMoved{ event->joystickId, event->axis, (float)joy_thresh * -2 };
				FeInputSingle pos_input( pos, mc_rect, joy_thresh, has_focus );
				FeInputSingle neg_input( neg, mc_rect, joy_thresh, has_focus );
				m_suppressed_inputs.erase( pos_input );
				m_suppressed_inputs.erase( neg_input );
			}

			if ( m_suppressed_inputs.empty() )
				m_suppress_pressed_inputs = false;

			return LAST_COMMAND;
		}

		if ( is_press_like_event( e ) && index.get_type() != FeInputSingle::Unsupported )
		{
			if ( m_suppressed_inputs.find( index ) != m_suppressed_inputs.end() )
				return LAST_COMMAND;
		}

		if ( is_release_like_event( e ) )
		{
			if ( const auto match = get_release_match_input( e, mc_rect, joy_thresh, has_focus ) )
			{
				if ( m_suppressed_inputs.erase( *match ) > 0 )
				{
					if ( m_suppressed_inputs.empty() )
						m_suppress_pressed_inputs = false;

					return LAST_COMMAND;
				}
			}
		}
	}

	// Joystick has moved
	else if ( e.is<FeEvent::JoystickMoved>() )
	{
		if ( !index.get_current_state( joy_thresh ) )
		{
			const auto* event = e.getIf<FeEvent::JoystickMoved>();

			// Special - released joystick needs to check for tracked pos/neg keys, since index will be empty
			FeEvent pos = FeEvent::JoystickMoved{ event->joystickId, event->axis, (float)joy_thresh * 2 };
			FeInputSingle tt( pos, mc_rect, joy_thresh, has_focus );
			std::set<FeInputSingle>::iterator itr = m_tracked_keys.find( tt );

			if ( itr == m_tracked_keys.end() ) {
				FeEvent neg = FeEvent::JoystickMoved{ event->joystickId, event->axis, (float)joy_thresh * -2 };
				FeInputSingle tt( neg, mc_rect, joy_thresh, has_focus );
				itr = m_tracked_keys.find( tt );
			}

			if ( itr != m_tracked_keys.end() )
			{
				FeInputMap::Command c = get_command_from_tracked_keys();
				m_tracked_keys.erase( itr );
				if ( c != LAST_COMMAND )
					return c;
			}
		}
	}

	// Keyboard key has been released
	else if ( e.is<FeEvent::KeyReleased>() )
	{
		FeEvent te = e;
		if ( const auto* key = e.getIf<FeEvent::KeyReleased>() ) te = FeEvent::KeyPressed{ key->code };
		FeInputSingle tt( te, mc_rect, joy_thresh, has_focus );
		std::set<FeInputSingle>::iterator itr = m_tracked_keys.find( tt );
		if ( itr != m_tracked_keys.end() )
		{
			FeInputMap::Command c = get_command_from_tracked_keys();
			m_tracked_keys.erase( itr );
			if ( c != LAST_COMMAND )
				return c;
		}
	}

	// Joystick button has been released
	else if ( e.is<FeEvent::JoystickButtonReleased>() )
	{
		FeEvent te = e;
		if ( const auto* joy = e.getIf<FeEvent::JoystickButtonReleased>() ) te = FeEvent::JoystickButtonPressed{ joy->joystickId, joy->button };
		FeInputSingle tt( te, mc_rect, joy_thresh, has_focus );
		std::set<FeInputSingle>::iterator itr = m_tracked_keys.find( tt );
		if ( itr != m_tracked_keys.end() )
		{
			FeInputMap::Command c = get_command_from_tracked_keys();
			m_tracked_keys.erase( itr );
			if ( c != LAST_COMMAND )
				return c;
		}
	}

	// Mouse button has been released
	else if ( e.is<FeEvent::MouseButtonReleased>() )
	{
		FeEvent te = e;
		if ( const auto* mouse = e.getIf<FeEvent::MouseButtonReleased>() ) te = FeEvent::MouseButtonPressed{ mouse->button };
		FeInputSingle tt( te, mc_rect, joy_thresh, has_focus );
		std::set<FeInputSingle>::iterator itr = m_tracked_keys.find( tt );
		if ( itr != m_tracked_keys.end() )
		{
			FeInputMap::Command c = get_command_from_tracked_keys();
			m_tracked_keys.erase( itr );
			if ( c != LAST_COMMAND )
				return c;
		}
	}

	// Touch event has ended
	else if ( e.is<FeEvent::TouchEnded>() )
	{
		if ( m_single_map.find( index ) != m_single_map.end() )
		{
			m_tracked_keys.insert( index );
			FeInputMap::Command c = get_command_from_tracked_keys();
			m_tracked_keys.erase( index );
			if ( c != LAST_COMMAND )
				return c;
		}
	}

	// Ignore unsupported input
	if ( index.get_type() == FeInputSingle::Unsupported )
		return LAST_COMMAND;

	std::map<FeInputSingle, std::vector<FeInputMapEntry*>>::const_iterator it;
	it = m_single_map.find( index );

	// Ignore unmapped keys
	if ( it == m_single_map.end() )
		return LAST_COMMAND;

	// Add any other mapped key to the list
	m_tracked_keys.insert( index );

	//
	// Special case:
	// If this is a repeatable command, trigger on the PRESS event.
	// For everything else trigger on RELEASE.
	//
	std::vector<FeInputMapEntry*>::const_iterator itv;
	for ( itv = (*it).second.begin(); itv != (*it).second.end(); ++itv )
	{
		FeInputMap::Command cmd = (*itv)->command;
		if ( is_repeatable_command( cmd ) || ( is_ui_command( cmd ) && ( cmd != Back )))
		{
			FeInputMap::Command c = get_command_from_tracked_keys();
			if ( c != LAST_COMMAND )
			{
				m_tracked_keys.erase( index );
				return c;
			}
		}
	}

	return LAST_COMMAND;
}

FeInputMap::Command FeInputMap::input_conflict_check( const FeInputMapEntry &e )
{
	if ( e.inputs.empty() )
		return FeInputMap::LAST_COMMAND;

	std::vector< FeInputMapEntry >::iterator ite;
	for ( ite=m_inputs.begin(); ite!=m_inputs.end(); ++ite )
	{
		if ( (*ite).inputs.size() == e.inputs.size() )
		{
			bool match=true;
			std::set< FeInputSingle >::iterator it;
			for ( it=(*ite).inputs.begin(); it!=(*ite).inputs.end(); ++it )
			{
				if ( e.inputs.find( (*it) ) == e.inputs.end() )
				{
					match=false;
					break;
				}
			}

			if ( match )
				return (*ite).command;
		}
	}

	return FeInputMap::LAST_COMMAND;
}

FeInputMap::Command FeInputMap::get_default_command( FeInputMap::Command c )
{
	if (( c < 0 ) || ( c >= Select ))
	{
		ASSERT( 0 ); // this shouldn't be happening
		return LAST_COMMAND;
	}

	return m_defaults[ c ];
}

void FeInputMap::set_default_command( FeInputMap::Command c, FeInputMap::Command v )
{
	m_defaults[ c ] = v;
}

bool FeInputMap::get_current_state( FeInputMap::Command c, int joy_thresh ) const
{
	std::vector< FeInputMapEntry >::const_iterator it;
	for ( it=m_inputs.begin(); it!=m_inputs.end(); ++it )
	{
		if (( (*it).command == c ) && (*it).get_current_state( joy_thresh ) )
			return true;
	}

	return false;
}

void FeInputMap::get_mappings( std::vector< FeMapping > &mappings ) const
{
	//
	// Make a mappings entry for each possible command mapping
	//
	mappings.clear();
	mappings.reserve( LAST_COMMAND );
	for ( int i=0; i< LAST_COMMAND; i++ )
		mappings.push_back( FeMapping( (FeInputMap::Command)i ) );

	//
	// Now populate the mappings vector with the various input mappings
	//
	std::vector< FeInputMapEntry >::const_iterator it;

	for ( it=m_inputs.begin(); it!=m_inputs.end(); ++it )
		mappings[ (*it).command ].input_list.push_back( (*it).as_string() );
}

void FeInputMap::set_mapping( const FeMapping &mapping )
{
	Command cmd = mapping.command;

	if ( cmd == LAST_COMMAND )
		return;

	std::vector< std::string >::const_iterator iti;

	for ( int i=m_inputs.size()-1; i>=0; i-- )
	{
		// Erase existing mappings to this command
		//
		if ( m_inputs[i].command == cmd )
		{
			if ( m_inputs[i].has_mouse_move() )
				m_mmove_count--;

			m_inputs.erase( m_inputs.begin() + i );
			continue;
		}

		// Erase conflicting inputs
		//
		std::string temp_str = m_inputs[i].as_string();
		for ( iti = mapping.input_list.begin(); iti != mapping.input_list.end(); ++iti )
		{
			if ( (*iti).compare( temp_str ) == 0 )
			{
				m_inputs.erase( m_inputs.begin() + i );
				continue;
			}
		}
	}

	//
	// Now update our map with the inputs from this mapping
	//
	for ( iti=mapping.input_list.begin();
			iti!=mapping.input_list.end(); ++iti )
	{
		FeInputMapEntry new_entry( *iti, cmd );

		if (!new_entry.inputs.empty())
		{
			m_inputs.push_back( new_entry );

			if ( new_entry.has_mouse_move() )
				m_mmove_count++;
		}
	}

	initialize_mappings();
}

int FeInputMap::process_setting( const std::string &setting,
	const std::string &value,
	const std::string &fn )
{
	if ( setting.compare( "default" ) == 0 )
	{
		// value: "<command> <command>"
		size_t pos=0;
		std::string from, to;
		Command fc, tc=LAST_COMMAND;

		token_helper( value, pos, from, FE_WHITESPACE );
		token_helper( value, pos, to, FE_WHITESPACE );

		fc = string_to_command( from );

		if (( fc < Select ) && ( !to.empty() ))
			tc = string_to_command( to );

		m_defaults[fc]=tc;
		return 0;
	}
	else if ( setting.compare( "map" ) == 0 )
	{
		size_t pos=0;
		std::string joy_num;

		token_helper( value, pos, joy_num, FE_WHITESPACE );

		if ( joy_num.compare( 0, 3, "Joy" ) == 0 )
		{
			int num = as_int( value.substr( 3 ) );

			std::string joy_name;
			token_helper( value, pos, joy_name );

			if (( num >= 0 ) && ( num <= static_cast<int>( FeJoystick::Count ) ))
				m_joy_config.push_back( std::pair<int, std::string>( num, joy_name ) );
		}

		return 0;
	}

	Command cmd = string_to_command( setting );
	if ( cmd == LAST_COMMAND )
	{
		invalid_setting( fn, "input_map", setting, commandStrings, NULL, "command" );
		return 1;
	}

	FeInputMapEntry new_entry( value, cmd );
	if ( new_entry.inputs.empty() )
	{
		FeLog() << "Unrecognized input type: " << value << " in file: " << fn << std::endl;
		return 1;
	}

	m_inputs.push_back( new_entry );

	if ( new_entry.has_mouse_move() )
		m_mmove_count++;

	return 0;
}

void FeInputMap::save( nowide::ofstream &f ) const
{
	for ( std::vector<FeInputMapEntry>::const_iterator it = m_inputs.begin(); it != m_inputs.end(); ++it )
		write_pair( f, commandStrings[(*it).command], (*it).as_string(), 1 );

	for ( int i=0; i < (int)Select; i++ )
		write_param(
			f,
			"default",
			commandStrings[i],
			( m_defaults[i] == LAST_COMMAND ) ? "" : commandStrings[m_defaults[i]],
			1
		);

	for ( size_t i=0; i < m_joy_config.size(); i++ )
		write_param(
			f,
			"map",
			"Joy" + m_joy_config[i].first,
			m_joy_config[i].second,
			1
		);
}

FeInputMap::Command FeInputMap::string_to_command( const std::string &s )
{
	int i=0;

	while ( FeInputMap::commandStrings[i] != NULL )
	{
		if ( s.compare( commandStrings[i] ) == 0 )
			return (Command)i;

		i++;
	}

	//
	// Redirect signals for backward compatibility
	//
	if ( s.compare( "prev_list" ) == 0 ) // in 1.5: prev_list -> prev_display
		return PrevDisplay;
	else if ( s.compare( "next_list" ) == 0 ) // in 1.5: next_list -> next_display
		return NextDisplay;
	else if ( s.compare( "page_up" ) == 0 ) // after 2.0.0: page_up -> prev_page
		return PrevPage;
	else if ( s.compare( "page_down" ) == 0 ) // after 2.0.0: page_down -> next_page
		return NextPage;
	else if ( s.compare( "exit_no_menu" ) == 0 ) // after 2.2.1: exit_no_menu -> exit_to_desktop
		return ExitToDesktop;
	else if ( s.compare( "reload" ) == 0 ) // after 3.1.0: reload -> reload_layout
		return ReloadLayout;

	return LAST_COMMAND;
}

// these are the commands that are repeatable if the input key is held down
//
bool FeInputMap::is_repeatable_command( FeInputMap::Command c )
{
	return (( c == PrevGame ) || ( c == NextGame )
		|| ( c == PrevPage ) || ( c == NextPage )
		|| ( c == NextLetter ) || ( c == PrevLetter )
		|| ( c == NextFavourite ) || ( c == PrevFavourite ));
}

// return true if c is the "Up", "Down", "Left", "Right", or "Back" command
//
bool FeInputMap::is_ui_command( FeInputMap::Command c )
{
	return ( c < Select );
}

// Note the alignment of settingStrings matters in fe_config.cpp
// (FeSoundMenu::get_options)
//
const char *FeSoundInfo::settingStrings[] =
{
	"sound_volume",
	"ambient_volume",
	"movie_volume",
	"loudness_normalisation",
	NULL
};
const char *FeSoundInfo::settingDispStrings[] =
{
	"Sound Volume",
	"Ambient Volume",
	"Movie Volume",
	"Loudness Normalisation",
	NULL
};

FeSoundInfo::FeSoundInfo()
	: m_sound_vol( 100 ),
	m_ambient_vol( 100 ),
	m_movie_vol( 100 ),
	m_mute( false ),
	m_loudness( true )
{
}

void FeSoundInfo::set_volume( SoundType t, const std::string &str )
{
	int v = as_int( str );

	if ( v < 0 )
		v = 0;
	else if ( v > 100 )
		v = 100;

	switch ( t )
	{
	case Movie:
		m_movie_vol = v;
		break;

	case Ambient:
		m_ambient_vol = v;
		break;

	case Sound:
	default:
		m_sound_vol = v;
	}
}

int FeSoundInfo::get_set_volume( SoundType t ) const
{
	switch ( t )
	{
	case Movie:
		return m_movie_vol;
	case Ambient:
		return m_ambient_vol;
	case Sound:
	default:
		return m_sound_vol;
	}
}

int FeSoundInfo::get_play_volume( SoundType t ) const
{
	if ( m_mute )
		return 0;

	return get_set_volume( t );
}

bool FeSoundInfo::get_mute() const
{
	return m_mute;
}

void FeSoundInfo::set_mute( bool m )
{
	m_mute=m;
}

bool FeSoundInfo::get_loudness() const
{
	return m_loudness;
}

void FeSoundInfo::set_loudness( bool enabled )
{
	m_loudness = enabled;
}

int FeSoundInfo::process_setting( const std::string &setting,
	const std::string &value,
	const std::string &fn )
{	if ( setting.compare( settingStrings[0] ) == 0 ) // sound_vol
		set_volume( Sound, value );
	else if ( setting.compare( settingStrings[1] ) == 0 ) // ambient_vol
		set_volume( Ambient, value );
	else if ( setting.compare( settingStrings[2] ) == 0 ) // movie_vol
		set_volume( Movie, value );
	else if ( setting.compare( settingStrings[3] ) == 0 ) // loudness
		set_loudness( config_str_to_bool( value ) );
	else
	{
		bool found=false;
		for ( int i=0; i < FeInputMap::LAST_EVENT; i++ )
		{
			if ( ( FeInputMap::commandStrings[i] )
				&& ( setting.compare( FeInputMap::commandStrings[i] ) == 0 ))
			{
				found=true;
				m_sounds[ (FeInputMap::Command)i ] = value;
				break;
			}
		}

		if (!found)
		{
			invalid_setting( fn, "sound", setting, settingStrings, FeInputMap::commandStrings );
			return 1;
		}
	}

	return 0;
}

bool FeSoundInfo::get_sound( FeInputMap::Command c, std::string &name ) const
{
	// Causes sound mappings to disappear permanently if Sound Volume is set to 0
	// if (( !m_mute ) && ( m_sound_vol > 0 ))
	{
		std::map<FeInputMap::Command, std::string>::const_iterator it;
		it = m_sounds.find( c );

		if ( it == m_sounds.end() )
			return false;

		name = (*it).second;
	}
	return true;
}

void FeSoundInfo::set_sound( FeInputMap::Command c, const std::string &name )
{
	if ( name.empty() )
		m_sounds.erase( c );
	else
		m_sounds[ c ] = name;
}

void FeSoundInfo::save( nowide::ofstream &f ) const
{
	std::map<FeInputMap::Command, std::string>::const_iterator it;

	for ( int i=0; i<3; i++ )
		write_pair( f, settingStrings[i], as_str( get_set_volume( (SoundType)i ) ), 1 );

	write_pair( f, settingStrings[3], m_loudness ? FE_CFG_YES_STR : FE_CFG_NO_STR, 1 );

	for ( it=m_sounds.begin(); it!=m_sounds.end(); ++it )
		write_pair( f, FeInputMap::commandStrings[ (*it).first ], (*it).second, 1 );
}
