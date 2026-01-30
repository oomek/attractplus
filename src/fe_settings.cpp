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

#include "fe_util.hpp"
#include "fe_settings.hpp"
#include "fe_present.hpp"
#include "fe_overlay.hpp"
#include "fe_cache.hpp"
#include "fe_vm.hpp"
#include "image_loader.hpp"
#include "zip.hpp"
#include <iostream>
#include <sstream>
#include "nowide/fstream.hpp"

#include <cstring>
#include <iomanip>
#include <algorithm>
#include <random>
#include <stdlib.h>
#include <cctype>
#include <ctime>

#include "language.h"

#include <SFML/System/Clock.hpp>
#include <SFML/Config.hpp>

#ifndef NO_MOVIE
#include "media.hpp" // for FeMedia::is_supported_media(), get/set_current_decoder()
#endif

#if defined(SFML_SYSTEM_WINDOWS)
const char *FE_DEFAULT_CFG_PATH		= "./";
#elif defined(SFML_SYSTEM_MACOS)
const char *FE_DEFAULT_CFG_PATH		= "$HOME/.attract/";
#elif defined(SFML_SYSTEM_ANDROID)
const char *FE_DEFAULT_CFG_PATH		= "$HOME/";
#else
const char *FE_DEFAULT_CFG_PATH		= "$HOME/.attract/";
#endif

const char *FE_ART_EXTENSIONS[]		=
{
	".png",
	".jpg",
	".jpeg",
	".bmp",
	".tga",
	NULL
};

#ifdef DATA_PATH
const char *FE_DATA_PATH = DATA_PATH;
#else
const char *FE_DATA_PATH = NULL;
#endif

const char *FE_CFG_FILE					= "attract.cfg";
const char *FE_STATE_FILE				= "attract.am";
const char *FE_WINDOW_FILE 				= "window.am";
const char *FE_SCRIPT_NV_FILE 			= "script.nv";
const char *FE_LAYOUT_NV_FILE 			= "layout.nv";
const char *FE_DISPLAYS_FILE			= "displays.cfg";
const char *FE_PLUGINS_FILE				= "plugins.cfg";
const char *FE_LAYOUTS_FILE				= "layouts.cfg";
const char *FE_SCREENSAVER_FILE		= "screensaver.nut";
const char *FE_PLUGIN_FILE				= "plugin.nut";
const char *FE_LOADER_FILE				= "loader.nut";
const char *FE_EMULATOR_INIT_FILE			= "init.nut";
const char *FE_INTRO_FILE				= "intro.nut";
const char *FE_LAYOUT_FILE_BASE		= "layout";
const char *FE_LAYOUT_FILE_EXTENSION	= ".nut";
const char *FE_LANGUAGE_FILE_EXTENSION = ".msg";
const char *FE_PLUGIN_FILE_EXTENSION	= FE_LAYOUT_FILE_EXTENSION;
const char *FE_GAME_EXTRA_FILE_EXTENSION = ".cfg";
const char *FE_GAME_OVERVIEW_FILE_EXTENSION = ".txt";
const char *FE_CFG_SUBDIR				= "config/";
const char *FE_LAYOUT_SUBDIR			= "layouts/";
const char *FE_SOUND_SUBDIR			= "sounds/";
const char *FE_SCREENSAVER_SUBDIR		= "screensaver/";
const char *FE_PLUGIN_SUBDIR 			= "plugins/";
const char *FE_LANGUAGE_SUBDIR		= "language/";
const char *FE_MODULES_SUBDIR			= "modules/";
const char *FE_LOADER_SUBDIR			= "loader/";
const char *FE_INTRO_SUBDIR			= "intro/";
const char *FE_SCRAPER_SUBDIR			= "scraper/";
const char *FE_MENU_ART_SUBDIR		= "menu-art/";
const char *FE_OVERVIEW_SUBDIR		= "overview/";
const char *FE_TEMPLATE_SCRIPT_SUBDIR	= "templates/scripts/";
const char *FE_EMULATOR_SCRIPT_SUBDIR	= "emulators/script/"; // Deprecated, left for migration cleanup
const char *FE_LIST_DEFAULT			= "default-display.cfg";
const char *FE_FILTER_DEFAULT			= "default-filter.cfg";
const char *FE_CFG_YES_STR				= "yes";
const char *FE_CFG_NO_STR				= "no";

const float SECONDS_IN_MINUTE	= 60;
const float SECONDS_IN_HOUR		= 60 * SECONDS_IN_MINUTE;
const float SECONDS_IN_DAY		= 24 * SECONDS_IN_HOUR;
const float SECONDS_IN_WEEK		= 7 * SECONDS_IN_DAY;
const float SECONDS_IN_YEAR		= 365.2425 * SECONDS_IN_DAY; // Accounts for leap year
const float SECONDS_IN_MONTH	= SECONDS_IN_YEAR / 12.0; // Average

const int FE_DEFAULT_UI_COLOR_TOKEN = 1; // Blue

const std::string FE_EMPTY_STRING;

namespace {
	std::mt19937 rnd{ std::random_device{}() };
}

//
// Populates result with valid config path, returns false if no path exists
// - config_path + subdir + name
// - FE_DATA_PATH + subdir + name
//
bool internal_resolve_config_file(
	const std::string &config_path,
	std::string &result,
	const char *subdir,
	const std::string &name )
{
	std::string path;
	if ( subdir ) path += subdir;
	path += name;

	if ( path_exists( config_path + path ) )
	{
		result = config_path + path;
		return true;
	}

	if ( FE_DATA_PATH != NULL && path_exists( FE_DATA_PATH + path ) )
	{
		result = FE_DATA_PATH + path;
		return true;
	}

	return false;
}

int find_idx_in_vec( int idx, const std::vector<int> &vec )
{
	int i=0;
	for ( i=0; i < (int)vec.size() - 1; i++ )
	{
		if ( idx <= vec[i] )
			break;
	}

	return i;
}

FeLanguage::FeLanguage(
	const std::string &language,
	const std::string &label):
	language( language ),
	label( label )
{
}

const char *FeSettings::windowModeTokens[] =
{
	"fillscreen",
	"fullscreen",
	"window",
	"window_no_border",
	NULL
};

const char *FeSettings::windowModeDispTokens[] =
{
	"Fill Screen", // Default
	"Fullscreen",
	"Window",
	"Borderless Window",
	NULL
};

const char *FeSettings::screenRotationTokens[] =
{
	"none",
	"right",
	"flip",
	"left",
	NULL
};

const char *FeSettings::screenRotationDispTokens[] =
{
	"None", // Default
	"Right",
	"Flip",
	"Left",
	NULL
};

const char *FeSettings::antialiasingTokens[] =
{
	"0",
	"2",
	"4",
	"8",
	NULL
};

const char *FeSettings::antialiasingDispTokens[] =
{
	"None", // Default
	"x2",
	"x4",
	"x8",
	NULL
};

const char *FeSettings::anisotropicTokens[] =
{
	"0",
	"2",
	"4",
	"8",
	"16",
	NULL
};

const char *FeSettings::anisotropicDispTokens[] =
{
	"None", // Default
	"x2",
	"x4",
	"x8",
	"x16",
	NULL
};

const char *FeSettings::filterWrapTokens[] =
{
	"wrap_within_display",
	"jump_to_next_display",
	"no_wrap",
	NULL
};

const char *FeSettings::filterWrapDispTokens[] =
{
	"Wrap within Display", // Default
	"Jump to Next Display",
	"No Wrap",
	NULL
};

const char *FeSettings::startupTokens[] =
{
	"show_last_selection",
	"show_random_selection",
	"displays_menu",
	"launch_last_game",
	NULL
};

const char *FeSettings::startupDispTokens[] =
{
	"Show Last Selection", // Default
	"Show Random Selection",
	"Show Displays Menu",
	"Launch Last Game",
	NULL
};

const char *FeSettings::prefixTokens[] =
{
	"none",
	"sort",
	"sort_show",
	NULL
};

const char *FeSettings::prefixDispTokens[] =
{
	"None",
	"Sort Only",
	"Sort and Show",
	NULL
};

std::vector<std::string> FeSettings::uiColorTokens =
{
	"68,0,187",
	"40,80,160", // default
	"0,187,255",
	"0,195,170",
	"0,204,68",
	"136,204,51",
	"204,204,51",
	"255,204,0",
	"255,136,0",
	"153,102,34",
	"204,68,17",
	"187,0,17",
	"255,68,136",
	"187,34,187",
	"136,34,187",
	"68,68,68",
	"121,121,121",
	"187,187,187",
};

std::vector<std::string> FeSettings::uiColorDispTokens =
{
	"Indigo",
	"Blue",
	"Cyan",
	"Turquoise",
	"Green",
	"Acid",
	"Gold",
	"Yellow",
	"Orange",
	"Brown",
	"Rust",
	"Red",
	"Pink",
	"Magenta",
	"Purple",
	"Grey",
	"Neutral",
	"White"
};

FeSettings::FeSettings( const std::string &config_path ):
	m_default_layout( "BasicPlus" ),
	m_default_romlist( "Mame" ),
	m_default_display( "Default Display" ),
	m_rl( m_config_path ),
	m_inputmap(),
	m_saver_params( FeLayoutInfo::ScreenSaver ),
	m_intro_params( FeLayoutInfo::Intro ),
	m_current_layout_params( FeLayoutInfo::Layout ),
	m_display_menu_per_display_params( FeLayoutInfo::Menu ),
	m_current_display( -1 ),
	m_selected_display( 0 ),
	m_actual_display_index( 0 ),
	m_current_config_object( NULL ),
	m_ssaver_time( 600 ),
	m_last_plugin_name( "" ),
	m_last_launch_display( 0 ),
	m_last_launch_filter( 0 ),
	m_last_launch_rom( 0 ),
	m_last_launch_clone( -1 ),
	m_clone_index( -1 ),
	m_joy_thresh( 75 ),
	m_mouse_thresh( 10 ),
	m_current_search_index( 0 ),
	m_custom_languages( false ),
	m_displays_menu_exit( true ),
	m_hide_brackets( true ),
	m_group_clones( true ),
	m_startup_mode( ShowLastSelection ),
	m_prefix_mode( SortAndShowPrefix ),
	m_confirm_favs( true ),
	m_confirm_exit( true ),
	m_layout_preview( false ),
	m_custom_overlay( true ),
	m_quick_menu( true ),
	m_track_usage( true ),
	m_multimon( false ),
#if defined(SFML_SYSTEM_LINUX) || defined(FORCE_FULLSCREEN)
	m_window_mode( Fullscreen ),
#else
	m_window_mode( Fillscreen ),
#endif
	m_smooth_images( true ),
	m_filter_wrap_mode( WrapWithinDisplay ),
	m_selection_max_step( 128 ),
	m_selection_delay( 400 ),
	m_selection_speed( 40 ),
	m_image_cache_mbytes( 100 ),
#ifdef SFML_SYSTEM_MACOS
	m_move_mouse_on_launch( false ), // hotcorners
#else
	m_move_mouse_on_launch( true ),
#endif
	m_scrape_snaps( true ),
	m_scrape_marquees( true ),
	m_scrape_flyers( false ),
	m_scrape_wheels( true ),
	m_scrape_fanart( false ),
	m_scrape_vids( false ),
	m_scrape_overview( true ),
#ifdef SFML_SYSTEM_WINDOWS
	m_hide_console( false ),
#endif
	m_power_saving( false ),
	m_check_for_updates( true ),
	m_screen_rotation( RotateNone ),
	m_antialiasing( 0 ),
	m_anisotropic( 0 ),
	m_loaded_game_extras( false ),
	m_present_state( Layout_Showing ),
	m_ui_font_size( 0 ),
	m_ui_color( FeSettings::uiColorTokens[ FE_DEFAULT_UI_COLOR_TOKEN ] ),
	m_split_config_format( false )
{
	if ( config_path.empty() )
		m_config_path = absolute_path( clean_path(FE_DEFAULT_CFG_PATH) );
	else
		m_config_path = absolute_path( clean_path( config_path, true ) );

	// absolute_path can drop the trailing slash
	if (
#ifdef SFML_SYSTEM_WINDOWS
			(m_config_path[m_config_path.size()-1] != '\\') &&
#endif
			(m_config_path[m_config_path.size()-1] != '/') )
		m_config_path += '/';

	FeCache::set_settings( this );
	load_default_display();
}

void FeSettings::clear()
{
	m_current_config_object=NULL;
	m_current_display = -1;

	m_inputmap.clear();
	m_displays.clear();
	m_rl.clear_emulators();
	m_plugins.clear();
}

void FeSettings::load()
{
	clear();

	std::string load_language( FE_DEFAULT_LANGUAGE );

	// Try to load from config/attract.cfg (new format)
	std::string filename = m_config_path + FE_CFG_SUBDIR + FE_CFG_FILE;

	if (( FE_DATA_PATH != NULL ) && ( !directory_exists( FE_DATA_PATH ) ))
	{
		FeLog() << "Warning: Attract-Mode Plus was compiled to look for its default configuration files in: "
			<< FE_DATA_PATH << ", which is not available." << std::endl;
	}

	if ( file_exists( filename ) )
	{
		if ( load_from_file( filename ) == false )
		{
			FeLog() << "Config file not found: " << filename << std::endl;
		}
		else
		{
			FeLog() << "Config: " << filename << std::endl;

			if ( m_language.empty() )
				m_language = FE_DEFAULT_LANGUAGE;

			load_language = m_language;
			m_split_config_format = true;

			load_displays_configs();
			load_plugins_configs();
			load_layouts_configs();
		}
	}
	else
	{
		// Fall back to attract.cfg (backward compatibility)
		filename = m_config_path + FE_CFG_FILE;
		if ( load_from_file( filename ) == false )
		{
			FeLog() << "Config file not found: " << filename << std::endl;
		}
		else
		{
			FeLog() << "Config: " << filename << std::endl;

			if ( m_language.empty() )
				m_language = FE_DEFAULT_LANGUAGE;

			load_language = m_language;

			// Migrate configuration files to config subdirectory
			FeLog() << "Migrating configuration files to 'config' subdirectory" << std::endl;
			confirm_directory( m_config_path, FE_CFG_SUBDIR );

			// Copy state files to new location
			std::string config_dir = m_config_path + FE_CFG_SUBDIR;
			copy_file( m_config_path + FE_STATE_FILE, config_dir + FE_STATE_FILE );
			copy_file( m_config_path + FE_WINDOW_FILE, config_dir + FE_WINDOW_FILE );
			copy_file( m_config_path + FE_SCRIPT_NV_FILE, config_dir + FE_SCRIPT_NV_FILE );
			copy_file( m_config_path + FE_LAYOUT_NV_FILE, config_dir + FE_LAYOUT_NV_FILE );
		}
	}

	// If we just migrated, save the configuration files
	if ( !m_split_config_format && file_exists( m_config_path + FE_CFG_FILE ) )
	{
		save();
	}

	// Load language strings now.
	//
	// If we didn't find a config file, then we leave m_language empty but load the english language strings
	// now.  The user will be prompted (using the english strings) to select a language and then launched
	// straight into configuration mode.
	//
	// If a config file was found but it didn't specify a language, then we use the english language (this
	// preserves the previous behaviour for config files created in an earlier version)
	//
	internal_load_language( load_language );

	load_state();

	if ( get_startup_mode() == ShowDisplaysMenu )
		m_current_display = -1;
	else if ( get_startup_mode() == ShowRandomSelection )
	{
		int display_count = (*get_displays()).size();
		if ( display_count )
		{
			srand( time( 0 ) );
			int display_index = rand() % display_count;
			FeDisplayInfo* display = get_display( display_index );
			int filter_count = display->get_filter_count();
			int filter_index = filter_count ? rand() % filter_count : 0;

			m_selected_display = display_index;
			display->set_current_filter_index( filter_index );
			display->set_rom_index( filter_index, rand() );
		}
	}

	// init_display() is called during set_display(), no need to call it here
	// Make sure we have some keyboard mappings
	//
	m_inputmap.initialize_mappings();

	// If no menu prompt is configured, default to calling it "Displays Menu" (in current language)
	//
	if ( m_menu_prompt.empty() )
		m_menu_prompt = _( "Displays Menu" );
}

// These values must align with fe_settings enum ConfigSettingIndex
const char *FeSettings::configSettingStrings[] =
{
	"language",
	"custom_language",
	"exit_command",
	"exit_message",
	"ui_font_size",
	"ui_color",
	"screen_saver_timeout",
	"displays_menu_exit",
	"hide_brackets",
	"group_clones",
	"startup_mode",
	"prefix_mode",
	"quick_menu",
	"confirm_favourites",
	"confirm_exit",
	"mouse_threshold",
	"joystick_threshold",
	"window_mode",
	"screen_rotation",
	"anti_aliasing",
	"anisotropic",
	"filter_wrap_mode",
	"layout_preview",
	"custom_overlay",
	"track_usage",
	"multiple_monitors",
	"smooth_images",
	"selection_max_step",
	"selection_delay_ms",
	"selection_speed_ms",
	"move_mouse_on_launch",
	"scrape_snaps",
	"scrape_marquees",
	"scrape_flyers",
	"scrape_wheels",
	"scrape_fanart",
	"scrape_videos",
	"scrape_overview",
	"thegamesdb_key",
	"power_saving",
	"check_for_updates",
#ifdef SFML_SYSTEM_WINDOWS
	"hide_console",
#endif
	"video_decoder",
	"menu_prompt",
	"menu_layout",
	"image_cache_mbytes",
	NULL
};

