/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-15 Andrew Mickelson
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

#include "fe_text.hpp"
#include "fe_settings.hpp"
#include "fe_util.hpp"
#include "fe_shader.hpp"
#include "fe_present.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <iostream>

FeText::FeText( FePresentableParent &p, const std::string &str,
	float x, float y, float w, float h )
	: FeBasePresentable( p ),
	m_string( str ),
	m_string_wrapped( str ),
	m_user_charsize( -1 ),
	m_user_margin( -1 ),
	m_outline( 0 ),
	m_bg_outline( 0 ),
	m_size( w, h ),
	m_position( x, y ),
	m_transform_origin( 0.f, 0.f ),
	m_anchor( 0.f, 0.f ),
	m_rotation_origin( 0.f, 0.f ),
	m_transform_origin_type( FeAlign::TopLeft ),
	m_anchor_type( FeAlign::TopLeft ),
	m_rotation_origin_type( FeAlign::TopLeft ),
	m_rotation( 0.0 ),
	m_scale_factor( 1.0 ),
	m_magic( false )
{
	update_font_size();
	update_transform();
}

void FeText::setFont( const FeFont &f )
{
	m_font = &f;
	m_glyphs_dirty = true;
	m_draw_text.setFont( m_scale_style == ScaleNone ? f : f.getUnhinted() );
}

Vec2f FeText::getPosition() const
{
	return m_position;
}

void FeText::setPosition( const Vec2f &p )
{
	m_position = p;
	update_transform();
	FePresent::script_do_update( this );
}

Vec2f FeText::getSize() const
{
	return m_size;
}

void FeText::setSize( const Vec2f &s )
{
	m_size = s;
	update_transform();
	FePresent::script_do_update( this );
}

float FeText::getRotation() const
{
	return m_rotation;
}

void FeText::setRotation( float r )
{
	if ( r == m_rotation )
		return;

	m_rotation = r;
	update_transform();
	FePresent::script_do_update( this );
}

int FeText::get_transform_origin_type() const
{
	return m_transform_origin_type;
}

int FeText::get_anchor_type() const
{
	return m_anchor_type;
}

int FeText::get_rotation_origin_type() const
{
	return m_rotation_origin_type;
}

float FeText::get_transform_origin_x() const
{
	return m_transform_origin.x;
}

float FeText::get_transform_origin_y() const
{
	return m_transform_origin.y;
}

float FeText::get_anchor_x() const
{
	return m_anchor.x;
}

float FeText::get_anchor_y() const
{
	return m_anchor.y;
}

float FeText::get_rotation_origin_x() const
{
	return m_rotation_origin.x;
}

float FeText::get_rotation_origin_y() const
{
	return m_rotation_origin.y;
}

void FeText::set_transform_origin( float x, float y )
{
	if (
		x != m_transform_origin.x || x != m_anchor.x || x != m_rotation_origin.x ||
		y != m_transform_origin.y || y != m_anchor.y || y != m_rotation_origin.y
	)
	{
		m_transform_origin = Vec2f( x, y );
		m_anchor = Vec2f( x, y );
		m_rotation_origin = Vec2f( x, y );
		update_transform();
		FePresent::script_flag_redraw();
	}
}

void FeText::set_transform_origin_type( int t )
{
	m_transform_origin_type = static_cast<FeAlign>( t );
	Vec2f a = align_type_to_vector( t );
	set_transform_origin( a.x, a.y );
}

void FeText::set_anchor( float x, float y )
{
	if ( x != m_anchor.x || y != m_anchor.y )
	{
		m_anchor = Vec2f( x, y );
		update_transform();
		FePresent::script_flag_redraw();
	}
}

void FeText::set_anchor_type( int t )
{
	m_anchor_type = static_cast<FeAlign>( t );
	Vec2f a = align_type_to_vector( t );
	set_anchor( a.x, a.y );
}

void FeText::set_rotation_origin( float x, float y )
{
	if ( x != m_rotation_origin.x || y != m_rotation_origin.y )
	{
		m_rotation_origin = Vec2f( x, y );
		update_transform();
		FePresent::script_flag_redraw();
	}
}

