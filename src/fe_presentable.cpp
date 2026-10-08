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
#include "fe_animation.hpp"
#include "fe_present.hpp"
#include "fe_model_3d.hpp"
#include "fe_color.hpp"

#include <cmath>
#include <sqrat.h>

FeGridView::FeGridView( FePresentableParent *parent, FeGrid *settings )
	: m_parent( parent ), m_settings( settings )
{
}

void FeGridView::refresh()
{
	FePresent::script_get_fep()->refresh_script_geometry();
	FePresent::script_flag_redraw();
}

void FeGridView::set_range( const Vec2f &offset, const Vec2f &size )
{
	if ( m_settings->size != size || m_settings->offset != offset )
	{
		m_settings->size = size;
		m_settings->offset = offset;
		refresh();
	}
}

float FeGridView::get_left() const
{
	return m_settings->transform( m_parent->get_grid_extent() ).inverse_position( Vec2f( 0, 0 ) ).x;
}

float FeGridView::get_top() const
{
	return m_settings->transform( m_parent->get_grid_extent() ).inverse_position( Vec2f( 0, 0 ) ).y;
}

float FeGridView::get_right() const
{
	Vec2f extent = m_parent->get_grid_extent();
	return m_settings->transform( extent ).inverse_position( extent ).x;
}

float FeGridView::get_bottom() const
{
	Vec2f extent = m_parent->get_grid_extent();
	return m_settings->transform( extent ).inverse_position( extent ).y;
}

float FeGridView::get_width() const
{
	Vec2f extent = m_parent->get_grid_extent();
	return m_settings->transform( extent ).inverse_size( extent ).x;
}

float FeGridView::get_height() const
{
	Vec2f extent = m_parent->get_grid_extent();
	return m_settings->transform( extent ).inverse_size( extent ).y;
}

void FeGridView::set_flag( bool &flag, Sqrat::Object value )
{
	if ( value.GetType() != OT_BOOL )
	{
		Sqrat::Error::Instance().Throw( value.GetVM(), "grid property requires a boolean" );
		return;
	}
	bool enabled = value.Cast<bool>();
	if ( flag != enabled )
	{
		flag = enabled;
		refresh();
	}
}

bool FeGridView::get_uniform( FeGridView *grid )
{
	return grid->m_settings->uniform;
}

void FeGridView::set_uniform( FeGridView *grid, Sqrat::Object value )
{
	grid->set_flag( grid->m_settings->uniform, value );
}

bool FeGridView::get_centered( FeGridView *grid )
{
	return grid->m_settings->center;
}

void FeGridView::set_centered( FeGridView *grid, Sqrat::Object value )
{
	grid->set_flag( grid->m_settings->center, value );
}

bool FeGridView::get_pixel_snap( FeGridView *grid )
{
	return grid->m_settings->pixel_snap;
}

void FeGridView::set_pixel_snap( FeGridView *grid, Sqrat::Object value )
{
	grid->set_flag( grid->m_settings->pixel_snap, value );
}

FeBasePresentable::FeBasePresentable( FePresentableParent &p )
	: m_parent( &p ),
	m_snap_x( false ),
	m_snap_y( false ),
	m_snap_width( false ),
	m_snap_height( false ),
	m_snap_offset( 0, 0 ),
	m_shader( NULL ),
	m_visible( true ),
	m_zbuffer( false ),
	m_z( 0.0f ),
	m_rotation_x( 0.0f ),
	m_rotation_y( 0.0f ),
	m_rotation_order( XYZ ),
	m_zorder( 0 ),
	m_script_pos( 0, 0 ),
	m_script_size( 0, 0 ),
	m_script_id( -1 ),
	m_script_geometry_set( false )
{
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		m_script_id = fep->get_script_id();
}

FeBasePresentable::~FeBasePresentable()
{
	FeAnimation::remove( this );
}

FePresentableParent::FePresentableParent( )
	: m_nesting_level ( 0 )
{
}

FePresentableParent::~FePresentableParent()
{
}

