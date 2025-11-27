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

#include "fe_info.hpp"
#include "fe_util.hpp"
#include "fe_util_sq.hpp"
#include "fe_cache.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <ctime>

#include <iomanip>

const char *FE_STAT_FILE_EXTENSION = ".stat";
const char FE_TAGS_SEP = ';';
const char TAGS_SEP_ARG[] = { FE_TAGS_SEP, 0 };

const FeRomInfo::Index FeRomInfo::BuildFullPath = FeRomInfo::Tags;
const FeRomInfo::Index FeRomInfo::BuildScore = FeRomInfo::PlayedCount;

bool FeRomTitleFormatter::display_format = false;
bool FeRomTitleFormatter::sort_format = false;

const char *FeRomInfo::indexStrings[] =
{
	"Name",
	"Title",
	"Emulator",
	"CloneOf",
	"Year",
	"Manufacturer",
	"Category",
	"Players",
	"Rotation",
	"Control",
	"Status",
	"DisplayCount",
	"DisplayType",
	"AltRomname",
	"AltTitle",
	"Extra",
	"Buttons",
	"Series",
	"Language",
	"Region",
	"Rating",

	"Favourite",
	"Tags",
	"PlayedCount",
	"PlayedTime",
	"PlayedLast",
	"Score",
	"Votes",
	"FileIsAvailable",
	"Shuffle",
	NULL
};

// Matches fe_info.cpp "enum Special"
const char *FeRomInfo::specialStrings[] =
{
	"DisplayName",
	"ListTitle",
	"FilterName",
	"ListFilterName",
	"ListSize",
	"ListEntry",
	"Search",
	"TitleLetter",
	"TitleFull",
	"SortName",
	"SortValue",
	"System",
	"SystemN",
	"Overview",
	"TagList",
	"FavouriteStar",
	"FavouriteStarAlt",
	"FavouriteHeart",
	"FavouriteHeartAlt",
	"PlayedAgo",
	"ScoreStar",
	"ScoreStarAlt",
	NULL
};

// Stats are list in the order they appear in the .stat file
const std::vector<FeRomInfo::Index> FeRomInfo::Stats = {
	FeRomInfo::PlayedCount,
	FeRomInfo::PlayedTime,
	FeRomInfo::PlayedLast,
	FeRomInfo::Score,
	FeRomInfo::Votes
};

FeRomInfo::FeRomInfo():
	m_display_title(""),
	m_sort_title("")
{
	m_info = std::vector<std::string>(LAST_INDEX);
}

FeRomInfo::FeRomInfo( const std::string &rn ):
	m_display_title(""),
	m_sort_title("")
{
	m_info = std::vector<std::string>(LAST_INDEX);
	m_info[Romname] = rn;
}

// Returns true if FeRomInfo Index is supposed to be numeric (for sorting)
// - Stats are guaranteed numeric since they're handled externally
// - `Players` is included since it was sorted numerically previously
// - Any romlist field may be overwritten by custom user data
const bool FeRomInfo::isNumeric( Index index )
{
	return ( index == FeRomInfo::Players )
		|| ( index == FeRomInfo::PlayedCount )
		|| ( index == FeRomInfo::PlayedTime )
		|| ( index == FeRomInfo::PlayedLast )
		|| ( index == FeRomInfo::Score )
		|| ( index == FeRomInfo::Votes )
		|| ( index == FeRomInfo::Shuffle );
}

// Returns true if FeRomInfo Index is a Stat
const bool FeRomInfo::isStat( Index index )
{
	return std::find( Stats.begin(), Stats.end(), index ) != Stats.end();
}

const std::string &FeRomInfo::get_info( int i ) const
{
	return m_info[i];
}

std::string FeRomInfo::get_info_escaped( int i ) const
{
	if ( m_info[i].find_first_of( ';' ) != std::string::npos )
	{
		std::string temp = m_info[i];
		perform_substitution( temp, "\"", "\\\"" );
		return ( "\"" + temp + "\"" );
	}
	else
		return m_info[i];
}

void FeRomInfo::set_info( Index i, const std::string &v )
{
	m_info[i] = v;

	if ( i == Title ) {
		m_display_title = FeRomTitleFormatter::get_display_title( v );
		m_sort_title = FeRomTitleFormatter::get_sort_title( v );
	}
}

//
// Return a unique identifier for this rom, used for tags and fav files
// - romname + FE_TAGS_SEP + emulator
//
const std::string FeRomInfo::get_id() const
{
	return m_info[Romname] + FE_TAGS_SEP + m_info[Emulator];
}

//
// Return cloneof name, or romname if none
//
const std::string FeRomInfo::get_clone_parent() const
{
	return m_info[Cloneof].empty()
		? m_info[Romname]
		: m_info[Cloneof];
}

//
// Add tag to the info string
// - Does NOT check if already has tag - check omitted for performance - caller should check as required
// - The tags logic requires a FE_TAGS_SEP character on each side of a tag.
//
void FeRomInfo::append_tag( const std::string &tag )
{
	if ( m_info[Tags].empty() ) m_info[Tags] = FE_TAGS_SEP;
	m_info[Tags] += tag + FE_TAGS_SEP;
}

//
// Remove tag from the info string
//
void FeRomInfo::remove_tag( const std::string &tag )
{
	// Tag must match with FE_TAGS_SEP on each side
	size_t pos = get_tag_pos( tag );
	if ( pos == std::string::npos ) return;

	// remove tag plus preceeding FE_TAGS_SEP
	int len = tag.size();
	m_info[Tags].erase( pos, len + 1 );

	// cleanup if no tags remaining
	if (( m_info[Tags].size() == 1 ) && ( m_info[Tags][0] == FE_TAGS_SEP ))
		m_info[Tags].clear();
}

