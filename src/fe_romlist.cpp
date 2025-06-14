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

#include <squirrel.h>
#include <sqstdstring.h>

#include <SFML/System/Clock.hpp>

const char *FE_ROMLIST_FILE_EXTENSION	= ".txt";
const char *FE_FAVOURITE_FILE_EXTENSION = ".tag";

const char *FE_ROMLIST_SUBDIR	= "romlists/";
const char *FE_STATS_SUBDIR                     = "stats/";

SQRex *FeRomListSorter::m_rex = NULL;

namespace
{
	bool fe_not_clone( const FeRomInfo &r )
	{
		return r.get_info( FeRomInfo::Cloneof ).empty();
	}
};

void FeRomListSorter::init_title_rex( const std::string &re_mask )
{
	ASSERT( m_rex == NULL );

	if ( re_mask.empty() )
		return;

	const SQChar *err( NULL );
	m_rex = sqstd_rex_compile( scsqchar( re_mask ), &err );

	if ( !m_rex )
		FeLog() << "Error compiling regular expression \""
			<< re_mask << "\": " << err << std::endl;
}

void FeRomListSorter::clear_title_rex()
{
	if ( m_rex )
		sqstd_rex_free( m_rex );

	m_rex = NULL;
}

FeRomListSorter::FeRomListSorter( FeRomInfo::Index c, bool rev )
	: m_comp( c ),
	m_reverse( rev )
{
}

// Returns a new string without prefixes such as "The" and "Vs."
std::string FeRomListSorter::get_trimmed_title( const std::string &title ) const
{
	SQRexMatch subexp;
	return (
		!title.empty()
		&& m_rex
		&& sqstd_rex_match( m_rex, scsqchar( title ) )
		&& sqstd_rex_getsubexp( m_rex, 1, &subexp )
	)
		? scstdstr( subexp.begin )
		: title;
}

bool FeRomListSorter::operator()( const FeRomInfo &one_obj, const FeRomInfo &two_obj ) const
{
	const std::string &one = one_obj.get_info( m_comp );
	const std::string &two = two_obj.get_info( m_comp );
	bool asc;

	if ( m_comp == FeRomInfo::Title )
		// Title sort
		asc = icompare( get_trimmed_title( one ), get_trimmed_title( two ) ) < 0;
	else if (( m_comp == FeRomInfo::PlayedCount ) || ( m_comp == FeRomInfo::PlayedTime ))
		// Numeric sort
		asc = as_int( one ) < as_int( two );
	else
		// String sort
		asc = icompare( one, two ) < 0;

	// If reverse_order then invert sorting
	return m_reverse ? !asc : asc;
}

// Returns first character of the trimmed lowercase rom title, or '0' if none
const char FeRomListSorter::get_first_letter( const FeRomInfo *one_info )
{
	if ( !one_info ) return '0';
	const std::string &title = get_trimmed_title( one_info->get_info( FeRomInfo::Title ) );
	return title.empty() ? '0' : std::tolower( title.at( 0 ) );
}

FeRomList::FeRomList( const std::string &config_path )
	: m_config_path( config_path ),
	m_fav_changed( false ),
	m_tags_changed( false ),
	m_availability_checked( false ),
	m_played_stats_checked( false ),
	m_group_clones( false )
{
}

FeRomList::~FeRomList()
{
}

void FeRomList::init_as_empty_list()
{
	m_romlist_name.clear();
	m_list.clear();
	m_filtered_list.clear();
	m_filtered_list.push_back( FeFilterEntry()  ); // there always has to be at least one filter
	m_tags.clear();
	m_availability_checked = false;
	m_played_stats_checked = false;
	m_group_clones = false;
	m_fav_changed=false;
	m_tags_changed=false;

	m_extra_favs.clear();
	m_extra_tags.clear();
}

void FeRomList::mark_favs_and_tags_changed()
{
	m_fav_changed=true;
	m_tags_changed=true;
}

//
// Returns true if FileIsAvailable is used by global or display filters
//
bool FeRomList::has_file_availability( FeDisplayInfo &display ) {
	FeFilter *global_filter = display.get_global_filter();
	bool has_global_rules = global_filter && (global_filter->get_rule_count() > 0);
	if ( has_global_rules && global_filter->test_for_target( FeRomInfo::FileIsAvailable ) ) return true;

	int filters_count = display.get_filter_count();
	for ( int i=0; i<filters_count; i++ )
		if ( display.get_filter( i )->test_for_target( FeRomInfo::FileIsAvailable ) ) return true;

	return false;
}