Vec2f FePresentableParent::get_grid_extent() const
{
	FePresent *fep = FePresent::script_get_fep();
	return fep ? Vec2f( fep->get_layout_size() ) : Vec2f( 0, 0 );
}

const FeGrid &FePresentableParent::get_grid( int ) const
{
	return grid;
}

FeGridView FePresentableParent::get_grid_view()
{
	return FeGridView( this, &grid );
}

Vec2f FePresentableParent::snap_position_to_pixel( const Vec2f &p ) const
{
	return Vec2f( std::round( p.x ), std::round( p.y ));
}

Vec2f FePresentableParent::snap_size_to_pixel( const Vec2f &s ) const
{
	return Vec2f( std::round( s.x ), std::round( s.y ));
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

void FeBasePresentable::on_transform_update()
{
}

FeGridTransform FeBasePresentable::grid_transform() const
{
	return m_parent->get_grid( m_script_id ).transform( m_parent->get_grid_extent() );
}

float FeBasePresentable::pixels_to_grid_height( float value ) const
{
	return value / grid_transform().scale.y;
}

float FeBasePresentable::pixels_to_grid_width( float value ) const
{
	return value / grid_transform().scale.x;
}

Vec2f FeBasePresentable::snap_position( const Vec2f &value, bool snap ) const
{
	Vec2f p = grid_transform().position( value );
	// 2D drawables snap position and size together after applying their anchors.
	if ( get_type() != 0 ) return p;
	if ( !snap || !get_pixel_snap() || !m_parent )
		return p;

	Vec2f result = m_parent->snap_position_to_pixel( p );
	if ( !m_snap_x || m_snap_width ) result.x = p.x;
	if ( !m_snap_y || m_snap_height ) result.y = p.y;
	return result;
}

Vec2f FeBasePresentable::snap_size( const Vec2f &value, bool snap ) const
{
	Vec2f s = grid_transform().size( value );
	if ( get_type() != 0 ) return s;
	if ( !snap || !get_pixel_snap() ) return s;
	Vec2f result = m_parent->snap_size_to_pixel( s );
	if ( !m_snap_width ) result.x = s.x;
	if ( !m_snap_height ) result.y = s.y;
	return result;
}

Vec2f FeBasePresentable::snap_draw_position( const Vec2f &pos ) const
{
	if ( !get_pixel_snap() || !m_parent || ( getRotation() != 0.0f )
			|| !( ( m_snap_x && m_snap_width ) || ( m_snap_y && m_snap_height ) ) )
		return pos;

	Vec2f edge = getPosition() + m_snap_offset;
	Vec2f snapped = m_parent->snap_position_to_pixel( edge );
	Vec2f adjusted = pos;
	if ( m_snap_x && m_snap_width ) adjusted.x += snapped.x - edge.x;
	if ( m_snap_y && m_snap_height ) adjusted.y += snapped.y - edge.y;
	return adjusted;
}

void FeBasePresentable::snap_draw_geometry( Vec2f &pos, Vec2f &size, bool endpoint ) const
{
	if ( !get_pixel_snap() || !m_parent ) return;
	bool snap_x = endpoint || m_snap_x;
	bool snap_y = endpoint || m_snap_y;
	bool snap_width = endpoint || m_snap_width;
	bool snap_height = endpoint || m_snap_height;
	Vec2f snapped_pos = m_parent->snap_position_to_pixel( pos );
	Vec2f snapped_size = m_parent->snap_size_to_pixel( size );

	// Rotated drawables retain independent size snapping.
	if ( getRotation() != 0.0f || get_type() == 0 )
	{
		if ( snap_x && ( endpoint || !snap_width ) ) pos.x = snapped_pos.x;
		if ( snap_y && ( endpoint || !snap_height ) ) pos.y = snapped_pos.y;
		if ( snap_width ) size.x = snapped_size.x;
		if ( snap_height ) size.y = snapped_size.y;
		return;
	}

	Vec2f anchor( get_anchor_x(), get_anchor_y() );
	Vec2f origin( get_origin_x(), get_origin_y() );
	Vec2f edge( pos.x - anchor.x * size.x - origin.x,
		pos.y - anchor.y * size.y - origin.y );
	Vec2f first = m_parent->snap_position_to_pixel( edge );
	Vec2f last = m_parent->snap_position_to_pixel( edge + size );
	if ( snap_x && snap_width )
	{
		size.x = last.x - first.x;
		pos.x = first.x + anchor.x * size.x + origin.x;
	}
	else
	{
		if ( snap_x ) pos.x = snapped_pos.x;
		if ( snap_width ) size.x = snapped_size.x;
	}
	if ( snap_y && snap_height )
	{
		size.y = last.y - first.y;
		pos.y = first.y + anchor.y * size.y + origin.y;
	}
	else
	{
		if ( snap_y ) pos.y = snapped_pos.y;
		if ( snap_height ) size.y = snapped_size.y;
	}
}

float FeBasePresentable::get_anchor_x() const
{
	return 0.0f;
}

float FeBasePresentable::get_anchor_y() const
{
	return 0.0f;
}

float FeBasePresentable::get_origin_x() const
{
	return 0.0f;
}

float FeBasePresentable::get_origin_y() const
{
	return 0.0f;
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
	return m_script_geometry_set ? m_script_pos.x : grid_transform().inverse_position( getPosition() ).x;
}

float FeBasePresentable::get_y() const
{
	return m_script_geometry_set ? m_script_pos.y : grid_transform().inverse_position( getPosition() ).y;
}

float FeBasePresentable::get_z() const
{
	return m_z;
}

void FeBasePresentable::set_snap( bool &property, bool value )
{
	if ( property == value ) return;
	property = value;
	on_transform_update();
	FePresent::script_flag_redraw();
}

void FeBasePresentable::set_x( float x )
{
	m_script_pos.x = x;
	m_script_geometry_set = true;
	set_snap( m_snap_x, get_pixel_snap() && m_parent );
	Vec2f pos = getPosition();
	pos.x = snap_position( m_script_pos ).x;
	setPosition( pos );
	FeAnimation::stop( this, _SC("x") );
}

void FeBasePresentable::set_y( float y )
{
	m_script_pos.y = y;
	m_script_geometry_set = true;
	set_snap( m_snap_y, get_pixel_snap() && m_parent );
	Vec2f pos = getPosition();
	pos.y = snap_position( m_script_pos ).y;
	setPosition( pos );
	FeAnimation::stop( this, _SC("y") );
}

void FeBasePresentable::set_z( float z )
{
	if ( z == m_z )
		return;

	m_z = z;

	FePresent::script_flag_sort_zorder();
	FePresent::script_flag_redraw();
}

float FeBasePresentable::get_width() const
{
	return m_script_geometry_set ? m_script_size.x : pixels_to_grid_width( getSize().x );
}

float FeBasePresentable::get_height() const
{
	return m_script_geometry_set ? m_script_size.y : pixels_to_grid_height( getSize().y );
}

void FeBasePresentable::set_width( float w )
{
	m_script_size.x = w;
	m_script_geometry_set = true;
	set_snap( m_snap_width, get_pixel_snap() && m_parent );
	Vec2f size = getSize();
	size.x = snap_size( m_script_size ).x;
	setSize( size );
	FeAnimation::stop( this, _SC("width") );
}

void FeBasePresentable::set_height( float h )
{
	m_script_size.y = h;
	m_script_geometry_set = true;
	set_snap( m_snap_height, get_pixel_snap() && m_parent );
	Vec2f size = getSize();
	size.y = snap_size( m_script_size ).y;
	setSize( size );
	FeAnimation::stop( this, _SC("height") );
}

void FeBasePresentable::set_pos(float x, float y)
{
	m_script_pos = Vec2f( x, y );
	m_script_geometry_set = true;
	set_snap( m_snap_x, get_pixel_snap() && m_parent );
	set_snap( m_snap_y, m_snap_x );
	setPosition( snap_position( m_script_pos ));
	FeAnimation::stop( this, _SC("x") );
	FeAnimation::stop( this, _SC("y") );
}

void FeBasePresentable::set_pos(float x, float y, float w, float h)
{
	m_script_pos = Vec2f( x, y );
	m_script_size = Vec2f( w, h );
	m_script_geometry_set = true;
	set_snap( m_snap_x, get_pixel_snap() && m_parent );
	set_snap( m_snap_y, m_snap_x );
	set_snap( m_snap_width, m_snap_x );
	set_snap( m_snap_height, m_snap_x );
	setPosition( snap_position( m_script_pos ));
	setSize( snap_size( m_script_size ));
	FeAnimation::stop( this, _SC("x") );
	FeAnimation::stop( this, _SC("y") );
	FeAnimation::stop( this, _SC("width") );
	FeAnimation::stop( this, _SC("height") );
}

bool FeBasePresentable::set_animated_property( const std::string &name, float value, bool snap )
{
	if ( name == "x" )
	{
		m_script_pos.x = value;
		m_script_geometry_set = true;
		set_snap( m_snap_x, snap && get_pixel_snap() && m_parent );
		Vec2f pos = getPosition();
		pos.x = snap_position( m_script_pos, snap ).x;
		setPosition( pos );
		return true;
	}
	else if ( name == "y" )
	{
		m_script_pos.y = value;
		m_script_geometry_set = true;
		set_snap( m_snap_y, snap && get_pixel_snap() && m_parent );
		Vec2f pos = getPosition();
		pos.y = snap_position( m_script_pos, snap ).y;
		setPosition( pos );
		return true;
	}
	else if ( name == "width" )
	{
		m_script_size.x = value;
		m_script_geometry_set = true;
		set_snap( m_snap_width, snap && get_pixel_snap() && m_parent );
		Vec2f size = getSize();
		size.x = snap_size( m_script_size, snap ).x;
		setSize( size );
		return true;
	}
	else if ( name == "height" )
	{
		m_script_size.y = value;
		m_script_geometry_set = true;
		set_snap( m_snap_height, snap && get_pixel_snap() && m_parent );
		Vec2f size = getSize();
		size.y = snap_size( m_script_size, snap ).y;
		setSize( size );
		return true;
	}

	return false;
}

float FeBasePresentable::snap_destination_to_pixels( const std::string &name, float destination ) const
{
	if ( !get_pixel_snap() || !m_parent )
		return destination;

	bool position = ( name == "x" ) || ( name == "y" );
	bool size = ( name == "width" ) || ( name == "height" );
	if ( !position && !size )
		return destination;

	bool x_axis = ( name == "x" ) || ( name == "width" );

	Vec2f pos = m_script_pos;
	Vec2f extent = m_script_size;
	Vec2f &value = position ? pos : extent;
	if ( x_axis ) value.x = destination;
	else value.y = destination;
	FeGridTransform transform = grid_transform();
	pos = transform.position( pos );
	extent = transform.size( extent );
	snap_draw_geometry( pos, extent, true );
	Vec2f snapped = position ? transform.inverse_position( pos ) : transform.inverse_size( extent );
	return x_axis ? snapped.x : snapped.y;
}

bool FeBasePresentable::get_pixel_snap() const
{
	return m_parent->get_grid( m_script_id ).pixel_snap;
}

void FeBasePresentable::set_parent( FePresentableParent &p )
{
	m_parent = &p;
	FePresent *fep = FePresent::script_get_fep();
	m_script_id = fep ? fep->get_script_id() : -1;
}

void FeBasePresentable::set_script_geometry( float x, float y, float w, float h )
{
	m_script_pos = Vec2f( x, y );
	m_script_size = Vec2f( w, h );
	m_script_geometry_set = true;
	refresh_script_geometry();
}

void FeBasePresentable::refresh_script_geometry()
{
	if ( !m_script_geometry_set )
		return;

	m_snap_x = get_pixel_snap() && !FeAnimation::is_running( this, "x" );
	m_snap_y = get_pixel_snap() && !FeAnimation::is_running( this, "y" );
	m_snap_width = get_pixel_snap() && !FeAnimation::is_running( this, "width" );
	m_snap_height = get_pixel_snap() && !FeAnimation::is_running( this, "height" );
	setPosition( snap_position( m_script_pos ));
	setSize( snap_size( m_script_size ));
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
	Color c = getColor();
	set_rgb( r, c.g, c.b, c.a );
}

void FeBasePresentable::set_g(int g)
{
	Color c = getColor();
	set_rgb( c.r, g, c.b, c.a );
}

void FeBasePresentable::set_b(int b)
{
	Color c = getColor();
	set_rgb( c.r, c.g, b, c.a );
}

void FeBasePresentable::set_a(int a)
{
	Color c = getColor();
	set_rgb( c.r, c.g, c.b, a );
}

void FeBasePresentable::set_rgb(int r, int g, int b)
{
	set_rgb( r, g, b, getColor().a );
}

void FeBasePresentable::set_rgb(int r, int g, int b, int a)
{
	setColor( Color( r, g, b, a ) );
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

bool FeBasePresentable::get_zbuffer() const
{
	return m_zbuffer;
}

void FeBasePresentable::set_zbuffer( bool enabled )
{
	if ( enabled == m_zbuffer )
		return;

	m_zbuffer = enabled;
	FePresent::script_flag_redraw();
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

float FeBasePresentable::get_rotation_x() const
{
	return m_rotation_x;
}

void FeBasePresentable::set_rotation_x( float rotation )
{
	if ( rotation == m_rotation_x )
		return;

	m_rotation_x = rotation;
	on_transform_update();
	FePresent::script_flag_redraw();
}

float FeBasePresentable::get_rotation_y() const
{
	return m_rotation_y;
}

void FeBasePresentable::set_rotation_y( float rotation )
{
	if ( rotation == m_rotation_y )
		return;

	m_rotation_y = rotation;
	on_transform_update();
	FePresent::script_flag_redraw();
}

float FeBasePresentable::get_rotation_z() const
{
	return getRotation();
}

void FeBasePresentable::set_rotation_z( float rotation )
{
	setRotation( rotation );
}

int FeBasePresentable::get_rotation_order() const
{
	return static_cast<int>( m_rotation_order );
}

void FeBasePresentable::set_rotation_order( int order )
{
	if ( order < XYZ || order > ZYX )
		return;

	const RotationOrder new_order = static_cast<RotationOrder>( order );
	if ( new_order == m_rotation_order )
		return;

	m_rotation_order = new_order;
	on_transform_update();
	FePresent::script_flag_redraw();
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

FeModel3D *FePresentableParent::add_model_3d(const char *n)
{
	FePresent *fep = FePresent::script_get_fep();

	if ( fep )
		return fep->add_model_3d( n, *this );

	return NULL;
}

FeImage *FePresentableParent::add_clone(FeImage *i )
{
	FePresent *fep = FePresent::script_get_fep();

	if ( fep )
		return fep->add_clone( i, *this );

	return NULL;
}

FeModel3D *FePresentableParent::add_clone( FeModel3D *m )
{
	FePresent *fep = FePresent::script_get_fep();

	if ( fep )
		return fep->add_clone( m, *this );

	return NULL;
}

FeText *FePresentableParent::add_text(const char *t, float x, float y, float w, float h)
{
	FePresent *fep = FePresent::script_get_fep();

	if ( fep )
		return fep->add_text( t, x, y, w, h, *this );

	return NULL;
}

FeListBox *FePresentableParent::add_listbox(float x, float y, float w, float h)
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
		const FeGrid &grid = get_grid( fep->get_script_id() );
		FeGridTransform transform = grid.transform( get_grid_extent() );
		Vec2f size = transform.size( Vec2f( w, h ) );
		if ( grid.pixel_snap )
		{
			Vec2f pos = transform.position( Vec2f( x, y ) );
			size = snap_position_to_pixel( pos + size ) - snap_position_to_pixel( pos );
		}
		return fep->add_surface( x, y, w, h,
			static_cast<int>( std::round( size.x ) ),
			static_cast<int>( std::round( size.y ) ), *this );
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