const char *FeSettings::otherSettingStrings[] =
{
	"display",
	"sound",
	"input_map",
	"general",
	"plugin",
	FeLayoutInfo::indexStrings[0], // "saver_config"
	FeLayoutInfo::indexStrings[1], // "layout_config"
	FeLayoutInfo::indexStrings[2], // "intro_config"
	FeLayoutInfo::indexStrings[3], // "menu_config"
	NULL
};


int FeSettings::process_setting( const std::string &setting,
					const std::string &value,
					const std::string &fn )
{
	if (( setting.compare( otherSettingStrings[0] ) == 0 ) // list
		|| ( setting.compare( "list" ) == 0 )) // for backwards compatability.  As of 1.5, "list" became "display"
	{
		// Check for duplicate display names
		std::string lowercase_value = lowercase( value );
		for ( auto& display : m_displays )
		{
			if ( lowercase( display.get_info( FeDisplayInfo::Name ) ) == lowercase_value )
			{
				FeLog() << "Warning: Duplicate display name found: '" << value
					<< "' - skipping duplicate entry" << std::endl;
				return 0;
			}
		}

		m_displays.push_back( FeDisplayInfo( value ) );
		m_current_config_object = &m_displays.back();
	}
	else if ( setting.compare( otherSettingStrings[1] ) == 0 ) // sound
		m_current_config_object = &m_sound_info;
	else if ( setting.compare( otherSettingStrings[2] ) == 0 ) // input_map
		m_current_config_object = &m_inputmap;
	else if ( setting.compare( otherSettingStrings[3] ) == 0 ) // general
		m_current_config_object = NULL;
	else if ( setting.compare( otherSettingStrings[4] ) == 0 ) // plugin
	{
		FePlugInfo new_plug( value );
		m_plugins.push_back( new_plug );
		m_current_config_object = &m_plugins.back();
	}
	else if ( setting.compare( otherSettingStrings[5] ) == 0 ) // saver_config
		m_current_config_object = &m_saver_params;
	else if ( setting.compare( otherSettingStrings[6] ) == 0 ) // layout_config
	{
		for ( std::vector< FeLayoutInfo >::iterator itr=m_layout_params.begin();
			itr != m_layout_params.end(); ++itr )
		{
			if ( value.compare( (*itr).get_name() ) == 0 )
			{
				// If a duplicate, replace existing
				(*itr).clear_params();
				m_current_config_object = &(*itr);
				return 0;
			}
		}

		// Add new layout info
		FeLayoutInfo new_entry( value );
		m_layout_params.push_back( new_entry );
		m_current_config_object = &m_layout_params.back();
	}
	else if ( setting.compare( otherSettingStrings[7] ) == 0 ) // intro_config
		m_current_config_object = &m_intro_params;
	else if ( setting.compare( otherSettingStrings[8] ) == 0 ) // menu_config
		m_current_config_object = &m_display_menu_per_display_params;
	else
	{
		int i=0;
		while ( configSettingStrings[i] != NULL )
		{
			if ( setting.compare( configSettingStrings[i] ) == 0 )
			{
				if ( set_info( i, value ) == false )
				{
					const char **valid = NULL;
					switch ( i )
					{
						case WindowMode:
							valid = windowModeTokens;
							break;

						case ScreenRotation:
							valid = screenRotationTokens;
							break;

						case FilterWrapMode:
							valid = filterWrapTokens;
							break;

						case StartupMode:
							valid = startupTokens;
							break;

						default:
							break;
					}

					invalid_setting( fn,
						configSettingStrings[i],
						value, valid, NULL, "value" );
					return 1;
				}
				return 0;
			}
			i++;
		}

		// if we get this far, then none of the settings associated with
		// this object were found, pass to the current child object
		if ( m_current_config_object != NULL )
			return m_current_config_object->process_setting( setting, value, fn );
		else
		{
			// <=2.4.1 there was an "accelerate_selection" boolean that if false
			// disabled the selection acceleration (the same result as setting
			// selection_max_step = 0)
			//
			if ( setting.compare( "accelerate_selection" ) == 0 )
			{
				if ( config_str_to_bool( value ) )
					m_selection_max_step = 128; // set to default max step
				else
					m_selection_max_step = 0;

				return 0;
			}

			invalid_setting( fn,
				"general", setting, otherSettingStrings, configSettingStrings );
			return 1;
		}
	}

	return 0;
}

// Setup m_current_layout_params with all the parameters for our current layout, including
// the 'per_display' layout parameters that are stored separately but that get merged in here
//
void FeSettings::load_layout_params()
{
	if ( m_current_display < 0 )
	{
		m_current_layout_params = get_layout_config( m_menu_layout );
		m_current_layout_params.merge_params( m_display_menu_per_display_params );
	}
	else
	{
		m_current_layout_params = get_layout_config(
			m_displays[ m_current_display ].get_info( FeDisplayInfo::Layout ) );
		m_current_layout_params.merge_params(
			m_displays[ m_current_display ].get_layout_per_display_params() );
	}
}

void FeSettings::init_display()
{
	FeLog() << std::endl << "*** Initializing display: '" << get_current_display_title() << "'" << std::endl;

	m_loaded_game_extras = false;

	//
	// Setting new_index to negative causes us to do the 'Displays Menu' w/ custom layout
	//
	if ( m_current_display < 0 )
	{
		m_rl.init_as_empty_list();
		FeRomInfoListType &rl = m_rl.get_list();


		for ( unsigned int i=0; i<m_display_menu.size(); i++ )
		{
			FeDisplayInfo *p = &(m_displays[m_display_menu[i]]);

			FeRomInfo rom( p->get_info( FeDisplayInfo::Name ) );
			rom.set_info( FeRomInfo::Title, p->get_info( FeDisplayInfo::Name ) );
			rom.set_info( FeRomInfo::Emulator, "@" );

			rl.push_back( rom );
		}

		if ( m_displays_menu_exit )
		{
			std::string exit_str;
			get_exit_message( exit_str );

			FeRomInfo rom( exit_str );
			rom.set_info( FeRomInfo::Title, exit_str );
			rom.set_info( FeRomInfo::Emulator, "@exit" );
			rom.set_info( FeRomInfo::AltRomname, "exit" );

			rl.push_back( rom );
		}

		FeDisplayInfo empty( "" );
		m_rl.create_filters( empty );

		// we want to keep m_current_search_index through the set_search_value() call
		int temp_idx = m_current_search_index;
		set_search_rule( "" );
		m_current_search_index = temp_idx;

		return;
	}

	set_search_rule( "" );

	const std::string &romlist_name = m_displays[m_current_display].get_info(FeDisplayInfo::Romlist);
	if ( romlist_name.empty() )
	{
		m_rl.init_as_empty_list();
		return;
	}

	m_path_cache.clear();

	std::string list_path;
	if ( !internal_resolve_config_file(
		m_config_path,
		list_path,
		FE_ROMLIST_SUBDIR,
		romlist_name + FE_ROMLIST_FILE_EXTENSION
	))
	{
		m_rl.init_as_empty_list();
		FeLog() << "Cannot find romlist: " << romlist_name << std::endl;
		return;
	}

	if ( m_rl.load_romlist(
		list_path,
		romlist_name,
		m_displays[m_current_display],
		m_group_clones,
		m_track_usage
	))
		m_rl.create_filters( m_displays[m_current_display] );
	else
		FeLog() << "Error opening romlist: " << romlist_name << std::endl;

}

void FeSettings::construct_display_maps()
{
	m_display_cycle.clear();
	m_display_menu.clear();

	for ( unsigned int i=0; i<m_displays.size(); i++ )
	{
		if ( m_displays[i].show_in_cycle() )
			m_display_cycle.push_back( i );

		if ( m_displays[i].show_in_menu() )
			m_display_menu.push_back( i );
	}
}

void FeSettings::load_displays_configs()
{
	std::string displays_file = m_config_path + FE_CFG_SUBDIR + FE_DISPLAYS_FILE;

	if ( file_exists( displays_file ) )
	{
		// Clear any displays that were loaded from attract.cfg
		// since we're loading from the new displays.cfg format
		m_displays.clear();

		load_from_file( displays_file );
		m_current_config_object = NULL;
	}
}

void FeSettings::save_displays_configs() const
{
	std::string displays_file = m_config_path + FE_CFG_SUBDIR + FE_DISPLAYS_FILE;
	nowide::ofstream file( displays_file.c_str() );
	if ( file.is_open() )
	{
		write_header( file );

		for ( std::vector<FeDisplayInfo>::const_iterator it=m_displays.begin(); it != m_displays.end(); ++it )
		{
			write_section( file, "display", (*it).get_info( FeDisplayInfo::Name ) );
			(*it).save( file, 1 );
		}

		file.close();
	}
}

void FeSettings::load_plugins_configs()
{
	std::string plugins_file = m_config_path + FE_CFG_SUBDIR + FE_PLUGINS_FILE;

	if ( file_exists( plugins_file ) )
	{
		// Clear any plugins that were loaded from attract.cfg
		// since we're loading from the new plugins.cfg format
		m_plugins.clear();

		load_from_file( plugins_file );
		m_current_config_object = NULL;
	}
}

void FeSettings::save_plugins_configs() const
{
	std::string plugins_file = m_config_path + FE_CFG_SUBDIR + FE_PLUGINS_FILE;
	nowide::ofstream file( plugins_file.c_str() );
	if ( file.is_open() )
	{
		write_header( file );

		std::vector<std::string> plugin_files;
		get_available_plugins( plugin_files );

		for ( auto& plugin : m_plugins )
		{
			// Get rid of configs for old plugins by not saving it if the
			// plugin itself is gone
			for ( auto& plugin_file : plugin_files )
			{
				if ( plugin_file == plugin.get_name() )
				{
					plugin.save( file );
					break;
				}
			}
		}

		file.close();
	}
}

void FeSettings::load_layouts_configs()
{
	std::string layouts_file = m_config_path + FE_CFG_SUBDIR + FE_LAYOUTS_FILE;

	if ( file_exists( layouts_file ) )
	{
		// Clear any layouts that were loaded from attract.cfg
		// since we're loading from the new layouts.cfg format
		m_layout_params.clear();

		load_from_file( layouts_file );
		m_current_config_object = NULL;
	}
}

void FeSettings::save_layouts_configs() const
{
	std::string layouts_file = m_config_path + FE_CFG_SUBDIR + FE_LAYOUTS_FILE;
	nowide::ofstream file( layouts_file.c_str() );
	if ( file.is_open() )
	{
		write_header( file );

		for ( auto& layout : m_layout_params )
			layout.save( file );

		file.close();
	}
}

void FeSettings::migration_cleanup_dialog( FeOverlay *overlay )
{
	if ( m_split_config_format )
		return;

	std::string old_config = m_config_path + FE_CFG_FILE;
	std::string new_config = m_config_path + FE_CFG_SUBDIR + FE_CFG_FILE;

	if ( file_exists( old_config ) && file_exists( new_config ) )
	{
		std::string msg = _( "Configuration files have been migrated\n to the 'config' folder.\nDelete old files?" );

		if ( overlay->confirm_dialog( msg, false ) == 0 )
		{
			FeLog() << "Deleting old configuration files..." << std::endl;

			if ( file_exists( old_config ) )
				delete_file( old_config );

			std::string old_state = m_config_path + FE_STATE_FILE;
			if ( file_exists( old_state ) )
				delete_file( old_state );

			std::string old_window = m_config_path + FE_WINDOW_FILE;
			if ( file_exists( old_window ) )
				delete_file( old_window );

			std::string old_script_nv = m_config_path + FE_SCRIPT_NV_FILE;
			if ( file_exists( old_script_nv ) )
				delete_file( old_script_nv );

			std::string old_layout_nv = m_config_path + FE_LAYOUT_NV_FILE;
			if ( file_exists( old_layout_nv ) )
				delete_file( old_layout_nv );

			std::string old_default_emulator = m_config_path + FE_EMULATOR_DEFAULT;
			if ( file_exists( old_default_emulator ) )
				delete_file( old_default_emulator );

			std::string old_default_filter = m_config_path + FE_FILTER_DEFAULT;
			if ( file_exists( old_default_filter ) )
				delete_file( old_default_filter );

			std::string old_default_display = m_config_path + FE_LIST_DEFAULT;
			if ( file_exists( old_default_display ) )
				delete_file( old_default_display );

			std::string old_emulator_templates = m_config_path + FE_EMULATOR_TEMPLATES_SUBDIR;
			if ( directory_exists( old_emulator_templates ) )
				delete_dir( old_emulator_templates );

			std::string old_scripts = m_config_path + FE_EMULATOR_SCRIPT_SUBDIR;
			if ( directory_exists( old_scripts ) )
				delete_dir( old_scripts );
		}
	}
	m_split_config_format = true;
}

void FeSettings::save_state()
{
	if ( !m_displays.size() )
		return;

	int display_idx = m_actual_display_index;

	// If we have a display stack we save the entry at the bottom of the stack as the
	// 'current' display
	//
	if ( !m_display_stack.empty() )
		display_idx = m_display_stack.front();

	if ( display_idx < 0 )
		display_idx = m_selected_display;

	m_rl.save_state();

	std::string filename( m_config_path );
	confirm_directory( m_config_path, FE_CFG_SUBDIR );

	filename += FE_CFG_SUBDIR;
	filename += FE_STATE_FILE;

	nowide::ofstream outfile( filename.c_str() );
	if ( outfile.is_open() )
	{
		outfile << m_displays[display_idx].get_info( FeDisplayInfo::Name ) << ";"
			<< m_displays[m_last_launch_display].get_info( FeDisplayInfo::Name ) << "," << m_last_launch_filter
			<< "," << m_last_launch_rom;

		if ( m_last_launch_clone >= 0 )
			outfile << "," << m_last_launch_clone;

		outfile << ";" << m_menu_layout_file << std::endl;

		for ( std::vector<FeDisplayInfo>::const_iterator itl=m_displays.begin();
					itl != m_displays.end(); ++itl )
			outfile << (*itl).state_as_output() << std::endl;

		outfile.close();
	}
}

void FeSettings::load_state()
{
	if ( m_displays.empty() )
	{
		m_current_display = -1;
		return;
	}

	std::string filename( m_config_path );
	filename += FE_CFG_SUBDIR;
	filename += FE_STATE_FILE;

	nowide::ifstream myfile( filename.c_str() );
	std::string line;

	if ( myfile.is_open() && myfile.good() )
	{
		getline( myfile, line );
		size_t pos=0;
		std::string tok;
		token_helper( line, pos, tok, ";" );

		// Try to parse as display name first
		// Fall back to index for backward compatibility
		int temp_display = get_display_index_from_name( tok );
		if ( temp_display >= 0 )
			m_current_display = temp_display;
		else
			m_current_display = as_int( tok );

		token_helper( line, pos, tok, ";" );

		int i=0;
		size_t pos2=0;
		while ( pos2 < tok.size() )
		{
			std::string tok2;
			token_helper( tok, pos2, tok2, "," );

			if ( i == 0 )
			{
				// Try to parse as display name first
				// Fall back to index for backward compatibility
				int temp_launch = get_display_index_from_name( tok2 );
				if ( temp_launch >= 0 )
					m_last_launch_display = temp_launch;
				else
					m_last_launch_display = as_int( tok2 );
			}
			else
			{
				int temp = as_int( tok2 );
				switch (i)
				{
					case 1: m_last_launch_filter = temp; break;
					case 2: m_last_launch_rom = temp; break;
					case 3: m_last_launch_clone = temp; break;
				}
			}
			i++;
		}

		token_helper( line, pos, tok, ";" );
		m_menu_layout_file = tok;

		// Build a map of display states by name, and a vector for old indexed format
		std::map<std::string, std::string> display_state_map;
		std::vector<std::string> display_state_vec;

		while ( myfile.good() )
		{
			std::string state_line;
			getline( myfile, state_line );

			if ( !state_line.empty() )
			{
				size_t semicolon = state_line.find(';');
				if ( semicolon != std::string::npos )
				{
					std::string first_field = state_line.substr(0, semicolon);

					// Check if first field is a valid display name
					bool is_display_name = false;
					for ( std::vector<FeDisplayInfo>::iterator itl=m_displays.begin(); itl != m_displays.end(); ++itl )
					{
						if ( first_field == (*itl).get_info( FeDisplayInfo::Name ))
						{
							is_display_name = true;
							break;
						}
					}

					if ( is_display_name )
						display_state_map[first_field] = state_line;
					else
						display_state_vec.push_back( state_line );
				}
			}
		}

		// Apply states by matching display names (new format)
		for ( std::vector<FeDisplayInfo>::iterator itl=m_displays.begin();
			itl != m_displays.end(); ++itl )
		{
			std::string display_name = (*itl).get_info( FeDisplayInfo::Name );
			std::map<std::string, std::string>::iterator it = display_state_map.find( display_name );
			if ( it != display_state_map.end() )
			{
				(*itl).process_state( it->second );
			}
		}

		// Apply states by index (old format)
		if ( !display_state_vec.empty() )
			for ( size_t i = 0; i < display_state_vec.size() && i < m_displays.size(); i++ )
				m_displays[i].process_state( display_state_vec[i], false );
	}

	// bound checking on the current list state
	if ( m_current_display >= (int)m_displays.size() )
		m_current_display = m_displays.size() - 1;
	if ( m_current_display < 0 )
		m_current_display = 0;

	m_selected_display = m_current_display;
	m_actual_display_index = m_current_display;

	// bound checking on the last launch state
	if ( m_last_launch_display >= (int)m_displays.size() )
		m_last_launch_display = m_displays.size() - 1;
	if ( m_last_launch_display < 0 )
		m_last_launch_display = 0;

	if ( m_last_launch_filter >= m_displays[ m_last_launch_display ].get_filter_count() )
		m_last_launch_filter = m_displays[ m_last_launch_display ].get_filter_count() - 1;
	if ( m_last_launch_filter < 0 )
		m_last_launch_filter = 0;

	// confirm loaded state points to layout files that actually exist (and reset if it doesn't)
	for ( std::vector<FeDisplayInfo>::iterator itr = m_displays.begin(); itr != m_displays.end(); ++itr )
	{
		std::string file = (*itr).get_current_layout_file();
		if ( !file.empty() )
		{
			std::string path;
			get_layout_dir( (*itr).get_info( FeDisplayInfo::Layout ), path );

			std::string fn = path + file + FE_LAYOUT_FILE_EXTENSION ;
			if ( !file_exists( fn ) )
			{
				FeDebug() << "Resetting saved layout file since the file does not actually exist. Display: "
					<< (*itr).get_info( FeDisplayInfo::Name )
					<< ", file: " << fn << :: std::endl;

				(*itr).set_current_layout_file( "" );
			}
		}
	}

	if ( !m_menu_layout_file.empty() )
	{
		std::string path;
		get_layout_dir( m_menu_layout, path );

		std::string fn = path + m_menu_layout_file + FE_LAYOUT_FILE_EXTENSION ;
		if ( !file_exists( fn ) )
		{
			FeDebug() << "Resetting Displays Menu layout file (file doesn't exist): " << fn << :: std::endl;
			m_menu_layout_file = "";
		}
	}
}

