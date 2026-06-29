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

#include "fe_listbox.hpp"
#include "fe_settings.hpp"
#include "fe_shader.hpp"
#include "fe_present.hpp"
#include "fe_util.hpp"
#include <iostream>
#include <algorithm>

const std::string DEFAULT_FORMAT_STRING = "[Title]";

FeListBox::FeListBox( FePresentableParent &p, int x, int y, int w, int h )
	: FeBasePresentable( p ),
	m_selColour( sf::Color::Yellow ),
	m_selBg( sf::Color::Blue ),
	m_selOutlineColour( sf::Color::Black ),
	m_position( x, y ),
	m_size( w, h ),
	m_transform_origin( 0.f, 0.f ),
	m_anchor( 0.f, 0.f ),
	m_rotation_origin( 0.f, 0.f ),
	m_transform_origin_type( FeAlign::TopLeft ),
	m_anchor_type( FeAlign::TopLeft ),
	m_rotation_origin_type( FeAlign::TopLeft ),
	m_selOutlineThickness( 0 ),
	m_selStyle( sf::Text::Regular ),
	m_rows( 11 ),
	m_list_align( FeAlign::Top ),
	m_userCharSize( 0 ),
	m_filter_offset( 0 ),
	m_rotation( 0.0 ),
	m_scale_factor( 1.0 ),
	m_scripted( true ),
	m_mode( Static ),
	m_selected_row( -1 ),
	m_list_start_offset( 0 ),
	m_selection_margin( 0 ),
	m_custom_sel( -1 ),
	m_has_custom_list( false )
{
	m_base_text.setPosition( m_position );
	m_base_text.setSize( m_size );
	m_base_text.setColor( sf::Color::White );
	m_base_text.setBgColor( sf::Color::Transparent );
}

FeListBox::FeListBox(
		FePresentableParent &p,
		const sf::Font *font,
		sf::Color colour,
		sf::Color bgcolour,
		sf::Color selcolour,
		sf::Color selbgcolour,
		unsigned int charactersize,
		int rows )
	: FeBasePresentable( p ),
	m_base_text( font, colour, bgcolour, charactersize, FeAlign::Centre ),
	m_selColour( selcolour ),
	m_selBg( selbgcolour ),
	m_selOutlineColour( sf::Color::Black ),
	m_position( 0.f, 0.f ),
	m_size( 0.f, 0.f ),
	m_transform_origin( 0.f, 0.f ),
	m_anchor( 0.f, 0.f ),
	m_rotation_origin( 0.f, 0.f ),
	m_transform_origin_type( FeAlign::TopLeft ),
	m_anchor_type( FeAlign::TopLeft ),
	m_rotation_origin_type( FeAlign::TopLeft ),
	m_selOutlineThickness( 0 ),
	m_selStyle( sf::Text::Regular ),
	m_rows( rows ),
	m_list_align( FeAlign::Top ),
	m_userCharSize( charactersize ),
	m_filter_offset( 0 ),
	m_rotation( 0.0 ),
	m_scale_factor( 1.0 ),
	m_scripted( false ),
	m_mode( Static ),
	m_selected_row( -1 ),
	m_list_start_offset( 0 ),
	m_selection_margin( 0 ),
	m_custom_sel( -1 )
{
}

FeListBox::FeListBox( FePresentableParent &p )
	: FeListBox( p, 0, 0, 0, 0 )
{
    m_scripted = false;
}

void FeListBox::setFont( const sf::Font &f )
{
	m_base_text.setFont( f );
}

sf::Vector2f FeListBox::getPosition() const
{
	return m_position;
}

void FeListBox::setPosition( const sf::Vector2f &p )
{
	if ( p == m_position )
		return;

	m_position = p;
	update_row_geometry();

	if ( m_scripted )
		FePresent::script_do_update( this );
}

sf::Vector2f FeListBox::getSize() const
{
	return m_size;
}

void FeListBox::setSize( const sf::Vector2f &s )
{
	if ( s == m_size )
		return;

	m_size = s;
	update_row_geometry();

	if ( m_scripted )
		FePresent::script_do_update( this );
}

float FeListBox::getRotation() const
{
	return m_rotation;
}

sf::Color FeListBox::getColor() const
{
	return m_base_text.getColor();
}