void FeText::set_rotation_origin_type( int t )
{
	m_rotation_origin_type = static_cast<FeAlign>( t );
	Vec2f o = align_type_to_vector( t );
	set_rotation_origin( o.x, o.y );
}

void FeText::set_transform_origin_x( float x )
{
	if ( x != m_transform_origin.x || x != m_anchor.x || x != m_rotation_origin.x )
	{
		m_transform_origin.x = x;
		m_anchor.x = x;
		m_rotation_origin.x = x;
		update_transform();
		FePresent::script_flag_redraw();
	}
}

void FeText::set_transform_origin_y( float y )
{
	if ( y != m_transform_origin.y || y != m_anchor.y || y != m_rotation_origin.y )
	{
		m_transform_origin.y = y;
		m_anchor.y = y;
		m_rotation_origin.y = y;
		update_transform();
		FePresent::script_flag_redraw();
	}
}

void FeText::set_anchor_x( float x )
{
	set_anchor( x, get_anchor_y() );
}

void FeText::set_anchor_y( float y )
{
	set_anchor( get_anchor_x(), y );
}

void FeText::set_rotation_origin_x( float x )
{
	set_rotation_origin( x, get_rotation_origin_y() );
}

void FeText::set_rotation_origin_y( float y )
{
	set_rotation_origin( get_rotation_origin_x(), y );
}

void FeText::set_rgb(int r, int g, int b, int a)
{
	FeBasePresentable::set_rgb(r, g, b, a);
	if ( m_link_outline_alpha )
		set_outline_alpha( -1 );
}

void FeText::setColor( Color c )
{
	if ( c == m_draw_text.getColor() )
		return;

	m_draw_text.setColor( c );
	FePresent::script_flag_redraw();
}

Color FeText::getColor() const
{
	return m_draw_text.getColor();
}

void FeText::setIndexOffset( int io )
{
	FeBasePresentable::setIndexOffset( io );
}

int FeText::getIndexOffset() const
{
	return FeBasePresentable::getIndexOffset();
}

void FeText::setFilterOffset( int fo )
{
	FeBasePresentable::setFilterOffset( fo );
}

int FeText::getFilterOffset() const
{
	return FeBasePresentable::getFilterOffset();
}

void FeText::on_new_list( FeSettings *s )
{
	// We only update the font size and scale if the string is not empty
	// so we do not render any unnecessary glyphs when the script updates the height of text
	//
	if ( m_string.size() > 0 )
		update_font_size();

	update_transform();
}

void FeText::update_font_size()
{
	const float unit = style_grid_scale( m_scale_style ).y;
	if ( !( m_grid_charsize > 0.f ))
	{
		Vec2f pos = m_position;
		Vec2f size = m_size;
		snap_draw_geometry( pos, size );
		const float parent_unit = style_grid_scale( m_scale_style & ScaleParent ).y;
		const float height = size.y * parent_unit / grid_transform().scale.y;
		m_grid_charsize = ( height > 12.f ? height - 4.f : 8.f ) / unit;
	}

	const float char_size = m_grid_charsize * unit;
	if ( m_pixel_charsize < 0.f )
	{
		m_pixel_charsize = std::max( 1.f, char_size );
		m_outline_ratio = m_outline / m_grid_charsize;
		m_glyphs_dirty = true;
	}

	float scale = 1.f / m_scale_factor;
	if ( m_scale_style != ScaleNone )
	{
		const float desired = std::max( 1.f, char_size * m_scale_factor );
		m_raster_size = m_first_size;
		while ( m_raster_size < m_last_size && desired > m_raster_size )
			m_raster_size *= 2;
		scale *= desired / m_raster_size;
	}
	else
		m_raster_size = static_cast<unsigned int>( std::round(
			std::max( 1.f, m_pixel_charsize * m_scale_factor )));

	m_draw_text.setTextScale( Vec2f( scale, scale ) );
	m_draw_text.setCharacterSize( m_raster_size );
	update_outline();
}

