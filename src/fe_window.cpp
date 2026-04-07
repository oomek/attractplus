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
#include "fe_input.hpp"
#include "fe_settings.hpp"
#include "fe_window.hpp"
#include "fe_present.hpp"
#include "fe_text.hpp"
#include "fe_listbox.hpp"
#include "fe_rectangle.hpp"
#include "tp.hpp"
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
#include <algorithm>
#include <array>
#include "nowide/fstream.hpp"

namespace
{
	struct FeVideoMode
	{
		Vec2u size;
		unsigned int bits_per_pixel;

		FeVideoMode( unsigned int width = 1280u, unsigned int height = 720u, unsigned int bpp = 32u )
			: size( width, height ),
			bits_per_pixel( bpp )
		{
		}
	};

	struct FeSdlJoystickSlot
	{
		SDL_JoystickID instance_id = 0;
		SDL_Joystick *joystick = nullptr;
		std::string name;
	};

	std::array<FeSdlJoystickSlot, FeJoystick::Count> g_sdl_joystick_slots = {};
	bool g_sdl_joysticks_initialized = false;

	bool ensure_sdl_joystick_subsystems()
	{
		if ( g_sdl_joysticks_initialized )
			return true;

		if ( !SDL_InitSubSystem( SDL_INIT_JOYSTICK ) )
			return false;

		SDL_SetJoystickEventsEnabled( true );

		if ( SDL_InitSubSystem( SDL_INIT_GAMEPAD ) )
			SDL_SetGamepadEventsEnabled( true );

		g_sdl_joysticks_initialized = true;
		return true;
	}

	void clear_sdl_joystick_slot( FeSdlJoystickSlot &slot )
	{
		if ( slot.joystick )
			SDL_CloseJoystick( slot.joystick );

		slot.instance_id = 0;
		slot.joystick = nullptr;
		slot.name.clear();
	}

	int find_sdl_joystick_slot( SDL_JoystickID instance_id )
	{
		for ( std::size_t i = 0; i < g_sdl_joystick_slots.size(); ++i )
		{
			if ( g_sdl_joystick_slots[i].instance_id == instance_id )
				return static_cast<int>( i );
		}

		return -1;
	}

	void sync_sdl_joystick_slots()
	{
		if ( !ensure_sdl_joystick_subsystems() )
			return;

		int joystick_count = 0;
		SDL_JoystickID *joysticks = SDL_GetJoysticks( &joystick_count );
		std::vector<SDL_JoystickID> active_ids;
		if ( joysticks && joystick_count > 0 )
			active_ids.assign( joysticks, joysticks + joystick_count );

		for ( FeSdlJoystickSlot &slot : g_sdl_joystick_slots )
		{
			if ( slot.instance_id == 0 )
				continue;

			if ( std::find( active_ids.begin(), active_ids.end(), slot.instance_id ) == active_ids.end() )
				clear_sdl_joystick_slot( slot );
		}

		for ( SDL_JoystickID instance_id : active_ids )
		{
			int slot_index = find_sdl_joystick_slot( instance_id );
			if ( slot_index < 0 )
			{
				for ( std::size_t i = 0; i < g_sdl_joystick_slots.size(); ++i )
				{
					if ( g_sdl_joystick_slots[i].instance_id == 0 )
					{
						slot_index = static_cast<int>( i );
						g_sdl_joystick_slots[i].instance_id = instance_id;
						break;
					}
				}
			}

			if ( slot_index < 0 )
				continue;

			FeSdlJoystickSlot &slot = g_sdl_joystick_slots[slot_index];
			if ( !slot.joystick )
				slot.joystick = SDL_OpenJoystick( instance_id );

			const char *name = slot.joystick
				? SDL_GetJoystickName( slot.joystick )
				: SDL_GetJoystickNameForID( instance_id );
			slot.name = name ? name : "";
		}

		if ( joysticks )
			SDL_free( joysticks );
	}

	int sdl_axis_index_from_fe( FeJoystick::Axis axis )
	{
		switch ( axis )
		{
		case FeJoystick::Axis::X: return 0;
		case FeJoystick::Axis::Y: return 1;
		case FeJoystick::Axis::Z: return 2;
		case FeJoystick::Axis::R: return 3;
		case FeJoystick::Axis::U: return 4;
		case FeJoystick::Axis::V: return 5;
		case FeJoystick::Axis::PovX: return 6;
		case FeJoystick::Axis::PovY: return 7;
		default: return -1;
		}
	}

	float normalize_sdl_axis_value( Sint16 value )
	{
		if ( value >= 0 )
			return static_cast<float>( value ) * ( 100.0f / 32767.0f );

		return static_cast<float>( value ) * ( 100.0f / 32768.0f );
	}

	FeJoystick::Axis sdl_gamepad_axis_to_fe( Uint8 axis )
	{
		switch ( axis )
		{
		case SDL_GAMEPAD_AXIS_LEFTX: return FeJoystick::Axis::X;
		case SDL_GAMEPAD_AXIS_LEFTY: return FeJoystick::Axis::Y;
		case SDL_GAMEPAD_AXIS_RIGHTX: return FeJoystick::Axis::Z;
		case SDL_GAMEPAD_AXIS_RIGHTY: return FeJoystick::Axis::R;
		case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: return FeJoystick::Axis::U;
		case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: return FeJoystick::Axis::V;
		default: return FeJoystick::Axis::X;
		}
	}

