#include "fe_cache.hpp"

#include <cereal/archives/binary.hpp>

const char *FE_CACHE_SUBDIR = "cache/";

//
// Converts pointers to indexes for caching
//
std::vector<int> FeCache::get_filter_list_indexes(
	std::vector<FeRomInfo*> &list,
	FeRomInfoListType &m_list
)
{
	FeRomInfoListType::iterator a = m_list.begin();
	FeRomInfoListType::iterator b = m_list.end();
	std::vector<int> indexes;
	std::transform(
		list.begin(),
		list.end(),
		std::back_inserter( indexes ),
		[ &a, &b ]( FeRomInfo* it ) { return std::distance( a, std::find( a, b, *it ) ); }
	);
	return indexes;
}

//
// Converts mapped pointers to indexes for caching
//
std::map<std::string, std::vector<int>> FeCache::get_clone_group_indexes(
	std::map<std::string, std::vector<FeRomInfo*>> &map,
	FeRomInfoListType &m_list
)
{
	std::map<std::string, std::vector<int>> indexes;
	for (
		std::map<std::string, std::vector<FeRomInfo*>>::iterator it = map.begin();
		it != map.end();
		++it
	)
	{
		indexes[it->first] = get_filter_list_indexes(it->second, m_list);
	}
	return indexes;
}

/**
 * Converts indexes to pointers, and inserts into list
 */
void FeCache::insert_filter_list_indexes(
	std::vector<int> &indexes,
	std::vector<FeRomInfo*> &list,
	FeRomInfoListType &m_list
)
{
	std::vector<FeRomInfo*> filtered_list;
	std::transform(
		indexes.begin(),
		indexes.end(),
		std::back_inserter( list ),
		[ &m_list ]( int index )
		{
			FeRomInfoListType::iterator it = m_list.begin();
			std::advance( it, index );
			return &( *it );
		}
	);
}

/**
 * Converts mapped indexes to pointers, and inserts into map
 */
void FeCache::insert_clone_group_indexes(
	std::map<std::string, std::vector<int>> &indexes,
	std::map<std::string, std::vector<FeRomInfo*>> &map,
	FeRomInfoListType &m_list
)
{
	for (
		std::map<std::string, std::vector<int>>::iterator it = indexes.begin();
		it != indexes.end();
		++it
	)
	{
		map[it->first] = std::vector<FeRomInfo*>();
		insert_filter_list_indexes(it->second, map[it->first], m_list);
	}
}

/**
 * Converts all filters to lookups for caching
 */
std::vector<FeFilterLookup> filters_to_lookups(FeRomList &romlist)
{
	std::vector<FeFilterLookup> lookups;
	std::vector<FeFilterEntry> filtered_list = romlist.get_filtered_list();
	std::transform(
		filtered_list.begin(),
		filtered_list.end(),
		std::back_inserter( lookups ),
		[ &romlist ]( FeFilterEntry entry ) { return entry.to_lookup( romlist ); }
	);
	return lookups;
}

/**
 * Converts all lookups to filters for cache loading
 */
void lookups_to_filters( std::vector<FeFilterLookup> &lookups, FeRomList &romlist )
{
	std::vector<FeFilterEntry> &filtered_list = romlist.get_filtered_list();
	std::transform(
		lookups.begin(),
		lookups.end(),
		std::back_inserter( filtered_list ),
		[ &romlist ]( FeFilterLookup lookup )
		{
			FeFilterEntry entry = FeFilterEntry();
			entry.from_lookup( lookup, romlist );
			return entry;
		}
	);
}

// -------------------------------------------------------------------------------------

//
// Returns sanitized filename for romlist cache
//
std::string get_cached_romlist_filename( FeRomList &romlist )
{
	return FE_CACHE_SUBDIR
		+ sanitize_filename( romlist.get_display().get_name() )
		+ ".romlist.bin";
}

//
// Saves romlist data to cache file
//
bool FeCache::save_cached_romlist( FeRomList &romlist )
{
	std::string filename = get_cached_romlist_filename( romlist );
	confirm_directory( romlist.get_config_path(), FE_CACHE_SUBDIR );
	nowide::ofstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		{
			// extra block needed to flush scope
			cereal::BinaryOutputArchive archive( file );
			romlist.serialize( archive );
		}
		file.close();
		return true;
	}
	catch (...)
	{
		file.close();
		delete_file( filename );
		return false;
	}
}

//
// Loads cache file and updates romlist data
//
bool FeCache::load_cached_romlist( FeRomList &romlist )
{
	std::string filename = get_cached_romlist_filename( romlist );
	if ( !file_exists( filename ) ) return false;
	nowide::ifstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		{
			// extra block needed to flush scope
			cereal::BinaryInputArchive archive( file );
			romlist.serialize( archive );
		}
		file.close();
		return true;
	}
	catch ( ... )
	{
		file.close();
		delete_file( filename );
		return false;
	}
}

// -------------------------------------------------------------------------------------

//
// Returns sanitized filename for filter cache
//
std::string get_cached_filters_filename( FeRomList &romlist )
{
	return FE_CACHE_SUBDIR
		+ sanitize_filename( romlist.get_display().get_name() )
		+ ".filters.bin";
}

//
// Saves filter data to cache file
//
bool FeCache::save_cached_filters( FeRomList &romlist )
{
	std::string filename = get_cached_filters_filename( romlist );
	confirm_directory( romlist.get_config_path(), FE_CACHE_SUBDIR );
	nowide::ofstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		{
			// extra block needed to flush scope
			cereal::BinaryOutputArchive archive( file );
			archive( filters_to_lookups ( romlist ) );
		}
		file.close();
		return true;
	}
	catch (...)
	{
		file.close();
		delete_file( filename );
		return false;
	}
	return false;
}

//
// Loads cache file and updates filter data
//
bool FeCache::load_cached_filters( FeRomList &romlist )
{
	std::string filename = get_cached_filters_filename( romlist );
	if ( !file_exists( filename ) ) return false;
	nowide::ifstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		{
			// extra block needed to flush scope
			std::vector<FeFilterLookup> lookups;
			cereal::BinaryInputArchive archive( file );
			archive( lookups );
			lookups_to_filters( lookups, romlist );
		}
		file.close();
		return true;
	}
	catch ( ... )
	{
		file.close();
		delete_file( filename );
		return false;
	}
	return false;
}
