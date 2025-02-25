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

#include "tp.hpp"
#include <iostream>
#include <cmath>

#include "fe_present.hpp"

FeTextPrimitive::FeTextPrimitive( )
	: m_texts( 1, sf::Text( *FePresent::script_get_fep()->get_default_font() )),
	m_align( Centre ),
	m_first_line( 1 ),
	m_lines( 1 ),
	m_lines_total( 1 ),
	m_margin( -1 ),
	m_outline( 0.0 ),
	m_line_spacing( 1.0 ),
	m_word_wrap( false ),
	m_needs_pos_set( false )
{
	setColor( sf::Color::White );
	setBgOutlineColor( sf::Color::Black );
	setBgColor( sf::Color::Transparent );
}

FeTextPrimitive::FeTextPrimitive(
			const sf::Font *font,
			sf::Color colour,
			sf::Color bgcolour,
			unsigned int charactersize,
			Alignment align )
	: m_texts( 1, sf::Text( *font )),
	m_align( align ),
	m_first_line( 1 ),
	m_lines( 1 ),
	m_lines_total( 1 ),
	m_margin( -1 ),
	m_outline( 0.0 ),
	m_line_spacing( 1.0 ),
	m_word_wrap( false ),
	m_needs_pos_set( false )
{
	setColor( colour );
	setBgColor( bgcolour );
	setCharacterSize( charactersize );
}

FeTextPrimitive::FeTextPrimitive( const FeTextPrimitive &c )
	: m_bgRect( c.m_bgRect ),
	m_texts( c.m_texts ),
	m_align( c.m_align ),
	m_first_line( c.m_first_line ),
	m_lines( c.m_lines ),
	m_lines_total( c.m_lines_total ),
	m_margin( c.m_margin ),
	m_outline( c.m_outline ),
	m_line_spacing( c.m_line_spacing ),
	m_word_wrap( c.m_word_wrap ),
	m_needs_pos_set( c.m_needs_pos_set )
{
}

void FeTextPrimitive::setColor( sf::Color c )
{
	for ( unsigned int i=0; i < m_texts.size(); i++ )
		m_texts[i].setFillColor( c );
}

sf::Color FeTextPrimitive::getColor() const
{
	return m_texts[0].getFillColor();
}

void FeTextPrimitive::setBgColor( sf::Color c )
{
	m_bgRect.setFillColor( c );
}

sf::Color FeTextPrimitive::getBgColor() const
{
	return m_bgRect.getFillColor();
}

void FeTextPrimitive::setOutlineColor( sf::Color c )
{
	for ( unsigned int i=0; i < m_texts.size(); i++ )
		m_texts[i].setOutlineColor( c );
}

sf::Color FeTextPrimitive::getOutlineColor() const
{
	return m_texts[0].getOutlineColor();
}

void FeTextPrimitive::setBgOutlineColor( sf::Color c )
{
	m_bgRect.setOutlineColor( c );
}

sf::Color FeTextPrimitive::getBgOutlineColor() const
{
	return m_bgRect.getOutlineColor();
}

