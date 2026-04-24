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

#ifndef FE_TYPES_HPP
#define FE_TYPES_HPP

#include <cmath>
#include <cstdint>
#include <type_traits>

template <typename T>
struct Vec2
{
	T x;
	T y;

	constexpr Vec2()
		: x( T() ),
		  y( T() )
	{
	}

	constexpr Vec2( T px, T py )
		: x( px ),
		  y( py )
	{
	}

	template <typename U>
	explicit constexpr Vec2( const Vec2<U> &other )
		: x( static_cast<T>( other.x ) ),
		  y( static_cast<T>( other.y ) )
	{
	}

	constexpr Vec2 &operator+=( const Vec2 &other )
	{
		x += other.x;
		y += other.y;
		return *this;
	}

	constexpr Vec2 &operator-=( const Vec2 &other )
	{
		x -= other.x;
		y -= other.y;
		return *this;
	}

	template <typename U, typename = std::enable_if_t<std::is_arithmetic_v<U>>>
	constexpr Vec2 &operator*=( U scalar )
	{
		x = static_cast<T>( x * scalar );
		y = static_cast<T>( y * scalar );
		return *this;
	}

	template <typename U, typename = std::enable_if_t<std::is_arithmetic_v<U>>>
	constexpr Vec2 &operator/=( U scalar )
	{
		x = static_cast<T>( x / scalar );
		y = static_cast<T>( y / scalar );
		return *this;
	}

	[[nodiscard]] constexpr auto lengthSquared() const
	{
		return ( x * x ) + ( y * y );
	}

	[[nodiscard]] double length() const
	{
		return std::sqrt( static_cast<double>( lengthSquared() ) );
	}
};

template <typename T>
[[nodiscard]] constexpr bool operator==( const Vec2<T> &lhs, const Vec2<T> &rhs )
{
	return lhs.x == rhs.x && lhs.y == rhs.y;
}

template <typename T>
[[nodiscard]] constexpr bool operator!=( const Vec2<T> &lhs, const Vec2<T> &rhs )
{
	return !( lhs == rhs );
}

template <typename T>
[[nodiscard]] constexpr Vec2<T> operator+( Vec2<T> lhs, const Vec2<T> &rhs )
{
	lhs += rhs;
	return lhs;
}

template <typename T>
[[nodiscard]] constexpr Vec2<T> operator-( Vec2<T> lhs, const Vec2<T> &rhs )
{
	lhs -= rhs;
	return lhs;
}

template <typename T>
[[nodiscard]] constexpr Vec2<T> operator-( const Vec2<T> &value )
{
	return Vec2<T>( -value.x, -value.y );
}

template <typename T, typename U, typename = std::enable_if_t<std::is_arithmetic_v<U>>>
[[nodiscard]] constexpr Vec2<T> operator*( Vec2<T> lhs, U scalar )
{
	lhs *= scalar;
	return lhs;
}

template <typename T, typename U, typename = std::enable_if_t<std::is_arithmetic_v<U>>>
[[nodiscard]] constexpr Vec2<T> operator*( U scalar, Vec2<T> rhs )
{
	rhs *= scalar;
	return rhs;
}

template <typename T, typename U, typename = std::enable_if_t<std::is_arithmetic_v<U>>>
[[nodiscard]] constexpr Vec2<T> operator/( Vec2<T> lhs, U scalar )
{
	lhs /= scalar;
	return lhs;
}

template <typename T>
struct Vec3
{
	T x;
	T y;
	T z;

	constexpr Vec3()
		: x( T() ),
		  y( T() ),
		  z( T() )
	{
	}

	constexpr Vec3( T px, T py, T pz )
		: x( px ),
		  y( py ),
		  z( pz )
	{
	}

	template <typename U>
	explicit constexpr Vec3( const Vec3<U> &other )
		: x( static_cast<T>( other.x ) ),
		  y( static_cast<T>( other.y ) ),
		  z( static_cast<T>( other.z ) )
	{
	}
};