//
// Returns a simple romlist hash used to detect changes
//
size_t FeRomList::get_romlist_hash( std::set<std::string> emu_set )
{
	// Find all unique romlist paths
	std::set<std::string> paths;
	for ( std::set<std::string>::iterator ite=emu_set.begin(); ite != emu_set.end(); ++ite )
	{
		FeEmulatorInfo *emu = get_emulator( *ite );
		if ( !emu ) continue;

		std::vector<std::string> emu_paths = emu->get_paths();
		for ( std::vector<std::string>::iterator itn=emu_paths.begin(); itn != emu_paths.end(); ++itn )
			paths.insert( emu->clean_path_with_wd( *itn, true ) );
	}

	// Return a hash of path subdirs and filenames
	return get_path_content_hash( paths );
}

bool FeRomList::load_romlist( const std::string &path,
	const std::string &romlist_name,
	FeDisplayInfo &display,
	bool group_clones,
	bool load_stats	)
{
	sf::Clock load_timer;
	std::string romlist_path = path + romlist_name + FE_ROMLIST_FILE_EXTENSION;
	time_t mtime = file_mtime( romlist_path );
	FeFilter *global_filter = display.get_global_filter();
	bool has_global_rules = global_filter && (global_filter->get_rule_count() > 0);

	// Reset all properties here in case cache load returns early
	m_romlist_name = romlist_name;
	m_fav_changed = false;
	m_tags_changed = false;
	m_availability_checked = false;
	m_played_stats_checked = !load_stats;

	int global_filtered_out_count = 0;
	bool loaded_globalfilter_cache = false;
	bool saved_globalfilter_cache = false;
	bool loaded_romlist_cache = false;
	bool saved_romlist_cache = false;
	bool success = false;

	// If any filter uses FileIsAvailable then check the romlist hash
	if ( has_file_availability( display ) )
	{
		std::set<std::string> emu_set;
		size_t prev_romlist_hash = 0;
		FeCache::load_romhash_cache( romlist_name, emu_set, prev_romlist_hash );
		size_t next_romlist_hash = get_romlist_hash( emu_set );

		if ( next_romlist_hash != prev_romlist_hash ) {
			FeCache::save_romhash_cache( romlist_name, emu_set, next_romlist_hash );
			FeCache::invalidate_rominfo( romlist_name, FeRomInfo::FileIsAvailable );
		}
	}

	// Attempt to load globalfilter cache
	if ( has_global_rules && FeCache::load_globalfilter_cache( display, *this ) )
	{
		// If globalfilter cache is not valid then invalidate all data for this display
		success = ( m_group_clones == group_clones ) && ( m_modified_time == mtime );
		if ( !success ) FeCache::invalidate_display( display );
		loaded_globalfilter_cache = success;
	}

	// If invalid globalfilter we need to load the romlist to build a new one
	if ( !has_global_rules || !loaded_globalfilter_cache ) {

		// Attempt to load romlist cache
		if ( FeCache::load_romlist_cache( m_romlist_name, *this ) )
		{
			// If romlist cache is not valid then invalidate all data for this romlist
			success = ( m_group_clones == group_clones ) && ( m_modified_time == mtime );
			if ( !success ) FeCache::invalidate_romlist( m_romlist_name );
			loaded_romlist_cache = success;
		}

		// Romlist cache has NOT been loaded
		if ( !loaded_romlist_cache )
		{
			m_group_clones = group_clones;
			m_modified_time = mtime;
			m_list.clear();

			// Loaded romlist from the original csv (slow)
			success = FeBaseConfigurable::load_from_file( romlist_path, ";" );

			// Init index value to assist list lookup mapping for saving
			int index = 0;
			for ( FeRomInfoListType::iterator it=m_list.begin(); it!=m_list.end(); ++it, index++ )
				(*it).index = index;

			// Save romlist cache to improve speed for builds using this romlist
			saved_romlist_cache = FeCache::save_romlist_cache( m_romlist_name, *this );
		}
	}

	//
	// Create rom name to romlist entry lookup map
	//
	std::map < std::string, FeRomInfo * > rom_map;
	std::map < std::string, FeRomInfo * >::iterator rm_itr;
	for ( FeRomInfoListType::iterator itr = m_list.begin(); itr != m_list.end(); ++itr )
		rom_map[ (*itr).get_info( FeRomInfo::Romname ) ] = &(*itr);

	//
	// Load favourites
	// - Unused favourites will go into m_extra_favs so they can be saved
	//
	m_extra_favs.clear();
	std::string load_name( m_config_path + FE_ROMLIST_SUBDIR + m_romlist_name + FE_FAVOURITE_FILE_EXTENSION );
	nowide::ifstream myfile( load_name.c_str() );

	if ( myfile.is_open() )
	{
		while ( myfile.good() )
		{
			size_t pos=0;
			std::string line, name;

			getline( myfile, line );
			token_helper( line, pos, name );

			if ( !name.empty() )
			{
				rm_itr = rom_map.find( name );
				if ( rm_itr != rom_map.end() )
					(*rm_itr).second->set_info( FeRomInfo::Favourite, "1" );
				else
					m_extra_favs.insert( name );
			}
		}

		myfile.close();
	}

	//
	// Load tags
	// - Unused tags will go into m_extra_tags so they can be saved
	//
	m_tags.clear();
	m_extra_tags.clear();
	load_name = m_config_path + FE_ROMLIST_SUBDIR + m_romlist_name + "/";

	if ( directory_exists( load_name ) )
	{
		std::vector<std::string> temp_tags;
		get_basename_from_extension( temp_tags, load_name, FE_FAVOURITE_FILE_EXTENSION );

		for ( std::vector<std::string>::iterator itr=temp_tags.begin(); itr!=temp_tags.end(); ++itr )
		{
			if ( (*itr).empty() )
				continue;

			nowide::ifstream myfile( std::string(load_name + (*itr) + FE_FAVOURITE_FILE_EXTENSION).c_str() );

			if ( !myfile.is_open() )
				continue;

			std::map<std::string, bool>::iterator itt = m_tags.begin();
			itt = m_tags.insert( itt, std::pair<std::string,bool>( (*itr), false ) );

			while ( myfile.good() )
			{
				size_t pos=0;
				std::string line, rname;

				getline( myfile, line );
				token_helper( line, pos, rname );

				if ( !rname.empty() )
				{
					rm_itr = rom_map.find( rname );
					if ( rm_itr != rom_map.end() )
						(*rm_itr).second->append_tag( (*itt).first.c_str() );
					else
						m_extra_tags.insert( std::pair<std::string,const char*>(rname, (*itt).first.c_str() ) );
				}
			}

			myfile.close();
		}
	}

	//
	// Apply global filter (skip if already loaded from cache)
	//
	if ( has_global_rules && !loaded_globalfilter_cache )
	{
		global_filter->init();
		std::set<std::string> emu_set;

		if ( global_filter->test_for_target( FeRomInfo::FileIsAvailable ) )
			get_file_availability();

		if ( global_filter->test_for_target( FeRomInfo::PlayedCount )
			|| global_filter->test_for_target( FeRomInfo::PlayedTime ) )
			get_played_stats();

		FeRomInfoListType::iterator last_it=m_list.begin();
		for ( FeRomInfoListType::iterator it=m_list.begin(); it!=m_list.end(); )
		{
			// Store a list of all emulators used in the romlist, since there may be multiple
			emu_set.insert( (*it).get_info( FeRomInfo::Emulator ) );

			if ( global_filter->apply_filter( *it ) )
			{
				if ( last_it != it )
					it = m_list.erase( last_it, it );
				else
					++it;

				last_it = it;
			}
			else
			{
				// This rom is being filtered out

				// Unused favourites will go into m_extra_favs so they can be saved (same as Load favourites above)
				if ( !(*it).get_info( FeRomInfo::Favourite ).empty() )
					m_extra_favs.insert( (*it).get_info( FeRomInfo::Romname ) );

				// Unused tags will go into m_extra_tags so they can be saved (same as Load tags above)
				if ( !(*it).get_info( FeRomInfo::Tags ).empty() )
				{
					const std::string &name = (*it).get_info( FeRomInfo::Romname );
					const std::string &tags = (*it).get_info( FeRomInfo::Tags );
					const char sep[] = { FE_TAGS_SEP, 0 };

					size_t pos=0;
					while ( pos < tags.size() )
					{
						std::string one_tag;
						token_helper( tags, pos, one_tag, sep );

						std::map<std::string, bool>::iterator itt = m_tags.find( one_tag );
						if ( itt != m_tags.end() )
							m_extra_tags.insert( std::pair<std::string,const char *>( name, (*itt).first.c_str() ) );
					}
				}

				global_filtered_out_count++;
				++it;
			}
		}

		if ( last_it != m_list.end() )
			m_list.erase( last_it, m_list.end() );

		// If grouping by clones partition list so clones are at the end.
		if ( m_group_clones ) std::stable_partition( m_list.begin(), m_list.end(), fe_not_clone );

		// Save globalfilter for next time
		saved_globalfilter_cache = FeCache::save_globalfilter_cache( display, *this );

		// Save hash for FileIsAvailable check on next load
		size_t next_romlist_hash = get_romlist_hash( emu_set );
		FeCache::save_romhash_cache( romlist_name, emu_set, next_romlist_hash );
	}

	FeLog() << " - Loaded romlist '" << m_romlist_name << "' in " << load_timer.getElapsedTime().asMilliseconds() << " ms (";
	if ( !has_global_rules && !loaded_romlist_cache ) FeLog() << m_list.size() << " entries from romlist";
	if ( loaded_romlist_cache ) FeLog() << m_list.size() << " entries from romlist cache";
	if ( has_global_rules && !loaded_globalfilter_cache ) FeLog() << m_list.size() << " kept, " << global_filtered_out_count << " discarded";
	if ( loaded_globalfilter_cache ) FeLog() << m_list.size() << " entries from globalfilter cache";
	if ( saved_romlist_cache ) FeLog() << ", cached romlist";
	if ( saved_globalfilter_cache ) FeLog() << ", cached globalfilter";
	FeLog() << ")" << std::endl;

	create_filters( display );
	return success;
}

