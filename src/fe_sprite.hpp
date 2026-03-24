/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-2026
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

#ifndef FE_SPRITE_HPP
#define FE_SPRITE_HPP

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

#include <vector>

#include "fe_renderer.hpp"

template <typename T>
struct RectEdges
{
	T left;
	T top;
	T right;
	T bottom;

	RectEdges()
		: left(),
		  top(),
		  right(),
		  bottom()
	{
	}

	template <typename U>
	RectEdges( const RectEdges<U> &other )
		: left( static_cast<T>( other.left ) ),
		  top( static_cast<T>( other.top ) ),
		  right( static_cast<T>( other.right ) ),
		  bottom( static_cast<T>( other.bottom ) )
	{
	}

	template <typename U>
	RectEdges( const sf::Vector2<U> &pos, const sf::Vector2<U> &size )
		: left( static_cast<T>( pos.x ) ),
		  top( static_cast<T>( pos.y ) ),
		  right( static_cast<T>( size.x ) ),
		  bottom( static_cast<T>( size.y ) )
	{
	}

	template <typename U>
	RectEdges( U l, U t, U r, U b )
		: left( static_cast<T>( l ) ),
		  top( static_cast<T>( t ) ),
		  right( static_cast<T>( r ) ),
		  bottom( static_cast<T>( b ) )
	{
	}

	template <typename U>
	bool operator==( const RectEdges<U> &other ) const
	{
		return left == static_cast<T>( other.left )
			&& top == static_cast<T>( other.top )
			&& right == static_cast<T>( other.right )
			&& bottom == static_cast<T>( other.bottom );
	}

	template <typename U>
	bool operator!=( const RectEdges<U> &other ) const
	{
		return !( *this == other );
	}
};

using IntEdges = RectEdges<int>;
using FloatEdges = RectEdges<float>;

struct FeSpriteGeometry
{
	FloatEdges texture_rect;
	FloatEdges crop;
	IntEdges border;
	IntEdges padding;
	sf::Vector2f scale;
	sf::Vector2f position;
	sf::Vector2f origin;
	float origin_z;
	sf::Vector2f skew;
	sf::Vector2f pinch;
	sf::Color color;
	float rotation_x;
	float rotation_y;
	float rotation_z;
	float border_scale;

	FeSpriteGeometry();
};

void fe_sprite_append_render_vertices(
	std::vector<FeRenderVertex> &out,
	const FeSpriteGeometry &geometry,
	float z );

#endif
