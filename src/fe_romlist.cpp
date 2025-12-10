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

#include "fe_romlist.hpp"
#include "fe_util.hpp"
#include "fe_util_sq.hpp"
#include "fe_cache.hpp"

#include <iostream>
#include "nowide/fstream.hpp"
#include <algorithm>
#include <numeric>
#include <random>

#include <squirrel.h>
#include <sqstdstring.h>

#include <SFML/System/Clock.hpp>

const char *FE_ROMLIST_FILE_EXTENSION	= ".txt";
const char *FE_TAG_FILE_EXTENSION		= ".tag";
const char *FE_ROMLIST_SUBDIR			= "romlists/";
const char *FE_STATS_SUBDIR				= "stats/";

namespace
{
	std::mt19937 rnd{ std::random_device{}() };

	bool fe_not_clone( const FeRomInfo &r )
	{
		return r.get_info( FeRomInfo::Cloneof ).empty();
	}
};

FeRomListSorter::FeRomListSorter( FeRomInfo::Index c, bool rev )
	: m_comp( c ),
	m_reverse( rev )
{
}

bool FeRomListSorter::operator()( const FeRomInfo &one_obj, const FeRomInfo &two_obj ) const
{
	bool asc;

	if ( m_comp == FeRomInfo::Title )
		// Title sort
		asc = icompare( one_obj.get_sort_title(), two_obj.get_sort_title() ) < 0;
	else if ( m_comp == FeRomInfo::Year )
		// Year sort
		asc = year_as_int( one_obj.get_info( m_comp ) ) < year_as_int( two_obj.get_info( m_comp ) );
	else if ( FeRomInfo::isNumeric( m_comp ) )
		// Numeric sort
		asc = as_float( one_obj.get_info( m_comp ) ) < as_float( two_obj.get_info( m_comp ) );
	else
		// String sort
		asc = icompare( one_obj.get_info( m_comp ), two_obj.get_info( m_comp ) ) < 0;

	// If reverse_order then invert sorting
	return m_reverse ? !asc : asc;
}

FeRomList::FeRomList( const std::string &config_path )
	: m_config_path( config_path ),
	m_fav_changed( false ),
	m_tags_changed( false ),
	m_availability_checked( false ),
	m_played_stats_checked( false ),
	m_group_clones( true )
{
}

FeRomList::~FeRomList()
{
}

void FeRomList::init_as_empty_list()
{
	m_romlist_path.clear();
	m_romlist_name.clear();
	m_list.clear();
	m_filtered_list.clear();
	m_filtered_list.push_back( FeFilterEntry() ); // there always has to be at least one filter
	m_tags.clear();
	m_extra_favs.clear();
	m_extra_tags.clear();

	m_availability_checked = false;
	m_played_stats_checked = false;
	m_group_clones = true;
	m_fav_changed = false;
	m_tags_changed = false;
}

void FeRomList::mark_favs_and_tags_changed()
{
	m_fav_changed = true;
	m_tags_changed = true;
}

// Return path to the favourites tag file (not guaranteed to exist)
std::string FeRomList::get_fav_path()
{
	return m_config_path + FE_ROMLIST_SUBDIR + m_romlist_name + FE_TAG_FILE_EXTENSION;
}

// Create tag dir, called prior to saving tags
bool FeRomList::confirm_tag_dir()
{
	return confirm_directory( m_config_path + FE_ROMLIST_SUBDIR, m_romlist_name );
}

// Return dir containing tags for this romlist
std::string FeRomList::get_tag_dir()
{
	return m_config_path + FE_ROMLIST_SUBDIR + m_romlist_name + "/";
}

// Return list of tag filenames in folder
std::vector<std::string> FeRomList::get_tag_names()
{
	std::vector<std::string> names;
	std::string tag_dir = get_tag_dir();
	if ( directory_exists( tag_dir ) )
		get_basename_from_extension( names, tag_dir, FE_TAG_FILE_EXTENSION );
	return names;
}

// Return path to given tag file
std::string FeRomList::get_tag_path( std::string tag )
{
	return get_tag_dir() + tag + FE_TAG_FILE_EXTENSION;
}

//
// Populate map wih { id -> [rominfo] }, used for Tag application
// - One id may have multiple rominfo items if the same rom appears multiple times in the romlist!
//
void FeRomList::get_romlist_map( std::map<std::string, std::vector<FeRomInfo*>> &rom_map )
{
	for ( FeRomInfoListType::iterator itr = m_list.begin(); itr != m_list.end(); ++itr )
	{
		rom_map[ (*itr).get_id() ].push_back( &(*itr) );
		// Adds legacy id to match tag files that dont have emu info
		rom_map[ (*itr).get_info( FeRomInfo::Romname ) + FE_TAGS_SEP ].push_back( &(*itr) );
	}
}

