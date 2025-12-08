/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-16 Andrew Mickelson
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

#define _USE_MATH_DEFINES
#include "fe_overlay.hpp"
#include "fe_settings.hpp"
#include "fe_config.hpp"
#include "fe_util.hpp"
#include <SFML/Graphics.hpp>
#include "base64.hpp"
#include <cmath>
#include <algorithm>

const int MIN_LIST_CHAR_SIZE = 12;
const int MIN_FOOTER_CHAR_SIZE = 9;
const int MIN_FOOTER_LINES = 3;
const bool FORCE_ODD_ROWS = true;

namespace
{

unsigned char my_char_table[] =
{
	' ',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'?', '!', ',', '.', ':', ';', '/', '\\','\"','\'',')', '(', '*', '+', '-', '=', '_', '$', '@', '%', '&', '~',
	0
};

int get_char_idx( unsigned char c )
{
	int i=0;
	while ( my_char_table[i] )
	{
		if ( my_char_table[i] == c )
			return i;
		i++;
	}

	return 0;
}

// Returns > 0 if value is "truthy", will match value of corresponding falsy
const int is_truthy( const std::string value )
{
	if ( icompare( value, _( "Yes" )) == 0 ) return 1;
	if ( icompare( value, _( "On" )) == 0 ) return 2;
	if ( icompare( value, _( "True" )) == 0 ) return 3;
	return 0;
}

// Returns > 0 if value is "falsy", will match value of corresponding truthy
const int is_falsy( const std::string value )
{
	if ( icompare( value, _( "No" )) == 0 ) return 1;
	if ( icompare( value, _( "Off" )) == 0 ) return 2;
	if ( icompare( value, _( "False" )) == 0 ) return 3;
	return 0;
}

// Returns true if the given list has a matching pair of truthy/falsy options in any order
const bool is_bool_list( const std::vector<std::string> &values )
{
	if ( values.size() != 2 ) return false;
	int v0 = is_truthy( values[0] );
	int v1 = is_falsy( values[1] );
	if ( !v0 || !v1 )
	{
		v0 = is_falsy( values[0] );
		v1 = is_truthy( values[1] );
	}
	return v0 && v1 && ( v0 == v1 );
}

std::string get_pill_glyph( bool checked )
{
	return checked ? FE_YES_ICON : FE_NO_ICON;
}

// Replaces right list value with pill glyph
void swap_bool_to_pill_glyphs( std::vector<std::string> &right_list, const std::vector<FeMenuOpt> &opt_list, int idx )
{
	if ( !is_bool_list( opt_list[idx].values_list )) return;
	right_list[ idx ] = get_pill_glyph( is_truthy( right_list[ idx ] ) );
}

};

class FeConfigContextImp : public FeConfigContext
{
private:
	FeOverlay &m_feo;

public:
	std::vector<std::string> left_list;
	std::vector<std::string> right_list;
	int exit_sel;

	FeConfigContextImp( FeSettings &fes, FeOverlay &feo );
	bool edit_dialog( const std::string &m, std::string &t );

	bool confirm_dialog( const std::string &m, bool default_yes = false );

	int option_dialog( const std::string &title,
				const std::vector < std::string > &options,
				int default_sel=0 );

	void splash_message( const std::string &msg,
		const std::string &aux = "" );

	void input_map_dialog( const std::string &m,
		FeInputMapEntry &res,
		FeInputMap::Command &conflict );

	void tags_dialog();

	void update_to_menu( FeBaseConfigMenu *m );

	bool check_for_cancel();

	int basic_dialog( const std::string &msg,
		const std::vector < std::string > &options,
		int default_sel=0,
		int cancel_sel=0 );
};

FeConfigContextImp::FeConfigContextImp( FeSettings &fes, FeOverlay &feo )
	: FeConfigContext( fes ), m_feo( feo ), exit_sel( 0 )
{
}

bool FeConfigContextImp::edit_dialog( const std::string &msg, std::string &t )
{
	return m_feo.edit_dialog( msg, t );
}

bool FeConfigContextImp::confirm_dialog( const std::string &msg, bool default_yes )
{
	return !m_feo.confirm_dialog( msg, default_yes );
}

int FeConfigContextImp::option_dialog( const std::string &title,
	const std::vector < std::string > &options,
	int default_sel )
{
	return m_feo.common_list_dialog( title, options, default_sel, default_sel );
}

void FeConfigContextImp::splash_message(
			const std::string &msg,
			const std::string &aux )
{
	m_feo.splash_message( msg, aux );
}

void FeConfigContextImp::input_map_dialog( const std::string &msg,
		FeInputMapEntry &res,
		FeInputMap::Command &conflict )
{
	m_feo.input_map_dialog( msg, res, conflict );
}

void FeConfigContextImp::tags_dialog()
{
	m_feo.tags_dialog( 0, FeInputMap::LAST_COMMAND );
}

void FeConfigContextImp::update_to_menu(
		FeBaseConfigMenu *m )
{
	opt_list.clear();
	left_list.clear();
	right_list.clear();

	m->get_options( *this );

	for ( int i=0; i < (int)opt_list.size(); i++ )
	{
		// `MENU` type options are forced to have an empty right_list value
		left_list.push_back( opt_list[i].setting );
		right_list.push_back( opt_list[i].get_value() );

		if ( opt_list[i].type == Opt::DEFAULTEXIT )
			exit_sel=i;
	}

	for ( size_t i = 0; i < right_list.size(); ++i )
		swap_bool_to_pill_glyphs( right_list, opt_list, i );
}

bool FeConfigContextImp::check_for_cancel()
{
	return m_feo.check_for_cancel();
}

int FeConfigContextImp::basic_dialog( const std::string &msg,
	const std::vector < std::string > &options,
	int default_sel,
	int cancel_sel )
{
	return m_feo.common_basic_dialog( msg, options, default_sel, cancel_sel );
}

class FeEventLoopCtx
{
public:
	FeEventLoopCtx(
		const std::vector<sf::Drawable *> &draw_list,
		int &sel,
		int default_sel,
		int list_size
	);

	const std::vector<sf::Drawable *> &draw_list; // draw list
	int &sel;			// selection counter
	int default_sel;	// default selection
	int list_size;		// total number of options
	int page_size;		// pagination size
	int move_count;
	std::optional<sf::Event> move_event;
	sf::Clock move_timer;
	FeInputMap::Command move_command;
	FeInputMap::Command extra_exit;

	void move_start( std::optional<sf::Event> ev, FeInputMap::Command c );
};

FeEventLoopCtx::FeEventLoopCtx(
	const std::vector<sf::Drawable *> &draw_list,
	int &sel,
	int default_sel,
	int list_size
):
	draw_list( draw_list ),
	sel( sel ),
	default_sel( default_sel ),
	list_size( list_size ),
	page_size( 1 ),
	move_count( 0 ),
	move_command( FeInputMap::LAST_COMMAND ),
	extra_exit( FeInputMap::LAST_COMMAND )
{
}

// Begin a repeatable command
void FeEventLoopCtx::move_start( std::optional<sf::Event> ev, FeInputMap::Command c )
{
	move_event = ev;
	move_command = c;
	move_count = 0;
	move_timer.restart();
}

class FeFlagMinder
{
public:
	FeFlagMinder( bool &flag )
		: m_flag( flag ), m_flag_set( false )
	{
		if ( !m_flag )
		{
			m_flag = true;
			m_flag_set = true;
		}
	}

	~FeFlagMinder()
	{
		if ( m_flag_set )
			m_flag = false;
	}

	bool flag_set() { return m_flag_set; };

private:
	bool &m_flag;
	bool m_flag_set;
};

FeOverlay::FeOverlay( FeWindow &wnd,
		FeSettings &fes,
		FePresent &fep )
	: m_wnd( wnd ),
	m_feSettings( fes ),
	m_fePresent( fep ),
	m_overlay_is_on( false )
{
	clear_menu_command();
	init();
}

void FeOverlay::init()
{
	m_screen_pos = sf::Vector2f( 0.0, 0.0 );
	m_screen_size = sf::Vector2f( m_fePresent.get_screen_size() );
	float screen_min = std::min( m_screen_size.x, m_screen_size.y );
	float px = std::max( 1.0, screen_min / 240.0 ); // one pixel at 240p

	float scale_x = m_fePresent.get_layout_scale_x();
	float scale_y = m_fePresent.get_layout_scale_y();
	float scale_factor = std::max( scale_x, scale_y );
	if ( scale_factor <= 0.0 ) scale_factor = 1.0;
	m_text_scale.x = m_text_scale.y = 1.0 / scale_factor;

	m_list_char_size = std::max(
		MIN_LIST_CHAR_SIZE,
		( m_feSettings.ui_font_size() > 0 )
			? m_feSettings.ui_font_size()
			: (int)( 10.0 * px * scale_factor )
	);
	m_footer_char_size = std::max(
		MIN_FOOTER_CHAR_SIZE,
		(int)( m_list_char_size * 0.65 )
	);
	m_header_char_size = m_list_char_size * 1.5;

	float footer_line_space = m_footer_char_size * 0.25;
	m_text_margin = m_list_char_size * 0.45;
	m_letterbox_margin = 8 * px;
	m_letterbox_top_height = m_header_char_size + m_letterbox_margin * 2.0;
	m_letterbox_bottom_height = std::max(
		(int)(
			( m_footer_char_size * MIN_FOOTER_LINES )
			+ ( footer_line_space * ( MIN_FOOTER_LINES - 1 ) )
			+ ( m_text_margin * 2.0 )
		),
		m_letterbox_top_height
	);

	m_border_thickness = 1 * px;
	m_row_height_scale = 1.45;
	m_index_width = m_list_char_size * 1.5 + m_text_margin * 2.0;

	m_enable_alpha = 255;
	m_disable_alpha = 80;
	m_font = m_fePresent.get_default_font();

	style_init();
}

