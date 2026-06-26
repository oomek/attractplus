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
#include "fe_present.hpp"
#include "fe_color.hpp"

FeBasePresentable::FeBasePresentable( FePresentableParent &p )
	: m_parent( &p ),
	m_shader( NULL ),
	m_visible( true ),
	m_zorder( 0 ),
	m_script_pos( 0, 0 ),
	m_script_size( 0, 0 ),
	m_grid( 0 ),
	m_grid_uniform( true ),
	m_script_geometry_set( false )
{
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		m_grid_uniform = fep->get_layout_grid_uniform();
}

FeBasePresentable::~FeBasePresentable()
{
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

sf::Vector2f FeBasePresentable::convert_position( const sf::Vector2f &p ) const
{
	FeCoordinateSpace space = m_parent ? m_parent->get_coordinate_space( get_grid_uniform() ) : FeCoordinateSpace();
	sf::Vector2f offset = m_parent ? m_parent->get_grid_offset( get_grid_uniform() ) : sf::Vector2f( 0, 0 );

	switch ( get_grid() )
	{
		case GridPercent:
			return sf::Vector2f(
				space.origin.x + space.size.x * p.x / 100.0f,
				space.origin.y + space.size.y * p.y / 100.0f ) + offset;
		case GridPixel:
		default:
			return p + offset;
	}
}

sf::Vector2f FeBasePresentable::convert_size( const sf::Vector2f &s ) const
{
	FeCoordinateSpace space = m_parent ? m_parent->get_coordinate_space( get_grid_uniform() ) : FeCoordinateSpace();

	switch ( get_grid() )
	{
		case GridPercent:
			return sf::Vector2f( space.size.x * s.x / 100.0f, space.size.y * s.y / 100.0f );
		case GridPixel:
		default:
			return s;
	}
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
	m_script_pos.x = x;
	m_script_geometry_set = true;
	setPosition( convert_position( m_script_pos ));
}

void FeBasePresentable::set_y( float y )
{
	m_script_pos.y = y;
	m_script_geometry_set = true;
	setPosition( convert_position( m_script_pos ));
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
	m_script_size.x = w;
	m_script_geometry_set = true;
	setSize( convert_size( m_script_size ));
}

void FeBasePresentable::set_height( float h )
{
	m_script_size.y = h;
	m_script_geometry_set = true;
	setSize( convert_size( m_script_size ));
}

void FeBasePresentable::set_pos(float x, float y)
{
	m_script_pos = sf::Vector2f( x, y );
	m_script_geometry_set = true;
	setPosition( convert_position( m_script_pos ));
}

void FeBasePresentable::set_pos(float x, float y, float w, float h)
{
	m_script_pos = sf::Vector2f( x, y );
	m_script_size = sf::Vector2f( w, h );
	m_script_geometry_set = true;
	setPosition( convert_position( m_script_pos ));
	setSize( convert_size( m_script_size ));
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

	setPosition( convert_position( m_script_pos ));
	setSize( convert_size( m_script_size ));
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

FeImage *FePresentableParent::add_surface(int w, int h)
{
	return add_surface( 0, 0, w, h );
}

FeImage *FePresentableParent::add_surface(float x, float y, int w, int h)
{
	FePresent *fep = FePresent::script_get_fep();

	if ( fep )
		return fep->add_surface( x, y, w, h, *this );

	return NULL;
}

FeImage *FePresentableParent::add_surface(
		float x, float y, float w, float h, int pixel_w, int pixel_h )
{
	FePresent *fep = FePresent::script_get_fep();

	if ( fep )
		return fep->add_surface( x, y, w, h, pixel_w, pixel_h, *this );

	return NULL;
}
