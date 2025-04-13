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
	sf::Color m_bg_colour;
	sf::Color m_edge_bg_color;
	sf::Color m_edge_line_colour;
	sf::Color m_sel_focus_colour;
	sf::Color m_sel_text_colour;
	sf::Color m_sel_blur_colour;
	sf::Color m_header_text_colour;
	sf::Color m_footer_text_colour;
	sf::Color m_text_colour;
	bool m_overlay_is_on;
	sf::Vector2i m_screen_size;
	sf::Vector2f m_text_scale;
	int m_text_size;
	int m_header_size;
	int m_footer_size;
	int m_edge_size;
	int m_line_size;
	int m_fade_alpha;
	const sf::Font *m_font;
	FeInputMap::Command m_menu_command;

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
	FeOverlay( FeWindow &wnd,
		FeSettings &fes,
		FePresent &fep );

	void splash_message( const std::string &, const std::string &rep="",
		const std::string &aux="" );

	int confirm_dialog( const std::string &msg,
		const std::string &rep="",
		bool default_yes = false,
		FeInputMap::Command default_exit = FeInputMap::Exit);

	bool config_dialog( int default_sel, FeInputMap::Command extra_exit );
	bool edit_game_dialog( int default_sel, FeInputMap::Command extra_exit );
	bool layout_options_dialog( bool preview, int &default_sel, FeInputMap::Command extra_exit );
	int languages_dialog();
	int tags_dialog( int default_sel, FeInputMap::Command extra_exit );

	FeInputMap::Command get_menu_command() { return m_menu_command; }
	void clear_menu_command() { m_menu_command = (FeInputMap::Command)-1; }

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

	bool check_for_cancel();

	// Common handler for exit.  Prompts for confirmation (if configured) and executes
	// the exit_command (if configured).
	//
	// returns true if frontend should exit, false otherwise
	//
	bool common_exit();

	void init();
	void style_init();
};

#endif