int FeListBox::get_transform_origin_type() const
{
	return m_transform_origin_type;
}

int FeListBox::get_anchor_type() const
{
	return m_anchor_type;
}

int FeListBox::get_rotation_origin_type() const
{
	return m_rotation_origin_type;
}

float FeListBox::get_transform_origin_x() const
{
	return m_transform_origin.x;
}

float FeListBox::get_transform_origin_y() const
{
	return m_transform_origin.y;
}

float FeListBox::get_anchor_x() const
{
	return m_anchor.x;
}

float FeListBox::get_anchor_y() const
{
	return m_anchor.y;
}

float FeListBox::get_rotation_origin_x() const
{
	return m_rotation_origin.x;
}

float FeListBox::get_rotation_origin_y() const
{
	return m_rotation_origin.y;
}

void FeListBox::set_transform_origin( float x, float y )
{
	if (
		x != m_transform_origin.x || x != m_anchor.x || x != m_rotation_origin.x ||
		y != m_transform_origin.y || y != m_anchor.y || y != m_rotation_origin.y
	)
	{
		m_transform_origin = sf::Vector2f( x, y );
		m_anchor = sf::Vector2f( x, y );
		m_rotation_origin = sf::Vector2f( x, y );
		update_row_geometry();
		FePresent::script_flag_redraw();
	}
}

void FeListBox::set_transform_origin_type( int t )
{
	m_transform_origin_type = static_cast<FeAlign>( t );
	sf::Vector2f a = align_type_to_vector( t );
	set_transform_origin( a.x, a.y );
}

void FeListBox::set_anchor( float x, float y )
{
	if ( x != m_anchor.x || y != m_anchor.y )
	{
		m_anchor = sf::Vector2f( x, y );
		update_row_geometry();
		FePresent::script_flag_redraw();
	}
}

void FeListBox::set_anchor_type( int t )
{
	m_anchor_type = static_cast<FeAlign>( t );
	sf::Vector2f a = align_type_to_vector( t );
	set_anchor( a.x, a.y );
}

void FeListBox::set_rotation_origin( float x, float y )
{
	if ( x != m_rotation_origin.x || y != m_rotation_origin.y )
	{
		m_rotation_origin = sf::Vector2f( x, y );
		update_row_geometry();
		FePresent::script_flag_redraw();
	}
}

void FeListBox::set_rotation_origin_type( int t )
{
	m_rotation_origin_type = static_cast<FeAlign>( t );
	sf::Vector2f o = align_type_to_vector( t );
	set_rotation_origin( o.x, o.y );
}

void FeListBox::set_transform_origin_x( float x )
{
	if ( x != m_transform_origin.x || x != m_anchor.x || x != m_rotation_origin.x )
	{
		m_transform_origin.x = x;
		m_anchor.x = x;
		m_rotation_origin.x = x;
		update_row_geometry();
		FePresent::script_flag_redraw();
	}
}

void FeListBox::set_transform_origin_y( float y )
{
	if ( y != m_transform_origin.y || y != m_anchor.y || y != m_rotation_origin.y )
	{
		m_transform_origin.y = y;
		m_anchor.y = y;
		m_rotation_origin.y = y;
		update_row_geometry();
		FePresent::script_flag_redraw();
	}
}

void FeListBox::set_anchor_x( float x )
{
	set_anchor( x, get_anchor_y() );
}

void FeListBox::set_anchor_y( float y )
{
	set_anchor( get_anchor_x(), y );
}

void FeListBox::set_rotation_origin_x( float x )
{
	set_rotation_origin( x, get_rotation_origin_y() );
}

void FeListBox::set_rotation_origin_y( float y )
{
	set_rotation_origin( get_rotation_origin_x(), y );
}

void FeListBox::set_outline( float t )
{
	if ( t == m_base_text.getOutlineThickness() )
		return;

	m_base_text.setOutlineThickness( t );

	if ( m_scripted )
		FePresent::script_do_update( this );
}

float FeListBox::get_outline()
{
	return m_base_text.getOutlineThickness();
}

void FeListBox::set_outline_red(int r)
{
	sf::Color c = getOutlineColor();
	set_outline_rgb( r, c.g, c.b, c.a );
}