//
// Populate given set with individual info tag names
// - Returns true if the set contains tags
//
bool FeRomInfo::get_tags( std::set<std::string> &tags )
{
	size_t pos = 0;
	std::string tag;
	while ( token_helper( m_info[Tags], pos, tag, TAGS_SEP_ARG ) )
		if ( !tag.empty() ) tags.insert( tag );
	return tags.size() > 0;
}

//
// Returns true if the given tags exists
//
bool FeRomInfo::has_tag( const std::string &tag )
{
	return get_tag_pos( tag ) != std::string::npos;
}

//
// Returns position of tags within info, or npos
//
size_t FeRomInfo::get_tag_pos( const std::string &tag )
{
	return m_info[Tags].find( FE_TAGS_SEP + tag + FE_TAGS_SEP );
}

void FeRomInfo::clear_stats()
{
	int size = (int)FeRomInfo::Stats.size();
	for ( int i=0; i<size; i++ )
		m_info[FeRomInfo::Stats[i]] = "0";
}

//
// Ensure stats have been loaded into this rominfo, resets them to zero if none exist
// - If force_update is true the stats are re-loaded even if they've already been set
//
bool FeRomInfo::load_stats(
	const std::string &path
)
{
	// Exit early if stats already loaded
	if ( !m_info[PlayedCount].empty() )
		return true;

	// Attempt to load stats from cache, and exit if successful
	if ( FeCache::get_stats_info( path, m_info ) )
		return true;

	std::string played_count;
	std::string played_time;
	std::string played_last;
	std::string score;
	std::string votes;

	if ( !path.empty() )
	{
		std::string filename = path + m_info[Emulator] + "/" + m_info[Romname] + FE_STAT_FILE_EXTENSION;
		nowide::ifstream myfile( filename );
		if ( myfile.is_open() )
		{
			if ( myfile.good() ) getline( myfile, played_count );
			if ( myfile.good() ) getline( myfile, played_time );
			if ( myfile.good() ) getline( myfile, played_last );
			if ( myfile.good() ) getline( myfile, score );
			if ( myfile.good() ) getline( myfile, votes );
			myfile.close();
		}
	}

	m_info[PlayedCount] = played_count.empty() ? "0" : played_count;
	m_info[PlayedTime] = played_time.empty() ? "0" : played_time;
	m_info[PlayedLast] = played_last.empty() ? "0" : played_last;
	m_info[Score] = score.empty() ? "0" : score;
	m_info[Votes] = votes.empty() ? "0" : votes;
	return true;
}

bool FeRomInfo::update_stats( const std::string &path, int count_incr, int played_incr )
{
	if ( !load_stats( path ) )
		return false;

	m_info[PlayedCount] = as_str( as_int( m_info[PlayedCount] ) + count_incr );
	m_info[PlayedTime] = as_str( as_int( m_info[PlayedTime] ) + played_incr );
	m_info[PlayedLast] = as_str( std::time(0) );

	return save_stats( path );
}

bool FeRomInfo::save_stats( const std::string &path )
{
	// Save stats to cache, and continue to save original stat file as well
	FeCache::set_stats_info( path, m_info );

	confirm_directory( path, m_info[Emulator] );
	std::string filename = path + m_info[Emulator] + "/" + m_info[Romname] + FE_STAT_FILE_EXTENSION;
	nowide::ofstream myfile( filename.c_str() );

	if ( !myfile.is_open() )
	{
		FeLog() << "Error writing stat file: " << filename << std::endl;
		return false;
	}

	myfile << m_info[PlayedCount] << std::endl
		<< m_info[PlayedTime] << std::endl
		<< m_info[PlayedLast] << std::endl
		<< m_info[Score] << std::endl
		<< m_info[Votes] << std::endl;
	myfile.close();
	return true;
}

int FeRomInfo::process_setting( const std::string &,
         const std::string &value, const std::string &fn )
{
	size_t pos=0;
	std::string token;

	for ( int i=1; i < LAST_INFO; i++ )
	{
		token_helper( value, pos, token );
		set_info( (Index)i, token );
	}

	return 0;
}

std::string FeRomInfo::as_output( void ) const
{
	std::string s = get_info_escaped( (Index)0 );
	for ( int i=1; i < LAST_INFO; i++ )
	{
		s += ';';
		s += get_info_escaped( (Index)i );
	}

	return s;
}

void FeRomInfo::clear()
{
	m_display_title.clear();
	m_sort_title.clear();
	for ( int i=0; i < LAST_INDEX; i++ )
		m_info[i].clear();
}

void FeRomInfo::copy_info( const FeRomInfo &src, Index idx )
{
	set_info( idx, src.m_info[idx] );
}

bool FeRomInfo::operator==( const FeRomInfo &o ) const
{
	return ( m_info[Romname].compare( o.m_info[Romname] ) == 0 )
		&& ( m_info[Emulator].compare( o.m_info[Emulator] ) == 0 );
}

bool FeRomInfo::operator!=( const FeRomInfo &o ) const
{
	return !( *this == o );
}

bool FeRomInfo::full_comparison( const FeRomInfo &o ) const
{
	for ( int i=0; i<LAST_INFO; i++ )
	{
		if ( m_info[i].compare( o.m_info[i] ) != 0 )
			return false;
	}

	return true;
}

const char *FeRule::filterCompStrings[] =
{
	"equals",
	"not_equals",
	"contains",
	"not_contains",
	"greater_than",
	"less_than",
	"greater_than_or_equal",
	"less_than_or_equal",
	NULL
};

const char *FeRule::filterCompDisplayStrings[] =
{
	"equals",
	"does not equal",
	"contains",
	"does not contain",
	"greater than",
	"less than",
	"greater than or equal",
	"less than or equal",
	NULL
};

