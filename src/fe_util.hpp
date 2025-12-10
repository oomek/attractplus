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

#ifndef FE_UTIL_HPP
#define FE_UTIL_HPP

#include <vector>
#include <string>
#include <set>
#include <SFML/Config.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window/Clipboard.hpp>
#include "nowide/fstream.hpp"

#ifdef FE_DEBUG
#include <cassert>
#endif

#define FE_VERSION_INT( a, b, c ) ((a)<<16 | (b)<<8 | (c))

extern const char *FE_WHITESPACE;

namespace FeUtil
{
    std::string narrow( const std::wstring& s );
    std::wstring widen( const std::string& s );
}

//
// Utility functions for processing config files:
//
bool token_helper( const std::string &from,
	size_t &pos, std::string &token, const char *sep=";" );

//
// Substitute all occurrences of "from" that appear in "target" with the text from "to"
// - Returns the number of substitutions made
//
int perform_substitution(
	std::string &target,
	const std::string &from,
	const std::string &to
);

//
// Substitute placeholders in target with values in list, $1 = rep[0], etc
//
int perform_substitution(
	std::string &target,
	const std::vector<std::string> &rep
);

//
// Join string parts using delim
//
std::string str_join(
	const std::vector<std::string> &parts,
	const std::string &delim
);

// Trim whitespace
std::string ltrim( const std::string str );
std::string rtrim( const std::string str );
std::string trim( const std::string str );

//
//
std::string name_with_brackets_stripped( const std::string &name );

//
// Case insensitive check if filename has the specified extension/ending.
//
bool tail_compare(
	const std::string &filename,
	const std::string &extension );

bool tail_compare(
	const std::string &filename,
	const std::vector<std::string> &ext_list );

bool tail_compare(
	const std::string &filename,
	const char **ext_list );

// Returns lowercase copy of string
std::string lowercase( const std::string &value );

// Returns uppercase copy of string
std::string uppercase( const std::string &value );

//
// Case insensitive compare of one and two
// - Returns -1 if one < two, 1 if one > two, 0 if equal
//
int icompare( const std::string &one, const std::string &two );

//
// Case insensitive check if the base filename in the given path is equal to base.
// (i.e. check the part labeled "BASE" in the following path: "/a/b/c/BASE.ext")
//
bool base_compare( const std::string &path, const std::string &base );

typedef bool (*output_callback_fn)( const char *, void * );
typedef void (*launch_callback_fn)( void * );

// Optional settings (and return values!) that are sent to run_program() when running an emulator
//
class run_program_options_class
{
public:
	// [in] "joy_thresh" - joystick threshold, only used if exit_hotkey or pause_hotkey
	//                is mapped to a joystick
	int joy_thresh;

	// [in] "launch_cb" = callback function to call after program launched
	launch_callback_fn launch_cb;

	//	[in] "wait_cb" = callback function to call repeatedly while waiting for program return [windows only]
	launch_callback_fn wait_cb;

	//	[in] "launch_opaque" - opaque ptr to pass to the launch callback (and wait callback) function.
	//                   run_program() doesn't care what this is.
	void *launch_opaque;

	// [in] "exit_hotkey" - hotkey that when pressed will force exit of program
	//					(use empty for no hotkey checking)
	std::string exit_hotkey;

	// [in] "exit_hotkey" - hotkey that when pressed will pause the program
	//					(use empty for no hotkey checking)
	std::string pause_hotkey;

	// [out] "running_pid" - process id of the still running process (if pause hotkey pressed)
	// [out] "running_wnd" - window handle of the still running process (if pause hotkey pressed)
	unsigned int running_pid;
	void *running_wnd;

	// Default constructor
	//
	run_program_options_class();
};

//
// Run the specified program, blocks while program running
//
// Arguments:
//		"prog" - program to run
//		"args" - space separated args. An arg can be wrapped in double quotes
//					if it contains spaces.
//		"work_dir" - the working directory to run the program from. If empty, will
//			try to pick one based on any path elements in  the "prog" argument.
//		"cb" - callback function to call with stdout output from program
//		"opaque" - opaque ptr to pass to the callback function.  run_program()
//					doesn't care what this is.
//		"block" - if true, don't return until program is done execution
//					if false, "cb" and "opaque" are ignored
//		"opt" - optional settings and return values used when launching an emulator
//
//	Returns true if program ran successfully
//
bool run_program( const std::string &prog,
	const std::string &args,
	const std::string &work_dir,
	output_callback_fn cb = NULL,
	void *opaque=NULL,
	bool block=true,
	run_program_options_class *opt = NULL );