//
// Populate m_list data from cache or file
// - Invalidates cache if outdated
//
int FeRomList::load_romlist_data(
	FeDisplayInfo &display
)
{
	FeFilter *global_filter = display.get_global_filter();
	bool has_global_rules = global_filter && ( global_filter->get_rule_count() > 0 );

	// Load globalfilter cache ( if has global rules )
	if ( has_global_rules && FeCache::load_globalfilter( display, *this ) )
		return RomlistResponse::Loaded_Global;

	// Load romlist cache ( if no global rules )
	if ( FeCache::load_romlist( *this ) )
		return RomlistResponse::Loaded_Romlist;

	// Reaching this point means the romlist was invalidated
	// That means this display was also invalidated
	// Re-save it now to prevent another invalidation in create_filters
	FeCache::save_display( display, *this );

	// Load romlist from file
	if ( !FeBaseConfigurable::load_from_file( m_romlist_path, ";" ) )
		return RomlistResponse::Loaded_None;

	// If grouping by clones, partition the list so masters are ordered before clones
	// This way masters will be used for the group label, and will be selected upon entering
	if ( m_group_clones )
		std::stable_partition( m_list.begin(), m_list.end(), fe_not_clone );

	return FeCache::save_romlist( *this )
		? RomlistResponse::Loaded_File | RomlistResponse::Saved_Romlist
		: RomlistResponse::Loaded_File;
}

//
// Used during load_from_file to append rom to m_list
//
int FeRomList::process_setting(
	const std::string &setting,
	const std::string &value,
	const std::string &fn )
{
	FeRomInfo rom( setting );
	rom.process_setting( setting, value, fn );
	rom.index = m_list.size(); // index for filter cache
	m_list.push_back( rom );
	return 0;
}

//
// Apply Favourites from file to the current m_list
// - Stores unused favs in m_extra_favs for saving
//
void FeRomList::load_fav_data(
	std::map<std::string, std::vector<FeRomInfo*>> &rom_map
)
{
	m_extra_favs.clear();
	nowide::ifstream file( get_fav_path() );
	if ( !file.is_open() ) return;

	size_t pos;
	std::string line, name, emu, id;
	std::map<std::string, std::vector<FeRomInfo*>>::iterator itm;

	while ( file.good() )
	{
		pos = 0;
		getline( file, line );
		token_helper( line, pos, name );
		if ( name.empty() ) continue;
		token_helper( line, pos, emu );
		id = name + FE_TAGS_SEP + emu;

		// Add fav to the rom, or the extra list if no rom
		itm = rom_map.find( id );
		if ( itm != rom_map.end() )
		{
			for ( std::vector<FeRomInfo*>::iterator itr=(*itm).second.begin(); itr!=(*itm).second.end(); ++itr )
				(*itr)->set_info( FeRomInfo::Favourite, "1" );
		}
		else
			m_extra_favs.insert( id );
	}

	file.close();
}

//
// Apply Tags from file(s) to the current m_list
// - Populates m_tags
// - Stores unused tags in m_extra_tags for saving
//
void FeRomList::load_tag_data(
	std::map<std::string, std::vector<FeRomInfo*>> &rom_map
)
{
	m_tags.clear();
	m_extra_tags.clear();
	std::vector<std::string> tags = get_tag_names();

	size_t pos;
	std::string line, name, emu, tag;
	std::map<std::string, std::vector<FeRomInfo*>>::iterator itm;

	for ( std::vector<std::string>::iterator itt=tags.begin(); itt!=tags.end(); ++itt )
	{
		tag = *itt;
		nowide::ifstream file( get_tag_path( tag ) );
		if ( !file.is_open() ) continue;

		// Add tag to the master list
		m_tags.insert( std::pair<std::string, bool>( tag, false ) );

		// Collect all tags into a set which removes any duplicates
		std::set<std::string> ids;
		while ( file.good() )
		{
			pos = 0;
			getline( file, line );
			token_helper( line, pos, name );
			if ( name.empty() ) continue;
			token_helper( line, pos, emu );
			ids.insert( name + FE_TAGS_SEP + emu );
		}
		file.close();

		// Go thru set and add tag to rom, or the extra list if no rom
		for ( std::set<std::string>::iterator iti=ids.begin(); iti!=ids.end(); ++iti )
		{
			itm = rom_map.find( *iti );
			if ( itm != rom_map.end() )
			{
				for ( std::vector<FeRomInfo*>::iterator itr=(*itm).second.begin(); itr!=(*itm).second.end(); ++itr )
					(*itr)->append_tag( tag );
			}
			else
				m_extra_tags.insert( std::pair( tag, *iti ) );
		}

	}
}

//
// Add shuffle data to romlist for randomized sorting
//
void FeRomList::load_shuffle_data()
{
	std::vector<int> shuffle( m_list.size() );
	std::iota( shuffle.begin(), shuffle.end(), 0 );
	std::shuffle( shuffle.begin(), shuffle.end(), rnd );
	int i = 0;
	for ( FeRomInfoListType::iterator it=m_list.begin(); it!=m_list.end(); ++it )
		(*it).set_info( FeRomInfo::Shuffle, as_str( shuffle[i++] ) );
}

