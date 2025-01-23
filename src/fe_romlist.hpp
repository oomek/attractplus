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

#ifndef FE_VERSION_NUM
#define FE_VERSION_NUM 1
#endif

#include "fe_info.hpp"

#include <map>
#include <set>
#include <list>

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
	static SQRex *m_rex;

public:
	FeRomListSorter( FeRomInfo::Index c = FeRomInfo::Title, bool rev=false );

	bool operator()( const FeRomInfo &obj1, const FeRomInfo &obj2 ) const;

	const char get_first_letter( const FeRomInfo *one );

	static void init_title_rex( const std::string & );
	static void clear_title_rex();
};

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
private:
	// Stores indexes to the m_list entries, populated by to_indexes
	std::vector<int> filter_list_indexes;

	// If clone grouping is on, this stores each clone groups indexes, populated by to_indexes
	std::map<std::string, std::vector<int>> clone_group_indexes;

public:
	// Stores a pointer to the m_list entries
	std::vector<FeRomInfo*> filter_list;
	// If clone grouping is on, this stores each clone groups pointers
	std::map<std::string, std::vector<FeRomInfo*>> clone_group;

	void clear() {
		filter_list.clear();
		clone_group.clear();
		filter_list_indexes.clear();
		clone_group_indexes.clear();
	};

	void to_indexes();
	void from_indexes( std::vector<FeRomInfo*> &m_list );

	template<class Archive>
	void serialize( Archive &archive, std::uint32_t const version )
	{
		if ( version != FE_VERSION_NUM ) throw "Invalid FeRomList cache";
		archive( filter_list_indexes, clone_group_indexes );
	}
};

CEREAL_CLASS_VERSION( FeFilterEntry, FE_VERSION_NUM );

class FeRomList : public FeBaseConfigurable
{
private:
	FeRomInfoListType m_list; // this is where we keep the info on all the games available for the current display
	std::vector< FeFilterEntry > m_filtered_list;
	std::vector<FeEmulatorInfo> m_emulators; // we keep the emulator info here because we need it for checking file availability

	std::map<std::string, bool> m_tags; // bool is flag of whether the tag has been changed
	std::set<std::string> m_extra_favs; // store for favourites that are filtered out by global filter
	std::multimap<std::string, std::string> m_extra_tags; // store for tags that are filtered out by global filter
	FeFilter *m_global_filter_ptr; // this will only get set if we are globally filtering out games during the initial load

	std::string m_romlist_name;
	const std::string &m_config_path;
	bool m_fav_changed;
	bool m_tags_changed;
	bool m_availability_checked;
	bool m_played_stats_checked;
	bool m_group_clones;
	int m_global_filtered_out_count; // for keeping stats during load
	time_t m_modified_time;

	FeRomList( const FeRomList & );
	FeRomList &operator=( const FeRomList & );

	// helper function for building a single filter's list.  Used by create_filters() and fix_filters()
	//
	void build_single_filter_list( FeFilter *f, FeFilterEntry &result );

	void get_played_stats();

	void save_favs();
	void save_tags();

public:
	FeRomList( const std::string &config_path );
	~FeRomList();

	void init_as_empty_list();

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

	void get_tags_list( FeRomInfo &rom,
		std::vector< std::pair<std::string, bool> > &tags_list ) const;
	bool set_tag( FeRomInfo &rom, FeDisplayInfo &display, const std::string &tag, bool flag );

	bool is_filter_empty( int filter_idx ) const { return m_filtered_list[filter_idx].filter_list.empty(); };
	int filter_size( int filter_idx ) const { return (int)m_filtered_list[filter_idx].filter_list.size(); };
	const FeRomInfo &lookup( int filter_idx, int idx) const { return *(m_filtered_list[filter_idx].filter_list[idx]); };
	FeRomInfo &lookup( int filter_idx, int idx) { return *(m_filtered_list[filter_idx].filter_list[idx]); };

	void get_clone_group( int filter_idx, int idx, std::vector < FeRomInfo * > &group );

	FeRomInfoListType &get_list() { return m_list; };

	void get_file_availability();

	void load_stats( int filter_idx, int idx );

	// Fixes m_filtered_list as needed using the filters in the given "display", with the
	// assumption that the specified "target" attribute for all games might have been changed
	//
	// returns true if list changes might have been made
	//
	bool fix_filters( FeDisplayInfo &display, FeRomInfo::Index target );


	FeEmulatorInfo *get_emulator( const std::string & );
	FeEmulatorInfo *create_emulator( const std::string &, const std::string & );
	void delete_emulator( const std::string & );
	void clear_emulators() { m_emulators.clear(); }

	template<class Archive>
	void serialize( Archive &archive, std::uint32_t const version )
	{
		if ( version != FE_VERSION_NUM ) throw "Invalid FeRomList cache";
		archive( m_list, m_tags, m_extra_tags, m_extra_favs, m_group_clones, m_modified_time );
	}
};

CEREAL_CLASS_VERSION( FeRomList, FE_VERSION_NUM );

#endif
