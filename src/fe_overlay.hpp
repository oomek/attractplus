/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-2016 Andrew Mickelson
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

#ifndef FE_OVERLAY_HPP
#define FE_OVERLAY_HPP

#include <SFML/Graphics.hpp>
#include "fe_present.hpp"
#include "fe_window.hpp"
#include "fe_config.hpp"
#include "fe_text.hpp"
#include "fe_listbox.hpp"

class FeSettings;
class FeInputMapEntry;
class FeBaseConfigMenu;
class FeTextPrimitive;

class FeEventLoopCtx;

class FeOverlay
{
friend class FeConfigContextImp;

private:
	FeWindow &m_wnd;
	FeSettings &m_feSettings;
	FePresent &m_fePresent;
	FeSoundSystem &m_soundSystem;

	sf::Color m_bg_color;
	sf::Color m_text_color;
	sf::Color m_theme_color;
	sf::Color m_letterbox_color;
	sf::Color m_border_color;
	sf::Color m_focus_color;
	sf::Color m_blur_color;

	bool m_overlay_is_on;
	int m_overlay_list_index;
	int m_overlay_list_size;
	sf::Vector2f m_screen_size;
	sf::Vector2f m_screen_pos;
	sf::Vector2f m_text_scale;
	int m_index_width;
	int m_list_char_size;
	int m_header_char_size;
	int m_footer_char_size;
	float m_row_height_scale;
	int m_letterbox_top_height;
	int m_letterbox_bottom_height;
	int m_letterbox_margin;
	int m_text_margin;
	int m_footer_width;
	int m_border_thickness;
	int m_enable_alpha;
	int m_disable_alpha;
	const sf::Font *m_font;
	FeInputMap::Command m_menu_command;

	enum LayoutStyle {
		None	= 0,
		Top		= 1 << 0,
		Middle	= 1 << 1,
		Bottom	= 1 << 2,
		Left	= 1 << 3,
		Centre	= 1 << 4,
		Right	= 1 << 5,
		Body	= 1 << 6,
		Full	= 1 << 7,
		Large	= 1 << 8,
		Single	= 1 << 9
	};

	sf::RectangleShape layout_background();
	sf::RectangleShape layout_letterbox( int style = LayoutStyle::None );
	sf::RectangleShape layout_border( int style = LayoutStyle::None );
	FeTextPrimitive layout_header( int style = LayoutStyle::None );
	FeTextPrimitive layout_footer();
	FeTextPrimitive layout_message( int style = LayoutStyle::None );
	FeTextPrimitive layout_index( int style = LayoutStyle::None );
	sf::Sprite layout_logo( sf::Texture &texture, int style = LayoutStyle::None );
	FeListBox layout_list( int style = LayoutStyle::None );

	void theme_letterbox( sf::RectangleShape &rect );
	void theme_border( sf::RectangleShape &rect );
	void theme_list( FeListBox &list );

	int list_row_height( int style );

	bool text_index(
		FeListBox &list,
		FeTextPrimitive &text,
		std::vector<std::string> &values,
		int index
	);

	enum LayoutFocus {
		Select		= 1 << 0,
		Edit		= 1 << 1,
		Disabled	= 1 << 2
	};

	bool layout_focus(
		FeListBox &label_list,
		FeListBox &value_list,
		int focus = LayoutFocus::Select
	);

	FeOverlay( const FeOverlay & );
	FeOverlay &operator=( const FeOverlay & );

	void get_common(
		sf::Vector2i &size,
		sf::Vector2f &text_scale,
		int &char_size ) const;

	void input_map_dialog(
		const std::string &msg_str,
		FeInputMapEntry &res,
		FeInputMap::Command &conflict
	);

	int display_config_dialog( FeBaseConfigMenu *, bool & );
	int display_config_dialog(
		FeBaseConfigMenu *m,
		bool &parent_setting_changed,
		int default_sel,
		FeInputMap::Command extra_exit
	);

	void init_event_loop( FeEventLoopCtx & );
	bool event_loop( FeEventLoopCtx & );

	bool edit_loop( std::vector<sf::Drawable *> draw_list,
			std::basic_string<std::uint32_t> &str, FeTextPrimitive *lb );

public:
	enum SelCode {
		ExitToDesktop = -999
	};

	FeOverlay( FeWindow &wnd,
		FeSettings &fes,
		FePresent &fep,
		FeSoundSystem &ss );

	void splash_message( const std::string &, const std::string &aux="" );
	void splash_logo( const std::string &aux="" );

	int confirm_dialog( const std::string &msg,
		bool default_yes = false,
		FeInputMap::Command default_exit = FeInputMap::Exit);

	bool config_dialog( int default_sel, FeInputMap::Command extra_exit );
	bool edit_game_dialog( int default_sel, FeInputMap::Command extra_exit );
	bool layout_options_dialog( bool preview, int &default_sel, FeInputMap::Command extra_exit );
	bool plugin_options_dialog( int &default_sel, FeInputMap::Command extra_exit );
	int languages_dialog();
	int tags_dialog( int default_sel, FeInputMap::Command extra_exit );

	FeInputMap::Command get_menu_command() { return m_menu_command; }
	void clear_menu_command() { m_menu_command = FeInputMap::Command::Back; }

	int common_list_dialog(
		const std::string &title,
		const std::vector < std::string > &options,
		int default_sel,
		int cancel_sel,
		FeInputMap::Command extra_exit=FeInputMap::LAST_COMMAND );

	int common_basic_dialog(
		const std::string &message,
		const std::vector<std::string> &options,
		int default_sel,
		int cancel_sel,
		FeInputMap::Command extra_exit=FeInputMap::LAST_COMMAND );

	bool edit_dialog( const std::string &msg_str, std::string &text );

	bool overlay_is_on() const { return m_overlay_is_on; };
	int overlay_get_list_index() const { return m_overlay_list_index; };
	int overlay_get_list_size() const { return m_overlay_list_size; };

	bool check_for_cancel();

	// Common handler for exit.  Prompts for confirmation (if configured) and executes
	// the exit_command (if configured).
	//
	// returns true if frontend should exit, false otherwise
	//
	bool common_exit();

	void init();
	void style_init();
	void style_init( sf::Color theme_color );

};

#endif