void FeListBox::set_outline_green(int g)
{
	sf::Color c = getOutlineColor();
	set_outline_rgb( c.r, g, c.b, c.a );
}

void FeListBox::set_outline_blue(int b)
{
	sf::Color c = getOutlineColor();
	set_outline_rgb( c.r, c.g, b, c.a );
}

void FeListBox::set_outline_alpha(int a)
{
	sf::Color c = getOutlineColor();
	set_outline_rgb( c.r, c.g, c.b, a );
}

void FeListBox::set_outline_rgb( int r, int g, int b )
{
	float a = getOutlineColor().a;
	set_outline_rgb( r, g, b, a == 0 ? 255 : a ); // legacy - force alpha if none
}

void FeListBox::set_outline_rgb( int r, int g, int b, int a )
{
	setOutlineColor(sf::Color( r, g, b, a ));
}

sf::Color FeListBox::getOutlineColor() const
{
	return m_base_text.getOutlineColor();
}

void FeListBox::setOutlineColor( sf::Color c )
{
	if ( c == getOutlineColor() )
		return;

	m_base_text.setOutlineColor( c );
	FePresent::script_flag_redraw();
}

void FeListBox::set_sel_outline( float t )
{
	if ( t == m_selOutlineThickness )
		return;

	m_selOutlineThickness = t;
	FeTextPrimitive *sel;
	if ( getSelectedText( sel ) ) sel->setOutlineThickness( m_selOutlineThickness );

	if ( m_scripted )
		FePresent::script_do_update( this );
}

float FeListBox::get_sel_outline()
{
	return m_selOutlineThickness;
}

void FeListBox::set_sel_outline_red(int r)
{
	sf::Color c = getSelOutlineColor();
	set_sel_outline_rgb( r, c.g, c.b, c.a );
}

void FeListBox::set_sel_outline_green(int g)
{
	sf::Color c = getSelOutlineColor();
	set_sel_outline_rgb( c.r, g, c.b, c.a );
}

void FeListBox::set_sel_outline_blue(int b)
{
	sf::Color c = getSelOutlineColor();
	set_sel_outline_rgb( c.r, c.g, b, c.a );
}

void FeListBox::set_sel_outline_alpha(int a)
{
	sf::Color c = getSelOutlineColor();
	set_sel_outline_rgb( c.r, c.g, c.b, a );
}

void FeListBox::set_sel_outline_rgb( int r, int g, int b )
{
	float a = getSelOutlineColor().a;
	set_sel_outline_rgb( r, g, b, a == 0 ? 255 : a ); // legacy - force alpha if none
}

void FeListBox::set_sel_outline_rgb( int r, int g, int b, int a )
{
	setSelOutlineColor(sf::Color( r, g, b, a ));
}

sf::Color FeListBox::getSelOutlineColor() const
{
	return m_selOutlineColour;
}

void FeListBox::setSelOutlineColor( sf::Color c )
{
	if ( c == m_selOutlineColour )
		return;

	m_selOutlineColour = c;
	FeTextPrimitive *sel;
	if ( getSelectedText( sel ) )
		sel->setOutlineColor( m_selOutlineColour );

	if ( m_scripted )
		FePresent::script_flag_redraw();
}

void FeListBox::update_row_geometry()
{
	int row_count = getRowCount();
	if ( row_count <= 0 )
		return;

	sf::Vector2f size = getSize();
	sf::Vector2f top_left = m_position - sf::Vector2f( m_anchor.x * size.x, m_anchor.y * size.y );
	sf::Vector2f pivot = m_position + sf::Vector2f(
		( m_rotation_origin.x - m_anchor.x ) * size.x,
		( m_rotation_origin.y - m_anchor.y ) * size.y
	);
	int actual_spacing = (int)size.y / row_count;

	m_base_text.setPosition( top_left );
	m_base_text.setSize( size );
	m_base_text.setOrigin( sf::Vector2f( 0.f, 0.f ));

	sf::Transform rotater;
	rotater.rotate( sf::degrees( m_rotation ), pivot );

	for ( int i=0; i< row_count; i++ )
	{
		m_texts[i].setOrigin( sf::Vector2f( 0.f, 0.f ));
		m_texts[i].setPosition( rotater.transformPoint({ top_left.x, top_left.y + ( i * actual_spacing )}));
		m_texts[i].setSize( size.x, actual_spacing );
		m_texts[i].setRotation( m_rotation );
	}
}

