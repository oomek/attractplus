#include "fe_cache.hpp"

#define FE_CACHE_BINARY

#ifdef FE_CACHE_BINARY
#include <cereal/archives/binary.hpp>
typedef cereal::BinaryOutputArchive OutputArchive;
typedef cereal::BinaryInputArchive InputArchive;
const char *FE_CACHE_EXT = ".bin";
#else
#include <cereal/archives/json.hpp>
typedef cereal::JSONOutputArchive OutputArchive;
typedef cereal::JSONInputArchive InputArchive;
const char *FE_CACHE_EXT = ".json";
#endif // FE_CACHE_BINARY

const char *FE_CACHE_SUBDIR = "cache/";
const char *FE_CACHE_ROMLIST = ".romlist";
const char *FE_CACHE_FILTER = ".filter";
const std::string FE_EMPTY_STRING;

std::string FeCache::m_config_path;

// -------------------------------------------------------------------------------------

//
// Create path for config files
//
void FeCache::set_config_path( std::string path )
{
	m_config_path = path;
	confirm_directory( m_config_path, FE_CACHE_SUBDIR );
}

//
// Returns sanitized filename for romlist cache
//
std::string FeCache::get_romlist_cache_filename(
	FeDisplayInfo &display
)
{
	std::string name = display.get_name();
	if ( name.empty() ) return FE_EMPTY_STRING;
	return m_config_path
		+ FE_CACHE_SUBDIR
		+ sanitize_filename( name )
		+ FE_CACHE_ROMLIST
		+ FE_CACHE_EXT;
}

//
// Returns sanitized filename for filter cache
//
std::string FeCache::get_filter_cache_filename(
	FeDisplayInfo &display,
	int filter_index
)
{
	std::string name = display.get_name();
	if ( name.empty() ) return FE_EMPTY_STRING;
	return m_config_path
		+ FE_CACHE_SUBDIR
		+ sanitize_filename( name )
		+ FE_CACHE_FILTER
		+ "." + as_str(filter_index)
		+ FE_CACHE_EXT;
}

// -------------------------------------------------------------------------------------

//
// Converts FeRomInfo pointers to indexes for caching
//
void FeCache::filter_list_to_indexes(
	std::vector<int> &indexes,
	std::vector<FeRomInfo*> &list
)
{
	indexes.clear();
	indexes.reserve( list.size() );

	std::transform(
		list.begin(),
		list.end(),
		std::back_inserter( indexes ),
		[]( FeRomInfo* info ) { return info->index; }
	);
}

//
// Converts mapped FeRomInfo pointers to indexes for caching
//
void FeCache::clone_group_to_indexes(
	std::map<std::string, std::vector<int>> &indexes,
	std::map<std::string, std::vector<FeRomInfo*>> &map
)
{
	indexes.clear();

	std::for_each(
		map.begin(),
		map.end(),
		[ &indexes ]( std::pair<std::string, std::vector<FeRomInfo*>> it )
		{
			indexes[ it.first ] = std::vector<int>();
			filter_list_to_indexes( indexes[ it.first ], it.second );
		}
	);
}

//
// Restores indexes back to FeRomInfo pointers and inserts into list
//
void FeCache::indexes_to_filter_list(
	std::vector<FeRomInfo*> &list,
	std::vector<int> &indexes,
	std::vector<FeRomInfo*> &lookup
)
{
	list.clear();
	list.reserve( indexes.size() );

	std::transform(
		indexes.begin(),
		indexes.end(),
		std::back_inserter( list ),
		[ &lookup ]( int index ) { return lookup[index]; }
	);
}

//
// Restores mapped indexes back to FeRomInfo pointers and inserts into map
//
void FeCache::indexes_to_clone_group(
	std::map<std::string, std::vector<FeRomInfo*>> &map,
	std::map<std::string, std::vector<int>> &indexes,
	std::vector<FeRomInfo*> &lookup
)
{
	map.clear();

	std::for_each(
		indexes.begin(),
		indexes.end(),
		[ &map, &lookup ]( std::pair<std::string, std::vector<int>> it )
		{
			map[ it.first ] = std::vector<FeRomInfo*>();
			indexes_to_filter_list( map[ it.first ], it.second, lookup );
		}
	);
}

//
// Returns romlist romInfoList as an indexed vector of pointers
// - Must only be used after romlist filtered, sorted and group_cloned ordered
//
std::vector<FeRomInfo*> FeCache::get_romlist_lookup(
	FeRomInfoListType &m_list
)
{
	std::vector<FeRomInfo*> v;
	v.reserve(m_list.size());
	for ( FeRomInfoListType::iterator it=m_list.begin(); it!=m_list.end(); ++it )
	{
		v.push_back(&(*it));
	}
	return v;
}

