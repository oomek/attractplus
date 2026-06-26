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

#include "fe_types.hpp"
#include "fe_align.hpp"
#include "fe_presentable.hpp"
#include "fe_blend.hpp"
#include "fe_renderer.hpp"
#include "fe_transform.hpp"

class FeSettings;

class FeRectangle : public FeBasePresentable
{
public:
	FeRectangle( FePresentableParent &p,
		float x, float y, float w, float h );
	FeRectangle( FePresentableParent &p );
	FeRectangle( const FeRectangle & );

	Vec2f getPosition() const;
	void setPosition( const Vec2f & );
	void setPosition( float x, float y ) { return setPosition( Vec2f( x, y ) ); };
	Vec2f getSize() const;
	void setSize( const Vec2f & );
	void setSize( float w, float h ) { return setSize( Vec2f( w, h ) ); };
	float getRotation() const;
	void setRotation( float );
	Color getColor() const;
	Color getOutlineColor();
	void setColor( Color );
	void setOutlineColor( Color );

	float get_outline();
	void set_outline( float o );

	float get_origin_x() const;
	float get_origin_y() const;
	int get_transform_origin_type() const;
	int get_anchor_type() const;
	int get_rotation_origin_type() const;
	float get_transform_origin_x() const;
	float get_transform_origin_y() const;
	float get_anchor_x() const;
	float get_anchor_y() const;
	float get_rotation_origin_x() const;
	float get_rotation_origin_y() const;
	int get_outline_red() const;
	int get_outline_green() const;
	int get_outline_blue() const;
	int get_outline_alpha() const;

	float get_corner_radius() const;
	float get_corner_radius_x() const;
	float get_corner_radius_y() const;
	float get_corner_ratio() const;
	float get_corner_ratio_x() const;
	float get_corner_ratio_y() const;
	int get_corner_point_count() const;
	int get_type() const;

	void set_origin_x( float x );
	void set_origin_y( float y );
	void set_transform_origin( float x, float y );
	void set_transform_origin_type( int t );
	void set_anchor( float x, float y );
	void set_anchor_type( int t );
	void set_rotation_origin( float x, float y );
	void set_rotation_origin_type( int t );
	void set_transform_origin_x( float x );
	void set_transform_origin_y( float y );
	void set_anchor_x( float x );
	void set_anchor_y( float y );
	void set_rotation_origin_x( float x );
	void set_rotation_origin_y( float y );
	void set_outline_red( int r );
	void set_outline_green( int g );
	void set_outline_blue( int b );
	void set_outline_alpha( int a );
	void set_outline_rgb( int r, int g, int b );
	void set_outline_rgb( int r, int g, int b, int a );

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
	bool build_render_geometry( FeRenderGeometry &geometry ) const;
	void refresh_script_geometry() override;

private:
	FeRectangle &operator=( const FeRectangle & );

	Vec2f m_position;
	Vec2f m_size;
	Vec2f m_origin;
	Vec2f m_transform_origin;
	Vec2f m_rotation_origin;
	Vec2f m_anchor;
	Vec2f m_render_position;
	Vec2f m_render_size;
	Vec2f m_render_origin;
	Vec2f m_corner_radius_actual;
	FeAlign m_transform_origin_type;
	FeAlign m_anchor_type;
	FeAlign m_rotation_origin_type;
	float m_rotation;
	float m_outline_thickness;

	int m_corner_point_count;
	int m_corner_point_actual;
	Vec2f m_corner_radius;
	Vec2f m_corner_ratio;
	Color m_fill_color;
	Color m_outline_color;
	bool m_corner_ratio_x;
	bool m_corner_ratio_y;
	bool m_corner_auto;
	float m_outline;
	FeBlend::Mode m_blend_mode;

	void scale();
	void update_corner_radius();
	void update_corner_ratio();
	void update_corner_points();
	std::size_t get_shape_point_count() const;
	Vec2f get_shape_point( std::size_t index ) const;
	static Vec2f get_shape_point( std::size_t index, const Vec2f &size, const Vec2f &radius, unsigned int corner_point_count );
};

#endif
