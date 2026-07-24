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

#include <vector>

#include "fe_renderer.hpp"
#include "fe_types.hpp"

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
	RectEdges( const Vec2<U> &pos, const Vec2<U> &size )
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

class FeSprite
{
public:
	FeSprite();

	void setTextureRect( const FloatRect &rectangle );
	const FloatRect &getTextureRect() const;

	void setColor( Color color );
	Color getColor() const;

	FloatRect getLocalBounds() const;

	void setCrop( FloatEdges crop );
	FloatEdges getCrop() const;

	const IntEdges &getBorder() const;
	void setBorder( const IntEdges &border );

	const IntEdges &getPadding() const;
	void setPadding( const IntEdges &padding );

	void setScale( const Vec2f &scale );
	const Vec2f &getScale() const;

	void setPosition( const Vec2f &position );
	const Vec2f &getPosition() const;

	void setOrigin( const Vec3f &origin );
	const Vec3f &getOrigin() const;

	void setRotation( float rotation );
	float getRotation() const;

	void setRotationX( float rotation );
	float getRotationX() const;

	void setRotationY( float rotation );
	float getRotationY() const;

	void setRotationOrder( int order );
	int getRotationOrder() const;

	void setBorderScale( float scale );
	float getBorderScale() const;

	void append_render_vertices( std::vector<FeRenderVertex> &out, float zorder ) const;

private:
	void updateGeometry();

	std::vector<FeRenderVertex> m_vertices;
	FloatRect m_textureRect;
	FloatEdges m_crop;
	IntEdges m_border;
	IntEdges m_padding;
	Vec2f m_scale;
	Vec2f m_position;
	Vec3f m_origin;
	Color m_color;
	float m_rotation_x;
	float m_rotation_y;
	float m_rotation_z;
	int m_rotation_order;
	float m_border_scale;
};

#endif
