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
		Paged=2
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

	int getIndexOffset() const;
	void setIndexOffset( int );
	int getFilterOffset() const;
	void setFilterOffset( int );

	void setColor( sf::Color );
	void setBgColor( sf::Color );
	void setSelColor( sf::Color );
	void setSelBgColor( sf::Color );
	void setSelStyle( int );
	int getSelStyle();
	void setTextScale( const sf::Vector2f & );

	bool getSelectedText( FeTextPrimitive* &sel );

	void setRotation( float );

	// Set an empty list here to reset control from "custom text" mode
	void setCustomText( const int index, const std::vector<std::string> &list );
	void setCustomSelection( const int index );

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
	void set_margin( int );
	int get_margin();

	const sf::Drawable &drawable() const { return (const sf::Drawable &)*this; };

	int get_bgr();
	int get_bgg();
	int get_bgb();
	int get_bga();
	int get_charsize();
	int get_glyph_size();
	float get_spacing();
	int get_rows();
	int get_list_size();
	int get_style();
	int get_justify();
	int get_align();
	int get_selection_mode();
	int get_selection_margin();
	void set_bgr(int r);
	void set_bgg(int g);
	void set_bgb(int b);
	void set_bga(int a);
	void set_bg_rgb( int, int, int );
	void set_charsize(int s);
	void set_spacing(float s);
	void set_rows(int r);
	void set_style(int s);
	void set_justify(int j);
	void set_align(int a);
	void set_selection_mode(int m);
	void set_selection_margin(int m);
	int get_selr();
	int get_selg();
	int get_selb();
	int get_sela();
	void set_selr(int r);
	void set_selg(int g);
	void set_selb(int b);
	void set_sela(int a);
	void set_sel_rgb( int, int, int );
	int get_selbgr();
	int get_selbgg();
	int get_selbgb();
	int get_selbga();
	const char *get_font();
	const char *get_format_string();

	void set_selbgr(int r);
	void set_selbgg(int g);
	void set_selbgb(int b);
	void set_selbga(int a);
	void set_selbg_rgb( int, int, int );
	void set_font( const char *f );
	void set_format_string( const char *s );
	int get_selected_row() const;
private:
	void internalSetText( const int index );


	FeTextPrimitive m_base_text;
	std::vector<std::string> m_custom_list;
	std::vector<FeTextPrimitive> m_texts;
	std::string m_font_name;
	std::string m_format_string;
	sf::Color m_selColour;
	sf::Color m_selBg;
	int m_selStyle;
	int m_rows;
	int m_userCharSize;
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
	bool has_custom_list() { return m_custom_list.size() > 0; }

	void draw( sf::RenderTarget &target, sf::RenderStates states ) const;
};

#endif