void FeText::prepare_glyphs( const std::string &str )
{
	if ( m_scale_style == ScaleNone || !m_font )
		return;
	if ( !m_glyphs_dirty && str == m_prepared_string )
		return;

	// Five doubled levels, prepared for all characters before any group resize.
	const unsigned int base = 1u << static_cast<unsigned int>( std::clamp(
		std::floor( std::log2( std::max( 1.f, m_pixel_charsize * m_scale_factor ))), 0.f, 10.f ));
	m_first_size = std::max( 1u, base / 4 );
	m_last_size = base * 4;
	const FeFont &font = m_font->getUnhinted();
	const bool bold = m_draw_text.getStyle() & FeJustifyText::Bold;
	auto chars = utf8_to_utf32( str + " Xx" );
	if ( m_draw_text.getCase() & FeTextPrimitive::Capitalize )
	{
		const auto count = chars.size();
		for ( std::size_t i = 0; i < count; ++i )
			chars += std::toupper( chars[i] );
	}
	for ( unsigned int size = m_first_size; size <= m_last_size; size *= 2 )
		for ( auto c : chars )
		{
			if ( c == '\n' || c == '\r' )
				continue;
			if ( m_draw_text.getCase() & FeTextPrimitive::Uppercase ) c = std::toupper( c );
			if ( m_draw_text.getCase() & FeTextPrimitive::Lowercase ) c = std::tolower( c );
			font.getGlyph( c, size, bold );
			if ( m_outline_ratio > 0.f )
				font.getGlyph( c, size, bold, m_outline_ratio * size );
		}
	m_prepared_string = str;
	m_glyphs_dirty = false;
}

void FeText::update_margin()
{
	if ( m_user_margin < 0 )
	{
		m_draw_text.setMargin( -1 );
		return;
	}

	m_draw_text.setMargin( static_cast<int>(
		std::round( std::max( 0.0f, grid_height_to_pixels( m_user_margin )))));
}

void FeText::update_outline()
{
	const float unit = style_grid_scale( m_scale_style ).y;
	if ( m_scale_style != ScaleNone )
		m_draw_text.setOutlineThickness( m_outline_ratio * m_raster_size, true );
	else
		m_draw_text.setOutlineThickness( m_outline * unit );
	m_draw_text.setBgOutlineThickness( m_bg_outline * unit );
}

void FeText::update_transform()
{
	Vec2f pos = m_position;
	Vec2f size = m_size;
	snap_draw_geometry( pos, size );
	pos += Vec2f(
		( m_rotation_origin.x - m_anchor.x ) * size.x,
		( m_rotation_origin.y - m_anchor.y ) * size.y
	);
	Vec2f origin = Vec2f(
		m_rotation_origin.x * size.x,
		m_rotation_origin.y * size.y
	);

	m_draw_text.setSize( size );
	m_draw_text.setAlignmentSnap( get_pixel_snap() );
	m_draw_text.setOrigin( origin );
	m_draw_text.setPosition( pos );
	m_draw_text.setRotation( m_rotation );
}

void FeText::on_new_selection( FeSettings *feSettings )
{
	// Prepare glyphs after the initial box has been resolved through the parent grid.
	if ( !m_geometry_initialized ) return;

	std::string str = m_string;

	m_magic = FePresent::script_process_magic_strings( str,
			get_effective_filter_offset(),
			get_effective_index_offset() );

	m_magic = feSettings->do_text_substitutions( str, get_effective_filter_offset(), get_effective_index_offset() ) || m_magic;

	prepare_glyphs( str );
	update_font_size();
	m_draw_text.setString( str );
}

void FeText::set_scale_factor( float scale_x, float scale_y )
{
	const float new_scale_factor = ( scale_x > scale_y ) ? scale_x : scale_y;
	if ( new_scale_factor == m_scale_factor )
		return;

	m_scale_factor = new_scale_factor;
	if ( m_scale_factor <= 0.f )
		m_scale_factor = 1.f;

	update_font_size();
	update_transform();
	update_margin();
	update_outline();
}

void FeText::refresh_script_geometry()
{
	FeBasePresentable::refresh_script_geometry();
	if ( !m_geometry_initialized )
	{
		m_geometry_initialized = true;
		m_grid_charsize = m_user_charsize;
		m_pixel_charsize = -1.f;
		update_font_size();
		FePresent::script_do_update( this );
	}
	update_transform();
	update_font_size();
	update_margin();
	update_outline();
}

