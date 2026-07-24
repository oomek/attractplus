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

#include "fe_rectangle.hpp"
#include "fe_settings.hpp"
#include "fe_util.hpp"
#include "fe_shader.hpp"
#include "fe_present.hpp"
#include <algorithm>
#include <cmath>

namespace
{
	const float DELTA = 0.01; // Used to create non-zero rect size
	const int MAX_CORNER_POINTS = 32; // Abitrary limit
}

FeRectangle::FeRectangle( FePresentableParent &p,
	float x, float y, float w, float h )
	: FeBasePresentable( p ),
	m_position( x, y ),
	m_size( w, h ),
	m_origin( 0.f, 0.f ),
	m_rotation_origin( 0.f, 0.f ),
	m_anchor( 0.f, 0.f ),
	m_render_position( x, y ),
	m_render_size( w, h ),
	m_render_origin( 0.f, 0.f ),
	m_corner_radius_actual( 0.f, 0.f ),
	m_anchor_type( TopLeft ),
	m_rotation_origin_type( TopLeft ),
	m_rotation ( 0.0 ),
	m_outline_thickness( 0.0f ),
	m_corner_point_count( 12 ),
	m_corner_point_actual( -1 ),
	m_corner_radius( 0.f, 0.f ),
	m_corner_ratio( 0.f, 0.f ),
	m_fill_color( Color::White ),
	m_outline_color( Color::White ),
	m_corner_ratio_x( false ),
	m_corner_ratio_y( false ),
	m_corner_auto( false ),
	m_blend_mode( FeBlend::Alpha )
{
	scale();
}

FeRectangle::FeRectangle( FePresentableParent &p )
	: FeRectangle( p, 0.0f, 0.0f, 0.0f, 0.0f )
{
}

FeRectangle::FeRectangle( const FeRectangle & ) = default;

Vec2f FeRectangle::getPosition() const
{
	return m_position;
}

void FeRectangle::setPosition( const Vec2f &p )
{
	if ( p != m_position )
	{
		m_position = p;
		scale();
		FePresent::script_flag_redraw();
	}
}

Vec2f FeRectangle::getSize() const
{
	return m_size;
}

void FeRectangle::setSize( const Vec2f &s )
{
	if ( s != m_size )
	{
		m_size = s;
		scale();
		FePresent::script_flag_redraw();
	}
}

float FeRectangle::getRotation() const
{
	return m_rotation;
}

void FeRectangle::setRotation( float r )
{
	if ( r != m_rotation )
	{
		m_rotation = r;
		scale();
		FePresent::script_flag_redraw();
	}
}

Color FeRectangle::getColor() const
{
	return m_fill_color;
}

Color FeRectangle::getOutlineColor()
{
	return m_outline_color;
}

void FeRectangle::setColor( Color c )
{
	if ( c == m_fill_color )
		return;

	m_fill_color = c;
	FePresent::script_flag_redraw();
}

void FeRectangle::setOutlineColor( Color c )
{
	if ( c == m_outline_color )
		return;

	m_outline_color = c;
	FePresent::script_flag_redraw();
}

float FeRectangle::get_outline()
{
	return m_outline_thickness;
}

void FeRectangle::set_outline( float o )
{
	if ( o != m_outline_thickness )
	{
		m_outline_thickness = o;
		FePresent::script_flag_redraw();
	}
}

int FeRectangle::get_olr() const
{
	return m_outline_color.r;
}

int FeRectangle::get_olg() const
{
	return m_outline_color.g;
}

int FeRectangle::get_olb() const
{
	return m_outline_color.b;
}

int FeRectangle::get_ola() const
{
	return m_outline_color.a;
}

void FeRectangle::set_olr( int r )
{
	Color c=getOutlineColor();
	c.r=r;
	setOutlineColor(c);
}

void FeRectangle::set_olg( int g )
{
	Color c=getOutlineColor();
	c.g=g;
	setOutlineColor(c);
}

void FeRectangle::set_olb( int b )
{
	Color c=getOutlineColor();
	c.b=b;
	setOutlineColor(c);
}

void FeRectangle::set_ola( int a )
{
	Color c=getOutlineColor();
	c.a=a;
	setOutlineColor(c);
}

void FeRectangle::set_olrgb( int r, int g, int b )
{
	Color c=getOutlineColor();
	c.r=r;
	c.g=g;
	c.b=b;
	setOutlineColor(c);
}