//
// Add given rom tags to the extra lists for saving
// - Called when rom is filtered from list
//
void FeRomList::store_extra_tags( FeRomInfo &rom )
{
	if ( !rom.get_info( FeRomInfo::Favourite ).empty() )
		m_extra_favs.insert( rom.get_id() );

	if ( !rom.get_info( FeRomInfo::Tags ).empty() )
	{
		const std::string &id = rom.get_id();
		std::vector<std::pair<std::string, bool>> tags_list;
		get_tags_list( rom, tags_list );

		for ( std::vector<std::pair<std::string, bool>>::iterator itr=tags_list.begin(); itr!=tags_list.end(); ++itr )
			if ( (*itr).second ) m_extra_tags.insert( std::pair( (*itr).first, id ) );
	}
}

//
// Apply global filter to current m_list
// - This method is SLOW as it applies filters to ALL m_list entries
//
int FeRomList::apply_global_filter(
	FeDisplayInfo &display
)
{
	FeFilter *global_filter = display.get_global_filter();
	bool has_global_rules = global_filter && (global_filter->get_rule_count() > 0);
	if ( !has_global_rules ) return RomlistResponse::Loaded_None;

	global_filter->init();

	// Filter m_list and remove items directly
	FeRomInfoListType::iterator last_it=m_list.begin();
	for ( FeRomInfoListType::iterator it=m_list.begin(); it!=m_list.end(); )
	{
		// If filtered out, store rom tags for saving
		if ( !global_filter->apply_filter( *it ) )
		{
			store_extra_tags( *it );
			++it;
			continue;
		}

		// Remove filtered roms
		if ( last_it != it )
			it = m_list.erase( last_it, it );
		else
			++it;

		last_it = it;
	}

	// Remove trailing filtered roms
	if ( last_it != m_list.end() )
		m_list.erase( last_it, m_list.end() );

	// Save globalfilter cache for next time
	return FeCache::save_globalfilter( display, *this )
		? RomlistResponse::Filtered_Global | RomlistResponse::Saved_Global
		: RomlistResponse::Filtered_Global;
}

//
// Loads a Romlist from cache (or file if not cache) into m_list
// - Applies GlobalFilters and saves cache as necessary
// - Does NOT create filter lists, caller must use `create_filters`
//
bool FeRomList::load_romlist(
	const std::string &path,
	const std::string &romlist_name,
	FeDisplayInfo &display,
	bool group_clones,
	bool load_stats	)
{
	init_as_empty_list();
	m_romlist_path = path;
	m_romlist_name = romlist_name;
	m_group_clones = group_clones;
	m_played_stats_checked = !load_stats;

	sf::Clock load_timer;

	bool test_available = display.test_for_targets({ FeRomInfo::FileIsAvailable });
	bool test_stats = display.test_for_targets( std::set<FeRomInfo::Index>( FeRomInfo::Stats.begin(), FeRomInfo::Stats.end() ) );
	bool test_shuffle = display.test_for_targets({ FeRomInfo::Shuffle });
	std::map<std::string, std::vector<std::string>> emu_roms;

	FeCache::clear_stats();
	if ( FeCache::validate_romlistmeta( *this ) && FeCache::validate_display( display, *this ) && test_available )
		FeCache::validate_available( *this, emu_roms );

	int response = load_romlist_data( display );
	int romlist_size = m_list.size();

	std::map<std::string, std::vector<FeRomInfo*>> rom_map;
	get_romlist_map( rom_map );
	load_fav_data( rom_map );
	load_tag_data( rom_map );
	if ( test_available ) get_file_availability( emu_roms );
	if ( test_stats ) get_played_stats();
	if ( test_shuffle ) load_shuffle_data();

	if ( !(response & RomlistResponse::Loaded_Global) )
		response |= apply_global_filter( display );

	FeLog() << " - Loaded romlist '" << m_romlist_name << "' in " << load_timer.getElapsedTime().asMilliseconds() << " ms (";
	FeLog() << romlist_size << " entries";
	if ( response & RomlistResponse::Loaded_File ) 		FeLog() << " from romlist";
	if ( response & RomlistResponse::Loaded_Romlist ) 	FeLog() << " from romlist cache";
	if ( response & RomlistResponse::Loaded_Global ) 	FeLog() << " from globalfilter cache";
	if ( response & RomlistResponse::Filtered_Global ) 	FeLog() << ", " << m_list.size() << " kept";
	if ( response & RomlistResponse::Saved_Romlist ) 	FeLog() << ", updated romlist cache";
	if ( response & RomlistResponse::Saved_Global ) 	FeLog() << ", updated globalfilter cache";
	FeLog() << ")" << std::endl;

	return ( response & RomlistResponse::Loaded_Romlist )
		|| ( response & RomlistResponse::Loaded_Global )
		|| ( response & RomlistResponse::Loaded_File );
}

//
// Populate the filter entry
//
void FeRomList::build_single_filter_list(
	FeFilter *f,
	FeFilterEntry &result
)
{
	build_filter_entry( f, result );
	if ( f ) f->set_size( result.filter_list.size() ); // Store size of pre-limited list
	sort_filter_entry( f, result );
}

