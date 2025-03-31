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
const char *FE_CACHE_STATS = "stats";
const char *FE_CACHE_FILTER = "filter";
const char *FE_CACHE_DISPLAY = "display";
const char *FE_CACHE_ROMHASH = "romhash";
const char *FE_CACHE_ROMLIST = "romlist";
const char *FE_CACHE_GLOBALFILTER = "globalfilter";
const std::string FE_EMPTY_STRING;

std::vector<FeDisplayInfo>* FeCache::m_displays = {};
std::string FeCache::m_config_path = "";
std::string FeCache::m_romlist_args = "";

// Global storage for [emulator][romname] stats
std::map<std::string, FeListStats> FeCache::stats_cache = std::map<std::string, FeListStats>{};

// -------------------------------------------------------------------------------------

//
// Create path for config files
//
void FeCache::set_config_path(
	std::string path
)
{
	m_config_path = path;
	confirm_directory( m_config_path, FE_CACHE_SUBDIR );
}

//
// Store the displays lists, since cache sometimes need to invalidate multiple of them
//
void FeCache::set_displays(
	std::vector<FeDisplayInfo>* displays
)
{
	m_displays = displays;
};

// -------------------------------------------------------------------------------------

//
// Returns sanitized filename for romhash cache
//
std::string FeCache::get_romhash_cache_filename(
	const std::string &romlist_name
)
{
	if ( romlist_name.empty() ) return FE_EMPTY_STRING;
	return m_config_path
		+ FE_CACHE_SUBDIR
		+ FE_CACHE_ROMHASH + "."
		+ sanitize_filename( romlist_name )
		+ FE_CACHE_EXT;
}

//
// Returns sanitized filename for romlist cache
//
std::string FeCache::get_romlist_cache_filename(
	const std::string &romlist_name
)
{
	if ( romlist_name.empty() ) return FE_EMPTY_STRING;
	return m_config_path
		+ FE_CACHE_SUBDIR
		+ FE_CACHE_ROMLIST + "."
		+ sanitize_filename( romlist_name )
		+ FE_CACHE_EXT;
}

//
// Returns sanitized filename for globalfilter cache
//
std::string FeCache::get_globalfilter_cache_filename(
	FeDisplayInfo &display
)
{
	std::string name = display.get_name();
	if ( name.empty() ) return FE_EMPTY_STRING;
	return m_config_path
		+ FE_CACHE_SUBDIR
		+ FE_CACHE_DISPLAY + "."
		+ sanitize_filename( name ) + "."
		+ FE_CACHE_GLOBALFILTER
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
		+ FE_CACHE_DISPLAY + "."
		+ sanitize_filename( name ) + "."
		+ FE_CACHE_FILTER + "."
		+ as_str( filter_index )
		+ FE_CACHE_EXT;
}

//
// Returns sanitized filename for stat cache
//
std::string FeCache::get_stats_cache_filename(
	std::string &emulator
)
{
	if ( emulator.empty() ) return FE_EMPTY_STRING;
	return m_config_path
		+ FE_CACHE_SUBDIR
		+ FE_CACHE_STATS + "."
		+ sanitize_filename( emulator )
		+ FE_CACHE_EXT;
}

// -------------------------------------------------------------------------------------

//
// Clears all cache using the given romlist (displays > globalfilters and filters)
//
void FeCache::invalidate_romlist(
	const std::string &romlist_name
)
{
	std::string filename = get_romhash_cache_filename( romlist_name );
	if ( file_exists( filename ) ) delete_file( filename );

	for (std::vector<FeDisplayInfo>::const_iterator itr=(*m_displays).begin(); itr!=(*m_displays).end(); ++itr)
	{
		FeDisplayInfo display = (*itr);
		if ( display.get_romlist_name() == romlist_name ) invalidate_display( display );
	}
}

//
// Clears all cache belonging to the given display (globalfilter and filters)
//
void FeCache::invalidate_display(
	FeDisplayInfo &display
)
{
	invalidate_globalfilter( display );
	int filters_count = display.get_filter_count();
	if ( filters_count == 0 ) filters_count = 1;
	for ( int i=0; i<filters_count; i++ ) invalidate_filter( display, i );
}

//
// Clears a cached globalfilter file
//
void FeCache::invalidate_globalfilter(
	FeDisplayInfo &display
)
{
	invalidate_romlist_args();
	std::string filename = get_globalfilter_cache_filename( display );
	if ( file_exists( filename ) ) delete_file( filename );
}

//
// Clears a cached filter file
//
void FeCache::invalidate_filter(
	FeDisplayInfo &display,
	int filter_index
)
{
	invalidate_romlist_args();
	std::string filename = get_filter_cache_filename( display, filter_index );
	if ( file_exists( filename ) ) delete_file( filename );
}