void FeListBox::init_dimensions()
{
	sf::Vector2f size = getSize();

	int actual_spacing = (int)size.y / m_rows;
	int char_size = ( m_userCharSize > 0 ) ? m_userCharSize
		: ( actual_spacing > 12 ) ? actual_spacing - 4
		: 8;

	m_base_text.setTextScale( sf::Vector2f( 1.0, 1.0 ) / m_scale_factor );
	m_base_text.setCharacterSize( char_size * m_scale_factor );

	// Add or remove text elements to match row count
	while ( getRowCount() < m_rows )
		m_texts.push_back( FeTextPrimitive() );
	while ( getRowCount() > m_rows )
		m_texts.pop_back();

	// Re-position text elements
	for ( int i=0; i< m_rows; i++ )
		m_texts[i].setFrom( m_base_text );

	update_row_geometry();

	update_styles();
}

void FeListBox::update_styles()
{
	sf::Color color = m_base_text.getColor();
	sf::Color bgColor = m_base_text.getBgColor();
	sf::Color outlineColour = m_base_text.getOutlineColor();
	float outlineThickness = m_base_text.getOutlineThickness();
	int style = m_base_text.getStyle();

	for ( int i=0; i< m_rows; i++ )
	{
		if ( i == m_selected_row )
		{
			m_texts[i].setColor( m_selColour );
			m_texts[i].setBgColor( m_selBg );
			m_texts[i].setOutlineColor( m_selOutlineColour );
			m_texts[i].setOutlineThickness( m_selOutlineThickness );
			m_texts[i].setStyle( m_selStyle );
			continue;
		}

		m_texts[i].setColor( color );
		m_texts[i].setBgColor( bgColor );
		m_texts[i].setOutlineColor( outlineColour );
		m_texts[i].setOutlineThickness( outlineThickness );
		m_texts[i].setStyle( style );
	}
}

void FeListBox::setColor( sf::Color c )
{
	if ( c == m_base_text.getColor() )
		return;

	m_base_text.setColor( c );

	for ( int i=0; i < getRowCount(); i++ )
		if ( i != m_selected_row )
			m_texts[i].setColor( c );

	if ( m_scripted )
		FePresent::script_flag_redraw();
}

void FeListBox::setSelColor( sf::Color c )
{
	if ( c == m_selColour )
		return;

	m_selColour = c;
	FeTextPrimitive *sel;
	if ( getSelectedText( sel ) ) sel->setColor( m_selColour );

	if ( m_scripted )
		FePresent::script_flag_redraw();
}

void FeListBox::setSelBgColor( sf::Color c )
{
	if ( c == m_selBg )
		return;

	m_selBg = c;
	FeTextPrimitive *sel;
	if ( getSelectedText( sel ) ) sel->setBgColor( m_selBg );

	if ( m_scripted )
		FePresent::script_flag_redraw();
}

void FeListBox::setSelStyle( int s )
{
	if ( s == m_selStyle )
		return;

	m_selStyle = s;
	FeTextPrimitive *sel;
	if ( getSelectedText( sel ) ) sel->setStyle( m_selStyle );

	if ( m_scripted )
		FePresent::script_flag_redraw();
}

int FeListBox::getSelStyle()
{
	return m_selStyle;
}

void FeListBox::setTextScale( const sf::Vector2f &scale )
{
	m_base_text.setTextScale( scale );

	for ( int i=0; i < getRowCount(); i++ )
		m_texts[i].setTextScale( scale );
}

bool FeListBox::getSelectedText( FeTextPrimitive* &sel )
{
	int n = getRowCount();
	if (( n == 0 ) || ( m_selected_row < 0 ) || ( m_selected_row >= n ))
		return false;

	sel = &m_texts[ m_selected_row ];
	return true;
}

//
// Set the selection index of a custom list
//
void FeListBox::setCustomSelection( const int index )
{
	m_custom_sel = index;
	refresh_selection();
}

//
// Set a custom list
// - List may be empty if user provides empty list, or no display filters
//
void FeListBox::setCustomText( const int index, const std::vector<std::string> &list )
{
	m_has_custom_list = true;
	m_custom_list = list;
	m_custom_sel = list.empty() ? -1 : index;
	refresh_list();
}