// Update all the layout colours after the `theme_color` has changed
//
void FeOverlay::style_init()
{
	// Defaults to the first uiColorTokens value, which is the class blue
	sf::Color col;
	if ( !str_to_color( m_feSettings.get_info( FeSettings::UIColor ), col ))
		str_to_color( FeSettings::uiColorTokens[ FE_DEFAULT_UI_COLOR_TOKEN ], col );
	style_init( col );
}

void FeOverlay::style_init( sf::Color theme_color )
{
	m_theme_color = theme_color;
	m_bg_color = sf::Color( 0, 0, 0, 230 );
	m_text_color = sf::Color( 255, 255, 255, 255 );
	m_letterbox_color = theme_color * sf::Color( 64, 64, 64, 255 );
	m_border_color = theme_color * sf::Color( 192, 192, 192, 255 );
	m_focus_color = theme_color * sf::Color( 160, 160, 160, 255 );
	m_blur_color = theme_color * sf::Color( 88, 88, 88, 255 );
}

// Simple overlay with message and footer
// - Drawn once - displayed for a single frame
// - `fe.overlay.splash_menu`
//
void FeOverlay::splash_message( const std::string &msg, const std::string &aux )
{
	sf::RectangleShape bg = layout_background();
	FeTextPrimitive message = layout_message( LayoutStyle::Middle );
	FeTextPrimitive extra = layout_footer();

	message.setString( msg );
	extra.setString( aux );

	const sf::Transform &t = m_fePresent.get_ui_transform();

	// Process tick only when Layout is fully loaded
	if ( m_fePresent.is_layout_loaded() )
		m_fePresent.tick();

	m_wnd.clear();
	m_wnd.draw( m_fePresent, t );
	m_wnd.draw( bg, t );
	m_wnd.draw( message, t );
	m_wnd.draw( extra, t );
	m_wnd.display();
}

void FeOverlay::splash_logo( const std::string &aux )
{
	sf::RectangleShape bg = layout_background();
	FeTextPrimitive extra = layout_footer();

	sf::Texture logo_texture = sf::Texture( *(FePresent::script_get_fep()->get_logo_full_image()) );
	sf::Sprite logo = layout_logo( logo_texture, LayoutStyle::Large | LayoutStyle::Middle | LayoutStyle::Centre );

	extra.setString( aux );

	const sf::Transform &t = m_fePresent.get_ui_transform();

	// Process tick only when Layout is fully loaded
	if ( m_fePresent.is_layout_loaded() )
		m_fePresent.tick();

	m_wnd.clear();
	m_wnd.draw( m_fePresent, t );
	m_wnd.draw( bg, t );
	m_wnd.draw( logo, t );
	m_wnd.draw( extra, t );
	m_wnd.display();
}

// Dialog with message and Yes/No option
// - Exit, Clear, Delete confirmations
//
int FeOverlay::confirm_dialog( const std::string &msg,
	bool default_yes,
	FeInputMap::Command default_exit )
{
	std::vector<std::string> list = { _( "Yes" ), _( "No" ) };
	return common_basic_dialog( msg, list, (default_yes ? 0 : 1), 1, default_exit );
}

// Dialog with header and list, triggered by layouts
// - May be overriden
// - Filters, Tags, Display menu
// - `fe.overlay.list_dialog` with > 2 entries
//
int FeOverlay::common_list_dialog(
	const std::string &title,
	const std::vector < std::string > &options,
	int default_sel,
	int cancel_sel,
	FeInputMap::Command extra_exit )
{
	int sel = default_sel;
	std::vector<sf::Drawable *> draw_list;
	FeEventLoopCtx c( draw_list, sel, cancel_sel, options.size() );
	c.extra_exit = extra_exit;

	FeFlagMinder fm( m_overlay_is_on );

	// var gets used if we trigger the ShowOverlay transition
	int var = 0;
	if ( extra_exit != FeInputMap::LAST_COMMAND )
		var = (int)extra_exit;

	FeText *custom_caption;
	FeListBox *custom_lb;
	bool do_custom = m_fePresent.get_overlay_custom_controls(
		custom_caption, custom_lb );

	if ( fm.flag_set() && do_custom )
	{
		//
		// Use custom controls set by script
		//
		std::string old_cap;
		if ( custom_caption )
		{
			old_cap = custom_caption->get_string();
			custom_caption->set_string( title.c_str() );
		}

		if ( custom_lb )
			custom_lb->setCustomText( sel, options );

		m_overlay_list_index = sel;
		m_overlay_list_size = options.size();
		c.page_size = m_fePresent.get_page_size();

		m_fePresent.on_transition( ShowOverlay, var );

		init_event_loop( c );
		while ( event_loop( c ) == false )
		{
			m_overlay_list_index = sel;

			if ( custom_lb )
				custom_lb->setCustomSelection( sel );

			m_fePresent.on_transition( NewSelOverlay, sel );
		}

		// Transition prior to reset allows layout to "fade-out" last state
		m_fePresent.on_transition( HideOverlay, 0 );

		// reset to the old text in these controls when done
		if ( custom_caption )
			custom_caption->set_string( old_cap.c_str() );

		if ( custom_lb )
		{
			custom_lb->removeCustomText();
			custom_lb->on_new_list( &m_feSettings );
		}
	}
	else
	{
		//
		// Use the default built-in controls
		//
		sf::RectangleShape bg = layout_background();
		sf::RectangleShape letterbox = layout_letterbox( LayoutStyle::Top );
		sf::RectangleShape border = layout_border( LayoutStyle::Top );
		FeTextPrimitive header = layout_header( LayoutStyle::Centre );
		FeListBox dialog = layout_list( LayoutStyle::Body | LayoutStyle::Large );

		draw_list.push_back( &bg );
		draw_list.push_back( &letterbox );
		draw_list.push_back( &border );
		draw_list.push_back( &header );
		draw_list.push_back( &dialog );

		header.setString( title );
		dialog.setCustomText( sel, options );

		m_overlay_list_index = sel;
		m_overlay_list_size = options.size();
		c.page_size = dialog.get_rows() / 2;

		if ( fm.flag_set() )
			m_fePresent.on_transition( ShowOverlay, var );

		init_event_loop( c );
		while ( event_loop( c ) == false )
		{
			m_overlay_list_index = sel;

			dialog.setCustomSelection( sel );

			if ( fm.flag_set() )
				m_fePresent.on_transition( NewSelOverlay, sel );
		}

		if ( fm.flag_set() )
			m_fePresent.on_transition( HideOverlay, 0 );
	}

	return sel;
}

// Languages dialog with list only
// - Only display on first-run
int FeOverlay::languages_dialog()
{
	std::vector<FeLanguage> ll;
	m_feSettings.get_languages_list( ll );

	if ( ll.size() <= 1 )
	{
		// if there is nothing to select, then set what we can and get out of here
		//
		std::string fallback = ll.empty() ? FE_DEFAULT_LANGUAGE : ll.front().language;

		m_feSettings.set_language( fallback );
		FeLog() << "Error: " << ll.size() << " Language resource(s) found, forcing language to \""
			<< fallback << "\"." << std::endl;
		return 0;
	}

	// Try and get a useful default setting based on POSIX locale value...
	//
	// TODO: figure out how to do this right on Windows...
	//
	std::string loc, test( FE_DEFAULT_LANGUAGE );
	try { loc = std::locale("").name(); } catch (...) {}
	if ( loc.size() > 1 )
		test = loc;

	int status( -3 ), current_i( 0 ), i( 0 );
	for ( std::vector<FeLanguage>::iterator itr=ll.begin(); itr != ll.end(); ++itr )
	{
		if (( status < 0 ) && ( test.compare( 0, 5, (*itr).language ) == 0 ))
		{
			current_i = i;
			status = 0;
		}
		else if (( status < -1 ) && ( test.compare( 0, 2, (*itr).language) == 0 ))
		{
			current_i = i;
			status = -1;
		}
		else if (( status < -2 ) && ( (*itr).language.compare( 0, 2, FE_DEFAULT_LANGUAGE ) == 0 ))
		{
			current_i = i;
			status = -2;
		}

		i++;
	}

	int sel = current_i;

	sf::RectangleShape bg = layout_background();
	FeListBox dialog = layout_list( LayoutStyle::Full | LayoutStyle::Large );
	std::vector<sf::Drawable*> draw_list = { &bg, &dialog };

	std::vector<std::string> labels;
	for ( const auto& lang : ll )
		labels.push_back( lang.label );

	dialog.setCustomText( sel, labels );

	FeEventLoopCtx c( draw_list, sel, -1, ll.size() );
	FeFlagMinder fm( m_overlay_is_on );
	c.page_size = dialog.get_rows() / 2;

	init_event_loop( c );
	while ( event_loop( c ) == false )
		dialog.setCustomSelection( sel );

	if ( sel >= 0 )
		m_feSettings.set_language( ll[sel].language );

	return sel;
}