inline void FeRomList::add_group_entry(
	FeRomInfo &rom,
	FeFilterEntry &result
)
{
	// Find the group
	std::string group_name = rom.get_clone_parent();
	std::map<std::string,std::vector<FeRomInfo*>>::iterator itg = result.clone_group.find( group_name );

	// Add a new group if none
	if ( itg == result.clone_group.end() )
	{
		result.filter_list.push_back( &rom );
		itg = result.clone_group.insert( itg, std::pair( group_name, std::vector<FeRomInfo*>() ) );
	}

	// Add current rom to the group
	itg->second.push_back( &rom );
}

//
// Apply the given filter to populate the filter_list and clone_group
//
void FeRomList::build_filter_entry(
	FeFilter *f,
	FeFilterEntry &result
)
{
	// Clear clone_group and filter_list
	result.clear();
	std::vector<FeRomInfo*> &filter_list = result.filter_list;

	// Since m_list could be large we make the filter loops as tight as possible
	if ( f )
	{
		f->init();
		m_comparisons += m_list.size();
		if ( m_group_clones )
		{
			for ( FeRomInfoListType::iterator itr=m_list.begin(); itr!=m_list.end(); ++itr )
				if ( f->apply_filter( *itr ) ) add_group_entry( *itr, result );
		}
		else
		{
			for ( FeRomInfoListType::iterator itr=m_list.begin(); itr!=m_list.end(); ++itr )
				if ( f->apply_filter( *itr ) ) filter_list.push_back( &(*itr) );
		}
	}
	else
	{
		if ( m_group_clones )
		{
			for ( FeRomInfoListType::iterator itr=m_list.begin(); itr!=m_list.end(); ++itr )
				add_group_entry( *itr, result );
		}
		else
		{
			filter_list.reserve( m_list.size() );
			for ( FeRomInfoListType::iterator itr=m_list.begin(); itr!=m_list.end(); ++itr )
				filter_list.push_back( &(*itr) );
		}
	}
}

//
// Apply filter sorting and limiting to the entry
//
void FeRomList::sort_filter_entry(
	FeFilter *f,
	FeFilterEntry &result
)
{
	if ( !f ) return;
	std::vector<FeRomInfo*> &filter_list = result.filter_list;
	std::map<std::string, std::vector<FeRomInfo*>> &clone_group = result.clone_group;

	// Sort and limit the filtered list
	FeRomInfo::Index sort_by = f->get_sort_by();
	bool rev = f->get_reverse_order();
	int limit = f->get_list_limit();

	// Sorting
	if ( sort_by != FeRomInfo::LAST_INDEX )
	{
		// Sort the filter list
		std::stable_sort( filter_list.begin(), filter_list.end(), FeRomListSorter2( sort_by, rev ) );

		// Sort the clone groups
		std::map<std::string, std::vector<FeRomInfo*>>::iterator itg;
		for ( itg = clone_group.begin(); itg != clone_group.end(); ++itg )
			std::stable_sort( (*itg).second.begin(), (*itg).second.end(), FeRomListSorter2( sort_by, rev ) );
	}
	else if ( rev != false )
	{
		// If not sorted the romlist entry order is used - but may still be reversed
		std::reverse( filter_list.begin(), filter_list.end() );

		// Reverse the clone groups
		std::map<std::string, std::vector<FeRomInfo*>>::iterator itg;
		for ( itg = clone_group.begin(); itg != clone_group.end(); ++itg )
			std::reverse( (*itg).second.begin(), (*itg).second.end() );
	}

	// Limiting
	if (( limit != 0 ) && ( (int)filter_list.size() > abs( limit ) ))
	{
		int a = limit > 0 ? limit : 0; // +ve limit keeps head
		int b = limit > 0 ? 0 : limit; // -ve limit keeps tail
		filter_list.erase( filter_list.begin() + a, filter_list.end() + b );
	}
}

//
// Populate group with clones of the rom at the given index
//
void FeRomList::get_clone_group(
	int filter_idx,
	int idx,
	std::vector<FeRomInfo*> &group
)
{
	FeRomInfo &rom = lookup( filter_idx, idx );

	// Get the clone name
	std::string group_name = rom.get_clone_parent();

	// Find the clone group for the target group_name
	std::map<std::string, std::vector<FeRomInfo*>>::iterator it = m_filtered_list[filter_idx].clone_group.find( group_name );
	if ( it != m_filtered_list[filter_idx].clone_group.end() )
	{
		// Populate the group
		std::vector<FeRomInfo*>::iterator itr;
		for ( itr = (*it).second.begin(); itr != (*it).second.end(); ++itr )
			group.push_back( (*itr) );
	}
	else
	{
		// No group found, just return the target rom
		group.push_back( &rom );
	}
}