//
// Clears cache for all displays using the given romlist, and a rule with a changed rominfo value
// - Favourite, Tags, PlayedCount, PlayedTime
//
void FeCache::invalidate_rominfo(
	const std::string &romlist_name,
	FeRomInfo::Index target
)
{
	for (std::vector<FeDisplayInfo>::const_iterator itr=(*m_displays).begin(); itr!=(*m_displays).end(); ++itr)
	{
		FeDisplayInfo display = (*itr);
		if ( display.get_romlist_name() == romlist_name )
		{
			// Clear the globalfilter if it uses the target
			FeFilter *global_filter = display.get_global_filter();
			bool changed = global_filter && global_filter->test_for_target( target );
			if ( changed ) invalidate_globalfilter( display );

			int filters_count = display.get_filter_count();

			// If no filters there's still a single cache file containing entire romlist
			if ( changed && filters_count == 0 ) invalidate_filter( display, 0 );

			// Clear filters that are using the target, or if the romlist has changed
			for ( int i=0; i<filters_count; i++ )
			{
				if ( changed || display.get_filter( i )->test_for_target( target ) )
				{
					invalidate_filter( display, i );
				}
			}
		}
	}
}

// -------------------------------------------------------------------------------------

//
// Saves romhash to cache file
//
bool FeCache::save_romhash_cache(
	const std::string &romlist_name,
	std::set<std::string> &emu_set,
	size_t &hash
)
{
	std::string filename = get_romhash_cache_filename( romlist_name );
	if ( filename.empty() ) return false;
	nowide::ofstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		{	// block flushes archive
			OutputArchive archive( file );
			archive( emu_set, hash );
		}
		file.close();
		return true;
	}
	catch (...)
	{
		file.close();
		return false;
	}
}

//
// Loads romhash file
//
bool FeCache::load_romhash_cache(
	const std::string &romlist_name,
	std::set<std::string> &emu_set,
	size_t &hash
)
{
	std::string filename = get_romhash_cache_filename( romlist_name );
	if ( !file_exists( filename ) ) return false;
	nowide::ifstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		{	// block flushes archive
			InputArchive archive( file );
			archive( emu_set, hash );
		}
		file.close();
		return true;
	}
	catch ( ... )
	{
		file.close();
		return false;
	}
}

// -------------------------------------------------------------------------------------

//
// Saves romlist to cache file
//
bool FeCache::save_romlist_cache(
	const std::string &romlist_name,
	FeRomList &romlist
)
{
	std::string filename = get_romlist_cache_filename( romlist_name );
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
		invalidate_romlist( romlist_name );
		return false;
	}
}

//
// Loads romlist file and updates romlist
//
bool FeCache::load_romlist_cache(
	const std::string &romlist_name,
	FeRomList &romlist
)
{
	std::string filename = get_romlist_cache_filename( romlist_name );
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
		invalidate_romlist( romlist_name );
		return false;
	}
}

// -------------------------------------------------------------------------------------

//
// Saves globalfilter to cache file
//
bool FeCache::save_globalfilter_cache(
	FeDisplayInfo &display,
	FeRomList &romlist
)
{
	std::string filename = get_globalfilter_cache_filename( display );
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
		invalidate_globalfilter( display );
		return false;
	}
}

//
// Loads globalfilter file and updates romlist
//
bool FeCache::load_globalfilter_cache(
	FeDisplayInfo &display,
	FeRomList &romlist
)
{
	std::string filename = get_globalfilter_cache_filename( display );
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
		invalidate_globalfilter( display );
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
		// Create indexes, which is what gets cached
		FeFilterIndexes indexes;
		indexes.entry_to_index( entry );
		{	// block flushes archive
			OutputArchive archive( file );
			archive( indexes );
		}
		file.close();
		return true;
	}
	catch (...)
	{
		file.close();
		invalidate_filter( display, filter_index );
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
	std::map<int, FeRomInfo*> &lookup
)
{
	std::string filename = get_filter_cache_filename( display, filter_index );
	if ( !file_exists( filename ) ) return false;
	nowide::ifstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		FeFilterIndexes indexes;
		{	// block flushes archive
			InputArchive archive( file );
			archive( indexes );
		}
		// Restore indexes back into entries
		indexes.index_to_entry( entry, lookup );
		file.close();
		return true;
	}
	catch ( ... )
	{
		file.close();
		invalidate_filter( display, filter_index );
		return false;
	}
}

// -------------------------------------------------------------------------------------

//
// Saves stats to cache file
//
bool FeCache::save_stats_cache(
	std::string &emulator
)
{
	if ( FeCache::stats_cache.find(emulator) == FeCache::stats_cache.end() ) return false;
	std::string filename = get_stats_cache_filename( emulator );
	if ( filename.empty() ) return false;
	nowide::ofstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		{	// block flushes archive
			OutputArchive archive( file );
			archive( FeCache::stats_cache[emulator] );
		}
		file.close();
		return true;
	}
	catch (...)
	{
		file.close();
		return false;
	}
}