void FeTextPrimitive::fit_string(
			const std::basic_string<std::uint32_t> &s,
			int &position,
			int &first_char,
			int &last_char )
{
	// There is a special case where we edit the string in UI
	// We do not cut the trailing space in this situation
	//
	bool edit_mode( false );
	if (( position >= 0 ) && !m_word_wrap )
		edit_mode = true;

	if ( position < 0 )
		position = 0;

	// start from "position", "i" measures to right, "j" to the left
	int i( position );
	int j( position );
	int last_space( 0 );
	first_char = position;

	const sf::Font *font = getFont();
	unsigned int charsize = m_texts[0].getCharacterSize();
	unsigned int spacing = charsize;
	float width = m_bgRect.getSize().x / m_texts[0].getScale().x;

	int running_total( 0 );
	int running_width( 0 );
	int kerning( 0 );

	const sf::Glyph *g = &font->getGlyph( s[i], charsize, m_texts[0].getStyle() & sf::Text::Bold );

	if ( font->getLineSpacing( spacing ) > spacing )
		spacing = font->getLineSpacing( spacing );

	if ( m_margin < 0 )
		width -= spacing;
	else
		width -= m_margin * 2.0;

	if ( !edit_mode )
		running_total = -g->bounds.position.x;

	// We need to adjust the width to make sure that at least one character is displayed per line
	width = std::max( width, (float)g->bounds.size.x );

	while (( running_width <= width ) && (( i < (int)s.size() ) || ( !m_word_wrap && ( j > 0 ))))
	{
		if ( i < (int)s.size() )
		{
			if ( i > first_char )
			{
				kerning = font->getKerning( s[std::max( 0, i - 1 )], s[i], charsize );
				running_total += kerning;
			}

			g = &font->getGlyph( s[i], charsize, m_texts[0].getStyle() & sf::Text::Bold );
			running_width = std::max( running_width, (int)( running_total + g->bounds.position.x + g->bounds.size.x ));
			running_total += g->advance;

			if ( s[i] == L' ' )
				last_space = i;

			if ( s[i] == L'\n' )
			{
				// If we encounter a newline character, we break the string there
				//
				last_char = i - 1;
				i++;
				position = i;
				return;
			}

			i++;
		}

		if ( !m_word_wrap && ( j > 0 ) && ( running_width <= width ))
		{
			j--;
			kerning = font->getKerning( s[j], s[std::min( j + 1, (int)s.size() - 1 )], charsize );
			running_total += kerning;
			g = &font->getGlyph( s[j], charsize, m_texts[0].getStyle() & sf::Text::Bold );
			running_width = std::max( running_width, (int)( running_total + g->bounds.position.x + g->bounds.size.x ));
			running_total += g->advance;
		}
	}

	first_char = j;

	// If we are word wrapping and found a space, then break at the space.
	// Otherwise, fit as much on this line as we can.
	//
	if (( last_space > 0 ) && m_word_wrap )
	{
		last_char = last_space - 1;
		i = last_space + 1;
	}
	else
	{
		if ( !edit_mode )
		{
			i--;
			last_char = i - 1;
			if ( last_char < 0 ) last_char = 0;
			if ( s[last_char] == L' ' ) last_char--;
		}
		else
		{
			if ( i < (int)s.size() ) i--;
			last_char = i - 1;
		}
	}

	position = i;
}

void FeTextPrimitive::setString( const std::string &t )
{
	//
	// UTF-8 character encoding is assumed.
	// Need to convert to UTF-32 before giving string to SFML
	//
	std::basic_string<std::uint32_t> tmp;
	sf::Utf8::toUtf32( t.begin(), t.end(), std::back_inserter( tmp ));

	// We need to add one trailing space to the string
	// for the word wrap function to work properly
	//
	std::fill_n( back_inserter( tmp ), 1, L' ' );

	setString( tmp );
}