void FeText::build_render_geometry( std::vector<FeRenderGeometry> &geometry ) const
{
	const std::size_t begin = geometry.size();
	m_draw_text.append_render_geometry( geometry, get_z() );
	if ( get_shader() )
	{
		for ( std::size_t i = begin; i < geometry.size(); ++i )
		{
			geometry[i].zbuffer = get_zbuffer();
			geometry[i].shader = get_shader();
			geometry[i].custom_shader = true;
		}
	}
	else
	{
		for ( std::size_t i = begin; i < geometry.size(); ++i )
			geometry[i].zbuffer = get_zbuffer();
	}
	for ( std::size_t i = begin; i < geometry.size(); ++i ) apply_group_geometry( geometry[i] );
}

void FeText::set_word_wrap( bool w )
{
	m_draw_text.setWordWrap( w );
	FePresent::script_do_update( this );
}

bool FeText::get_word_wrap()
{
	return m_draw_text.getWordWrap();
}

void FeText::set_no_margin( bool m )
{
	m_user_margin = m ? 0.0f : -1.0f;
	update_margin();
	FePresent::script_do_update( this );
}

bool FeText::get_no_margin()
{
	return ( m_user_margin >= 0.0f );
}

void FeText::set_margin( float m )
{
	float margin = ( m < 0.0f ) ? -1.0f : m;
	if ( margin == m_user_margin )
		return;

	m_user_margin = margin;
	update_margin();
	FePresent::script_do_update( this );
}

float FeText::get_margin()
{
	return m_user_margin;
}

void FeText::set_outline( float t )
{
	t = std::max( 0.0f, t );
	m_outline = t;
	m_outline_ratio = t / m_grid_charsize;
	m_glyphs_dirty = true;
	update_outline();
	FePresent::script_do_update( this );
}

float FeText::get_outline()
{
	return m_outline;
}

void FeText::set_bg_outline( float t )
{
	if ( t == m_bg_outline )
		return;

	m_bg_outline = t;
	update_outline();
	FePresent::script_do_update( this );
}

float FeText::get_bg_outline()
{
	return m_bg_outline;
}

void FeText::set_first_line_hint( int l )
{
	if ( l != m_draw_text.getFirstLineHint() )
	{
		m_draw_text.setFirstLineHint( l );
		FePresent::script_do_update( this );
	}
}

int FeText::get_first_line_hint()
{
	return m_draw_text.getFirstLineHint();
}

int FeText::get_lines()
{
	return m_draw_text.getLines();
}

int FeText::get_lines_total()
{
	return m_draw_text.getLinesTotal();
}

const char *FeText::get_string()
{
	return m_string.c_str();
}

const char *FeText::get_string_wrapped()
{
	m_string_wrapped = m_draw_text.getStringWrapped();
	return m_string_wrapped.c_str();
}

int FeText::get_type() const
{
	return FePresentableTypeText;
}

bool FeText::get_magic() const
{
	return m_magic;
}

void FeText::set_string(const char *s)
{
	m_string=s;
	m_magic=false;
	FePresent::script_do_update( this );
}

float FeText::get_cursor_pos( int i )
{
	if ( m_string.empty() || get_word_wrap() )
		return 0;
	int pos = std::clamp( i, 0, (int)m_string.size() );
	std::basic_string<std::uint32_t> str = utf8_to_utf32( m_string );
	return pixels_to_grid_width( m_draw_text.setString( str, pos ).x
		- ( m_draw_text.getPosition().x - m_draw_text.getOrigin().x ));
}

float FeText::get_actual_width()
{
	return pixels_to_grid_width( m_draw_text.getActualWidth() );
}

float FeText::get_actual_height()
{
	return pixels_to_grid_height( m_draw_text.getActualHeight() );
}

int FeText::get_bg_red()
{
	return m_draw_text.getBgColor().r;
}

int FeText::get_bg_green()
{
	return m_draw_text.getBgColor().g;
}

int FeText::get_bg_blue()
{
	return m_draw_text.getBgColor().b;
}

int FeText::get_bg_alpha()
{
	return m_draw_text.getBgColor().a;
}

int FeText::get_bg_outline_red()
{
	return m_draw_text.getBgOutlineColor().r;
}