//
// Remove a custom list (will revert to the romlist)
// - Caller must call `on_new_list` afterward to re-establish the non-custom listbox
//
void FeListBox::removeCustomText()
{
	m_has_custom_list = false;
	m_custom_list = std::vector<std::string>();
	m_custom_sel = -1;
}

void FeListBox::refresh_selection()
{
	int display_rows = getRowCount();
	if ( display_rows == 0 )
		return;

	// List size may be zero, but the display rows still need updating to clear them
	int list_size = get_list_size();
	int sel = get_list_sel();

	switch ( m_mode )
	{
		case Moving:
		case Bounded:
		{
			int margin = get_selection_margin();
			int window_start = m_list_start_offset;
			int window_end = window_start + display_rows - 1;
			bool is_bounded = m_mode == Bounded;

			if ( list_size > display_rows || is_bounded )
			{
				if ( sel < window_start + margin )
				{
					// Scroll the list up
					m_list_start_offset = sel - margin;
					if ( !is_bounded ) m_list_start_offset = std::max( 0, m_list_start_offset );
				}
				else if ( sel > window_end - margin )
				{
					// If the margin is *exactly* half of the display_rows,
					// adjust the margin to match the scroll up behavior for symmetry
					bool is_margin_middle = ( display_rows % 2 == 0 ) && ( margin == display_rows / 2 );
					int down_margin = is_margin_middle ? margin : display_rows - 1 - margin;

					// Scroll the list down
					m_list_start_offset = sel - down_margin;
					if ( !is_bounded ) m_list_start_offset = std::min( list_size - display_rows, m_list_start_offset );
				}
			}

			m_selected_row = sel - m_list_start_offset;
			break;
		}

		case Paged:
		{
			int margin = get_selection_margin();
			int page_size = std::max( 1, display_rows - margin * 2 );
			int page = sel / page_size;

			m_list_start_offset = page * page_size - margin;
			m_selected_row = (sel % page_size) + margin;
			break;
		}

		case Static:
		default: // Fallback to Static mode
			m_list_start_offset = sel - m_selected_row;
			break;
	}

	std::string text_string;
	std::string format_string = m_format_string.empty()
		? DEFAULT_FORMAT_STRING
		: m_format_string;

	for ( int i=0; i < display_rows; i++ )
	{
		int listentry = i + m_list_start_offset;
		if ( listentry < 0 || listentry >= list_size )
			text_string = "";
		else if ( m_has_custom_list )
			text_string = m_custom_list[ listentry ];
		else
		{
			// Substitute magic string on-demand
			text_string = format_string;
			FePresent::script_process_magic_strings(
				text_string,
				m_filter_offset,
				i - m_selected_row
			);
			m_feSettings->do_text_substitutions_absolute(
				text_string,
				m_display_filter_index,
				listentry
			);
		}

		m_texts[i].setString( text_string );
	}

	update_styles();
}

void FeListBox::setRotation( float r )
{
	if ( r == m_rotation )
		return;

	m_rotation = r;

	update_row_geometry();

	if ( m_scripted )
		FePresent::script_flag_redraw();
}

void FeListBox::update_list_settings( FeSettings *s )
{
	m_feSettings = s;
	if ( !m_has_custom_list )
	{
		m_display_filter_index = s->get_filter_index_from_offset( m_filter_offset );
		m_display_filter_size = s->get_filter_size( m_display_filter_index );
		m_display_rom_index = s->get_rom_index( m_display_filter_index, 0 );
	}
}

void FeListBox::refresh_list()
{
	init_dimensions();

	int sel = get_list_sel();
	int size = get_list_size();
	int rows = getRowCount();

	// Default m_selected_row to middle row
	if ( m_selected_row == -1 )
		m_selected_row = rows / 2;

	switch ( m_mode )
	{
		case Moving:
		case Bounded:
		{
			// Maintain m_selected_row on list change (if possible)
			int goal_sel_row = m_selected_row;
			bool is_bounded = m_mode == Bounded;

			// When using row alignment, m_selected_row will be changed to align the list
			int empty_rows = rows - size;
			if ( empty_rows > 0 )
			{
				switch ( m_list_align )
				{
					case FeAlign::Top:
						goal_sel_row = sel;
						break;
					case FeAlign::Centre:
						goal_sel_row = sel + empty_rows / 2;
						break;
					case FeAlign::Bottom:
						goal_sel_row = sel + empty_rows;
						break;
					default:
						break;
				}
			}

			int margin = is_bounded ? get_selection_margin() : 0;
			int ma = -margin;
			int mb = size - rows + margin;

			m_list_start_offset = std::clamp( sel - goal_sel_row, std::min(ma, mb), std::max(ma, mb));
			m_selected_row = sel - m_list_start_offset;
			break;
		}
	}

	refresh_selection();
}