FeRule::FeRule( FeRomInfo::Index t, FilterComp c, const std::string &w )
	: m_filter_target( t ),
	m_filter_comp( c ),
	m_filter_what( w ),
	m_regex_compiled( false ),
	m_is_exception( false ),
	m_use_rex( false ),
	m_use_year( false ),
	m_filter_float( 0.0 )
{
}

FeRule::FeRule( const FeRule &r )
	: m_filter_target( r.m_filter_target ),
	m_filter_comp( r.m_filter_comp ),
	m_filter_what( r.m_filter_what ),
	m_regex_compiled( r.m_regex_compiled ),
	m_is_exception( r.m_is_exception ),
	m_use_rex( r.m_use_rex ),
	m_use_year( r.m_use_year ),
	m_filter_float( r.m_filter_float )
{
	if ( r.m_regex_compiled )
		m_rex = r.m_rex;
}

FeRule::~FeRule() {}

FeRule &FeRule::operator=( const FeRule &r )
{
	m_filter_target = r.m_filter_target;
	m_filter_comp = r.m_filter_comp;
	m_filter_what = r.m_filter_what;
	m_is_exception = r.m_is_exception;

	if ( r.m_regex_compiled )
		m_rex = r.m_rex;

	return *this;
}

bool FeRule::init()
{
	// Check if comparing year, since the values might need some "massaging"
	m_use_year = m_filter_target == FeRomInfo::Year;

	// Greater/less comparisons will need the "what" as a float
	if ( m_filter_comp == FilterGreaterThan
		|| m_filter_comp == FilterLessThan
		|| m_filter_comp == FilterGreaterThanOrEqual
		|| m_filter_comp == FilterLessThanOrEqual )
	{
		m_use_rex = false;
		m_filter_float = as_float( m_filter_what );
		return true;
	}

	// Check for traces of regular expressions, otherwise faster comparisons with be used
	m_use_rex = m_filter_what.find_first_of( ".+*?^$()[]{}|\\" ) != std::string::npos;
	if ( !m_use_rex || m_regex_compiled || m_filter_what.empty() )
		return true;

	try
	{
		// Create wide case-insensitive regexp
		m_rex = std::wregex( FeUtil::widen( m_filter_what ), std::regex_constants::ECMAScript | std::regex_constants::icase );
		m_regex_compiled = true;
	}
	catch ( const std::regex_error& e )
	{
		FeLog() << "Error compiling regular expression \"" << m_filter_what << "\": " << e.what() << std::endl;
		m_regex_compiled = false;
		return false;
	}

	return true;
}

bool FeRule::apply_rule( const FeRomInfo &rom ) const
{
	if (( m_filter_target == FeRomInfo::LAST_INDEX )
		|| ( m_filter_comp == FeRule::LAST_COMPARISON )
		|| ( m_use_rex && !m_regex_compiled ))
		return true;

	const std::string &target = rom.get_info( m_filter_target );

	switch ( m_filter_comp )
	{
	case FilterEquals:
		return target.empty()
			? m_filter_what.empty()
			: m_use_rex
				? std::regex_match( FeUtil::widen( target ), m_rex )
				: ( icompare( target, m_filter_what ) == 0 );

	case FilterNotEquals:
		return target.empty()
			? !m_filter_what.empty()
			: m_use_rex
				? !std::regex_match( FeUtil::widen( target ), m_rex )
				: ( icompare( target, m_filter_what ) != 0 );

	case FilterContains:
		return target.empty()
			? false
			: m_use_rex
				? std::regex_search( FeUtil::widen( target ), m_rex )
				: ( lowercase( target ).find( lowercase( m_filter_what ) ) != std::string::npos );

	case FilterNotContains:
		return target.empty()
			? true
			: m_use_rex
				? !std::regex_search( FeUtil::widen( target ), m_rex )
				: ( lowercase( target ).find( lowercase( m_filter_what ) ) == std::string::npos );

	case FilterGreaterThan:
		return target.empty()
			? false
			: m_use_year
				? year_as_int( target ) > m_filter_float
				: as_float( target ) > m_filter_float;

	case FilterGreaterThanOrEqual:
		return target.empty()
			? false
			: m_use_year
				? year_as_int( target ) >= m_filter_float
				: as_float( target ) >= m_filter_float;

	case FilterLessThan:
		return target.empty()
			? true
			: m_use_year
				? year_as_int( target ) < m_filter_float
				: as_float( target ) < m_filter_float;

	case FilterLessThanOrEqual:
		return target.empty()
			? true
			: m_use_year
				? year_as_int( target ) <= m_filter_float
				: as_float( target ) <= m_filter_float;

	default:
		return true;
	}
}

void FeRule::save( nowide::ofstream &f, const int indent ) const
{
	if (( m_filter_target == FeRomInfo::LAST_INDEX ) || ( m_filter_comp == LAST_COMPARISON ))
		return;

	write_pair(
		f,
		FeFilter::indexStrings[ m_is_exception ? FeFilter::Exception : FeFilter::Rule ],
		as_str( FeRomInfo::indexStrings[ m_filter_target ] )
			+ " " + filterCompStrings[ m_filter_comp ]
			+ " " + m_filter_what,
		indent
	);
}

void FeRule::set_values(
		FeRomInfo::Index i,
		FilterComp c,
		const std::string &w )
{
	m_regex_compiled = false;
	m_filter_target = i;
	m_filter_comp = c;
	m_filter_what = w;
}

