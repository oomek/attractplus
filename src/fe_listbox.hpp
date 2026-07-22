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

#ifndef FE_LISTBOX_HPP
#define FE_LISTBOX_HPP

#include <SFML/Graphics.hpp>
#include "fe_align.hpp"
#include "fe_presentable.hpp"
#include "tp.hpp"

#include <map>

class FeSettings;
class FeLanguage;
class FeFontContainer;

//
// The text game list
//
class FeListBox : public FeBasePresentable, public sf::Drawable
{
public:
	enum SelectionMode
	{
		Static=0,
		Moving=1,
		Paged=2,
		Bounded=3
	};
	// Constructor for use in scripts.  sets m_scripted to true
	FeListBox( FePresentableParent &p, int x, int y, int w, int h );

	// Constructor for use in overlay.  sets m_scripted to false
	FeListBox( FePresentableParent &p,
			const sf::Font *font,
			sf::Color colour,
			sf::Color bgcolour,
			sf::Color selcolour,
			sf::Color selbgcolour,
			unsigned int characterSize,
			int rows );

	// Constructor for use in overlay.  sets m_scripted to false
	FeListBox( FePresentableParent &p );

	FeListBox( const FeListBox & );

	FeListBox &operator=( const FeListBox & );

	void setFont( const sf::Font & );
	sf::Vector2f getPosition() const;
	void setPosition( const sf::Vector2f & );
	void setPosition( int x, int y ) {return setPosition(sf::Vector2f(x,y));};
	sf::Vector2f getSize() const;
	void setSize( const sf::Vector2f & );
	void setSize( int w, int h ) {return setSize(sf::Vector2f(w,h));};
	float getRotation() const;
	sf::Color getColor() const;
	sf::Color getOutlineColor() const;
	sf::Color getSelOutlineColor() const;
	int get_transform_origin_type() const;
	int get_anchor_type() const;
	int get_rotation_origin_type() const;
	float get_transform_origin_x() const;
	float get_transform_origin_y() const;
	float get_anchor_x() const;
	float get_anchor_y() const;
	float get_rotation_origin_x() const;
	float get_rotation_origin_y() const;

	int getIndexOffset() const;
	void setIndexOffset( int );
	int getFilterOffset() const;
	void setFilterOffset( int );

	void setColor( sf::Color );
	void setOutlineColor( sf::Color );
	void setSelOutlineColor( sf::Color );
	void setBgColor( sf::Color );
	void setSelColor( sf::Color );
	void setSelBgColor( sf::Color );
	void setSelStyle( int );
	int getSelStyle();
	void setTextScale( const sf::Vector2f & );
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

	bool getSelectedText( FeTextPrimitive* &sel );

	void setRotation( float );

	// Call `removeCustomText` to reset control from "custom text" mode
	void setCustomText( const int index, const std::vector<std::string> &list );
	void setCustomSelection( const int index );
	void removeCustomText();

	int getRowCount() const;

	void clear();
	void init_dimensions();
	void update_styles();

	// Overrides from base class:
	//
	void on_new_list( FeSettings * );
	void on_new_selection( FeSettings * );
	void set_scale_factor( float, float );

	void set_no_margin( bool );
	bool get_no_margin();
	void set_margin( float );
	float get_margin();

	const sf::Drawable &drawable() const { return (const sf::Drawable &)*this; };