void FeListBox::on_new_list( FeSettings *s )
{
	update_list_settings( s );
	refresh_list();
}

void FeListBox::on_new_selection( FeSettings *s )
{
	update_list_settings( s );
	refresh_selection();
}

void FeListBox::set_scale_factor( float scale_x, float scale_y )
{
	m_scale_factor = ( scale_x > scale_y ) ? scale_x : scale_y;
	if ( m_scale_factor <= 0.f )
		m_scale_factor = 1.f;
}

void FeListBox::draw( sf::RenderTarget &target, sf::RenderStates states ) const
{
	FeShader *s = get_shader();
	if ( s )
	{
		const sf::Shader *sh = s->get_shader();
		if ( sh )
			states.shader = sh;
	}

	for ( std::vector<FeTextPrimitive>::const_iterator itl=m_texts.begin();
				itl != m_texts.end(); ++itl )
		target.draw( (*itl), states );
}

void FeListBox::clear()
{
	m_texts.clear();
}

int FeListBox::getRowCount() const
{
	return m_texts.size();
}

void FeListBox::setIndexOffset( int io )
{
}

int FeListBox::getIndexOffset() const
{
	return 0;
}

void FeListBox::setFilterOffset( int fo )
{
	if (fo == m_filter_offset)
		return;

	m_filter_offset = fo;

	if ( m_scripted )
		FePresent::script_do_update( this );
}

int FeListBox::getFilterOffset() const
{
	return m_filter_offset;
}

int FeListBox::get_bg_red()
{
	return m_base_text.getBgColor().r;
}

int FeListBox::get_bg_green()
{
	return m_base_text.getBgColor().g;
}

int FeListBox::get_bg_blue()
{
	return m_base_text.getBgColor().b;
}

int FeListBox::get_bg_alpha()
{
	return m_base_text.getBgColor().a;
}

int FeListBox::get_outline_red()
{
	return getOutlineColor().r;
}

int FeListBox::get_outline_green()
{
	return getOutlineColor().g;
}

int FeListBox::get_outline_blue()
{
	return getOutlineColor().b;
}

int FeListBox::get_outline_alpha()
{
	return getOutlineColor().a;
}

int FeListBox::get_sel_outline_red()
{
	return getSelOutlineColor().r;
}

int FeListBox::get_sel_outline_green()
{
	return getSelOutlineColor().g;
}

int FeListBox::get_sel_outline_blue()
{
	return getSelOutlineColor().b;
}

int FeListBox::get_sel_outline_alpha()
{
	return getSelOutlineColor().a;
}

int FeListBox::get_charsize()
{
	return m_userCharSize;
}

int FeListBox::get_glyph_size()
{
	return m_base_text.getGlyphSize();
}

float FeListBox::get_spacing()
{
	return m_base_text.getCharacterSpacing();
}

int FeListBox::get_rows()
{
	return m_rows;
}

int FeListBox::get_list_align()
{
	return m_list_align;
}

int FeListBox::get_list_sel()
{
	return m_has_custom_list
		? m_custom_sel
		: m_display_rom_index;
}

int FeListBox::get_list_size()
{
	return m_has_custom_list
		? m_custom_list.size()
		: m_display_filter_size;
}

int FeListBox::get_style()
{
	return m_base_text.getStyle();
}

int FeListBox::get_justify()
{
	return m_base_text.getJustify();
}

int FeListBox::get_align()
{
	return m_base_text.getAlignment();
}

int FeListBox::get_case()
{
	return (int)m_base_text.getCase();
}

int FeListBox::get_type() const
{
	return FePresentableTypeListbox;
}