	bool get_sdl_desktop_geometry( bool do_multimon, FeVideoMode &mode, Vec2i &position )
	{
		int display_count = 0;
		SDL_DisplayID *displays = SDL_GetDisplays( &display_count );
		if ( !displays || display_count <= 0 )
		{
			if ( displays )
				SDL_free( displays );
			return false;
		}

		bool success = false;

		if ( do_multimon )
		{
			bool have_bounds = false;
			int left = 0;
			int top = 0;
			int right = 0;
			int bottom = 0;

			for ( int i = 0; i < display_count; ++i )
			{
				SDL_Rect rect = {};
				if ( !SDL_GetDisplayBounds( displays[i], &rect ) )
					continue;

				if ( !have_bounds )
				{
					left = rect.x;
					top = rect.y;
					right = rect.x + rect.w;
					bottom = rect.y + rect.h;
					have_bounds = true;
				}
				else
				{
					left = std::min( left, rect.x );
					top = std::min( top, rect.y );
					right = std::max( right, rect.x + rect.w );
					bottom = std::max( bottom, rect.y + rect.h );
				}
			}

			if ( have_bounds )
			{
				position = Vec2i( left, top );
				mode.size.x = static_cast<unsigned int>( std::max( right - left, 1 ) );
				mode.size.y = static_cast<unsigned int>( std::max( bottom - top, 1 ) );
				success = true;
			}
		}

		if ( !success )
		{
			SDL_DisplayID display = SDL_GetPrimaryDisplay();
			if ( !display )
				display = displays[0];

			SDL_Rect rect = {};
			if ( SDL_GetDisplayBounds( display, &rect ) )
				position = Vec2i( rect.x, rect.y );

			if ( const SDL_DisplayMode *desktop_mode = SDL_GetDesktopDisplayMode( display ) )
			{
				mode.size.x = static_cast<unsigned int>( std::max( desktop_mode->w, 1 ) );
				mode.size.y = static_cast<unsigned int>( std::max( desktop_mode->h, 1 ) );
				success = true;
			}
			else if ( rect.w > 0 && rect.h > 0 )
			{
				mode.size.x = static_cast<unsigned int>( rect.w );
				mode.size.y = static_cast<unsigned int>( rect.h );
				success = true;
			}
		}

		SDL_free( displays );
		return success;
	}

#if defined(SFML_SYSTEM_WINDOWS)
	bool get_win32_desktop_geometry( bool do_multimon, FeVideoMode &mode, Vec2i &position )
	{
		if ( do_multimon )
		{
			position.x = GetSystemMetrics( SM_XVIRTUALSCREEN );
			position.y = GetSystemMetrics( SM_YVIRTUALSCREEN );
			mode.size.x = static_cast<unsigned int>( std::max( GetSystemMetrics( SM_CXVIRTUALSCREEN ), 1 ) );
			mode.size.y = static_cast<unsigned int>( std::max( GetSystemMetrics( SM_CYVIRTUALSCREEN ), 1 ) );
			return true;
		}

		MONITORINFO monitor_info = {};
		monitor_info.cbSize = sizeof( monitor_info );
		if ( const HMONITOR monitor = MonitorFromPoint( POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY ) )
		{
			if ( GetMonitorInfo( monitor, &monitor_info ) )
			{
				const LONG width = monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
				const LONG height = monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;
				position.x = monitor_info.rcMonitor.left;
				position.y = monitor_info.rcMonitor.top;
				mode.size.x = static_cast<unsigned int>( std::max<LONG>( width, 1 ) );
				mode.size.y = static_cast<unsigned int>( std::max<LONG>( height, 1 ) );
				return true;
			}
		}

		position = Vec2i( 0, 0 );
		mode.size.x = static_cast<unsigned int>( std::max( GetSystemMetrics( SM_CXSCREEN ), 1 ) );
		mode.size.y = static_cast<unsigned int>( std::max( GetSystemMetrics( SM_CYSCREEN ), 1 ) );
		return true;
	}
#endif

	void apply_overlay_transform( std::vector<FeRenderGeometry> &geometry, const FeTransform &transform )
	{
		if ( transform.isIdentity() )
			return;

		for ( FeRenderGeometry &entry : geometry )
		{
			for ( FeRenderVertex &vertex : entry.vertices )
			{
				const Vec2f p = transform.transformPoint( { vertex.x, vertex.y } );
				vertex.x = p.x;
				vertex.y = p.y;
			}
		}
	}

	void append_solid_triangle(
		FeRenderGeometry &geometry,
		const Vec2f &p0,
		const Vec2f &p1,
		const Vec2f &p2,
		const Color &color )
	{
		FeRenderVertex v0 = {};
		v0.x = p0.x;
		v0.y = p0.y;
		v0.z = 0.0f;
		v0.r = color.r;
		v0.g = color.g;
		v0.b = color.b;
		v0.a = color.a;

		FeRenderVertex v1 = v0;
		v1.x = p1.x;
		v1.y = p1.y;

		FeRenderVertex v2 = v0;
		v2.x = p2.x;
		v2.y = p2.y;

		geometry.vertices.push_back( v0 );
		geometry.vertices.push_back( v1 );
		geometry.vertices.push_back( v2 );
	}

	void append_rect_fill( FeRenderGeometry &geometry, const FeTransform &transform, const Vec2f &size, const Color &color )
	{
		if ( color.a == 0 )
			return;

		const Vec2f p0 = transform.transformPoint( { 0.0f, 0.0f } );
		const Vec2f p1 = transform.transformPoint( { size.x, 0.0f } );
		const Vec2f p2 = transform.transformPoint( { 0.0f, size.y } );
		const Vec2f p3 = transform.transformPoint( { size.x, size.y } );
		append_solid_triangle( geometry, p0, p1, p2, color );
		append_solid_triangle( geometry, p2, p1, p3, color );
	}

	bool append_overlay_rect_geometry( std::vector<FeRenderGeometry> &geometry, const FeOverlayRect &rect )
	{
		if ( rect.color.a == 0 || rect.size.x <= 0.0f || rect.size.y <= 0.0f )
			return false;

		FeRenderGeometry fill;
		fill.clear();
		fill.textured = false;
		fill.texture_width = 1.0f;
		fill.texture_height = 1.0f;
		fill.zbuffer = false;

		const FeTransform transform = FeTransform().translate( { rect.position.x, rect.position.y } );
		append_rect_fill( fill, transform, rect.size, rect.color );
		if ( fill.vertices.empty() )
			return false;

		geometry.push_back( std::move( fill ) );
		return true;
	}

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

	Vec2i get_global_mouse_position()
	{
		float x = 0.0f;
		float y = 0.0f;
		SDL_GetGlobalMouseState( &x, &y );
		return Vec2i( static_cast<int>( x ), static_cast<int>( y ) );
	}

