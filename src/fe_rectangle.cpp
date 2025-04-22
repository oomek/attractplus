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
#include <cmath>

namespace
{
	const float DELTA = 0.01; // Used to create non-zero rect size
	const int MAX_CORNER_POINTS = 32; // Abitrary limit
}

FeRectangle::FeRectangle( FePresentableParent &p,
	float x, float y, float w, float h )
	: FeBasePresentable( p ),
	m_rect( sf::Vector2f( w, h ), sf::Vector2f( 0, 0 ), 1 ),
	m_position( x, y ),
	m_size( w, h ),
	m_origin( 0.f, 0.f ),
	m_rotation_origin( 0.f, 0.f ),
	m_anchor( 0.f, 0.f ),
	m_anchor_type( TopLeft ),
	m_rotation_origin_type( TopLeft ),
	m_rotation ( 0.0 ),
	m_corner_point_count( 12 ),
	m_corner_point_actual( -1 ),
	m_corner_radius( 0.f, 0.f ),
	m_corner_ratio( 0.f, 0.f ),
	m_corner_ratio_x( false ),
	m_corner_ratio_y( false ),
	m_corner_auto( false ),
	m_blend_mode( FeBlend::Alpha )
{
	setColor( sf::Color::White );
	m_rect.setTextureRect( sf::IntRect( sf::Vector2i( 0, 0 ), sf::Vector2i( 1, 1 )));
	scale();
}

sf::Vector2f FeRectangle::getPosition() const
{
	return m_position;
}

void FeRectangle::setPosition( const sf::Vector2f &p )
{
	if ( p != m_position )
	{
		m_position = p;
		scale();
		FePresent::script_flag_redraw();
	}
}

sf::Vector2f FeRectangle::getSize() const
{
	return m_size;
}

void FeRectangle::setSize( const sf::Vector2f &s )
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

sf::Color FeRectangle::getColor() const
{
	return m_rect.getFillColor();
}

sf::Color FeRectangle::getOutlineColor()
{
	return m_rect.getOutlineColor();
}

void FeRectangle::setColor( sf::Color c )
{
	if ( c == m_rect.getFillColor() )
		return;

	m_rect.setFillColor( c );
	FePresent::script_flag_redraw();
}

void FeRectangle::setOutlineColor( sf::Color c )
{
	if ( c == m_rect.getOutlineColor() )
		return;

	m_rect.setOutlineColor( c );
	FePresent::script_flag_redraw();
}

float FeRectangle::get_outline()
{
	return m_rect.getOutlineThickness();
}

void FeRectangle::set_outline( float o )
{
	if ( o != m_rect.getOutlineThickness() )
	{
		m_rect.setOutlineThickness( o );
		FePresent::script_flag_redraw();
	}
}

int FeRectangle::get_olr() const
{
	return m_rect.getOutlineColor().r;
}

int FeRectangle::get_olg() const
{
	return m_rect.getOutlineColor().g;
}

int FeRectangle::get_olb() const
{
	return m_rect.getOutlineColor().b;
}

int FeRectangle::get_ola() const
{
	return m_rect.getOutlineColor().a;
}

void FeRectangle::set_olr( int r )
{
	sf::Color c=getOutlineColor();
	c.r=r;
	setOutlineColor(c);
}

void FeRectangle::set_olg( int g )
{
	sf::Color c=getOutlineColor();
	c.g=g;
	setOutlineColor(c);
}

void FeRectangle::set_olb( int b )
{
	sf::Color c=getOutlineColor();
	c.b=b;
	setOutlineColor(c);
}

void FeRectangle::set_ola( int a )
{
	sf::Color c=getOutlineColor();
	c.a=a;
	setOutlineColor(c);
}

void FeRectangle::set_olrgb( int r, int g, int b )
{
	sf::Color c=getOutlineColor();
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
		m_anchor = sf::Vector2f( x, y );
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeRectangle::set_anchor_type( int t )
{
	m_anchor_type = (FeRectangle::Alignment)t;
	sf::Vector2f a = alignTypeToVector( t );
	set_anchor( a.x, a.y );
}

void FeRectangle::set_rotation_origin( float x, float y )
{
	if ( x != m_rotation_origin.x || y != m_rotation_origin.y )
	{
		m_rotation_origin = sf::Vector2f( x, y );
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeRectangle::set_rotation_origin_type( int t )
{
	m_rotation_origin_type = (FeRectangle::Alignment)t;
	sf::Vector2f o = alignTypeToVector( t );
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
	if ( m_corner_point_actual != n )
	{
		m_corner_point_actual = n;
		m_rect.setCornerPointCount( m_corner_point_actual );
	}
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

	m_rect.setCornerRadius( sf::Vector2f( rx, ry ) );
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
			m_corner_radius = sf::Vector2f( s, s );
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

sf::Vector2f FeRectangle::alignTypeToVector( int type )
{
	switch( type )
	{
		case Left:
			return sf::Vector2f( 0.0f, 0.5f );

		case Centre:
			return sf::Vector2f( 0.5f, 0.5f );

		case Right:
			return sf::Vector2f( 1.0f, 0.5f );

		case Top:
			return sf::Vector2f( 0.5f, 0.0f );

		case Bottom:
			return sf::Vector2f( 0.5f, 1.0f );

		case TopLeft:
			return sf::Vector2f( 0.0f, 0.0f );

		case TopRight:
			return sf::Vector2f( 1.0f, 0.0f );

		case BottomLeft:
			return sf::Vector2f( 0.0f, 1.0f );

		case BottomRight:
			return sf::Vector2f( 1.0f, 1.0f );

		default:
			return sf::Vector2f( 0.0f, 0.0f );
	}
}

void FeRectangle::draw( sf::RenderTarget &target, sf::RenderStates states ) const
{
	FeShader *s = get_shader();
	if ( s )
	{
		const sf::Shader *sh = s->get_shader();
		if ( sh )
			states.shader = sh;
	}
	else
		states.shader = FeBlend::get_default_shader_rectangle( m_blend_mode );

	states.blendMode = FeBlend::get_blend_mode( m_blend_mode );

	target.draw( m_rect, states );
}

void FeRectangle::scale()
{
	sf::Vector2f pos = m_position;
	sf::Vector2f size = m_size;

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

	pos += sf::Vector2f(( m_rotation_origin.x - m_anchor.x ) * size.x, ( m_rotation_origin.y -  m_anchor.y ) * size.y );

	m_rect.setPosition( pos );
	m_rect.setRotation( sf::degrees( m_rotation ));
	m_rect.setSize( size );
	m_rect.setOrigin({( m_origin.x + m_rotation_origin.x * size.x ), ( m_origin.y + m_rotation_origin.y * size.y )});

}