void FeRomList::create_filters(
	FeDisplayInfo &display )
{
	// create_filters may be called by update_romlist_after_edit
	// - we must validate AGAIN to clear any filters the rom may now appear in
	// - NOTE: this may also invalidate the globalfilter - but it is NOT reloaded at this time... (same as legacy)
	FeCache::validate_display( display, *this );

	sf::Clock load_timer;

	// Prepare an indexed lookup for filter cache loading
	std::map<int, FeRomInfo*> lookup;
	for ( FeRomInfoListType::iterator it=m_list.begin(); it!=m_list.end(); ++it )
		lookup[it->index] = &(*it);

	// If no filters configured create a single filter containing entire romlist
	int filters_count = std::max( display.get_filter_count(), 1 );
	int filters_cached = 0;
	m_comparisons = 0;

	// Apply filters
	m_filtered_list.clear();
	m_filtered_list.reserve( filters_count );
	for ( int i=0; i<filters_count; i++ )
	{
		m_filtered_list.push_back( FeFilterEntry() );

		// Attempt to load filter from cache
		if ( FeCache::load_filter( display, m_filtered_list[i], i, lookup ) )
		{
			filters_cached++;
			continue;
		}

		// If no cache, build and save the filter from scratch
		build_single_filter_list( display.get_filter( i ), m_filtered_list[i] );
		FeCache::save_filter( display, m_filtered_list[i], i );
	}

	// Keep the rom_index for each filter in-range by wrapping it
	// - This feature is used by show_random_selection which set a random index before knowing the list size
	for ( int i=0; i<filters_count; i++ )
	{
		int filter_list_count = (int)m_filtered_list[i].filter_list.size();
		display.set_rom_index( i, filter_list_count ? display.get_rom_index( i ) % filter_list_count : 0 );
	}

	FeLog() << " - Loaded filters in "
		<< load_timer.getElapsedTime().asMilliseconds() << " ms ("
		<< filters_count << " filters, "
		<< filters_cached << " from cache, "
		<< m_comparisons << " comparisons"
		<< ")" << std::endl;
}

//
// Save changed favs and tags
//
void FeRomList::save_state()
{
	save_favs();
	save_tags();
}

//
// Save m_list and m_extra fav to file
//
void FeRomList::save_favs()
{
	// Exit early if no changes
	if ( !m_fav_changed || m_romlist_name.empty() )
		return;

	m_fav_changed = false;

	// The list of favs to write
	std::set<std::string> values;

	// Add extra favs
	for ( std::set<std::string>::const_iterator itr = m_extra_favs.begin(); itr != m_extra_favs.end(); ++itr )
		values.insert( *itr );

	// Add m_list favs
	for ( FeRomInfoListType::const_iterator itr = m_list.begin(); itr != m_list.end(); ++itr )
		if ( !(*itr).get_info( FeRomInfo::Favourite ).empty() )
			values.insert( (*itr).get_id() );

	// Delete file if no values
	std::string path = get_fav_path();
	if ( values.empty() )
	{
		delete_file( path );
		return;
	}

	// Write values to file
	nowide::ofstream file( path );
	if ( file.is_open() )
	{
		for ( std::set<std::string>::const_iterator it = values.begin(); it != values.end(); ++it )
			file << (*it) << std::endl;
		file.close();
	}
}

//
// Save m_list and m_extra tags to named tag files
//
void FeRomList::save_tags()
{
	if ( !m_tags_changed || m_romlist_name.empty() )
		return;

	m_tags_changed = false;

	// The list of tags to write
	std::multimap<std::string, std::string> tag_roms;

	// Add extra tags
	for ( std::multimap<std::string, std::string>::const_iterator itr = m_extra_tags.begin(); itr != m_extra_tags.end(); ++itr )
		tag_roms.insert( *itr );

	// Add m_list tags
	for ( FeRomInfoListType::iterator itr = m_list.begin(); itr != m_list.end(); ++itr )
	{
		const std::string id = (*itr).get_id();
		std::set<std::string> tags;
		if ( !(*itr).get_tags( tags ) ) continue;

		for ( std::set<std::string>::iterator it = tags.begin(); it != tags.end(); ++it )
			tag_roms.insert( std::pair( *it, id ) );
	}

	// Ensure tags path exists
	confirm_tag_dir();

	// Save the tags
	std::map<std::string, bool>::const_iterator itt;
	for ( itt = m_tags.begin(); itt != m_tags.end(); ++itt )
	{
		// Exit early if this tag has not been flagged as changed
		if ( (*itt).second == false ) continue;
		std::string tag = (*itt).first;

		// Get values matching the current tag
		std::pair<
			std::multimap<std::string, std::string>::const_iterator,
			std::multimap<std::string, std::string>::const_iterator
		> values = tag_roms.equal_range( tag );

		// Delete file if no values
		std::string path = get_tag_path( tag );
		if ( values.first == values.second )
		{
			delete_file( path );
			return;
		}

		// Write values to file
		nowide::ofstream file( path );
		if ( file.is_open() )
		{
			for ( std::multimap<std::string, std::string>::const_iterator it = values.first; it != values.second; ++it )
				file << (*it).second << std::endl;
			file.close();
		}
	}
}