	void set_global_mouse_position( const Vec2i &pos )
	{
		SDL_WarpMouseGlobal( static_cast<float>( pos.x ), static_cast<float>( pos.y ) );
	}

#if defined(USE_XLIB)
	unsigned long get_x11_window_number( const FeSdl3GpuContext &context )
	{
		return static_cast<unsigned long>(
			reinterpret_cast<std::uintptr_t>( context.get_native_window_handle() ) );
	}
#endif

	FeJoystick::Axis sdl_joystick_axis_to_fe( Uint8 axis )
	{
		switch ( axis )
		{
		case 0: return FeJoystick::Axis::X;
		case 1: return FeJoystick::Axis::Y;
		case 2: return FeJoystick::Axis::Z;
		case 3: return FeJoystick::Axis::R;
		case 4: return FeJoystick::Axis::U;
		case 5: return FeJoystick::Axis::V;
		case 6: return FeJoystick::Axis::PovX;
		case 7: return FeJoystick::Axis::PovY;
		default: return FeJoystick::Axis::X;
		}
	}

	FeEvent::Vector2i sdl_touch_position_to_window( const SDL_TouchFingerEvent &event, SDL_Window *window )
	{
		int width = 0;
		int height = 0;
		if ( window )
			SDL_GetWindowSize( window, &width, &height );

		FeEvent::Vector2i position = {};
		position.x = static_cast<int>( std::lround( event.x * static_cast<float>( std::max( width, 1 ) ) ) );
		position.y = static_cast<int>( std::lround( event.y * static_cast<float>( std::max( height, 1 ) ) ) );
		return position;
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
		case SDL_EVENT_FINGER_DOWN:
		case SDL_EVENT_FINGER_UP:
		case SDL_EVENT_FINGER_MOTION:
		case SDL_EVENT_FINGER_CANCELED:
			return event.tfinger.windowID == window_id;
		default:
			return true;
		}
	}

	std::optional<FeEvent> translate_sdl_event( const SDL_Event &event, SDL_Window *window, bool key_repeat_enabled )
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
			return FeEvent::Closed{};

		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			return FeEvent::Closed{};

		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			return FeEvent::FocusGained{};

		case SDL_EVENT_WINDOW_FOCUS_LOST:
			return FeEvent::FocusLost{};

		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			return FeEvent::Resized{ { static_cast<unsigned int>( event.window.data1 ),
				static_cast<unsigned int>( event.window.data2 ) } };

		case SDL_EVENT_TEXT_INPUT:
		{
			const char32_t unicode = decode_utf8_first_codepoint( event.text.text );
			if ( unicode != 0 )
				return FeEvent::TextEntered{ unicode };
			return {};
		}

		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
		{
			if (( event.type == SDL_EVENT_KEY_DOWN ) && event.key.repeat && !key_repeat_enabled )
				return {};

			const SDL_Keymod mods = SDL_GetModState();
			const int key = static_cast<int>( event.key.scancode );
			const bool alt = ( mods & SDL_KMOD_ALT ) != 0;
			const bool control = ( mods & SDL_KMOD_CTRL ) != 0;
			const bool shift = ( mods & SDL_KMOD_SHIFT ) != 0;
			const bool system = ( mods & SDL_KMOD_GUI ) != 0;

			if ( event.type == SDL_EVENT_KEY_DOWN )
				return FeEvent::KeyPressed{ key, alt, control, shift, system };

			return FeEvent::KeyReleased{ key, alt, control, shift, system };
		}

		case SDL_EVENT_MOUSE_MOTION:
			return FeEvent::MouseMoved{ { static_cast<int>( event.motion.x ), static_cast<int>( event.motion.y ) } };

		case SDL_EVENT_MOUSE_WHEEL:
		{
			const FeEvent::MouseWheel wheel = ( event.wheel.x != 0.0f ) ? FeEvent::MouseWheel::Horizontal : FeEvent::MouseWheel::Vertical;
			const float delta = ( event.wheel.x != 0.0f ) ? event.wheel.x : event.wheel.y;
			return FeEvent::MouseWheelScrolled{
				wheel,
				delta,
				{ static_cast<int>( event.wheel.mouse_x ), static_cast<int>( event.wheel.mouse_y ) }
			};
		}

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			return FeEvent::MouseButtonPressed{
				static_cast<int>( event.button.button ),
				{ static_cast<int>( event.button.x ), static_cast<int>( event.button.y ) }
			};

		case SDL_EVENT_MOUSE_BUTTON_UP:
			return FeEvent::MouseButtonReleased{
				static_cast<int>( event.button.button ),
				{ static_cast<int>( event.button.x ), static_cast<int>( event.button.y ) }
			};

		case SDL_EVENT_FINGER_MOTION:
			return FeEvent::TouchMoved{
				static_cast<unsigned int>( event.tfinger.fingerID ),
				sdl_touch_position_to_window( event.tfinger, window )
			};

		case SDL_EVENT_FINGER_DOWN:
			return FeEvent::TouchBegan{
				static_cast<unsigned int>( event.tfinger.fingerID ),
				sdl_touch_position_to_window( event.tfinger, window )
			};

		case SDL_EVENT_FINGER_UP:
		case SDL_EVENT_FINGER_CANCELED:
			return FeEvent::TouchEnded{
				static_cast<unsigned int>( event.tfinger.fingerID ),
				sdl_touch_position_to_window( event.tfinger, window )
			};

		case SDL_EVENT_JOYSTICK_AXIS_MOTION:
		{
			const int joystick_id = fe_joystick_translate_sdl_instance_id( static_cast<int>( event.jaxis.which ) );
			return FeEvent::JoystickMoved{
				static_cast<unsigned int>( ( joystick_id >= 0 ) ? joystick_id : 0 ),
				static_cast<int>( sdl_joystick_axis_to_fe( event.jaxis.axis ) ),
				normalize_sdl_axis_value( event.jaxis.value )
			};
		}

		case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
		{
			const int joystick_id = fe_joystick_translate_sdl_instance_id( static_cast<int>( event.jbutton.which ) );
			return FeEvent::JoystickButtonPressed{
				static_cast<unsigned int>( ( joystick_id >= 0 ) ? joystick_id : 0 ),
				static_cast<unsigned int>( event.jbutton.button )
			};
		}

		case SDL_EVENT_JOYSTICK_BUTTON_UP:
		{
			const int joystick_id = fe_joystick_translate_sdl_instance_id( static_cast<int>( event.jbutton.which ) );
			return FeEvent::JoystickButtonReleased{
				static_cast<unsigned int>( ( joystick_id >= 0 ) ? joystick_id : 0 ),
				static_cast<unsigned int>( event.jbutton.button )
			};
		}

		case SDL_EVENT_JOYSTICK_ADDED:
			fe_joystick_refresh_devices();
			return FeEvent::JoystickConnected{ 0u };

		case SDL_EVENT_JOYSTICK_REMOVED:
			fe_joystick_refresh_devices();
			return FeEvent::JoystickDisconnected{ 0u };

		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		{
			const int joystick_id = fe_joystick_translate_sdl_instance_id( static_cast<int>( event.gaxis.which ) );
			return FeEvent::JoystickMoved{
				static_cast<unsigned int>( ( joystick_id >= 0 ) ? joystick_id : 0 ),
				static_cast<int>( sdl_gamepad_axis_to_fe( event.gaxis.axis ) ),
				normalize_sdl_axis_value( event.gaxis.value )
			};
		}

		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
		{
			const int joystick_id = fe_joystick_translate_sdl_instance_id( static_cast<int>( event.gbutton.which ) );
			return FeEvent::JoystickButtonPressed{
				static_cast<unsigned int>( ( joystick_id >= 0 ) ? joystick_id : 0 ),
				static_cast<unsigned int>( event.gbutton.button )
			};
		}

		case SDL_EVENT_GAMEPAD_BUTTON_UP:
		{
			const int joystick_id = fe_joystick_translate_sdl_instance_id( static_cast<int>( event.gbutton.which ) );
			return FeEvent::JoystickButtonReleased{
				static_cast<unsigned int>( ( joystick_id >= 0 ) ? joystick_id : 0 ),
				static_cast<unsigned int>( event.gbutton.button )
			};
		}

		case SDL_EVENT_GAMEPAD_ADDED:
			fe_joystick_refresh_devices();
			return FeEvent::JoystickConnected{ 0u };

		case SDL_EVENT_GAMEPAD_REMOVED:
			fe_joystick_refresh_devices();
			return FeEvent::JoystickDisconnected{ 0u };

		case SDL_EVENT_GAMEPAD_REMAPPED:
			fe_joystick_refresh_devices();
			return FeEvent::JoystickConnected{ 0u };

		default:
			return {};
		}
	}

}