void FeSettings::reset_input()
{
	m_inputmap.clear_tracked_keys();
}

FeInputMap::Command FeSettings::map_input( const std::optional<sf::Event> &e )
{
	if ( e.has_value() )
		return m_inputmap.map_input( e.value(), m_mousecap_rect, m_joy_thresh, m_rwnd->hasFocus() );
	else
		return FeInputMap::LAST_COMMAND;
}

FeInputMap::Command FeSettings::input_conflict_check( const FeInputMapEntry &e )
{
	return m_inputmap.input_conflict_check( e );
}

void FeSettings::get_input_config_metrics( sf::IntRect &mousecap_rect, int &joy_thresh )
{
	mousecap_rect = m_mousecap_rect;
	joy_thresh = m_joy_thresh;
}

FeInputMap::Command FeSettings::get_default_command( FeInputMap::Command c )
{
	return m_inputmap.get_default_command( c );
}

void FeSettings::set_default_command( FeInputMap::Command c, FeInputMap::Command v )
{
	m_inputmap.set_default_command( c, v );
}

bool FeSettings::get_current_state( FeInputMap::Command c )
{
	return m_inputmap.get_current_state( c, m_joy_thresh );
}

bool FeSettings::get_key_state( std::string key )
{
	return FeInputMapEntry( key ).get_current_state( m_joy_thresh );
}

void FeSettings::init_mouse_capture( sf::RenderWindow *window )
{
	sf::Vector2i size = sf::Vector2i( window->getSize() );
	sf::Vector2i center = size / 2;
	int radius = size.x * m_mouse_thresh / 400;

	m_rwnd = window;
	m_mousecap_rect.position.x = center.x - radius;
	m_mousecap_rect.position.y = center.y - radius;
	m_mousecap_rect.size.x = radius * 2;
	m_mousecap_rect.size.y = radius * 2;
}

// Returns true if has mouse controls, has focus, and the pos is outside the capture rectangle
bool FeSettings::test_mouse_wrap() const
{
	return (
		has_mouse_moves()
		&& m_rwnd->hasFocus()
		&& !m_mousecap_rect.contains( sf::Mouse::getPosition( *m_rwnd ) )
	);
}

// Wrap the mouse position to the capture rectangle
void FeSettings::wrap_mouse()
{
	sf::Vector2i pos = sf::Mouse::getPosition( *m_rwnd );
	sf::Vector2i center = sf::Vector2i( m_rwnd->getSize() ) / 2;
	sf::Vector2i diff = pos - center;

	// A single axis gets wrapped at a time, which allows diagonals to register
	if ( fabs( diff.x ) >= fabs( diff.y ) )
		pos.x = center.x;
	else
		pos.y = center.y;

	sf::Mouse::setPosition( pos, *m_rwnd );
}

int FeSettings::get_filter_index_from_offset( int offset ) const
{
	if ( m_current_display < 0 )
		return 0;

	int f_count = m_displays[m_current_display].get_filter_count();

	if ( f_count == 0 )
		return 0;

	int off = abs( offset ) % f_count;

	int retval = get_current_filter_index();
	if ( offset < 0 )
		retval -= off;
	else
		retval += off;

	if ( retval < 0 )
		retval += f_count;
	if ( retval >= f_count )
		retval -= f_count;

	return retval;
}

int FeSettings::get_filter_count() const
{
	if ( m_current_display < 0 )
		return 1;

	return m_displays[m_current_display].get_filter_count();
}

int FeSettings::get_filter_size( int filter_index ) const
{
	if ( !m_current_search.empty()
			&& ( get_current_filter_index() == filter_index ))
		return m_current_search.size();

	return m_rl.filter_size( filter_index );
}

int FeSettings::get_rom_index( int filter_index, int offset ) const
{
	int retval, rl_size;
	if ( m_current_display < 0 )
	{
		retval = ( m_selected_display == -1 ) // if EXIT menu item is selected
			? m_display_menu.size() // return index *after* all the display item list indexes
			: find_idx_in_vec(m_selected_display, m_display_menu); // list index of the selected display
		rl_size = m_rl.filter_size( filter_index );
	}
	else if ( !m_current_search.empty()
			&& ( get_current_filter_index() == filter_index ))
	{
		retval = m_current_search_index;
		rl_size = m_current_search.size();
	}
	else
	{
		retval = m_displays[m_current_display].get_rom_index( filter_index );
		rl_size = m_rl.filter_size( filter_index );
	}

	// keep in bounds
	if ( retval >= rl_size )
		retval = rl_size - 1;

	if ( retval < 0 )
		retval = 0;

	// apply the offset
	if ( rl_size > 0 )
	{
		int off = abs( offset ) % rl_size;
		if ( offset < 0 )
			retval -= off;
		else
			retval += off;

		if ( retval < 0 )
			retval = retval + rl_size;
		if ( retval >= rl_size )
			retval = retval - rl_size;
	}

	return retval;
}

void FeSettings::set_search_rule( const std::string &rule_str )
{
	FeDebug() << "set_search_rule = '" << rule_str << "'" << std::endl;

	m_current_search.clear();
	m_current_search_index=0;
	m_current_search_str.clear();
	m_clone_index = -1;

	if ( rule_str.empty() )
		return;

	// The search rule is not stored if it fails to parse
	FeRule rule;
	if ( rule.process_setting( "", rule_str, "" ) )
		return;

	// The search rule is not stored if it fails to init (bad regex)
	if ( !rule.init() )
		return;

	int filter_index = get_current_filter_index();
	for ( int i=0; i<m_rl.filter_size( filter_index ); i++ )
	{
		FeRomInfo &r = m_rl.lookup( filter_index, i );
		if ( rule.apply_rule( r ) )
			m_current_search.push_back( &r );
	}

	// The search rule is not stored if there are no results
	if ( !m_current_search.empty() )
		m_current_search_str = rule_str;
}

bool FeSettings::switch_to_clone_group( int idx )
{
	// check if we aren't grouping clones of if we are already showing
	// the clone list (in which case we return false)
	if (( !m_group_clones ) || ( m_clone_index >= 0 ))
		return false;

	int fi = get_current_filter_index();
	int ri = get_rom_index( fi, 0 );

	// Check if we have a seach going on...
	if ( !m_current_search.empty() )
	{
		//
		// Search results are temporary, so if we are currently
		// showing search results we need to find the selected game
		// in the filter itself (without the search applied)
		//
		FeRomInfo *rom = get_rom_absolute( fi, ri );

		for ( int i=0; i<m_rl.filter_size( fi ); i++ )
		{
			if ( m_rl.lookup( fi, i ) == *rom )
			{
				ri = i;
				break;
			}
		}
	}

	std::vector < FeRomInfo * > group;
	m_rl.get_clone_group( fi, ri, group );

	if ( group.size() > 1 )
	{
		const std::string &t = m_rl.lookup(fi, ri).get_info( FeRomInfo::Title );

		if ( idx >= 0 )
		{
			// if a specific idx value was provided, select that index
			m_current_search_index = idx;
		}
		else
		{
			// if no idx was provided, default select the entry with the same
			// title as the clone group
			m_current_search_index=0;

			for ( int i=0; i < (int)group.size(); i++ )
			{
				if ( t.compare( group[i]->get_info( FeRomInfo::Title ) ) == 0 )
					m_current_search_index = i;
			}
		}

		m_current_search.swap( group );
		m_current_search_str = name_with_brackets_stripped( t );
		m_clone_index = ri;

		FeDebug() << "Switched to clone group '" << t << "' size=" << m_current_search.size() << std::endl;
		return true;
	}

	return false;
}

bool FeSettings::switch_from_clone_group()
{
	if ( m_clone_index < 0 )
		return false;

	int filter_index = get_current_filter_index();
	int rom_index = m_clone_index;

	// Need to clear search rule before set_current_selection()
	set_search_rule( "" );

	set_current_selection( filter_index, rom_index  );
	return true;
}

int FeSettings::get_clone_index()
{
	return m_clone_index;
}

const std::string &FeSettings::get_search_rule() const
{
	return m_current_search_str;
}

const std::string &FeSettings::get_current_display_title() const
{
	if ( m_current_display < 0 )
		return m_menu_prompt; // When showing the 'Displays Menu', title=configured menu prompt

	return m_displays[m_current_display].get_info( FeDisplayInfo::Name );
}

const std::string &FeSettings::get_rom_info( int filter_offset, int rom_offset, FeRomInfo::Index index )
{
	int filter_index = get_filter_index_from_offset( filter_offset );
	int rom_index = get_rom_index( filter_index, rom_offset );
	return get_rom_info_absolute( filter_index, rom_index, index );
}

const std::string &FeSettings::get_rom_info_absolute( int filter_index, int rom_index, FeRomInfo::Index index )
{
	if ( get_filter_size( filter_index ) < 1 )
		return FE_EMPTY_STRING;

	// Make sure we have additional fields if user is requesting them.
	if ( index == FeRomInfo::FileIsAvailable ) m_rl.get_file_availability();
	if ( FeRomInfo::isStat( index )) m_rl.get_played_stats();

	// handle situation where we are currently showing a search result
	if ( !m_current_search.empty() && ( get_current_filter_index() == filter_index ))
		return m_current_search[ rom_index ]->get_info( index );

	return m_rl.lookup( filter_index, rom_index ).get_info( index );
}

FeRomInfo *FeSettings::get_rom_offset( int filter_offset, int rom_offset )
{
	int filter_index = get_filter_index_from_offset( filter_offset );
	int rom_index = get_rom_index( filter_index, rom_offset );
	return get_rom_absolute( filter_index, rom_index );
}

FeRomInfo *FeSettings::get_rom_absolute( int filter_index, int rom_index )
{
	if ( get_filter_size( filter_index ) < 1 )
		return NULL;

	// handle situation where we are currently showing a search result
	//
	if ( !m_current_search.empty()
			&& ( get_current_filter_index() == filter_index ))
		return m_current_search[ rom_index ];

	return &(m_rl.lookup( filter_index, rom_index ));
}

bool FeSettings::get_path(
	FePathType t,
	std::string &path,
	std::string &file ) const
{
	std::string temp;
	FePathType tt;
	if ( t == Current )
	{
		switch ( m_present_state )
		{
		case ScreenSaver_Showing: tt = ScreenSaver; break;
		case Intro_Showing: tt = Intro; break;
		case Layout_Showing:
		default:
			tt = Layout;
			break;
		}
	}
	else
		tt = t;

	switch ( tt )
	{
	case Intro:
		if ( internal_resolve_config_file( m_config_path,
				temp, FE_INTRO_SUBDIR, FE_INTRO_FILE ) )
		{
			size_t len = temp.find_last_of( "/\\" );
			ASSERT( len != std::string::npos );

			path = temp.substr( 0, len + 1 );
			file = FE_INTRO_FILE;
			return true;
		}
		return false;

	case Loader:
		if ( internal_resolve_config_file( m_config_path,
				temp, FE_LOADER_SUBDIR, FE_LOADER_FILE ) )
		{
			size_t len = temp.find_last_of( "/\\" );
			ASSERT( len != std::string::npos );

			path = temp.substr( 0, len + 1 );
			file = FE_LOADER_FILE;
			return true;
		}
		return false;

	case ScreenSaver:
		if ( internal_resolve_config_file( m_config_path,
				temp, FE_SCREENSAVER_SUBDIR, FE_SCREENSAVER_FILE ) )
		{
			size_t len = temp.find_last_of( "/\\" );
			ASSERT( len != std::string::npos );

			path = temp.substr( 0, len + 1 );
			file = FE_SCREENSAVER_FILE;

			return true;
		}
		else
		{
			FeLog() << "Error loading screensaver: " << FE_SCREENSAVER_FILE
					<< std::endl;
			return false;
		}
		break;

	case Layout:
	default:
		if ( m_current_display < 0 )
		{
			// We are showing the "Displays Menu"
			if ( !get_layout_dir( m_menu_layout, path ) )
				return false;

			file = m_menu_layout_file;
		}
		else
		{
			if ( !get_layout_dir(
					m_displays[ m_current_display ].get_info( FeDisplayInfo::Layout ),
					path ) )
				return false;

			file = m_displays[m_current_display].get_current_layout_file();
		}

		if ( file.empty() )
		{
			std::string temp = path + FE_LAYOUT_FILE_BASE
				+ FE_LAYOUT_FILE_EXTENSION;
			if ( path_exists( temp ) )
			{
				file = FE_LAYOUT_FILE_BASE;
				file += FE_LAYOUT_FILE_EXTENSION;
			}
		}
		else
			file += FE_LAYOUT_FILE_EXTENSION;
		break;
	}
	return true;
}

bool FeSettings::get_path(
	FePathType t,
	std::string &path ) const
{
	std::string temp;
	return get_path( t, path, temp );
}

void FeSettings::get_layout_file_basenames_from_path(
	const std::string &path,
	std::vector<std::string> &names_list )
{
	std::vector<std::string> temp_list;

	// check if we are dealing with an archive (.zip, .7z, etc.)
	int i=0;
	int a_index=-1;
	while ( FE_ARCHIVE_EXT[i] != NULL )
	{
		if ( tail_compare( path, FE_ARCHIVE_EXT[i] ) )
		{
			a_index = i;
			break;
		}

		i++;
	}

	if ( a_index >= 0 )
	{
		// Archive

		std::vector<std::string> t2;
		fe_zip_get_dir( path.c_str(), t2 );

		size_t zlen = strlen( FE_ARCHIVE_EXT[a_index] );

		for ( std::vector<std::string>::iterator itr=t2.begin();
			itr != t2.end(); ++itr )
		{
			if ( tail_compare( *itr, FE_LAYOUT_FILE_EXTENSION ) )
				temp_list.push_back(
					(*itr).substr( 0, (*itr).size()-zlen ) );
		}
	}
	else
	{
		get_basename_from_extension( temp_list, path,
			FE_LAYOUT_FILE_EXTENSION );
	}

	names_list.clear();

	int test_len = strlen( FE_LAYOUT_FILE_BASE );
	for ( std::vector< std::string >::iterator itr=temp_list.begin(); itr != temp_list.end(); ++itr )
	{
		// Archives may contain layout*.nut files in a subfolder of the archive,
		// so we want to exclude any preceding path info from our comparison here
		//
		std::size_t pos = (*itr).find_last_of( "/\\" );
		if ( pos != std::string::npos )
			pos++;
		else
			pos = 0;

		if ( (*itr).compare( pos, test_len, FE_LAYOUT_FILE_BASE ) == 0 )
			names_list.push_back( *itr );
	}
}

bool FeSettings::get_layout_dir(
	const std::string &layout_name,
	std::string &layout_dir ) const
{
	if ( layout_name.empty() )
		return false;

	std::string temp;
	if ( internal_resolve_config_file( m_config_path, layout_dir,
			FE_LAYOUT_SUBDIR, layout_name + "/" ) )
		return true;

	int i=0;
	while ( FE_ARCHIVE_EXT[i] != NULL )
	{
		if ( internal_resolve_config_file( m_config_path,
				layout_dir,
				FE_LAYOUT_SUBDIR,
				layout_name + FE_ARCHIVE_EXT[i] ) )
			return true;

		i++;
	}

	return false;
}

FeLayoutInfo &FeSettings::get_layout_config( const std::string &layout_name )
{
	for ( std::vector<FeLayoutInfo>::iterator itr=m_layout_params.begin(); itr != m_layout_params.end(); ++itr )
	{
		if ( layout_name.compare( (*itr).get_name() ) == 0 )
			return (*itr);
	}

	// Add a new config entry if one doesn't exist
	m_layout_params.push_back( FeLayoutInfo( layout_name ) );
	return m_layout_params.back();
}

FeLayoutInfo &FeSettings::get_current_config( FePathType t )
{
	FePathType tt;
	if ( t == Current )
	{
		switch ( m_present_state )
		{
		case ScreenSaver_Showing: tt = ScreenSaver; break;
		case Intro_Showing: tt = Intro; break;
		case Layout_Showing:
		default:
			tt = Layout;
			break;
		}
	}
	else
		tt = t;

	switch ( tt )
	{
	case Intro:
		return m_intro_params;
	case ScreenSaver:
		return m_saver_params;
	case Loader:
	case Layout:
	default:
		return m_current_layout_params;
	}
}

const std::string &FeSettings::get_config_dir() const
{
	return m_config_path;
}