//
// Set the favourite state for the given rom
// - Returns true if the fav change has triggered a filter rebuild (used by a rule)
//
bool FeRomList::set_fav( FeRomInfo &rom, FeDisplayInfo &display, bool fav )
{
	bool has_fav = !rom.get_info( FeRomInfo::Favourite ).empty();
	if ( has_fav == fav ) return false;

	m_fav_changed = true;
	rom.set_info( FeRomInfo::Favourite, fav ? "1" : "" );
	return fix_filters( display, { FeRomInfo::Favourite } );
}

//
// Return list of all available tags
//
std::vector<std::string> FeRomList::get_tags_available()
{
	std::vector<std::string> tags;
	for (const auto& pair : m_tags) tags.push_back(pair.first);
	return tags;
}

//
// Populate given list with rom tag availability [{ tag, flagged }]
//
void FeRomList::get_tags_list(
	FeRomInfo &rom,
	std::vector<std::pair<std::string, bool>> &tags_list
) const
{
	std::set<std::string> tags;
	rom.get_tags( tags );

	for ( std::map<std::string, bool>::const_iterator itr=m_tags.begin(); itr!=m_tags.end(); ++itr )
		tags_list.push_back( std::pair( (*itr).first, tags.find( (*itr).first ) != tags.end() ) );
}

bool FeRomList::replace_tags( FeRomInfo &rom, FeDisplayInfo &display, const std::string &tags )
{
	size_t pos = 0;
	std::string tag;
	std::set<std::string> current_tags;
	std::set<std::string> new_tags;
	std::set<std::string> add_tags;
	std::set<std::string> rem_tags;

	rom.get_tags( current_tags );

	while ( token_helper( tags, pos, tag ) )
		if ( !tag.empty() )
			new_tags.insert( tag );

	for ( auto &tag : new_tags )
		if ( !current_tags.count( tag ) )
			add_tags.insert( tag );

	for ( auto &tag : current_tags )
		if ( !new_tags.count( tag ) )
			rem_tags.insert( tag );

	if ( !add_tags.size() && !rem_tags.size() )
		return false;

	for ( auto &tag : add_tags )
	{
		rom.append_tag( tag );
		std::map<std::string, bool>::iterator itt = m_tags.find( tag );
		if ( itt != m_tags.end() )
			(*itt).second = true;
		else
			m_tags.insert( std::pair( tag, true ) );
	}

	for ( auto &tag : rem_tags )
	{
		rom.remove_tag( tag );
		std::map<std::string, bool>::iterator itt = m_tags.find( tag );
		if ( itt != m_tags.end() )
			(*itt).second = true;
	}

	m_tags_changed = true;
	return fix_filters( display, { FeRomInfo::Tags } );
}

//
// Add or remove tag from rom
//
bool FeRomList::set_tag( FeRomInfo &rom, FeDisplayInfo &display, const std::string &tag, bool add_tag )
{
	bool found = rom.has_tag( tag );
	bool changed = ( found != add_tag );
	if ( !changed ) return false;

	if ( !found )
		rom.append_tag( tag );
	else
		rom.remove_tag( tag );

	// Update m_tags changed flag (whether added or removed)
	m_tags_changed = true;
	std::map<std::string, bool>::iterator itt = m_tags.find( tag );
	if ( itt != m_tags.end() )
		(*itt).second = true;
	else if ( add_tag )
		m_tags.insert( std::pair( tag, true ) );

	return fix_filters( display, { FeRomInfo::Tags } );
}

//
// Rebuild filters that use the given info-target for sorting or filtering
//
bool FeRomList::fix_filters( FeDisplayInfo &display, std::set<FeRomInfo::Index> targets )
{
	FeCache::invalidate_rominfo( display.get_romlist_name(), targets );

	bool retval = false;
	for ( int i=0; i<display.get_filter_count(); i++ )
	{
		FeFilter *f = display.get_filter( i );
		ASSERT( f );

		if ( f->test_for_targets( targets ) )
		{
			build_single_filter_list( f, m_filtered_list[i] );
			retval = true;
		}
	}

	return retval;
}

//
// Check availability of all roms in m_list
//
void FeRomList::get_file_availability(
	std::map<std::string, std::vector<std::string>> emu_roms
)
{
	if ( m_availability_checked )
		return;

	std::map<std::string, std::vector<FeRomInfo*>> emu_map;

	// Create map of emulator->[rominfo] since m_list may contain multiple emulators
	for ( FeRomInfoListType::iterator itr=m_list.begin(); itr != m_list.end(); ++itr )
		emu_map[ ((*itr).get_info( FeRomInfo::Emulator )) ].push_back( &(*itr) );

	// Check each emulator list
	for ( std::map<std::string,std::vector<FeRomInfo*>>::iterator ite=emu_map.begin(); ite != emu_map.end(); ++ite )
	{
		std::string emu_name = (*ite).first;
		FeEmulatorInfo *emu = get_emulator( emu_name );
		if ( !emu ) continue;

		// Get a list of rom names, re-use given emu_roms if available
		// - emu_roms comes from validate_available check
		// - using it saves a second gather (it will be empty if invalid)
		std::vector<std::string> rom_names;
		if ( emu_roms.find( emu_name ) != emu_roms.end() )
			rom_names = emu_roms[ emu_name ];
		else
		{
			emu->gather_rom_names( rom_names );
			emu_roms.insert( std::pair( emu_name, rom_names ));
		}

		// convert to set for faster find
		std::set<std::string> rom_set( rom_names.begin(), rom_names.end() );

		// For each rom, check if it's in the romname set
		for ( std::vector<FeRomInfo*>::iterator itr=(*ite).second.begin(); itr!=(*ite).second.end(); ++itr )
		{
			(*itr)->set_info(
				FeRomInfo::FileIsAvailable,
				( rom_set.find( (*itr)->get_info( FeRomInfo::Romname ) ) != rom_set.end() ) ? "1" : ""
			);
		}
	}

	// Store roms for future checks
	FeCache::save_available( *this, emu_roms );

	m_availability_checked = true;
}