void fe_joystick_update()
{
	if ( ensure_sdl_joystick_subsystems() )
		sync_sdl_joystick_slots();
}

void fe_joystick_refresh_devices()
{
	fe_joystick_update();
}

void fe_joystick_shutdown()
{
	for ( FeSdlJoystickSlot &slot : g_sdl_joystick_slots )
		clear_sdl_joystick_slot( slot );
}

bool fe_joystick_is_connected( unsigned int joystick_id )
{
	if ( !ensure_sdl_joystick_subsystems() )
		return false;

	sync_sdl_joystick_slots();
	return joystick_id < g_sdl_joystick_slots.size()
		&& g_sdl_joystick_slots[joystick_id].instance_id != 0
		&& g_sdl_joystick_slots[joystick_id].joystick != nullptr;
}

bool fe_joystick_is_button_pressed( unsigned int joystick_id, unsigned int button )
{
	if ( !ensure_sdl_joystick_subsystems() )
		return false;

	sync_sdl_joystick_slots();
	if ( joystick_id >= g_sdl_joystick_slots.size() || !g_sdl_joystick_slots[joystick_id].joystick )
		return false;

	return SDL_GetJoystickButton( g_sdl_joystick_slots[joystick_id].joystick, static_cast<int>( button ) );
}

float fe_joystick_get_axis_position( unsigned int joystick_id, FeJoystick::Axis axis )
{
	if ( !ensure_sdl_joystick_subsystems() )
		return 0.0f;

	sync_sdl_joystick_slots();
	if ( joystick_id >= g_sdl_joystick_slots.size() || !g_sdl_joystick_slots[joystick_id].joystick )
		return 0.0f;

	const int axis_index = sdl_axis_index_from_fe( axis );
	if ( axis_index < 0 )
		return 0.0f;

	return normalize_sdl_axis_value(
		SDL_GetJoystickAxis( g_sdl_joystick_slots[joystick_id].joystick, axis_index ) );
}

std::string fe_joystick_get_name( unsigned int joystick_id )
{
	if ( !ensure_sdl_joystick_subsystems() )
		return "";

	sync_sdl_joystick_slots();
	if ( joystick_id >= g_sdl_joystick_slots.size() )
		return "";

	return g_sdl_joystick_slots[joystick_id].name;
}

int fe_joystick_translate_sdl_instance_id( int instance_id )
{
	if ( !ensure_sdl_joystick_subsystems() )
		return instance_id;

	sync_sdl_joystick_slots();
	return find_sdl_joystick_slot( static_cast<SDL_JoystickID>( instance_id ) );
}

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
	const Vec2i &pos,
	const Vec2u &size
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

	return CallWindowProc( s_window_wnd_proc, hwnd, msg, wParam, lParam );
}
#endif

FeWindow::FeWindow( FeSettings &fes )
	: m_fes( fes ),
	m_running_pid( 0 ),
	m_running_wnd( NULL ),
	m_win_mode( 0 )
{
}

FeWindow::~FeWindow()
{
	if ( m_running_pid && process_exists( m_running_pid ) )
		kill_program( m_running_pid );

#if defined(SFML_SYSTEM_WINDOWS)
	close_blackout();
#endif
	fe_joystick_shutdown();
}

bool FeWindow::owns_sdl_window() const
{
	return m_gpu_context.get_window() != nullptr;
}