void FeRomList::build_single_filter_list( FeFilter *f,
	FeFilterEntry &result )
{
	if ( f )
	{
		if ( f->get_size() > 0 ) // if this is non zero then we've loaded before and know how many to expect
			result.filter_list.reserve( f->get_size() );

		if ( f->test_for_target( FeRomInfo::FileIsAvailable ) )
			get_file_availability();

		if ( f->test_for_target( FeRomInfo::PlayedCount )
				|| f->test_for_target( FeRomInfo::PlayedTime ) )
			get_played_stats();

		f->init();

		for ( FeRomInfoListType::iterator itr=m_list.begin(); itr!=m_list.end(); ++itr )
		{
			if ( f->apply_filter( *itr ) )
			{
				if ( m_group_clones )
				{
					std::string id = (*itr).get_info( FeRomInfo::Cloneof );
					if ( id.empty() )
						id = (*itr).get_info( FeRomInfo::Romname );

					std::map<std::string,std::vector<FeRomInfo *> >::iterator it;
					it = result.clone_group.find( id );

					if ( it == result.clone_group.end() )
					{
						result.filter_list.push_back( &( *itr ) );

						it = result.clone_group.insert( it,
							std::pair< std::string, std::vector < FeRomInfo * > >(
								id,
								std::vector<FeRomInfo *>() ) );
					}
					(*it).second.push_back( &( *itr ) );
				}
				else
					result.filter_list.push_back( &( *itr ) );
			}
		}
	}
	else // no filter situation, so we just add the entire list...
	{
		result.filter_list.reserve( m_list.size() );
		if ( m_group_clones )
		{
			std::string last_name, this_name;
			for ( FeRomInfoListType::iterator itr=m_list.begin(); itr!=m_list.end(); ++itr )
			{
				std::string id = (*itr).get_info( FeRomInfo::Cloneof );
				if ( id.empty() )
					id = (*itr).get_info( FeRomInfo::Romname );

				std::map<std::string,std::vector<FeRomInfo *> >::iterator it;
				it = result.clone_group.find( id );

				if ( it == result.clone_group.end() )
				{
					result.filter_list.push_back( &( *itr ) );
					it = result.clone_group.insert( it,
						std::pair< std::string, std::vector < FeRomInfo * > >(
							id,
							std::vector<FeRomInfo *>() ) );
				}
				(*it).second.push_back( &( *itr ) );
			}
		}
		else
		{
			for ( FeRomInfoListType::iterator itr=m_list.begin(); itr!=m_list.end(); ++itr )
				result.filter_list.push_back( &( *itr ) );
		}
	}

	if ( f )
	{
		// track the size of the filtered list in our filter info object
		f->set_size( result.filter_list.size() );

		//
		// Sort and/or prune now if configured for this filter
		//
		FeRomInfo::Index sort_by=f->get_sort_by();
		bool rev = f->get_reverse_order();
		int list_limit = f->get_list_limit();

		if ( sort_by != FeRomInfo::LAST_INDEX )
		{
			if (( sort_by == FeRomInfo::PlayedCount )
					|| ( sort_by == FeRomInfo::PlayedTime ))
				get_played_stats();

			std::stable_sort( result.filter_list.begin(),
				result.filter_list.end(),
				FeRomListSorter2( sort_by, rev ) );

			std::map< std::string, std::vector < FeRomInfo * > >::iterator itg;
			for ( itg= result.clone_group.begin(); itg != result.clone_group.end(); ++itg )
			{
				std::stable_sort( (*itg).second.begin(), (*itg).second.end(),
					FeRomListSorter2( sort_by, rev ) );
			}
		}
		else if ( rev != false )
			std::reverse( result.filter_list.begin(), result.filter_list.end() );

		if (( list_limit != 0 ) && ( (int)result.filter_list.size() > abs( list_limit ) ))
		{
			if ( list_limit > 0 )
				result.filter_list.erase( result.filter_list.begin() + list_limit, result.filter_list.end() );
			else
				result.filter_list.erase( result.filter_list.begin(), result.filter_list.end() + list_limit );
		}
	}
}

