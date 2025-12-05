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

#ifndef FE_INFO_HPP
#define FE_INFO_HPP

#include "fe_base.hpp"
#include "fe_util.hpp"
#include <map>
#include <set>
#include <vector>
#include "nowide/fstream.hpp"
#include "cereal/cereal.hpp"
#include <regex>

extern const char *FE_STAT_FILE_EXTENSION;
extern const char FE_TAGS_SEP;

class FeRomTitleFormatter {

public:
	static bool display_format;
	static bool sort_format;

	// Returns a new string with prefixes such as "The" and "Vs." moved to the end
	static std::string get_formatted_title( const std::string &title )
	{
		if ( title.empty() )
			return title;

		std::string format_title = title;
		std::string suffix = "";
		std::string brackets = "";

		if ( lowercase( format_title.substr( 0, 4 ) ) == "vs. " )
		{
			suffix += format_title.substr( 0, 4 );
			format_title = format_title.substr( 4 );
		}

		if ( lowercase( format_title.substr( 0, 4 ) ) == "the " )
		{
			suffix += format_title.substr( 0, 4 );
			format_title = format_title.substr( 4 );
		}

		size_t brackets_pos = format_title.find_first_of( "(/[" );
		if ( brackets_pos != std::string::npos )
		{
			brackets = format_title.substr( brackets_pos );
			format_title = format_title.substr( 0, brackets_pos );
		}

		if ( !suffix.empty() )
			format_title = rtrim( format_title ) + ", " + rtrim( suffix );

		if ( !brackets.empty() )
			format_title = rtrim( format_title ) + " " + brackets;

		return format_title;
	}

	static std::string get_display_title( const std::string &title )
	{
		return display_format ? get_formatted_title( title ) : title;
	}

	static std::string get_sort_title( const std::string &title )
	{
		return sort_format ? get_formatted_title( title ) : title;
	}
};

//
// Class for storing information regarding a specific rom
//
class FeRomInfo : public FeBaseConfigurable
{
public:
	enum Index
	{
		Romname=0,
		Title,
		Emulator,
		Cloneof,
		Year,
		Manufacturer,
		Category,
		Players,
		Rotation,
		Control,
		Status,
		DisplayCount,
		DisplayType,
		AltRomname,
		AltTitle,
		Extra,
		Buttons,
		Series,
		Language,
		Region,
		Rating,

		Favourite,		// everything from Favourite on is NOT loaded from romlist
		Tags,
		PlayedCount,
		PlayedTime,
		PlayedLast,
		Score,
		Votes,

		FileIsAvailable, // this point and beyond is dynamically added
		Shuffle,
		LAST_INDEX
	};

	// Matches fe_info.hpp "FeRomInfo::specialStrings"
	enum Special
	{
		DisplayName = 0,
		ListTitle, // Deprecated as of 1.5
		FilterName,
		ListFilterName, // Deprecated as of 1.5
		ListSize,
		ListEntry,
		Search,
		TitleLetter,
		TitleFull,
		SortName,
		SortValue,
		System,
		SystemN,
		Overview,
		TagList,
		FavouriteStar,
		FavouriteStarAlt,
		FavouriteHeart,
		FavouriteHeartAlt,
		PlayedAgo,
		ScoreStar,
		ScoreStarAlt,
		LAST_SPECIAL
	};

	// Certain indices gets repurposed during -listsoftware
	// romlist building/importing...
	static const Index BuildFullPath;
	static const Index BuildScore;
	static const Index LAST_INFO = Favourite; // Everything prior to Favourite is loaded from romlist

	static const char *indexStrings[];
	static const char *specialStrings[];

	static const std::vector<FeRomInfo::Index> Stats; // List of indexes used for Stats
	static const bool isNumeric( Index index ); // Returns true if FeRomInfo Index value should be considered numeric for sorting
	static const bool isStat( Index index ); // Returns true if FeRomInfo Index is a Stat

	FeRomInfo();
	FeRomInfo( const std::string &romname );

	const std::string &get_info( int ) const;
	void set_info( enum Index, const std::string & );

	const std::string get_id() const;
	const std::string get_clone_parent() const;
	void append_tag( const std::string &tag );
	void remove_tag( const std::string &tag );
	bool get_tags( std::set<std::string> &tags );
	bool has_tag( const std::string &tag );

	int process_setting( const std::string &setting,
		const std::string &value,
		const std::string &fn );
	std::string as_output( void ) const;

	void clear_stats();
	bool load_stats( const std::string &path );
	bool save_stats( const std::string &path );
	bool update_stats( const std::string &path, int count_incr, int played_incr );