float FeRectangle::get_origin_x() const
{
	return m_origin.x;
}

float FeRectangle::get_origin_y() const
{
	return m_origin.y;
}

int FeRectangle::get_anchor_type() const
{
	return (FeRectangle::Alignment)m_anchor_type;
}

int FeRectangle::get_rotation_origin_type() const
{
	return (FeRectangle::Alignment)m_rotation_origin_type;
}

float FeRectangle::get_anchor_x() const
{
	return m_anchor.x;
}

float FeRectangle::get_anchor_y() const
{
	return m_anchor.y;
}

float FeRectangle::get_rotation_origin_x() const
{
	return m_rotation_origin.x;
}

float FeRectangle::get_rotation_origin_y() const
{
	return m_rotation_origin.y;
}

void FeRectangle::set_origin_x( float x )
{
	if ( x != m_origin.x )
	{
		m_origin.x = x;
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeRectangle::set_origin_y( float y )
{
	if ( y != m_origin.y )
	{
		m_origin.y = y;
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeRectangle::set_anchor( float x, float y )
{
	if ( x != m_anchor.x || y != m_anchor.y )
	{
		m_anchor = Vec2f( x, y );
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeRectangle::set_anchor_type( int t )
{
	m_anchor_type = (FeRectangle::Alignment)t;
	Vec2f a = alignTypeToVector( t );
	set_anchor( a.x, a.y );
}

void FeRectangle::set_rotation_origin( float x, float y )
{
	if ( x != m_rotation_origin.x || y != m_rotation_origin.y )
	{
		m_rotation_origin = Vec2f( x, y );
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeRectangle::set_rotation_origin_type( int t )
{
	m_rotation_origin_type = (FeRectangle::Alignment)t;
	Vec2f o = alignTypeToVector( t );
	set_rotation_origin( o.x, o.y );
}

void FeRectangle::set_anchor_x( float x )
{
	if ( x != m_anchor.x )
	{
		m_anchor.x = x;
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeRectangle::set_anchor_y( float y )
{
	if ( y != m_anchor.y )
	{
		m_anchor.y = y;
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeRectangle::set_rotation_origin_x( float x )
{
	if ( x != m_rotation_origin.x )
	{
		m_rotation_origin.x = x;
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeRectangle::set_rotation_origin_y( float y )
{
	if ( y != m_rotation_origin.y )
	{
		m_rotation_origin.y = y;
		scale();
		FePresent::script_flag_redraw();
	}
}

int FeRectangle::get_blend_mode() const
{
	return (FeBlend::Mode)m_blend_mode;
}

void FeRectangle::set_blend_mode( int b )
{
	m_blend_mode = (FeBlend::Mode)b;
}

float FeRectangle::get_corner_radius() const
{
	return m_corner_radius.x;
}

float FeRectangle::get_corner_radius_x() const
{
	return m_corner_radius.x;
}

float FeRectangle::get_corner_radius_y() const
{
	return m_corner_radius.y;
}

void FeRectangle::set_corner_radius_x( float rx )
{
	if ( m_corner_radius.x != rx || m_corner_ratio_x || m_corner_auto )
	{
		m_corner_auto = false;
		m_corner_ratio_x = false;
		m_corner_radius.x = rx;
		update_corner_radius();
	}
}

void FeRectangle::set_corner_radius_y( float ry )
{
	if ( m_corner_radius.y != ry || m_corner_ratio_y || m_corner_auto )
	{
		m_corner_auto = false;
		m_corner_ratio_y = false;
		m_corner_radius.y = ry;
		update_corner_radius();
	}
}

void FeRectangle::set_corner_radius( float rx, float ry )
{
	if ( m_corner_radius.x != rx || m_corner_radius.y != ry || m_corner_ratio_x || m_corner_ratio_y || m_corner_auto )
	{
		m_corner_auto = false;
		m_corner_ratio_x = false;
		m_corner_ratio_y = false;
		m_corner_radius.x = rx;
		m_corner_radius.y = ry;
		update_corner_radius();
	}
}

void FeRectangle::set_corner_radius( float r )
{
	if ( m_corner_radius.x != r || m_corner_radius.y != r || m_corner_ratio_x || m_corner_ratio_y || !m_corner_auto )
	{
		m_corner_auto = true;
		m_corner_ratio_x = false;
		m_corner_ratio_y = false;
		m_corner_radius.x = r;
		m_corner_radius.y = r;
		update_corner_radius();
	}
}

float FeRectangle::get_corner_ratio() const
{
	return m_corner_ratio.x;
}

float FeRectangle::get_corner_ratio_x() const
{
	return m_corner_ratio.x;
}

float FeRectangle::get_corner_ratio_y() const
{
	return m_corner_ratio.y;
}

void FeRectangle::set_corner_ratio_x( float rx )
{
	if ( m_corner_ratio.x != rx || !m_corner_ratio_x || m_corner_auto )
	{
		m_corner_auto = false;
		m_corner_ratio_x = true;
		m_corner_ratio.x = rx;
		update_corner_ratio();
	}
}

void FeRectangle::set_corner_ratio_y( float ry )
{
	if ( m_corner_ratio.y != ry || !m_corner_ratio_y || m_corner_auto )
	{
		m_corner_auto = false;
		m_corner_ratio_y = true;
		m_corner_ratio.y = ry;
		update_corner_ratio();
	}
}

void FeRectangle::set_corner_ratio( float rx, float ry )
{
	if ( m_corner_ratio.x != rx || m_corner_ratio.y != ry || !m_corner_ratio_x || !m_corner_ratio_y || m_corner_auto )
	{
		m_corner_auto = false;
		m_corner_ratio_x = true;
		m_corner_ratio_y = true;
		m_corner_ratio.x = rx;
		m_corner_ratio.y = ry;
		update_corner_ratio();
	}
}

void FeRectangle::set_corner_ratio( float r )
{
	if ( m_corner_ratio.x != r || m_corner_ratio.y != r || !m_corner_ratio_x || !m_corner_ratio_y || !m_corner_auto )
	{
		m_corner_auto = true;
		m_corner_ratio_x = true;
		m_corner_ratio_y = true;
		m_corner_ratio.x = r;
		m_corner_ratio.y = r;
		update_corner_ratio();
	}
}

int FeRectangle::get_corner_point_count() const
{
	return m_corner_point_count;
}

int FeRectangle::get_type() const
{
	return FePresentableTypeRectangle;
}

void FeRectangle::set_corner_point_count( int n )
{
	m_corner_point_count = n;
	update_corner_points();
}

void FeRectangle::update_corner_points()
{
	// Reduce to a single corner if x or y radius is zero
	int n = ( m_corner_radius.x != 0 && m_corner_radius.y != 0 && m_corner_point_count > 0 ) ? m_corner_point_count : 1;
	if ( n > MAX_CORNER_POINTS ) n = MAX_CORNER_POINTS;
	m_corner_point_actual = n;
}

void FeRectangle::update_corner_radius()
{
	// Ensure corners are < 0.5 rect size to prevent point overlap, which causes outline issues
	float mx = std::max( DELTA, std::fabs( m_size.x ) - DELTA );
	float my = std::max( DELTA, std::fabs( m_size.y ) - DELTA );
	float mx2 = mx / 2;
	float my2 = my / 2;
	float cx = std::fabs( m_corner_radius.x );
	float cy = std::fabs( m_corner_radius.y );
	if ( m_corner_auto && cx > mx2 ) cy = mx2 / cx * cy;
	if ( m_corner_auto && cy > my2 ) cx = my2 / cy * cx;
	float rx = std::min( mx2, cx );
	float ry = std::min( my2, cy );

	// Flip corners to fix negative size rectangles
	if ( m_size.x < 0 ) rx = -rx;
	if ( m_size.y < 0 ) ry = -ry;

	m_corner_radius_actual = Vec2f( rx, ry );
	update_corner_points();
}

void FeRectangle::update_corner_ratio()
{
	if ( m_corner_ratio_x || m_corner_ratio_y )
	{
		// Ensure ratio corners have a non-zero size to use, otherwise a zero result creates square outlines
		float mx = std::max( DELTA, std::fabs( m_size.x ) );
		float my = std::max( DELTA, std::fabs( m_size.y ) );

		if ( m_corner_auto )
		{
			// If AUTO use the smallest side for the radius
			float s = m_corner_ratio.x * std::min( mx, my );
			m_corner_radius = Vec2f( s, s );
		}
		else
		{
			// Otherwise calc ratios for each axis
			if ( m_corner_ratio_x ) m_corner_radius.x = m_corner_ratio.x * mx;
			if ( m_corner_ratio_y ) m_corner_radius.y = m_corner_ratio.y * my;
		}
	}

	update_corner_radius();
}

Vec2f FeRectangle::alignTypeToVector( int type )
{
	switch( type )
	{
		case Left:
			return Vec2f( 0.0f, 0.5f );

		case Centre:
			return Vec2f( 0.5f, 0.5f );

		case Right:
			return Vec2f( 1.0f, 0.5f );

		case Top:
			return Vec2f( 0.5f, 0.0f );

		case Bottom:
			return Vec2f( 0.5f, 1.0f );

		case TopLeft:
			return Vec2f( 0.0f, 0.0f );

		case TopRight:
			return Vec2f( 1.0f, 0.0f );

		case BottomLeft:
			return Vec2f( 0.0f, 1.0f );

		case BottomRight:
			return Vec2f( 1.0f, 1.0f );

		default:
			return Vec2f( 0.0f, 0.0f );
	}
}

std::size_t FeRectangle::get_shape_point_count() const
{
	return static_cast<std::size_t>( m_corner_point_actual ) * 4u;
}

Vec2f FeRectangle::get_shape_point( std::size_t index ) const
{
	return get_shape_point(
		index,
		m_render_size,
		m_corner_radius_actual,
		static_cast<unsigned int>( m_corner_point_actual ) );
}

Vec2f FeRectangle::get_shape_point( std::size_t index, const Vec2f &size, const Vec2f &radius, unsigned int corner_point_count )
{
	if ( corner_point_count <= 1u )
	{
		switch ( index )
		{
			default:
			case 0: return Vec2f( 0.0f, 0.0f );
			case 1: return Vec2f( size.x, 0.0f );
			case 2: return Vec2f( size.x, size.y );
			case 3: return Vec2f( 0.0f, size.y );
		}
	}

	const std::size_t point_count = static_cast<std::size_t>( corner_point_count ) * 4u;
	if ( index >= point_count )
		return Vec2f( 0.0f, 0.0f );

	Vec2f center;
	const unsigned int center_index = static_cast<unsigned int>( index / corner_point_count );
	static const float half_pi = 3.141592654f / 2.0f;
	const float angle = ( static_cast<float>( index ) - static_cast<float>( center_index ) )
		* half_pi / static_cast<float>( corner_point_count - 1u );

	switch ( center_index )
	{
		case 0: center = Vec2f( size.x - radius.x, radius.y ); break;
		case 1: center = Vec2f( radius.x, radius.y ); break;
		case 2: center = Vec2f( radius.x, size.y - radius.y ); break;
		case 3: center = Vec2f( size.x - radius.x, size.y - radius.y ); break;
		default: center = Vec2f( 0.0f, 0.0f ); break;
	}

	return Vec2f(
		radius.x * std::cos( angle ) + center.x,
		-radius.y * std::sin( angle ) + center.y );
}

bool FeRectangle::build_render_geometry( FeRenderGeometry &geometry ) const
{
	geometry.clear();
	geometry.zbuffer = get_zbuffer();

	const std::size_t point_count = get_shape_point_count();
	if ( point_count < 3 )
		return false;

	const FeTransform transform = FeTransform()
		.translate( m_render_position )
		.rotate( m_rotation )
		.translate( -m_render_origin );
	const Color fill_color = m_fill_color;
	const Vec2f rect_size = m_render_size;

	auto normalized_uv = [&]( const Vec2f &point )
	{
		const float u = ( rect_size.x != 0.0f ) ? ( point.x / rect_size.x ) : 0.0f;
		const float v = ( rect_size.y != 0.0f ) ? ( point.y / rect_size.y ) : 0.0f;
		return Vec2f( u, v );
	};
	auto append_triangle = [&]( const Vec2f &p0,
		const Vec2f &p1,
		const Vec2f &p2,
		const Vec2f &uv0,
		const Vec2f &uv1,
		const Vec2f &uv2,
		const Color &color )
	{
		FeRenderVertex v0 = {};
		v0.x = p0.x;
		v0.y = p0.y;
		v0.z = get_z();
		v0.u = uv0.x;
		v0.v = uv0.y;
		v0.r = color.r;
		v0.g = color.g;
		v0.b = color.b;
		v0.a = color.a;

		FeRenderVertex v1 = v0;
		v1.x = p1.x;
		v1.y = p1.y;
		v1.u = uv1.x;
		v1.v = uv1.y;

		FeRenderVertex v2 = v0;
		v2.x = p2.x;
		v2.y = p2.y;
		v2.u = uv2.x;
		v2.v = uv2.y;

		geometry.vertices.push_back( v0 );
		geometry.vertices.push_back( v1 );
		geometry.vertices.push_back( v2 );
	};

	const Vec2f first_local = get_shape_point( 0 );
	const Vec2f first = transform.transformPoint( first_local );

	for ( std::size_t i = 1; i + 1 < point_count; ++i )
	{
		const Vec2f second_local = get_shape_point( i );
		const Vec2f third_local = get_shape_point( i + 1 );
		const Vec2f second = transform.transformPoint( second_local );
		const Vec2f third = transform.transformPoint( third_local );
		const Vec2f uv0 = normalized_uv( first_local );
		const Vec2f uv1 = normalized_uv( second_local );
		const Vec2f uv2 = normalized_uv( third_local );

		append_triangle( first, second, third, uv0, uv1, uv2, fill_color );
	}

	const float outline = m_outline_thickness;
	const Color outline_color = m_outline_color;
	if ( outline > 0.0f && outline_color.a > 0 )
	{
		const unsigned int corner_points = static_cast<unsigned int>( m_corner_point_actual );
		const Vec2f outer_size( rect_size.x + outline * 2.0f, rect_size.y + outline * 2.0f );
		const Vec2f outer_radius(
			std::max( 0.0f, m_corner_radius_actual.x + outline ),
			std::max( 0.0f, m_corner_radius_actual.y + outline ) );
		const Vec2f outline_offset( outline, outline );

		for ( std::size_t i = 0; i < point_count; ++i )
		{
			const std::size_t next = ( i + 1 ) % point_count;
			const Vec2f inner_local_0 = get_shape_point( i );
			const Vec2f inner_local_1 = get_shape_point( next );
			const Vec2f outer_local_0 = get_shape_point( i, outer_size, outer_radius, corner_points ) - outline_offset;
			const Vec2f outer_local_1 = get_shape_point( next, outer_size, outer_radius, corner_points ) - outline_offset;

			const Vec2f inner_0 = transform.transformPoint( inner_local_0 );
			const Vec2f inner_1 = transform.transformPoint( inner_local_1 );
			const Vec2f outer_0 = transform.transformPoint( outer_local_0 );
			const Vec2f outer_1 = transform.transformPoint( outer_local_1 );

			append_triangle(
				outer_0,
				outer_1,
				inner_0,
				normalized_uv( outer_local_0 ),
				normalized_uv( outer_local_1 ),
				normalized_uv( inner_local_0 ),
				outline_color );
			append_triangle(
				inner_0,
				outer_1,
				inner_1,
				normalized_uv( inner_local_0 ),
				normalized_uv( outer_local_1 ),
				normalized_uv( inner_local_1 ),
				outline_color );
		}
	}

	geometry.texture_id = nullptr;
	geometry.texture_width = 1.0f;
	geometry.texture_height = 1.0f;
	geometry.blend_mode = m_blend_mode;
	geometry.shader = get_shader();
	geometry.custom_shader = ( geometry.shader != nullptr );
	geometry.textured = false;
	geometry.texture_dynamic = false;
	return !geometry.vertices.empty();
}

void FeRectangle::scale()
{
	Vec2f pos = m_position;
	Vec2f size = m_size;

	// update corners before checking if size needs adjusting
	update_corner_ratio();

	// If there's a corner ensure theres a non-zero area to draw it in
	// - Fixes outline spike issue on zero-sized rectangles
	if ( m_corner_radius.x != 0 || m_corner_radius.y != 0 )
	{
		// Use 2x Delta so corner can floor at 1x Delta (see above)
		size.x = std::max( 2.0f * DELTA, std::fabs( m_size.x ) );
		size.y = std::max( 2.0f * DELTA, std::fabs( m_size.y ) );
		if (m_size.x < 0) size.x = -size.x;
		if (m_size.y < 0) size.y = -size.y;
	}

	pos += Vec2f(
		( m_rotation_origin.x - m_anchor.x ) * size.x,
		( m_rotation_origin.y -  m_anchor.y ) * size.y );

	m_render_position = pos;
	m_render_size = size;
	m_render_origin = Vec2f(
		m_origin.x + m_rotation_origin.x * size.x,
		m_origin.y + m_rotation_origin.y * size.y );

}
