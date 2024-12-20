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
const char *FE_CACHE_FILTERS = ".filters";
const std::string FE_EMPTY_STRING;

//
// Returns cache base dir
//
std::string get_cache_dir( const std::string config_path )
{
	return config_path + FE_CACHE_SUBDIR;
}

//
// Returns sanitized filename for romlist cache
//
std::string get_display_cache_filename(
	const std::string config_path,
	FeDisplayInfo &display
)
{
	std::string name = display.get_name();
	if ( name.empty() ) return FE_EMPTY_STRING;
	return get_cache_dir( config_path )
		+ sanitize_filename( name )
		+ FE_CACHE_ROMLIST
		+ FE_CACHE_EXT;
}

//
// Returns sanitized filename for filter cache
//
std::string get_filters_cache_filename(
	const std::string config_path,
	FeDisplayInfo &display
)
{
	std::string name = display.get_name();
	if ( name.empty() ) return FE_EMPTY_STRING;
	return get_cache_dir( config_path )
		+ sanitize_filename( name )
		+ FE_CACHE_FILTERS
		+ FE_CACHE_EXT;
}

//
// Returns romlist m_list as a vector containing pointers
//
std::vector<FeRomInfo*> get_romlist_lookup(
	FeRomList &romlist
)
{
	FeRomInfoListType &m_list = romlist.get_list();
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

// -------------------------------------------------------------------------------------

//
// Saves romlist data to cache file
//
bool FeCache::save_display_cache(
	const std::string config_path,
	FeDisplayInfo &display,
	FeRomList &romlist
)
{
	std::string filename = get_display_cache_filename( config_path, display );
	if ( filename.empty() ) return false;
	confirm_directory( config_path, FE_CACHE_SUBDIR );
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
		clear_display_cache( config_path, display );
		return false;
	}
}

//
// Loads cache file and updates romlist data
//
bool FeCache::load_display_cache(
	const std::string config_path,
	FeDisplayInfo &display,
	FeRomList &romlist
)
{
	std::string filename = get_display_cache_filename( config_path, display );
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
		clear_display_cache( config_path, display );
		return false;
	}
}

//
// Clears cached display as well as its filters
// - Used when the Display settings have changed
// - Used when the Global Filter results have changed (due to Favourites, PlayedTime, PlayedCount changing)
//
bool FeCache::clear_display_cache(
	const std::string config_path,
	FeDisplayInfo &display
)
{
	std::string filename = get_display_cache_filename( config_path, display );
	bool exists = file_exists( filename );
	if ( exists ) delete_file( filename );
	return clear_filters_cache( config_path, display ) || exists;
}

// -------------------------------------------------------------------------------------

//
// Saves filter data cache file
//
bool FeCache::save_filters_cache(
	const std::string config_path,
	FeDisplayInfo &display,
	FeRomList &romlist
)
{
	std::string filename = get_filters_cache_filename( config_path, display );
	if ( filename.empty() ) return false;
	confirm_directory( config_path, FE_CACHE_SUBDIR );
	nowide::ofstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		// Update entry indexes, which is what will be cached
		std::vector<FeFilterEntry> &filtered_list = romlist.get_filtered_list();
		std::for_each(
			filtered_list.begin(),
			filtered_list.end(),
			[]( FeFilterEntry &it ) { it.to_indexes(); }
		);

		{	// block flushes archive
			OutputArchive archive( file );
			archive( filtered_list );
		}
		file.close();
		return true;
	}
	catch (...)
	{
		file.close();
		clear_filters_cache( config_path, display );
		return false;
	}
}

//
// Loads filter cache and updates romlist filter data
//
bool FeCache::load_filters_cache(
	const std::string config_path,
	FeDisplayInfo &display,
	FeRomList &romlist
)
{
	std::string filename = get_filters_cache_filename( config_path, display );
	if ( !file_exists( filename ) ) return false;
	nowide::ifstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		std::vector<FeFilterEntry> &filtered_list = romlist.get_filtered_list();
		{	// block flushes archive
			InputArchive archive( file );
			archive( filtered_list );
		}

		// Convert entry indexes back into pointers
		std::vector<FeRomInfo*> v = get_romlist_lookup( romlist );
		std::for_each(
			filtered_list.begin(),
			filtered_list.end(),
			[ &v ]( FeFilterEntry &it ) { it.from_indexes( v ); }
		);
		file.close();
		return true;
	}
	catch ( ... )
	{
		file.close();
		clear_filters_cache( config_path, display );
		return false;
	}
}

//
// Clears cached filter
//
bool FeCache::clear_filters_cache(
	const std::string config_path,
	FeDisplayInfo &display
)
{
	std::string filename = get_filters_cache_filename( config_path, display );
	bool exists = file_exists( filename );
	if ( exists ) delete_file( filename );
	return exists;
}

// -------------------------------------------------------------------------------------

//
// Clears cache for all items that use the given RomInfo target
//
bool FeCache::fix_cache(
	const std::string config_path,
	FeDisplayInfo &display,
	FeRomInfo::Index target
)
{
	FeFilter *global_filter = display.get_global_filter();
	if ( global_filter && ( global_filter->test_for_target( target ) ) )
	{
		return clear_display_cache( config_path, display );
	}

	for ( int i=0; i<display.get_filter_count(); i++ )
	{
		if ( display.get_filter( i )->test_for_target( target ) )
		{
			return clear_filters_cache( config_path, display );
		}
	}

	return false;
}