	int get_bg_red();
	int get_bg_green();
	int get_bg_blue();
	int get_bg_alpha();
	int get_outline_red();
	int get_outline_green();
	int get_outline_blue();
	int get_outline_alpha();
	int get_sel_outline_red();
	int get_sel_outline_green();
	int get_sel_outline_blue();
	int get_sel_outline_alpha();
	float get_charsize();
	int get_glyph_size();
	float get_spacing();
	int get_rows();
	int get_list_align();
	int get_list_sel();
	int get_list_size();
	int get_style();
	int get_justify();
	int get_align();
	int get_case();
	int get_selection_mode();
	int get_selection_margin();
	void set_bg_red(int r);
	void set_bg_green(int g);
	void set_bg_blue(int b);
	void set_bg_alpha(int a);
	void set_outline_red(int r);
	void set_outline_green(int g);
	void set_outline_blue(int b);
	void set_outline_alpha(int a);
	void set_sel_outline_red(int r);
	void set_sel_outline_green(int g);
	void set_sel_outline_blue(int b);
	void set_sel_outline_alpha(int a);
	void set_bg_rgb( int, int, int );
	void set_bg_rgb( int, int, int, int );
	void set_charsize(float s);
	void set_spacing(float s);
	void set_rows(int r);
	void set_list_align(int a);
	void set_style(int s);
	void set_justify(int j);
	void set_align(int a);
	void set_case(int c);
	void set_selection_mode(int m);
	void set_selection_margin(int m);
	int get_sel_red();
	int get_sel_green();
	int get_sel_blue();
	int get_sel_alpha();
	void set_sel_red(int r);
	void set_sel_green(int g);
	void set_sel_blue(int b);
	void set_sel_alpha(int a);
	void set_sel_rgb( int, int, int );
	void set_sel_rgb( int, int, int, int );
	int get_sel_bg_red();
	int get_sel_bg_green();
	int get_sel_bg_blue();
	int get_sel_bg_alpha();
	const char *get_font();
	const char *get_format_string();

	void set_outline( float );
	void set_outline_rgb( int, int, int );
	void set_outline_rgb( int, int, int, int );
	void set_sel_outline_rgb( int, int, int );
	void set_sel_outline_rgb( int, int, int, int );
	void set_sel_outline( float );

	float get_outline();
	float get_sel_outline();

	void set_sel_bg_red(int r);
	void set_sel_bg_green(int g);
	void set_sel_bg_blue(int b);
	void set_sel_bg_alpha(int a);
	void set_sel_bg_rgb( int, int, int );
	void set_sel_bg_rgb( int, int, int, int );
	void set_font( const char *f );
	void set_format_string( const char *s );
	int get_selected_row() const;
	void set_selected_row( int r );
	int get_type() const;
	void refresh_script_geometry() override;
private:
	void update_list_settings( FeSettings *s );
	void refresh_selection();
	void refresh_list();
	void update_row_geometry();
	void update_text_metrics();
	void update_margin();

	FeTextPrimitive m_base_text;
	std::vector<std::string> m_custom_list;
	std::vector<FeTextPrimitive> m_texts;
	std::string m_font_name;
	std::string m_format_string;
	sf::Color m_selColour;
	sf::Color m_selBg;
	sf::Color m_selOutlineColour;
	sf::Vector2f m_position;
	sf::Vector2f m_size;
	sf::Vector2f m_transform_origin;
	sf::Vector2f m_anchor;
	sf::Vector2f m_rotation_origin;
	FeAlign m_transform_origin_type;
	FeAlign m_anchor_type;
	FeAlign m_rotation_origin_type;
	float m_selOutlineThickness;
	int m_selStyle;
	int m_rows;
	FeAlign m_list_align;
	float m_userCharSize;
	float m_userMargin;
	int m_filter_offset;
	float m_rotation;
	float m_scale_factor;
	bool m_scripted; // True when the list is used in a layout, false when used in the builtin-menu
	int m_mode;
	int m_selected_row;
	int m_list_start_offset;
	int m_selection_margin;

	int m_display_filter_index;
	int m_display_filter_size;
	int m_display_rom_index;
	FeSettings *m_feSettings;

	// Contains the selection index of m_custom_list, if the list is not empty.
	// Otherwise the control gets automagically updated with game info whenever the selection changes.
	int m_custom_sel;
	bool m_has_custom_list;

	void draw( sf::RenderTarget &target, sf::RenderStates states ) const;
};

#endif