int FeRule::process_setting( const std::string &setting,
         const std::string &value, const std::string &fn )
{
	if ( setting.compare(
			FeFilter::indexStrings[ FeFilter::Exception ] ) == 0 )
		m_is_exception=true;

	std::string token;
	size_t pos=0;

	token_helper( value, pos, token, FE_WHITESPACE );

	int i=0;
	while( FeRomInfo::indexStrings[i] != NULL )
	{
		if ( token.compare( FeRomInfo::indexStrings[i] ) == 0 )
			break;
		i++;
	}

	if ( i >= FeRomInfo::LAST_INDEX )
	{
		invalid_setting( fn, "rule", token,
				FeRomInfo::indexStrings, NULL, "target" );
		return 1;
	}

	m_filter_target = (FeRomInfo::Index)i;
	token_helper( value, pos, token, FE_WHITESPACE );

	i=0;
	while( filterCompStrings[i] != NULL )
	{
		if ( token.compare( filterCompStrings[i] ) == 0 )
			break;
		i++;
	}

	if ( i >= LAST_COMPARISON )
	{
		invalid_setting( fn, "rule", token, filterCompStrings,
				NULL, "comparison" );
		return 1;
	}

	m_filter_comp = (FilterComp)i;

	// Remainder of the line is filter regular expression (which may contain
	// spaces...)
	//
	if ( pos < value.size() )
		m_filter_what = value.substr( pos );

	return 0;
}

const char *FeFilter::indexStrings[] =
{
	"rule",
	"exception",
	"sort_by",
	"reverse_order",
	"list_limit",
	NULL
};

FeFilter::FeFilter( const std::string &name )
	: m_name( name ),
	m_rom_index( 0 ),
	m_list_limit( 0 ),
	m_size( 0 ),
	m_sort_by( FeRomInfo::LAST_INDEX ),
	m_reverse_order( false )
{
}

void FeFilter::init()
{
	for ( std::vector<FeRule>::iterator itr=m_rules.begin();
			itr != m_rules.end(); ++itr )
		(*itr).init();
}

bool FeFilter::apply_filter( const FeRomInfo &rom ) const
{
	for ( std::vector<FeRule>::const_iterator itr=m_rules.begin();
		itr != m_rules.end(); ++itr )
	{
		if ( (*itr).apply_rule( rom ) == (*itr).is_exception() )
			return (*itr).is_exception();
	}

	return true;
}

int FeFilter::process_setting( const std::string &setting,
         const std::string &value, const std::string &fn )
{
	if (( setting.compare( indexStrings[Rule] ) == 0 ) // rule
		|| ( setting.compare( indexStrings[Exception] ) == 0 ))
	{
		FeRule new_rule;

		if ( !value.empty() )
			new_rule.process_setting( setting, value, fn );

		m_rules.push_back( new_rule );
	}
	else if ( setting.compare( indexStrings[SortBy] ) == 0 ) // sort_by
	{
		for ( int i=0; i < FeRomInfo::LAST_INDEX; i++ )
		{
			if ( value.compare( FeRomInfo::indexStrings[i] ) == 0 )
			{
				set_sort_by( (FeRomInfo::Index)i );
				break;
			}
		}
	}
	else if ( setting.compare( indexStrings[ReverseOrder] ) == 0 ) // reverse_order
	{
		set_reverse_order( config_str_to_bool( value, true ) );
	}
	else if ( setting.compare( indexStrings[ListLimit] ) == 0 ) // list_limit
	{
		set_list_limit( as_int( value ) );
	}
	else
	{
		invalid_setting( fn, "filter", setting, indexStrings );
		return 1;
	}

	return 0;
}

void FeFilter::save( nowide::ofstream &f, const char *filter_tag, const int indent ) const
{
	write_pair( f, filter_tag, quote_config( m_name ), indent );

	if ( m_sort_by != FeRomInfo::LAST_INDEX )
		write_pair( f, indexStrings[SortBy], FeRomInfo::indexStrings[ m_sort_by ], indent + 1 );

	if ( m_reverse_order != false )
		write_pair( f, indexStrings[ReverseOrder], "yes", indent + 1 );

	if ( m_list_limit != 0 )
		write_pair( f, indexStrings[ListLimit], as_str( m_list_limit ), indent + 1 );

	for ( std::vector<FeRule>::const_iterator itr=m_rules.begin(); itr != m_rules.end(); ++itr )
		(*itr).save( f, indent + 1 );
}

bool FeFilter::test_for_targets( std::set<FeRomInfo::Index> targets ) const
{
	if ( targets.find( get_sort_by() ) != targets.end() )
		return true;

	for ( std::vector<FeRule>::const_iterator itr=m_rules.begin(); itr!=m_rules.end(); ++itr )
	{
		if ( targets.find( (*itr).get_target() ) != targets.end() )
			return true;
	}

	return false;
}

void FeFilter::clear()
{
	m_name.clear();
	m_rules.clear();
	m_rom_index=0;
	m_list_limit=0;
	m_size=0;
	m_sort_by=FeRomInfo::LAST_INDEX;
	m_reverse_order=false;
}

const char *FeDisplayInfo::indexStrings[] =
{
	"name",
	"layout",
	"romlist",
	"in_cycle",
	"in_menu",
	NULL
};

const char *FeDisplayInfo::otherStrings[] =
{
	"filter",
	"global_filter",
	NULL
};

FeDisplayInfo::FeDisplayInfo( const std::string &n )
	: m_rom_index( 0 ),
	m_filter_index( 0 ),
	m_current_config_filter( NULL ),
	m_global_filter( "" )
{
	m_info[ Name ] = n;
	m_info[ InCycle ] = "yes";
	m_info[ InMenu ] = "yes";
}

const std::string &FeDisplayInfo::get_info( int i ) const
{
	return m_info[i];
}

int FeDisplayInfo::get_rom_index( int filter_index ) const
{
	if (( filter_index >= 0 ) && ( filter_index < (int)m_filters.size() ))
		return m_filters[ filter_index ].get_rom_index();

	return m_rom_index;
}

void FeDisplayInfo::set_rom_index( int filter_index, int rom_index )
{
	if (( filter_index >= 0 ) && ( filter_index < (int)m_filters.size() ))
		m_filters[ filter_index ].set_rom_index( rom_index );
	else
		m_rom_index = rom_index;
}