// -------------------------------------------------------------------------------------

//
// Clears all cache belonging to the given display (romlist and all filters)
//
void FeCache::clear_display_cache(
	FeDisplayInfo &display
)
{
	clear_romlist_cache( display );

	int filters_count = display.get_filter_count();
	if ( filters_count == 0 ) filters_count = 1;
	for ( int i=0; i<filters_count; i++ ) clear_filter_cache( display, i );
}

//
// Clears cached romlist file
//
void FeCache::clear_romlist_cache(
	FeDisplayInfo &display
)
{
	std::string filename = get_romlist_cache_filename( display );
	if ( file_exists( filename ) ) delete_file( filename );
}

//
// Clears cached filter file
//
void FeCache::clear_filter_cache(
	FeDisplayInfo &display,
	int filter_index
)
{
	std::string filename = get_filter_cache_filename( display, filter_index );
	if ( file_exists( filename ) ) delete_file( filename );
}

//
// Clears cache for all lists that use the given RomInfo target
// - Favourite, Tags, PlayedCount, PlayedTime
//
void FeCache::invalidate(
	FeDisplayInfo &display,
	FeRomInfo::Index target
)
{
	// Check whether the global filter uses the given RomInfo
	FeFilter *global_filter = display.get_global_filter();
	bool romlist_changed = global_filter && global_filter->test_for_target( target );

	// Always clear the romlist, since it stores tags & stats that need updating
	clear_romlist_cache( display );

	int filters_count = display.get_filter_count();
	if ( filters_count == 0 ) filters_count = 1;

	for ( int i=0; i<filters_count; i++ )
	{
		// If the romlist has changed, or the filter uses the given RomInfo, then clear it
		if ( romlist_changed || display.get_filter( i )->test_for_target( target ) )
		{
			clear_filter_cache( display, i );
		}
	}
}

// -------------------------------------------------------------------------------------

//
// Saves romlist to cache file
//
bool FeCache::save_romlist_cache(
	FeDisplayInfo &display,
	FeRomList &romlist
)
{
	std::string filename = get_romlist_cache_filename( display );
	if ( filename.empty() ) return false;
	nowide::ofstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		{	// block flushes archive
			OutputArchive archive( file );
			archive( romlist );
		}
		file.close();
		return true;
	}
	catch (...)
	{
		file.close();
		clear_romlist_cache( display );
		return false;
	}
}

//
// Loads cache file and updates romlist
//
bool FeCache::load_romlist_cache(
	FeDisplayInfo &display,
	FeRomList &romlist
)
{
	std::string filename = get_romlist_cache_filename( display );
	if ( !file_exists( filename ) ) return false;
	nowide::ifstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		{	// block flushes archive
			InputArchive archive( file );
			archive( romlist );
		}
		file.close();
		return true;
	}
	catch ( ... )
	{
		file.close();
		clear_romlist_cache( display );
		return false;
	}
}

// -------------------------------------------------------------------------------------

//
// Saves filter to cache file
//
bool FeCache::save_filter_cache(
	FeDisplayInfo &display,
	FeFilterEntry &entry,
	int filter_index
)
{
	std::string filename = get_filter_cache_filename( display, filter_index );
	if ( filename.empty() ) return false;
	nowide::ofstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		// Update entry indexes, which is what will be cached
		entry.to_indexes();

		{	// block flushes archive
			OutputArchive archive( file );
			archive( entry );
		}
		file.close();
		return true;
	}
	catch (...)
	{
		file.close();
		clear_filter_cache( display, filter_index );
		return false;
	}
}

//
// Loads cache file and updates romlist filter
//
bool FeCache::load_filter_cache(
	FeDisplayInfo &display,
	FeFilterEntry &entry,
	int filter_index,
	std::vector<FeRomInfo*> &lookup
)
{
	std::string filename = get_filter_cache_filename( display, filter_index );
	if ( !file_exists( filename ) ) return false;
	nowide::ifstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		{	// block flushes archive
			InputArchive archive( file );
			archive( entry );
		}

		// Convert entry indexes back into pointers
		entry.from_indexes( lookup );
		file.close();
		return true;
	}
	catch ( ... )
	{
		file.close();
		clear_filter_cache( display, filter_index );
		return false;
	}
}