bool FeSettings::config_file_exists() const
{
	std::string config_file = m_config_path + FE_CFG_SUBDIR + FE_CFG_FILE;
	return file_exists( config_file );
}

bool FeSettings::set_display( int index, bool stack_previous )
{
	if ( m_displays.size() == 0 )
		return false;

	std::string old_path, old_file;

	get_path( Layout, old_path, old_file );
	FeLayoutInfo old_config = get_current_config( Layout );

	//
	// Construct our display index views here, for lack of a better spot
	//
	// Do this here so that index views are rebuilt on a forced reset of
	// the display (which happens when settings get changed for example)
	//
	construct_display_maps();

	if ( stack_previous )
	{
		if ( index == -1 )
		{
			if ( !m_display_stack.empty() )
			{
				// Pop a display off the stack
				index = m_display_stack.back();
				m_display_stack.pop_back();
			}
			else
			{
				// Callers must check back_displays_available() before popping the stack
				ASSERT( 0 ); // This should never happen
			}
		}
		else if (( m_current_display != index ) && ( m_current_display >= 0 ))
		{
			// Push a display onto the stack
			m_display_stack.push_back( m_current_display );
		}
	}
	else
	{
		// If not stacking then clear the stack
		m_display_stack.clear();
	}

	m_current_display = index;

	if ( m_current_display != -1 )
	{
		m_actual_display_index = m_current_display;
		m_selected_display = m_current_display;
	}

	m_rl.save_state();
	init_display();

	std::string new_path, new_file;
	get_path( Layout, new_path, new_file );

	//
	// returning true triggers a full reload of the layout. We do this only if the
	// layout file has changed or if the layout parameters have changed (which can happen
	// if the layout contains 'per_display' parameters
	//
	return (( old_path.compare( new_path ) != 0 )
		|| ( old_file.compare( new_file ) != 0 )
		|| ( old_config != get_current_config( Layout ) ));
}

// return true if layout needs to be reloaded as a result
bool FeSettings::navigate_display( int step, bool wrap_mode )
{
	int i = find_idx_in_vec( m_current_display, m_display_cycle );
	i += step;

	int idx=0;
	if ( !m_display_cycle.empty() )
	{
		if ( i >= (int)m_display_cycle.size() )
			idx = m_display_cycle[0];
		else if ( i < 0 )
			idx = m_display_cycle[m_display_cycle.size()-1];
		else
			idx = m_display_cycle[i];
	}

	if ( idx >= (int)m_displays.size() )
		return false;

	bool retval = set_display( idx );

	if ( wrap_mode )
	{
		if ( step > 0 )
			set_current_selection( 0, -1 );
		else if ( m_current_display >= 0 )
			set_current_selection( std::max( 0, m_displays[m_current_display].get_filter_count() - 1 ), -1 );
	}

	return retval;
}

// return true if layout needs to be reloaded as a result
bool FeSettings::navigate_filter( int step )
{
	if ( m_current_display < 0 )
		return false;

	int filter_count = m_displays[m_current_display].get_filter_count();
	int new_filter = m_displays[m_current_display].get_current_filter_index() + step;

	if ( new_filter >= filter_count )
	{
		if ( m_filter_wrap_mode == JumpToNextDisplay )
			return navigate_display( 1, true );

		new_filter = ( m_filter_wrap_mode == NoWrap ) ? filter_count - 1 : 0;
	}
	if ( new_filter < 0 )
	{
		if ( m_filter_wrap_mode == JumpToNextDisplay )
			return navigate_display( -1, true );

		new_filter = ( m_filter_wrap_mode == NoWrap ) ? 0 : filter_count - 1;
	}

	set_search_rule( "" );
	set_current_selection( std::max( 0, new_filter ), -1 );
	return false;
}

int FeSettings::get_current_display_index() const
{
	return m_current_display;
}

int FeSettings::get_actual_display_index() const
{
	return m_actual_display_index;
}

int FeSettings::get_selected_display_index() const
{
	return m_selected_display;
}

int FeSettings::get_display_index_from_name( const std::string &n ) const
{
	for ( unsigned int i=0; i<m_displays.size(); i++ )
	{
		if ( icompare( n, m_displays[i].get_info( FeDisplayInfo::Name ) ) == 0 )
			return i;
	}
	return -1;
}

int FeSettings::get_filter_index_from_name( const std::string &n ) const
{
	std::vector<std::string> names;
	if ( m_current_display < 0 )
	{
		std::string temp_title;
		std::vector<int> temp_ind;
		int temp_idx;
		get_displays_menu( temp_title, names, temp_ind, temp_idx );
	}
	else
		m_displays[m_current_display].get_filters_list( names );

	for ( unsigned int i=0; i<names.size(); i++ )
	{
		if ( icompare( n, names[i] ) == 0 )
			return i;
	}
	return -1;
}

// if rom_index < 0, then the rom index is left unchanged and only the filter index is changed
void FeSettings::set_current_selection( int filter_index, int rom_index )
{
	// handle situation where we are currently showing a search result or clone group
	//
	if ( m_current_display < 0 )
	{
		// Get the index of the selected display, or -1 if EXIT is selected
		m_selected_display = (rom_index < (int)m_display_menu.size()) ? m_display_menu[rom_index] : -1;
	}
	else if (( !m_current_search.empty() && ( get_current_filter_index() == filter_index )))
	{
		m_current_search_index = rom_index;
	}
	else
	{
		m_displays[m_current_display].set_current_filter_index( filter_index );
		if ( rom_index >= 0 )
			m_displays[m_current_display].set_rom_index( filter_index, rom_index );
	}

	m_loaded_game_extras = false;
}

namespace {
	const char *game_extra_strings[] = {
		"executable",
		"args",
		NULL
	};
};

const std::string &FeSettings::get_game_extra( GameExtra id )
{
	if ( !m_loaded_game_extras )
	{
		m_game_extras.clear();
		m_loaded_game_extras = true;

		//
		// Load extra rom settings now (if available)
		//
		if ( m_current_display < 0 )
			return FE_EMPTY_STRING;

		const std::string &romlist_name = m_displays[m_current_display].get_info(FeDisplayInfo::Romlist);
		if ( romlist_name.empty() )
			return FE_EMPTY_STRING;

		if ( !load_game_extras(
				romlist_name,
				get_rom_info( 0, 0, FeRomInfo::Romname ),
				m_game_extras ) )
			return FE_EMPTY_STRING;
	}

	std::map<GameExtra,std::string>::iterator it = m_game_extras.find( id );
	if ( it != m_game_extras.end() )
		return (*it).second;

	return FE_EMPTY_STRING;
}

bool FeSettings::load_game_extras(
		const std::string &romlist_name,
		const std::string &romname,
		std::map<GameExtra,std::string> &extras )
{
	std::string path( m_config_path );
	path += FE_ROMLIST_SUBDIR;
	path += romlist_name;
	path += "/";

	if (( !directory_exists( path ) ) || romname.empty() )
		return false;

	path += romname;
	path += FE_GAME_EXTRA_FILE_EXTENSION;
	if ( !file_exists( path ) )
		return false;

	nowide::ifstream in_file( path.c_str() );
	if ( !in_file.is_open() )
		return false;

	// Before v2.4, Overviews were stored with game extras.
	//
	// There is specific code here to clean up pre 2.4 game extras data (commented)
	// this code is intended to be removed at some point in a future version
	bool fix_extras_now=false;

	while ( in_file.good() )
	{
		std::string line, s, v;
		getline( in_file, line );
		if ( line_to_setting_and_value( line, s, v ) )
		{
			int i=0;
			while ( game_extra_strings[i] != NULL )
			{
				if ( s.compare( game_extra_strings[i] ) == 0 )
				{
					extras[ (GameExtra)i ] = v;
					break;
				}
				i++;
			}

			if ( game_extra_strings[i] == NULL )
			{
				// pre 2.4 cleanup code, see comment above
				if ( s.compare( "overview" ) == 0 )
					fix_extras_now = true;
				else
					FeLog() << " ! Unrecognized game setting: " << s << " " << v << std::endl;
			}
		}
	}

	in_file.close();

	FeDebug() << "Loaded game extras from: " << path << std::endl;

	// pre 2.4 cleanup code, see comment above
	// fix this game extras (removing overview entry)
	//
	if ( fix_extras_now )
		save_game_extras( romlist_name, romname, extras );

	return true;
}

void FeSettings::set_game_extra( GameExtra id, const std::string &value )
{
	if ( value.empty() )
	{
		std::map<GameExtra,std::string>::iterator it=m_game_extras.find( id );
		if ( it != m_game_extras.end() )
			m_game_extras.erase( it );
	}
	else
		m_game_extras[ id ] = value;
}

void FeSettings::save_game_extras()
{
	if ( m_current_display < 0 )
		return;

	const std::string &romlist_name = m_displays[m_current_display].get_info(FeDisplayInfo::Romlist);
	if ( romlist_name.empty() )
		return;

	save_game_extras(
		romlist_name,
		get_rom_info( 0, 0, FeRomInfo::Romname ),
		m_game_extras );
}

void FeSettings::save_game_extras( const std::string &romlist_name,
		const std::string &romname,
		const std::map<GameExtra,std::string> &extras )
{
	std::string path( m_config_path );
	path += FE_ROMLIST_SUBDIR;

	confirm_directory( path, romlist_name );

	path += romlist_name;
	path += "/";

	if ( romname.empty() )
		return;

	path += romname;
	path += FE_GAME_EXTRA_FILE_EXTENSION;

	if ( extras.empty() )
	{
		if ( file_exists( path ) )
			delete_file( path );

		return;
	}

	nowide::ofstream out_file( path.c_str() );
	if ( !out_file.is_open() )
		return;

	std::map<GameExtra,std::string>::const_iterator it;
	for ( it=extras.begin(); it!=extras.end(); ++it )
		out_file << game_extra_strings[(int)(*it).first] << " " << (*it).second << std::endl;

	out_file.close();

	FeDebug() << "Wrote game extras to: " << path << std::endl;
}

bool FeSettings::get_game_overview_filepath( const std::string &emu, const std::string &romname, std::string &path )
{
	path = m_config_path;

	confirm_directory( path, FE_SCRAPER_SUBDIR );
	path += FE_SCRAPER_SUBDIR;

	path += emu;
	path += "/";

	confirm_directory( path, FE_OVERVIEW_SUBDIR );
	path += FE_OVERVIEW_SUBDIR;

	path += romname;
	path += FE_GAME_OVERVIEW_FILE_EXTENSION;

	return path_exists( path );
}

bool FeSettings::get_game_overview_absolute( int filter_index, int rom_index, std::string &ov )
{
	std::string emulator = get_rom_info_absolute( filter_index, rom_index, FeRomInfo::Emulator );
	std::string romname = get_rom_info_absolute( filter_index, rom_index, FeRomInfo::Romname );

	if ( romname.empty() )
		romname = emulator;

	std::string path;
	if ( !get_game_overview_filepath( emulator, romname, path ) )
		return false;

	// We keep the last loaded game overview in memory, use it now if it is a match
	//
	if ( path.compare( m_last_game_overview_path ) == 0 )
	{
		ov = m_last_game_overview_text;
		return true;
	}

	ov.clear();

	nowide::ifstream myfile( path.c_str() );
	if ( !myfile.is_open() )
		return false;

	while ( myfile.good() )
	{
		std::string line;
		getline( myfile, line );

		ov += line;
		ov += "\n";
	}
	myfile.close();

	remove_trailing_spaces( ov );

	m_last_game_overview_path = path;
	m_last_game_overview_text = ov;

	FeDebug() << "Loaded game overview from: " << path
		<< " [" << emulator << "][" << romname << "]" << std::endl;

	return true;

}

void FeSettings::set_game_overview( const std::string &emu,
	const std::string &romname, const std::string &ov, bool overwrite )
{
	std::string path;
	bool file_exists = get_game_overview_filepath( emu, romname, path );

	if ( !file_exists || overwrite )
	{
		if ( ov.empty() && file_exists )
		{
			// delete overview file if setting overview to an empty string
			delete_file( path );

			FeDebug() << "Deleted game overview file: " << path << std::endl;
			return;
		}

		nowide::ofstream outfile( path.c_str() );
		if ( outfile.is_open() )
		{
			outfile << ov << std::endl;
			outfile.close();

			m_last_game_overview_path = path;
			m_last_game_overview_text = ov;

			FeDebug() << "Wrote game overview to: " << path
				<< " [" << emu << "][" << romname << "]" << std::endl;
		}
	}
}

int FeSettings::get_current_filter_index() const
{
	if ( m_current_display < 0 )
		return 0;

	return m_displays[m_current_display].get_current_filter_index();
}

const std::string &FeSettings::get_filter_name( int filter_index )
{
	if ( m_current_display < 0 )
		return FE_EMPTY_STRING;

	FeFilter *f = m_displays[m_current_display].get_filter( filter_index );

	if ( !f )
		return FE_EMPTY_STRING;

	return f->get_name();
}

void FeSettings::get_current_sort( FeRomInfo::Index &idx, bool &rev, int &limit )
{
	idx = FeRomInfo::LAST_INDEX;
	rev = false;
	limit = 0;

	if ( m_current_display < 0 )
		return;

	FeFilter *f = m_displays[m_current_display].get_filter(
			m_displays[m_current_display].get_current_filter_index() );

	if ( f )
	{
		idx = f->get_sort_by();
		rev = f->get_reverse_order();
		limit = f->get_list_limit();
	}
}

void FeSettings::step_current_selection( int step )
{
	int filter_index = get_current_filter_index();
	set_current_selection( filter_index, get_rom_index( filter_index, step )  );
}

bool FeSettings::select_last_launch( bool initial_load )
{
	bool retval = false;

	// On initial_load m_current_display is set *before* the display is actually loaded
	// - switch_to_clone_group requires the romlist, which is loaded by set_display
	if (( initial_load || ( m_current_display != m_last_launch_display )) &&
		( m_last_launch_display < (int)m_displays.size() ))
	{
		set_display( m_last_launch_display );
		retval = true;
	}

	set_search_rule( "" );
	set_current_selection( m_last_launch_filter, m_last_launch_rom );

	if (( m_group_clones ) && ( m_last_launch_clone >= 0 ))
		switch_to_clone_group( m_last_launch_clone );

	return retval;
}

bool FeSettings::is_last_launch( int filter_offset, int index_offset )
{
	return (( m_last_launch_display == m_current_display )
		&& ( m_last_launch_filter == get_filter_index_from_offset( filter_offset ) )
		&& ( m_last_launch_rom == get_rom_index( m_last_launch_filter, index_offset ) ));
}

bool FeSettings::get_current_fav()
{
	int filter_index = get_current_filter_index();

	const std::string &s = get_rom_info_absolute( filter_index,
		get_rom_index( filter_index, 0 ),
		FeRomInfo::Favourite );

	if ( s.empty() || ( s.compare("1") != 0 ))
		return false;
	else
		return true;
}

bool FeSettings::set_fav_absolute( bool status, int filter_index, int rom_index )
{
	if ( m_current_display < 0 )
		return false;

	FeRomInfo *r = get_rom_absolute( filter_index, rom_index );

	if ( !r )
		return false;

	return m_rl.set_fav( *r, m_displays[m_current_display], status );
}

bool FeSettings::set_fav_offset( bool status, int filter_offset, int rom_offset )
{
	int filter_index = get_filter_index_from_offset( filter_offset );
	int rom_index = get_rom_index( filter_index, rom_offset );
	return set_fav_absolute( status, filter_index, rom_index );
}

bool FeSettings::set_fav_current( bool status )
{
	int filter_index = get_current_filter_index();
	int rom_index = get_rom_index( filter_index, 0 );
	return set_fav_absolute( status, filter_index, rom_index );
}

int FeSettings::get_prev_fav_offset()
{
	int filter_index = get_current_filter_index();
	int idx = get_rom_index( filter_index, 0 );

	for ( int i=1; i < get_filter_size( filter_index ); i++ )
	{
		int t_idx = ( i <= idx ) ? ( idx - i )
			: ( get_filter_size( filter_index ) - ( i - idx ) );

		if ( get_rom_info_absolute( filter_index,
				t_idx,
				FeRomInfo::Favourite ).compare( "1" ) == 0 )
			return ( t_idx - idx );
	}

	return 0;
}

int FeSettings::get_next_fav_offset()
{
	int filter_index = get_current_filter_index();
	int idx = get_rom_index( filter_index, 0 );

	for ( int i=1; i < get_filter_size( filter_index ); i++ )
	{
		int t_idx = ( idx + i ) % get_filter_size( filter_index );
		if ( get_rom_info_absolute( filter_index,
				t_idx,
				FeRomInfo::Favourite ).compare( "1" ) == 0 )
			return ( t_idx - idx );
	}

	return 0;
}

namespace
{
	std::string get_sort_letter( const FeRomInfo *rom )
	{
		if ( !rom ) return "";
		const std::string &title = rom->get_sort_title();
		if ( title.empty() ) return "";
		return FeUtil::narrow( std::wstring( 1, FeUtil::widen( title ).at( 0 ) ) );
	}
}

int FeSettings::get_next_letter_offset( int step )
{
	int filter_index = get_current_filter_index();
	int filter_size = get_filter_size( filter_index );
	int idx = get_rom_index( filter_index, 0 );
	int dir = step > 0 ? 1 : -1;

	std::string curr_title = get_sort_letter( get_rom_absolute( filter_index, idx ) );
	const char curr_l = curr_title.empty() ? ' ' : std::tolower( curr_title.at( 0 ) );
	bool is_alpha = std::isalpha( curr_l );

	for ( int i=1; i<filter_size; i++ )
	{
		int t_idx = ( idx + filter_size + dir * i ) % filter_size;
		std::string test_title = get_sort_letter( get_rom_absolute( filter_index, t_idx ) );
		const char test_l = test_title.empty() ? ' ' : std::tolower( test_title.at( 0 ) );

		if ( is_alpha ? ( test_l != curr_l ) : std::isalpha( test_l ) )
			return t_idx - idx;
	}

	return 0;
}

