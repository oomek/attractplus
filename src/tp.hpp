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

#ifndef TEXT_Primitive_HPP
#define TEXT_Primitive_HPP

#include <SFML/Graphics.hpp>
#include <vector>

class FeTextPrimitive : public sf::Drawable
{
public:
	enum Alignment {
		Left=1,
		Centre=2,
		Right=4,
		Top=8,
		Bottom=16,
		Middle=32
	};

	FeTextPrimitive();

	FeTextPrimitive( const sf::Font *font,
			sf::Color colour,
			sf::Color bgcolour,
			unsigned int charactersize,
			Alignment align=Centre );

	FeTextPrimitive( const FeTextPrimitive & );

	void setColor( sf::Color );
	void setBgColor( sf::Color );
	void setOutlineColor( sf::Color );
	void setBgOutlineColor( sf::Color );

	void setString( const std::string & );	// does utf-8 conversion!

	//
	// cursor_string_pos is the character in the string that the cursor
	// should be under.  Valid positions are from 0 to t.size().  This
	// function returns the screen location of the cursor character PROVIDED
	// that WordWrap is set to false (cursor positioning is not supported
	// with // wordwrapping on)
	//
	sf::Vector2f setString( const std::basic_string<std::uint32_t> &t,
					int cursor_string_pos=-1 ); // no utf-8 conversion

	void setFont( const sf::Font & );
	void setCharacterSize( unsigned int );
	void setCharacterSpacing( float );
	void setLineSpacing( float );
	void setAlignment( Alignment );
	void setPosition( int x, int y ) {return setPosition(sf::Vector2f(x,y));};
	void setPosition( const sf::Vector2f & );
	void setSize( int w, int h ) {return setSize(sf::Vector2f(w,h));};
	void setSize( const sf::Vector2f & );
	void setStyle( int );
	void setRotation( float );
	void setBgOutlineThickness( float );
	float getBgOutlineThickness();
	void setFirstLineHint( int );
	void setWordWrap( bool );
	bool getWordWrap();
	void setNoMargin( bool );
	bool getNoMargin();
	void setMargin( int );
	int getMargin();
	void setOutlineThickness( float );
	float getOutlineThickness();
	void setTextScale( const sf::Vector2f & );

	const sf::Font *getFont() const;
	sf::Color getColor() const;
	sf::Color getBgColor() const;
	sf::Color getOutlineColor() const;
	sf::Color getBgOutlineColor() const;
	unsigned int getCharacterSize() const;
	unsigned int getGlyphSize() const;
	float getCharacterSpacing() const;
	float getLineSpacing() const;
	int getLineSpacingFactored( const sf::Font *, int ) const;
	Alignment getAlignment() const;
	sf::Vector2f getPosition() const;
	sf::Vector2f getSize() const;
	float getRotation() const;
	int getStyle() const;
	int getFirstLineHint() const;
	int getLines() const;
	int getLinesTotal() const;
	sf::Vector2f getTextScale() const;
	std::string getStringWrapped();

	int getActualWidth(); // return the width of the actual text
	int getActualHeight(); // return the height of the actual text

private:
	sf::RectangleShape m_bgRect;
	mutable std::vector<sf::Text> m_texts;
	Alignment m_align;

	// this is set to -1 when "no word wrapping" is set.
	// otherwise it is a value of 0+ and corresponds to the first line
	// of text being displayed in the control.  The user can change this
	// to control scrolling text where the text set into the control is
	// larger than the area available to display it.
	int m_first_line;
	int m_lines;
	int m_lines_total;
	int m_margin;
	int m_outline;
	float m_line_spacing;
	bool m_word_wrap;

	mutable bool m_needs_pos_set;

	//
	// Determines how to fit the given string "s" into the text space
	// parameters:
	//		[in] s 				- string to be displayed
	//		[out] position		- If WordWrap is true, this is the first position in
	//							  "s" that can be displayed.  If WordWrap is false, this
	//							  is the location of the cursor in "s".
	//		[out] first_char	- position of the first character to display
	//		[out] last_char		- position of the last character to display
	//
	void fit_string(
			const std::basic_string<std::uint32_t> &s,
			int &position,
			int &first_char,
			int &last_char );

	void set_positions() const;

	// override from base
	void draw( sf::RenderTarget &target, sf::RenderStates states ) const;
};

#endif