int FeOverlay::tags_dialog( int default_sel, FeInputMap::Command extra_exit )
{
	int sel = default_sel;
	bool tags_changed = false;
	bool list_changed = false;
	std::string title = _( "Tags" );

	// Remain in tags dialog until exited
	while ( sel >= 0 )
	{
		// Prepare the list
		std::vector<std::pair<std::string, bool>> tags_list;
		m_feSettings.get_current_tags_list( tags_list );

		std::vector<std::string> list;
		for ( std::vector<std::pair<std::string, bool>>::iterator itr=tags_list.begin(); itr!=tags_list.end(); ++itr )
		{
			list.push_back( (*itr).second ? as_str( FE_TAG_PREFIX ) + "$1" : "$1" );
			perform_substitution( list.back(), { (*itr).first } );
		}
		list.push_back( _( "Add Tag" ) );
		list.push_back( _( "Back") );

		// Get the selection
		sel = common_list_dialog( title, list, sel, -1, extra_exit );

		if ( sel == (int)list.size() - 1 )
		{
			// Back
			sel = -1;
		}
		else if ( sel == (int)list.size() - 2 )
		{
			// Add Tag
			std::string tag_name;
			edit_dialog( _( "Enter Tag Name" ), tag_name );

			if ( !tag_name.empty() )
			{
				tags_changed = true;
				if ( m_feSettings.set_tag_current( tag_name, true ) )
					list_changed = true;
			}
		}
		else if ( sel >= 0 )
		{
			// Toggle existing tag
			tags_changed = true;
			if ( m_feSettings.set_tag_current( tags_list[sel].first, !tags_list[sel].second ) )
				list_changed = true;
		}
	}

	if ( tags_changed )
	{
		// Update without reset if the list has not been changed
		// - This will allow layout lists to display the changed tags
		m_fePresent.update_to( ToNewList, list_changed );
		m_fePresent.on_transition( ToNewList, 0 );
		m_fePresent.on_transition( ChangedTag, FeRomInfo::Tags );
	}

	return sel;
}

// Simple dialog with message and list
// - No headers or footers
// - Used by Exit menu
// - fe.overlay.list_dialog where <= 2 options
//
int FeOverlay::common_basic_dialog(
			const std::string &msg_str,
			const std::vector<std::string> &list,
			int default_sel,
			int cancel_sel,
			FeInputMap::Command extra_exit )
{
	std::vector<sf::Drawable *> draw_list;
	int sel=default_sel;

	FeEventLoopCtx c( draw_list, sel, cancel_sel, list.size() );
	c.extra_exit = extra_exit;

	FeFlagMinder fm( m_overlay_is_on );

	// var gets used if we trigger the ShowOverlay transition
	int var = 0;
	if ( extra_exit != FeInputMap::LAST_COMMAND )
		var = (int)extra_exit;

	FeText *custom_caption;
	FeListBox *custom_lb;
	bool do_custom = m_fePresent.get_overlay_custom_controls(
		custom_caption, custom_lb );

	if ( fm.flag_set() && do_custom )
	{
		//
		// Custom overlay controlled by the script
		//
		std::string old_cap;
		if ( custom_caption )
		{
			old_cap = custom_caption->get_string();
			custom_caption->set_string( msg_str.c_str() );
		}

		m_overlay_list_index = sel;
		m_overlay_list_size = list.size();
		c.page_size = m_fePresent.get_page_size();

		if ( custom_lb )
			custom_lb->setCustomText( sel, list );

		m_fePresent.on_transition( ShowOverlay, var );

		init_event_loop( c );
		while ( event_loop( c ) == false )
		{
			m_overlay_list_index = sel;

			if ( custom_lb )
				custom_lb->setCustomSelection( sel );

			m_fePresent.on_transition( NewSelOverlay, sel );
		}

		// Transition prior to reset allows layout to "fade-out" last state
		m_fePresent.on_transition( HideOverlay, 0 );

		// reset to the old text in these controls when done
		if ( custom_caption )
			custom_caption->set_string( old_cap.c_str() );

		if ( custom_lb )
		{
			custom_lb->removeCustomText();
			custom_lb->on_new_list( &m_feSettings );
		}
	}
	else
	{
		//
		// Use the default built-in controls
		//
		sf::RectangleShape bg = layout_background();
		FeTextPrimitive message = layout_message( LayoutStyle::Top );
		FeListBox dialog = layout_list( LayoutStyle::Bottom | LayoutStyle::Large );

		message.setString( msg_str );
		dialog.setCustomText( sel, list );

		draw_list.push_back( &bg );
		draw_list.push_back( &message );
		draw_list.push_back( &dialog );

		m_overlay_list_index = sel;
		m_overlay_list_size = list.size();
		c.page_size = dialog.get_rows() / 2;

		if ( fm.flag_set() )
			m_fePresent.on_transition( ShowOverlay, var );

		init_event_loop( c );
		while ( event_loop( c ) == false )
		{
			m_overlay_list_index = sel;

			dialog.setCustomSelection( sel );

			if ( fm.flag_set() )
				m_fePresent.on_transition( NewSelOverlay, sel );
		}

		if ( fm.flag_set() )
			m_fePresent.on_transition( HideOverlay, 0 );
	}

	return sel;
}

// Simple dialog with message and text-entry input
// - Enter Romlist/Emulator/Filter/Display Name
//
bool FeOverlay::edit_dialog(
	const std::string &msg_str,
	std::string &text )
{
	sf::RectangleShape bg = layout_background();
	FeTextPrimitive message = layout_message( LayoutStyle::Top );
	FeTextPrimitive tp = layout_message( LayoutStyle::Bottom );

	tp.setWordWrap( false );

	message.setString( msg_str );

	std::vector<sf::Drawable*> draw_list = { &bg, &message, &tp };

	std::basic_string<std::uint32_t> str = utf8_to_utf32( text );

	FeFlagMinder fm( m_overlay_is_on );

	// NOTE: ShowOverlay and HideOverlay events are  not sent when a
	// script triggers the edit dialog.  This is on purpose.
	//
	if ( edit_loop( draw_list, str, &tp ) )
	{
		text = utf32_to_utf8( str );
		return true;
	}

	return false;
}

// Simple message dialog that awaits keypress
//
void FeOverlay::input_map_dialog(
			const std::string &msg_str,
			FeInputMapEntry &res,
			FeInputMap::Command &conflict )
{
	sf::RectangleShape bg = layout_background();
	FeTextPrimitive message = layout_message( LayoutStyle::Middle );

	message.setString( msg_str );

	// Make sure the appropriate mouse capture variables are set, in case
	// the user has just changed the mouse threshold
	//
	sf::RenderWindow &rwnd = m_wnd.get_win();
	m_feSettings.init_mouse_capture( &rwnd );

	// Centre the mouse in case the user is mapping a mouse move event
	sf::Mouse::setPosition( sf::Vector2i( m_screen_size ) / 2, rwnd );

	// empty the window event queue
	while ( const std::optional ev = m_wnd.pollEvent() )
	{
		// no op
	}

	bool redraw=true;

	// this should only happen from the config dialog
	ASSERT( m_overlay_is_on );

	bool multi_mode=false; // flag if we are checking for multiple inputs.
	bool done=false;

	sf::IntRect mc_rect;
	int joy_thresh;
	m_feSettings.get_input_config_metrics( mc_rect, joy_thresh );

	std::set < std::pair<int,int> > joystick_moves;
	FeInputMapEntry entry;
	sf::Clock timeout;

	const sf::Transform &t = m_fePresent.get_ui_transform();
	while ( m_wnd.isOpen() )
	{
		while ( const std::optional ev = m_wnd.pollEvent() )
		{
			if ( ev.has_value() )
			{
				if ( ev->is<sf::Event::Closed>() )
					return;

				if ( multi_mode && (( ev->is<sf::Event::KeyReleased>() )
						|| ( ev->is<sf::Event::JoystickButtonReleased>() )
						|| ( ev->is<sf::Event::MouseButtonReleased>() )))
					done = true;
				else
				{
					FeInputSingle single( ev.value(), mc_rect, joy_thresh, rwnd.hasFocus() );
					if ( single.get_type() != FeInputSingle::Unsupported )
					{
						if (( ev->is<sf::Event::KeyPressed>() )
								|| ( ev->is<sf::Event::JoystickButtonPressed>() )
								|| ( ev->is<sf::Event::MouseButtonPressed>() ))
							multi_mode = true;
						else if ( const auto* mv = ev->getIf<sf::Event::JoystickMoved>() )
						{
							multi_mode = true;
							joystick_moves.insert( std::pair<int,int>( mv->joystickId, static_cast<int>( mv->axis )));
						}

						bool dup=false;

						std::set< FeInputSingle >::iterator it;
						for ( it = entry.inputs.begin(); it != entry.inputs.end(); ++it )
						{
							if ( (*it) == single )
							{
								dup=true;
								break;
							}
						}

						if ( !dup )
						{
							entry.inputs.insert( single );
							timeout.restart();
						}

						if ( !multi_mode )
							done = true;
					}
					else if ( const auto* mv = ev->getIf<sf::Event::JoystickMoved>() )
					{
						// test if a joystick has been released
						std::pair<int,int> test( mv->joystickId, static_cast<int>( mv->axis ));

						if ( joystick_moves.find( test ) != joystick_moves.end() )
							done = true;
					}
				}
			}
		}

		if ( timeout.getElapsedTime() > sf::seconds( 6 ) )
			done = true;

		if ( done )
		{
			res = entry;
			conflict = m_feSettings.input_conflict_check( entry );
			return;
		}

		if ( m_fePresent.tick() )
			redraw = true;

		if ( redraw || !m_feSettings.get_info_bool( FeSettings::PowerSaving ) )
		{
			m_fePresent.redraw_surfaces();

			m_wnd.clear();
			m_wnd.draw( m_fePresent, t );
			m_wnd.draw( bg, t );
			m_wnd.draw( message, t );
			m_wnd.display();
			redraw = false;
		}
		else
			sf::sleep( sf::milliseconds( 30 ) );
	}
}

