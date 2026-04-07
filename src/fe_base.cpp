/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-2018 Andrew Mickelson
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

#include "fe_base.hpp"
#include "fe_util.hpp"
#include "base64.hpp"
#include <SFML/Config.hpp>

extern "C"
{
#include <libavutil/log.h>
}

#include <string>
#include <sstream>
#include <iomanip>
#include <SDL3/SDL.h>
#include "nowide/fstream.hpp"
#include "nowide/iostream.hpp"

#define FE_NAME_D           "Attract-Mode Plus"
#define FE_AUTHOR_D         "Andrew Mickelson, Radek Dutkiewicz & Chadnaut"
#define FE_COPYRIGHT_D      "Copyright © 2013-2026"

const char *FE_NAME         = FE_NAME_D;
const char *FE_COPYRIGHT    = FE_NAME_D " " FE_VERSION_D " " FE_BUILD_D "\n" FE_AUTHOR_D "\n" FE_COPYRIGHT_D;
const char *FE_VERSION      = FE_VERSION_D;
const char *FE_BUILD_NUMBER = FE_BUILD_D;

const char *FE_WHITESPACE   = " \t\r";
const char *FE_DIR_TOKEN    = "<DIR>";
const float FE_SCORE_MAX    = 5.0;

const char *FE_TAG_ICON             = "🏷";
const char *FE_HEART_ICON           = "♥";
const char *FE_HEART_OUTLINE_ICON   = "♡";
const char *FE_STAR_ICON            = "★";
const char *FE_STAR_OUTLINE_ICON    = "☆";
const char *FE_STAR_HALF_ICON       = "⯪";
const char *FE_YES_ICON             = "☒";
const char *FE_NO_ICON              = "☐";

const char *FE_TAG_PREFIX       = "🏷 ";
const char *FE_TAG_DELIM        = "  ";

const char *FE_DEFAULT_LANGUAGE          = "en";
const char *FE_DEFAULT_ARTWORK           = "snap";
const char *FE_EMULATOR_SUBDIR           = "emulators/";
const char *FE_TEMPLATE_SUBDIR          = "templates/";
const char *FE_TEMPLATE_EMULATOR_SUBDIR = "templates/emulators/";
const char *FE_EMULATOR_TEMPLATES_SUBDIR = "emulators/templates/"; // Deprecated, left for migration cleanup
const char *FE_EMULATOR_FILE_EXTENSION   = ".cfg";
const char *FE_EMULATOR_DEFAULT          = "default-emulator.cfg";

const std::uint32_t FE_CACHE_VERSION = FE_CACHE_VERSION_D;

namespace {
	nowide::ofstream g_logfile;
#ifdef SDL_PLATFORM_WINDOWS
	nowide::ofstream g_nullstream( "NUL" );
#else
	nowide::ofstream g_nullstream( "/dev/null" );
#endif
	enum FeLogLevel g_log_level=FeLog_Info;

	void ffmpeg_log_callback( void *ptr, int level, const char *fmt, va_list vargs )
	{
		if ( level <= av_log_get_level() )
		{
			char buff[256];
			vsnprintf( buff, 256, fmt, vargs );
			FeLog() << "FFmpeg: " << buff;
		}
	}
};

std::ostream &FeDebug()
{
	if ( g_log_level == FeLog_Debug )
		return FeLog();
	else
		return g_nullstream;
}

std::ostream &FeLog()
{
	if ( g_log_level == FeLog_Silent )
		return g_nullstream;

	if ( g_logfile.is_open() )
		return g_logfile;
	else
		return nowide::cout;
}

void fe_set_log_file( const std::string &fn )
{
	if ( fn.empty() )
		g_logfile.close();
	else
		g_logfile.open( fn.c_str() );
}

void fe_set_log_level( enum FeLogLevel f )
{
	g_log_level = f;
	if ( f == FeLog_Silent )
		av_log_set_callback( NULL );
	else
	{
		av_log_set_callback( ffmpeg_log_callback );
		av_log_set_level( ( f == FeLog_Debug ) ? AV_LOG_VERBOSE : AV_LOG_ERROR );
	}
}