	void clear(); // Clear all m_info data

	// convenience method to copy info attribute at idx from src
	void copy_info( const FeRomInfo &src, Index idx );

	bool operator==( const FeRomInfo & ) const;      // compares romname and emulator only
	bool operator!=( const FeRomInfo & ) const;      // compares romname and emulator only
	bool full_comparison( const FeRomInfo & ) const; // compares all fields that get loaded from the romlist file
	int index; // Stores the m_list index, after global_filter applied

	// Save only the romlist info values (not the late-loaded stats data)
	template<class Archive>
	void save(Archive &archive, std::uint32_t const version) const
	{
		if ( version != FE_CACHE_VERSION ) throw "Invalid FeRomInfo cache";
		archive( std::vector<std::string>( m_info.begin(), m_info.begin() + LAST_INFO ), index );
	}

	template<class Archive>
	void load(Archive &archive, std::uint32_t const version)
	{
		if ( version != FE_CACHE_VERSION ) throw "Invalid FeRomInfo cache";
		archive( m_info, index );
		m_display_title = FeRomTitleFormatter::get_display_title( m_info[ Title ] );
		m_sort_title = FeRomTitleFormatter::get_sort_title( m_info[ Title ] );
	}

	const std::string &get_display_title() const { return m_display_title; }
	const std::string &get_sort_title() const { return m_sort_title; }

private:
	std::string get_info_escaped( int ) const;
	size_t get_tag_pos( const std::string &tag );

	std::vector<std::string> m_info;
	std::string m_display_title;
	std::string m_sort_title;
};

CEREAL_CLASS_VERSION( FeRomInfo, FE_CACHE_VERSION );

//
// Class for a single rule in a list filter
//
class FeRule : public FeBaseConfigurable
{
public:
	enum FilterComp {
		FilterEquals=0,
		FilterNotEquals,
		FilterContains,
		FilterNotContains,
		FilterGreaterThan,
		FilterLessThan,
		FilterGreaterThanOrEqual,
		FilterLessThanOrEqual,
		LAST_COMPARISON
	};

	static const char *filterCompStrings[];
	static const char *filterCompDisplayStrings[];

	FeRule( FeRomInfo::Index target=FeRomInfo::LAST_INDEX,
		FilterComp comp=LAST_COMPARISON,
		const std::string &what="" );
	FeRule( const FeRule & );

	~FeRule();

	FeRule &operator=( const FeRule & );

	bool init();
	bool apply_rule( const FeRomInfo &rom ) const;
	void save( nowide::ofstream &, const int indent = 0 ) const;

	FeRomInfo::Index get_target() const { return m_filter_target; };
	FilterComp get_comp() const { return m_filter_comp; };
	const std::string &get_what() const { return m_filter_what; };

	bool is_exception() const { return m_is_exception; };
	void set_is_exception( bool f ) { m_is_exception=f; };

	void set_values( FeRomInfo::Index i, FilterComp c, const std::string &w );

	int process_setting( const std::string &,
         const std::string &value, const std::string &fn );

private:
	FeRomInfo::Index m_filter_target;
	FilterComp m_filter_comp;
	std::string m_filter_what;
	std::wregex m_rex;
	bool m_regex_compiled;
	bool m_is_exception;
	bool m_use_rex;
	bool m_use_year;
	float m_filter_float;
};

//
// Class for a list filter
//
class FeFilter : public FeBaseConfigurable
{
public:
	enum Index { Rule=0, Exception, SortBy, ReverseOrder, ListLimit };
	static const char *indexStrings[];

	FeFilter( const std::string &name );
	void init();
	bool apply_filter( const FeRomInfo &rom ) const;

	int process_setting( const std::string &setting,
		const std::string &value,
		const std::string &fn );

	void save( nowide::ofstream &, const char *filter_tag, const int indent = 0 ) const;
	const std::string &get_name() const { return m_name; };
	void set_name( const std::string &n ) { m_name = n; };

	int get_rom_index() const { return m_rom_index; };
	void set_rom_index( int i ) { m_rom_index=i; };

	// Get the total number of roms (prior to limiting)
	int get_size() const { return m_size; };
	void set_size( int s ) { m_size=s; };

	std::vector<FeRule> &get_rules() { return m_rules; };
	int get_rule_count() const { return m_rules.size(); };

	FeRomInfo::Index get_sort_by() const { return m_sort_by; }
	bool get_reverse_order() const { return m_reverse_order; }
	int get_list_limit() const { return m_list_limit; }