std::string FeDisplayInfo::get_current_layout_file() const
{
	return m_current_layout_file;
}

void FeDisplayInfo::set_current_layout_file( const std::string &n )
{
	m_current_layout_file = n;
}

void FeDisplayInfo::set_info( int setting,
         const std::string &value )
{
	if (( setting == Layout ) && ( value.compare( m_info[ Layout ] ) != 0 ))
	{
		// If changing the layout, reset the layout file as well
		m_current_layout_file = "";
	}

	m_info[ setting ] = value;
}

int FeDisplayInfo::process_setting( const std::string &setting,
         const std::string &value, const std::string &fn )
{
	// name is igored here, it gets set directly
	//
	if ( setting.compare( indexStrings[Layout] ) == 0 ) // layout
		m_info[ Layout ] = value;
	else if ( setting.compare( indexStrings[Romlist] ) == 0 ) // romlist
		m_info[ Romlist ] = value;
	else if ( setting.compare( indexStrings[InCycle] ) == 0 ) // in_cycle
		m_info[ InCycle ] = value;
	else if ( setting.compare( indexStrings[InMenu] ) == 0 ) // in_menu
		m_info[ InMenu ] = value;
	else if ( setting.compare( otherStrings[0] ) == 0 ) // filter
	{
		size_t pos=0;
		std::string name;
		token_helper( value, pos, name, FE_WHITESPACE );

		// Create a new filter with the given name
		//
		m_filters.push_back( FeFilter( name ) );
		m_current_config_filter = &(m_filters.back());
	}
	else if ( setting.compare( otherStrings[1] ) == 0 ) // global_filter
	{
		m_global_filter.clear();
		m_current_config_filter = &(m_global_filter);
	}
	else
	{
		if ( m_layout_per_display_params.process_setting( setting, value, fn ) == 0 )
		{
			// nothing to do
		}
		else if ( m_current_config_filter )
			m_current_config_filter->process_setting( setting, value, fn );
		else
		{
			invalid_setting( fn, "display", setting, indexStrings + 1, otherStrings );
			return 1;
		}
	}

	return 0;
}

int FeDisplayInfo::process_state( const std::string &state_string, bool new_format )
{
	// state string can be in one of two formats:
	//
	// New: "[display_name];[curr_rom];[curr_layout_filename];[curr_filter];"
	// Old: "[curr_rom];[curr_layout_filename];[curr_filter];"
	//
	// With [curr_rom] = "[rom_index filter0],[rom_index filter1],..."
	//
	size_t pos=0;
	std::string val;

	// Skip the display name field - we match by name before calling this function
	if ( new_format )
		token_helper( state_string, pos, val );

	// Get the [curr_rom] string
	token_helper( state_string, pos, val );

	//
	// Process the [curr_rom] string
	//
	if ( m_filters.empty() )
	{
		// If there are no filters we stash the current rom in m_rom_index
		m_rom_index = as_int( val );
	}
	else
	{
		// If there are filters we get a current rom for each filter
		size_t sub_pos=0;
		int findex=0;
		while (( sub_pos < val.size() ) && ( findex < (int)m_filters.size() ))
		{
			std::string sub_val;
			token_helper( val, sub_pos, sub_val, "," );
			m_filters[findex].set_rom_index( as_int( sub_val ) );
			findex++;
		}
	}

	if ( pos >= state_string.size() )
		return 0;

	token_helper( state_string, pos, val );
	m_current_layout_file = val;

	if ( pos >= state_string.size() )
		return 0;

	token_helper( state_string, pos, val );
	m_filter_index = as_int( val );

	if ( m_filter_index >= (int)m_filters.size() )
		m_filter_index = m_filters.size() - 1;
	if ( m_filter_index < 0 )
		m_filter_index = 0;

	return 0;
}

std::string FeDisplayInfo::state_as_output() const
{
	std::ostringstream state;

	state << get_info( Name ) << ";";

	if ( m_filters.empty() )
	{
		state << m_rom_index;
	}
	else
	{
		for ( std::vector<FeFilter>::const_iterator itr=m_filters.begin();
			itr != m_filters.end(); ++itr )
		{
			state << (*itr).get_rom_index() << ",";
		}
	}

	state << ";" << m_current_layout_file << ";"
		<< m_filter_index << ";";

	return state.str();
}

FeFilter *FeDisplayInfo::get_filter( int i )
{
	if ( i < 0 )
		return &m_global_filter;

	if ( i >= (int)m_filters.size() )
		return NULL;

	return &(m_filters[i]);
}

void FeDisplayInfo::append_filter( const FeFilter &f )
{
	m_filters.push_back( f );
}

void FeDisplayInfo::delete_filter( int i )
{
	if (( m_filter_index > 0 ) && ( m_filter_index >= i ))
		m_filter_index--;

	m_filters.erase( m_filters.begin() + i );
}

void FeDisplayInfo::get_filters_list( std::vector<std::string> &l ) const
{
	l.clear();

	for ( std::vector<FeFilter>::const_iterator itr=m_filters.begin();
			itr != m_filters.end(); ++itr )
	{
		l.push_back( (*itr).get_name() );
	}
}

void FeDisplayInfo::save( nowide::ofstream &f, const int indent ) const
{
	if ( !get_info( Layout ).empty() )
		write_pair( f, indexStrings[Layout], get_info( Layout ), indent );
	if ( !get_info( Romlist ).empty() )
		write_pair( f, indexStrings[Romlist], get_info( Romlist ), indent );
	write_pair( f, indexStrings[InCycle], get_info( InCycle ), indent );
	write_pair( f, indexStrings[InMenu], get_info( InMenu ), indent );

	if ( m_global_filter.get_rule_count() > 0 )
		m_global_filter.save( f, otherStrings[1], indent );

	for ( std::vector<FeFilter>::const_iterator itr=m_filters.begin(); itr != m_filters.end(); ++itr )
		(*itr).save( f, otherStrings[0], indent );

	m_layout_per_display_params.save( f, indent );
	f << std::endl;
}

