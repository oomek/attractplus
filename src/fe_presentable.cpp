/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013 Andrew Mickelson
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

#include "fe_presentable.hpp"
#include "fe_animate.hpp"
#include "fe_present.hpp"
#include "fe_color.hpp"

#include <cmath>

FeBasePresentable::FeBasePresentable( FePresentableParent &p )
	: m_parent( &p ),
	m_snap_x( false ),
	m_snap_y( false ),
	m_snap_width( false ),
	m_snap_height( false ),
	m_snap_offset( 0, 0 ),
	m_shader( NULL ),
	m_visible( true ),
	m_zorder( 0 ),
	m_script_pos( 0, 0 ),
	m_script_size( 0, 0 ),
	m_grid( 0 ),
	m_grid_uniform( true ),
	m_pixel_snap( false ),
	m_script_geometry_set( false )
{
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
	{
		m_grid_uniform = fep->get_layout_grid_uniform();
		m_pixel_snap = fep->get_layout_pixel_snap();
	}
}

FeBasePresentable::~FeBasePresentable()
{
	FeAnimate::remove( this );
}

FePresentableParent::FePresentableParent( )
	: m_nesting_level ( 0 )
{
}

FePresentableParent::~FePresentableParent()
{
}

FeCoordinateSpace FePresentableParent::get_coordinate_space( bool ) const
{
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		return FeCoordinateSpace( sf::Vector2f( 0, 0 ), sf::Vector2f( fep->get_layout_size() ));

	return FeCoordinateSpace();
}

sf::Vector2f FePresentableParent::get_grid_offset( bool ) const
{
	return sf::Vector2f( 0, 0 );
}

sf::Vector2f FePresentableParent::snap_position_to_pixel( const sf::Vector2f &p ) const
{
	return sf::Vector2f( std::round( p.x ), std::round( p.y ));
}

sf::Vector2f FePresentableParent::snap_size_to_pixel( const sf::Vector2f &s ) const
{
	return sf::Vector2f( std::round( s.x ), std::round( s.y ));
}

void FePresentableParent::refresh_script_geometry()
{
	for ( std::vector<FeBasePresentable *>::iterator itr=elements.begin();
			itr != elements.end(); ++itr )
		(*itr)->refresh_script_geometry();
}

void FeBasePresentable::on_new_selection( FeSettings * )
{
}

void FeBasePresentable::on_new_list( FeSettings * )
{
}

void FeBasePresentable::set_scale_factor( float, float )
{
}

sf::Vector2f FeBasePresentable::pos_from_grid_units( const sf::Vector2f &p, bool snap ) const
{
	FeCoordinateSpace space = m_parent ? m_parent->get_coordinate_space( get_grid_uniform() ) : FeCoordinateSpace();
	sf::Vector2f offset = m_parent ? m_parent->get_grid_offset( get_grid_uniform() ) : sf::Vector2f( 0, 0 );
	sf::Vector2f pos;

	switch ( get_grid() )
	{
		case GridNormalised:
			pos = sf::Vector2f(
				space.origin.x + space.size.x * p.x,
				space.origin.y + space.size.y * p.y ) + offset;
			break;
		case GridPercent:
			pos = sf::Vector2f(
				space.origin.x + space.size.x * p.x / 100.0f,
				space.origin.y + space.size.y * p.y / 100.0f ) + offset;
			break;
		case GridPixel:
		default:
			pos = p + offset;
			break;
	}

	if ( snap && get_pixel_snap() && m_parent )
	{
		sf::Vector2f snapped = m_parent->snap_position_to_pixel( pos );
		if ( !m_snap_width ) pos.x = snapped.x;
		if ( !m_snap_height ) pos.y = snapped.y;
	}

	return pos;
}