void FeRomList::get_clone_group( int filter_idx, int idx, std::vector < FeRomInfo * > &group )
{
	FeRomInfo &ri = lookup( filter_idx, idx );

	std::string id;
	id = ri.get_info( FeRomInfo::Cloneof );

	if ( id.empty() )
		id = ri.get_info( FeRomInfo::Romname );

	std::map< std::string, std::vector < FeRomInfo * > >::iterator it;

	it = m_filtered_list[filter_idx].clone_group.find( id );
	if ( it != m_filtered_list[filter_idx].clone_group.end() )
	{
		std::vector<FeRomInfo *>::iterator itr;

		for ( itr= (*it).second.begin(); itr != (*it).second.end(); ++itr )
			group.push_back( (*itr) );
	}
	else
		group.push_back( &ri );
}

void FeRomList::create_filters(
	FeDisplayInfo &display )
{
	sf::Clock load_timer;

	// Prepare an indexed lookup for faster filter cache loading
	std::map<int, FeRomInfo*> lookup;
	for ( FeRomInfoListType::iterator it=m_list.begin(); it!=m_list.end(); ++it )
		lookup[it->index] = &(*it);

	int filters_cached = 0;

	// If no filters configured create a single filter containing entire romlist
	int filters_count = display.get_filter_count();
	int display_filters_count = filters_count;
	if ( filters_count == 0 ) filters_count = 1;

	// Apply filters
	m_filtered_list.clear();
	m_filtered_list.reserve( filters_count );
	for ( int i=0; i<filters_count; i++ )
	{
		m_filtered_list.push_back( FeFilterEntry() );

		// Attempt to load filter from cache
		if ( FeCache::load_filter_cache( display, m_filtered_list[i], i, lookup ) )
		{
			if (i < display_filters_count)
				display.get_filter( i )->set_size( m_filtered_list[i].filter_list.size() );
			filters_cached++;
			continue;
		}

		// If no cache, build and save the filter from scratch
		build_single_filter_list( display.get_filter( i ), m_filtered_list[i] );
		FeCache::save_filter_cache( display, m_filtered_list[i], i );
	}

	FeLog() << " - Loaded filters in "
		<< load_timer.getElapsedTime().asMilliseconds() << " ms ("
		<< filters_count << " filters, "
		<< filters_cached << " from cache, "
		<< ((filters_count - filters_cached) * m_list.size()) << " comparisons"
		<< ")" << std::endl;
}