	void set_sort_by( FeRomInfo::Index i ) { m_sort_by=i; }
	void set_reverse_order( bool r ) { m_reverse_order=r; }
	void set_list_limit( int p ) { m_list_limit=p; }

	// Returns true if any of the targets are used by sort or filter rules
	bool test_for_targets( std::set<FeRomInfo::Index> targets ) const;

	void clear();

private:
	std::string m_name;
	std::vector<FeRule> m_rules;
	int m_rom_index;
	int m_list_limit; // limit the number of list entries if non-zero.
		// Positive value limits to the first
		// x values, Negative value limits to the last abs(x) values.
	int m_size;
	FeRomInfo::Index m_sort_by;
	bool m_reverse_order;
};

class FeScriptConfigurable : public FeBaseConfigurable
{
public:
	bool get_param( const std::string &label, std::string &v ) const;
	void set_param( const std::string &label, const std::string &v );
	void get_param_labels( std::vector<std::string> &labels ) const;
	void clear_params() { m_params.clear(); };

	int process_setting( const std::string &setting,
		const std::string &value,
		const std::string &filename );

	void save( nowide::ofstream &, const int indent = 0 ) const;

	void merge_params( const FeScriptConfigurable &o );

protected:
	static const char *indexString;
	std::map<std::string,std::string> m_params;
};

//
// Class for storing information regarding a specific display
//
class FeDisplayInfo : public FeBaseConfigurable
{
public:

	enum Index {
		Name=0,
		Layout,
		Romlist,
		InCycle,
		InMenu,
		LAST_INDEX
	};
	static const char *indexStrings[];
	static const char *otherStrings[];

	FeDisplayInfo( const std::string &name );

	const std::string &get_info( int ) const;
	void set_info( int setting, const std::string &value );

	int process_setting( const std::string &setting,
		const std::string &value,
		const std::string &fn );

	int process_state( const std::string &state_string, bool new_format = true );
	std::string state_as_output() const;

	void set_current_filter_index( int i ) { m_filter_index=i; };
	int get_current_filter_index() const { return m_filter_index; };
	int get_filter_count() const { return m_filters.size(); };
	FeFilter *get_filter( int ); // use get_filter( -1 ) to get global filter
	void append_filter( const FeFilter &f );
	void delete_filter( int i );
	void get_filters_list( std::vector<std::string> &l ) const;

	FeFilter *get_global_filter() { return &m_global_filter; };

	std::string get_current_layout_file() const;
	void set_current_layout_file( const std::string & );

	int get_rom_index( int filter_index ) const;
	void set_rom_index( int filter_index, int rom_index );

	void save( nowide::ofstream &, const int indent = 0 ) const;

	const std::string &get_name() const { return m_info[Name]; };
	const std::string &get_layout() const { return m_info[Layout]; };
	const std::string &get_romlist_name() const { return m_info[Romlist]; };

	bool show_in_cycle() const;
	bool show_in_menu() const;
	bool test_for_targets( std::set<FeRomInfo::Index> targets );

	FeScriptConfigurable &get_layout_per_display_params() { return m_layout_per_display_params; };

private:
	std::string m_info[LAST_INDEX];
	std::string m_current_layout_file;
	int m_rom_index; // only used if there are no filters on this display
	int m_filter_index;
	FeFilter *m_current_config_filter;
	FeScriptConfigurable m_layout_per_display_params; // used to store "per display" layout parameters

	std::vector< FeFilter > m_filters;
	FeFilter m_global_filter;
};

//
// Defines used to disable features that are unsupported on certain systems
//
#if defined(USE_DRM) || defined(SFML_SYSTEM_MACOS)
 #define NO_PAUSE_HOTKEY
#endif

#if defined(USE_DRM)
 #define NO_NBM_WAIT
#endif

//
// Class for storing information regarding a specific emulator
//
class FeEmulatorInfo : public FeBaseConfigurable
{
public:

	enum Index
	{
		Name=0,
		Executable,
		Command,
		Working_dir,
		Rom_path,
		Rom_extension,
		System,
		Info_source,
		Import_extras,
		NBM_wait,	// non-blocking mode wait time (in seconds)
		Exit_hotkey,
		Pause_hotkey,
		LAST_INDEX
	};

	enum InfoSource
	{
		None=0,
		Listxml,	// "mame"
		Listsoftware,	// "mess"
		Steam,
		Thegamesdb,
		Scummvm,
		Listsoftware_tgdb,	// "mess" + thegamesdb.net
		LAST_INFOSOURCE
	};

	static const char *indexStrings[];
	static const char *indexDispStrings[];
	static const char *infoSourceStrings[];