void resume_program(
	unsigned int pid,
	void *wnd,
	run_program_options_class *opt = NULL );

void kill_program( unsigned int pid );

bool process_exists( unsigned int pid );

//
// Utility functions for file processing:
//

// Replaces invalid filename characters with a dash
std::string sanitize_filename( const std::string &file );

// Returns the modified time of the file
time_t get_file_mtime( const std::string &file );

// Set the modified time of the file
bool set_file_mtime( const std::string &file, time_t mtime );

// Copy file from src to dst, preserving modified time
bool copy_file( const std::string &src, const std::string &dst );

// return true if path exists (file or directory)
bool path_exists( const std::string &file );

// return true if file exists
bool file_exists( const std::string &file );

// return true if specified path is an existing directory
bool directory_exists( const std::string &file );

// read the contents of the given file, return true on success
bool read_file_content( const std::string &file, std::string &content );

// write the contents to the given file, return true on success
bool write_file_content( const std::string &file, const std::string content );

// Check if provided path exists and whether it's a file or a directory
// Returns one of the following: IsFile, IsDirectory, IsNotFound
int check_path( const std::string &file );

// return true if the specified path is a relative path
bool is_relative_path( const std::string &file );

// clean the path string for usage.  Performs substitution of $HOME etc...
// if add_trailing_slash is true, this function adds a trailing '/' if
// path is to a directory
std::string clean_path( const std::string &path,
	bool add_trailing_slash = false );

// get program path (NOT the working directory)
std::string get_program_path();

// return path as an absolute path
std::string absolute_path( const std::string &path );

// return the value after the last dir separator
std::string path_filename( const std::string &path );

//
// Search "base_path"'s dir structure for a file with the given "base_name".
// Valid extensions for the "result" file are in the NULL terminated list
// "valid_extensions".
//
// put the full path and name in "result" and return true if found
//
bool search_for_file( const std::string &base_path,
	const std::string &file,
	const char **valid_extensions,
	std::string &result );

//
// Return list of subdirectories in path
//
bool get_subdirectories(
	std::vector<std::string> &list,
	const std::string &path );
//
// Return "list" of the base filenames in "path" where the file extension
// is in the vector "extensions"
//
// "list" is sorted alphabetically
//
bool get_basename_from_extension(
	std::vector<std::string> &list,
	const std::string &path,
	const std::string &extension,
	bool strip_extension = true );

//
// Return "in_list" of filenames in "path" where the base filename is "base_name"
//
// if filter is non-NULL: Only include files with an extension in the "ext_filter"
// NULL terminated list in "in_list".  All others go in "out_list".
//
bool get_filename_from_base( std::vector<std::string> &in_list,
	std::vector<std::string> &out_list,
	const std::string &path,
	const std::string &base_name,
	const char **ext_filter=NULL );

//
// Get a filename that does not currently exist.
// The resulting filename must start with "base" and end with "extension"
//
// Results:
// "result" is the resulting filename with the full path and extension.
// the returned string is the resulting basename only (no path or extension)
//
std::string get_available_filename(
	const std::string &path,
	const std::string &base,
	const std::string &extension,
	std::string &result );

//
// Create directory
//
bool make_dir(
    const std::string &dir
);

//
// Recursively delete directory and all its contents
//
void delete_dir( const std::string &path );

//
// Create "base" directory if it doesn't exist
// Create "base" and "sub" directory if they dont exist
// Returns true if either directory had to be created
//
bool confirm_directory(
    const std::string &base,
    const std::string &sub // Optional
);

//
// Create file if it doesn't exist
// Returns true if file had to be created
//
bool confirm_file(
    const std::string &path
);

//
// Delete named file
//
void delete_file( const std::string &file );

