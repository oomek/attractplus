/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-15 Andrew Mickelson
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

#include "fe_settings.hpp"
#include "fe_util.hpp"
#include <iostream>
#include <cstring>
#include <sstream>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Window/Context.hpp>

#include <SFML/OpenGL.hpp>
#define GL_SHADING_LANGUAGE_VERSION    0x8B8C

void write_section( std::string title )
{
	FeLog() << std::endl << title << ":" << std::endl;
}

void write_option( std::string args, std::string help = "" )
{
	FeLog() << "  " << std::setw(36) << std::left << args;
	if ( !help.empty() ) FeLog() << " " << help;
	FeLog() << std::endl;
}

void process_args( int argc, char *argv[],
			std::string &config_path,
			bool &process_console,
			std::string &startup_display,
			std::string &startup_filter,
			std::string &log_file,
			FeLogLevel &log_level,
			bool &window_topmost,
			std::vector<int> &window_args )
{
	//
	// Deal with command line arguments
	//
	std::vector <FeImportTask> task_list;
	std::string output_name;
	FeFilter filter( "" );
	bool full=false;

	int next_arg=1;

	while ( next_arg < argc )
	{
		if (( strcmp( argv[next_arg], "-c" ) == 0 )
				|| ( strcmp( argv[next_arg], "--config" ) == 0 ))
		{
			next_arg++;
			if ( next_arg < argc )
			{
				config_path = argv[next_arg];
				next_arg++;
			}
			else
			{
				FeLog() << "Error, no config directory specified with --config option." << std::endl;
				exit(1);
			}
		}
		else if (( strcmp( argv[next_arg], "-g" ) == 0 )
				|| ( strcmp( argv[next_arg], "--logfile" ) == 0 ))
		{
			next_arg++;
			if ( next_arg < argc )
			{
				log_file = argv[next_arg];
				next_arg++;
			}
			else
			{
				FeLog() << "Error, no log file specified with --log option." << std::endl;
				exit(1);
			}
		}
		else if (( strcmp( argv[next_arg], "-l" ) == 0 )
				|| ( strcmp( argv[next_arg], "--loglevel" ) == 0 ))
		{
			next_arg++;
			if ( next_arg < argc )
			{
				if ( strcmp( argv[next_arg], "silent" ) == 0 )
				{
					log_level = FeLog_Silent;
					next_arg++;
				}
				else if ( strcmp( argv[next_arg], "info" ) == 0 )
				{
					log_level = FeLog_Info;
					next_arg++;
				}
				else if ( strcmp( argv[next_arg], "debug" ) == 0 )
				{
					log_level = FeLog_Debug;
					next_arg++;
				}
				else
				{
					FeLog() << "Unrecognized loglevel: " << argv[next_arg] << std::endl;
					exit( 1 );
				}
			}
			else
			{
				FeLog() << "Error, no log level specified with --loglevel option." << std::endl;
				exit(1);
			}

			next_arg++;
		}
		else if (( strcmp( argv[next_arg], "-b" ) == 0 )
				|| ( strcmp( argv[next_arg], "--build-romlist" ) == 0 ))
		{
			next_arg++;
			int first_cmd_arg = next_arg;

			for ( ; next_arg < argc; next_arg++ )
			{
				if ( argv[next_arg][0] == '-' )
					break;

				task_list.push_back( FeImportTask( FeImportTask::BuildRomlist, argv[next_arg] ));
			}

			if ( next_arg == first_cmd_arg )
			{
				FeLog() << "Error, no target emulators specified with --build-romlist option."
							<<  std::endl;
				exit(1);
			}
		}
		else if (( strcmp( argv[next_arg], "-i" ) == 0 )
				|| ( strcmp( argv[next_arg], "--import-romlist" ) == 0 ))
		{
			std::string my_filename;
			std::string my_emulator;

			next_arg++;
			if ( next_arg < argc )
			{
				my_filename = argv[next_arg];
				next_arg++;
			}
			else
			{
				FeLog() << "Error, no filename specified with --import-romlist option." << std::endl;
				exit(1);
			}

			if ( ( next_arg < argc ) && ( argv[next_arg][0] != '-' ) )
			{
				my_emulator = argv[ next_arg ];
				next_arg++;
			}

			task_list.push_back( FeImportTask( FeImportTask::ImportRomlist, my_emulator, my_filename ));
		}
		else if (( strcmp( argv[next_arg], "-o" ) == 0 )
				|| ( strcmp( argv[next_arg], "--output" ) == 0 ))
		{
			next_arg++;
			if ( next_arg < argc )
			{
				output_name = argv[next_arg];
				next_arg++;
			}
			else
			{
				FeLog() << "Error, no output filename specified with --output option." << std::endl;
				exit(1);
			}
		}
		else if (( strcmp( argv[next_arg], "-u" ) == 0 )
				|| ( strcmp( argv[next_arg], "--full" ) == 0 ))
		{
			full = true;
			next_arg++;
		}
		else if (( strcmp( argv[next_arg], "-F" ) == 0 )
				|| ( strcmp( argv[next_arg], "-f" ) == 0 )
				|| ( strcmp( argv[next_arg], "--filter" ) == 0 ))
		{
			FeRule rule;

			next_arg++;
			if ( next_arg < argc )
			{
				if ( rule.process_setting( "rule", argv[next_arg], "" ) != 0 )
				{
					// Error message already displayed
					exit(1);
				}
				next_arg++;
			}
			else
			{
				FeLog() << "Error, no rule specified with --filter option." << std::endl;
				exit(1);
			}

			filter.get_rules().push_back( rule );
		}
		else if (( strcmp( argv[next_arg], "-E" ) == 0 )
				|| ( strcmp( argv[next_arg], "-e" ) == 0 )
				|| ( strcmp( argv[next_arg], "--exception" ) == 0 ))
		{
			FeRule ex;

			next_arg++;
			if ( next_arg < argc )
			{
				if ( ex.process_setting( "exception", argv[next_arg], "" ) != 0 )
				{
					// Error message already displayed
					exit(1);
				}
				next_arg++;
			}
			else
			{
				FeLog() << "Error, no exception specified with --exception option." << std::endl;
				exit(1);
			}

			filter.get_rules().push_back( ex );
		}
		else if (( strcmp( argv[next_arg], "-s" ) == 0 )
				|| ( strcmp( argv[next_arg], "--scrape-art" ) == 0 ))
		{
			next_arg++;
			int first_cmd_arg = next_arg;

			for ( ; next_arg < argc; next_arg++ )
			{
				if ( argv[next_arg][0] == '-' )
					break;

				task_list.push_back( FeImportTask( FeImportTask::ScrapeArtwork, argv[next_arg] ));
			}

			if ( next_arg == first_cmd_arg )
			{
				FeLog() << "Error, no target emulators specified with --scrape-art option."
							<<  std::endl;
				exit(1);
			}
		}
		else if (( strcmp( argv[next_arg], "-v" ) == 0 )
				|| ( strcmp( argv[next_arg], "--version" ) == 0 ))
		{
			fe_print_version();
			FeLog() << std::endl;

			sf::Context c; // initializes GL so glGetString() call will work

			FeLog() << "OpenGL " << glGetString( GL_VERSION ) << std::endl
				<< " - vendor  : " << glGetString( GL_VENDOR ) << std::endl
				<< " - renderer: " << glGetString( GL_RENDERER ) << std::endl
				<< " - shader  : " << glGetString( GL_SHADING_LANGUAGE_VERSION ) << std::endl << std::endl;

			if ( sf::Shader::isAvailable() )
				FeLog() << "Shaders are available." << std::endl;
			else
				FeLog() << "Shaders are not available." << std::endl;

			exit(0);
		}
		else if (( strcmp( argv[next_arg], "-t" ) == 0 )
				|| ( strcmp( argv[next_arg], "--topmost" ) == 0 ))
		{
			window_topmost = true;
			next_arg++;
		}
		else if (( strcmp( argv[next_arg], "-d" ) == 0 )
				|| ( strcmp( argv[next_arg], "--display" ) == 0 ))
		{
			next_arg++;
			startup_display = argv[next_arg];
			next_arg++;
			if ( next_arg < argc && argv[next_arg][0] != '-' )
			{
				startup_filter = argv[next_arg];
				next_arg++;
			}
		}
		else if (( strcmp( argv[next_arg], "-w" ) == 0 )
				|| ( strcmp( argv[next_arg], "--window" ) == 0 ))
		{
			next_arg++;

			for ( ; (next_arg < argc) && (window_args.size() < 4); next_arg++ )
			{
				// Window delimiter may be space (reads multiple args) or comma (splits single arg)
				// - Reads from argv until 4 window args are found
				std::istringstream iss(argv[next_arg]);
				std::string s;
				while ( std::getline( iss, s, ',' ) ) {
					window_args.push_back( atoi(s.c_str()) );
				}
			}

			// Must be 4 args, if not then clear them all
			if ( window_args.size() != 4 )
				window_args.clear();
		}
#ifndef SFML_SYSTEM_WINDOWS
		else if (( strcmp( argv[next_arg], "-n" ) == 0 )
				|| ( strcmp( argv[next_arg], "--console" ) == 0 ))
		{
			process_console=true;
			next_arg++;
		}
#endif
		else
		{
			int retval=1;
			if (( strcmp( argv[next_arg], "-h" ) != 0 )
				&& ( strcmp( argv[next_arg], "--help" ) != 0 ))
			{
				FeLog() << "Unrecognized command line option: " << argv[next_arg] <<  std::endl;
				retval=0;
			}

			FeLog() << FE_COPYRIGHT << std::endl;

			write_section("Usage");
			write_option( path_filename( argv[0] ) + " [option]..." );

			write_section( "Romlist Creation" );
			write_option( "-b, --build-romlist <emu>...", "Build romlists for the given emulator(s)" );
			write_option( "-i, --import-romlist <file> [emu]", "Import a romlist, optionally override emulator. Accepts: *.txt (AM), *.xml (Mame), *.lst (MameWah)" );
			write_option( "-f, --filter <rule>", "Apply the given filter when creating a romlist" );
			write_option( "-e, --exception <rule>", "Apply the given exception when creating a romlist" );
			write_option( "-o, --output <name>", "The name for the created romlist (will overwrite existing)" );
			write_option( "-u, --full", "Include all possible roms, used with --build-romlist (Mame only)" );

			write_section( "Artwork Scraping" );
			write_option( "-s, --scrape-art <emu>...", "Scrape missing artwork for the given emulator(s)" );

			write_section( "Options" );
			write_option( "-l, --loglevel (silent|info|debug)", "Set the logging level" );
			write_option( "-g, --logfile <file>", "Write log output to the given file" );
#ifndef SFML_SYSTEM_WINDOWS
			write_option( "-n, --console", "Enable script console" );
#endif
			write_option( "-c, --config <dir>", "Set the directory containing attract.cfg" );
			write_option( "-d, --display <display> [filter]", "Show the given Display and Filter on startup" );
			write_option( "-w, --window <x> <y> <w> <h>", "Set the position and size for window modes" );
			write_option( "-t, --topmost", "Keep the window always on top" );
			write_option( "-v, --version", "Show version information" );
			write_option( "-h, --help", "Show this message" );

			FeLog() << std::endl;
			exit( retval );
		}
	}

	if ( !task_list.empty() )
	{
		FeSettings feSettings( config_path );
		feSettings.load_from_file( feSettings.get_config_dir() + FE_CFG_FILE );

		int retval = feSettings.build_romlist( task_list, output_name, filter, full );
		exit( retval ? 0 : 1 );
	}
}