	FeEmulatorInfo();
	FeEmulatorInfo( const std::string &name );

	const std::string get_info( int ) const;
	void set_info( enum Index, const std::string & );

	InfoSource get_info_source() const { return m_info_source; };

	// get artwork path.  Multiple paths are semicolon separated
	bool get_artwork( const std::string &label, std::string &paths ) const;

	// get artwork paths in the provided vector
	bool get_artwork( const std::string &label, std::vector< std::string > &paths ) const;

	// add artwork paths to the specified artwork
	void add_artwork( const std::string &label, const std::string &paths );

	// get a list of all artworks labels with the related paths in a string
	void get_artwork_list( std::vector<std::pair<std::string,std::string> > & ) const;

	void delete_artwork( const std::string & );

	const std::vector<std::string> &get_paths() const;
	const std::vector<std::string> &get_extensions() const;
	const std::vector<std::string> &get_systems() const;
	const std::vector<std::string> &get_import_extras() const;

	int process_setting( const std::string &setting,
		const std::string &value,
		const std::string &filename );

	void save( const std::string &filename ) const;

	// search paths and exts to gather all base rom names (i.e. "galaga")
	void gather_rom_names( std::vector<std::string> &name_list ) const;

	// same as above but pair base and full paths
	// (i.e. "galaga", "c:\beer\galaga.zip")
	void gather_rom_names( std::vector<std::string> &name_list,
		std::vector<std::string> &full_path_list ) const;

	// clean_path() and use Working_dir setting if relative in_path
	std::string clean_path_with_wd( const std::string &in_path, bool add_trailing_slash=false ) const;

	bool is_mame() const;
	bool is_mess() const;

private:
	std::string vector_to_string( const std::vector< std::string > &vec ) const;
	std::string m_name;
	std::string m_executable;
	std::string m_command;
	std::string m_workdir;
	std::string m_exit_hotkey;
	std::string m_pause_hotkey;

	std::vector<std::string> m_paths;
	std::vector<std::string> m_extensions;
	std::vector<std::string> m_systems;
	std::vector<std::string> m_import_extras;

	InfoSource m_info_source;
	int m_nbm_wait;

	//
	// Considered using a std::multimap here but C++98 doesn't guarantee the
	// relative order of equivalent values, so we do a map of vectors so that
	// we can maintain the precedence ordering of our artwork paths...
	//
	std::map<std::string, std::vector<std::string> > m_artwork;
};

//
// Class for storing plugin configuration
//
class FePlugInfo : public FeScriptConfigurable
{
public:
	FePlugInfo( const std::string &name );
	const std::string &get_name() const { return m_name; };

	bool get_enabled() const { return m_enabled; };
	void set_enabled( bool e ) { m_enabled=e; };

	int process_setting( const std::string &setting,
		const std::string &value,
		const std::string &filename );

	void save( nowide::ofstream & ) const;

private:
	static const char *indexStrings[];

	std::string m_name;
	bool m_enabled;
};

//
// Class for storing layout/screensaver configuration
//
class FeLayoutInfo : public FeScriptConfigurable
{
public:
	static const char *indexStrings[];
	enum Type { ScreenSaver=0, Layout, Intro, Menu };

	FeLayoutInfo( Type t );
	FeLayoutInfo( const std::string &name ); // type=Layout

	const std::string &get_name() const { return m_name; };

	void save( nowide::ofstream & ) const;

	bool operator!=( const FeLayoutInfo & );

private:
	std::string m_name;
	Type m_type;
};

//
// Class to contain the interface language strings
//
class FeTranslationMap : public FeBaseConfigurable
{
public:
	FeTranslationMap();

	void clear();

	int process_setting(
		const std::string &setting,
		const std::string &value,
		const std::string &filename
	);

	// Return token translation string
	static const std::string get_translation(
		const std::string &token,
		const std::vector<std::string> &rep = {}
	);

private:
	FeTranslationMap( const FeTranslationMap & );
	FeTranslationMap &operator=( const FeTranslationMap & );

	// Holds the src/translation pairs
	// - Note that an instance is require to setup this map
	static std::map<std::string, std::string> m_map;
};

//
// Global translation function, accepts optional replacement list
//
const std::string _( const std::string &token, const std::vector<std::string> &rep = {} );
const std::string _( const char* token, const std::vector<std::string> &rep = {} );
const std::vector<std::string> _( const std::vector<std::string> &tokens, const std::vector<std::string> &rep = {} );
const std::vector<std::string> _( const char* tokens[], const std::vector<std::string> &rep = {} );

#endif