bool FeDisplayInfo::show_in_cycle() const
{
	return config_str_to_bool( m_info[InCycle] );
}

bool FeDisplayInfo::show_in_menu() const
{
	return config_str_to_bool( m_info[InMenu] );
}

//
// Returns true if the global or any other filter uses any target in its ruleset
//
bool FeDisplayInfo::test_for_targets( std::set<FeRomInfo::Index> targets )
{
	FeFilter *global_filter = get_global_filter();
	if ( global_filter && global_filter->test_for_targets( targets ) ) return true;

	int filters_count = get_filter_count();
	for ( int i=0; i<filters_count; i++ )
		if ( get_filter( i )->test_for_targets( targets ) ) return true;

	return false;
}

const char *FeEmulatorInfo::indexStrings[] =
{
	"name",
	"executable",
	"args",
	"workdir",
	"rompath",
	"romext",
	"system",
	"info_source",
	"import_extras",
	"nb_mode_wait",
	"exit_hotkey",
	"pause_hotkey",
	NULL
};

const char *FeEmulatorInfo::indexDispStrings[] =
{
	"Name",
	"Executable",
	"Command Arguments",
	"Working Directory",
	"Rom Path(s)",
	"Rom Extension(s)",
	"System Identifier",
	"Info Source/Scraper",
	"Additional Import File(s)",
	"Non-Blocking Mode Wait",
	"Exit Hotkey",
	"Pause Hotkey",
	NULL
};

const char *FeEmulatorInfo::infoSourceStrings[] =
{
	"",
	"listxml",
	"listsoftware",
	"steam",
	"thegamesdb.net",
	"scummvm",
	"listsoftware+thegamesdb.net",
	NULL
};

FeEmulatorInfo::FeEmulatorInfo()
	: m_info_source( None ),
	m_nbm_wait( 0 )
{
}

FeEmulatorInfo::FeEmulatorInfo( const std::string &n )
	: m_name( n ),
	m_info_source( None ),
	m_nbm_wait( 0 )
{
}

const std::string FeEmulatorInfo::get_info( int i ) const
{
	switch ( (Index)i )
	{
	case Name:
		return m_name;
	case Executable:
		return m_executable;
	case Command:
		return m_command;
	case Working_dir:
		return m_workdir;
	case Rom_path:
		return vector_to_string( m_paths );
	case Rom_extension:
		return vector_to_string( m_extensions );
	case System:
		return vector_to_string( m_systems );
	case Info_source:
		return infoSourceStrings[m_info_source];
	case Import_extras:
		return vector_to_string( m_import_extras );
	case NBM_wait:
		return as_str( m_nbm_wait );
	case Exit_hotkey:
		return m_exit_hotkey;
	case Pause_hotkey:
		return m_pause_hotkey;
	default:
		return "";
	}
}

void FeEmulatorInfo::set_info( enum Index i, const std::string &s )
{
	switch ( i )
	{
	case Name:
		m_name = s; break;
	case Executable:
		m_executable = s; break;
	case Command:
		m_command = s; break;
	case Working_dir:
		m_workdir = s; break;
	case Rom_path:
		m_paths.clear();
		string_to_vector( s, m_paths );
		break;
	case Rom_extension:
		m_extensions.clear();
		string_to_vector( s, m_extensions, true );
		break;
	case System:
		m_systems.clear();
		string_to_vector( s, m_systems );
		break;
	case Info_source:
		for ( int i=0; i<LAST_INFOSOURCE; i++ )
		{
			if ( s.compare( infoSourceStrings[i] ) == 0 )
			{
				m_info_source = (InfoSource)i;
				return;
			}
		}

		//
		// Special case handling for versions <= 1.6.0
		//
		if ( s.compare( "mame" ) == 0 )
			m_info_source = Listxml;
		else if ( s.compare( "mess" ) == 0 )
			m_info_source = Listsoftware;
		else
			m_info_source = None;

		break;

	case Import_extras:
		m_import_extras.clear();
		string_to_vector( s, m_import_extras );
		break;
	case NBM_wait:
#if !defined(NO_NBM_WAIT)
		m_nbm_wait = as_int( s );
#endif
		break;
	case Exit_hotkey:
		m_exit_hotkey = s; break;
	case Pause_hotkey:
#if !defined(NO_PAUSE_HOTKEY)
		m_pause_hotkey = s;
#endif
		break;
	default:
		break;
	}
}

const std::vector<std::string> &FeEmulatorInfo::get_paths() const
{
	return m_paths;
}

const std::vector<std::string> &FeEmulatorInfo::get_extensions() const
{
	return m_extensions;
}

const std::vector<std::string> &FeEmulatorInfo::get_systems() const
{
	return m_systems;
}

const std::vector<std::string> &FeEmulatorInfo::get_import_extras() const
{
	return m_import_extras;
}

bool FeEmulatorInfo::get_artwork( const std::string &label, std::string &artwork ) const
{
	std::map<std::string, std::vector<std::string> >::const_iterator itm;
	itm = m_artwork.find( label );
	if ( itm == m_artwork.end() )
		return false;

	artwork = vector_to_string( (*itm).second );
	return true;
}

bool FeEmulatorInfo::get_artwork( const std::string &label, std::vector< std::string > &artwork ) const
{
	std::map<std::string, std::vector<std::string> >::const_iterator itm;
	itm = m_artwork.find( label );
	if ( itm == m_artwork.end() )
		return false;

	artwork.clear();
	for ( std::vector<std::string>::const_iterator its = (*itm).second.begin();
			its != (*itm).second.end(); ++its )
		artwork.push_back( *its );

	return true;
}