const FeRenderRawTextureSource *FeWindow::cache_overlay_image( const SDL_Surface &image )
{
	OverlayTextureEntry &entry = m_overlay_images[ &image ];
	if ( !image.pixels || image.w <= 0 || image.h <= 0 )
		return nullptr;

	const std::size_t row_size = static_cast<std::size_t>( image.w ) * 4;
	entry.pixels.resize( static_cast<std::size_t>( image.h ) * row_size );
	for ( int y = 0; y < image.h; ++y )
	{
		const std::uint8_t *src = static_cast<const std::uint8_t *>( image.pixels ) + ( static_cast<std::size_t>( y ) * image.pitch );
		std::uint8_t *dst = entry.pixels.data() + ( static_cast<std::size_t>( y ) * row_size );
		std::memcpy( dst, src, row_size );
	}

	entry.source.pixels = entry.pixels.data();
	entry.source.width = static_cast<unsigned int>( image.w );
	entry.source.height = static_cast<unsigned int>( image.h );
	return &entry.source;
}

bool FeWindow::append_native_overlay_item( const FeOverlayDrawItem &item, const FeTransform &transform )
{
	std::vector<FeRenderGeometry> geometry;

	switch ( item.type )
	{
	case FeOverlayDrawItem::OverlayRect:
		if ( !append_overlay_rect_geometry( geometry, *static_cast<const FeOverlayRect *>( item.item ) ) )
			return true;
		break;
	case FeOverlayDrawItem::TextPrimitive:
		static_cast<const FeTextPrimitive *>( item.item )->append_render_geometry( geometry, 0.0f );
		break;
	case FeOverlayDrawItem::ListBox:
		static_cast<const FeListBox *>( item.item )->build_render_geometry( geometry );
		break;
	case FeOverlayDrawItem::Text:
		static_cast<const FeText *>( item.item )->build_render_geometry( geometry );
		break;
	case FeOverlayDrawItem::Rectangle:
		{
			FeRenderGeometry entry;
			if ( static_cast<const FeRectangle *>( item.item )->build_render_geometry( entry ) )
				geometry.push_back( std::move( entry ) );
			else
				return true;
		}
		break;
	}

	apply_overlay_transform( geometry, transform );
	for ( FeRenderGeometry &entry : geometry )
		entry.zbuffer = false;
	m_overlay_geometry.insert( m_overlay_geometry.end(), geometry.begin(), geometry.end() );
	return true;
}

void FeWindow::draw_overlay_image( const SDL_Surface &image, const FloatRect &bounds, bool smooth, const Color &color )
{
	if ( !owns_sdl_window() )
		return;

	const FeRenderRawTextureSource *source = cache_overlay_image( image );
	if ( !source || source->width == 0 || source->height == 0 )
		return;

	FeRenderGeometry entry;
	entry.clear();
	entry.texture_id = source;
	entry.texture_source_type = FeRenderTextureSourceRawRgba;
	entry.texture_repeated = false;
	entry.texture_smooth = smooth;
	entry.texture_mipmap = false;
	entry.texture_width = static_cast<float>( source->width );
	entry.texture_height = static_cast<float>( source->height );
	entry.blend_mode = FeBlend::Alpha;
	entry.zbuffer = false;
	entry.shader = nullptr;
	entry.custom_shader = false;
	entry.textured = true;
	entry.texture_dynamic = false;
	entry.texture_content_version = 1;
	entry.vertices.reserve( 6 );

	const float left = bounds.position.x;
	const float top = bounds.position.y;
	const float right = bounds.position.x + bounds.size.x;
	const float bottom = bounds.position.y + bounds.size.y;
	const Vec2f positions[6] = {
		{ left, top },
		{ right, top },
		{ left, bottom },
		{ left, bottom },
		{ right, top },
		{ right, bottom }
	};
	const Vec2f texcoords[6] = {
		{ 0.0f, 0.0f },
		{ static_cast<float>( source->width ), 0.0f },
		{ 0.0f, static_cast<float>( source->height ) },
		{ 0.0f, static_cast<float>( source->height ) },
		{ static_cast<float>( source->width ), 0.0f },
		{ static_cast<float>( source->width ), static_cast<float>( source->height ) }
	};

	for ( int i = 0; i < 6; ++i )
	{
		FeRenderVertex vertex = {};
		vertex.x = positions[i].x;
		vertex.y = positions[i].y;
		vertex.z = 0.0f;
		vertex.u = texcoords[i].x;
		vertex.v = texcoords[i].y;
		vertex.r = color.r;
		vertex.g = color.g;
		vertex.b = color.b;
		vertex.a = color.a;
		entry.vertices.push_back( vertex );
	}

	m_overlay_geometry.push_back( std::move( entry ) );
}

void FeWindow::display()
{
	bool used_sdl_gpu_present = false;
	if ( m_gpu_context.should_present() )
		used_sdl_gpu_present = m_gpu_context.execute_frame( m_overlay_geometry.empty() ? nullptr : &m_overlay_geometry );

	m_overlay_geometry.clear();
	m_overlay_images.clear();

	// Starting from Windows Vista non fullscreen window modes
	// should be synced by DWM, instead of v-sync
#ifdef SFML_SYSTEM_WINDOWS
	if ( !used_sdl_gpu_present && ( m_win_mode != FeSettings::Fullscreen ) )
		DwmFlush();
	check_for_sleep();
#endif
}

#ifdef SFML_SYSTEM_WINDOWS
HWND FeWindow::get_blackout_hwnd() const
{
	if ( !m_blackout )
		return nullptr;

	const SDL_PropertiesID props = SDL_GetWindowProperties( m_blackout );
	if ( !props )
		return nullptr;

	return static_cast<HWND>( SDL_GetPointerProperty( props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr ) );
}

bool FeWindow::ensure_blackout_window( const Vec2i &screen_pos, const Vec2u &screen_size )
{
	close_blackout();

	m_blackout = SDL_CreateWindow( "", 16, 16, SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN );
	if ( !m_blackout )
	{
		FeLog() << "WARNING: Failed to create blackout SDL window: " << SDL_GetError() << std::endl;
		return false;
	}

	SDL_SetWindowSize(
		m_blackout,
		static_cast<int>( screen_size.x + 2u ),
		static_cast<int>( screen_size.y + 2u ) );
	SDL_SetWindowPosition( m_blackout, screen_pos.x - 1, screen_pos.y - 1 );

	if ( const HWND hwnd = get_blackout_hwnd() )
	{
		const LONG_PTR style = GetWindowLongPtr( hwnd, GWL_EXSTYLE );
		SetWindowLongPtr( hwnd, GWL_EXSTYLE, style | WS_EX_TOOLWINDOW );
	}

	SDL_ShowWindow( m_blackout );
	clear_blackout();
	return true;
}