int FeRomList::process_setting( const std::string &setting,
				const std::string &value,
				const std::string &fn )
{
	FeRomInfo next_rom( setting );
	next_rom.process_setting( setting, value, fn );
	m_list.push_back( next_rom );

	return 0;
}

void FeRomList::save_state()
{
	save_favs();
	save_tags();
}

void FeRomList::save_favs()
{
	if (( !m_fav_changed ) || ( m_romlist_name.empty() ))
		return;

	//
	// First gather all the favourites from the current list into our extra favs list
	//
	for ( FeRomInfoListType::const_iterator itr = m_list.begin(); itr != m_list.end(); ++itr )
	{
		if ( !((*itr).get_info( FeRomInfo::Favourite ).empty()) )
			m_extra_favs.insert( (*itr).get_info( FeRomInfo::Romname ) );
	}

	//
	// Now save the contents of favs list
	//
	std::string fname = m_config_path + FE_ROMLIST_SUBDIR + m_romlist_name + FE_FAVOURITE_FILE_EXTENSION;

	if ( m_extra_favs.empty() )
	{
		delete_file( fname );
	}
	else
	{
		nowide::ofstream outfile( fname.c_str() );
		if ( !outfile.is_open() )
			return;

		for ( std::set<std::string>::const_iterator itf = m_extra_favs.begin(); itf != m_extra_favs.end(); ++itf )
			outfile << (*itf) << std::endl;

		outfile.close();
	}
}