sf::Vector2f FeBasePresentable::size_from_grid_units( const sf::Vector2f &s, bool snap ) const
{
	FeCoordinateSpace space = m_parent ? m_parent->get_coordinate_space( get_grid_uniform() ) : FeCoordinateSpace();
	sf::Vector2f size;

	switch ( get_grid() )
	{
		case GridNormalised:
			size = sf::Vector2f( space.size.x * s.x, space.size.y * s.y );
			break;
		case GridPercent:
			size = sf::Vector2f( space.size.x * s.x / 100.0f, space.size.y * s.y / 100.0f );
			break;
		case GridPixel:
		default:
			size = s;
			break;
	}

	if ( snap && get_pixel_snap() && m_parent )
		size = m_parent->snap_size_to_pixel( size );

	return size;
}

sf::Vector2f FeBasePresentable::snap_draw_position( const sf::Vector2f &pos ) const
{
	if ( !get_pixel_snap() || !m_parent || ( getRotation() != 0.0f )
			|| !( ( m_snap_x && m_snap_width ) || ( m_snap_y && m_snap_height ) ) )
		return pos;

	sf::Vector2f edge = getPosition() + m_snap_offset;
	sf::Vector2f snapped = m_parent->snap_position_to_pixel( edge );
	sf::Vector2f adjusted = pos;
	if ( m_snap_x && m_snap_width ) adjusted.x += snapped.x - edge.x;
	if ( m_snap_y && m_snap_height ) adjusted.y += snapped.y - edge.y;
	return adjusted;
}

int FeBasePresentable::getIndexOffset() const
{
	return 0;
}

void FeBasePresentable::setIndexOffset( int io )
{
}

int FeBasePresentable::getFilterOffset() const
{
	return 0;
}

void FeBasePresentable::setFilterOffset( int io )
{
}

float FeBasePresentable::get_x() const
{
	return m_script_geometry_set ? m_script_pos.x : getPosition().x;
}

float FeBasePresentable::get_y() const
{
	return m_script_geometry_set ? m_script_pos.y : getPosition().y;
}

void FeBasePresentable::set_x( float x )
{
	FeAnimate::remove( this, _SC("x") );
	m_script_pos.x = x;
	m_script_geometry_set = true;
	m_snap_x = get_pixel_snap() && m_parent;
	sf::Vector2f pos = getPosition();
	pos.x = pos_from_grid_units( m_script_pos ).x;
	setPosition( pos );
}

void FeBasePresentable::set_y( float y )
{
	FeAnimate::remove( this, _SC("y") );
	m_script_pos.y = y;
	m_script_geometry_set = true;
	m_snap_y = get_pixel_snap() && m_parent;
	sf::Vector2f pos = getPosition();
	pos.y = pos_from_grid_units( m_script_pos ).y;
	setPosition( pos );
}

float FeBasePresentable::get_width() const
{
	return m_script_geometry_set ? m_script_size.x : getSize().x;
}

float FeBasePresentable::get_height() const
{
	return m_script_geometry_set ? m_script_size.y : getSize().y;
}

void FeBasePresentable::set_width( float w )
{
	FeAnimate::remove( this, _SC("width") );
	m_script_size.x = w;
	m_script_geometry_set = true;
	m_snap_width = get_pixel_snap() && m_parent;
	sf::Vector2f size = getSize();
	size.x = size_from_grid_units( m_script_size ).x;
	setSize( size );
}

void FeBasePresentable::set_height( float h )
{
	FeAnimate::remove( this, _SC("height") );
	m_script_size.y = h;
	m_script_geometry_set = true;
	m_snap_height = get_pixel_snap() && m_parent;
	sf::Vector2f size = getSize();
	size.y = size_from_grid_units( m_script_size ).y;
	setSize( size );
}

void FeBasePresentable::set_pos(float x, float y)
{
	FeAnimate::remove( this, _SC("x") );
	FeAnimate::remove( this, _SC("y") );
	m_script_pos = sf::Vector2f( x, y );
	m_script_geometry_set = true;
	m_snap_x = get_pixel_snap() && m_parent;
	m_snap_y = m_snap_x;
	setPosition( pos_from_grid_units( m_script_pos ));
}