void FeWindow::clear_blackout()
{
	if ( const HWND hwnd = get_blackout_hwnd() )
	{
		RECT rect = {};
		GetClientRect( hwnd, &rect );
		if ( HDC dc = GetDC( hwnd ) )
		{
			FillRect( dc, &rect, static_cast<HBRUSH>( GetStockObject( BLACK_BRUSH ) ) );
			ReleaseDC( hwnd, dc );
		}
		UpdateWindow( hwnd );
	}
}

void FeWindow::close_blackout()
{
	if ( m_blackout )
	{
		SDL_DestroyWindow( m_blackout );
		m_blackout = nullptr;
	}
}

bool FeWindow::blackout_has_focus() const
{
	return m_blackout && ( SDL_GetKeyboardFocus() == m_blackout || SDL_GetMouseFocus() == m_blackout );
}

void FeWindow::check_for_sleep()
{
	if ( s_system_resumed )
	{
		FeDebug() << "! NOTE: Resume from sleep detected. Resetting audio device" << std::endl;
		fe_sleep( fe_milliseconds( 2000 ) ); // Wait 2 seconds to allow audio devices to wake up

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
					fe_sleep( fe_milliseconds( 100 ) );
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
	if ( owns_sdl_window() )
		save();

	if ( owns_sdl_window() )
		m_gpu_context.release_window();

	bool do_multimon = is_multimon_config( m_fes );
	m_win_mode = m_fes.get_window_mode();
	const bool use_sdl_owned_window = true;
	const bool sdl_video_ready = use_sdl_owned_window && m_gpu_context.ensure_video_subsystem();
	if ( use_sdl_owned_window )
		fe_joystick_refresh_devices();

	Vec2i wpos( 0, 0 );  // position to set window to
	FeVideoMode vm( 1280u, 720u, 32u ); // width/height/bpp of surface to create

#if !defined(SFML_SYSTEM_WINDOWS)
	if ( sdl_video_ready )
		get_sdl_desktop_geometry( do_multimon, vm, wpos );
#endif

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

	get_win32_desktop_geometry( do_multimon, vm, wpos );

	const Vec2i screen_pos = wpos;
	const Vec2u screen_size( vm.size.x, vm.size.y );

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
		wpos = screen_pos;
		wpos.x -= 1;
		wpos.y -= 1;
		vm.size.x = screen_size.x;
		vm.size.y = screen_size.y;
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
			m_win_pos.m_pos = Vec2i( x, y );
			m_win_pos.m_size = Vec2u( static_cast<unsigned int>( w ), static_cast<unsigned int>( h ) );
			m_win_pos.load_from_file( m_fes.get_config_dir() + FE_CFG_SUBDIR + FE_WINDOW_FILE );
		}

		// Minimum window size (1x1) prevents (0x0) crash (may be set in window.am or commandline)
		m_win_pos.m_size.x = std::max( m_win_pos.m_size.x, (unsigned int)1 );
		m_win_pos.m_size.y = std::max( m_win_pos.m_size.y, (unsigned int)1 );

		IntRect window_rect(
			m_win_pos.m_pos.x,
			m_win_pos.m_pos.y,
			static_cast<int>( m_win_pos.m_size.x ),
			static_cast<int>( m_win_pos.m_size.y ) );
#if !defined(NO_MULTIMON) && defined(SFML_SYSTEM_WINDOWS)
		// The window can be positioned anywhere in the virtual screen
		IntRect vm_rect(
			GetSystemMetrics( SM_XVIRTUALSCREEN ),
			GetSystemMetrics( SM_YVIRTUALSCREEN ),
			GetSystemMetrics( SM_CXVIRTUALSCREEN ),
			GetSystemMetrics( SM_CYVIRTUALSCREEN ) );
#else
		// No multi-monitor support
		IntRect vm_rect(
			wpos.x,
			wpos.y,
			static_cast<int>( vm.size.x ),
			static_cast<int>( vm.size.y ) );
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

	Vec2i wsize( static_cast<int>( vm.size.x ), static_cast<int>( vm.size.y ) );

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
	if (( m_win_mode == FeSettings::WindowNoBorder ) && ( screen_size == Vec2u( vm.size.x, vm.size.y ) ))
	{
		FeLog() << "Borderless window size matches the display resolution. Switching to Fullscreen." << std::endl;
		m_win_mode = FeSettings::Fullscreen;
		wpos = screen_pos;
	}

	// To avoid problems with black screen on launching games when window mode is set to Fullscreen
	// we hide the main renderwindow and show this m_blackout window instead
	// which has the extended size by 1 pixel in each direction to stop Windows
	// from treating it as exclusive borderless.
	//
	if ( m_win_mode == FeSettings::Fullscreen )
		ensure_blackout_window( screen_pos, screen_size );
	else
		close_blackout();
#endif

	if ( use_sdl_owned_window )
	{
		const auto try_create_sdl_window =
			[&]( std::string *error_message ) -> bool
			{
				SDL_WindowFlags flags = 0;
#if defined(SFML_SYSTEM_WINDOWS)
				flags = static_cast<SDL_WindowFlags>( flags | SDL_WINDOW_HIDDEN );
#else
				flags = static_cast<SDL_WindowFlags>( flags | SDL_WINDOW_VULKAN );
#endif
				if ( m_win_mode == FeSettings::Window )
					flags = static_cast<SDL_WindowFlags>( flags | SDL_WINDOW_RESIZABLE );
				else if ( m_win_mode == FeSettings::Fullscreen )
					flags = static_cast<SDL_WindowFlags>( flags | SDL_WINDOW_FULLSCREEN );
				else
					flags = static_cast<SDL_WindowFlags>( flags | SDL_WINDOW_BORDERLESS );

				SDL_Window *sdl_window = SDL_CreateWindow( FE_NAME, static_cast<int>( vm.size.x ), static_cast<int>( vm.size.y ), flags );
				if ( !sdl_window )
				{
					if ( error_message )
						*error_message = SDL_GetError();
					FeLog() << "WARNING: SDL3 GPU SDL window creation failed: " << SDL_GetError() << std::endl;
					return false;
				}

				if ( m_win_mode == FeSettings::Window )
					SDL_SetWindowPosition( sdl_window, wpos.x, wpos.y );
#if defined(SFML_SYSTEM_WINDOWS)
				else
					SDL_SetWindowPosition( sdl_window, wpos.x, wpos.y );
#endif

				if ( !m_gpu_context.claim_window( sdl_window ) )
				{
					if ( error_message )
						*error_message = SDL_GetError();
					FeLog() << "WARNING: SDL3 GPU window claim failed: " << SDL_GetError() << std::endl;
					SDL_DestroyWindow( sdl_window );
					return false;
				}

#if defined(SFML_SYSTEM_WINDOWS)
				void *native_handle = m_gpu_context.get_native_window_handle();
				if ( !native_handle )
				{
					if ( error_message )
						*error_message = "SDL3 GPU native window handle lookup failed.";
					FeLog() << "WARNING: SDL3 GPU native window handle lookup failed." << std::endl;
					m_gpu_context.release_window();
					return false;
				}
#endif

				SDL_ShowWindow( sdl_window );
				if ( error_message )
					error_message->clear();
				return true;
			};

		if ( !sdl_video_ready )
			FeLog() << "WARNING: SDL3 video initialization failed before SDL window creation." << std::endl;
		else
			try_create_sdl_window( nullptr );
	}

	// On Windows Vista and above all non fullscreen window modes
	// go through DWM. We have to disable vsync
	// when we rely solely on DwmFlush()

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
		if ( const unsigned long window = get_x11_window_number( m_gpu_context ) )
			set_x11_fullscreen_state( window );
	}