//
// Loads cache file and updates stats
//
bool FeCache::load_stats_cache(
	std::string &emulator
)
{
	std::string filename = get_stats_cache_filename( emulator );
	if ( !file_exists( filename ) ) return false;
	nowide::ifstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		{	// block flushes archive
			InputArchive archive( file );
			archive( FeCache::stats_cache[emulator] );
		}
		file.close();
		return true;
	}
	catch ( ... )
	{
		file.close();
		FeCache::stats_cache[emulator] = {};
		return false;
	}
	return false;
}

// -------------------------------------------------------------------------------------

//
// Ensure stats cache entry exists for emulator
// - If not, read all stats files and create one
//
bool FeCache::confirm_stats_cache(
	const std::string &path,
	std::string &emulator
) {
	// Exit if already loaded
	if ( FeCache::stats_cache.find(emulator) != FeCache::stats_cache.end() ) return true;

	// Exit if successfully loaded from cache
	if ( load_stats_cache( emulator ) ) return true;

	// Create new stats cache objectfor emulator
	FeCache::stats_cache[emulator] = FeListStats();
	std::vector<std::string> file_list;
	std::string stats_path = path + emulator + "/";

	// Exit if no stats to load
	if ( !get_basename_from_extension( file_list, stats_path, FE_STAT_FILE_EXTENSION, true ) ) return false;

	// Parse all stat files into cache object
	std::string played_count;
	std::string played_time;
	for ( std::vector<std::string>::iterator itr=file_list.begin(); itr != file_list.end(); ++itr )
	{
		nowide::ifstream stat_file( (stats_path + *itr + FE_STAT_FILE_EXTENSION).c_str() );
		if ( !stat_file.is_open() ) break;
		played_count = "0";
		played_time = "0";
		if ( stat_file.good() ) getline( stat_file, played_count );
		if ( stat_file.good() ) getline( stat_file, played_time );
		FeCache::stats_cache[emulator].list[*itr] = FeStat( played_count, played_time );
		stat_file.close();
	}

	// Save new stats cache for next time
	save_stats_cache(emulator);
	return true;
}

//
// Load stats from cache to rominfo
// - Always returns true to prevent callee attempting to load stats that dont exist
//
bool FeCache::get_stats_info(
	const std::string &path,
	std::vector<std::string> &rominfo
) {
	std::string emulator = rominfo[FeRomInfo::Emulator];
	std::string romname = rominfo[FeRomInfo::Romname];

	// Exit if no stats for this emulator, or this rom
	if ( !confirm_stats_cache( path, emulator ) ) return true;
	if ( FeCache::stats_cache[emulator].list.find(romname) == FeCache::stats_cache[emulator].list.end() ) return true;

	// Copy stats to rominfo object
	rominfo[FeRomInfo::PlayedCount] = FeCache::stats_cache[emulator].list[romname].played_count;
	rominfo[FeRomInfo::PlayedTime] = FeCache::stats_cache[emulator].list[romname].played_time;
	return true;
}

//
// Save stats from rominfo to cache
//
bool FeCache::set_stats_info(
	const std::string &path,
	std::vector<std::string> &rominfo
) {
	std::string emulator = rominfo[FeRomInfo::Emulator];
	std::string romname = rominfo[FeRomInfo::Romname];
	std::string played_count = rominfo[FeRomInfo::PlayedCount];
	std::string played_time = rominfo[FeRomInfo::PlayedTime];

	// Exit if error creating cache for stat
	if ( !confirm_stats_cache( path, emulator ) ) return true;

	// Update stats in cache object, and save it
	FeCache::stats_cache[emulator].list[romname] = FeStat( played_count, played_time );
	save_stats_cache( emulator );
	return true;
}

// -------------------------------------------------------------------------------------

//
// Store id for args used to load current romlist
// - Returns true if changed, indicating the romlist should be reloaded
//
bool FeCache::set_romlist_args(
	const std::string &path,
	const std::string &romlist_name,
	FeDisplayInfo &display,
	bool group_clones,
	bool load_stats
) {
	std::string args = path
		+ ";" + display.get_name()
		+ ";" + romlist_name
		+ ";" + ( group_clones ? "1" : "0" )
		+ ";" + ( load_stats ? "1" : "0" );
	bool changed = m_romlist_args != args;
	m_romlist_args = args;
	return changed;
}

//
// Clear stored args id, forcing a reload on next load_romlist
//
void FeCache::invalidate_romlist_args()
{
	m_romlist_args = "";
}