std::vector<std::string> FeSettings::get_tags_available()
{
	return m_rl.get_tags_available();
}

void FeSettings::get_current_tags_list(
	std::vector< std::pair<std::string, bool> > &tags_list )
{
	int filter_index = get_current_filter_index();

	FeRomInfo *r = get_rom_absolute( filter_index,
			get_rom_index( filter_index, 0 ) );

	if ( !r )
		return;

	m_rl.get_tags_list( *r, tags_list );
}

bool FeSettings::replace_tags_offset( const std::string &tags, int filter_offset, int rom_offset )
{
	int filter_index = get_filter_index_from_offset( filter_offset );
	int rom_index = get_rom_index( filter_index, rom_offset );
	return replace_tags_absolute( tags, filter_index, rom_index );
}

bool FeSettings::replace_tags_absolute( const std::string &tags, int filter_index, int rom_index )
{
	if ( m_current_display < 0 )
		return false;

	FeRomInfo *r = get_rom_absolute( filter_index, rom_index );
	if ( !r )
		return false;

	return m_rl.replace_tags( *r, m_displays[m_current_display], tags );
}

bool FeSettings::set_tag_offset( const std::string &tag, bool add_tag, int filter_offset, int rom_offset )
{
	int filter_index = get_filter_index_from_offset( filter_offset );
	int rom_index = get_rom_index( filter_index, rom_offset );
	return set_tag_absolute( tag, add_tag, filter_index, rom_index );
}

bool FeSettings::set_tag_current( const std::string &tag, bool add_tag )
{
	int filter_index = get_current_filter_index();
	int rom_index = get_rom_index( filter_index, 0 );
	return set_tag_absolute( tag, add_tag, filter_index, rom_index );
}

bool FeSettings::set_tag_absolute( const std::string &tag, bool add_tag, int filter_index, int rom_index )
{
	if ( m_current_display < 0 )
		return false;

	FeRomInfo *r = get_rom_absolute( filter_index, rom_index );
	if ( !r )
		return false;

	return m_rl.set_tag( *r, m_displays[m_current_display], tag, add_tag );
}

void FeSettings::toggle_layout()
{
	std::vector<std::string> list;

	std::string layout_path, layout_file;
	get_path( Layout, layout_path, layout_file );

	// strip extension off layout_file
	layout_file = layout_file.substr( 0,
		layout_file.size() - strlen( FE_LAYOUT_FILE_EXTENSION ) );

	get_layout_file_basenames_from_path(
			layout_path,
			list );

	if ( list.size() <= 1 ) // nothing to do if there isn't more than one file
		return;

	unsigned int index=0;
	for ( unsigned int i=0; i< list.size(); i++ )
	{
		if ( layout_file.compare( list[i] ) == 0 )
		{
			index = i;
			break;
		}
	}

	layout_file = list[ ( index + 1 ) % list.size() ];

	if ( m_current_display < 0 )
		m_menu_layout_file = layout_file;
	else
		m_displays[ m_current_display ].set_current_layout_file( layout_file );
}

void FeSettings::set_volume( FeSoundInfo::SoundType t, const std::string &v )
{
	m_sound_info.set_volume( t, v );
}

int FeSettings::get_set_volume( FeSoundInfo::SoundType t ) const
{
	return m_sound_info.get_set_volume( t );
}

int FeSettings::get_play_volume( FeSoundInfo::SoundType t ) const
{
	return m_sound_info.get_play_volume( t );
}

bool FeSettings::get_mute() const
{
	return m_sound_info.get_mute();
}

void FeSettings::set_mute( bool m )
{
	m_sound_info.set_mute( m );
}

bool FeSettings::get_loudness() const
{
	return m_sound_info.get_loudness();
}

void FeSettings::set_loudness( bool enabled )
{
	m_sound_info.set_loudness( enabled );

	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
		fep->set_audio_loudness( enabled );
}

bool FeSettings::get_sound_file( FeInputMap::Command c, std::string &s, bool full_path ) const
{
	std::string filename;
	if ( m_sound_info.get_sound( c, filename ) )
	{
		if ( full_path )
		{
			if ( !internal_resolve_config_file( m_config_path, s, FE_SOUND_SUBDIR, filename ) )
			{
				FeLog() << "Sound file not found: " << filename << std::endl;
				return false;
			}
		}
		else
			s = filename;

		return true;
	}
	return false;
}

void FeSettings::set_sound_file( FeInputMap::Command c, const std::string &s )
{
	m_sound_info.set_sound( c, s );
}

void FeSettings::get_sounds_list( std::vector < std::string > &ll ) const
{
	ll.clear();
	internal_gather_config_files( ll, "", FE_SOUND_SUBDIR );
}

void FeSettings::prep_for_launch( std::string &command,
	std::string &args,
	std::string &work_dir,
	FeEmulatorInfo *&emu )
{
	int filter_index = get_current_filter_index();

	if ( get_filter_size( filter_index ) < 1 )
		return;

	int rom_index = get_rom_index( filter_index, 0 );

	FeRomInfo *rom = get_rom_absolute( filter_index, rom_index );
	if ( !rom )
		return;

	emu = get_emulator( rom->get_info( FeRomInfo::Emulator ) );
	if ( !emu )
		return;

	const std::string &rom_name = rom->get_info( FeRomInfo::Romname );

	m_last_launch_display = get_current_display_index();
	m_last_launch_filter = filter_index;
	m_last_launch_rom = 0;
	m_last_launch_clone = -1;

	if ( !m_current_search.empty()
			&& ( get_current_filter_index() == filter_index ))
	{
		if ( m_clone_index >= 0 )
		{
			//
			// Running from a clone group
			m_last_launch_rom = m_clone_index;
			m_last_launch_clone = rom_index;

		}
		else
		{
			//
			// Search results are temporary, so if we are currently
			// showing search results we need to find the selected game
			// in the filter itself (without the search applied)
			//
			for ( int i=0; i<m_rl.filter_size( filter_index ); i++ )
			{
				if ( m_rl.lookup( filter_index, i ) == *rom )
				{
					m_last_launch_rom = i;
					break;
				}
			}
		}
	}
	else
		m_last_launch_rom = rom_index;

	std::string rom_path, extension, romfilename;

	std::vector<std::string>::const_iterator itr;

	const std::vector<std::string> &exts = emu->get_extensions();
	const char *my_filter[ exts.size() + 1 ];
	bool check_subdirs = false;

	unsigned int i=0;
	for ( std::vector<std::string>::const_iterator itr= exts.begin();
			itr != exts.end(); ++itr )
	{
		if ( exts[i].compare( FE_DIR_TOKEN ) == 0 )
			check_subdirs = true;
		else
		{
			my_filter[i] = (*itr).c_str();
			i++;
		}
	}
	my_filter[ i ] = NULL;

	const std::vector<std::string> &paths = emu->get_paths();

	//
	// Search for the rom
	//
	bool found=false;
	for ( itr = paths.begin(); itr != paths.end(); ++itr )
	{
		std::string path = emu->clean_path_with_wd( *itr, true );
		perform_substitution( path, "[name]", rom_name );

		std::vector < std::string > in_list;
		std::vector < std::string > out_list;

		get_filename_from_base( in_list, out_list, path, rom_name, my_filter );

		if (( check_subdirs ) && ( directory_exists( path + rom_name ) ))
			in_list.push_back( path + rom_name );

		if ( !in_list.empty() )
		{
			//
			// Found the rom to run
			//
			rom_path = path;
			for ( std::vector<std::string>::const_iterator i = in_list.begin(); i != in_list.end(); ++i )
				if ( romfilename.empty() || romfilename.length() > i->length() )
					romfilename = *i;
			found = true;
			break;
		}
	}

	if ( found )
	{
		//
		// figure out the extension
		//
		for ( itr = exts.begin(); itr != exts.end(); ++itr )
		{
			if ( ( (*itr).compare( FE_DIR_TOKEN ) == 0 )
					? directory_exists( romfilename )
					: tail_compare( romfilename, (*itr) ) )
			{
				extension = (*itr);
				break;
			}
		}
	}
	else
	{
		if ( !exts.empty() )
			extension = exts.front();

		if ( !paths.empty() )
			rom_path = clean_path( paths.front(), true );

		romfilename = rom_path + rom_name + extension;

		FeLog() << "Warning: could not locate rom.  Best guess: "
				<< romfilename << std::endl;
	}

	args = get_game_extra( Arguments );
	if ( args.empty() )
		args = emu->get_info( FeEmulatorInfo::Command );

	perform_substitution( args, "[nothing]", "" );
	perform_substitution( args, "[name]", rom_name );
	perform_substitution( args, "[rompath]", rom_path );
	perform_substitution( args, "[romext]", extension );
	perform_substitution( args, "[romfilename]", romfilename );
	perform_substitution( args, "[emulator]",
				emu->get_info( FeEmulatorInfo::Name ) );
	perform_substitution( args, "[workdir]",
				clean_path( emu->get_info( FeEmulatorInfo::Working_dir ) ) );

	const std::vector<std::string> &systems = emu->get_systems();
	if ( !systems.empty() )
	{
		perform_substitution( args, "[system]", systems.front() );
		perform_substitution( args, "[systemn]", systems.back() );
	}

	do_text_substitutions_absolute( args, filter_index, rom_index );

	std::string temp = get_game_extra( Executable );
	if ( temp.empty() )
		temp = emu->get_info( FeEmulatorInfo::Executable );

	command = clean_path( temp );

	work_dir = clean_path( emu->get_info( FeEmulatorInfo::Working_dir ), true );
	if ( work_dir.empty() )
	{
		size_t pos = command.find_last_of( "/\\" );
		if ( pos != std::string::npos )
			work_dir = command.substr( 0, pos );
	}

	save_state();
}

bool FeSettings::update_stats_offset( int play_count, int play_time, int filter_offset, int rom_offset )
{
	int filter_index = get_filter_index_from_offset( filter_offset );
	int rom_index = get_rom_index( filter_index, rom_offset );
	return update_stats_absolute( play_count, play_time, filter_index, rom_index );
}

bool FeSettings::update_stats_current( int play_count, int play_time )
{
	int filter_index = get_current_filter_index();
	int rom_index = get_rom_index( filter_index, 0 );
	return update_stats_absolute( play_count, play_time, filter_index, rom_index );
}

bool FeSettings::update_stats_absolute( int play_count, int play_time, int filter_index, int rom_index )
{
	if (( m_current_display < 0 ) || ( !m_track_usage ))
		return false;

	if ( get_filter_size( filter_index ) < 1 )
		return false;

	int selected_rom_index = get_rom_index( filter_index, 0 );
	FeRomInfo *selected_rom = get_rom_absolute( filter_index, selected_rom_index );
	if ( !selected_rom )
		return false;

	FeRomInfo *rom = get_rom_absolute( filter_index, rom_index );
	if ( !rom )
		return false;

	std::string path = m_config_path + FE_STATS_SUBDIR;

	FeDebug() << "Updating stats:"
		<< " increment play count by " << play_count
		<< " and play time by " << play_time << " seconds."
		<< std::endl;

	if ( !rom->update_stats( path, play_count, play_time ) )
		return false;

	bool changed = m_rl.fix_filters( m_displays[m_current_display], std::set<FeRomInfo::Index>( FeRomInfo::Stats.begin(), FeRomInfo::Stats.end() ) );

	// Stats update may have changed the list, and the current selection
	if ( changed && ( &m_rl.lookup( filter_index, selected_rom_index ) != selected_rom ))
	{
		// Select the previously selected rom
		for ( int i=0; i<m_rl.filter_size( filter_index ); i++ )
		{
			if ( m_rl.lookup( filter_index, i ) == *selected_rom )
			{
				set_current_selection( filter_index, i );
				break;
			}
		}
	}

	return changed;
}

int FeSettings::exit_command() const
{
	int r( -1 );
	if ( !m_exit_command.empty() )
		r = system( m_exit_command.c_str() );

	return r;
}

void FeSettings::get_exit_message( std::string &exit_message ) const
{
	exit_message = m_exit_message.empty()
		? _( "Exit Attract-Mode Plus" )
		: m_exit_message;
}

void FeSettings::get_exit_question( std::string &exit_question ) const
{
	exit_question = m_exit_message.empty()
		? _( "Exit Attract-Mode Plus?" )
		: m_exit_question;
}

//
// Perform substitutions of the [MagicString] sequences occurring in str
//
void FeSettings::do_text_substitutions( std::string &str, int filter_offset, int index_offset )
{
	int filter_index = get_filter_index_from_offset( filter_offset );
	int rom_index = get_rom_index( filter_index, index_offset );
	do_text_substitutions_absolute( str, filter_index, rom_index );
}

//
// Perform substitutions of the [MagicString] sequences occurring in str
//
void FeSettings::do_text_substitutions_absolute( std::string &str, int filter_index, int rom_index )
{
	size_t pos = str.find( '[' );
	while ( pos != std::string::npos )
	{
		size_t close = str.find_first_of( ']', pos+1 );
		if ( close == std::string::npos )
			break; // done, no more enclosed tokens

		std::string token = str.substr( pos+1, close-pos-1 );
		std::string rep;

		if ( get_token_value( token, filter_index, rom_index, rep ) )
		{
			str.replace( pos, close-pos+1, rep );
			pos += rep.size() - 1;
		}

		pos = str.find( '[', pos+1 );
	}
}

namespace {
	//
	// Format PlayedTime to "# Unit(s)" where Units is Seconds, Minutes, Hours, or Days
	// - A single decimal is shown for time over 1 hour
	//
	std::string get_played_time_display_string( std::string value )
	{
		int seconds = as_int( value );
		float val;
		std::string unit;

		if ( seconds < SECONDS_IN_MINUTE )
		{
			val = seconds;
			unit = "$1 Second";
		}
		else if ( seconds < SECONDS_IN_HOUR )
		{
			val = seconds / SECONDS_IN_MINUTE;
			unit = "$1 Minute";
		}
		else if ( seconds < SECONDS_IN_DAY )
		{
			val = seconds / SECONDS_IN_HOUR;
			unit = "$1 Hour";
		}
		else // seconds >= SECONDS_IN_DAY
		{
			val = seconds / SECONDS_IN_DAY;
			unit = "$1 Day";
		}

		if ( seconds < SECONDS_IN_HOUR )
		{
			int v = floor( val );
			if ( v != 1 ) unit += "s";
			return _( unit, { as_str( v ) });
		}

		float v = floor( val * 10 ) / 10;
		bool single = ( v == 1.0 );
		if ( !single ) unit += "s";
		return _( unit, { as_str( v, single ? 0 : 1 ) });
	}

	//
	// Format PlayedLast to "YYYY-mm-dd HH:MM:SS"
	//
	std::string get_played_last_display_string( std::string value )
	{
		std::time_t timestamp = static_cast<std::time_t>( as_int( value ) );

		if ( timestamp == 0 )
			return _( "Never" );

		char label[128]; // buffer large enough to fit formatted timestamp
		std::strftime( std::data(label), std::size(label), _( "_datetime" ).c_str(), std::localtime( &timestamp ) );
		return label;
	}

	//
	// Format PlayedLast to "# Unit(s) Ago"
	//
	std::string get_played_ago_display_string( std::string value )
	{
		std::time_t timestamp = static_cast<std::time_t>( as_int( value ) );

		if ( timestamp == 0 )
			return _( "Never" );

		int seconds = std::time(0) - timestamp;
		if ( seconds < SECONDS_IN_MINUTE )
			return _( "A Moment Ago" );

		float val;
		std::string unit;

		if ( seconds < SECONDS_IN_HOUR )
		{
			val = seconds / SECONDS_IN_MINUTE;
			unit = "$1 Minute";
		}
		else if ( seconds < SECONDS_IN_DAY )
		{
			val = seconds / SECONDS_IN_HOUR;
			unit = "$1 Hour";
		}
		else if ( seconds < SECONDS_IN_WEEK )
		{
			val = seconds / SECONDS_IN_DAY;
			unit = "$1 Day";
		}
		else if ( seconds < SECONDS_IN_MONTH )
		{
			val = seconds / SECONDS_IN_WEEK;
			unit = "$1 Week";
		}
		else if ( seconds < SECONDS_IN_YEAR )
		{
			val = seconds / SECONDS_IN_MONTH;
			unit = "$1 Month";
		}
		else // seconds >= SECONDS_IN_YEAR
		{
			val = seconds / SECONDS_IN_YEAR;
			unit = "$1 Year";
		}

		int v = floor( val );
		if ( v != 1 ) unit += "s";
		return _( unit + " Ago", { as_str( v ) });
	}

	// Return score string value as float
	// - Value between 0.0 and 5.0
	// - Value decimals may be 0.0 or 0.5
	float get_score( std::string value )
	{
		float score = as_float( value );
		score = std::clamp( score, 0.f, FE_SCORE_MAX );
		score = std::round( score * 2.0 ) / 2.0;
		return score;
	}
}