//
// Load stats into all roms in m_list
// - Force stat loading since globalfilter may have been loaded from cache with stale stats
//
void FeRomList::get_played_stats()
{
	if ( m_played_stats_checked )
		return;

	std::string path = m_config_path + FE_STATS_SUBDIR;

	// Find all unique emulator names
	std::unordered_set<std::string> emu_names;
	for ( auto &rom : m_list )
		emu_names.insert( rom.get_info( FeRomInfo::Emulator ) );

	// Get dir listing of all emulator stats
	std::unordered_map<std::string, std::unordered_set<std::string>> available_stats;
	for ( auto &emu : emu_names )
	{
		std::string emu_dir = path + emu;
		std::unordered_set<std::string> emu_stats;

		if ( directory_exists( emu_dir ) )
		{
			std::vector<std::string> filenames;
			get_basename_from_extension( filenames, emu_dir, FE_STAT_FILE_EXTENSION );
			emu_stats = std::unordered_set<std::string>( filenames.begin(), filenames.end() );
		}

		available_stats.insert({ emu, emu_stats });
	}

	// Load stats for each rom
	for ( auto &rom : m_list )
	{
		std::string emu = rom.get_info( FeRomInfo::Emulator );
		std::string name = rom.get_info( FeRomInfo::Romname );

		// Check file is available to avoid costly file-open attempt in load_stats
		if ( available_stats[emu].count( name ) )
			rom.load_stats( path );
		else
			rom.clear_stats();
	}

	m_played_stats_checked = true;
}

//
// Load stats into rom at given index
//
void FeRomList::load_stats( int filter_idx, int idx )
{
	lookup( filter_idx, idx ).load_stats( m_config_path + FE_STATS_SUBDIR );
}

// -------------------------------------------------------------------------------------

// NOTE: this function is implemented in fe_settings.cpp
bool internal_resolve_config_file(
	const std::string &config_path,
	std::string &result,
	const char *subdir,
	const std::string &name  );


FeEmulatorInfo *FeRomList::get_emulator( const std::string & emu )
{
	if ( emu.empty() )
		return NULL;

	// Check if we already haved loaded the matching emulator object
	//
	for ( std::vector<FeEmulatorInfo>::iterator ite=m_emulators.begin();
			ite != m_emulators.end(); ++ite )
	{
		if ( emu.compare( (*ite).get_info( FeEmulatorInfo::Name ) ) == 0 )
			return &(*ite);
	}

	// Emulator not loaded yet, load it now
	//
	std::string filename;
	if ( internal_resolve_config_file( m_config_path, filename, FE_EMULATOR_SUBDIR, emu + FE_EMULATOR_FILE_EXTENSION ) )
	{
		FeEmulatorInfo new_emu( emu );
		if ( new_emu.load_from_file( filename ) )
		{
			m_emulators.push_back( new_emu );
			return &(m_emulators.back());
		}
	}

	// Could not find emulator config
	return NULL;
}

FeEmulatorInfo *FeRomList::create_emulator( const std::string &emu, const std::string &emu_template )
{
	// If an emulator with the given name already exists we return it
	//
	FeEmulatorInfo *tmp = get_emulator( emu );
	if ( tmp != NULL )
		return tmp;

	//
	// Fill in with default values if there is a "default" emulator
	//
	FeEmulatorInfo new_emu( emu );

	std::string defaults_file;
	if ( !emu_template.empty() )
	{
		defaults_file = m_config_path;
		defaults_file += FE_TEMPLATE_EMULATOR_SUBDIR;
		defaults_file += emu_template;
		defaults_file += FE_EMULATOR_FILE_EXTENSION;

		if ( !new_emu.load_from_file( defaults_file ) )
			FeLog() << "Unable to open file: " << defaults_file << std::endl;
	}
	else if ( internal_resolve_config_file( m_config_path, defaults_file, FE_TEMPLATE_SUBDIR, FE_EMULATOR_DEFAULT ) )
	{
		if ( !new_emu.load_from_file( defaults_file ) )
			FeLog() << "Unable to open file: " << defaults_file << std::endl;

		//
		// Find and replace the [emulator] token, replace with the specified
		// name.  This is only done in the path fields.  It is not done for
		// the FeEmulator::Command field.
		//
		const char *EMU_TOKEN = "[emulator]";

		std::string temp = new_emu.get_info( FeEmulatorInfo::Rom_path );
		if ( perform_substitution( temp, EMU_TOKEN, emu ) )
			new_emu.set_info( FeEmulatorInfo::Rom_path, temp );

		std::vector<std::pair<std::string,std::string> > al;
		std::vector<std::pair<std::string,std::string> >::iterator itr;
		new_emu.get_artwork_list( al );
		for ( itr=al.begin(); itr!=al.end(); ++itr )
		{
			std::string temp = (*itr).second;
			if ( perform_substitution( temp, EMU_TOKEN, emu ) )
			{
				new_emu.delete_artwork( (*itr).first );
				new_emu.add_artwork( (*itr).first, temp );
			}
		}
	}

	m_emulators.push_back( new_emu );
	return &(m_emulators.back());
}

