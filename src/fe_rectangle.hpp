/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-21 Andrew Mickelson
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

#ifndef FE_RECTANGLE_HPP
#define FE_RECTANGLE_HPP

#include <SFML/Graphics.hpp>
#include "fe_presentable.hpp"
#include "fe_blend.hpp"
#include "rounded_rectangle_shape.hpp"

class FeSettings;

class FeRectangle : public sf::Drawable, public FeBasePresentable
{
public:
	public:
	enum Alignment
	{
		Left,
		Centre,
		Right,
		Top,
		Bottom,
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight
	};

	FeRectangle( FePresentableParent &p,
		float x, float y, float w, float h );

	sf::Vector2f getPosition() const;
	void setPosition( const sf::Vector2f & );
	void setPosition( float x, float y ) {return setPosition(sf::Vector2f(x,y));};
	sf::Vector2f getSize() const;
	void setSize( const sf::Vector2f & );
	void setSize( float w, float h ) {return setSize(sf::Vector2f(w,h));};
	float getRotation() const;
	void setRotation( float );
	sf::Color getColor() const;
	sf::Color getOutlineColor();
	void setColor( sf::Color );
	void setOutlineColor( sf::Color );

	float get_outline();
	void set_outline( float o );

	float get_origin_x() const;
	float get_origin_y() const;
	int get_anchor_type() const;
	int get_rotation_origin_type() const;
	float get_anchor_x() const;
	float get_anchor_y() const;
	float get_rotation_origin_x() const;
	float get_rotation_origin_y() const;
	int get_olr() const;
	int get_olg() const;
	int get_olb() const;
	int get_ola() const;

	float get_corner_radius() const;
	float get_corner_radius_x() const;
	float get_corner_radius_y() const;
	float get_corner_ratio() const;
	float get_corner_ratio_x() const;
	float get_corner_ratio_y() const;
	int get_corner_point_count() const;

	void set_origin_x( float x );
	void set_origin_y( float y );
	void set_anchor( float x, float y );
	void set_anchor_type( int t );
	void set_rotation_origin( float x, float y );
	void set_rotation_origin_type( int t );
	void set_anchor_x( float x );
	void set_anchor_y( float y );
	void set_rotation_origin_x( float x );
	void set_rotation_origin_y( float y );
	void set_olr( int r );
	void set_olg( int g );
	void set_olb( int b );
	void set_ola( int a );
	void set_olrgb( int r, int g, int b );

	void set_corner_radius( float r );
	void set_corner_radius( float rx, float ry );
	void set_corner_radius_x( float rx );
	void set_corner_radius_y( float ry );
	void set_corner_ratio( float r );
	void set_corner_ratio( float rx, float ry );
	void set_corner_ratio_x( float rx );
	void set_corner_ratio_y( float ry );
	void set_corner_point_count( int n );

	int get_blend_mode() const;
	void set_blend_mode( int b );

	const sf::Drawable &drawable() const { return (const sf::Drawable &)*this; };

protected:
	void draw( sf::RenderTarget &target, sf::RenderStates states ) const;

private:
	sf::RoundedRectangleShape m_rect;
	FeRectangle( const FeRectangle & );
	FeRectangle &operator=( const FeRectangle & );

	sf::Vector2f m_position;
	sf::Vector2f m_size;
	sf::Vector2f m_origin;
	sf::Vector2f m_rotation_origin;
	sf::Vector2f m_anchor;
	FeRectangle::Alignment m_anchor_type;
	FeRectangle::Alignment m_rotation_origin_type;
	FeBlend::Mode m_blend_mode;
	float m_rotation;

	int m_corner_point_count;
	int m_corner_point_actual;
	sf::Vector2f m_corner_radius;
	sf::Vector2f m_corner_ratio;
	bool m_corner_ratio_x;
	bool m_corner_ratio_y;
	bool m_corner_auto;

	void scale();
	void update_corner_radius();
	void update_corner_ratio();
	void update_corner_points();
	sf::Vector2f alignTypeToVector( int a );
};

#endif