bool FeOverlay::config_dialog( int default_sel, FeInputMap::Command extra_exit )
{
	FeConfigMenu m;
	bool settings_changed=false;
	if ( display_config_dialog( &m, settings_changed, default_sel, extra_exit ) < 0 )
		m_wnd.close();

	return settings_changed;
}

bool FeOverlay::edit_game_dialog( int default_sel, FeInputMap::Command extra_exit )
{
	bool settings_changed=false;

	if ( m_feSettings.get_current_display_index() < 0 )
	{
		//
		// If invoked when showing the "displays menu", edit the
		// display that is currently selected
		//
		int index = m_feSettings.get_selected_display_index();

		if ( index >= 0 )
		{
			FeDisplayEditMenu m;
			m.set_display_index( index );

			if ( display_config_dialog( &m, settings_changed, default_sel, extra_exit ) < 0 )
				m_wnd.close();
			else
			{
				// Save the updated settings to disk
				m_feSettings.save();

				// This forces the display to reinitialize with the updated settings
				m_feSettings.set_display( m_feSettings.get_current_display_index() );
			}
		}
	}
	else
	{
		const std::string &emu_name = m_feSettings.get_rom_info( 0, 0, FeRomInfo::Emulator );

		if ( emu_name.compare( 0, 1, "@" ) == 0 )
		{
			// Show shortcut dialog if this is a shortcut...
			//
			FeEditShortcutMenu m;

			if ( display_config_dialog( &m, settings_changed, default_sel, extra_exit ) < 0 )
				m_wnd.close();
		}
		else
		{
			FeEditGameMenu m;

			if ( display_config_dialog( &m, settings_changed, default_sel, extra_exit ) < 0 )
				m_wnd.close();
		}
	}

	// TODO: should only return true when setting_changed is true or when user deleted
	// the rom completely
	return true;
}

bool FeOverlay::layout_options_dialog(
	bool preview,
	int &default_sel,
	FeInputMap::Command extra_exit
)
{
	FeLayoutEditMenu m;

	FeDisplayInfo *display = NULL;
	FeScriptConfigurable *per_display = NULL;
	FeLayoutInfo *layout = NULL;

	int display_index = m_feSettings.get_current_display_index();

	if ( display_index < 0 )
	{
		// Displays Menu
		display = NULL;
		per_display = &m_feSettings.get_display_menu_per_display_params();

		if ( !m_feSettings.has_custom_displays_menu() )
			return false;

		layout = &m_feSettings.get_layout_config( m_feSettings.get_info( FeSettings::MenuLayout ) );
	}
	else
	{
		display = m_feSettings.get_display( display_index );
		per_display = &display->get_layout_per_display_params();
		layout = &m_feSettings.get_layout_config( display->get_info( FeDisplayInfo::Layout ) );
	}

	m.set_layout( layout, per_display, display );
	m.exit_on_change = preview;

	bool settings_changed=false;
	if ( display_config_dialog( &m, settings_changed, default_sel, extra_exit ) < 0 )
		m_wnd.close();

	default_sel = m.last_sel;

	if ( settings_changed )
	{
		// Save the updated settings to disk
		// main.cpp calls load_layout() upon return to show the updated settings
		m_feSettings.save();
	}

	return settings_changed;
}

bool FeOverlay::plugin_options_dialog(
	int &default_sel,
	FeInputMap::Command extra_exit
)
{
	FePlugInfo *plug;
	int plug_index;
	if ( !m_feSettings.get_last_plugin( plug, plug_index ) )
		return false;

	FePluginEditMenu m;
	m.set_plugin( plug, plug_index );
	bool settings_changed = false;

	if ( display_config_dialog( &m, settings_changed, default_sel, extra_exit ) < 0 )
		m_wnd.close();

	default_sel = m.last_sel;

	if ( settings_changed )
		m_feSettings.save();

	return settings_changed;
}

int FeOverlay::display_config_dialog(
	FeBaseConfigMenu *m,
	bool &parent_setting_changed )
{
	return display_config_dialog( m, parent_setting_changed, -1, FeInputMap::LAST_COMMAND );
}

// -------------------------------------------------------------------------------------

// Create a background curtain, used to obscure the layout below
sf::RectangleShape FeOverlay::layout_background()
{
	sf::RectangleShape rect;
	rect.setPosition( m_screen_pos );
	rect.setSize( m_screen_size );
	rect.setFillColor( m_bg_color );
	return rect;
}

// Create a letterbox rectangle, used as header and footer backgrounds.
// Style uses LayoutStyle enum:
// - Top - Position at top of screen
// - Bottom - Position at bottom of screen
sf::RectangleShape FeOverlay::layout_letterbox( int style )
{
	float y;
	if ( style & LayoutStyle::Top ) y = 0;
	if ( style & LayoutStyle::Bottom ) y = m_screen_size.y - m_letterbox_bottom_height;

	int height;
	if ( style & LayoutStyle::Top ) height = m_letterbox_top_height;
	if ( style & LayoutStyle::Bottom ) height = m_letterbox_bottom_height;

	sf::RectangleShape rect;
	rect.setSize( sf::Vector2f( m_screen_size.x, height ) );
	rect.setPosition( m_screen_pos + sf::Vector2f( 0, y ));
	theme_letterbox( rect );
	return rect;
}

// Create a horizontal border line, used for the header and footer (avoid screen overflow of outlines)
// Style uses LayoutStyle enum:
// - Top - Position on top letterbox (bottom edge)
// - Bottom - Position on bottom letterbox (top edge)
sf::RectangleShape FeOverlay::layout_border( int style )
{
	float y;
	if ( style & LayoutStyle::Top ) y = m_letterbox_top_height;
	if ( style & LayoutStyle::Bottom ) y = m_screen_size.y - m_letterbox_bottom_height - m_border_thickness;

	sf::RectangleShape rect;
	rect.setSize( sf::Vector2f( m_screen_size.x, m_border_thickness ) );
	rect.setPosition( m_screen_pos + sf::Vector2f( 0, y ) );
	theme_border( rect );
	return rect;
}

// Create a header text object, containing large text for page titles
// Style uses LayoutStyle enum:
// - Left - Align left, leaving room for logo
// - Centre - Align centre
FeTextPrimitive FeOverlay::layout_header( int style )
{
	int logo_padding;
	if ( style & LayoutStyle::Left ) logo_padding = m_header_char_size + m_letterbox_margin * 2.0;
	if ( style & LayoutStyle::Centre ) logo_padding = 0;

	FeTextPrimitive text;
	text.setFont( *m_font );
	text.setCharacterSize( m_header_char_size );
	text.setPosition( m_screen_pos + sf::Vector2f( logo_padding, 0 ) );
	text.setSize( sf::Vector2f( m_screen_size.x - 2 * logo_padding, m_letterbox_top_height ) );
	text.setMargin( 0 );
	text.setTextScale( m_text_scale );
	text.setColor( m_text_color );
	text.setAlignment( (FeTextPrimitive::Alignment)( FeTextPrimitive::Middle | (( style & LayoutStyle::Centre ) ? FeTextPrimitive::Centre : FeTextPrimitive::Left) ) );
	return text;
}

// Create a footer text object, containing small text for help descriptions
FeTextPrimitive FeOverlay::layout_footer()
{
	int width = std::min( m_screen_size.x, m_screen_size.y / 3.f * 4.f );

	FeTextPrimitive text;
	text.setFont( *m_font );
	text.setCharacterSize( m_footer_char_size );
	text.setWordWrap( true );
	text.setPosition( m_screen_pos + sf::Vector2f( floor( ( m_screen_size.x - width ) / 2.0 ), m_screen_size.y - m_letterbox_bottom_height ) );
	text.setSize( sf::Vector2f( width, m_letterbox_bottom_height ) );
	text.setMargin( m_text_margin );
	text.setTextScale( m_text_scale );
	text.setColor( m_text_color );
	text.setAlignment( static_cast<FeTextPrimitive::Alignment>( FeTextPrimitive::Middle | FeTextPrimitive::Centre ) );
	return text;
}

// Create a message block used for header-less dialogs such as the Exit dialog
// Style uses LayoutStyle enum:
// - Top - Position at top of screen
// - Bottom - Position at bottom of screen
// - Middle - Position at middle of screen
FeTextPrimitive FeOverlay::layout_message( int style )
{
	float x = 0;
	float y;
	float width = m_screen_size.x;
	float height;

	if ( style & LayoutStyle::Top ) height = m_screen_size.y / 2 - m_header_char_size;
	if ( style & LayoutStyle::Bottom ) height = m_screen_size.y / 2 - m_header_char_size;
	if ( style & LayoutStyle::Middle ) height = m_screen_size.y;

	if ( style & LayoutStyle::Top ) y = m_header_char_size;
	if ( style & LayoutStyle::Bottom ) y = m_screen_size.y / 2.0;
	if ( style & LayoutStyle::Middle ) y = 0;

	FeTextPrimitive text;
	text.setFont( *m_font );
	text.setCharacterSize( m_header_char_size );
	text.setWordWrap( true );
	text.setTextScale( m_text_scale );
	text.setColor( m_text_color );
	text.setPosition( m_screen_pos + sf::Vector2f( x, y ) );
	text.setSize( sf::Vector2f( width, height ) );
	return text;
}