void FeRomList::delete_emulator( const std::string & emu )
{
	//
	// Delete file
	//
	std::string path = m_config_path;
	path += FE_EMULATOR_SUBDIR;
	path += emu;
	path += FE_EMULATOR_FILE_EXTENSION;

	FeLog() << "Deleting emulator: " << path << std::endl;
	delete_file( path );

	//
	// Delete from our list if it has been loaded
	//
	for ( std::vector<FeEmulatorInfo>::iterator ite=m_emulators.begin();
			ite != m_emulators.end(); ++ite )
	{
		if ( emu.compare( (*ite).get_info( FeEmulatorInfo::Name ) ) == 0 )
		{
			m_emulators.erase( ite );
			break;
		}
	}
}

// -------------------------------------------------------------------------------------
// FeFilterIndexes

//
// Converts FeRomInfo pointers to indexes for caching
//
bool FeFilterIndexes::filter_list_to_indexes(
	const std::vector<FeRomInfo*> &filter_list,
	std::vector<int> &indexes
)
{
	indexes.clear();
	indexes.reserve( filter_list.size() );

	for ( std::vector<FeRomInfo*>::const_iterator it=filter_list.begin(); it!=filter_list.end(); ++it)
		indexes.push_back( (*it)->index );

	return true;
}

//
// Restores indexes back to FeRomInfo pointers and inserts into list
// - Returns false if index invalid (occurs when lookup or index is stale)
//
bool FeFilterIndexes::indexes_to_filter_list(
	const std::vector<int> &indexes,
	std::vector<FeRomInfo*> &filter_list,
	const std::map<int, FeRomInfo*> &lookup
)
{
	filter_list.clear();
	filter_list.reserve( indexes.size() );

	try {
		for ( std::vector<int>::const_iterator it=indexes.begin(); it!=indexes.end(); ++it)
			filter_list.push_back( lookup.at( *it ) );

		return true;
	}
	catch ( ... )
	{
		filter_list.clear();
		return false;
	}
}

//
// Converts mapped FeRomInfo pointers to indexes for caching
//
bool FeFilterIndexes::clone_group_to_indexes(
	const std::map<std::string, std::vector<FeRomInfo*>> &clone_group,
	std::map<std::string, std::vector<int>> &indexes
)
{
	indexes.clear();

	for ( std::map<std::string, std::vector<FeRomInfo*>>::const_iterator it=clone_group.begin(); it!=clone_group.end(); ++it)
	{
		indexes[ (*it).first ] = std::vector<int>();
		if ( !filter_list_to_indexes( (*it).second, indexes[ (*it).first ] ) )
		{
			indexes.clear();
			return false;
		}
	}
	return true;
}

//
// Restores mapped indexes back to FeRomInfo pointers and inserts into map
//
bool FeFilterIndexes::indexes_to_clone_group(
	const std::map<std::string, std::vector<int>> &indexes,
	std::map<std::string, std::vector<FeRomInfo*>> &clone_group,
	const std::map<int, FeRomInfo*> &lookup
)
{
	clone_group.clear();

	for ( std::map<std::string, std::vector<int>>::const_iterator it=indexes.begin(); it!=indexes.end(); ++it)
	{
		clone_group[ (*it).first ] = std::vector<FeRomInfo*>();
		if ( !indexes_to_filter_list( (*it).second, clone_group[ (*it).first ], lookup ) )
		{
			clone_group.clear();
			return false;
		}
	}
	return true;
}

// Set indexes from given entry
bool FeFilterIndexes::entry_to_index(
	const FeFilterEntry &entry
)
{
	return filter_list_to_indexes( entry.filter_list, m_filter_list )
		&& clone_group_to_indexes( entry.clone_group, m_clone_group );
}

// Populate given entry from lookup indexes
bool FeFilterIndexes::index_to_entry(
	FeFilterEntry &entry,
	const std::map<int, FeRomInfo*> &lookup
)
{
	return indexes_to_filter_list( m_filter_list, entry.filter_list, lookup )
		&& indexes_to_clone_group( m_clone_group, entry.clone_group, lookup );
}
