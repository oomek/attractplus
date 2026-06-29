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
#include <iostream>

FeText::FeText( FePresentableParent &p, const std::string &str,
	int x, int y, int w, int h )
	: FeBasePresentable( p ),
	m_string( str ),
	m_string_wrapped( str ),
	m_index_offset( 0 ),
	m_filter_offset( 0 ),
	m_user_charsize( -1 ),
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

void FeText::setFont( const sf::Font &f )
{
	m_draw_text.setFont( f );
}

sf::Vector2f FeText::getPosition() const
{
	return m_position;
}

void FeText::setPosition( const sf::Vector2f &p )
{
	m_position = p;
	update_transform();
	FePresent::script_do_update( this );
}

sf::Vector2f FeText::getSize() const
{
	return m_size;
}

void FeText::setSize( const sf::Vector2f &s )
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
		m_transform_origin = sf::Vector2f( x, y );
		m_anchor = sf::Vector2f( x, y );
		m_rotation_origin = sf::Vector2f( x, y );
		update_transform();
		FePresent::script_flag_redraw();
	}
}

void FeText::set_transform_origin_type( int t )
{
	m_transform_origin_type = static_cast<FeAlign>( t );
	sf::Vector2f a = align_type_to_vector( t );
	set_transform_origin( a.x, a.y );
}

void FeText::set_anchor( float x, float y )
{
	if ( x != m_anchor.x || y != m_anchor.y )
	{
		m_anchor = sf::Vector2f( x, y );
		update_transform();
		FePresent::script_flag_redraw();
	}
}

void FeText::set_anchor_type( int t )
{
	m_anchor_type = static_cast<FeAlign>( t );
	sf::Vector2f a = align_type_to_vector( t );
	set_anchor( a.x, a.y );
}

void FeText::set_rotation_origin( float x, float y )
{
	if ( x != m_rotation_origin.x || y != m_rotation_origin.y )
	{
		m_rotation_origin = sf::Vector2f( x, y );
		update_transform();
		FePresent::script_flag_redraw();
	}
}