void FeEmulatorInfo::add_artwork( const std::string &label,
		const std::string &artwork )
{
	// don't clear m_artwork[ label ], it may have entries already
	// see process_setting() and special case for migrating movie settings
	// from pre 1.2.2 versions
	//
	string_to_vector( artwork, m_artwork[ label ] );
}

void FeEmulatorInfo::get_artwork_list(
		std::vector< std::pair< std::string, std::string > > &out_list ) const
{
	out_list.clear();
	std::map<std::string, std::vector<std::string> >::const_iterator itm;

	for ( itm=m_artwork.begin(); itm != m_artwork.end(); ++itm )
	{
		std::string path_list = vector_to_string( (*itm).second );
		out_list.push_back( std::pair<std::string,std::string>( (*itm).first, path_list ) );
	}
}

void FeEmulatorInfo::delete_artwork( const std::string &label )
{
	std::map<std::string, std::vector<std::string> >::iterator itm;
	itm = m_artwork.find( label );
	if ( itm != m_artwork.end() )
		m_artwork.erase( itm );
}

int FeEmulatorInfo::process_setting( const std::string &setting,
         const std::string &value, const std::string &fn )
{

	const char *stokens[] = { "artwork", NULL };

	// name is ignored here, it gets set directly
	//
	for ( int i=1; i < LAST_INDEX; i++ )
	{
		if ( setting.compare( indexStrings[i] ) == 0 )
		{
			set_info( (Index)i, value );
			return 0;
		}
	}

	// Backwards compatability
	//
	if ( setting.compare( "minimum_run_time" ) == 0 ) // "nb_mode_wait" was "minimum_run_time" (<= v2.2)
	{
		set_info( NBM_wait, value );
		return 0;
	}

	if ( setting.compare( stokens[0] ) == 0 ) // artwork
	{
		size_t pos=0;
		std::string label, path;
		token_helper( value, pos, label, FE_WHITESPACE );
		token_helper( value, pos, path, "\n" );
		add_artwork( label, path );
	}
	else
	{
		invalid_setting( fn, "emulator", setting,
				indexStrings + 1, stokens );
		return 1;
	}
	return 0;
}

void FeEmulatorInfo::save( const std::string &filename ) const
{
	FeLog() << "Writing emulator config to: " << filename << std::endl;

	nowide::ofstream outfile( filename.c_str() );
	if ( outfile.is_open() )
	{
		write_header( outfile );

		// skip name, which gets derived from the filename
		for ( int i=1; i < LAST_INDEX; i++ )
		{
			// don't output nbm_wait if it is zero
			if (( i == NBM_wait ) && ( m_nbm_wait == 0 ))
				continue;

			std::string val = get_info( (Index) i );
			if ( !val.empty() )
				write_pair( outfile, as_str( indexStrings[i] ), val );
		}

		std::vector<std::pair<std::string, std::string>> art_list;
		get_artwork_list( art_list );
		for ( std::vector< std::pair< std::string, std::string > >::iterator itr=art_list.begin(); itr != art_list.end(); ++itr )
			write_param( outfile, "artwork", quote_config( (*itr).first ), (*itr).second );

		outfile.close();
   }
}

std::string FeEmulatorInfo::vector_to_string( const std::vector< std::string > &vec ) const
{
	std::string ret_str;
	for ( unsigned int i=0; i < vec.size(); i++ )
	{
		if ( i > 0 ) // there could be empty entries in the vector...
			ret_str += ";";

		ret_str += vec[i];
	}
	return ret_str;
}

void FeEmulatorInfo::gather_rom_names( std::vector<std::string> &name_list ) const
{
	std::vector< std::string > ignored;
	gather_rom_names( name_list, ignored );
}

void FeEmulatorInfo::gather_rom_names(
	std::vector<std::string> &name_list,
	std::vector<std::string> &full_path_list ) const
{
	for ( std::vector<std::string>::const_iterator itr=m_paths.begin();
			itr!=m_paths.end(); ++itr )
	{
		std::string path = clean_path_with_wd( *itr, true );

		for ( std::vector<std::string>::const_iterator ite = m_extensions.begin();
				ite != m_extensions.end(); ++ite )
		{
			std::vector<std::string> temp_list;
			std::vector<std::string>::iterator itn;

			if ( (*ite).compare( FE_DIR_TOKEN ) == 0 )
			{
				get_subdirectories( temp_list, path );

				for ( itn = temp_list.begin(); itn != temp_list.end(); ++itn )
				{
					full_path_list.push_back( path + *itn );
					name_list.push_back( std::string() );
					name_list.back().swap( *itn );
				}
			}
			else
			{
				get_basename_from_extension( temp_list, path, (*ite), true );

				for ( itn = temp_list.begin(); itn != temp_list.end(); ++itn )
				{
					full_path_list.push_back( path + *itn + *ite );
					name_list.push_back( std::string() );
					name_list.back().swap( *itn );
				}
			}
		}
	}
}

std::string FeEmulatorInfo::clean_path_with_wd( const std::string &in_path, bool add_trailing_slash ) const
{
	std::string res = clean_path( in_path, add_trailing_slash );

	if ( is_relative_path( res ) )
	{
		std::string temp;
		temp.swap( res );
		res = clean_path( m_workdir, true ) + temp;
	}

	return res;
}

bool FeEmulatorInfo::is_mame() const
{
	return ( m_info_source == Listxml );
}

bool FeEmulatorInfo::is_mess() const
{
	return (( m_info_source == Listsoftware )
		|| ( m_info_source == Listsoftware_tgdb ));
}

const char *FeScriptConfigurable::indexString = "param";