void FeListBox::set_no_margin( bool m )
{
	if ( m == m_base_text.getNoMargin() )
		return;

	m_base_text.setNoMargin( m );

	if ( m_scripted )
		FePresent::script_do_update( this );
}

bool FeListBox::get_no_margin()
{
	return m_base_text.getNoMargin();
}

void FeListBox::set_margin( int m )
{
	if ( m == m_base_text.getMargin() )
		return;

	m_base_text.setMargin( m );

	if ( m_scripted )
		FePresent::script_do_update( this );
}

int FeListBox::get_margin()
{
	return m_base_text.getMargin();
}

void FeListBox::setBgColor( sf::Color c )
{
	if ( c == m_base_text.getBgColor() )
		return;

	m_base_text.setBgColor(c);
	for ( int i=0; i < getRowCount(); i++ )
		if ( i != m_selected_row )
			m_texts[i].setBgColor( c );

	if ( m_scripted )
		FePresent::script_flag_redraw();
}

void FeListBox::set_bg_red(int r)
{
	sf::Color c = m_base_text.getBgColor();
	set_bg_rgb( r, c.g, c.b, c.a );
}

void FeListBox::set_bg_green(int g)
{
	sf::Color c = m_base_text.getBgColor();
	set_bg_rgb( c.r, g, c.b, c.a );
}

void FeListBox::set_bg_blue(int b)
{
	sf::Color c = m_base_text.getBgColor();
	set_bg_rgb( c.r, c.g, b, c.a );
}

void FeListBox::set_bg_alpha(int a)
{
	sf::Color c = m_base_text.getBgColor();
	set_bg_rgb( c.r, c.g, c.b, a );
}

void FeListBox::set_bg_rgb( int r, int g, int b )
{
	sf::Color c = m_base_text.getBgColor();
	set_bg_rgb( r, g, b, c.a );
}

void FeListBox::set_bg_rgb( int r, int g, int b, int a )
{
	setBgColor(sf::Color( r, g, b, a ));
}

void FeListBox::set_charsize(int s)
{
	if ( s == m_userCharSize )
		return;

	m_userCharSize = s;

	if ( m_scripted )
		FePresent::script_do_update( this );
}

void FeListBox::set_spacing(float s)
{
	if ( s == m_base_text.getCharacterSpacing() )
		return;

	m_base_text.setCharacterSpacing(s);

	if ( m_scripted )
		FePresent::script_do_update( this );
}

void FeListBox::set_rows(int r)
{
	r = std::max( 1, r );
	if ( r == m_rows )
		return;

	m_rows = r;

	// To maintain legacy behavior, trigger m_selected_row re-calc on static row changes
	if ( m_mode == Static )
		m_selected_row = -1;

	if ( m_scripted )
		FePresent::script_do_update( this );
}

void FeListBox::set_list_align(int a)
{
	FeAlign align = static_cast<FeAlign>( a );

	if ( align == m_list_align )
		return;

	m_list_align = align;

	// Trigger m_selected_row re-calc, which aligns the list
	if ( align != FeAlign::None )
		m_selected_row = -1;

	if ( m_scripted )
		FePresent::script_do_update( this );
}

void FeListBox::set_style(int s)
{
	if ( s == m_base_text.getStyle() )
		return;

	m_base_text.setStyle(s);
	for ( int i=0; i < getRowCount(); i++ )
		if ( i != m_selected_row )
			m_texts[i].setStyle( s );

	if ( m_scripted )
		FePresent::script_flag_redraw();
}

void FeListBox::set_justify(int j)
{
	if ( j == m_base_text.getJustify() )
		return;

	m_base_text.setJustify(j);
	for ( int i=0; i < getRowCount(); i++ )
		m_texts[i].setJustify( j );

	if ( m_scripted )
		FePresent::script_flag_redraw();
}

void FeListBox::set_align(int a)
{
	FeAlign align = static_cast<FeAlign>( a );
	if ( align == m_base_text.getAlignment() )
		return;

	m_base_text.setAlignment( align );

	for ( int i=0; i < getRowCount(); i++ )
		m_texts[i].setAlignment( align );

	if ( m_scripted )
		FePresent::script_flag_redraw();
}

