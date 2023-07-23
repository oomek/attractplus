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

FeRectangle::FeRectangle( FePresentableParent &p,
	float x, float y, float w, float h )
	: FeBasePresentable( p ),
	m_position( x, y ),
	m_size( w, h ),
	m_rotation ( 0.0 ),
	m_origin( 0.f, 0.f ),
	m_anchor( 0.f, 0.f ),
	m_rotation_origin( 0.f, 0.f ),
	m_anchor_type( TopLeft ),
	m_rotation_origin_type( TopLeft ),
	m_blend_mode( FeBlend::Alpha ),
	m_rect( sf::Vector2f( w, h ))
{
	setColor( sf::Color::White );
	// setSize( w, h );
	// setPosition( x, y );
	m_rect.setTextureRect( sf::IntRect( sf::Vector2i( 0, 0 ), sf::Vector2i( 1, 1 )));
	scale();
}

const sf::Vector2f &FeRectangle::getPosition() const
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

const sf::Vector2f &FeRectangle::getSize() const
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

const sf::Color &FeRectangle::getColor() const
{
	return m_rect.getFillColor();
}

const sf::Color &FeRectangle::getOutlineColor() const
{
	return m_rect.getOutlineColor();
}

void FeRectangle::setColor( const sf::Color &c )
{
	if ( c == m_rect.getFillColor() )
		return;

	m_rect.setFillColor( c );
	FePresent::script_flag_redraw();
}

void FeRectangle::setOutlineColor( const sf::Color &c )
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
		states.shader = FeBlend::get_default_shader( m_blend_mode );

	states.blendMode = FeBlend::get_blend_mode( m_blend_mode );

	target.draw( m_rect, states );
}

void FeRectangle::scale()
{
	sf::Vector2f final_pos = m_position;

	final_pos += sf::Vector2f(( m_rotation_origin.x - m_anchor.x ) * m_size.x, ( m_rotation_origin.y -  m_anchor.y ) * m_size.y );

	m_rect.setPosition( final_pos );
	m_rect.setRotation( m_rotation );
	m_rect.setSize( m_size );
	m_rect.setOrigin(( m_origin.x + m_rotation_origin.x * m_size.x ), ( m_origin.y + m_rotation_origin.y * m_size.y ));
}