// Create the AM Logo sprite, sized to fit the header
// - The texture must be created by the caller to control its lifetime
sf::Sprite FeOverlay::layout_logo( sf::Texture &texture, int style )
{
	texture.setSmooth( true );
	std::ignore = texture.generateMipmap();
	sf::Vector2u logo_size = texture.getSize();
	int margin = ( m_letterbox_top_height - m_header_char_size ) / 2.0;

	float height = m_header_char_size;
	float width = height / logo_size.y * logo_size.x;

	if ( style & LayoutStyle::Large ) {
		width = std::min( m_screen_size.x, m_screen_size.y ) * 0.55;
		height = width / logo_size.x * logo_size.y;
	}

	float x = m_letterbox_margin;
	float y = margin;
	if ( style & LayoutStyle::Centre ) x = ( m_screen_size.x - width ) / 2.0;
	if ( style & LayoutStyle::Middle ) y = ( m_screen_size.y - height ) / 2.0;

	sf::Sprite sprite( texture );
	sprite.setScale( sf::Vector2f( width / logo_size.x, height / logo_size.y ) );
	sprite.setPosition( m_screen_pos + sf::Vector2f( x, y ) );
	return sprite;
}

// Create a list index overlay for use with LayoutPreview lists
// Style uses LayoutStyle enum:
// - Left - Position on left of screen
// - Right - Position on right of screen
FeTextPrimitive FeOverlay::layout_index( int style )
{
	int charsize = m_list_char_size;

	int height = list_row_height( style );

	int width = m_screen_size.x;
	if ( style & LayoutStyle::Left ) width = floor( m_screen_size.x / 2.0 );
	if ( style & LayoutStyle::Right ) width = ceil( m_screen_size.x / 2.0 );

	int x = 0;
	if ( style & LayoutStyle::Right ) x = floor( m_screen_size.x / 2.0 );

	int y = m_screen_size.y - m_letterbox_bottom_height - height;

	FeTextPrimitive text;
	text.setFont( *m_font );
	text.setCharacterSize( charsize );
	text.setPosition( m_screen_pos + sf::Vector2f( x, y ) );
	text.setSize( sf::Vector2f( width, height ) );
	text.setMargin( m_text_margin );
	text.setTextScale( m_text_scale );
	text.setColor( m_text_color );
	text.setAlignment( (FeTextPrimitive::Alignment)( FeTextPrimitive::Middle | FeTextPrimitive::Left ) );
	return text;
}

// Create a list object for use in Config, Tag, Filter, Display, and Confirm dialogs
// Style uses LayoutStyle enum:
// - Large - Use large rows
// - Left - Position on left of screen (label list)
// - Right - Position on right of screen (value list)
// - Full - Position with no header or footer
// - Body - Position with header only
// - Middle - Position with header and footer
// - Bottom - Position at bottom of screen (for Confirm dialog)
// - Single - Single row positioned above footer (for LayoutPreview)
FeListBox FeOverlay::layout_list( int style )
{
	int h1 = m_letterbox_top_height + m_border_thickness;
	int h2 = m_letterbox_bottom_height + m_border_thickness;
	int charsize = ( style & LayoutStyle::Large ) ? m_header_char_size : m_list_char_size;

	int body_height;
	if ( style & LayoutStyle::Full ) body_height = m_screen_size.y;
	if ( style & LayoutStyle::Body ) body_height = m_screen_size.y - h1;
	if ( style & LayoutStyle::Middle ) body_height = m_screen_size.y - h1 - h2;
	if ( style & LayoutStyle::Bottom ) body_height = m_screen_size.y / 2.0 - m_header_char_size;

	int body_top;
	if ( style & LayoutStyle::Full ) body_top = 0;
	if ( style & LayoutStyle::Body ) body_top = h1;
	if ( style & LayoutStyle::Middle ) body_top = h1;
	if ( style & LayoutStyle::Bottom ) body_top = m_screen_size.y / 2.0;

	int row_height = list_row_height( style );
	int rows = body_height / row_height;
	if ( style & LayoutStyle::Single ) rows = 1; // Layout Preview
	if ( style & LayoutStyle::Bottom ) rows = 3; // Confirm Yes/No

	int height = row_height * rows;

	int width = m_screen_size.x;
	if ( style & LayoutStyle::Left ) width = floor( m_screen_size.x / 2.0 );
	if ( style & LayoutStyle::Right ) width = ceil( m_screen_size.x / 2.0 );

	int x = 0;
	if ( style & LayoutStyle::Right ) x = floor( m_screen_size.x / 2.0 );

	if ( style & LayoutStyle::Single )
	{
		// Single item list ignores the border, otherwise it appears too high
		body_top = m_screen_size.y - m_letterbox_bottom_height - height;
		body_height = height;
	}
	int y = body_top + ( body_height - height ) / 2.0;

	FePresentableParent temp;
	FeListBox list( temp );
	list.setFont( *m_font );
	list.set_charsize( charsize );
	list.set_align( FeTextPrimitive::Middle | FeTextPrimitive::Centre );
	list.set_margin( m_text_margin );
	list.setTextScale( m_text_scale );
	list.set_rows( rows );
	list.setPosition( m_screen_pos + sf::Vector2f( x, y ) );
	list.setSize( sf::Vector2f( width, height ) );
	list.init_dimensions();
	theme_list( list );
	return list;
}

void FeOverlay::theme_letterbox( sf::RectangleShape &rect )
{
	rect.setFillColor( m_letterbox_color );
}

void FeOverlay::theme_border( sf::RectangleShape &rect )
{
	rect.setFillColor( m_border_color );
}

void FeOverlay::theme_list( FeListBox &list )
{
	list.setColor( m_text_color );
	list.setBgColor( sf::Color::Transparent );
	list.setSelColor( m_text_color );
	list.setSelBgColor( m_focus_color );
}

// Return row height for given style
// - maximize row height to fit Tags dialog if large, Config dialog otherwise
int FeOverlay::list_row_height( int style )
{
	int h1 = m_letterbox_top_height + m_border_thickness;
	int h2 = m_letterbox_bottom_height + m_border_thickness;
	int charsize = ( style & LayoutStyle::Large ) ? m_header_char_size : m_list_char_size;
	int min_row_height = charsize * m_row_height_scale;
	int body_height = m_screen_size.y - h1 - h2;
	if ( style & LayoutStyle::Large ) body_height = m_screen_size.y - h1;
	int rows = floor( body_height / min_row_height );
	if ( FORCE_ODD_ROWS && ( rows % 2 == 0 )) rows--;
	return body_height / rows;
}

// Update the given lists to place focus on the label or the value, "focus" may be:
// - LayoutFocus::Select - The label is given focus
// - LayoutFocus::Edit - The value is given focus
// - LayoutFocus::Disabled - The value is disabled
bool FeOverlay::layout_focus(
	FeListBox &label_list,
	FeListBox &value_list,
	int focus )
{
	bool edit = focus & LayoutFocus::Edit;
	bool disabled = focus & LayoutFocus::Disabled;
	label_list.setSelBgColor( edit ? m_blur_color : m_focus_color );
	label_list.set_a( edit ? m_disable_alpha : m_enable_alpha );
	value_list.setSelBgColor( edit ? m_focus_color : m_blur_color );
	value_list.set_sela( disabled ? m_disable_alpha : m_enable_alpha );
	return true;
}

// Update text to match list index
// - If the selected list string overlaps the overlay
// - Hide list text and show content in overlay instead
bool FeOverlay::text_index(
	FeListBox &list,
	FeTextPrimitive &text,
	std::vector<std::string> &values,
	int index
)
{
	if ( index < 0 )
	{
		text.setString( "" );
		list.set_sela( m_enable_alpha );
		return true;
	}

	// Find the width of the element
	int width = text.getSize().x - text.getMargin() * 2;

	// Find the width of the index string
	std::string prefix = "↕ " + as_str( index ) + "  ";
	text.setString( prefix );
	int w1 = text.getActualWidth();

	// Find the width of the list string
	std::string val = values[index];
	text.setString( val );
	int w2 = text.getActualWidth();

	// Hide list text and show with index if it overlaps
	bool fits = ( w1 + w2 ) <= width;
	text.setString( prefix + ( fits ? "" : val ) );
	list.set_sela( fits ? m_enable_alpha : 0 );

	return true;
}

// -------------------------------------------------------------------------------------