void FeRomList::save_tags()
{
	if (( !m_tags_changed ) || ( m_romlist_name.empty() ))
		return;

	// First construct a mapping of tags to rom entries
	//
	std::multimap< std::string, const char * > tag_to_rom_map;

	for ( std::multimap< std::string, const char * >::const_iterator ite = m_extra_tags.begin(); ite != m_extra_tags.end(); ++ite )
	{
		tag_to_rom_map.insert(
				std::pair<std::string,const char *>(
						(*ite).second,
						(*ite).first.c_str() ) );
	}

	for ( FeRomInfoListType::const_iterator itr = m_list.begin(); itr != m_list.end(); ++itr )
	{
		const std::string &my_tags = (*itr).get_info( FeRomInfo::Tags );

		size_t pos=0;
		do
		{
			std::string one_tag;
			const char sep[] = { FE_TAGS_SEP, 0 };
			token_helper( my_tags, pos, one_tag, sep );

			if ( !one_tag.empty() )
			{
				tag_to_rom_map.insert(
						std::pair<std::string,const char *>(
								one_tag,
								((*itr).get_info( FeRomInfo::Romname )).c_str() ) );
			}
		} while ( pos < my_tags.size() );
	}

	std::string user_path = m_config_path + FE_ROMLIST_SUBDIR;
	confirm_directory( user_path, m_romlist_name );
	std::string my_path = user_path + m_romlist_name + "/";

	//
	// Now save the tags
	//
	std::map<std::string,bool>::const_iterator itt;
	for ( itt = m_tags.begin(); itt != m_tags.end(); ++itt )
	{
		if ( (*itt).second == false )
			continue;

		std::string file_name = my_path + (*itt).first + FE_FAVOURITE_FILE_EXTENSION;

		std::pair<std::multimap<std::string,const char *>::const_iterator,std::multimap<std::string,const char *>::const_iterator> ret;
		ret = tag_to_rom_map.equal_range( (*itt).first );
		if ( ret.first == ret.second )
		{
			delete_file( file_name );
		}
		else
		{
			nowide::ofstream outfile( file_name.c_str() );
			if ( !outfile.is_open() )
				continue;

			for ( std::multimap<std::string,const char *>::const_iterator ito = ret.first; ito != ret.second; ++ito )
				outfile << (*ito).second << std::endl;

			outfile.close();
		}
	}
}

bool FeRomList::set_fav( FeRomInfo &r, FeDisplayInfo &display, bool fav )
{
	r.set_info( FeRomInfo::Favourite, fav ? "1" : "" );
	m_fav_changed=true;

	FeCache::invalidate_rominfo( display.get_romlist_name(), FeRomInfo::Favourite );
	return fix_filters( display, FeRomInfo::Favourite );
}