//
// Populate value with the given magic token result
// - Returns false if the given token is invalid
//
bool FeSettings::get_token_value( std::string &token, int filter_index, int rom_index, std::string &value )
{
	int i = get_token_index( FeRomInfo::indexStrings, token );
	switch ( i )
	{
		case FeRomInfo::Title: {
			// Don't strip brackets when showing a clones group
			FeRomInfo *rom = get_rom_absolute( filter_index, rom_index );
			value = rom ? rom->get_display_title() : "";
			if ( m_hide_brackets && m_clone_index < 0 ) value = name_with_brackets_stripped( value );
			return true;
		}
		case FeRomInfo::PlayedTime:
			value = get_played_time_display_string( get_rom_info_absolute( filter_index, rom_index, FeRomInfo::PlayedTime ) );
			return true;
		case FeRomInfo::PlayedLast:
			value = get_played_last_display_string( get_rom_info_absolute( filter_index, rom_index, FeRomInfo::PlayedLast ) );
			return true;
		case FeRomInfo::Score:
			value = as_str( as_float( get_rom_info_absolute( filter_index, rom_index, (FeRomInfo::Index)i ) ), 1 );
			return true;
		default:
			value = get_rom_info_absolute( filter_index, rom_index, (FeRomInfo::Index)i );
			return true;
		case -1:
			return get_special_token_value( token, filter_index, rom_index, value );
	}
}

namespace
{
	// Returns capitalised first character of the display title, or an empty string if none
	// - May return a wide string, ie: pound character
	std::string get_display_letter( const FeRomInfo *rom )
	{
		if ( !rom ) return "";
		const std::string &title = rom->get_display_title();
		if ( title.empty() ) return "";
		return FeUtil::narrow( std::wstring( 1, std::toupper( FeUtil::widen( title )[0] ) ) );
	}
}

//
// Special attributes
//
bool FeSettings::get_special_token_value( std::string &token, int filter_index, int rom_index, std::string &value )
{
	int i = get_token_index( FeRomInfo::specialStrings, token );
	switch ( i )
	{
		case FeRomInfo::DisplayName:
		case FeRomInfo::ListTitle:
			value = get_current_display_title();
			return true;
		case FeRomInfo::FilterName:
		case FeRomInfo::ListFilterName:
			value = get_filter_name( filter_index );
			return true;
		case FeRomInfo::ListSize:
			value = as_str( get_filter_size( filter_index ) );
			return true;
		case FeRomInfo::ListEntry:
			value = as_str( rom_index + 1 );
			return true;
		case FeRomInfo::Search:
			value = m_current_search_str;
			return true;
		case FeRomInfo::TitleFull:
			value = get_rom_info_absolute( filter_index, rom_index, FeRomInfo::Title );
			return true;
		case FeRomInfo::TitleLetter:
			value = get_display_letter( get_rom_absolute( filter_index, rom_index ) );
			return true;
		case FeRomInfo::SortName:
		{
			FeRomInfo::Index sort_by;
			bool reverse_sort;
			int list_limit;
			get_current_sort( sort_by, reverse_sort, list_limit );
			std::string sort_token = ( sort_by == FeRomInfo::LAST_INDEX ) ? "None" : FeRomInfo::indexStrings[sort_by];
			value = _( sort_token );
			return true;
		}
		case FeRomInfo::SortValue:
		{
			FeRomInfo::Index sort_by;
			bool reverse_sort;
			int list_limit;
			get_current_sort( sort_by, reverse_sort, list_limit );
			std::string sort_token = FeRomInfo::indexStrings[ ( sort_by == FeRomInfo::LAST_INDEX ) ? FeRomInfo::Title : sort_by ];
			return get_token_value( sort_token, filter_index, rom_index, value );
		}
		case FeRomInfo::System:
		case FeRomInfo::SystemN:
		{
			const std::string &emu_name = get_rom_info_absolute( filter_index, rom_index, FeRomInfo::Emulator );
			const FeEmulatorInfo *emu = get_emulator( emu_name );
			if ( emu ) {
				const std::vector<std::string> &systems = emu->get_systems();
				if ( !systems.empty() ) value = ( i == FeRomInfo::SystemN ) ? systems.back() : systems.front();
			}
			return true;
		}
		case FeRomInfo::Overview:
			get_game_overview_absolute( filter_index, rom_index, value );
			return true;
		case FeRomInfo::FavouriteStar:
			value = get_rom_info_absolute( filter_index, rom_index, FeRomInfo::Favourite ) == "1" ? FE_STAR_ICON : "";
			return true;
		case FeRomInfo::FavouriteStarAlt:
			value = get_rom_info_absolute( filter_index, rom_index, FeRomInfo::Favourite ) == "1" ? FE_STAR_OUTLINE_ICON : "";
			return true;
		case FeRomInfo::FavouriteHeart:
			value = get_rom_info_absolute( filter_index, rom_index, FeRomInfo::Favourite ) == "1" ? FE_HEART_ICON : "";
			return true;
		case FeRomInfo::FavouriteHeartAlt:
			value = get_rom_info_absolute( filter_index, rom_index, FeRomInfo::Favourite ) == "1" ? FE_HEART_OUTLINE_ICON : "";
			return true;
		case FeRomInfo::TagList:
			{
				FeRomInfo *rom = get_rom_absolute( filter_index, rom_index );
				if ( !rom ) return true;
				std::set<std::string> tagset;
				if ( !rom->get_tags( tagset ) ) return true;
				std::vector<std::string> tags( tagset.begin(), tagset.end() );
				std::sort( tags.begin(), tags.end() );
				std::string delim = as_str( FE_TAG_DELIM ) + as_str( FE_TAG_PREFIX );
				value = as_str( FE_TAG_PREFIX ) + str_join( tags, delim );
				return true;
			}
		case FeRomInfo::PlayedAgo:
			value = get_played_ago_display_string( get_rom_info_absolute( filter_index, rom_index, FeRomInfo::PlayedLast ) );
			return true;
		case FeRomInfo::ScoreStar:
			{
				value.clear();
				float score = get_score( get_rom_info_absolute( filter_index, rom_index, FeRomInfo::Score ) );
				int score_int = std::floor( score );
				bool half = score - score_int > 0;
				for ( int i=0; i<score_int; i++) value += FE_STAR_ICON;
				if ( half ) value += FE_STAR_HALF_ICON;
				return true;
			}
		case FeRomInfo::ScoreStarAlt:
			{
				value.clear();
				float score = get_score( get_rom_info_absolute( filter_index, rom_index, FeRomInfo::Score ) );
				int score_int = std::floor( score );
				bool half = score - score_int > 0;
				for ( int i=0; i<score_int; i++) value += FE_STAR_ICON;
				if ( half ) value += FE_STAR_HALF_ICON;
				for ( int i=score_int+(half?1:0); i<(int)FE_SCORE_MAX; i++ ) value += FE_STAR_OUTLINE_ICON;
				return true;
			}
		default:
			return false;
	}
}

FeEmulatorInfo *FeSettings::get_emulator( const std::string &n )
{
	return m_rl.get_emulator( n );
}

FeEmulatorInfo *FeSettings::create_emulator( const std::string &n, const std::string &t )
{
	return m_rl.create_emulator( n, t );
}

void FeSettings::delete_emulator( const std::string &n )
{
	m_rl.delete_emulator( n );
}

void FeSettings::get_list_of_emulators( std::vector<std::string> &emu_list, bool get_templates )
{
	std::string path = get_config_dir();

	if ( get_templates )
		path += FE_TEMPLATE_EMULATOR_SUBDIR;
	else
		path += FE_EMULATOR_SUBDIR;

	get_basename_from_extension(
		emu_list,
		path,
		FE_EMULATOR_FILE_EXTENSION );

	std::sort( emu_list.begin(), emu_list.end() );
}

bool FeSettings::get_font_file(
	std::string &ffile,
	const std::string &fontname ) const
{
	//
	// First try to load font file directly
	//
	std::string filename = clean_path( fontname );

	if ( is_relative_path( filename ))
		filename = FePresent::script_get_base_path() + filename;

	if ( file_exists( filename ))
	{
		ffile = filename;
		return true;
	}

	FeLog() << " Error, font file not found: " << fontname << std::endl;

	ffile = ""; // make sure default font is assigned
	return false;
}

bool FeSettings::get_multimon() const
{
	return m_multimon;
}

FeSettings::WindowType FeSettings::get_window_mode() const
{
	return m_window_mode;
}

FeSettings::RotationState FeSettings::get_screen_rotation() const
{
	return m_screen_rotation;
}

int FeSettings::get_antialiasing() const
{
	return m_antialiasing;
}

int FeSettings::get_anisotropic() const
{
	return m_anisotropic;
}

FeSettings::FilterWrapModeType FeSettings::get_filter_wrap_mode() const
{
	return m_filter_wrap_mode;
}

FeSettings::StartupModeType FeSettings::get_startup_mode() const
{
	return m_startup_mode;
}

FeSettings::PrefixModeType FeSettings::get_prefix_mode() const
{
	return m_prefix_mode;
}

std::string FeSettings::get_ui_color() const
{
	return m_ui_color;
}

int FeSettings::get_screen_saver_timeout() const
{
	return m_ssaver_time;
}

// Returns true if a custom displays_menu is configured
bool FeSettings::has_custom_displays_menu()
{
	return !get_info( MenuLayout ).empty();
}

void FeSettings::get_displays_menu(
	std::string &title,
	std::vector<std::string> &names,
	std::vector<int> &indices,
	int &current_index ) const
{
	title = m_menu_prompt;

	names.clear();
	indices = m_display_menu;

	for ( unsigned int i=0; i<indices.size(); i++ )
		names.push_back(
			m_displays[indices[i]].get_info( FeDisplayInfo::Name ) );

	int temp = find_idx_in_vec( m_current_display, m_display_menu );
	current_index = ( temp < 0 ) ? 0 : temp;
}

const std::string FeSettings::get_info( int index ) const
{
	switch ( index )
	{
	case Language:
		return m_language;
	case ExitCommand:
		return m_exit_command;
	case ExitMessage:
		return m_exit_message;
	case UIFontSize:
		if ( m_ui_font_size > 0 )
			return as_str( m_ui_font_size );
		break;
	case ScreenSaverTimeout:
		return as_str( m_ssaver_time );
	case MouseThreshold:
		return as_str( m_mouse_thresh );
	case JoystickThreshold:
		return as_str( m_joy_thresh );
	case WindowMode:
		return windowModeTokens[ m_window_mode ];
	case ScreenRotation:
		return screenRotationTokens[ m_screen_rotation ];
	case AntiAliasing:
		return as_str( m_antialiasing );
	case Anisotropic:
		return as_str( m_anisotropic );
	case FilterWrapMode:
		return filterWrapTokens[ m_filter_wrap_mode ];
	case SelectionMaxStep:
		return as_str( m_selection_max_step );
	case SelectionDelay:
		return as_str( m_selection_delay );
	case SelectionSpeed:
		return as_str( m_selection_speed );
	case ImageCacheMBytes:
		return as_str( m_image_cache_mbytes );
	case StartupMode:
		return startupTokens[ m_startup_mode ];
	case PrefixMode:
		return prefixTokens[ m_prefix_mode ];
	case ThegamesdbKey:
		return m_tgdb_key;

	case CustomLanguages:
	case DisplaysMenuExit:
	case HideBrackets:
	case GroupClones:
	case ConfirmFavourites:
	case ConfirmExit:
	case LayoutPreview:
	case CustomOverlay:
	case QuickMenu:
	case TrackUsage:
	case MultiMon:
	case SmoothImages:
	case MoveMouseOnLaunch:
	case ScrapeSnaps:
	case ScrapeMarquees:
	case ScrapeFlyers:
	case ScrapeWheels:
	case ScrapeFanArt:
	case ScrapeVids:
	case ScrapeOverview:
	case PowerSaving:
	case CheckForUpdates:
#ifdef SFML_SYSTEM_WINDOWS
	case HideConsole:
#endif
		return ( get_info_bool( index ) ? FE_CFG_YES_STR : FE_CFG_NO_STR );
	case VideoDecoder:
#ifdef NO_MOVIE
		return "software";
#else
		return FeMedia::get_current_decoder();
#endif

	case MenuPrompt:
		return m_menu_prompt;

	case MenuLayout:
		return m_menu_layout;

	case UIColor:
		return m_ui_color;

	default:
		break;
	}
	return FE_EMPTY_STRING;
}

bool FeSettings::get_info_bool( int index ) const
{
	switch ( index )
	{
	case CustomLanguages:
		return m_custom_languages;
	case DisplaysMenuExit:
		return m_displays_menu_exit;
	case HideBrackets:
		return m_hide_brackets;
	case GroupClones:
		return m_group_clones;
	case ConfirmFavourites:
		return m_confirm_favs;
	case ConfirmExit:
		return m_confirm_exit;
	case LayoutPreview:
		return m_layout_preview;
	case CustomOverlay:
		return m_custom_overlay;
	case QuickMenu:
		return m_quick_menu;
	case TrackUsage:
		return m_track_usage;
	case MultiMon:
		return m_multimon;
	case SmoothImages:
		return m_smooth_images;
	case MoveMouseOnLaunch:
		return m_move_mouse_on_launch;
	case ScrapeSnaps:
		return m_scrape_snaps;
	case ScrapeMarquees:
		return m_scrape_marquees;
	case ScrapeFlyers:
		return m_scrape_flyers;
	case ScrapeWheels:
		return m_scrape_wheels;
	case ScrapeFanArt:
		return m_scrape_fanart;
	case ScrapeVids:
		return m_scrape_vids;
	case ScrapeOverview:
		return m_scrape_overview;
	case PowerSaving:
		return m_power_saving;
	case CheckForUpdates:
		return m_check_for_updates;
#ifdef SFML_SYSTEM_WINDOWS
	case HideConsole:
		return m_hide_console;
#endif
	default:
		break;
	}
	return false;
}

bool FeSettings::set_info( int index, const std::string &value )
{
	switch ( index )
	{
	case Language:
		m_language = value;
		break;

	case CustomLanguages:
		m_custom_languages = config_str_to_bool( value );
		break;

	case ExitCommand:
		m_exit_command = value;
		break;

	case ExitMessage:
		m_exit_message = value;
		m_exit_question = value + "?";
		break;

	case UIFontSize:
		m_ui_font_size = as_int( value );
		break;

	case ScreenSaverTimeout:
		m_ssaver_time = as_int( value );
		break;

	case DisplaysMenuExit:
		m_displays_menu_exit = config_str_to_bool( value );
		break;

	case HideBrackets:
		m_hide_brackets = config_str_to_bool( value );
		break;

	case GroupClones:
		m_group_clones = config_str_to_bool( value );
		break;

	case StartupMode:
		{
			int i = get_token_index( startupTokens, value );
			if (i == -1) return false;
			m_startup_mode = (StartupModeType)i;
		}
		break;

	case PrefixMode:
		{
			int i = get_token_index( prefixTokens, value );
			if (i == -1) return false;
			m_prefix_mode = (PrefixModeType)i;
			FeRomTitleFormatter::display_format = m_prefix_mode == SortAndShowPrefix;
			FeRomTitleFormatter::sort_format = m_prefix_mode == SortAndShowPrefix || m_prefix_mode == SortPrefix;
		}
		break;

	case ThegamesdbKey:
		m_tgdb_key = value;
		break;

	case ConfirmFavourites:
		m_confirm_favs = config_str_to_bool( value );
		break;

	case ConfirmExit:
		m_confirm_exit = config_str_to_bool( value );
		break;

	case QuickMenu:
		m_quick_menu = config_str_to_bool( value );
		break;

	case MouseThreshold:
		m_mouse_thresh = std::clamp( as_int( value ), 1, 100 );
		break;

	case JoystickThreshold:
		m_joy_thresh = std::clamp( as_int( value ), 1, 100 );
		break;

	case WindowMode:
#if !defined(FORCE_FULLSCREEN)
		{
			int i = get_token_index( windowModeTokens, value );
			if (i == -1) return false;
			m_window_mode = (WindowType)i;
		}
#endif
		break;

	case ScreenRotation:
		{
			int i = get_token_index( screenRotationTokens, value );
			if (i == -1) return false;
			m_screen_rotation = (RotationState)i;
		}
		break;

	case AntiAliasing:
		// Limit to the maximum antialiasing level supported by the system but not greater than 8
		m_antialiasing = std::min( std::min( as_int( value ), static_cast<int>( sf::RenderTexture::getMaximumAntiAliasingLevel() )), 8 );

		// Check if it's a power of 2 and if it's greater than 1
		if ( m_antialiasing > 1 && !( m_antialiasing & ( m_antialiasing - 1 )))
			break;

		m_antialiasing = 0;
		break;

	case Anisotropic:
		// Limit Anisotropic Filtering to x16
		m_anisotropic = std::min( as_int( value ), 16 );

		// Check if it's a power of 2 and if it's greater than 1
		if ( m_anisotropic > 1 && !( m_anisotropic & ( m_anisotropic - 1 )))
			break;

		m_anisotropic = 0;
		break;

	case FilterWrapMode:
		{
			int i = get_token_index( filterWrapTokens, value );
			if (i == -1) return false;
			m_filter_wrap_mode = (FilterWrapModeType)i;
		}
		break;

	case LayoutPreview:
		m_layout_preview = config_str_to_bool( value );
		break;

	case CustomOverlay:
		m_custom_overlay = config_str_to_bool( value );
		break;

	case TrackUsage:
		m_track_usage = config_str_to_bool( value );
		break;

	case MultiMon:
#if !defined(NO_MULTIMON)
		m_multimon = config_str_to_bool( value );
#endif
		break;

	case SmoothImages:
		m_smooth_images = config_str_to_bool( value );
		break;

	case SelectionMaxStep:
		m_selection_max_step = std::max( 0, as_int( value ) );

		// check for non-integer input and set to default if encountered
		if (( m_selection_max_step == 0 ) && ( value.compare( "0" ) != 0 ))
			m_selection_max_step = 128;
		break;

	case SelectionDelay:
		m_selection_delay = std::max( 0, as_int( value ) );
		break;

	case SelectionSpeed:
		m_selection_speed = std::max( 0, as_int( value ) );
		break;

	case ImageCacheMBytes:
		m_image_cache_mbytes = std::max( 0, as_int( value ) );

		FeDebug() << "Setting image cache size to " << m_image_cache_mbytes << " MBytes." << std::endl;
		FeImageLoader::set_cache_size( m_image_cache_mbytes * 1024 * 1024 );
		break;

	case MoveMouseOnLaunch:
		m_move_mouse_on_launch = config_str_to_bool( value );
		break;

	case ScrapeSnaps:
		m_scrape_snaps = config_str_to_bool( value );
		break;

	case ScrapeMarquees:
		m_scrape_marquees = config_str_to_bool( value );
		break;

	case ScrapeFlyers:
		m_scrape_flyers = config_str_to_bool( value );
		break;

	case ScrapeWheels:
		m_scrape_wheels = config_str_to_bool( value );
		break;

	case ScrapeFanArt:
		m_scrape_fanart = config_str_to_bool( value );
		break;

	case ScrapeVids:
		m_scrape_vids = config_str_to_bool( value );
		break;

	case ScrapeOverview:
		m_scrape_overview = config_str_to_bool( value );
		break;

	case PowerSaving:
		m_power_saving = config_str_to_bool( value );
		break;

	case CheckForUpdates:
		m_check_for_updates = config_str_to_bool( value );
		break;

#ifdef SFML_SYSTEM_WINDOWS
	case HideConsole:
		m_hide_console = config_str_to_bool( value );
		break;
#endif

	case VideoDecoder:
#ifndef NO_MOVIE
		FeMedia::set_current_decoder( value );
#endif
		break;

	case MenuLayout:
		if ( m_menu_layout.compare( value ) != 0 )
		{
			m_menu_layout = value;
			m_menu_layout_file.clear();
		}
		break;

	case MenuPrompt:
		m_menu_prompt = value;
		break;

	case UIColor:
	{
		sf::Color col;
		if ( str_to_color( value, col ) )
			color_to_rgb( col, m_ui_color );
		break;
	}

	default:
		return false;
	}

	return true;
}