int FeText::get_bg_outline_green()
{
	return m_draw_text.getBgOutlineColor().g;
}

int FeText::get_bg_outline_blue()
{
	return m_draw_text.getBgOutlineColor().b;
}

int FeText::get_bg_outline_alpha()
{
	return m_draw_text.getBgOutlineColor().a;
}

int FeText::get_outline_red()
{
	return m_draw_text.getOutlineColor().r;
}

int FeText::get_outline_green()
{
	return m_draw_text.getOutlineColor().g;
}

int FeText::get_outline_blue()
{
	return m_draw_text.getOutlineColor().b;
}

int FeText::get_outline_alpha()
{
	return m_draw_text.getOutlineColor().a;
}

float FeText::get_charsize()
{
	if ( m_user_charsize > 0 )
		return m_user_charsize;

	return pixels_to_grid_height( m_draw_text.getCharacterSize() );
}

float FeText::get_glyph_size()
{
	return pixels_to_grid_height( m_draw_text.getGlyphSize() );
}

float FeText::get_spacing()
{
	return m_draw_text.getCharacterSpacing();
}

float FeText::get_line_spacing()
{
	return m_draw_text.getLineSpacing();
}

float FeText::get_line_height()
{
	return pixels_to_grid_height( m_draw_text.getLineSpacingFactored(
		m_draw_text.getFont(), m_draw_text.getCharacterSize() ));
}

int FeText::get_style()
{
	return m_draw_text.getStyle();
}

int FeText::get_justify()
{
	return m_draw_text.getJustify();
}

int FeText::get_align()
{
	return m_draw_text.getAlignment();
}

int FeText::get_case()
{
	return (int)m_draw_text.getCase();
}

const char *FeText::get_font()
{
	return m_font_name.c_str();
}

void FeText::set_bg_red(int r)
{
	Color c = m_draw_text.getBgColor();
	set_bg_rgb( r, c.g, c.b, c.a );
}

void FeText::set_bg_green(int g)
{
	Color c = m_draw_text.getBgColor();
	set_bg_rgb( c.r, g, c.b, c.a );
}

void FeText::set_bg_blue(int b)
{
	Color c = m_draw_text.getBgColor();
	set_bg_rgb( c.r, c.g, b, c.a );
}

void FeText::set_bg_alpha(int a)
{
	Color c = m_draw_text.getBgColor();
	set_bg_rgb( c.r, c.g, c.b, a );
}

void FeText::set_bg_rgb( int r, int g, int b )
{
	Color old = m_draw_text.getBgColor();
	Color col = Color( r, g, b, old.a );
	if ( col != old )
		set_bg_rgb( r, g, b, col.a == 0 ? 255 : col.a ); // legacy - force alpha if none
}

void FeText::set_bg_rgb( int r, int g, int b, int a )
{
	Color old = m_draw_text.getBgColor();
	Color col = Color( r, g, b, a );
	if ( col != old )
	{
		m_draw_text.setBgColor( col );
		FePresent::script_flag_redraw();
	}
	if ( m_link_bg_outline_alpha )
		set_bg_outline_alpha( -1 );
}

void FeText::set_bg_outline_red(int r)
{
	Color c = m_draw_text.getBgOutlineColor();
	set_bg_outline_rgb( r, c.g, c.b, c.a );
}

void FeText::set_bg_outline_green(int g)
{
	Color c = m_draw_text.getBgOutlineColor();
	set_bg_outline_rgb( c.r, g, c.b, c.a );
}

void FeText::set_bg_outline_blue(int b)
{
	Color c = m_draw_text.getBgOutlineColor();
	set_bg_outline_rgb( c.r, c.g, b, c.a );
}

void FeText::set_bg_outline_alpha(int a)
{
	Color c = m_draw_text.getBgOutlineColor();
	set_bg_outline_rgb( c.r, c.g, c.b, a );
}

void FeText::set_bg_outline_rgb( int r, int g, int b )
{
	Color old = m_draw_text.getBgOutlineColor();
	Color col = Color( r, g, b, old.a );
	if ( col != old )
		set_bg_outline_rgb( r, g, b, m_link_bg_outline_alpha ? -1 : ( col.a == 0 ? 255 : col.a ) ); // legacy - force alpha if none
}

