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

#ifndef FE_ROMLIST_HPP
#define FE_ROMLIST_HPP

#include "fe_info.hpp"

#include <map>
#include <set>
#include <list>
#include <regex>

#include "cereal/cereal.hpp"
#include <cereal/types/list.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/polymorphic.hpp>

typedef std::list<FeRomInfo> FeRomInfoListType;
extern const char *FE_ROMLIST_FILE_EXTENSION;
extern const char *FE_ROMLIST_SUBDIR;
extern const char *FE_STATS_SUBDIR;

//
// Comparison used when sorting/merging FeRomLists
//
class FeRomListSorter
{
private:
	FeRomInfo::Index m_comp;
	bool m_reverse;
	static std::string get_formatted_title( const std::string &title );

public:
	FeRomListSorter( FeRomInfo::Index c = FeRomInfo::Title, bool rev=false );
	bool operator()( const FeRomInfo &obj1, const FeRomInfo &obj2 ) const;
};

//
// Identical to FeRomListSorter, but accepts FeRomInfo pointers rather than references
//
class FeRomListSorter2
{
private:
	FeRomListSorter m_sorter;

public:
	FeRomListSorter2( FeRomInfo::Index c = FeRomInfo::Title, bool rev=false ) : m_sorter( c, rev ) {};
	bool operator()( const FeRomInfo *one, const FeRomInfo *two ) const { return m_sorter.operator()(*one,*two); };
};

class FeFilterEntry
{
public:
	// Stores a pointer to the m_list entries
	std::vector<FeRomInfo*> filter_list;
	// If clone grouping is on, this stores each clone groups pointers
	std::map<std::string, std::vector<FeRomInfo*>> clone_group;

	void clear() {
		filter_list.clear();
		clone_group.clear();
	};
};

// Helper class for saving FeFilterEntry
// - Converts FeRomInfo pointers to indexes and back
class FeFilterIndexes
{
private:
	std::vector<int> m_filter_list;
	std::map<std::string, std::vector<int>> m_clone_group;
	int m_size;
	std::string m_filter_id;

	bool filter_list_to_indexes(
		const std::vector<FeRomInfo*> &filter_list,
		std::vector<int> &indexes
	);

	bool indexes_to_filter_list(
		const std::vector<int> &indexes,
		std::vector<FeRomInfo*> &filter_list,
		const std::map<int, FeRomInfo*> &lookup
	);

	bool clone_group_to_indexes(
		const std::map<std::string, std::vector<FeRomInfo*>> &clone_group,
		std::map<std::string, std::vector<int>> &indexes
	);

	bool indexes_to_clone_group(
		const std::map<std::string, std::vector<int>> &indexes,
		std::map<std::string, std::vector<FeRomInfo*>> &clone_group,
		const std::map<int, FeRomInfo*> &lookup
	);

public:
	FeFilterIndexes() {}
	FeFilterIndexes( FeFilterEntry &entry );

	void clear() {
		m_filter_list.clear();
		m_clone_group.clear();
	};

	void set_size(int s) { m_size = s; }
	const int get_size() const { return m_size; }

	void set_filter_id( std::string id ) { m_filter_id = id; }
	const std::string get_filter_id() const { return m_filter_id; }

	bool entry_to_index(
		const FeFilterEntry &entry
	);

	bool index_to_entry(
		FeFilterEntry &entry,
		const std::map<int, FeRomInfo*> &lookup
	);

	template<class Archive>
	void serialize( Archive &archive, std::uint32_t const version )
	{
		if ( version != FE_CACHE_VERSION ) throw "Invalid FeFilterIndexes cache";
		archive( m_filter_list, m_clone_group, m_size, m_filter_id );
	}
};

CEREAL_CLASS_VERSION( FeFilterIndexes, FE_CACHE_VERSION );

class FeRomList : public FeBaseConfigurable
{
private:
	FeRomInfoListType m_list; // this is where we keep the info on all the games available for the current display
	std::vector< FeFilterEntry > m_filtered_list;
	std::vector<FeEmulatorInfo> m_emulators; // we keep the emulator info here because we need it for checking file availability

	std::map<std::string, bool> m_tags; // bool is flag of whether the tag has been changed
	std::set<std::string> m_extra_favs; // store for all favourites that are filtered out by global filter
	std::multimap<std::string, std::string> m_extra_tags; // store for tag:name that are filtered out by global filter
	FeFilter *m_global_filter_ptr; // this will only get set if we are globally filtering out games during the initial load

	std::string m_romlist_path;
	std::string m_romlist_name;
	const std::string &m_config_path;
	bool m_fav_changed;
	bool m_tags_changed;
	bool m_availability_checked;
	bool m_played_stats_checked;
	bool m_group_clones;
	int m_comparisons; // for keeping stats during load