void FeText::set_rotation_origin_type( int t )
{
	m_rotation_origin_type = static_cast<FeAlign>( t );
	sf::Vector2f o = align_type_to_vector( t );
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

void FeText::setColor( sf::Color c )
{
	if ( c == m_draw_text.getColor() )
		return;

	m_draw_text.setColor( c );
	FePresent::script_flag_redraw();
}

sf::Color FeText::getColor() const
{
	return m_draw_text.getColor();
}

void FeText::setIndexOffset( int io )
{
	if ( m_index_offset != io )
	{
		m_index_offset=io;
		FePresent::script_do_update( this );
	}
}

int FeText::getIndexOffset() const
{
	return m_index_offset;
}

void FeText::setFilterOffset( int fo )
{
	if ( m_filter_offset != fo )
	{
		m_filter_offset=fo;
		FePresent::script_do_update( this );
	}
}

int FeText::getFilterOffset() const
{
	return m_filter_offset;
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
	int char_size = 8 * m_scale_factor;
	if ( m_user_charsize > 0 )
		char_size = m_user_charsize * m_scale_factor;
	else if ( m_size.y > 12 )
		char_size = ( m_size.y - 4 ) * m_scale_factor;

	m_draw_text.setTextScale( sf::Vector2f( 1.f / m_scale_factor, 1.f / m_scale_factor ) );
	m_draw_text.setCharacterSize( char_size );
}

void FeText::update_transform()
{
	sf::Vector2f pos = m_position + sf::Vector2f(
		( m_rotation_origin.x - m_anchor.x ) * m_size.x,
		( m_rotation_origin.y - m_anchor.y ) * m_size.y
	);
	sf::Vector2f origin = sf::Vector2f(
		m_rotation_origin.x * m_size.x,
		m_rotation_origin.y * m_size.y
	);

	m_draw_text.setSize( m_size );
	m_draw_text.setOrigin( origin );
	m_draw_text.setPosition( pos );
	m_draw_text.setRotation( m_rotation );
}

void FeText::on_new_selection( FeSettings *feSettings )
{
	std::string str = m_string;

	m_magic = FePresent::script_process_magic_strings( str,
			m_filter_offset,
			m_index_offset );

	m_magic = feSettings->do_text_substitutions( str, m_filter_offset, m_index_offset ) || m_magic;

	m_draw_text.setString( str );
}

void FeText::set_scale_factor( float scale_x, float scale_y )
{
	m_scale_factor = ( scale_x > scale_y ) ? scale_x : scale_y;
	if ( m_scale_factor <= 0.f )
		m_scale_factor = 1.f;
}

void FeText::draw( sf::RenderTarget &target, sf::RenderStates states ) const
{
	FeShader *s = get_shader();
	if ( s )
	{
		const sf::Shader *sh = s->get_shader();
		if ( sh )
			states.shader = sh;
	}

	target.draw( m_draw_text, states );
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
	m_draw_text.setNoMargin( m );
	FePresent::script_do_update( this );
}

bool FeText::get_no_margin()
{
	return m_draw_text.getNoMargin();
}

void FeText::set_margin( int m )
{
	m_draw_text.setMargin( m );
	FePresent::script_do_update( this );
}

int FeText::get_margin()
{
	return m_draw_text.getMargin();
}

void FeText::set_outline( float t )
{
	m_draw_text.setOutlineThickness( t );
	FePresent::script_do_update( this );
}

float FeText::get_outline()
{
	return m_draw_text.getOutlineThickness();
}

void FeText::set_bg_outline( float t )
{
	m_draw_text.setBgOutlineThickness( t );
	FePresent::script_do_update( this );
}

float FeText::get_bg_outline()
{
	return m_draw_text.getBgOutlineThickness();
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
	return m_draw_text.setString( str, pos ).x - ( m_draw_text.getPosition().x - m_draw_text.getOrigin().x );
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

int FeText::get_charsize()
{
	if ( m_user_charsize > 0 )
		return m_user_charsize;

	return m_draw_text.getCharacterSize();
}

int FeText::get_glyph_size()
{
	return m_draw_text.getGlyphSize();
}

float FeText::get_spacing()
{
	return m_draw_text.getCharacterSpacing();
}

float FeText::get_line_spacing()
{
	return m_draw_text.getLineSpacing();
}

int FeText::get_line_height()
{
	return m_draw_text.getLineSpacingFactored( m_draw_text.getFont(), m_draw_text.getCharacterSize() );
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
	sf::Color c = m_draw_text.getBgColor();
	set_bg_rgb( r, c.g, c.b, c.a );
}

void FeText::set_bg_green(int g)
{
	sf::Color c = m_draw_text.getBgColor();
	set_bg_rgb( c.r, g, c.b, c.a );
}

void FeText::set_bg_blue(int b)
{
	sf::Color c = m_draw_text.getBgColor();
	set_bg_rgb( c.r, c.g, b, c.a );
}

void FeText::set_bg_alpha(int a)
{
	sf::Color c = m_draw_text.getBgColor();
	set_bg_rgb( c.r, c.g, c.b, a );
}

void FeText::set_bg_rgb( int r, int g, int b )
{
	sf::Color old = m_draw_text.getBgColor();
	sf::Color col = sf::Color( r, g, b, old.a );
	if ( col != old )
		set_bg_rgb( r, g, b, col.a == 0 ? 255 : col.a ); // legacy - force alpha if none
}

void FeText::set_bg_rgb( int r, int g, int b, int a )
{
	sf::Color old = m_draw_text.getBgColor();
	sf::Color col = sf::Color( r, g, b, a );
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
	sf::Color c = m_draw_text.getBgOutlineColor();
	set_bg_outline_rgb( r, c.g, c.b, c.a );
}

void FeText::set_bg_outline_green(int g)
{
	sf::Color c = m_draw_text.getBgOutlineColor();
	set_bg_outline_rgb( c.r, g, c.b, c.a );
}

void FeText::set_bg_outline_blue(int b)
{
	sf::Color c = m_draw_text.getBgOutlineColor();
	set_bg_outline_rgb( c.r, c.g, b, c.a );
}

void FeText::set_bg_outline_alpha(int a)
{
	sf::Color c = m_draw_text.getBgOutlineColor();
	set_bg_outline_rgb( c.r, c.g, c.b, a );
}

void FeText::set_bg_outline_rgb( int r, int g, int b )
{
	sf::Color old = m_draw_text.getBgOutlineColor();
	sf::Color col = sf::Color( r, g, b, old.a );
	if ( col != old )
		set_bg_outline_rgb( r, g, b, m_link_bg_outline_alpha ? -1 : ( col.a == 0 ? 255 : col.a ) ); // legacy - force alpha if none
}

void FeText::set_bg_outline_rgb( int r, int g, int b, int a )
{
	m_link_bg_outline_alpha = a == -1;
	if ( m_link_bg_outline_alpha ) a = get_bg_alpha();
	sf::Color old = m_draw_text.getBgOutlineColor();
	sf::Color col = sf::Color( r, g, b, a );
	if ( col != old )
	{
		m_draw_text.setBgOutlineColor( col );
		FePresent::script_flag_redraw();
	}
}

void FeText::set_outline_red(int r)
{
	sf::Color c = m_draw_text.getOutlineColor();
	set_outline_rgb( r, c.g, c.b, c.a );
}

void FeText::set_outline_green(int g)
{
	sf::Color c = m_draw_text.getOutlineColor();
	set_outline_rgb( c.r, g, c.b, c.a );
}

void FeText::set_outline_blue(int b)
{
	sf::Color c = m_draw_text.getOutlineColor();
	set_outline_rgb( c.r, c.g, b, c.a );
}

void FeText::set_outline_alpha(int a)
{
	sf::Color c = m_draw_text.getOutlineColor();
	set_outline_rgb( c.r, c.g, c.b, a );
}

void FeText::set_outline_rgb( int r, int g, int b )
{
	sf::Color old = m_draw_text.getOutlineColor();
	sf::Color col = sf::Color( r, g, b, old.a );
	if ( col != old )
		set_outline_rgb( r, g, b, m_link_outline_alpha ? -1 : ( col.a == 0 ? 255 : col.a ) ); // legacy - force alpha if none
}

void FeText::set_outline_rgb( int r, int g, int b, int a )
{
	m_link_outline_alpha = a == -1;
	if ( m_link_outline_alpha ) a = get_a();
	sf::Color old = m_draw_text.getOutlineColor();
	sf::Color col = sf::Color( r, g, b, a );
	if ( col != old )
	{
		m_draw_text.setOutlineColor( col );
		FePresent::script_flag_redraw();
	}
}

void FeText::set_charsize(int s)
{
	if ( s != m_user_charsize )
	{
		m_user_charsize = s;
		update_font_size();
		FePresent::script_do_update( this );
	}
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
		FePresent::script_flag_redraw();
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

	const sf::Font *font=&(fc->get_font());
	if ( font )
	{
		setFont( *font );
		m_font_name = f;

		FePresent::script_do_update( this );
	}
}