sf::Vector2f FeTextPrimitive::setString(
			const std::basic_string<std::uint32_t> &t,
			int position )
{
	//
	// We count the total lines
	//
	m_lines = 1;
	m_lines_total = 1;

	int tmp_pos = -1;
	int tmp_first_char = 0;
	int tmp_last_char = 0;

	if ( m_word_wrap && (int)t.size() > 1 )
	{
		while ( true )
		{
			if ( tmp_pos >= (int)t.size() ) break;
			fit_string( t, tmp_pos, tmp_first_char, tmp_last_char );
			m_lines_total++;
		}
		m_lines_total--;
	}

	//
	// Calculate the number of lines we can fit in our RectShape
	//
	const sf::Font *font = getFont();
	const sf::Glyph *glyph = &font->getGlyph( L'X', m_texts[0].getCharacterSize(), m_texts[0].getStyle() & sf::Text::Bold );
	float glyphSize = glyph->bounds.size.y * m_texts[0].getScale().y;
	sf::FloatRect rectSize = sf::FloatRect( m_bgRect.getPosition(), m_bgRect.getSize() );
	int spacing = getLineSpacingFactored( font, floorf( m_texts[0].getCharacterSize() * m_texts[0].getScale().y ));
	float default_spacing = font->getLineSpacing( m_texts[0].getCharacterSize() ) * m_texts[0].getScale().y;

	if ( m_align & ( Top | Bottom | Middle ))
	{
		if ( m_margin < 0 )
			m_lines = std::max( 1, (int)floorf(( rectSize.size.y + spacing - default_spacing - glyphSize ) / spacing ));
		else
			m_lines = std::max( 1, (int)floorf(( rectSize.size.y + spacing - glyphSize - m_margin * 2.0 ) / spacing ));
	}
	else
		m_lines = std::max( 1, (int)( rectSize.size.y / spacing ));

	//
	// Cut the string if it is too big to fit our dimension
	//
	int first_char, last_char;
	int disp_cpos( position );

	if ( m_word_wrap )
		position = -1;

	if ( m_texts.size() > 1 )
		m_texts.resize( 1, sf::Text( *getFont() ));

	//
	// We cut the first line of text here
	//
	fit_string( t, position, first_char, last_char );

	if ( m_word_wrap )
	{
		if ( m_first_line > m_lines_total - m_lines + 1 ) m_first_line = std::max( 1, m_lines_total - m_lines + 1 );

		int actual_first_line = 1;
		while ( actual_first_line < m_first_line )
		{
			fit_string( t, position, first_char, last_char );
			actual_first_line++;
		}
	}
	m_texts[0].setString( sf::String::fromUtf32( t.data() + first_char, t.data() + first_char + ( last_char - first_char + 1 )));

	disp_cpos -= first_char;

	//
	// If we are word wrapping calculate the rest of lines
	//
	if ( m_word_wrap )
	{
		//
		// We break the string to lines here starting from the second line
		//
		int actual_line_count = 1;
		for ( int i = 1; i < m_lines; i++ )
		{
			if ( position >= (int)t.size() ) break;
			fit_string( t, position, first_char, last_char );
			m_texts.push_back( m_texts[0] );
			m_texts.back().setString( sf::String::fromUtf32( t.data() + first_char, t.data() + first_char + (last_char - first_char + 1 )));
			actual_line_count++;
		}
	}

	set_positions(); // We need to set the positions now for findCharacterPos() to work below

	// We apply kerning to cursor position
	if ( disp_cpos >= 0 )
	{
		int kerning = font->getKerning( m_texts[0].getString()[std::max( 0, disp_cpos - 1 )],
							            m_texts[0].getString()[disp_cpos],
							            m_texts[0].getCharacterSize() ) * m_texts[0].getScale().x;
		return m_texts[0].findCharacterPos( disp_cpos ) + sf::Vector2f( kerning, 0.0 );
	}
	else
		return sf::Vector2f( 0, 0 );
}

void FeTextPrimitive::set_positions() const
{
	float charSize = m_texts[0].getCharacterSize() * m_texts[0].getScale().y;
	float spacing = charSize;
	int glyphSize = getGlyphSize();

	const sf::Font *font = getFont();
	int margin = ( m_margin < 0 ) ? floorf( font->getLineSpacing( charSize ) / 2.0 ) : m_margin;
	spacing = getLineSpacingFactored( font, spacing );

	sf::Vector2f rectPos = m_bgRect.getPosition();
	sf::FloatRect rectSize = sf::FloatRect( m_bgRect.getPosition(), m_bgRect.getSize() );

	for ( int i=0; i < (int)m_texts.size(); i++ )
	{
		sf::Vector2f textPos;

		// we need to account for the scaling that we have applied to our text...
		sf::FloatRect textSize = m_texts[i].getLocalBounds();
		textSize.size.x *= m_texts[i].getScale().x;
		textSize.size.y *= m_texts[i].getScale().y;
		textSize.position.x *= m_texts[i].getScale().x;

		// outline compensation
		float outline = ceilf( m_outline );

		// set position x
		if ( m_align & Left )
			textPos.x = rectPos.x - outline;
		else if ( m_align & Right )
			textPos.x = rectPos.x + floorf( rectSize.size.x ) - textSize.size.x + outline;
		else if ( m_align & Centre )
			textPos.x = rectPos.x + floorf(( rectSize.size.x - textSize.size.x ) / 2.0 );

		if ( m_align & ( Top | Bottom | Middle ))
			textPos.x -= textSize.position.x;

		// set position y
		if ( m_align & Top )
			textPos.y = rectPos.y + ceilf( spacing * i - charSize + glyphSize );
		else if ( m_align & Bottom )
			textPos.y = rectPos.y + floorf( rectSize.size.y  - charSize - spacing * ( m_texts.size() - i - 1 ));
		else if ( m_align & Middle )
			textPos.y = rectPos.y + floorf( spacing * i + ( rectSize.size.y + glyphSize - charSize * 2 - spacing * ( m_texts.size() - 1 ) + 0.5 ) / 2.0 );
		else
			textPos.y = rectPos.y + ceilf( spacing * i + ( rectSize.size.y - ( spacing * m_texts.size() )) / 2.0 );

		if ( m_align & Top ) textPos.y += margin;
		if ( m_align & Bottom ) textPos.y -= margin;
		if ( m_align & Left ) textPos.x += margin;
		if ( m_align & Right ) textPos.x -= margin;

		sf::Transform trans;
		trans.rotate( m_bgRect.getRotation(), { rectPos.x, rectPos.y });
		m_texts[i].setPosition( trans.transformPoint( textPos ) );
		m_texts[i].setRotation( m_bgRect.getRotation() );
	}

	m_needs_pos_set = false;
}

