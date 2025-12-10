/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-17 Andrew Mickelson
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

#ifdef USE_LIBCURL
#include <curl/curl.h>
#endif

#include "fe_settings.hpp"
#include "fe_present.hpp"
#include "fe_overlay.hpp"
#include "fe_util.hpp"
#include "fe_image.hpp"
#include "fe_sound.hpp"
#include "fe_text.hpp"
#include "fe_window.hpp"
#include "fe_vm.hpp"
#include "fe_blend.hpp"
#include "fe_net.hpp"
#include <iostream>
#include <cmath>
#include <cstdlib>
#include "nowide/args.hpp"
#include <SFML/Audio.hpp>

#ifdef SFML_SYSTEM_ANDROID
#include "fe_util_android.hpp"
#endif

#ifdef SFML_SYSTEM_WINDOWS
#include <windows.h>
#include "nvapi.hpp"

extern "C"
{
// Request more powerful GPU (Nvidea,AMD)
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

void process_args( int argc, char *argv[],
			std::string &config_path,
			bool &process_console,
			std::string &startup_display,
			std::string &startup_filter,
			std::string &log_file,
			FeLogLevel &log_level,
			bool &window_topmost,
			std::vector<int> &window_args );

int main(int argc, char *argv[])
{
	std::string config_path, log_file;
	bool launch_game = false;
	bool initial_load = true;
	bool process_console = false;
	std::string startup_display;
	std::string startup_filter;
	int last_fullscreen_mode = FeSettings::WindowType::Fullscreen;
	int last_window_mode = FeSettings::WindowType::Window;
	int last_display_index = -1;
	FeLogLevel log_level = FeLog_Info;
	bool window_topmost = false;
	std::vector<int> window_args = std::vector<int>();
	int layout_sel = 0;
	int plugin_sel = 0;

#ifdef USE_LIBCURL
	curl_global_init( CURL_GLOBAL_ALL );
#endif

	nowide::args a( argc, argv );
	process_args( argc, argv, config_path, process_console, startup_display, startup_filter, log_file, log_level, window_topmost, window_args );

	FeSettings feSettings( config_path );
	feSettings.set_window_topmost( window_topmost );

	FeWindowPosition win_pos;

	if ( !window_args.empty() )
	{
		win_pos.m_temporary = true;
		win_pos.m_pos = sf::Vector2i( window_args[0], window_args[1] );
		win_pos.m_size = sf::Vector2u( window_args[2], window_args[3] );
	}

#ifdef USE_LIBCURL
	FeVersionChecker versionChecker;
#endif

	//
	// Setup logging
	//
#if defined(SFML_SYSTEM_WINDOWS) && !defined(WINDOWS_CONSOLE)
	if ( log_file.empty() ) // on windows non-console version, write log to "last_run.log" by default
	{
		log_file = feSettings.get_config_dir();
		log_file += "last_run.log";
	}
#endif
	//
	// If a log file was supplied at the command line, write to that log file
	// If no file is supplied, logging is to stdout
	//
	if ( !log_file.empty() )
		fe_set_log_file( clean_path( log_file ) );

	// The following call also initializes the log callback for ffmpeg
	//
	fe_set_log_level( log_level );

	//
	// Run the front-end
	//
	FeLog() << "--------------------------------------------------------------------------------" << std::endl;
	fe_print_version();
	FeLog() << std::endl;

	// Redirect SFML error buffer to FeLog
	sf::err().rdbuf(FeLog().rdbuf());

	if ( !sf::Shader::isAvailable() )
	{
		FeLog() << "Error, Attract-Mode Plus requires shader support."  << std::endl;
		return 1;
	}

#ifdef SFML_SYSTEM_WINDOWS
	// Detect an nvidia card and if it's found create an nvidia profile for optimizations
	if ( nvapi_init() > 0 )
		FeLog() << "Nvidia GPU detected. Attract-Mode Plus profile was not found so it has been created.\n"
				<< "In order for the changes to take effect, please restart Attract-Mode Plus\n" << std::endl;
	FeDebug() << std::endl;
#endif

	feSettings.load();

	//
	// Set up music/sound playing objects
	//
	FeSoundSystem soundsys( &feSettings );

	soundsys.play_ambient();

	FeWindow window( feSettings );
	window.set_window_position( win_pos );
	window.initial_create();

#ifdef WINDOWS_CONSOLE
	if ( feSettings.get_hide_console() )
		hide_console();
#endif

	FeVM feVM( feSettings, window, soundsys.get_ambient_sound(), process_console );
	FeOverlay feOverlay( window, feSettings, feVM );
	feVM.set_overlay( &feOverlay );

	// Set transforms now in case load_layout never gets called (ie: first-run), required for multimon
	feVM.set_transforms();

	bool exit_selected=false;

	feSettings.migration_cleanup_dialog( &feOverlay );

	if ( feSettings.get_info( FeSettings::Language ).empty() )
	{
#ifdef SFML_SYSTEM_ANDROID
		android_copy_assets();
#endif
		// If our language isn't set at this point, we want to prompt the user for the language
		// they wish to use
		//
		if ( feOverlay.languages_dialog() < 0 )
			exit_selected = true;

		if ( !exit_selected && feOverlay.confirm_dialog( _( "Auto-detect emulators?" ), true ) == 0 )
			feVM.setup_wizard();
	}

	soundsys.sound_event( FeInputMap::EventStartup );

	bool redraw=true, has_focus=true;
	std::optional<sf::Event> joy_guard;

	// variables used to track movement when a key is held down
	FeInputMap::Command move_state( FeInputMap::LAST_COMMAND ); // command mapped to the move
	FeInputMap::Command move_triggered( FeInputMap::LAST_COMMAND ); // "repeatable" command triggered on move (if any)
	sf::Clock move_timer;
	std::optional<sf::Event> move_event;
	int move_last_triggered( 0 );
	sf::Clock command_timer;
	command_timer.stop();

	// the splash_logo will be displayed if there are no displays configured
	bool config_mode = false;

#ifdef USE_LIBCURL
	if ( feSettings.get_info_bool( FeSettings::CheckForUpdates ) )
		versionChecker.initiate();
#endif

	// Only show intro if there are displays configured
	if ( !(feSettings.displays_count() < 1) )
	{
		if ( !startup_display.empty() )
		{
			// Display set by commandline so startup with provided selection
			feSettings.set_info( FeSettings::StartupMode, "show_last_selection" );

			int display_index = feSettings.get_display_index_from_name( startup_display );
			if ( display_index < 0 )
				display_index = std::clamp( as_int( startup_display ), 0, (int)(*feSettings.get_displays()).size() - 1 );
			FeLog() << "Startup Display: '" << startup_display << "' = " << display_index << std::endl;
			feSettings.set_display( display_index );

			if ( !startup_filter.empty() )
			{
				int filter_index = feSettings.get_filter_index_from_name( startup_filter );
				if ( filter_index < 0 )
					filter_index = std::clamp( as_int( startup_filter ), 0, feSettings.get_display( display_index )->get_filter_count() - 1 );
				FeLog() << "Startup Filter: '" << startup_filter << "' = " << filter_index << std::endl;
				feSettings.set_current_selection( filter_index, -1 );
			}
		}
		else
		{
			// Load the display early so the intro can use the romlist and artwork
			feSettings.set_display( feSettings.get_selected_display_index() );
		}

		// Attempt to start the intro
		if ( !feVM.load_intro() )
		{
			// Post a dummy command to trigger poll_command to end the Intro_Showing mode
			feVM.post_command( FeInputMap::EventStartup );
		}
	}

	while (window.isOpen() && (!exit_selected))
	{
		if ( config_mode )
		{
			//
			// Enter config mode
			//
			int old_mode = feSettings.get_window_mode();
			int old_aa = feSettings.get_antialiasing();
			int old_multimon = feSettings.get_multimon();

			if ( feOverlay.config_dialog( -1, FeInputMap::Configure ) )
			{
				feSettings.on_joystick_connect(); // update joystick mappings

				soundsys.stop();
				soundsys.play_ambient();

				// Recreate window if the window mode changed
				if (( feSettings.get_window_mode() != old_mode )
					|| ( feSettings.get_antialiasing() != old_aa )
					|| ( feSettings.get_multimon() != old_multimon ))
				{
					window.on_exit();
					window.initial_create();
					feVM.init_monitors();
				}

				// Settings have changed, reload the display
				feSettings.set_display( feSettings.has_custom_displays_menu()
					? feSettings.get_current_display_index()
					: feSettings.get_selected_display_index() // in case config has removed custom display
				);
				feVM.load_layout();
			}
			feVM.reset_screen_saver();
			config_mode=false;
			redraw=true;
		}
		else if (( launch_game )
			&& ( !soundsys.is_sound_event_playing( FeInputMap::Select ) ))
		{
			const std::string &emu_name = feSettings.get_rom_info( 0, 0, FeRomInfo::Emulator );

			if ( emu_name.compare( 0, 1, "@" ) == 0 )
			{
				if ( emu_name.size() > 1 )
				{
					// If emu name size >1 and  starts with a "@", then we treat the rest of the
					// string as the signal we have to send
					//
					FeVM::cb_signal( emu_name.substr( 1 ).c_str() );
				}
				else
				{
					// If emu name is just "@", then this is a shortcut to another display, so instead
					// of running a game we switch to the display specified in the rom_info's Romname field
					//
					std::string name = feSettings.get_rom_info( 0, 0, FeRomInfo::Romname );
					int index = feSettings.get_display_index_from_name( name );

					// if index not found or if we are already in the specified display, then
					// jump to the altromname display instead
					//
					if (( index < 0 ) || ( index == feSettings.get_current_display_index() ))
					{
						name = feSettings.get_rom_info( 0, 0, FeRomInfo::AltRomname );
						if ( !name.empty() )
							index =  feSettings.get_display_index_from_name( name );
					}

					if ( index < 0 )
					{
						FeLog() << "Error resolving shortcut, Display `" << name << "' not found." << std::endl;
					}
					else
					{
						if ( feSettings.set_display( index, true ) )
							feVM.load_layout();
						else
							feVM.update_to_new_list( 0, true );
					}
				}
				launch_game=false;
			}

			if ( launch_game && feSettings.switch_to_clone_group() )
			{
				feVM.update_to_new_list( 0, true );
				launch_game=false;
			}

			if ( launch_game )
			{
				soundsys.stop();

				soundsys.release_audio( true );
				feVM.pre_run();

				// window.run() returns true if our window has been closed while
				// the other program was running
				if ( !window.run() )
					exit_selected = true;

				feVM.post_run();
				soundsys.release_audio( false );

				soundsys.sound_event( FeInputMap::EventGameReturn );
				soundsys.play_ambient();

#ifdef USE_DRM
				feSettings.switch_from_clone_group();
				feVM.load_layout();
#else
				if ( feSettings.switch_from_clone_group() )
					feVM.update_to_new_list( 0, true );
#endif

				has_focus=true;
			}

			launch_game=false;
			redraw=true;
		}

		FeInputMouse::clear();
		FeInputMap::Command c;
		std::optional<sf::Event> ev;
		bool from_ui;
		while ( feVM.poll_command( c, ev, from_ui ) )
		{
			if ( ev.has_value() )
			{
				//
				// Special case handling based on event type
				//
				if ( ev->is<sf::Event::Closed>() )
					exit_selected = true;

				else if ( feSettings.test_mouse_wrap() )
				{
					feSettings.wrap_mouse();
				}

				else if ( ev->is<sf::Event::KeyReleased>() ||
							ev->is<sf::Event::MouseButtonReleased>() ||
							ev->is<sf::Event::JoystickButtonReleased>() )
				{
					//
					// We always want to reset the screen saver on these events,
					// even if they aren't mapped otherwise (mapped events cause
					// a reset too)
					//
					if (( c == FeInputMap::LAST_COMMAND )
							&& ( feVM.reset_screen_saver() ))
						redraw = true;
				}

				else if ( ev->is<sf::Event::FocusGained>() )
				{
					redraw = true;
				}

				else if ( ev->is<sf::Event::Resized>() )
				{
					const auto* resize = ev->getIf<sf::Event::Resized>();
					if ( resize )
					{
						window.get_win().setView( sf::View( sf::FloatRect({ 0, 0 }, { static_cast<float>( resize->size.x ), static_cast<float>( resize->size.y )})));
						feVM.init_monitors();
						feVM.load_layout();
						redraw = true;
					}
				}

				else if ( ev->is<sf::Event::JoystickMoved>() )
				{
					// Set the guard if there's a command mapped to this movement
					if ( c != FeInputMap::LAST_COMMAND )
						joy_guard = ev;

					// Skip only if no command AND we have an active guard
					else if ( joy_guard.has_value() && joy_guard->is<sf::Event::JoystickMoved>() )
						continue;
				}

				else if ( ev->is<sf::Event::JoystickConnected>() ||
							ev->is<sf::Event::JoystickDisconnected>() )
				{
					feSettings.on_joystick_connect();
				}
			}

			// Test if we need to keep the joystick axis guard up
			//
			if ( joy_guard.has_value() && joy_guard->is<sf::Event::JoystickMoved>() )
			{
				const auto* mov = joy_guard->getIf<sf::Event::JoystickMoved>();
				if ( mov )
				{
					float pos = sf::Joystick::getAxisPosition( mov->joystickId, mov->axis );
					if ( std::abs( pos ) < feSettings.get_joy_thresh() )
						joy_guard = std::nullopt;
				}
			}

			// Nothing further to do if there is no command or if we are in the process
			// of launching a game already
			//
			if (( c == FeInputMap::LAST_COMMAND ) || ( launch_game ))
				continue;

			//
			// KEY REPEAT LOGIC (1 of 2)
			//
			// if 'move_state' is set, we typically bail here and let the move timer trigger
			// the next action
			//
			// Special case: If we didn't get a move_triggered value set when the move state was first
			// set, then capture the next fe.signal() command after the move state started, as the
			// user's scripts may be remapping the signal to another value.  If that is the case, we
			// let the signal proceed (and set move_triggered below... at step 2 of 2)
			//
			if (( move_state != FeInputMap::LAST_COMMAND )
					&& ( from_ui || ( move_triggered != FeInputMap::LAST_COMMAND )))
				continue;

			if ( from_ui )
			{
				// setup variables to test for when the navigation keys are held down
				move_state = c;
				move_timer.restart();
				move_event = ev;
				move_triggered = FeInputMap::LAST_COMMAND;
				command_timer.restart();
			}

			//
			// Map Up/Down/Left/Right/Back to their default action now
			//
			if ( FeInputMap::is_ui_command( c ) )
			{
				//
				// Give the script the option to handle the (pre-map) action.
				//
				if ( feVM.script_handle_event( c ) )
				{
					FeDebug() << "Command intercepted by script handler: "
						<< FeInputMap::commandStrings[c] << std::endl;

					redraw=true;
					continue;
				}

				soundsys.sound_event( c );

				//
				// Handle special case Back UI button behaviour
				//
				if ( c == FeInputMap::Back )
				{
					//
					// If a display shortcut was used previously, then the "Back" UI
					// button goes back to the previous display accordingly...
					//
					if ( feSettings.back_displays_available() )
					{
						if ( feSettings.set_display( -1, true ) )
							feVM.load_layout();
						else
							feVM.update_to_new_list( 0, true );

						redraw=true;
						continue;
					}

					//
					// If FE is configured to group clones and is presently showing a group,
					// then the "Back" UI button goes back to the main romlist...
					//
					if (( feSettings.get_info_bool( FeSettings::GroupClones ) )
						&& ( feSettings.switch_from_clone_group() ))
					{
						feVM.update_to_new_list( 0, true );
						continue;
					}

					//
					// If FE is configured to show displays menu on startup, then the "Back" UI
					// button goes back to that menu accordingly...
					//
					if (( feSettings.get_startup_mode() == FeSettings::ShowDisplaysMenu )
						&& ( feSettings.get_present_state() == FeSettings::Layout_Showing )
						&& ( feSettings.get_current_display_index() >= 0 ))
					{
						FeVM::cb_signal( "displays_menu" );
						redraw=true;
						continue;
					}
				}

				c = feSettings.get_default_command( c );
				if ( c == FeInputMap::LAST_COMMAND )
				{
					redraw=true;
					continue;
				}
			}

			//
			// Give the script the option to handle the command.
			//
			if ( feVM.script_handle_event( c ) )
			{
				FeDebug() << "Command intercepted by script handler: "
					<< FeInputMap::commandStrings[c] << std::endl;

				redraw=true;
				continue;
			}

			//
			// Check if we need to get out of intro mode
			//
			if ( feSettings.get_present_state() == FeSettings::Intro_Showing )
			{
				FeSettings::StartupModeType mode = feSettings.get_startup_mode();
				move_state = FeInputMap::LAST_COMMAND;
				move_triggered = FeInputMap::LAST_COMMAND;
				move_last_triggered = 0;
				redraw = true;
				bool has_layout = false;

				// Clear any commands the intro may have queued up
				c = FeInputMap::LAST_COMMAND;
				feVM.clear_commands();

				if ( initial_load && mode == FeSettings::LaunchLastGame )
				{
					// Only LaunchLastGame on initial_load, not after manually triggered intro
					has_layout = feSettings.select_last_launch( initial_load );
					if ( has_layout )
					{
						feVM.load_layout( initial_load );
						initial_load = false;
					}
					launch_game = true;
				}

				if ( mode == FeSettings::ShowDisplaysMenu )
				{
					// Update command to show DisplaysMenu in the current loop
					has_layout = feSettings.has_custom_displays_menu();
					c = FeInputMap::DisplaysMenu;
				}

				if ( !has_layout )
				{
					feVM.load_layout( initial_load );
					initial_load = false;
				}

				if ( c == FeInputMap::LAST_COMMAND ) continue;
			}

			//
			// KEY REPEAT LOGIC (2 of 2)
			//
			// If we get here then this is the first pass through input potentially being held down
			// (i.e. repeat navigation).  Since we now know what the ultimate value of c is now, record
			// it in move_triggered
			//
			// ( !from_ui && ( move_triggered == LAST_COMMAND ) ) will catch where remapping by script
			//
			// ( move_state == LAST_COMMAND ) will catch the regular case with no remapping
			//
			if (( !from_ui && ( move_triggered == FeInputMap::LAST_COMMAND ))
					|| ( move_state != FeInputMap::LAST_COMMAND ))
			{
				if ( FeInputMap::is_repeatable_command( c ) )
					move_triggered = c;
			}

			//
			// Default command handling
			//
			FeDebug() << "Handling command: " << FeInputMap::commandStrings[c] << std::endl;

			soundsys.sound_event( c );
			if ( feVM.handle_event( c ) )
				redraw = true;
			else
			{
				// handle the things that fePresent doesn't do
				switch ( c )
				{

				// Force window re-init
				case FeInputMap::ResetWindow:
				{
					window.on_exit();
					window.initial_create();
					feVM.init_monitors();
					feVM.load_layout();
					continue;
				}

				// Reload the layout
				case FeInputMap::ReloadLayout:
				{
					feVM.load_layout();
					continue;
				}

				// Reload the configuration/emulator/window/layout
				case FeInputMap::ReloadConfig:
				{
					int old_mode = feSettings.get_window_mode();
					int old_aa = feSettings.get_antialiasing();
					int old_multimon = feSettings.get_multimon();

					feSettings.load();
					feSettings.on_joystick_connect(); // update joystick mappings

					soundsys.stop();
					soundsys.play_ambient();

					// Recreate window if the window mode changed
					if (( feSettings.get_window_mode() != old_mode )
						|| ( feSettings.get_antialiasing() != old_aa )
						|| ( feSettings.get_multimon() != old_multimon ))
					{
						window.on_exit();
						window.initial_create();
						feVM.init_monitors();
					}

					// Settings have changed, reload the display
					feSettings.set_display( feSettings.has_custom_displays_menu()
						? feSettings.get_current_display_index()
						: feSettings.get_selected_display_index() // in case config has removed custom display
					);
					feVM.load_layout();
					feVM.reset_screen_saver();
					continue;
				}

				case FeInputMap::Exit:
					{
						if ( feSettings.get_startup_mode() != FeSettings::ShowDisplaysMenu && feSettings.has_custom_displays_menu() )
						{
							int display_index = feSettings.get_current_display_index();
							if ( display_index == -1 && last_display_index != -1 )
							{
								display_index = last_display_index;
								last_display_index = -1;
								feSettings.set_display( display_index );
								feVM.load_layout();
								redraw=true;
								break;
							}
						}
						if ( feOverlay.common_exit() )
							exit_selected=true;

						redraw=true;
					}
					break;

				case FeInputMap::ExitToDesktop:
					exit_selected = true;
					break;

				case FeInputMap::ReplayLastGame:
					if ( feSettings.select_last_launch() )
						feVM.load_layout();
					else
						feVM.update_to_new_list();

					launch_game=true;
					redraw=true;
					break;

				case FeInputMap::Select:
					launch_game=true;
					break;

				case FeInputMap::ToggleMute:
					feSettings.set_mute( !feSettings.get_mute() );
					break;

				case FeInputMap::ScreenShot:
					{
						std::string filename;
						get_available_filename( feSettings.get_config_dir(),
										"screen", ".png", filename );

						sf::RenderWindow &w = window.get_win();
						sf::Texture texture;
						if ( texture.resize({ w.getSize().x, w.getSize().y }))
							texture.update( w );
						sf::Image sshot_img = texture.copyToImage();
						std::ignore = sshot_img.saveToFile( filename );
					}
					break;

				case FeInputMap::Configure:
					config_mode = true;
					break;

				case FeInputMap::InsertGame:
					{
						std::vector<std::string> options = {
							_( "Insert Game Entry" ), // 0
							_( "Insert Display Shortcut" ), // 1
							_( "Insert Command Shortcut" ) // 2
						};
						int sel_idx = feOverlay.common_list_dialog( _( "Insert Menu Entry" ), options, 0, -1, c );

						if ( sel_idx == FeOverlay::ExitToDesktop )
							exit_selected = true;

						if ( sel_idx < 0 ) break;

						std::string emu_name;
						std::string def_name;

						switch ( sel_idx )
						{
						case 0:
							{
								def_name = _( "Blank Game" );
								std::vector<std::string> tl;
								feSettings.get_list_of_emulators( tl );
								if ( !tl.empty() )
									emu_name = tl[0];
							}
							break;

						case 1:
							{
								emu_name = "@";
								def_name = _( "Display Shortcut" );

								if ( feSettings.displays_count() > 0 )
									def_name = feSettings.get_display( 0 )
										->get_info( FeDisplayInfo::Name );
							}
							break;

						case 2:
							emu_name = "@exit";
							def_name = _( "Exit" );
							break;

						default:
							break;
						};

						FeRomInfo new_entry( def_name );
						new_entry.set_info( FeRomInfo::Title, def_name );
						new_entry.set_info( FeRomInfo::Emulator, emu_name );

						int f_idx = feSettings.get_current_filter_index();

						FeRomInfo dummy;
						FeRomInfo *r = feSettings.get_rom_absolute(
							f_idx, feSettings.get_rom_index( f_idx, 0 ) );

						if ( !r )
							r = &dummy;

						// initial update shows new entry behind config dialog
						feSettings.update_romlist_after_edit( *r, new_entry, FeSettings::InsertEntry );
						feSettings.init_display();
						feVM.update_to_new_list();

						if ( feOverlay.edit_game_dialog( 0, c ) )
							feVM.update_to_new_list();

						redraw=true;
					}
					break;

				case FeInputMap::EditGame:
					if ( feOverlay.edit_game_dialog( 0, c ) )
						feVM.update_to_new_list();

					redraw=true;
					break;

				case FeInputMap::LayoutOptions:
					{
						bool preview = feSettings.get_info_bool( FeSettings::LayoutPreview );
						while ( feOverlay.layout_options_dialog( preview, layout_sel, c ) )
						{
							feVM.load_layout();
							if ( !preview ) break;
						}
						redraw = true;
					}
					break;

				case FeInputMap::PluginOptions:
					{
						if ( feOverlay.plugin_options_dialog( plugin_sel, c ) )
							feVM.load_layout();
						redraw = true;
						break;
					}

				case FeInputMap::DisplaysMenu:
					{
						// If user has configured a custom layout for the display
						if ( feSettings.has_custom_displays_menu() )
						{
							int display_index = feSettings.get_current_display_index();

							if (display_index == -1 && last_display_index != -1)
							{
								// Already showing the custom display menu
								if ( feSettings.get_info_bool( FeSettings::QuickMenu ) ) {
									// Return to last display if quick_menu option is set
									display_index = last_display_index;
									last_display_index = -1;
								}
								else
								{
									// Otherwise remain in the custom layout (do nothing)
									break;
								}
							}
							else
							{
								// Set display to -1 to load the custom display menu layout
								last_display_index = display_index;
								display_index = -1;
							}

							feSettings.set_display( display_index );
							feVM.load_layout( initial_load );
							initial_load = false;
							redraw=true;
							break;
						}

						//
						// If no custom layout is configured, then we simply show the
						// displays menu the same way as any other popup menu...
						//
						std::vector<std::string> disp_names;
						std::vector<int> disp_indices;
						int current_idx;

						std::string title;
						feSettings.get_displays_menu( title, disp_names, disp_indices, current_idx );

						int exit_opt=-999;
						if ( feSettings.get_info_bool( FeSettings::DisplaysMenuExit ) )
						{
							//
							// Add an exit option at the end of the lists menu
							//
							std::string exit_str;
							feSettings.get_exit_message( exit_str );
							disp_names.push_back( exit_str );
							exit_opt = disp_names.size() - 1;
						}

						if ( !disp_names.empty() )
						{
							int sel_idx = feOverlay.common_list_dialog(
								title,
								disp_names,
								current_idx,
								-1,
								c );

							if ( sel_idx == exit_opt )
							{
								if ( feOverlay.common_exit() )
									exit_selected = true;
							}
							else if ( sel_idx >= 0 )
							{
								if ( feSettings.set_display( disp_indices[sel_idx] ) )
									feVM.load_layout();
								else
									feVM.update_to_new_list( 0, true );
							}
							else if ( sel_idx == FeOverlay::ExitToDesktop )
							{
								exit_selected = true;
							}

							redraw=true;
						}
					}
					break;

				case FeInputMap::FiltersMenu:
					{
						std::vector<std::string> names_list;
						feSettings.get_current_display_filter_names( names_list );

						int sel_idx = feOverlay.common_list_dialog(
										_( "Filters" ),
										names_list,
										feSettings.get_current_filter_index(),
										-1,
										c );

						if ( sel_idx >= 0 )
						{
							feSettings.set_current_selection( sel_idx, -1 );
							feVM.update_to_new_list();
						}
						else if ( sel_idx == FeOverlay::ExitToDesktop )
						{
							exit_selected = true;
						}

						redraw=true;
					}
					break;

				case FeInputMap::ToggleFullscreen:
					{
						int old_mode = feSettings.get_window_mode();
						int is_window = ( old_mode == FeSettings::WindowType::Window || old_mode == FeSettings::WindowType::WindowNoBorder );
						if ( is_window ) last_window_mode = old_mode; else last_fullscreen_mode = old_mode;
						int new_mode = is_window ? last_fullscreen_mode : last_window_mode;

						feSettings.set_info( FeSettings::WindowMode, FeSettings::windowModeTokens[new_mode] );

						if ( feSettings.get_window_mode() != old_mode )
						{
							feSettings.save();
							window.on_exit();
							window.initial_create();
							feVM.init_monitors();
							feVM.load_layout();
						}
					}
					break;

				case FeInputMap::ToggleFavourite:
					{
						bool new_state = !feSettings.get_current_fav();
						bool confirm_fav = true;

						if ( feSettings.get_info_bool( FeSettings::ConfirmFavourites ) )
						{
							std::string title = feSettings.get_rom_info( 0, 0, FeRomInfo::Title );
							std::string msg = new_state
								? _( "Add '$1' to Favourites?", { title })
								: _( "Remove '$1' from Favourites?", { title });

							int sel_idx = feOverlay.confirm_dialog(
								msg,
								false,
								FeInputMap::ToggleFavourite );

							confirm_fav = ( sel_idx == 0 );
							if ( sel_idx == FeOverlay::ExitToDesktop )
								exit_selected = true;
						}

						if ( confirm_fav )
						{
							// Update without reset if the list has not been changed
							// - This will allow layout lists to display the changed favs
							bool list_changed = feSettings.set_fav_current( new_state );
							feVM.update_to_new_list( 0, list_changed );
							feVM.on_transition( ChangedTag, FeRomInfo::Favourite );
						}

						redraw = true;
					}
					break;

				case FeInputMap::ToggleTags:
					{
						int sel_idx = feOverlay.tags_dialog( 0, c );

						if ( sel_idx == FeOverlay::ExitToDesktop )
							exit_selected = true;

						redraw = true;
					}
					break;

				default:
					break;
				}

				// Overlay menu_commands are populated when the `QuickMenu` setting is true
				// - They allow other menu commands to behave as a menu exit
				// - Post the commands back onto the queue to switch to the next menu
				FeInputMap::Command menu_command = feOverlay.get_menu_command();
				if ( menu_command != FeInputMap::Command::Back )
				{
					feOverlay.clear_menu_command();
					feVM.post_command( menu_command );
				}
			}
		}

		// End loop early and go directly to config, preventing a layout frame drawn between menu switching
		if ( config_mode ) continue;

#ifdef USE_LIBCURL
		if ( feSettings.get_info_bool( FeSettings::CheckForUpdates ) && versionChecker.check_result() )
		{
			std::string message = "Attract-Mode Plus update is available\nNew version: " + versionChecker.get_remote_version() + "\nCurrent version: " + versionChecker.get_current_version();
			feOverlay.common_basic_dialog( message, {"OK"}, 0, 0 );
		}
#endif

		//
		// Determine if we have to do anything because a key is being held down
		//
		if ( move_state != FeInputMap::LAST_COMMAND )
		{
			bool cont=false;

			if ( move_event->is<sf::Event::KeyPressed>() )
			{
				const auto* key = move_event->getIf<sf::Event::KeyPressed>();
				if ( key && sf::Keyboard::isKeyPressed( key->code ))
					cont=true;
			}

			else if ( move_event->is<sf::Event::MouseButtonPressed>() )
			{
				const auto* btn = move_event->getIf<sf::Event::MouseButtonPressed>();
				if ( sf::Mouse::isButtonPressed( btn->button ))
					cont=true;
			}

			else if ( move_event->is<sf::Event::JoystickButtonPressed>() )
			{
				const auto* btn = move_event->getIf<sf::Event::JoystickButtonPressed>();
				if ( sf::Joystick::isButtonPressed( btn->joystickId, btn->button ))
					cont=true;
			}

			else if ( move_event->is<sf::Event::JoystickMoved>() )
			{
				const auto* mov = move_event->getIf<sf::Event::JoystickMoved>();
				float pos = sf::Joystick::getAxisPosition( mov->joystickId,	mov->axis );
				if ( std::abs( pos ) > feSettings.get_joy_thresh() )
					cont=true;
			}

			if ( cont )
			{
				command_timer.restart();
				int t = move_timer.getElapsedTime().asMilliseconds();
				if (( t > feSettings.selection_delay() ) && ( t - move_last_triggered > feSettings.selection_speed() ))
				{
					if (( move_triggered == FeInputMap::LAST_COMMAND )
						|| feVM.script_handle_event( move_triggered ))
					{
						//
						// If we are ending a "repeatable" input (prev/next game etc) or a UI input
						// that maps to a repeatable input (i.e. "Up"->"prev game") by the script event
						// returning true then we trigger the "End Navigation" transition now
						//
						if ( move_triggered != FeInputMap::LAST_COMMAND )
						{
							feVM.update_to( EndNavigation, false );
							feVM.on_transition( EndNavigation, 0 );
						}

						move_state = FeInputMap::LAST_COMMAND;
						move_triggered = FeInputMap::LAST_COMMAND;
						move_last_triggered = 0;

						redraw=true;
					}
					else
					{
						soundsys.sound_event( c );

						move_last_triggered = t;
						int step = 1;

						int max_step = feSettings.selection_max_step();
						if ( max_step > 1 )
						{
							int s = t / 400;

							// Ramp up the step depending how long the key has been held
							//
							// s:    1                    8                    15             20    22 23  24  25  26   27   28   29    30
							// step: 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 4, 4, 4, 4, 4, 6, 6, 8, 16, 32, 64, 128, 256, 512, 1024, 2048
							//
							if ( s < 8 )
								step = 1;
							else if ( s < 15 )
								step = 2;
							else if ( s < 20 )
								step = 4;
							else if ( s < 22 )
								step = 6;
							else
								step = 1 << std::min( s - 19, 11 ); // sanity check - don't go above 2^11 (2048)

							// make sure we don't go over user specified max
							step = std::min( step, max_step );
						}

						switch ( move_triggered )
						{
						// Key repeat. First press in FePresent::handle_event()
						case FeInputMap::PrevGame: step = -step; break;
						case FeInputMap::NextGame: break; // do nothing
						case FeInputMap::PrevPage: step *= -feVM.get_page_size(); break;
						case FeInputMap::NextPage: step *= feVM.get_page_size(); break;
						case FeInputMap::PrevFavourite:
							{
								int temp = feSettings.get_prev_fav_offset();
								step = ( temp < 0 ) ? temp : 0;
							}
							break;
						case FeInputMap::NextFavourite:
							{
								int temp = feSettings.get_next_fav_offset();
								step = ( temp > 0 ) ? temp : 0;
							}
							break;
						case FeInputMap::PrevLetter:
							{
								int temp = feSettings.get_next_letter_offset( -1 );
								step = ( temp < 0 ) ? temp : 0;
							}
							break;
						case FeInputMap::NextLetter:
							{
								int temp = feSettings.get_next_letter_offset( 1 );
								step = ( temp > 0 ) ? temp : 0;
							}
							break;
						default: break;
						}

						//
						// Limit the size of our step so that there is no wrapping around at the end of the list
						//
						int curr_sel = feSettings.get_rom_index( feSettings.get_current_filter_index(), 0 );
						if ( ( curr_sel + step ) < 0 )
							step = -curr_sel;
						else
						{
							int list_size = feSettings.get_filter_size( feSettings.get_current_filter_index() );
							if ( ( curr_sel + step ) >= list_size )
								step = list_size - curr_sel - 1;
						}

						if ( step != 0 )
						{
							feVM.change_selection( step, false );
							redraw=true;
						}
					}
				}
			}
			else
			{
				//
				// If we are ending a "repeatable" input (prev/next game etc) or a UI input
				// that maps to a repeatable input (i.e. "Up"->"prev game") then trigger the
				// "End Navigation" stuff now
				//
				if ( move_triggered != FeInputMap::LAST_COMMAND )
				{
					feVM.update_to( EndNavigation, false );
					feVM.on_transition( EndNavigation, 0 );
				}

				move_state = FeInputMap::LAST_COMMAND;
				move_triggered = FeInputMap::LAST_COMMAND;
				move_last_triggered = 0;

				redraw=true;
			}
		}

		if ( feSettings.get_present_state() == FeSettings::Layout_Showing && ( feSettings.displays_count() < 1 ) )
		{
			feOverlay.splash_logo( _( "Press TAB to Configure" ) );
			continue;
		}

		// Save nv a short time after the last ui command
		if ( command_timer.isRunning() && command_timer.getElapsedTime().asMilliseconds() > 5000 )
		{
			command_timer.reset();
			feSettings.save_state();
			feVM.save_script_nv();
			feVM.save_layout_nv();
		}

		//
		// Log any unexpected loss of window focus
		//
		if ( has_focus )
		{
			if ( !window.hasFocus() )
			{
				has_focus = false;
				FeLog() << " ! Unexpectedly lost focus to: " << get_focus_process() << std::endl;
			}
		}
		else
			has_focus = window.hasFocus();

		if ( feVM.tick() )
			redraw=true;

		if ( feVM.saver_activation_check() )
		{
			soundsys.sound_event( FeInputMap::ScreenSaver );
			command_timer.restart();
		}

		if ( redraw || !feSettings.get_info_bool( FeSettings::PowerSaving ) )
		{
			feVM.redraw_surfaces();

			// begin drawing
			window.clear();
			window.draw( feVM );
			window.display();
			redraw=false;
		}
		else
			sf::sleep( sf::milliseconds( 15 ) );

		soundsys.tick();
	}

	window.on_exit();
	feVM.on_stop_frontend();

	if ( window.isOpen() )
		window.close();

	soundsys.stop();

#ifdef USE_LIBCURL
	versionChecker.reset();
	curl_global_cleanup();
#endif

	FeDebug() << "Attract-Mode Plus ended normally" << std::endl;
	return 0;
}