std::vector<FeDisplayInfo> *FeSettings::get_displays()
{
	return &m_displays;
}

FeDisplayInfo *FeSettings::get_display( int index )
{
	if ( ( index < 0 ) || ( index >= (int)m_displays.size() ))
		return NULL;

	std::vector<FeDisplayInfo>::iterator itr=m_displays.begin() + index;
	return &(*itr);
}

void FeSettings::create_filter( FeDisplayInfo &d, const std::string &name ) const
{
	FeFilter new_filter( name );

	std::string defaults_file;
	if ( internal_resolve_config_file( m_config_path, defaults_file, FE_TEMPLATE_SUBDIR, FE_FILTER_DEFAULT ) )
		new_filter.load_from_file( defaults_file );

	d.append_filter( new_filter );
}

bool FeSettings::load_default_display()
{
	return apply_default_display( m_default_display );
}

bool FeSettings::apply_default_display( FeDisplayInfo &display )
{
	std::string filename;
	if ( !internal_resolve_config_file( m_config_path, filename, FE_TEMPLATE_SUBDIR, FE_LIST_DEFAULT ) )
		return false;

	display.load_from_file( filename );
	return true;
}

bool FeSettings::save_default_display()
{
	std::string defaults_file;
	internal_resolve_config_file( m_config_path, defaults_file, FE_TEMPLATE_SUBDIR, FE_LIST_DEFAULT );
	nowide::ofstream outfile( defaults_file.c_str() );
	if ( !outfile.is_open() )
		return false;

	write_header( outfile );
	m_default_display.save( outfile );
	outfile.close();
	return true;
}

FeDisplayInfo *FeSettings::get_default_display()
{
	return &m_default_display;
}

FeDisplayInfo *FeSettings::create_display( const std::string &n )
{
	if ( m_current_display == -1 )
		m_current_display = 0;

	FeDisplayInfo display( n );
	apply_default_display( display );

	std::vector<std::string> layouts;
	std::vector<std::string> romlists;
	get_layouts_list( layouts );
	get_romlists_list( romlists );

	// Find default values using the priority:
	// - FE_LIST_DEFAULT > Hard-coded(if avail) > Last-Display > First-Available

	std::string layout = display.get_info( FeDisplayInfo::Layout );
	if (layout.empty() && ( std::find( layouts.begin(), layouts.end(), m_default_layout ) != layouts.end() ))
		layout = m_default_layout;
	if (layout.empty() && !m_displays.empty())
		layout = m_displays.back().get_info( FeDisplayInfo::Layout );
	if (layout.empty() && !layouts.empty())
		layout = layouts.front();

	std::string romlist = display.get_info( FeDisplayInfo::Romlist );
	if (romlist.empty() && ( std::find( romlists.begin(), romlists.end(), m_default_romlist ) != romlists.end() ))
		romlist = m_default_romlist;
	if (romlist.empty() && !m_displays.empty())
		romlist = m_displays.back().get_info( FeDisplayInfo::Romlist );
	if (romlist.empty() && !romlists.empty())
		romlist = romlists.front();

	display.set_info( FeDisplayInfo::Layout, layout );
	display.set_info( FeDisplayInfo::Romlist, romlist );
	m_displays.push_back( display );
	return &(m_displays.back());
}

void FeSettings::get_layouts_list( std::vector<std::string> &layouts ) const
{
	std::string path = m_config_path + FE_LAYOUT_SUBDIR;

	get_subdirectories( layouts, path );

	if ( FE_DATA_PATH != NULL )
	{
		std::string t = FE_DATA_PATH;
		t += FE_LAYOUT_SUBDIR;

		get_subdirectories( layouts, t );
	}

	if ( !layouts.empty() )
	{
		// Sort the list and remove duplicates
		std::sort( layouts.begin(), layouts.end() );
		layouts.erase( std::unique( layouts.begin(), layouts.end() ), layouts.end() );
	}
}

void FeSettings::delete_display( int index )
{
	if ( ( index < 0 ) || ( index >= (int)m_displays.size() ))
		return;

	std::vector<FeDisplayInfo>::iterator itr=m_displays.begin() + index;
	m_displays.erase( itr );

	if ( m_current_display >= index )
		m_current_display--;

	if ( m_current_display < 0 )
		m_current_display=0;
}

bool FeSettings::check_duplicate_display_name( const std::string &name, int exclude_index ) const
{
	std::string lowercase_name = lowercase( name );
	for ( int i = 0; i < (int)m_displays.size(); i++ )
	{
		if ( i == exclude_index )
			continue;

		if ( lowercase( m_displays[i].get_info( FeDisplayInfo::Name ) ) == lowercase_name )
			return true;
	}
	return false;
}

void FeSettings::get_current_display_filter_names(
		std::vector<std::string> &list ) const
{
	if ( m_current_display < 0 )
		return;

	m_displays[ m_current_display ].get_filters_list( list );
}

bool FeSettings::check_romlist_configured( const std::string &n ) const
{
	for ( std::vector<FeDisplayInfo>::const_iterator itr=m_displays.begin();
			itr!=m_displays.end(); ++itr )
	{
		if ( n.compare( (*itr).get_info( FeDisplayInfo::Romlist ) ) == 0 )
			return true;
	}

	return false;
}

void FeSettings::save() const
{
	confirm_directory( m_config_path, FE_CFG_SUBDIR );
	confirm_directory( m_config_path, FE_ROMLIST_SUBDIR );
	confirm_directory( m_config_path, FE_EMULATOR_SUBDIR );
	confirm_directory( m_config_path, FE_LAYOUT_SUBDIR );
	confirm_directory( m_config_path, FE_SCREENSAVER_SUBDIR );
	confirm_directory( m_config_path, FE_INTRO_SUBDIR );
	confirm_directory( m_config_path, FE_MODULES_SUBDIR );
	confirm_directory( m_config_path, FE_SOUND_SUBDIR );
	confirm_directory( m_config_path, FE_PLUGIN_SUBDIR );
	confirm_directory( m_config_path, FE_MENU_ART_SUBDIR );
	// no FE_LANGUAGE_SUBDIR

	std::string menu_art = m_config_path;
	menu_art += FE_MENU_ART_SUBDIR;
	confirm_directory( menu_art, "flyer/" );
	confirm_directory( menu_art, "wheel/" );
	confirm_directory( menu_art, "marquee/" );
	confirm_directory( menu_art, "snap/" );
	confirm_directory( menu_art, "fanart/" );

	save_displays_configs();
	save_plugins_configs();
	save_layouts_configs();

	std::string filename( m_config_path );
	filename += FE_CFG_SUBDIR;
	filename += FE_CFG_FILE;

	FeLog() << "Writing config to: " << filename << std::endl;

	nowide::ofstream outfile( filename.c_str() );
	if ( outfile.is_open() )
	{
		// generated
		write_header( outfile );

		// sound
		outfile << otherSettingStrings[1] << std::endl;
		m_sound_info.save( outfile );
		outfile << std::endl;

		// input_map
		outfile << otherSettingStrings[2] << std::endl;
		m_inputmap.save( outfile );
		outfile << std::endl;

		// general
		outfile << otherSettingStrings[3] << std::endl;
		for ( int i=0; i < LAST_INDEX; i++ )
			write_pair( outfile, configSettingStrings[i], get_info( i ), 1 );
		outfile << std::endl;

		// saver_config
		m_saver_params.save( outfile );

		// intro_config
		m_intro_params.save( outfile );
		m_display_menu_per_display_params.save( outfile );

		outfile.close();
	}
}

int FeSettings::displays_count() const
{
	return m_displays.size();
}

void FeSettings::get_available_plugins( std::vector < std::string > &ll ) const
{
	//
	// Gather plugins that are subdirectories in the plugins directory
	//
	get_subdirectories( ll, m_config_path + FE_PLUGIN_SUBDIR );

	if ( FE_DATA_PATH != NULL )
	{
		std::string t = FE_DATA_PATH;
		t += FE_PLUGIN_SUBDIR;
		get_subdirectories( ll, t );
	}

	//
	// Also gather plugins that are lone .nut files in the plugins directory
	//
	internal_gather_config_files(
		ll,
		FE_PLUGIN_FILE_EXTENSION,
		FE_PLUGIN_SUBDIR );

	//
	// Can be an archive file (.zip, etc) in the plugins directory as well
	//
	int i=0;
	while ( FE_ARCHIVE_EXT[i] != NULL )
	{
		internal_gather_config_files(
			ll,
			FE_ARCHIVE_EXT[i],
			FE_PLUGIN_SUBDIR );
		i++;
	}

	if ( !ll.empty() )
	{
		// Sort the list and remove duplicates
		std::sort( ll.begin(), ll.end() );
		ll.erase( std::unique( ll.begin(), ll.end() ), ll.end() );
	}
}

void FeSettings::get_plugin( const std::string &label,
		FePlugInfo *&plug,
		int &index )
{
	std::vector< FePlugInfo >::iterator itr;
	for ( int i=0; i<(int)m_plugins.size(); i++ )
	{
		if ( label.compare( m_plugins[i].get_name() ) == 0 )
		{
			plug = &(m_plugins[i]);
			index = i;
			return;
		}
	}

	// No config for this plugin currently.  Add one
	//
	m_plugins.push_back( FePlugInfo( label ) );
	plug = &(m_plugins.back());
	index = m_plugins.size() - 1;
}

void FeSettings::set_last_plugin( FePlugInfo *plug )
{
	m_last_plugin_name = plug->get_name();
}

// Populates the last edited plugin / the first active plugin / the first plugin, or return false
bool FeSettings::get_last_plugin( FePlugInfo *&plug, int &index )
{
	auto plugins = get_plugins();
	std::sort(plugins.begin(), plugins.end(), [](FePlugInfo &a, FePlugInfo &b) {
		return a.get_name() < b.get_name();
	});

	// if no previous plugin, find first active one
	if ( m_last_plugin_name.empty() )
	{
		for ( auto &plugin : plugins )
		{
			if ( !plugin.get_enabled() ) continue;
			m_last_plugin_name = plugin.get_name();
			break;
		}
	}

	// if no active plugin, find first inactive
	if ( m_last_plugin_name.empty() && plugins.size() )
		m_last_plugin_name = plugins[0].get_name();

	// if found plugin, populate args
	if ( !m_last_plugin_name.empty() )
		get_plugin( m_last_plugin_name, plug, index );

	// return true if populated
	return !m_last_plugin_name.empty();
}

bool FeSettings::get_plugin_enabled( const std::string &label ) const
{
	std::vector< FePlugInfo >::const_iterator itr;
	for ( itr = m_plugins.begin(); itr != m_plugins.end(); ++itr )
	{
		if ( label.compare( (*itr).get_name() ) == 0 )
			return (*itr).get_enabled();
	}

	// if there is no config then it isn't enabled
	return false;
}

void FeSettings::get_plugin_full_path(
				const std::string &label,
				std::string &path,
				std::string &filename ) const
{
	std::string temp;

	//
	// There are three valid locations for plugins:
	//
	// <config_dir>/plugins/<name>/plugin.nut
	// <config_dir>/plugins/<name>.nut
	// <config_dir>/plugins/<name>.zip (plugin.nut)
	//
	if ( internal_resolve_config_file( m_config_path, temp, FE_PLUGIN_SUBDIR, label + "/" ) )
	{
		path.swap( temp );
		filename = FE_PLUGIN_FILE;
		return;
	}

	if ( internal_resolve_config_file( m_config_path,
			temp,
			FE_PLUGIN_SUBDIR,
			label + FE_PLUGIN_FILE_EXTENSION ) )
	{
		size_t len = temp.find_last_of( "/\\" );
		ASSERT( len != std::string::npos );

		path = temp.substr( 0, len + 1 );
		filename = label + FE_PLUGIN_FILE_EXTENSION;
		return;
	}

	int i=0;
	while ( FE_ARCHIVE_EXT[i] != NULL )
	{
		if ( internal_resolve_config_file( m_config_path,
				temp,
				FE_PLUGIN_SUBDIR,
				label + FE_ARCHIVE_EXT[i] ) )
		{
			path.swap( temp );
			filename = FE_PLUGIN_FILE;
			return;
		}
		i++;
	}


	FeLog() << "Plugin file not found: " << label << std::endl;
}

void FeSettings::get_plugin_full_path( int id,
		std::string &path ) const
{
	if (( id < 0 ) || ( id >= (int)m_plugins.size() ))
		return;

	std::string ignored;
	get_plugin_full_path( m_plugins[id].get_name(), path, ignored );
}

//
// Loads language resources
// - Always loads the default language, since it contains ALL translations
// - Load the selected language over the default one, any missing entries will use default values
// - Optionally load a custom language from a file
//
void FeSettings::internal_load_language( const std::string &language )
{
	m_translation_map.clear();

	// Find the builtin languages first
	const char* key = 0;
	const char* defaultKey = 0;
	for ( std::map<const char*, const char*>::const_iterator itl=_binary_languages.begin(); itl!=_binary_languages.end(); ++itl)
	{
		size_t pos = 0;
		std::string itl_language;
		token_helper( itl->first, pos, itl_language );

		if ( itl_language.compare( language ) == 0 )
			key = itl->first;
		if ( itl_language.compare( FE_DEFAULT_LANGUAGE ) == 0 )
			defaultKey = itl->first;
	}

	// Optionally find the custom language file
	std::string filename = "";
	bool hasCustom = get_info_bool( CustomLanguages )
		&& internal_resolve_config_file( m_config_path, filename, FE_LANGUAGE_SUBDIR, language + FE_LANGUAGE_FILE_EXTENSION );

	if ( defaultKey )
		m_translation_map.load_from_binary( _binary_languages.at( defaultKey ), ";" );

	if ( key && ( key != defaultKey ) )
		m_translation_map.load_from_binary( _binary_languages.at( key ), ";" );

	if ( hasCustom )
		m_translation_map.load_from_file( filename, ";" );

	if ( !key && !hasCustom )
		FeLog() << "Error loading language resource: " << language << std::endl;
}

void FeSettings::set_language( const std::string &s )
{
	if ( s.compare( m_language ) != 0 )
	{
		m_language = s;
		internal_load_language( m_language );
	}
}

namespace
{
	bool language_comp( const FeLanguage &a, const FeLanguage &b )
	{
		return icompare( a.language, b.language ) < 0;
	}
};

void FeSettings::get_languages_list( std::vector<FeLanguage> &ll ) const
{

	ll.clear();
	std::set<std::string> languages;

	// Add all the built-in language options
	for ( std::map<const char*, const char*>::const_iterator itl=_binary_languages.begin(); itl!=_binary_languages.end(); ++itl)
	{
		size_t pos = 0;
		std::string language;
		std::string label;
		token_helper( itl->first, pos, language );
		token_helper( itl->first, pos, label );
		languages.insert( language );
		ll.push_back( FeLanguage( language, label ) );
	}

	if ( get_info_bool( CustomLanguages ) )
	{
		// Read msg files from language folder
		// - First line must be: #@Label
		std::vector<std::string> names;
		internal_gather_config_files( names, FE_LANGUAGE_FILE_EXTENSION, FE_LANGUAGE_SUBDIR );
		for ( std::vector<std::string>::iterator itr=names.begin(); itr!=names.end(); ++itr )
		{
			if ( languages.count( *itr ) )
				continue;

			std::string filename;
			if ( !internal_resolve_config_file( m_config_path, filename, FE_LANGUAGE_SUBDIR, (*itr) + FE_LANGUAGE_FILE_EXTENSION ))
				continue;

			nowide::ifstream myfile( filename );
			if ( !myfile.is_open() )
				continue;

			std::string line;
			getline( myfile, line );
			myfile.close();

			if (( line.size() < 2 ) || ( line.compare( 0, 2, "#@" ) != 0 ))
				continue;

			std::string label = line.substr( 2 );
			if ( label.empty() )
				continue;

			ll.push_back( FeLanguage( *itr, label ) );
		}
	}

	std::sort( ll.begin(), ll.end(), language_comp );
}