#endif

	FeDebug() << "Created " << FE_NAME << " Window: " << wsize.x << "x" << wsize.y << " @ "
		<< wpos.x << "," << wpos.y << " [OpenGL surface: "
		<< vm.size.x << "x" << vm.size.y << " bpp=" << vm.bits_per_pixel << "]" << std::endl;

#if defined(SFML_SYSTEM_WINDOWS)
	HWND hwnd = nullptr;
	hwnd = static_cast<HWND>( m_gpu_context.get_native_window_handle() );
	if ( hwnd )
	{
		set_win32_foreground_window( hwnd, m_fes.get_window_topmost() ? HWND_TOPMOST : HWND_TOP );

		// Enable dark mode titlebar on Windows
		BOOL value = TRUE;
		DwmSetWindowAttribute( hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof( value ));

		s_window_wnd_proc = reinterpret_cast<WNDPROC>( GetWindowLongPtr( hwnd, GWLP_WNDPROC ));
		SetWindowLongPtr( hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( CustomWndProc ));

		// Trigger title bar redraw to fix Win10 dark-mode (initially draws in light-mode)
		display();
		SendMessage(hwnd, WM_NCACTIVATE, FALSE, 0);
		SendMessage(hwnd, WM_NCACTIVATE, TRUE, 0);
	}
#endif

	m_fes.init_mouse_capture( this );

	// Only mess with the mouse position if mouse moves mapped
	if ( m_fes.has_mouse_moves() )
		set_mouse_position( wsize / 2 );
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
		fe_sleep( fe_milliseconds( 1000 ) );
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
			if ( ev.has_value() && ev->is<FeEvent::Closed>() )
				return;
		}
	}
}

bool FeWindow::run()
{
	Vec2i reset_pos;
	bool windowed = is_windowed_mode( m_fes.get_window_mode() );

	if ( !windowed && m_fes.get_info_bool( FeSettings::MoveMouseOnLaunch ) )
	{
		// Move the mouse to the bottom right corner so it isn't visible
		// when the emulator launches.
		//
		reset_pos = get_global_mouse_position();

		Vec2i hide_pos = get_position();
		hide_pos.x += static_cast<int>( get_size().x ) - 1;
		hide_pos.y += static_cast<int>( get_size().y ) - 1;

		set_global_mouse_position( hide_pos );
	}

	FeClock timer;

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
	// On DRM/KMS we must fully release the SDL GPU device before the emulator starts,
	// otherwise the frontend can keep the display path busy after the window is gone.
	m_gpu_context.shutdown();
#endif

	bool have_paused_prog = m_running_pid && process_exists( m_running_pid );

#if defined(SFML_SYSTEM_WINDOWS)
	auto get_frontend_hwnd = [&]() -> HWND
	{
		return static_cast<HWND>( m_gpu_context.get_native_window_handle() );
	};

	if ( m_win_mode == FeSettings::Fullscreen )
	{
		m_gpu_context.present_blank_frame();
		if ( HWND hwnd = get_frontend_hwnd() )
			set_win32_foreground_window( hwnd, HWND_BOTTOM );
		clear_blackout();
		if ( SDL_Window *window = m_gpu_context.get_window() )
		{
			SDL_HideWindow( window );
		}
		if ( const HWND blackout_hwnd = get_blackout_hwnd() )
			set_win32_foreground_window( blackout_hwnd, HWND_TOP );
	}
	else
	{
		if ( HWND hwnd = get_frontend_hwnd() )
			set_win32_foreground_window( hwnd, HWND_TOP );
		if ( !is_multimon_config( m_fes ))
			m_gpu_context.present_blank_frame();
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
				if ( ev.has_value() && ev->is<FeEvent::Closed>() )
					return false;
			}

#if defined(SFML_SYSTEM_WINDOWS)
			has_focus = hasFocus() || blackout_has_focus();
#else
			has_focus = hasFocus();
#endif

			const FeTime elapsed_time = timer.getElapsedTime();
			if (( elapsed_time >= fe_seconds( nbm_wait ) )
				&& ( has_focus ))
			{
				FeDebug() << "Attract-Mode Plus has focus, stopped non-blocking wait after "
					<< elapsed_time.asSeconds() << "s" << std::endl;

				done_wait = true;
			}
			else
				fe_sleep( fe_milliseconds( 25 ) );
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
	if ( const unsigned long window = get_x11_window_number( m_gpu_context ) )
		set_x11_foreground_window( window );
 #endif

#elif defined(SFML_SYSTEM_MACOS)
	osx_take_focus();
#elif defined(SFML_SYSTEM_WINDOWS)
	if ( m_win_mode == FeSettings::Fullscreen )
	{
		clear_blackout();
		if ( SDL_Window *window = m_gpu_context.get_window() )
		{
			SDL_ShowWindow( window );
		}

		// Since we are double/triple buffering in fullscreen
		// we need to clear the frames rendered ahead
		// to avoid back buffer flashing on game launch/exit
		for ( int i = 0; i < 3; i++ )
			m_gpu_context.present_blank_frame();
		if ( HWND hwnd = get_frontend_hwnd() )
			set_win32_foreground_window( hwnd, HWND_TOP );
	}
	else
	{
		if ( HWND hwnd = get_frontend_hwnd() )
			set_win32_foreground_window( hwnd, HWND_TOP );
	}
#endif

	if ( !windowed && m_fes.get_info_bool( FeSettings::MoveMouseOnLaunch ) )
		set_global_mouse_position( reset_pos );

	// Empty the window event queue, so we don't go triggering other
	// right away after running an emulator

	while ( const std::optional ev = pollEvent() )
	{
		if ( ev->is<FeEvent::Closed>() )
			return false;
	}

	FeDebug() << "Resuming frontend after game launch" << std::endl;

	return true;
}