int FeTextPrimitive::getActualWidth()
{
	float w = 0;

	for ( unsigned int i=0; i < m_texts.size(); i++ )
	{
		sf::FloatRect textSize = m_texts[i].getLocalBounds();
		textSize.size.x = ceilf( textSize.size.x * m_texts[i].getScale().x );

		if ( textSize.size.x > w )
			w = textSize.size.x;
	}

	return (int)w;
}

int FeTextPrimitive::getActualHeight()
{
	float h = 0;

	if ( m_texts.size() > 0 )
	{
		sf::FloatRect first = m_texts.front().getGlobalBounds();
		sf::FloatRect last = m_texts.back().getGlobalBounds();
		h = last.position.y + last.size.y - first.position.y;
	}

	return (int)h;
}

void FeTextPrimitive::setFont( const sf::Font &font )
{
	for ( unsigned int i=0; i < m_texts.size(); i++ )
		m_texts[i].setFont( font );

	m_needs_pos_set = true;
}

const sf::Font *FeTextPrimitive::getFont() const
{
	return &m_texts[0].getFont();
}

void FeTextPrimitive::setCharacterSize( unsigned int size )
{
	for ( unsigned int i=0; i < m_texts.size(); i++ )
		m_texts[i].setCharacterSize( size );

	m_needs_pos_set = true;
}

unsigned int FeTextPrimitive::getCharacterSize() const
{
	return ceilf( m_texts[0].getCharacterSize() * m_texts[0].getScale().y );
}

unsigned int FeTextPrimitive::getGlyphSize() const
{
	const sf::Font *font = getFont();
	const int charSize = m_texts[0].getCharacterSize();
	const sf::Glyph *glyph = &font->getGlyph( L'X', charSize, m_texts[0].getStyle() & sf::Text::Bold );
	return floorf(glyph->bounds.size.y * m_texts[0].getScale().y);
}

void FeTextPrimitive::setCharacterSpacing( float factor )
{
	for ( unsigned int i=0; i < m_texts.size(); i++ )
		m_texts[i].setLetterSpacing( factor );

	m_needs_pos_set = true;
}

float FeTextPrimitive::getCharacterSpacing() const
{
	return m_texts[0].getLetterSpacing();
}

void FeTextPrimitive::setLineSpacing( float factor )
{
	m_line_spacing = ( factor > 0.0 ) ? factor : 0.0;
	m_needs_pos_set = true;
}

float FeTextPrimitive::getLineSpacing() const
{
	return m_line_spacing;
}

int FeTextPrimitive::getLineSpacingFactored( const sf::Font *font, int charsize ) const
{
	return std::max( 1, (int)ceilf( font->getLineSpacing( charsize ) * m_line_spacing ));
}

sf::Vector2f FeTextPrimitive::getPosition() const
{
	return m_bgRect.getPosition();
}