// Configuration menu
// - Selection list (front page)
// - Option/Value list (sub pages)
//
int FeOverlay::display_config_dialog(
	FeBaseConfigMenu *m,
	bool &parent_setting_changed,
	int default_sel,
	FeInputMap::Command extra_exit )
{
	style_init();
	FeConfigContextImp ctx( m_feSettings, *this );
	ctx.update_to_menu( m );

	bool is_edit = ctx.style == FeConfigContext::EditList;
	bool is_preview = m->exit_on_change;

	//
	// Set up display objects
	//
	sf::RectangleShape bg = layout_background();
	sf::RectangleShape letterbox_top = layout_letterbox( LayoutStyle::Top );
	sf::RectangleShape letterbox_bottom = layout_letterbox( LayoutStyle::Bottom );
	sf::RectangleShape border_top = layout_border( LayoutStyle::Top );
	sf::RectangleShape border_bottom = layout_border( LayoutStyle::Bottom );
	FeTextPrimitive header = layout_header( LayoutStyle::Left );
	FeTextPrimitive footer = layout_footer();
	sf::Texture logo_texture = sf::Texture( *(FePresent::script_get_fep()->get_logo_image()) );
	sf::Sprite logo = layout_logo( logo_texture );
	FeListBox sdialog = layout_list(( is_edit ? LayoutStyle::Left : LayoutStyle::Centre ) | ( is_preview ? LayoutStyle::Single : LayoutStyle::Middle ));
	FeListBox vdialog = layout_list(( is_edit ? LayoutStyle::Right : LayoutStyle::Centre ) | ( is_preview ? LayoutStyle::Single : LayoutStyle::Middle ));
	FeTextPrimitive sindex = layout_index( LayoutStyle::Left );
	FeTextPrimitive vindex = layout_index( LayoutStyle::Right );

	std::vector<sf::Drawable*> draw_list;
	if ( !is_preview ) draw_list.push_back( &bg );
	draw_list.push_back( &sdialog );
	if ( is_edit ) draw_list.push_back( &vdialog );
	if ( is_edit && is_preview ) draw_list.push_back( &sindex );
	if ( is_edit && is_preview ) draw_list.push_back( &vindex );
	if ( !is_preview ) draw_list.push_back( &letterbox_top );
	if ( !is_preview ) draw_list.push_back( &border_top );
	if ( !is_preview ) draw_list.push_back( &header );
	if ( !is_preview ) draw_list.push_back( &logo );
	draw_list.push_back( &letterbox_bottom );
	draw_list.push_back( &border_bottom );
	draw_list.push_back( &footer );

	header.setString( ctx.title );

	// A passed selection will override the context default_sel - useful when reloading the menu
	ctx.curr_sel = default_sel >= 0 ? default_sel : ctx.default_sel;
	if ( ctx.curr_sel >= (int)ctx.left_list.size() )
		ctx.curr_sel = 0;
	m->last_sel = ctx.curr_sel;

	sdialog.setCustomText( ctx.curr_sel, ctx.left_list );
	vdialog.setCustomText( ctx.curr_sel, ctx.right_list );

	if ( !ctx.help_msg.empty() )
	{
		footer.setString( ctx.help_msg );
		ctx.help_msg = "";
	}
	else
	{
		footer.setString( ctx.curr_opt().help_msg );
	}

	FeFlagMinder fm( m_overlay_is_on );

	//
	// Event loop processing
	//
	while ( true )
	{
		FeEventLoopCtx c( draw_list, ctx.curr_sel, ctx.exit_sel, ctx.left_list.size() );
		c.extra_exit = extra_exit;
		c.page_size = sdialog.get_rows() / 2;

		init_event_loop( c );
		text_index( vdialog, vindex, ctx.right_list, -1 );
		layout_focus( sdialog, vdialog, ( ctx.curr_opt().type == Opt::INFO ) ? LayoutFocus::Disabled : LayoutFocus::Select );

		while ( text_index( sdialog, sindex, ctx.left_list, is_preview ? c.sel : -1 ) && event_loop( c ) == false )
		{
			m->last_sel = ctx.curr_sel;

			footer.setString( ctx.curr_opt().help_msg );

			// we reset the entire Text because edit mode may
			// have changed our list contents
			//
			layout_focus( sdialog, vdialog, ( ctx.curr_opt().type == Opt::INFO ) ? LayoutFocus::Disabled : LayoutFocus::Select );

			sdialog.setCustomText( ctx.curr_sel, ctx.left_list );
			vdialog.setCustomText( ctx.curr_sel, ctx.right_list );
		}

		text_index( sdialog, sindex, ctx.left_list, -1 );

		//
		// User selected something, process it
		//
		if ( ctx.curr_sel < 0 ) // exit
		{
			if ( ctx.save_req )
			{
				if ( m->save( ctx ) )
					parent_setting_changed = true;
			}

			return ctx.curr_sel;
		}

		FeBaseConfigMenu *sm( NULL );
		if ( m->on_option_select( ctx, sm ) == false )
			continue;

		if ( ctx.update_req )
		{
			// update the menu to match the current options
			// - called when layoutEdit is_function is selected
			// - resets lists rather than calling display_config_dialog to preserve help msg
			ctx.update_req = false;
			ctx.update_to_menu( m );
			sdialog.setCustomText( ctx.curr_sel, ctx.left_list );
			vdialog.setCustomText( ctx.curr_sel, ctx.right_list );
			footer.setString( ctx.curr_opt().help_msg );
		}

		if ( !ctx.help_msg.empty() )
		{
			footer.setString( ctx.help_msg );
			ctx.help_msg = "";
		}

		int t = ctx.curr_opt().type;
		if ( t == Opt::LIST && is_bool_list( ctx.curr_opt().values_list ) )
			t = Opt::TOGGLE;

		switch ( t )
		{
		case Opt::MENU:
		case Opt::RELOAD:
		case Opt::EXIT:
		case Opt::DEFAULTEXIT:
		{
			if ( ctx.save_req )
			{
				if ( t != Opt::EXIT )
				{
					if ( m->save( ctx ) )
						parent_setting_changed = true;

					ctx.save_req = false;
				}
				else
					parent_setting_changed = true;
			}

			switch ( t )
			{
			case Opt::RELOAD:
				return display_config_dialog( m, parent_setting_changed, ctx.curr_sel, extra_exit );
			case Opt::MENU:
				if ( sm )
				{
					bool submenu_setting_changed( false );
					int sm_ret = display_config_dialog( sm, submenu_setting_changed, -1, extra_exit );
					if ( sm_ret < 0 ) return sm_ret;

					if ( submenu_setting_changed )
					{
						ctx.save_req = true;
						if ( m->save( ctx ) ) parent_setting_changed = true;
					}

					// The submenu may have modified stuff in this menu - reload to display changes
					return display_config_dialog( m, parent_setting_changed, ctx.curr_sel, extra_exit );
				}
				break;
			case Opt::EXIT:
			case Opt::DEFAULTEXIT:
			default:
				return ctx.curr_sel;
			}
			break;
		}
		case Opt::EDIT:
		case Opt::LIST:
		{
			//
			// User has selected to edit a specific entry.
			// Update the UI and enter the appropriate event loop
			//
			layout_focus( sdialog, vdialog, LayoutFocus::Edit );

			FeTextPrimitive *tp;
			if ( !vdialog.getSelectedText( tp ) )
				continue;

			if ( t == Opt::LIST )
			{
				if ( !ctx.curr_opt().values_list.empty() )
				{
					int original_value = ctx.curr_opt().get_vindex();
					int new_value = original_value;
					bool refresh_colour = ctx.opt_list[ctx.curr_sel].trigger_colour;

					FeEventLoopCtx c( draw_list, new_value, -1, ctx.curr_opt().values_list.size() );
					c.extra_exit = extra_exit;
					c.page_size = sdialog.get_rows() / 2;

					vdialog.setCustomText( new_value, ctx.curr_opt().values_list );

					init_event_loop( c );
					while ( text_index( vdialog, vindex, ctx.curr_opt().values_list, is_preview ? c.sel : -1 ) && event_loop( c ) == false )
					{
						vdialog.setCustomText( new_value, ctx.curr_opt().values_list );

						// Special case for menu colour option only
						if ( refresh_colour )
						{
							sf::Color col;
							str_to_color( FeSettings::uiColorTokens[ c.sel ], col );

							style_init( col );
							theme_letterbox( letterbox_top );
							theme_letterbox( letterbox_bottom );
							theme_border( border_top );
							theme_border( border_bottom );
							theme_list( sdialog );
							theme_list( vdialog );
							layout_focus( sdialog, vdialog, LayoutFocus::Edit );
						}
					}

					if (( new_value >= 0 ) && ( new_value != original_value ))
					{
						ctx.save_req = true;
						ctx.curr_opt().set_value( new_value );
						ctx.right_list[ctx.curr_sel] = ctx.curr_opt().get_value();
					}
					else if ( refresh_colour )
					{
						style_init();
						theme_letterbox( letterbox_top );
						theme_letterbox( letterbox_bottom );
						theme_border( border_top );
						theme_border( border_bottom );
						theme_list( sdialog );
						theme_list( vdialog );
						layout_focus( sdialog, vdialog, LayoutFocus::Edit );
					}

					vdialog.setCustomText( ctx.curr_sel, ctx.right_list );
				}
			}
			else
			{
				const std::string &e_str( ctx.curr_opt().get_value() );
				std::basic_string<std::uint32_t> str = utf8_to_utf32( e_str );

				if ( edit_loop( draw_list, str, tp ) == true )
				{
					ctx.save_req = true;

					std::string d_str = utf32_to_utf8( str );
					ctx.curr_opt().set_value( d_str );
					ctx.right_list[ctx.curr_sel] = d_str;
				}
			}

			tp->setString( ctx.right_list[ctx.curr_sel] );
			layout_focus( sdialog, vdialog, LayoutFocus::Select);

			// If trigger_reload, save & reload to show ui changes instantly
			if ( ctx.save_req )
			{
				if ( m->exit_on_change )
				{
					// Save & return so caller can apply changes
					if ( m->save( ctx ) ) parent_setting_changed = true;
					return ctx.curr_sel;
				}
				else if ( ctx.opt_list[ctx.curr_sel].trigger_reload )
				{
					// Save & reload to show ui changes
					if ( m->save( ctx ) ) parent_setting_changed = true;
					return display_config_dialog( m, parent_setting_changed, ctx.curr_sel, extra_exit );
				}
			}
			break;
		}
		case Opt::TOGGLE:
		{
			int original_value = ctx.curr_opt().get_vindex();
			int new_value = ( original_value == 0 ) ? 1 : 0;
			ctx.curr_opt().set_value( new_value );
			ctx.right_list[ ctx.curr_sel ] = get_pill_glyph( is_truthy( ctx.curr_opt().values_list[new_value] ) );

			vdialog.setCustomText( ctx.curr_sel, ctx.right_list );
			layout_focus( sdialog, vdialog, LayoutFocus::Edit );

			ctx.save_req = true;

			if ( m->exit_on_change )
			{
				if ( m->save( ctx ) ) parent_setting_changed = true;
				return ctx.curr_sel;
			}
			else if ( ctx.opt_list[ ctx.curr_sel ].trigger_reload )
			{
				if ( m->save( ctx ) ) parent_setting_changed = true;
				return display_config_dialog( m, parent_setting_changed, ctx.curr_sel, extra_exit );
			}
			break;
		}
		default:
			break;
		}
	}
	return ctx.curr_sel;
}