void FeBasePresentable::set_pos(float x, float y, float w, float h)
{
	FeAnimate::remove( this, _SC("x") );
	FeAnimate::remove( this, _SC("y") );
	FeAnimate::remove( this, _SC("width") );
	FeAnimate::remove( this, _SC("height") );
	m_script_pos = sf::Vector2f( x, y );
	m_script_size = sf::Vector2f( w, h );
	m_script_geometry_set = true;
	m_snap_x = get_pixel_snap() && m_parent;
	m_snap_y = m_snap_x;
	m_snap_width = m_snap_x;
	m_snap_height = m_snap_x;
	setPosition( pos_from_grid_units( m_script_pos ));
	setSize( size_from_grid_units( m_script_size ));
}

bool FeBasePresentable::set_animated_property( const std::string &name, float value, bool snap )
{
	if ( name == "x" )
	{
		m_script_pos.x = value;
		m_script_geometry_set = true;
		m_snap_x = snap && get_pixel_snap() && m_parent;
		sf::Vector2f pos = getPosition();
		pos.x = pos_from_grid_units( m_script_pos, snap ).x;
		setPosition( pos );
		return true;
	}
	else if ( name == "y" )
	{
		m_script_pos.y = value;
		m_script_geometry_set = true;
		m_snap_y = snap && get_pixel_snap() && m_parent;
		sf::Vector2f pos = getPosition();
		pos.y = pos_from_grid_units( m_script_pos, snap ).y;
		setPosition( pos );
		return true;
	}
	else if ( name == "width" )
	{
		m_script_size.x = value;
		m_script_geometry_set = true;
		m_snap_width = snap && get_pixel_snap() && m_parent;
		sf::Vector2f size = getSize();
		size.x = size_from_grid_units( m_script_size, snap ).x;
		setSize( size );
		return true;
	}
	else if ( name == "height" )
	{
		m_script_size.y = value;
		m_script_geometry_set = true;
		m_snap_height = snap && get_pixel_snap() && m_parent;
		sf::Vector2f size = getSize();
		size.y = size_from_grid_units( m_script_size, snap ).y;
		setSize( size );
		return true;
	}

	return false;
}

float FeBasePresentable::snap_grid_destination_to_pixels( const std::string &name, float destination ) const
{
	if ( !get_pixel_snap() || !m_parent )
		return destination;

	bool position = ( name == "x" ) || ( name == "y" );
	bool size = ( name == "width" ) || ( name == "height" );
	if ( !position && !size )
		return destination;

	bool x_axis = ( name == "x" ) || ( name == "width" );
	FeCoordinateSpace space = m_parent->get_coordinate_space( get_grid_uniform() );
	float value;

	if ( position )
	{
		sf::Vector2f pos = m_script_pos;
		if ( x_axis )
			pos.x = destination;
		else
			pos.y = destination;

		bool snap_edge = ( x_axis && m_snap_width ) || ( !x_axis && m_snap_height );
		sf::Vector2f snapped;
		if ( snap_edge )
		{
			sf::Vector2f edge = pos_from_grid_units( pos ) + m_snap_offset;
			snapped = m_parent->snap_position_to_pixel( edge ) - m_snap_offset;
		}
		else
			snapped = pos_from_grid_units( pos );

		sf::Vector2f offset = m_parent->get_grid_offset( get_grid_uniform() );
		value = x_axis ? snapped.x - offset.x : snapped.y - offset.y;
	}
	else
	{
		sf::Vector2f size = m_script_size;
		if ( x_axis )
			size.x = destination;
		else
			size.y = destination;

		sf::Vector2f snapped = size_from_grid_units( size );
		value = x_axis ? snapped.x : snapped.y;
	}

	float extent = x_axis ? space.size.x : space.size.y;
	float origin = position ? ( x_axis ? space.origin.x : space.origin.y ) : 0.0f;

	switch ( get_grid() )
	{
		case GridNormalised:
			return extent != 0.0f ? ( value - origin ) / extent : 0.0f;
		case GridPercent:
			return extent != 0.0f ? ( value - origin ) * 100.0f / extent : 0.0f;
		case GridPixel:
		default:
			return value;
	}
}

