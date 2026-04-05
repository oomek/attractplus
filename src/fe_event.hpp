/*
 *
 *  Attract-Mode Plus frontend
 *  Copyright (C) 2026 Radek Dutkiewicz
 *
 *  This file is part of Attract-Mode Plus
 *
 *  Attract-Mode Plus is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Attract-Mode Plus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Attract-Mode Plus.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef FE_EVENT_HPP
#define FE_EVENT_HPP

#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

class FeEvent
{
public:
	struct Vector2i
	{
		int x = 0;
		int y = 0;
	};

	struct Vector2u
	{
		unsigned int x = 0;
		unsigned int y = 0;
	};

	enum class MouseWheel
	{
		Vertical = 0,
		Horizontal = 1
	};

	struct Closed
	{
	};

	struct FocusGained
	{
	};

	struct FocusLost
	{
	};

	struct Resized
	{
		Vector2u size = {};
	};

	struct TextEntered
	{
		char32_t unicode = 0;
	};

	struct KeyPressed
	{
		int code = -1;
		bool alt = false;
		bool control = false;
		bool shift = false;
		bool system = false;
	};

	struct KeyReleased
	{
		int code = -1;
		bool alt = false;
		bool control = false;
		bool shift = false;
		bool system = false;
	};

	struct MouseMoved
	{
		Vector2i position = {};
	};

	struct MouseWheelScrolled
	{
		MouseWheel wheel = MouseWheel::Vertical;
		float delta = 0.0f;
		Vector2i position = {};
	};

	struct MouseButtonPressed
	{
		int button = 0;
		Vector2i position = {};
	};

	struct MouseButtonReleased
	{
		int button = 0;
		Vector2i position = {};
	};

	struct JoystickMoved
	{
		unsigned int joystickId = 0;
		int axis = 0;
		float position = 0.0f;
	};

	struct JoystickButtonPressed
	{
		unsigned int joystickId = 0;
		unsigned int button = 0;
	};

	struct JoystickButtonReleased
	{
		unsigned int joystickId = 0;
		unsigned int button = 0;
	};

	struct JoystickConnected
	{
		unsigned int joystickId = 0;
	};

	struct JoystickDisconnected
	{
		unsigned int joystickId = 0;
	};

	struct TouchMoved
	{
		unsigned int finger = 0;
		Vector2i position = {};
	};

	struct TouchBegan
	{
		unsigned int finger = 0;
		Vector2i position = {};
	};

	struct TouchEnded
	{
		unsigned int finger = 0;
		Vector2i position = {};
	};

	FeEvent() = default;

	template <class T, class = std::enable_if_t<!std::is_same_v<std::decay_t<T>, FeEvent>>>
	FeEvent( T &&event )
		: m_data( std::forward<T>( event ) )
	{
	}

	template <class T>
	bool is() const
	{
		return std::holds_alternative<T>( m_data );
	}

	template <class T>
	const T *getIf() const
	{
		return std::get_if<T>( &m_data );
	}

	template <class T>
	T *getIf()
	{
		return std::get_if<T>( &m_data );
	}

private:
	using Data = std::variant<
		std::monostate,
		Closed,
		FocusGained,
		FocusLost,
		Resized,
		TextEntered,
		KeyPressed,
		KeyReleased,
		MouseMoved,
		MouseWheelScrolled,
		MouseButtonPressed,
		MouseButtonReleased,
		JoystickMoved,
		JoystickButtonPressed,
		JoystickButtonReleased,
		JoystickConnected,
		JoystickDisconnected,
		TouchMoved,
		TouchBegan,
		TouchEnded>;

	Data m_data;
};

#endif