bool FeOverlay::check_for_cancel()
{
	while ( const std::optional ev = m_wnd.pollEvent() )
	{
		if ( ev.has_value() )
		{
			FeInputMap::Command c = m_feSettings.map_input( ev );

			if (( c == FeInputMap::Back )
					|| ( c == FeInputMap::Exit )
					|| ( c == FeInputMap::ExitToDesktop ))
				return true;
		}
	}

	return false;
}

void FeOverlay::init_event_loop( FeEventLoopCtx &ctx )
{
	//
	// Make sure the Back and Select buttons are NOT down, to avoid immediately
	// triggering an exit/selection
	//
	const sf::Transform &t = m_fePresent.get_ui_transform();

	sf::Clock timer;
	while (( timer.getElapsedTime() < sf::seconds( 6 ) )
			&& ( m_feSettings.get_current_state( FeInputMap::Back )
				|| m_feSettings.get_current_state( FeInputMap::ExitToDesktop )
				|| m_feSettings.get_current_state( FeInputMap::Select ) ))
	{
		while ( const std::optional ev = m_wnd.pollEvent() )
		{
		}

		if ( m_fePresent.tick() )
		{
			m_fePresent.redraw_surfaces();
			m_wnd.clear();
			m_wnd.draw( m_fePresent, t );

			for ( std::vector<sf::Drawable *>::const_iterator itr=ctx.draw_list.begin();
					itr < ctx.draw_list.end(); ++itr )
				m_wnd.draw( *(*itr), t );

			m_wnd.display();
		}
		else
			sf::sleep( sf::milliseconds( 30 ) );
	}
}

//
// Return true if the user selected something.  False if they have simply
// navigated the selection up or down.
//
bool FeOverlay::event_loop( FeEventLoopCtx &ctx )
{
	const sf::Transform &t = m_fePresent.get_ui_transform();

	clear_menu_command();
	bool quick_menu = m_feSettings.get_info_bool( FeSettings::QuickMenu );
	bool redraw=true;

	while ( m_wnd.isOpen() )
	{
		while ( const std::optional ev = m_wnd.pollEvent() )
		{
			if ( ev.has_value() )
			{
				FePresent::script_get_fep()->reset_screen_saver();

				FeInputMap::Command c = m_feSettings.map_input( ev );

				if (( c != FeInputMap::LAST_COMMAND )
						&& ( c == ctx.extra_exit ))
					c = FeInputMap::Exit;

				if ( m_feSettings.test_mouse_wrap() ) m_feSettings.wrap_mouse();

				switch( c )
				{
				case FeInputMap::Configure:
					if ( !quick_menu ) break;
					m_menu_command = c;
					ctx.sel = ctx.default_sel;
					return true;
				case FeInputMap::ToggleTags:
				case FeInputMap::DisplaysMenu:
				case FeInputMap::FiltersMenu:
				case FeInputMap::LayoutOptions:
				case FeInputMap::EditGame:
				case FeInputMap::InsertGame:
				case FeInputMap::ToggleFavourite:
					if ( !quick_menu || ctx.extra_exit == FeInputMap::Configure ) break;
					m_menu_command = c;
					ctx.sel = ctx.default_sel;
					return true;
				case FeInputMap::Exit:
					if ( !quick_menu ) break;
					ctx.sel = ctx.default_sel;
					return true;
				case FeInputMap::Back:
					ctx.sel = ctx.default_sel;
					return true;
				case FeInputMap::ExitToDesktop:
					ctx.sel = FeOverlay::ExitToDesktop;
					return true;
				case FeInputMap::Select:
					return true;
				case FeInputMap::PrevPage:
					if ( ev->is<sf::Event::JoystickMoved>() && ctx.move_event.has_value() && ctx.move_event->is<sf::Event::JoystickMoved>() )
						return false;

					ctx.sel -= ctx.page_size;
					ctx.sel = ev->is<sf::Event::MouseMoved>()
						? std::max( ctx.sel, 0 )
						: (ctx.sel + ctx.list_size) % ctx.list_size;

					ctx.move_start( ev, c );
					return false;

				case FeInputMap::NextPage:
					if ( ev->is<sf::Event::JoystickMoved>() && ctx.move_event.has_value() && ctx.move_event->is<sf::Event::JoystickMoved>() )
						return false;

					ctx.sel += ctx.page_size;
					ctx.sel = ev->is<sf::Event::MouseMoved>()
						? std::min( ctx.sel, ctx.list_size - 1 )
						: (ctx.sel + ctx.list_size) % ctx.list_size;

					ctx.move_start( ev, c );
					return false;

				case FeInputMap::Up:
					if ( ev->is<sf::Event::JoystickMoved>() && ctx.move_event.has_value() && ctx.move_event->is<sf::Event::JoystickMoved>() )
						return false;

					ctx.sel -= 1;
					ctx.sel = ev->is<sf::Event::MouseMoved>()
						? std::max( ctx.sel, 0 )
						: (ctx.sel + ctx.list_size) % ctx.list_size;

					ctx.move_start( ev, c );
					return false;

				case FeInputMap::Down:
					if ( ev->is<sf::Event::JoystickMoved>() && ctx.move_event.has_value() && ctx.move_event->is<sf::Event::JoystickMoved>() )
						return false;

					ctx.sel += 1;
					ctx.sel = ev->is<sf::Event::MouseMoved>()
						? std::min( ctx.sel, ctx.list_size - 1 )
						: (ctx.sel + ctx.list_size) % ctx.list_size;

					ctx.move_start( ev, c );
					return false;

				default:
					break;
				}
			}
		}

		if ( m_fePresent.tick() )
			redraw = true;

		if ( redraw || !m_feSettings.get_info_bool( FeSettings::PowerSaving ) )
		{
			m_fePresent.redraw_surfaces();
			m_wnd.clear();
			m_wnd.draw( m_fePresent, t );

			for ( std::vector<sf::Drawable *>::const_iterator itr=ctx.draw_list.begin();
					itr < ctx.draw_list.end(); ++itr )
				m_wnd.draw( *(*itr), t );

			m_wnd.display();
			redraw = false;
		}
		else
			sf::sleep( sf::milliseconds( 30 ) );

		if ( ctx.move_command != FeInputMap::LAST_COMMAND )
		{
			bool cont=false;

			if ( const auto* key = ctx.move_event->getIf<sf::Event::KeyPressed>() )
			{
				if ( key && sf::Keyboard::isKeyPressed( key->code ) )
					cont=true;
			}

			else if ( const auto* btn = ctx.move_event->getIf<sf::Event::MouseButtonPressed>() )
			{
				if ( btn && sf::Mouse::isButtonPressed( btn->button ) )
					cont=true;
			}

			else if ( const auto* btn = ctx.move_event->getIf<sf::Event::JoystickButtonPressed>() )
			{
				if ( sf::Joystick::isButtonPressed( btn->joystickId, btn->button ) )
					cont=true;
			}

			else if ( const auto* mov = ctx.move_event->getIf<sf::Event::JoystickMoved>() )
			{
				{
					float pos = sf::Joystick::getAxisPosition( mov->joystickId, mov->axis );
					if ( std::abs( pos ) > m_feSettings.get_joy_thresh() )
						cont=true;
				}
			}

			if ( cont )
			{
				int t = ctx.move_timer.getElapsedTime().asMilliseconds();
				if ( t > m_feSettings.selection_delay() + ctx.move_count * m_feSettings.selection_speed() )
				{
					int last_sel = ctx.sel;
					switch ( ctx.move_command )
					{
						case FeInputMap::PrevPage:
							if ( ctx.sel > 0 )
								ctx.sel = std::max( ctx.sel - ctx.page_size, 0 );
							break;
						case FeInputMap::NextPage:
							if ( ctx.sel < ctx.list_size - 1 )
								ctx.sel = std::min( ctx.sel + ctx.page_size, ctx.list_size - 1 );
							break;
						case FeInputMap::Up:
							if ( ctx.sel > 0 )
								ctx.sel--;
							break;
						case FeInputMap::Down:
							if ( ctx.sel < ctx.list_size - 1 )
								ctx.sel++;
							break;
						default:
							break;
					}

					if ( ctx.sel != last_sel )
					{
						ctx.move_count++;
						return false;
					}
				}
			}
			else
			{
				ctx.move_command = FeInputMap::LAST_COMMAND;
				ctx.move_event = std::nullopt;
			}
		}
	}
	return true;
}

class FeKeyRepeat
{
private:
	sf::RenderWindow &m_wnd;
public:
	FeKeyRepeat( sf::RenderWindow &wnd )
	: m_wnd( wnd )
	{
		m_wnd.setKeyRepeatEnabled( true );
	}