template <typename T>
[[nodiscard]] constexpr bool operator==( const Vec3<T> &lhs, const Vec3<T> &rhs )
{
	return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

template <typename T>
[[nodiscard]] constexpr bool operator!=( const Vec3<T> &lhs, const Vec3<T> &rhs )
{
	return !( lhs == rhs );
}

template <typename T>
struct RectT
{
	Vec2<T> position;
	Vec2<T> size;

	constexpr RectT()
		: position(),
		  size()
	{
	}

	constexpr RectT( T left, T top, T width, T height )
		: position( left, top ),
		  size( width, height )
	{
	}

	constexpr RectT( const Vec2<T> &pos, const Vec2<T> &dimensions )
		: position( pos ),
		  size( dimensions )
	{
	}

	template <typename U>
	explicit constexpr RectT( const RectT<U> &other )
		: position( other.position ),
		  size( other.size )
	{
	}

	[[nodiscard]] constexpr bool contains( T px, T py ) const
	{
		return px >= position.x
			&& py >= position.y
			&& px < position.x + size.x
			&& py < position.y + size.y;
	}

	template <typename U>
	[[nodiscard]] constexpr bool contains( const Vec2<U> &point ) const
	{
		return contains( static_cast<T>( point.x ), static_cast<T>( point.y ) );
	}

	[[nodiscard]] constexpr bool findIntersection( const RectT<T> &other ) const
	{
		const T left = ( position.x > other.position.x ) ? position.x : other.position.x;
		const T top = ( position.y > other.position.y ) ? position.y : other.position.y;
		const T right = ( position.x + size.x < other.position.x + other.size.x )
			? position.x + size.x
			: other.position.x + other.size.x;
		const T bottom = ( position.y + size.y < other.position.y + other.size.y )
			? position.y + size.y
			: other.position.y + other.size.y;

		return ( left < right ) && ( top < bottom );
	}
};

template <typename T>
[[nodiscard]] constexpr bool operator==( const RectT<T> &lhs, const RectT<T> &rhs )
{
	return lhs.position == rhs.position && lhs.size == rhs.size;
}

template <typename T>
[[nodiscard]] constexpr bool operator!=( const RectT<T> &lhs, const RectT<T> &rhs )
{
	return !( lhs == rhs );
}

using Vec2i = Vec2<int>;
using Vec2u = Vec2<unsigned int>;
using Vec2f = Vec2<float>;
using Vec3i = Vec3<int>;
using Vec3f = Vec3<float>;
using IntRect = RectT<int>;
using FloatRect = RectT<float>;

struct Color
{
	std::uint8_t r;
	std::uint8_t g;
	std::uint8_t b;
	std::uint8_t a;

	constexpr Color()
		: r( 0 ),
		  g( 0 ),
		  b( 0 ),
		  a( 255 )
	{
	}

	constexpr Color( std::uint8_t pr, std::uint8_t pg, std::uint8_t pb, std::uint8_t pa = 255 )
		: r( pr ),
		  g( pg ),
		  b( pb ),
		  a( pa )
	{
	}

	static const Color Transparent;
	static const Color Black;
	static const Color White;
	static const Color Red;
	static const Color Green;
	static const Color Blue;
	static const Color Yellow;
	static const Color Magenta;
	static const Color Cyan;
};

[[nodiscard]] constexpr bool operator==( const Color &lhs, const Color &rhs )
{
	return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

[[nodiscard]] constexpr bool operator!=( const Color &lhs, const Color &rhs )
{
	return !( lhs == rhs );
}

[[nodiscard]] constexpr Color operator*( const Color &lhs, const Color &rhs )
{
	return Color(
		static_cast<std::uint8_t>( static_cast<unsigned int>( lhs.r ) * rhs.r / 255u ),
		static_cast<std::uint8_t>( static_cast<unsigned int>( lhs.g ) * rhs.g / 255u ),
		static_cast<std::uint8_t>( static_cast<unsigned int>( lhs.b ) * rhs.b / 255u ),
		static_cast<std::uint8_t>( static_cast<unsigned int>( lhs.a ) * rhs.a / 255u ) );
}

inline constexpr Color Color::Transparent{ 0, 0, 0, 0 };
inline constexpr Color Color::Black{ 0, 0, 0, 255 };
inline constexpr Color Color::White{ 255, 255, 255, 255 };
inline constexpr Color Color::Red{ 255, 0, 0, 255 };
inline constexpr Color Color::Green{ 0, 255, 0, 255 };
inline constexpr Color Color::Blue{ 0, 0, 255, 255 };
inline constexpr Color Color::Yellow{ 255, 255, 0, 255 };
inline constexpr Color Color::Magenta{ 255, 0, 255, 255 };
inline constexpr Color Color::Cyan{ 0, 255, 255, 255 };

#endif