void FeRomList::get_tags_list( FeRomInfo &rom,
		std::vector< std::pair<std::string, bool> > &tags_list ) const
{
	std::string curr_tags = rom.get_info(FeRomInfo::Tags);

	std::set<std::string> my_set;
	size_t pos=0;
	do
	{
		std::string one_tag;
		const char sep[] = { FE_TAGS_SEP, 0 };
		token_helper( curr_tags, pos, one_tag, sep );
		if ( !one_tag.empty() )
		{
			my_set.insert( one_tag );
		}
	} while ( pos < curr_tags.size() );

	for ( std::map<std::string, bool>::const_iterator itr=m_tags.begin(); itr!=m_tags.end(); ++itr )
	{
		tags_list.push_back(
				std::pair<std::string, bool>((*itr).first,
						( my_set.find( (*itr).first ) != my_set.end() ) ) );
	}
}

bool FeRomList::set_tag( FeRomInfo &rom, FeDisplayInfo &display, const std::string &tag, bool flag )
{
	std::string curr_tags = rom.get_info(FeRomInfo::Tags);
	size_t pos = curr_tags.find( FE_TAGS_SEP + tag + FE_TAGS_SEP );

	std::map<std::string, bool>::iterator itt = m_tags.begin();

	if ( flag == true )
	{
		if ( pos == std::string::npos )
		{
			rom.append_tag( tag );
			m_tags_changed = true;

			itt = m_tags.find( tag );
			if ( itt != m_tags.end() )
				(*itt).second = true;
			else
				itt = m_tags.insert( itt, std::pair<std::string,bool>( tag, true ) );
		}
	}
	else if ( pos != std::string::npos )
	{
		pos++; // because we searched for the preceeding FE_TAGS_SEP as well
		int len = tag.size();

		if (( ( pos + len ) < curr_tags.size() )
				&& ( curr_tags.at( pos + len ) == FE_TAGS_SEP ))
			len++;

		curr_tags.erase( pos, len );

		//
		// Clean up our leading FE_TAGS_SEP if there are no tags left
		//
		if (( curr_tags.size() == 1 ) && (curr_tags[0] == FE_TAGS_SEP ))
			curr_tags.clear();

		rom.set_info( FeRomInfo::Tags, curr_tags );
		m_tags_changed = true;

		itt = m_tags.find( tag );
		if ( itt != m_tags.end() )
			(*itt).second = true;
		else
			itt = m_tags.insert( itt, std::pair<std::string,bool>( tag, true ) );
	}

	FeCache::invalidate_rominfo( display.get_romlist_name(), FeRomInfo::Tags );
	return fix_filters( display, FeRomInfo::Tags );
}

bool FeRomList::fix_filters( FeDisplayInfo &display, FeRomInfo::Index target )
{
	bool retval = false;
	for ( int i=0; i<display.get_filter_count(); i++ )
	{
		FeFilter *f = display.get_filter( i );
		ASSERT( f );

		if ( f->test_for_target( target ) )
		{
			m_filtered_list[i].clear();
			build_single_filter_list( f, m_filtered_list[i] );
			retval = true;
		}
	}

	return retval;
}

void FeRomList::get_file_availability()
{
	if ( m_availability_checked )
		return;

	m_availability_checked = true;

	std::map<std::string,std::vector<FeRomInfo *> > emu_map;

	for ( FeRomInfoListType::iterator itr=m_list.begin(); itr != m_list.end(); ++itr )
		emu_map[ ((*itr).get_info( FeRomInfo::Emulator )) ].push_back( &(*itr) );

	// figure out what roms we have for each emulator
	for ( std::map<std::string,std::vector<FeRomInfo *> >::iterator ite=emu_map.begin();
					ite != emu_map.end(); ++ite )
	{
		FeEmulatorInfo *emu = get_emulator( (*ite).first );
		if ( emu )
		{
			std::vector<std::string> name_vector;
			emu->gather_rom_names( name_vector );

			std::set<std::string> name_set;
			for ( std::vector<std::string>::iterator itv=name_vector.begin(); itv!=name_vector.end(); ++itv )
				name_set.insert( *itv );

			for ( std::vector<FeRomInfo *>::iterator itp=(*ite).second.begin(); itp!=(*ite).second.end(); ++itp )
			{
				if ( name_set.find( (*itp)->get_info( FeRomInfo::Romname ) ) != name_set.end() )
					(*itp)->set_info( FeRomInfo::FileIsAvailable, "1" );
			}
		}
	}
}