	~FeKeyRepeat()
	{
		m_wnd.setKeyRepeatEnabled( false );
	}
};

bool FeOverlay::edit_loop( std::vector<sf::Drawable *> d,
			std::basic_string<std::uint32_t> &str, FeTextPrimitive *tp )
{
	sf::Clock cursor_timer;
	const sf::Transform &t = m_fePresent.get_ui_transform();

	const sf::Font *font = tp->getFont();
	sf::Text cursor( *font,"|", tp->getCharacterSize() / tp->getTextScale().x );
	cursor.setFillColor( tp->getColor() );
	cursor.setScale( tp->getTextScale() );

	int cursor_pos=str.size();
	cursor.setPosition({ static_cast<float>( tp->setString( str, cursor_pos ).x - std::floor( cursor.getLocalBounds().size.x / 2.0 + cursor.getLocalBounds().position.x ) * cursor.getScale().x ), 0 }); // x
	cursor.setPosition({ cursor.getPosition().x, static_cast<float>( std::floor( tp->getPosition().y + ( tp->getSize().y + tp->getGlyphSize() - tp->getCharacterSize() * 2 + 0.5 ) / 2.0 ))}); // y

	bool redraw=true;
	FeKeyRepeat key_repeat_enabler( m_wnd.get_win() );

	std::optional<sf::Event> joy_guard;
	bool did_delete( false ); // flag if the user just deleted a character using the UI controls

	while ( m_wnd.isOpen() )
	{
		while ( const std::optional ev = m_wnd.pollEvent() )
		{
			if ( ev.has_value() )
			{
				if ( ev->is<sf::Event::Closed>() )
					return false;

				else if ( const auto* txt = ev->getIf<sf::Event::TextEntered>() )
				{

					did_delete = false;

					if ( txt->unicode == 8 ) // backspace
					{
						if ( cursor_pos > 0 )
						{
							str.erase( cursor_pos - 1, 1 );
							cursor_pos--;
						}
						redraw = true;
					}
					else if (( txt->unicode < 32 ) || ( txt->unicode == 127 ))
					{
						//
						// Don't respond to control characters < 32 (line feeds etc.)
						// and don't handle 127 (delete) here, it is dealt with as a keypress
						//
					}
					else
					{
						if ( cursor_pos < (int)str.size() )
							str.insert( cursor_pos, 1, txt->unicode );
						else
							str += txt->unicode;

						cursor_pos++;
						redraw = true;
					}
				}

				else if ( const auto* key = ev->getIf<sf::Event::KeyPressed>() )
				{
					did_delete = false;

					switch ( key->code )
					{
					case sf::Keyboard::Key::Left:
						if ( cursor_pos > 0 )
							cursor_pos--;

						redraw = true;
						break;

					case sf::Keyboard::Key::Right:
						if ( cursor_pos < (int)str.size() )
							cursor_pos++;

						redraw = true;
						break;

					case sf::Keyboard::Key::Enter:
						return true;

					case sf::Keyboard::Key::Escape:
						return false;

					case sf::Keyboard::Key::End:
						cursor_pos = str.size();
						redraw = true;
						break;

					case sf::Keyboard::Key::Home:
						cursor_pos = 0;
						redraw = true;
						break;

					case sf::Keyboard::Key::Delete:
						if ( cursor_pos < (int)str.size() )
							str.erase( cursor_pos, 1 );

						redraw = true;
						break;

					case sf::Keyboard::Key::V:
	#ifdef SFML_SYSTEM_MACOS
						if ( key->system )
	#else
						if ( key->control )
	#endif
						{
							std::basic_string<std::uint32_t> temp = utf8_to_utf32( clean_str( clipboard_get_content() ) );
							str.insert( cursor_pos, temp.c_str() );
							cursor_pos += temp.length();
						}
						redraw = true;
						break;

					default:
						break;
					}
				}
				else
				{
					//
					// Handle UI actions from non-keyboard input
					//
					{
						FeInputMap::Command c = m_feSettings.map_input( ev );

						switch ( c )
						{
						case FeInputMap::Left:
							if (( ev->is<sf::Event::JoystickMoved>() )
									&& ( joy_guard.has_value() && joy_guard->is<sf::Event::JoystickMoved>() ))
								break;

							did_delete = false;

							if ( cursor_pos > 0 )
								cursor_pos--;

							redraw = true;

							if ( ev->is<sf::Event::JoystickMoved>() )
								joy_guard = ev;
							break;

						case FeInputMap::Right:
							if (( ev->is<sf::Event::JoystickMoved>() )
									&& ( joy_guard.has_value() && joy_guard->is<sf::Event::JoystickMoved>() ))
								break;

							did_delete = false;

							if ( cursor_pos < (int)str.size() )
								cursor_pos++;

							redraw = true;

							if ( ev->is<sf::Event::JoystickMoved>() )
								joy_guard = ev;
							break;

						case FeInputMap::Up:
							if (( ev->is<sf::Event::JoystickMoved>() )
									&& ( joy_guard.has_value() && joy_guard->is<sf::Event::JoystickMoved>() ))
								break;

							if ( cursor_pos < (int)str.size() )
							{
								if ( did_delete )
								{
									str.insert( cursor_pos, 1, my_char_table[0] );
									redraw = true;
									did_delete = false;
								}
								else
								{
									int idx = get_char_idx( str[cursor_pos] );

									if ( my_char_table[idx+1] )
									{
										str[cursor_pos] = my_char_table[idx+1];
										redraw = true;
										did_delete = false;
									}
								}
							}
							else // allow inserting new characters at the end by pressing Up
							{
								str += my_char_table[0];
								redraw = true;
								did_delete = false;
							}

							if ( ev->is<sf::Event::JoystickMoved>() )
								joy_guard = ev;
							break;

						case FeInputMap::Down:
							if (( ev->is<sf::Event::JoystickMoved>() )
									&& ( joy_guard.has_value() && joy_guard->is<sf::Event::JoystickMoved>() ))
								break;

							if ( did_delete ) // force user to do something else to confirm delete
								break;

							if ( cursor_pos < (int)str.size() )
							{
								int idx = get_char_idx( str[cursor_pos] );

								if ( idx > 0 )
								{
									str[cursor_pos] = my_char_table[idx-1];
									redraw = true;
								}
								else //if ( idx == 0 )
								{
									// Special case allowing the user to delete
									// a character
									str.erase( cursor_pos, 1 );
									redraw = true;
									did_delete = true;
								}
							}

							if ( ev->is<sf::Event::JoystickMoved>() )
								joy_guard = ev;
							break;

						case FeInputMap::Back:
							return false;

						case FeInputMap::Select:
							return true;
						default:
							break;
						}
					}
				}
			}

			if ( redraw )
			{
				cursor.setPosition({ static_cast<float>( tp->setString( str, cursor_pos ).x - std::floor( cursor.getLocalBounds().size.x / 2.0 + cursor.getLocalBounds().position.x ) * cursor.getScale().x ), 0 }); // x
				cursor.setPosition({ cursor.getPosition().x, static_cast<float>( std::floor( tp->getPosition().y + ( tp->getSize().y + tp->getGlyphSize() - tp->getCharacterSize() * 2 + 0.5 ) / 2.0 ))}); // y
				cursor_timer.restart();
			}
		}

		if ( m_fePresent.tick() )
			redraw = true;

		// When left or right is hold reset the timer, so the cursor isn't blinking
		if ( m_feSettings.get_current_state( FeInputMap::Left ) || m_feSettings.get_current_state( FeInputMap::Right ))
			cursor_timer.restart();

		m_fePresent.redraw_surfaces();
		m_wnd.clear();
		m_wnd.draw( m_fePresent, t );

		for ( std::vector<sf::Drawable *>::iterator itr=d.begin();
				itr < d.end(); ++itr )
			m_wnd.draw( *(*itr), t );

		int ms = cursor_timer.getElapsedTime().asMilliseconds();
		int cursor_fade = std::clamp( sin( ms / 500.0 * M_PI ) * 2.0 + 1.0, 0.0, 1.0 ) * 255;

		cursor.setFillColor( m_text_color * sf::Color( 255, 255, 255, cursor_fade ));

		m_wnd.draw( cursor, t );
		m_wnd.display();

		if ( !redraw && m_feSettings.get_info_bool( FeSettings::PowerSaving ) )
			sf::sleep( sf::milliseconds( 30 ) );

		redraw = false;

		//
		// Check if previous joystick move is now done (in which case we clear the guard)
		//
		if ( joy_guard.has_value() && joy_guard->is<sf::Event::JoystickMoved>() )
		{
			const auto* mov = joy_guard->getIf<sf::Event::JoystickMoved>();
			if ( mov )
			{
				float pos = sf::Joystick::getAxisPosition( mov->joystickId, mov->axis );

				if ( std::abs( pos ) < m_feSettings.get_joy_thresh() )
					joy_guard = std::nullopt;
			}
		}

	}
	return true;
}

bool FeOverlay::common_exit()
{
	if ( !m_feSettings.get_info_bool( FeSettings::ConfirmExit ) )
	{
		m_feSettings.exit_command();
		return true;
	}

	std::string exit_msg;
	m_feSettings.get_exit_question( exit_msg );

	int retval = confirm_dialog( exit_msg );

	//
	// retval is 0 if the user confirmed exit.
	// it is <0 if we are being forced to close
	//
	if ( retval < 1 )
	{
		if ( retval == 0 )
			m_feSettings.exit_command();

		return true;
	}

	return false;
}