//
// Helpers for writing config files
//
std::string quote_config( std::string value );
void write_indent( nowide::ofstream &f, int indent );
void write_header( nowide::ofstream &f );
void write_section( nowide::ofstream &f, std::string name, std::string value );
void write_pair( nowide::ofstream &f, std::string name, std::string value, int indent=0 );
void write_param( nowide::ofstream &f, std::string param, std::string name, std::string value, int indent=0 );

//
// Return argument as a string
//
std::string as_str( const int i );
std::string as_str( const size_t t );
std::string as_str( const time_t t );
std::string as_str( const char *t );
std::string as_str( const float f, const int decimals=3 );

//
// Return string as integer
//
int as_int( const std::string &s );

//
// Return string as float
//
float as_float( const std::string &s );

//
// Return year string as int
// - 19 = 0
// - 19?? = 1900
// - 198? = 1980
// - 1980 = 1980
// - 1980? = 1980
//
int year_as_int( const std::string &s );

//
// Convert utf8 to utf32
//
std::basic_string<std::uint32_t> utf8_to_utf32( const std::string & );
std::string utf32_to_utf8( const std::basic_string<std::uint32_t> & );

//
// Strip control characters such as line feeds, etc
//
std::string clean_str( const std::string & );

//
// Return bool representing given string
// - "yes" or "true" = true, anything else = false
// - If permissive "no" or "false" = false, anything else = true
//
bool config_str_to_bool( const std::string &s, bool permissive = false );

//
// Return the name of the operating system.
//
const char *get_OS_string();

//
// return the contents of the clipboard (if implemented for OS)
//
std::string clipboard_get_content();

//
// set the contents of the clipboard
//
void clipboard_set_content( const std::string & );

//
// We use this in fe_window but implement it here because the XWindows
// namespace (Window, etc) clashes with the SFML namespace used in fe_window
// (sf::Window)
//
#if defined(USE_XLIB)
void get_x11_multimon_geometry( int &x, int &y, unsigned int &width, unsigned int &height );
void get_x11_primary_screen_size( unsigned int &width, unsigned int &height );

// set foreground window to the specified window.
void set_x11_foreground_window( unsigned long w );

// set window with fullscreen wm hint.
void set_x11_fullscreen_state( unsigned long w );
#endif

#ifndef NO_MOVIE
//
// Print FFmpeg version information to stdout.
//
void print_ffmpeg_version_info();
#endif

std::string url_escape( const std::string &raw );
std::string newline_escape( const std::string &raw );

void remove_trailing_spaces( std::string &str );

void get_url_components( const std::string &url,
	std::string &host,
	std::string &req );

std::string get_crc32( char *buff, int size );

void string_to_vector( const std::string &input,
	std::vector< std::string > &vec, bool allow_empty=false );

//
// Take config file line and populate setting and value pairs
// - Return true if setting found (value may be empty)
// - Return false otherwise (ie: comment line) and populate with empty strings
//
bool line_to_setting_and_value( const std::string &line,
	std::string &setting,
	std::string &value,
	const char *sep=FE_WHITESPACE );

// return the name of the process that currently has window focus
std::string get_focus_process();

//
// Non-blocking check for input on stdin
// return true if input found, false otherwise
//
bool get_console_stdin( std::string &str );

//
// Windows systems: Hide the console window if not launched from the command line
//
#ifdef SFML_SYSTEM_WINDOWS
void hide_console();
#endif

#ifdef FE_DEBUG
#define ASSERT(a) assert(a)
#else
#define ASSERT(a)
#endif

#endif

// Create a vector container #size values from..to
std::vector<std::string> create_range( int from, int to, int size );

// Create a vector container #size values from..to, clamped at min/max
std::vector<std::string> create_range( int from, int to, int size, int min, int max );

// Auto string to sf::Color
bool str_to_color( const std::string &str, sf::Color &col );

// rgb string to sf::Color
bool rgb_to_color( const std::string &str, sf::Color &col );

// hex string to sf::Color
bool hex_to_color( const std::string &str, sf::Color &col );

// sf::Color to rgb string
void color_to_rgb( const sf::Color &col, std::string &str );

// sf::Color to hex string
void color_to_hex( const sf::Color &col, std::string &str );

//
// Return index of token in tokens, or -1 if not found
//
int get_token_index( const char *tokens[], const std::string &token );

// Return true if capslock enabled
bool get_capslock_state();