void FeRomList::get_played_stats()
{
	if ( m_played_stats_checked )
		return;

	for ( FeRomInfoListType::iterator itr=m_list.begin(); itr != m_list.end(); ++itr )
		(*itr).load_stats( m_config_path + FE_STATS_SUBDIR );

	m_played_stats_checked = true;
}

void FeRomList::load_stats( int filter_idx, int idx )
{
	lookup( filter_idx, idx ).load_stats( m_config_path + FE_STATS_SUBDIR );
}

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
		defaults_file += FE_EMULATOR_TEMPLATES_SUBDIR;
		defaults_file += emu_template;
		defaults_file += FE_EMULATOR_FILE_EXTENSION;

		if ( !new_emu.load_from_file( defaults_file ) )
			FeLog() << "Unable to open file: " << defaults_file << std::endl;
	}
	else if ( internal_resolve_config_file( m_config_path, defaults_file, NULL, FE_EMULATOR_DEFAULT ) )
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
void FeFilterIndexes::filter_list_to_indexes(
	std::vector<FeRomInfo*> &filter_list,
	std::vector<int> &indexes
)
{
	indexes.clear();
	indexes.reserve( filter_list.size() );

	std::transform(
		filter_list.begin(),
		filter_list.end(),
		std::back_inserter( indexes ),
		[]( FeRomInfo* info ) { return info->index; }
	);
}

//
// Converts mapped FeRomInfo pointers to indexes for caching
//
void FeFilterIndexes::clone_group_to_indexes(
	std::map<std::string, std::vector<FeRomInfo*>> &clone_group,
	std::map<std::string, std::vector<int>> &indexes
)
{
	indexes.clear();

	std::for_each(
		clone_group.begin(),
		clone_group.end(),
		[ this, &indexes ]( std::pair<std::string, std::vector<FeRomInfo*>> it )
		{
			indexes[ it.first ] = std::vector<int>();
			filter_list_to_indexes( it.second, indexes[ it.first ] );
		}
	);
}

//
// Restores indexes back to FeRomInfo pointers and inserts into list
//
void FeFilterIndexes::indexes_to_filter_list(
	std::vector<int> &indexes,
	std::vector<FeRomInfo*> &filter_list,
	std::map<int, FeRomInfo*> &lookup
)
{
	filter_list.clear();
	filter_list.reserve( indexes.size() );

	std::transform(
		indexes.begin(),
		indexes.end(),
		std::back_inserter( filter_list ),
		[ &lookup ]( int index ) { return lookup[index]; }
	);
}

//
// Restores mapped indexes back to FeRomInfo pointers and inserts into map
//
void FeFilterIndexes::indexes_to_clone_group(
	std::map<std::string, std::vector<int>> &indexes,
	std::map<std::string, std::vector<FeRomInfo*>> &clone_group,
	std::map<int, FeRomInfo*> &lookup
)
{
	clone_group.clear();

	std::for_each(
		indexes.begin(),
		indexes.end(),
		[ this, &clone_group, &lookup ]( std::pair<std::string, std::vector<int>> it )
		{
			clone_group[ it.first ] = std::vector<FeRomInfo*>();
			indexes_to_filter_list( it.second, clone_group[ it.first ], lookup );
		}
	);
}

// Set indexes from given entry
void FeFilterIndexes::entry_to_index(
	FeFilterEntry &entry
)
{
	filter_list_to_indexes( entry.filter_list, filter_list );
	clone_group_to_indexes( entry.clone_group, clone_group );
}

// Populate given entry from lookup indexes
void FeFilterIndexes::index_to_entry(
	FeFilterEntry &entry,
	std::map<int, FeRomInfo*> &lookup
)
{
	indexes_to_filter_list( filter_list, entry.filter_list, lookup );
	indexes_to_clone_group( clone_group, entry.clone_group, lookup );
}