void FeWindow::on_exit()
{
#if defined(SFML_SYSTEM_WINDOWS)
	close_blackout();
#endif
}

bool FeWindow::has_running_process()
{
	return ( m_running_pid != 0 );
}

void FeWindow::save()
{
	if ( is_windowed_mode( m_win_mode ) && !m_win_pos.m_temporary )
	{
		if ( SDL_Window *window = m_gpu_context.get_window() )
		{
			int x = 0;
			int y = 0;
			int w = 0;
			int h = 0;
			SDL_GetWindowPosition( window, &x, &y );
			SDL_GetWindowSize( window, &w, &h );
			FeWindowPosition win_pos( Vec2i( x, y ), Vec2u( static_cast<unsigned int>( w ), static_cast<unsigned int>( h ) ) );
			win_pos.save( m_fes.get_config_dir() + FE_CFG_SUBDIR + FE_WINDOW_FILE );
		}
	}
}

void FeWindow::close()
{
	save();
	m_gpu_context.release_window();
}

bool FeWindow::hasFocus()
{
	if ( SDL_Window *window = m_gpu_context.get_window() )
		return ( SDL_GetKeyboardFocus() == window || SDL_GetMouseFocus() == window );

	return false;
}

bool FeWindow::isOpen()
{
	if ( m_gpu_context.get_window() )
		return true;

	return false;
}

Vec2u FeWindow::get_size() const
{
	if ( SDL_Window *window = m_gpu_context.get_window() )
	{
		int width = 0;
		int height = 0;
		SDL_GetWindowSize( window, &width, &height );
		return Vec2u( static_cast<unsigned int>( width ), static_cast<unsigned int>( height ) );
	}

	return {};
}

Vec2i FeWindow::get_position() const
{
	if ( SDL_Window *window = m_gpu_context.get_window() )
	{
		int x = 0;
		int y = 0;
		SDL_GetWindowPosition( window, &x, &y );
		return Vec2i( x, y );
	}

	return {};
}

Vec2i FeWindow::get_mouse_position() const
{
	if ( SDL_Window *window = m_gpu_context.get_window() )
	{
		float x = 0.0f;
		float y = 0.0f;
		SDL_GetMouseState( &x, &y );
		return Vec2i( static_cast<int>( x ), static_cast<int>( y ) );
	}

	return {};
}

void FeWindow::set_mouse_position( const Vec2i &pos )
{
	if ( SDL_Window *window = m_gpu_context.get_window() )
		SDL_WarpMouseInWindow( window, static_cast<float>( pos.x ), static_cast<float>( pos.y ) );
}

void FeWindow::set_key_repeat_enabled( bool enabled )
{
	m_key_repeat_enabled = enabled;
}

void FeWindow::set_text_input_enabled( bool enabled )
{
	if ( SDL_Window *window = m_gpu_context.get_window() )
	{
		if ( enabled )
			SDL_StartTextInput( window );
		else if ( SDL_TextInputActive( window ) )
			SDL_StopTextInput( window );
	}
}

void FeWindow::set_mouse_cursor_visible( bool visible )
{
	if ( SDL_Window *window = m_gpu_context.get_window() )
		visible ? SDL_ShowCursor() : SDL_HideCursor();
}

void FeWindow::on_resize( const Vec2u &size )
{
	(void)size;
}

bool FeWindow::save_screenshot( const std::string &filename )
{
	return m_gpu_context.save_screenshot( filename );
}

void FeWindow::clear()
{
	m_overlay_geometry.clear();
	m_overlay_images.clear();
}

void FeWindow::draw( const FeOverlayDrawItem &item, const FeTransform &r )
{
	if ( !owns_sdl_window() )
		return;

	append_native_overlay_item( item, r );
}

void FeWindow::draw( const FeOverlayRect &rect, const FeTransform &r )
{
	draw( FeOverlayDrawItem( rect ), r );
}

void FeWindow::draw( const FeTextPrimitive &text, const FeTransform &r )
{
	draw( FeOverlayDrawItem( text ), r );
}

void FeWindow::draw( const FeListBox &listbox, const FeTransform &r )
{
	draw( FeOverlayDrawItem( listbox ), r );
}

void FeWindow::draw( const FeText &text, const FeTransform &r )
{
	draw( FeOverlayDrawItem( text ), r );
}

void FeWindow::draw( const FeRectangle &rect, const FeTransform &r )
{
	draw( FeOverlayDrawItem( rect ), r );
}

std::optional<FeEvent> FeWindow::pollEvent()
{
	if ( SDL_Window *window = m_gpu_context.get_window() )
	{
		SDL_Event event;
		while ( SDL_PollEvent( &event ) )
		{
			if ( const auto translated = translate_sdl_event( event, window, m_key_repeat_enabled ) )
				return translated;
		}

		return {};
	}

	return {};
}