int FeBasePresentable::get_grid() const
{
	if ( m_grid )
		return m_grid;

	FePresent *fep = FePresent::script_get_fep();
	return fep ? fep->get_layout_grid() : GridPixel;
}

void FeBasePresentable::set_grid( int g )
{
	if ( g != m_grid )
	{
		m_grid = g;
		refresh_script_geometry();
	}
}

bool FeBasePresentable::get_grid_uniform() const
{
	return m_grid_uniform;
}

void FeBasePresentable::set_grid_uniform( bool u )
{
	if ( u != m_grid_uniform )
	{
		m_grid_uniform = u;
		refresh_script_geometry();
	}
}

bool FeBasePresentable::get_pixel_snap() const
{
	return m_pixel_snap;
}

void FeBasePresentable::set_pixel_snap( bool s )
{
	if ( s != m_pixel_snap )
	{
		m_pixel_snap = s;
		refresh_script_geometry();
	}
}

void FeBasePresentable::set_parent( FePresentableParent &p )
{
	m_parent = &p;
}

void FeBasePresentable::set_script_geometry( float x, float y, float w, float h )
{
	m_script_pos = sf::Vector2f( x, y );
	m_script_size = sf::Vector2f( w, h );
	m_script_geometry_set = true;
	refresh_script_geometry();
}

void FeBasePresentable::refresh_script_geometry()
{
	if ( !m_script_geometry_set )
		return;

	m_snap_x = get_pixel_snap() && m_parent;
	m_snap_y = m_snap_x;
	m_snap_width = m_snap_x;
	m_snap_height = m_snap_x;
	setPosition( pos_from_grid_units( m_script_pos ));
	setSize( size_from_grid_units( m_script_size ));
}

int FeBasePresentable::get_r() const
{
	return getColor().r;
}

int FeBasePresentable::get_g() const
{
	return getColor().g;
}

int FeBasePresentable::get_b() const
{
	return getColor().b;
}

int FeBasePresentable::get_a() const
{
	return getColor().a;
}

void FeBasePresentable::set_r(int r)
{
	sf::Color c = getColor();
	set_rgb( r, c.g, c.b, c.a );
}

void FeBasePresentable::set_g(int g)
{
	sf::Color c = getColor();
	set_rgb( c.r, g, c.b, c.a );
}

void FeBasePresentable::set_b(int b)
{
	sf::Color c = getColor();
	set_rgb( c.r, c.g, b, c.a );
}

void FeBasePresentable::set_a(int a)
{
	sf::Color c = getColor();
	set_rgb( c.r, c.g, c.b, a );
}

void FeBasePresentable::set_rgb(int r, int g, int b)
{
	set_rgb( r, g, b, getColor().a );
}

void FeBasePresentable::set_rgb(int r, int g, int b, int a)
{
	setColor(sf::Color( r, g, b, a ));
}

bool FeBasePresentable::get_visible() const
{
	return m_visible;
}

void FeBasePresentable::set_visible( bool v )
{
	if ( v != m_visible )
	{
		m_visible = v;
		FePresent::script_flag_redraw();
	}
}

FeShader *FeBasePresentable::get_shader() const
{
	return m_shader;
}

FeShader *FeBasePresentable::script_get_shader() const
{
	if ( m_shader )
		return m_shader;
	else
	{
		FePresent *fep = FePresent::script_get_fep();
		return fep->get_empty_shader();
	}
}

void FeBasePresentable::script_set_shader( FeShader *sh )
{
	m_shader = sh;
}

int FeBasePresentable::get_zorder()
{
	return m_zorder;
}