bool FeScriptConfigurable::get_param( const std::string &label, std::string &v ) const
{
	std::map<std::string,std::string>::const_iterator itr;

	itr = m_params.find( label );
	if ( itr != m_params.end() )
	{
		v = (*itr).second;
		return true;
	}

	return false;
}

void FeScriptConfigurable::set_param( const std::string &label, const std::string &v )
{
	m_params[ label ] = v;
}

void FeScriptConfigurable::get_param_labels( std::vector<std::string> &labels ) const
{
	std::map<std::string,std::string>::const_iterator itr;
	for ( itr=m_params.begin(); itr!=m_params.end(); ++itr )
		labels.push_back( (*itr).first );
}

int FeScriptConfigurable::process_setting( const std::string &setting,
         const std::string &value, const std::string &fn )
{
	if ( setting.compare( indexString ) == 0 ) // param
	{
		std::string label;
		size_t pos=0;

		token_helper( value, pos, label, FE_WHITESPACE );

		// Dont use token helper to get value, it is printed raw (legacy)
		// - It may contain quotes, leadings spaces, etc
		// - The value is expected to appear 1 space after the label
		m_params[ label ] = ( pos < value.size() ) ? value.substr( pos ) : "";
		return 0;
	}
	else
		return 1;
}

void FeScriptConfigurable::save( nowide::ofstream &f, const int indent ) const
{
	for ( std::map<std::string,std::string>::const_iterator itr=m_params.begin(); itr!=m_params.end(); ++itr )
	{
		// This key/value pair MUST be written with a single space
		// See comment in `process_setting` above
		if ( !(*itr).first.empty() )
			write_pair(
				f,
				indexString,
				(*itr).first + ((*itr).second.empty() ? "" : " " + (*itr).second),
				indent
			);
	}
}

void FeScriptConfigurable::merge_params( const FeScriptConfigurable &o )
{
	std::map<std::string,std::string>::const_iterator itr;

	for ( itr=o.m_params.begin(); itr!=o.m_params.end(); ++itr )
		m_params[ (*itr).first ] = (*itr).second;
}

const char *FePlugInfo::indexStrings[] = { "enabled","param",NULL };

FePlugInfo::FePlugInfo( const std::string & n )
	: m_name( n ), m_enabled( false )
{
}

int FePlugInfo::process_setting( const std::string &setting,
         const std::string &value, const std::string &fn )
{
	if ( setting.compare( indexStrings[0] ) == 0 ) // enabled
		set_enabled( config_str_to_bool( value ) );
	else if ( FeScriptConfigurable::process_setting( setting, value, fn ) ) // params
	{
		invalid_setting( fn, "plugin", setting, indexStrings );
		return 1;
	}
	return 0;
}

void FePlugInfo::save( nowide::ofstream &f ) const
{
	write_section( f, "plugin", m_name );
	write_pair( f, indexStrings[0], m_enabled ? "yes" : "no", 1 );
	FeScriptConfigurable::save( f, 1 );
	f << std::endl;
}

const char *FeLayoutInfo::indexStrings[] = {
	"saver_config",
	"layout_config",
	"intro_config",
	"menu_config",
	NULL
};

FeLayoutInfo::FeLayoutInfo( Type t )
	: m_type( t )
{
}

FeLayoutInfo::FeLayoutInfo( const std::string &name )
	: m_name( name ),
	m_type( Layout )
{
}

void FeLayoutInfo::save( nowide::ofstream &f ) const
{
	if ( !m_params.empty() )
	{
		write_section( f, indexStrings[ m_type ], ( m_type == Layout ) ? m_name : "" );
		FeScriptConfigurable::save( f, 1 );
		f << std::endl;
	}
}

bool FeLayoutInfo::operator!=( const FeLayoutInfo &o )
{
	return (( m_name != o.m_name )
		|| ( m_type != o.m_type )
		|| ( m_params != o.m_params ));
}


std::map<std::string, std::string> FeTranslationMap::m_map = {};

FeTranslationMap::FeTranslationMap()
{
}

void FeTranslationMap::clear()
{
	m_map.clear();
}

int FeTranslationMap::process_setting(
	const std::string &setting,
	const std::string &value,
	const std::string &filename
)
{
	m_map[setting] = value;
	return 0;
}

const std::string FeTranslationMap::get_translation( const std::string &token, const std::vector<std::string> &rep )
{
	if ( token.empty() )
		return "";

	std::map<std::string, std::string>::const_iterator it = m_map.find( token );

	std::string value;
	if ( it != m_map.end() )
		value = (*it).second;
	else
	{
		// FeLog() << "Missing translation: '" << token << "'" << std::endl;
		// Use token itself if not an "id" token, such as _help_name
		if ( token[0] != '_' )
			value = token;
	}

	perform_substitution( value, rep );
	return value;
}

// Return translated string
const std::string _( const std::string &token, const std::vector<std::string> &rep )
{
	return FeTranslationMap::get_translation( token, rep );
}

// Return translated char
const std::string _( const char* token, const std::vector<std::string> &rep )
{
	return _( std::string( token ), rep );
}

// Return translated list of strings
const std::vector<std::string> _( const std::vector<std::string> &tokens, const std::vector<std::string> &rep )
{
	int n = (int)tokens.size();
	std::vector<std::string> ret;
	ret.reserve( n );
	for ( int i=0; i<n; i++ ) ret.push_back( _( tokens[i], rep ) );
	return ret;
}

// Return translated list of chars
const std::vector<std::string> _( const char* tokens[], const std::vector<std::string> &rep )
{
	int n = 0;
	while ( tokens[n] != 0 ) n++;
	std::vector<std::string> ret;
	ret.reserve( n );
	for ( int i=0; i<n; i++ ) ret.push_back( _( tokens[i], rep ) );
	return ret;
}