void FeListBox::set_case(int c)
{
	if ( c == m_base_text.getCase() )
		return;

	m_base_text.setCase( (FeTextPrimitive::Case)c );

	for ( int i=0; i < getRowCount(); i++ )
		m_texts[i].setCase( (FeTextPrimitive::Case)c );

	if ( m_scripted )
		FePresent::script_flag_redraw();
}

int FeListBox::get_sel_red()
{
	return m_selColour.r;
}

int FeListBox::get_sel_green()
{
	return m_selColour.g;
}

int FeListBox::get_sel_blue()
{
	return m_selColour.b;
}

int FeListBox::get_sel_alpha()
{
	return m_selColour.a;
}

void FeListBox::set_sel_red(int r)
{
	sf::Color c = m_selColour;
	set_sel_rgb( r, c.g, c.b, c.a );
}

void FeListBox::set_sel_green(int g)
{
	sf::Color c = m_selColour;
	set_sel_rgb( c.r, g, c.b, c.a );
}

void FeListBox::set_sel_blue(int b)
{
	sf::Color c = m_selColour;
	set_sel_rgb( c.r, c.g, b, c.a );
}

void FeListBox::set_sel_alpha(int a)
{
	sf::Color c = m_selColour;
	set_sel_rgb( c.r, c.g, c.b, a );
}

void FeListBox::set_sel_rgb( int r, int g, int b )
{
	set_sel_rgb( r, g, b, m_selColour.a );
}

void FeListBox::set_sel_rgb( int r, int g, int b, int a )
{
	setSelColor(sf::Color( r, g, b, a ));
}

int FeListBox::get_sel_bg_red()
{
	return m_selBg.r;
}

int FeListBox::get_sel_bg_green()
{
	return m_selBg.g;
}

int FeListBox::get_sel_bg_blue()
{
	return m_selBg.b;
}

int FeListBox::get_sel_bg_alpha()
{
	return m_selBg.a;
}

const char *FeListBox::get_font()
{
	return m_font_name.c_str();
}

void FeListBox::set_sel_bg_red(int r)
{
	sf::Color c = m_selBg;
	set_sel_bg_rgb( r, c.g, c.b, c.a );
}

void FeListBox::set_sel_bg_green(int g)
{
	sf::Color c = m_selBg;
	set_sel_bg_rgb( c.r, g, c.b, c.a );
}

void FeListBox::set_sel_bg_blue(int b)
{
	sf::Color c = m_selBg;
	set_sel_bg_rgb( c.r, c.g, b, c.a );
}

void FeListBox::set_sel_bg_alpha(int a)
{
	sf::Color c = m_selBg;
	set_sel_bg_rgb( c.r, c.g, c.b, a );
}

void FeListBox::set_sel_bg_rgb( int r, int g, int b )
{
	set_sel_bg_rgb( r, g, b, m_selBg.a );
}

void FeListBox::set_sel_bg_rgb( int r, int g, int b, int a )
{
	setSelBgColor(sf::Color( r, g, b, a ));
}

void FeListBox::set_font( const char *f )
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

		if ( m_scripted )
			FePresent::script_do_update( this );
	}
}

const char *FeListBox::get_format_string()
{
	return m_format_string.c_str();
}

void FeListBox::set_format_string( const char *s )
{
	if ( s == m_format_string )
		return;

	m_format_string = s;

	if ( m_scripted )
		FePresent::script_do_update( this );
}

int FeListBox::get_selection_mode()
{
	return m_mode;
}

void FeListBox::set_selection_mode(int m)
{
	if ( m == m_mode )
		return;

	m_mode = m;
	if ( m_scripted )
		FePresent::script_do_update( this );
}

int FeListBox::get_selection_margin()
{
	return std::min( m_selection_margin, getRowCount() / 2 );
}

void FeListBox::set_selection_margin(int m)
{
	m = std::max( 0, m );
	if ( m == m_selection_margin )
		return;

	m_selection_margin = m;

	if ( m_scripted )
		FePresent::script_do_update( this );
}

int FeListBox::get_selected_row() const
{
	return m_selected_row;
}

void FeListBox::set_selected_row( int r )
{
	r = std::max( 0, r );
	if ( r == m_selected_row )
		return;

	m_selected_row = r;

	if ( m_scripted )
		FePresent::script_do_update( this );
}