void FeBasePresentable::set_zorder( int pos )
{
	if ( pos == m_zorder )
		return;

	m_zorder = pos;

	FePresent::script_flag_sort_zorder();
	FePresent::script_flag_redraw();
}

bool FeBasePresentable::get_magic() const
{
	return false;
}

int FeBasePresentable::get_type() const
{
	return 0;
}

int FePresentableParent::get_nesting_level()
{
	return m_nesting_level;
}

void FePresentableParent::set_nesting_level( int p )
{
	m_nesting_level = p;
}

FeImage *FePresentableParent::add_image(const char *n, float x, float y, float w, float h)
{
	FePresent *fep = FePresent::script_get_fep();

	if ( fep )
		return fep->add_image( false, n, x, y, w, h, *this );

	return NULL;
}

FeImage *FePresentableParent::add_image(const char *n, float x, float y )
{
	return add_image( n, x, y, 0, 0 );
}

FeImage *FePresentableParent::add_image(const char *n )
{
	return add_image( n, 0, 0, 0, 0 );
}

FeImage *FePresentableParent::add_artwork(const char *l, float x, float y, float w, float h )
{
	FePresent *fep = FePresent::script_get_fep();

	if ( fep )
		return fep->add_image( true, l, x, y, w, h, *this );

	return NULL;
}

FeImage *FePresentableParent::add_artwork(const char *l, float x, float y)
{
	return add_artwork( l, x, y, 0, 0 );
}

FeImage *FePresentableParent::add_artwork(const char *l )
{
	return add_artwork( l, 0, 0, 0, 0 );
}

FeImage *FePresentableParent::add_clone(FeImage *i )
{
	FePresent *fep = FePresent::script_get_fep();

	if ( fep )
		return fep->add_clone( i, *this );

	return NULL;
}

FeText *FePresentableParent::add_text(const char *t, int x, int y, int w, int h)
{
	FePresent *fep = FePresent::script_get_fep();

	if ( fep )
		return fep->add_text( t, x, y, w, h, *this );

	return NULL;
}

FeListBox *FePresentableParent::add_listbox(int x, int y, int w, int h)
{
	FePresent *fep = FePresent::script_get_fep();

	if ( fep )
		return fep->add_listbox( x, y, w, h, *this );

	return NULL;
}

FeRectangle *FePresentableParent::add_rectangle(float x, float y, float w, float h)
{
	FePresent *fep = FePresent::script_get_fep();

	if ( fep )
		return fep->add_rectangle( x, y, w, h, *this );

	return NULL;
}

FeImage *FePresentableParent::add_surface(float w, float h)
{
	return add_surface( 0, 0, w, h );
}

FeImage *FePresentableParent::add_surface(float x, float y, float w, float h)
{
	FePresent *fep = FePresent::script_get_fep();

	if ( fep )
	{
		sf::Vector2i texture_size = fep->get_surface_texture_size( *this, w, h );
		FeCoordinateSpace space = get_coordinate_space( fep->m_grid_uniform );

		switch ( fep->m_grid )
		{
			case GridNormalised:
				w = space.size.x != 0.0f ? texture_size.x / space.size.x : 0.0f;
				h = space.size.y != 0.0f ? texture_size.y / space.size.y : 0.0f;
				break;

			case GridPercent:
				w = space.size.x != 0.0f ? texture_size.x * 100.0f / space.size.x : 0.0f;
				h = space.size.y != 0.0f ? texture_size.y * 100.0f / space.size.y : 0.0f;
				break;

			case GridPixel:
			default:
				w = texture_size.x;
				h = texture_size.y;
				break;
		}

		return fep->add_surface( x, y, w, h, texture_size.x, texture_size.y, *this );
	}

	return NULL;
}

FeImage *FePresentableParent::add_surface(
		float x, float y, float w, float h, int texture_width, int texture_height )
{
	FePresent *fep = FePresent::script_get_fep();

	if ( fep )
		return fep->add_surface( x, y, w, h, texture_width, texture_height, *this );

	return NULL;
}