void FeText::set_bg_outline_rgb( int r, int g, int b, int a )
{
	m_link_bg_outline_alpha = a == -1;
	if ( m_link_bg_outline_alpha ) a = get_bg_alpha();
	Color old = m_draw_text.getBgOutlineColor();
	Color col = Color( r, g, b, a );
	if ( col != old )
	{
		m_draw_text.setBgOutlineColor( col );
		FePresent::script_flag_redraw();
	}
}

void FeText::set_outline_red(int r)
{
	Color c = m_draw_text.getOutlineColor();
	set_outline_rgb( r, c.g, c.b, c.a );
}

void FeText::set_outline_green(int g)
{
	Color c = m_draw_text.getOutlineColor();
	set_outline_rgb( c.r, g, c.b, c.a );
}

void FeText::set_outline_blue(int b)
{
	Color c = m_draw_text.getOutlineColor();
	set_outline_rgb( c.r, c.g, b, c.a );
}

void FeText::set_outline_alpha(int a)
{
	Color c = m_draw_text.getOutlineColor();
	set_outline_rgb( c.r, c.g, c.b, a );
}

void FeText::set_outline_rgb( int r, int g, int b )
{
	Color old = m_draw_text.getOutlineColor();
	Color col = Color( r, g, b, old.a );
	if ( col != old )
		set_outline_rgb( r, g, b, m_link_outline_alpha ? -1 : ( col.a == 0 ? 255 : col.a ) ); // legacy - force alpha if none
}

void FeText::set_outline_rgb( int r, int g, int b, int a )
{
	m_link_outline_alpha = a == -1;
	if ( m_link_outline_alpha ) a = get_a();
	Color old = m_draw_text.getOutlineColor();
	Color col = Color( r, g, b, a );
	if ( col != old )
	{
		m_draw_text.setOutlineColor( col );
		FePresent::script_flag_redraw();
	}
}

void FeText::set_charsize(float s)
{
	m_user_charsize = m_grid_charsize = s;
	m_pixel_charsize = -1.f;
	update_font_size();
	FePresent::script_do_update( this );
}

void FeText::set_scale_style( int mode )
{
	if ( mode < ScaleNone || mode > ScaleBoth || m_scale_style == mode )
		return;
	m_scale_style = mode;
	if ( m_font ) setFont( *m_font );
	m_pixel_charsize = -1.f;
	update_font_size();
	FePresent::script_do_update( this );
}

void FeText::set_spacing(float s)
{
	if ( s != m_draw_text.getCharacterSpacing() )
	{
		m_draw_text.setCharacterSpacing(s);
		FePresent::script_do_update( this );
	}
}

void FeText::set_line_spacing(float s)
{
	m_draw_text.setLineSpacing(s);
	FePresent::script_do_update( this );
}

void FeText::set_style(int s)
{
	if ( s != m_draw_text.getStyle() )
	{
		m_draw_text.setStyle(s);
		m_glyphs_dirty = true;
		FePresent::script_do_update( this );
	}
}

void FeText::set_justify(int j)
{
	if ( j != m_draw_text.getJustify() )
	{
		m_draw_text.setJustify(j);
		FePresent::script_do_update( this );
	}
}

void FeText::set_align(int a)
{
	FeAlign align = static_cast<FeAlign>( a );
	if ( align != m_draw_text.getAlignment() )
	{
		m_draw_text.setAlignment( align );
		FePresent::script_do_update( this );
	}
}

void FeText::set_case(int c)
{
	if ( c != m_draw_text.getCase() )
	{
		m_draw_text.setCase( (FeTextPrimitive::Case)c );
		m_glyphs_dirty = true;
		FePresent::script_do_update( this );
	}
}

void FeText::set_font( const char *f )
{
	FePresent *fep = FePresent::script_get_fep();
	if ( !fep )
		return;

	const FeFontContainer *fc = fep->get_pooled_font( f );

	if ( !fc )
		return;

	const FeFont *font=&(fc->get_font());
	if ( font )
	{
		setFont( *font );
		m_font_name = f;

		FePresent::script_do_update( this );
	}
}