	FeRomList( const FeRomList & );
	FeRomList &operator=( const FeRomList & );

	// helper function for building a single filter's list.  Used by create_filters() and fix_filters()
	//
	void build_single_filter_list( FeFilter *f, FeFilterEntry &result );
	void build_filter_entry( FeFilter *f, FeFilterEntry &result );
	void sort_filter_entry( FeFilter *f, FeFilterEntry &result );
	inline void add_group_entry(
		FeRomInfo &rom,
		FeFilterEntry &result
	);

	bool confirm_tag_dir();
	void save_favs();
	void save_tags();

	enum RomlistResponse
	{
		Loaded_None 	= 1 << 0, // Failed to load romlist or cache
		Loaded_Global 	= 1 << 1, // Loaded globalfilter cache
		Loaded_Romlist 	= 1 << 2, // Loaded romlist cache
		Loaded_File 	= 1 << 3, // Loaded romlist from raw text file
		Saved_Global	= 1 << 4, // Saved globalfilter cache
		Saved_Romlist	= 1 << 5, // Saved romlist cache
		Filtered_Global	= 1 << 6  // Applied globalfilter rules
	};

	void get_romlist_map( std::map<std::string, std::vector<FeRomInfo*>> &rom_map );
	int load_romlist_data( FeDisplayInfo &display );
	void load_fav_data( std::map<std::string, std::vector<FeRomInfo*>> &rom_map );
	void load_tag_data( std::map<std::string, std::vector<FeRomInfo*>> &rom_map );
	void load_shuffle_data();
	int apply_global_filter( FeDisplayInfo &display );
	void store_extra_tags( FeRomInfo &rom );

public:
	FeRomList( const std::string &config_path );
	~FeRomList();

	void init_as_empty_list();

	std::string get_fav_path();
	std::vector<std::string> get_tag_names();
	std::string get_tag_dir();
	std::string get_tag_path( std::string tag );

	bool load_romlist( const std::string &romlist_path,
		const std::string &romlist_name,
		FeDisplayInfo &display,
		bool group_clones,
		bool load_stats );

	void create_filters( FeDisplayInfo &display ); // called by load_romlist()

	int process_setting( const std::string &setting,
		const std::string &value,
		const std::string &fn );

	void mark_favs_and_tags_changed();
	void save_state();
	bool set_fav( FeRomInfo &rom, FeDisplayInfo &display, bool fav );

	std::vector<std::string> get_tags_available();
	void get_tags_list( FeRomInfo &rom, std::vector< std::pair<std::string, bool> > &tags_list ) const;
	bool set_tag( FeRomInfo &rom, FeDisplayInfo &display, const std::string &tag, bool flag );
	bool replace_tags( FeRomInfo &rom, FeDisplayInfo &display, const std::string &tags );

	int filter_size( int filter_idx ) const { return ( filter_idx < (int)m_filtered_list.size() ) ? (int)m_filtered_list[filter_idx].filter_list.size() : 0; };

	// Return rom directly by index
	const FeRomInfo &lookup( int filter_idx, int idx) const { return *(m_filtered_list[filter_idx].filter_list[idx]); };
	FeRomInfo &lookup( int filter_idx, int idx) { return *(m_filtered_list[filter_idx].filter_list[idx]); };

	void get_clone_group( int filter_idx, int idx, std::vector < FeRomInfo * > &group );

	FeRomInfoListType &get_list() { return m_list; };

	void get_file_availability( std::map<std::string, std::vector<std::string>> emu_roms = {} );
	void get_played_stats();

	void load_stats( int filter_idx, int idx );

	// Fixes m_filtered_list as needed using the filters in the given "display", with the
	// assumption that the specified "target" attribute for all games might have been changed
	//
	// returns true if list changes might have been made
	//
	bool fix_filters( FeDisplayInfo &display, std::set<FeRomInfo::Index> targets );

	const std::string get_romlist_path() const { return m_romlist_path; }
	const std::string get_romlist_name() const { return m_romlist_name; }
	FeEmulatorInfo *get_emulator( const std::string & );
	FeEmulatorInfo *create_emulator( const std::string &, const std::string & );
	void delete_emulator( const std::string & );
	void clear_emulators() { m_emulators.clear(); }

	template<class Archive>
	void serialize( Archive &archive, std::uint32_t const version )
	{
		if ( version != FE_CACHE_VERSION ) throw "Invalid FeRomList cache";
		archive( m_list );
	}
};

CEREAL_CLASS_VERSION( FeRomList, FE_CACHE_VERSION );

#endif