const char *fe_get_log_level_string()
{
	switch ( g_log_level )
	{
		case FeLog_Silent:
			return "silent";
		case FeLog_Info:
			return "info";
		case FeLog_Debug:
			return "debug";
		default:
			return "";
	}
}

void fe_print_version()
{
	const int sdl_version = SDL_GetVersion();

	FeLog() << "--------------------------------------------------------------------------------" << std::endl;
	FeLog() << FE_NAME << " " << FE_VERSION << " " << FE_BUILD_NUMBER << "("
		<< get_OS_string()
		<< ", SFML " << SFML_VERSION_MAJOR << '.' << SFML_VERSION_MINOR
		<< "." << SFML_VERSION_PATCH
		<< ", SDL "
		<< SDL_VERSIONNUM_MAJOR( sdl_version ) << '.'
		<< SDL_VERSIONNUM_MINOR( sdl_version ) << '.'
		<< SDL_VERSIONNUM_MICRO( sdl_version );

	if (( SDL_WasInit( SDL_INIT_VIDEO ) & SDL_INIT_VIDEO ) != 0 )
	{
		if ( const char *driver = SDL_GetCurrentVideoDriver() )
			FeLog() << " [" << driver << ']';
	}

	FeLog()
#ifdef USE_LIBARCHIVE
		<< ", 7z"
#endif
#ifdef USE_LIBCURL
		<< ", Curl"
#endif
		<< ")" << std::endl;
	print_ffmpeg_version_info();
	FeLog() << std::endl;
}

void FeBaseConfigurable::invalid_setting(
	const std::string & fn,
	const char *base,
	const std::string &setting,
	const char **valid1,
	const char **valid2,
	const char *label )
{
	FeLog() << "Unrecognized \"" << base << "\" " << label
		<< " of \"" << setting << "\"";

	if ( !fn.empty() )
		FeLog() << " in file: " << fn;

	FeLog() << ".";

	int i=0;
	if ( valid1 && valid1[i] )
	{
		FeLog() << "  Valid " << label <<"s are: " << valid1[i++];

		while (valid1[i])
			FeLog() << ", " << valid1[i++];
	}

	if ( valid2 )
	{
		i=0;
		while (valid2[i])
			FeLog() << ", " << valid2[i++];
	}

	FeLog() << std::endl;
}

const int DEBUG_MAX_LINES = 20;

bool FeBaseConfigurable::load_from_file( const std::string &filename, const char *sep )
{
	nowide::ifstream myfile( filename, std::ios::binary );
	if ( !myfile.is_open() )
		return false;

	bool debug = g_log_level == FeLog_Debug;
	int count = 0;
	std::string line, setting, value;

	while ( myfile.good() )
	{
		getline( myfile, line );
		if ( line_to_setting_and_value( line, setting, value, sep ) )
		{
			if ( debug && ( count <= DEBUG_MAX_LINES ))
			{
				debug = count++ < DEBUG_MAX_LINES;
				FeDebug() << "[" << filename << "] " << std::setw(15) << std::left << setting << " = " << value << std::endl;
				if ( !debug )
					FeDebug() << "[" << filename << "] DEBUG_MAX_LINES exceeded, truncating further debug output from this file." << std::endl;
			}
			process_setting( setting, value, filename );
		}
	}

	myfile.close();
	return true;
}

bool FeBaseConfigurable::load_from_string( const std::string &content, const char *sep )
{
	std::stringstream ss(content);
	std::string line, setting, value;

	while ( std::getline( ss, line ) )
		if ( line_to_setting_and_value( line, setting, value, sep ) )
			process_setting( setting, value, "" );

	return true;
}

bool FeBaseConfigurable::load_from_binary( const char *content, const char *sep )
{
	std::vector<unsigned char> data = base64_decode( content );
	std::string value = std::string( data.begin(), data.end() );
	return load_from_string( value, sep );
}
