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

#ifndef FE_TEXT_HPP
#define FE_TEXT_HPP

#include <SFML/Graphics.hpp>
#include "fe_align.hpp"
#include "fe_presentable.hpp"
#include "tp.hpp"

class FeSettings;

//
// Text (w/ background) to display info on screen
//
class FeText : public FeBasePresentable, public sf::Drawable
{
public:
	FeText( FePresentableParent &p,
		const std::string &str, int x, int y, int w, int h );

	void setFont( const sf::Font & );
	sf::Vector2f getPosition() const;
	void setPosition( const sf::Vector2f & );
	void setPosition( int x, int y ) {return setPosition(sf::Vector2f(x,y));};
	sf::Vector2f getSize() const;
	void setSize( const sf::Vector2f & );
	void setSize( int w, int h ) {return setSize(sf::Vector2f(w,h));};
	float getRotation() const;
	void setRotation( float );
	sf::Color getColor() const;
	void setColor( sf::Color );
	int get_transform_origin_type() const;
	int get_anchor_type() const;
	int get_rotation_origin_type() const;
	float get_transform_origin_x() const;
	float get_transform_origin_y() const;
	float get_anchor_x() const;
	float get_anchor_y() const;
	float get_rotation_origin_x() const;
	float get_rotation_origin_y() const;
	void set_transform_origin( float x, float y );
	void set_transform_origin_type( int t );
	void set_anchor( float x, float y );
	void set_anchor_type( int t );
	void set_rotation_origin( float x, float y );
	void set_rotation_origin_type( int t );
	void set_transform_origin_x( float x );
	void set_transform_origin_y( float y );
	void set_anchor_x( float x );
	void set_anchor_y( float y );
	void set_rotation_origin_x( float x );
	void set_rotation_origin_y( float y );

	// Overrides from base class:
	//
	void on_new_list( FeSettings * );
	void on_new_selection( FeSettings * );
	void set_scale_factor( float, float );

	const sf::Drawable &drawable() const { return (const sf::Drawable &)*this; };

	int getIndexOffset() const;
	void setIndexOffset( int );
	int getFilterOffset() const;
	void setFilterOffset( int );

	void set_word_wrap( bool );
	bool get_word_wrap();

	void set_no_margin( bool );
	bool get_no_margin();
	void set_margin( float );
	float get_margin();
	void set_outline( float );
	float get_outline();
	void set_bg_outline( float );
	float get_bg_outline();

	void set_first_line_hint( int l );
	int get_first_line_hint();
	int get_lines();
	int get_lines_total();

	const char *get_string();
	void set_string(const char *s);
	const char *get_string_wrapped();

	float get_cursor_pos( int i );

	int get_actual_width() { return m_draw_text.getActualWidth(); };
	int get_actual_height() { return m_draw_text.getActualHeight(); };

	int get_bg_red();
	int get_bg_green();
	int get_bg_blue();
	int get_bg_alpha();
	int get_bg_outline_red();
	int get_bg_outline_green();
	int get_bg_outline_blue();
	int get_bg_outline_alpha();
	int get_outline_red();
	int get_outline_green();
	int get_outline_blue();
	int get_outline_alpha();
	float get_charsize();
	int get_glyph_size();
	float get_spacing();
	float get_line_spacing();
	int get_line_height();
	int get_style();
	int get_justify();
	int get_align();
	int get_case();
	const char *get_font();
	void set_bg_red(int r);
	void set_bg_green(int g);
	void set_bg_blue(int b);
	void set_bg_alpha(int a);
	void set_bg_outline_red(int r);
	void set_bg_outline_green(int g);
	void set_bg_outline_blue(int b);
	void set_bg_outline_alpha(int a);
	void set_outline_red(int r);
	void set_outline_green(int g);
	void set_outline_blue(int b);
	void set_outline_alpha(int a);
	void set_rgb(int r, int g, int b, int a) override;
	void set_bg_rgb( int, int, int );
	void set_bg_rgb( int, int, int, int );
	void set_bg_outline_rgb( int, int, int );
	void set_bg_outline_rgb( int, int, int, int );
	void set_outline_rgb( int, int, int );
	void set_outline_rgb( int, int, int, int );
	void set_charsize(float s);
	void set_spacing(float s);
	void set_line_spacing(float s);
	void set_style(int s);
	void set_justify(int j);
	void set_align(int a);
	void set_case(int c);
	void set_font(const char *f);
	int get_type() const;
	bool get_magic() const;
	void refresh_script_geometry() override;

protected:
	void draw( sf::RenderTarget &target, sf::RenderStates states ) const;

private:
	FeText( const FeText & );
	FeText &operator=( const FeText & );

	void update_font_size();
	void update_margin();
	void update_transform();

	FeTextPrimitive m_draw_text;
	std::string m_string;
	std::string m_string_wrapped;
	std::string m_font_name;
	int m_index_offset;
	int m_filter_offset;
	float m_user_charsize;	 	// -1 if no charsize specified
	float m_user_margin;	 	// -1 if automatic margin is used
	sf::Vector2f m_size;		// unscaled size
	sf::Vector2f m_position;	// unscaled position
	sf::Vector2f m_transform_origin;
	sf::Vector2f m_anchor;
	sf::Vector2f m_rotation_origin;
	FeAlign m_transform_origin_type;
	FeAlign m_anchor_type;
	FeAlign m_rotation_origin_type;
	float m_rotation;
	float m_scale_factor;
	bool m_magic;
	bool m_link_outline_alpha{true}; // legacy - outline_alpha linked to alpha
	bool m_link_bg_outline_alpha{true}; // legacy - bg_outline_alpha linked to bg_alpha
};

#endif