void FeSettings::get_romlists_list( std::vector < std::string > &ll ) const
{
	ll.clear();
	internal_gather_config_files(
		ll,
		FE_ROMLIST_FILE_EXTENSION,
		FE_ROMLIST_SUBDIR );
}


void FeSettings::internal_gather_config_files(
			std::vector<std::string> &ll,
			const std::string &extension,
			const char *subdir ) const
{
	std::string config_path = m_config_path + subdir;

	// check the config directory first
	if ( path_exists( config_path ) )
		get_basename_from_extension( ll, config_path, extension );

	// then the data directory
	if ( FE_DATA_PATH != NULL )
	{
		std::string data_path = FE_DATA_PATH;
		data_path += subdir;

		get_basename_from_extension( ll, data_path, extension );
	}

	// Sort the list and remove duplicates
	std::sort( ll.begin(), ll.end() );
	ll.erase( std::unique( ll.begin(), ll.end() ), ll.end() );
}

bool FeSettings::get_module_path(
	const std::string &module,
	std::string &module_dir,
	std::string &module_file ) const
{
	//
	// Try for "[module].nut" first
	//
	std::string fname = module;
	if ( !tail_compare( fname, FE_LAYOUT_FILE_EXTENSION ) )
		fname += FE_LAYOUT_FILE_EXTENSION;

	std::string res;
	if ( internal_resolve_config_file( m_config_path, res,
		FE_MODULES_SUBDIR, fname ) )
	{
		size_t len = res.find_last_of( "/\\" );
		ASSERT( len != std::string::npos );

		module_dir = absolute_path( res.substr( 0, len + 1 ) );

		len = fname.find_last_of( "/\\" );
		if ( len != std::string::npos )
			module_file = fname.substr( len + 1 );
		else
			module_file = fname;

		return true;
	}

	//
	// Fall back to "[module]/module.nut" first
	//
	const char *MOD_FNAME = "module.nut";

	fname = module + "/";
	fname += MOD_FNAME;

	if ( internal_resolve_config_file( m_config_path, res,
		FE_MODULES_SUBDIR, fname ) )
	{
		size_t len = res.find_last_of( "/\\" );
		ASSERT( len != std::string::npos );

		module_dir = absolute_path( res.substr( 0, len + 1 ) );
		module_file = MOD_FNAME;
		return true;
	}

	return false;
}

bool gather_artwork_filenames(
	const std::vector < std::string > &art_paths,
	const std::string &target_name,
	std::vector<std::string> &vids,
	std::vector<std::string> &images,
	bool image_only,
	FePathCache *path_cache )
{
	for ( std::vector< std::string >::const_iterator itr = art_paths.begin();
			itr != art_paths.end(); ++itr )
	{
		std::vector < std::string > img_contents;
		std::vector < std::string > vid_contents;

		if ( path_cache )
		{
			path_cache->get_filename_from_base(
				img_contents,
				vid_contents,
				(*itr),
				target_name + '.',
				FE_ART_EXTENSIONS );
		}
		else
		{
			get_filename_from_base(
				img_contents,
				vid_contents,
				(*itr),
				target_name + '.',
				FE_ART_EXTENSIONS );
		}

#ifdef NO_MOVIE
		vid_contents.clear();
#else
		for ( std::vector<std::string>::iterator itn = vid_contents.begin();
				itn != vid_contents.end(); )
		{
			if ( FeMedia::is_supported_media_file( *itn ) )
				++itn;
			else
				itn = vid_contents.erase( itn );
		}
#endif

		if ( !img_contents.empty() || !vid_contents.empty() )
		{
			images.insert( images.end(), img_contents.begin(), img_contents.end() );
			vids.insert( vids.end(), vid_contents.begin(), vid_contents.end() );
			continue;
		}

		//
		// If there is a subdirectory in art_path with the
		// given target name, then we load a random video or
		// image from it at this point (if available)
		//
		std::string sd_path = (*itr) + target_name;
		if ( directory_exists( sd_path ) )
		{
			sd_path += "/";

			get_filename_from_base(
				img_contents,
				vid_contents,
				sd_path,
				"",
				FE_ART_EXTENSIONS );

#ifdef NO_MOVIE
			vid_contents.clear();
#else
			for ( std::vector<std::string>::iterator itn = vid_contents.begin();
					itn != vid_contents.end(); )
			{
				if ( FeMedia::is_supported_media_file( *itn ) )
					++itn;
				else
					itn = vid_contents.erase( itn );
			}
#endif

			std::shuffle( vid_contents.begin(), vid_contents.end(), rnd );
			std::shuffle( img_contents.begin(), img_contents.end(), rnd );

			images.insert( images.end(), img_contents.begin(), img_contents.end() );
			vids.insert( vids.end(), vid_contents.begin(), vid_contents.end() );
		}
	}

	if ( image_only ) vids.clear();
	return ( !images.empty() || !vids.empty() );
}

bool art_exists( const std::string &path, const std::string &base )
{
	std::vector<std::string> u1;
	std::vector<std::string> u2;

	return ( get_filename_from_base(
		u1, u2, path, base, FE_ART_EXTENSIONS ) );
}


bool FeSettings::internal_get_best_artwork_file(
	const FeRomInfo &rom,
	const std::string &art_name,
	std::vector<std::string> &vid_list,
	std::vector<std::string> &image_list,
	bool image_only,
	bool ignore_emu )
{
	std::vector < std::string > art_paths;

	// map boxart->flyer and banner->marquee so that artworks with those labels get
	// scraped artworks
	std::string scrape_art;
	if ( art_name.find( "box") != std::string::npos )
		scrape_art = "flyer";
	else if ( art_name.compare( "banner" ) == 0 )
		scrape_art = "marquee";
	else
		scrape_art = art_name;

	const std::string &romname = rom.get_info( FeRomInfo::Romname );
	std::string emu_name = rom.get_info( FeRomInfo::Emulator );

	if ( emu_name.compare( 0, 1, "@" ) == 0 )
	{
		// If emu_name starts with "@", this is a display menu or signal option.
		//
		// Check for the emulator associated with the display named in the romname
		// field, to try and get a sensible artwork in this circumstance
		//
		int temp_d = get_display_index_from_name( romname );

		if ( temp_d < 0 ) // if no match, fall back to the current display
			temp_d = get_current_display_index();

		if ( temp_d >= 0 )
			emu_name = get_display( temp_d )->get_info( FeDisplayInfo::Romlist );

		std::string add_path = get_config_dir() + FE_MENU_ART_SUBDIR + art_name + "/";
		if ( directory_exists( add_path ) )
			art_paths.push_back( add_path );

		if ( FE_DATA_PATH != NULL )
		{
			add_path = FE_DATA_PATH;
			add_path += FE_MENU_ART_SUBDIR;
			add_path += art_name;
			add_path += "/";

			if ( directory_exists( add_path ) )
				art_paths.push_back( add_path );
		}
	}

	std::string layout_path;
	get_path( Current, layout_path );

	FeEmulatorInfo *emu_info = get_emulator( emu_name );
	if ( emu_info )
	{
		std::vector < std::string > temp_list;
		emu_info->get_artwork( art_name, temp_list );
		for ( std::vector< std::string >::iterator itr = temp_list.begin();
			itr != temp_list.end(); ++itr )
		{
			art_paths.push_back(
				emu_info->clean_path_with_wd( *itr, !is_supported_archive( *itr ) ) );

			perform_substitution( art_paths.back(), "$LAYOUT", layout_path );
		}
	}

	std::string scraper_path = get_config_dir() + FE_SCRAPER_SUBDIR + emu_name + "/" + scrape_art + "/";
	if ( directory_exists( scraper_path ) )
		art_paths.push_back( scraper_path );

	if ( !art_paths.empty() )
	{
		const std::string &altname = rom.get_info( FeRomInfo::AltRomname );
		const std::string &cloneof = rom.get_info( FeRomInfo::Cloneof );

		std::vector<std::string> romname_image_list;
		if ( gather_artwork_filenames( art_paths, romname, vid_list, romname_image_list, image_only, &m_path_cache ) )
		{
			// test for "romname" specific videos first
			if ( !image_only && !vid_list.empty() )
				return true;
		}

		bool check_altname = ( !altname.empty() && ( romname.compare( altname ) != 0 ));

		std::vector<std::string> altname_image_list;
		if ( check_altname && gather_artwork_filenames( art_paths, altname, vid_list, altname_image_list, image_only, &m_path_cache ) )
		{
			// test for "altname" specific videos second
			if ( !image_only && !vid_list.empty() )
				return true;
		}

		bool check_cloneof = ( !cloneof.empty() && (altname.compare( cloneof ) != 0 ));

		std::vector<std::string> cloneof_image_list;
		if ( check_cloneof && gather_artwork_filenames( art_paths, cloneof, vid_list, cloneof_image_list, image_only, &m_path_cache ) )
		{
			// then "cloneof" specific videos
			if ( !image_only && !vid_list.empty() )
				return true;
		}

		// now return "romname" specific images if we have them
		if ( !romname_image_list.empty() )
		{
			image_list.swap( romname_image_list );
			return true;
		}

		// next is "altname" specific images
		if ( !altname_image_list.empty() )
		{
			image_list.swap( altname_image_list );
			return true;
		}

		// then "cloneof" specific images
		if ( !cloneof_image_list.empty() )
		{
			image_list.swap( cloneof_image_list );
			return true;
		}

		// then "emulator"
		if ( !ignore_emu && !emu_name.empty()
			&& gather_artwork_filenames( art_paths, emu_name, vid_list, image_list, image_only, &m_path_cache ) )
			return true;
	}

	return false;
}

void FeSettings::get_best_artwork_file(
	const FeRomInfo &rom,
	const std::string &art_name,
	std::vector<std::string> &vid_list,
	std::vector<std::string> &image_list,
	bool image_only )
{
	if ( internal_get_best_artwork_file( rom, art_name, vid_list, image_list, image_only, false ) )
		return;

	// check for layout fallback images/videos
	std::string layout_path;
	std::string archive_name;

	get_path( FeSettings::Current, layout_path );

	if ( layout_path.empty() )
		return;

	const std::string &emu_name = rom.get_info(
			FeRomInfo::Emulator );

	std::vector<std::string> layout_paths;
	layout_paths.push_back( layout_path );

	// check for "[emulator-[artlabel]" artworks first
	if ( gather_artwork_filenames( layout_paths,
		emu_name + "-" + art_name, vid_list, image_list, image_only, &m_path_cache ) )
	{
		if ( !image_only && !vid_list.empty() )
			return;
	}

	// then "[artlabel]"
	gather_artwork_filenames( layout_paths, art_name, vid_list, image_list, image_only, &m_path_cache );

}

bool FeSettings::has_artwork( const FeRomInfo &rom, const std::string &art_name )
{
	std::vector<std::string> temp1, temp2;
	return ( internal_get_best_artwork_file( rom, art_name, temp1, temp2, false, true ) );
}

bool FeSettings::has_video_artwork( const FeRomInfo &rom, const std::string &art_name )
{
	std::vector<std::string> vids, temp;
	internal_get_best_artwork_file( rom, art_name, vids, temp, false, true );
	return (!vids.empty());
}

bool FeSettings::has_image_artwork( const FeRomInfo &rom, const std::string &art_name )
{
	std::vector<std::string> temp, images;
	internal_get_best_artwork_file( rom, art_name, temp, images, true, true );
	return (!images.empty());
}

bool FeSettings::get_best_dynamic_image_file(
	int filter_index,
	int rom_index,
	const std::string &art_name,
	std::vector<std::string> &vid_list,
	std::vector<std::string> &image_list )
{
	std::string base = art_name;
	do_text_substitutions_absolute( base, filter_index, rom_index );

	std::string path;

	// split filename from directory paths correctly
	if ( is_relative_path( base ) )
		get_path( Current, path );

	size_t pos = base.find_last_of( "/\\" );
	if ( pos != std::string::npos )
	{
		path += base.substr( 0, pos+1 );
		base.erase( 0, pos+1 );
	}

	// strip extension from filename, if present
	pos = base.find_last_of( "." );
	if ( pos != std::string::npos )
		base.erase( pos );

	if ( base.empty() )
		return false;

	std::vector< std::string > paths;
	paths.push_back( path );

	return gather_artwork_filenames( paths, base, vid_list, image_list, false, NULL ); // TODO: image_only variable, or false?
}

bool FeSettings::update_romlist_after_edit(
	const FeRomInfo &original,
	const FeRomInfo &replacement,
	UpdateType u_type )
{
	// Exit early if display invalid
	if ( m_current_display < 0 )
		return false;

	const std::string &romlist_name = m_displays[m_current_display].get_info(FeDisplayInfo::Romlist);

	// Update the romlist file - it this fails do nothing else
	if ( !update_romlist_file( romlist_name, original, replacement, u_type ) )
		return false;

	// Update the current in-memory romlist entry
	update_romlist_entry( original, replacement, u_type );

	// Flag tags as changed so they get saved on exit
	if (( u_type == EraseEntry ) || ( u_type == InsertEntry ) || ( original != replacement ))
		m_rl.mark_favs_and_tags_changed();

	return true;
}

//
// Update the in-memory romlist
//
bool FeSettings::update_romlist_entry(
	const FeRomInfo &original,
	const FeRomInfo &replacement,
	UpdateType u_type
)
{
	FeRomInfoListType &rl = m_rl.get_list();
	for ( FeRomInfoListType::iterator it = rl.begin(); it != rl.end(); ++it )
	{
		if ( (*it).full_comparison( original ) )
		{
			switch ( u_type )
			{
				case EraseEntry:
					it = rl.erase( it );
					return true;
				case InsertEntry:
					it = rl.insert( it, replacement );
					return true;
				default:
				case UpdateEntry:
					(*it) = replacement;
					return true;
			}
		}
	}
	return false;
}

bool FeSettings::update_romlist_file(
	const std::string &romlist_name,
	const FeRomInfo &original,
	const FeRomInfo &replacement,
	UpdateType u_type )
{
	// Find valid romlist path
	std::string in_path;
	if ( !internal_resolve_config_file(
		m_config_path,
		in_path,
		FE_ROMLIST_SUBDIR,
		romlist_name + FE_ROMLIST_FILE_EXTENSION))
		return false;

	// Exit early if cannot open infile
	nowide::ifstream infile( in_path );
	if ( !infile.is_open() )
		return false;

	// Reload the ENTIRE romlist (ignoring all filters) and apply update
	FeRomInfoListType temp_list;
	bool found = false;
	while ( infile.good() )
	{
		std::string line, setting, value;
		getline( infile, line );
		if ( line_to_setting_and_value( line, setting, value, ";" ) )
		{
			FeRomInfo rom( setting );
			rom.process_setting( setting, value, "" );

			if ( !found && rom.full_comparison( original ) )
			{
				found=true;
				switch ( u_type )
				{
					case UpdateEntry:
						temp_list.push_back( replacement );
						continue;
					case InsertEntry:
						temp_list.push_back( replacement );
						break;
					case EraseEntry:
						continue;
				}
			}
			temp_list.push_back( rom );
		}
	}
	infile.close();

	// If we didn't find the original, add this as a new rom at the end
	// This way if the user edits on an empty list, they can create a first entry
	if ( !found && ( u_type == UpdateEntry || u_type == InsertEntry ))
		temp_list.push_back( replacement );

	// NOTE: We do NOT delete stats file upon during erase
	// - Stats are linked against the emulator, which may be used in other romlists!

	// Output path is always in config directory
	std::string out_path = m_config_path
		+ FE_ROMLIST_SUBDIR
		+ romlist_name
		+ FE_ROMLIST_FILE_EXTENSION;
	return write_romlist( out_path, temp_list );
}

//
// Write romlist to filename
//
bool FeSettings::write_romlist(
	const std::string &filename,
	const FeRomInfoListType &romlist )
{
	FeLog() << " + Writing " << romlist.size() << " entries to: " << filename << std::endl;

	nowide::ofstream outfile( filename );
	if ( !outfile.is_open() )
		return false;

	// Output romlist header
	int i=0;
	outfile << "#" << FeRomInfo::indexStrings[i++];
	while ( i < FeRomInfo::LAST_INFO ) outfile << ";" << FeRomInfo::indexStrings[i++];
	outfile << std::endl;

	// Output the list
	for ( FeRomInfoListType::const_iterator itl=romlist.begin(); itl != romlist.end(); ++itl )
		outfile << (*itl).as_output() << std::endl;

	outfile.close();
	return true;
}

bool FeSettings::get_emulator_setup_script( std::string &path, std::string &file )
{
	confirm_directory( m_config_path, FE_TEMPLATE_SUBDIR );
	confirm_directory( m_config_path, FE_TEMPLATE_EMULATOR_SUBDIR );

	std::string temp;
	if ( !internal_resolve_config_file( m_config_path,
			temp, FE_TEMPLATE_SCRIPT_SUBDIR, FE_EMULATOR_INIT_FILE ) )
	{
		FeLog() << "Unable to open emulator init script: "  << FE_EMULATOR_INIT_FILE << std::endl;
		return false;
	}

	size_t len = temp.find_last_of( "/\\" );
	ASSERT( len != std::string::npos );

	path = temp.substr( 0, len + 1 );
	file = FE_EMULATOR_INIT_FILE;

	return true;
}