sf::Vector2f FeTextPrimitive::getSize() const
{
	return m_bgRect.getSize();
}

void FeTextPrimitive::setPosition( const sf::Vector2f &p )
{
	m_bgRect.setPosition( p );
	m_needs_pos_set = true;
}

void FeTextPrimitive::setSize( const sf::Vector2f &s )
{
	m_bgRect.setSize( s );
	m_needs_pos_set = true;
}

void FeTextPrimitive::setAlignment( Alignment a )
{
	m_align = a;
	m_needs_pos_set = true;
}

FeTextPrimitive::Alignment FeTextPrimitive::getAlignment() const
{
	return m_align;
}

void FeTextPrimitive::setStyle( int s )
{
	for ( unsigned int i=0; i < m_texts.size(); i++ )
		m_texts[i].setStyle( s );
}

void FeTextPrimitive::setBgOutlineThickness( float t )
{
	m_bgRect.setOutlineThickness( t );
}

float FeTextPrimitive::getBgOutlineThickness()
{
	return m_bgRect.getOutlineThickness();
}

void FeTextPrimitive::setRotation( float r )
{
	m_bgRect.setRotation( sf::degrees( r ));
	m_needs_pos_set = true;
}

float FeTextPrimitive::getRotation() const
{
	return m_bgRect.getRotation().asDegrees();
}

int FeTextPrimitive::getStyle() const
{
	return m_texts[0].getStyle();
}

void FeTextPrimitive::setFirstLineHint( int line )
{
	if ( m_word_wrap )
		m_first_line = ( line < 1 ) ? 1 : line;
}

void FeTextPrimitive::setWordWrap( bool wrap )
{
	m_word_wrap = wrap;
	if ( !m_word_wrap ) m_first_line = 1;
}

bool FeTextPrimitive::getWordWrap()
{
	return m_word_wrap;
}

void FeTextPrimitive::setNoMargin( bool nomargin )
{
	m_margin = nomargin ? 0 : -1;
}

bool FeTextPrimitive::getNoMargin()
{
	return ( m_margin < 0 ) ? false : true;
}

void FeTextPrimitive::setMargin( int margin )
{
	m_margin = ( margin < 0 ) ? -1 : margin;
}

int FeTextPrimitive::getMargin()
{
	return m_margin;
}

void FeTextPrimitive::setOutlineThickness( float t )
{
	if ( t < 0.0 ) t = 0.0;
	m_outline = t;
	for ( unsigned int i=0; i < m_texts.size(); i++ )
		m_texts[i].setOutlineThickness( m_outline / m_texts[0].getScale().x );
}

float FeTextPrimitive::getOutlineThickness()
{
	return m_outline;
}

void FeTextPrimitive::setTextScale( const sf::Vector2f &s )
{
	for ( unsigned int i=0; i < m_texts.size(); i++ )
		m_texts[i].setScale( s );

	m_needs_pos_set = true;
}

sf::Vector2f FeTextPrimitive::getTextScale() const
{
		return m_texts[0].getScale();
}

int FeTextPrimitive::getFirstLineHint() const
{
	return m_first_line;
}

int FeTextPrimitive::getLines() const
{
	return m_lines;
}

int FeTextPrimitive::getLinesTotal() const
{
	return m_lines_total;
}

std::string FeTextPrimitive::getStringWrapped()
{
	std::string str;

	for ( unsigned int i=0; i < m_texts.size(); i++ )
	{
		std::string utf8(reinterpret_cast<const char*>(m_texts[i].getString().toUtf8().data()), m_texts[i].getString().toUtf8().size());
		str += utf8;
		str += "\n";
	}
	return str;
}

void FeTextPrimitive::draw( sf::RenderTarget &target, sf::RenderStates states ) const
{
	if ( m_needs_pos_set )
		set_positions();

	if ( m_bgRect.getFillColor().a > 0 )
		target.draw( m_bgRect, states );

	if ( m_texts[0].getFillColor().a > 0 )
		for ( unsigned int i=0; i < m_texts.size(); i++ )
			target.draw( m_texts[i], states );
}
