/*
 *
 *  Attract-Mode Plus frontend
 *  Copyright (C) 2026 Andrew Mickelson & Radek Dutkiewicz
 *
 *  This file is part of Attract-Mode Plus.
 *
 *  Attract-Mode Plus is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Attract-Mode Plus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Attract-Mode Plus.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "fe_grid.hpp"
#include "fe_vm.hpp"

#include <sqrat.h>

#include <algorithm>

FeGridContext::FeGridContext( FeVM &vm )
	: m_vm( vm )
{
}

int FePresent::get_layout_grid() const
{
	return m_grid;
}

void FePresent::set_layout_grid( int mode )
{
	if ( mode != m_grid )
	{
		m_grid = mode;
		refresh_script_geometry();
		flag_redraw();
	}
}

bool FePresent::get_layout_grid_uniform() const
{
	return m_grid_uniform;
}

void FePresent::set_layout_grid_uniform( bool uniform )
{
	m_grid_uniform = uniform;
}

bool FePresent::get_layout_pixel_snap() const
{
	return m_pixel_snap;
}

void FePresent::set_layout_pixel_snap( bool pixel_snap )
{
	m_pixel_snap = pixel_snap;
}

float FePresent::get_layout_grid_offset_x() const
{
	return m_grid_offset.x;
}

float FePresent::get_layout_grid_offset_y() const
{
	return m_grid_offset.y;
}

sf::Vector2f FePresent::get_layout_grid_offset( bool uniform ) const
{
	sf::Vector2f size( m_layoutSize );
	if ( uniform && ( m_grid != GridPixel ))
	{
		float side = std::min( size.x, size.y );
		size = sf::Vector2f( side, side );
	}

	switch ( m_grid )
	{
		case GridNormalised:
			return sf::Vector2f(
				size.x * m_grid_offset.x,
				size.y * m_grid_offset.y );

		case GridPercent:
			return sf::Vector2f(
				size.x * m_grid_offset.x / 100.0f,
				size.y * m_grid_offset.y / 100.0f );

		case GridPixel:
		default:
			return m_grid_offset;
	}
}

void FePresent::set_layout_grid_offset_x( float x )
{
	set_layout_grid_offset( x, m_grid_offset.y );
}

void FePresent::set_layout_grid_offset_y( float y )
{
	set_layout_grid_offset( m_grid_offset.x, y );
}

void FePresent::set_layout_grid_offset( float x, float y )
{
	if (( x != m_grid_offset.x ) || ( y != m_grid_offset.y ))
	{
		m_grid_offset = sf::Vector2f( x, y );
		refresh_script_geometry();
		flag_redraw();
	}
}

void FeGridContext::bind( Sqrat::Table &fe )
{
	using namespace Sqrat;

	fe.Bind( _SC("GridGlobals"), Class<FeGridContext, NoConstructor>()
		.Prop( _SC("mode"), &FeGridContext::get_mode, &FeGridContext::set_mode )
		.Prop( _SC("uniform"), &FeGridContext::get_uniform, &FeGridContext::set_uniform )
		.Prop( _SC("pixel_snap"), &FeGridContext::get_pixel_snap, &FeGridContext::set_pixel_snap )
		.Prop( _SC("offset_x"), &FeGridContext::get_offset_x, &FeGridContext::set_offset_x )
		.Prop( _SC("offset_y"), &FeGridContext::get_offset_y, &FeGridContext::set_offset_y )
		.Prop( _SC("left"), &FeGridContext::get_left )
		.Prop( _SC("right"), &FeGridContext::get_right )
		.Prop( _SC("top"), &FeGridContext::get_top )
		.Prop( _SC("bottom"), &FeGridContext::get_bottom )
		.Prop( _SC("width"), &FeGridContext::get_width )
		.Prop( _SC("height"), &FeGridContext::get_height )
		.Prop( _SC("margin_left"), &FeGridContext::get_margin_left )
		.Prop( _SC("margin_right"), &FeGridContext::get_margin_right )
		.Prop( _SC("margin_top"), &FeGridContext::get_margin_top )
		.Prop( _SC("margin_bottom"), &FeGridContext::get_margin_bottom )
	);

	fe.SetInstance( _SC("grid"), this );
}

void FeGridContext::clear_plugins()
{
	m_plugin_settings.clear();
}

void FeGridContext::reset_plugins( std::size_t count )
{
	m_plugin_settings.assign( count, Settings() );
}

FeGridContext::Settings *FeGridContext::get_plugin_settings( int script_id )
{
	if (( script_id < 0 ) || ( script_id >= (int)m_plugin_settings.size() ))
		return NULL;

	return &m_plugin_settings[script_id];
}

const FeGridContext::Settings *FeGridContext::get_plugin_settings( int script_id ) const
{
	if (( script_id < 0 ) || ( script_id >= (int)m_plugin_settings.size() ))
		return NULL;

	return &m_plugin_settings[script_id];
}

int FeGridContext::get_plugin_mode( int script_id ) const
{
	const Settings *settings = get_plugin_settings( script_id );
	return settings ? settings->mode : GridPixel;
}

bool FeGridContext::get_plugin_uniform( int script_id ) const
{
	const Settings *settings = get_plugin_settings( script_id );
	return settings ? settings->uniform : true;
}

bool FeGridContext::get_plugin_pixel_snap( int script_id ) const
{
	const Settings *settings = get_plugin_settings( script_id );
	return settings ? settings->pixel_snap : false;
}

sf::Vector2f FeGridContext::get_plugin_offset( int script_id, bool uniform ) const
{
	const Settings *settings = get_plugin_settings( script_id );
	if ( !settings )
		return sf::Vector2f( 0, 0 );

	sf::Vector2f size( m_vm.get_layout_size() );
	if ( uniform && ( settings->mode != GridPixel ))
	{
		float side = std::min( size.x, size.y );
		size = sf::Vector2f( side, side );
	}

	switch ( settings->mode )
	{
		case GridNormalised:
			return sf::Vector2f(
				size.x * settings->offset.x,
				size.y * settings->offset.y );

		case GridPercent:
			return sf::Vector2f(
				size.x * settings->offset.x / 100.0f,
				size.y * settings->offset.y / 100.0f );

		case GridPixel:
		default:
			return settings->offset;
	}
}

int FeGridContext::get_mode() const
{
	int script_id = m_vm.get_script_id();
	return script_id < 0 ? m_vm.get_layout_grid() : get_plugin_mode( script_id );
}

void FeGridContext::set_mode( int mode )
{
	int script_id = m_vm.get_script_id();
	if ( script_id < 0 )
		m_vm.set_layout_grid( mode );
	else
	{
		Settings *settings = get_plugin_settings( script_id );
		if ( settings )
			settings->mode = mode;
	}
}

bool FeGridContext::get_uniform() const
{
	int script_id = m_vm.get_script_id();
	return script_id < 0 ? m_vm.get_layout_grid_uniform() : get_plugin_uniform( script_id );
}

void FeGridContext::set_uniform( bool uniform )
{
	int script_id = m_vm.get_script_id();
	if ( script_id < 0 )
		m_vm.set_layout_grid_uniform( uniform );
	else
	{
		Settings *settings = get_plugin_settings( script_id );
		if ( settings )
			settings->uniform = uniform;
	}
}

bool FeGridContext::get_pixel_snap() const
{
	int script_id = m_vm.get_script_id();
	return script_id < 0 ? m_vm.get_layout_pixel_snap() : get_plugin_pixel_snap( script_id );
}

void FeGridContext::set_pixel_snap( bool pixel_snap )
{
	int script_id = m_vm.get_script_id();
	if ( script_id < 0 )
		m_vm.set_layout_pixel_snap( pixel_snap );
	else
	{
		Settings *settings = get_plugin_settings( script_id );
		if ( settings )
			settings->pixel_snap = pixel_snap;
	}
}

float FeGridContext::get_offset_x() const
{
	int script_id = m_vm.get_script_id();
	if ( script_id < 0 )
		return m_vm.get_layout_grid_offset_x();

	const Settings *settings = get_plugin_settings( script_id );
	return settings ? settings->offset.x : 0.0f;
}

void FeGridContext::set_offset_x( float x )
{
	int script_id = m_vm.get_script_id();
	if ( script_id < 0 )
	{
		m_vm.set_layout_grid_offset_x( x );
		return;
	}

	Settings *settings = get_plugin_settings( script_id );
	if ( settings && ( x != settings->offset.x ))
	{
		settings->offset.x = x;
		m_vm.refresh_script_geometry();
		m_vm.flag_redraw();
	}
}

float FeGridContext::get_offset_y() const
{
	int script_id = m_vm.get_script_id();
	if ( script_id < 0 )
		return m_vm.get_layout_grid_offset_y();

	const Settings *settings = get_plugin_settings( script_id );
	return settings ? settings->offset.y : 0.0f;
}

void FeGridContext::set_offset_y( float y )
{
	int script_id = m_vm.get_script_id();
	if ( script_id < 0 )
	{
		m_vm.set_layout_grid_offset_y( y );
		return;
	}

	Settings *settings = get_plugin_settings( script_id );
	if ( settings && ( y != settings->offset.y ))
	{
		settings->offset.y = y;
		m_vm.refresh_script_geometry();
		m_vm.flag_redraw();
	}
}

FeCoordinateSpace FeGridContext::get_bounds() const
{
	int mode = get_mode();
	bool uniform = get_uniform();
	sf::Vector2f parent_size( m_vm.get_layout_size() );
	sf::Vector2f space_origin( 0, 0 );
	sf::Vector2f space_size( parent_size );

	if ( uniform && ( mode != GridPixel ))
	{
		float side = std::min( parent_size.x, parent_size.y );
		space_size = sf::Vector2f( side, side );
		space_origin = sf::Vector2f(
			( parent_size.x - side ) / 2.0f,
			( parent_size.y - side ) / 2.0f );
	}

	int script_id = m_vm.get_script_id();
	sf::Vector2f offset = script_id < 0
		? m_vm.get_layout_grid_offset( uniform )
		: get_plugin_offset( script_id, uniform );

	if ( mode == GridPixel )
		return FeCoordinateSpace( sf::Vector2f( -offset.x, -offset.y ), parent_size );

	float extent = mode == GridNormalised ? 1.0f : 100.0f;
	sf::Vector2f origin(
		space_size.x != 0.0f ? -( space_origin.x + offset.x ) * extent / space_size.x : 0.0f,
		space_size.y != 0.0f ? -( space_origin.y + offset.y ) * extent / space_size.y : 0.0f );
	if ( origin.x == 0.0f )
		origin.x = 0.0f;
	if ( origin.y == 0.0f )
		origin.y = 0.0f;
	sf::Vector2f size(
		space_size.x != 0.0f ? parent_size.x * extent / space_size.x : 0.0f,
		space_size.y != 0.0f ? parent_size.y * extent / space_size.y : 0.0f );
	return FeCoordinateSpace( origin, size );
}

sf::Vector2f FeGridContext::get_extent() const
{
	switch ( get_mode() )
	{
		case GridNormalised:
			return sf::Vector2f( 1, 1 );

		case GridPercent:
			return sf::Vector2f( 100, 100 );

		case GridPixel:
		default:
			return sf::Vector2f( m_vm.get_layout_size() );
	}
}

float FeGridContext::get_left() const
{
	return get_bounds().origin.x;
}

float FeGridContext::get_right() const
{
	FeCoordinateSpace bounds = get_bounds();
	return bounds.origin.x + bounds.size.x;
}

float FeGridContext::get_top() const
{
	return get_bounds().origin.y;
}

float FeGridContext::get_bottom() const
{
	FeCoordinateSpace bounds = get_bounds();
	return bounds.origin.y + bounds.size.y;
}

float FeGridContext::get_width() const
{
	return get_bounds().size.x;
}

float FeGridContext::get_height() const
{
	return get_bounds().size.y;
}

float FeGridContext::get_margin_left() const
{
	return std::max( 0.0f, -get_left() );
}

float FeGridContext::get_margin_right() const
{
	return std::max( 0.0f, get_right() - get_extent().x );
}

float FeGridContext::get_margin_top() const
{
	return std::max( 0.0f, -get_top() );
}

float FeGridContext::get_margin_bottom() const
{
	return std::max( 0.0f, get_bottom() - get_extent().y );
}

sf::Vector2f FeGridContext::window_to_pos( const sf::Vector2i &pos ) const
{
	sf::Vector2f layout_pos = m_vm.get_transform().getInverse().transformPoint( sf::Vector2f( pos ));
	FeCoordinateSpace bounds = get_bounds();
	sf::Vector2i layout_size = m_vm.get_layout_size();
	return sf::Vector2f(
		layout_size.x != 0 ? bounds.origin.x + bounds.size.x * layout_pos.x / layout_size.x : bounds.origin.x,
		layout_size.y != 0 ? bounds.origin.y + bounds.size.y * layout_pos.y / layout_size.y : bounds.origin.y );
}
