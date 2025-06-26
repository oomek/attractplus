#include "fe_cache.hpp"

// Enable the romlist cache
// - disabling prevents all caching
#define FE_CACHE_ENABLE

// Enable cache debug logging
// - outputs using FeLog
// #define FE_CACHE_DEBUG

// Save cache as binary files
// - smaller and faster than JSON, but not easily readable
#define FE_CACHE_BINARY

// -------------------------------------------------------------------------------------

/*
	Events that invalidate the cache:

	- Configure
		- Display > Global Filter (invalidate_display)
		- Display > Filter (invalidate_filter)
		- General > Group Clones (invalidate_display)
	- GUI Edit
		- Edit Rom Info (invalidate_romlistmeta)
		- Add / Remove Tag (invalidate_rominfo)
		- Add / Remove Fav (invalidate_rominfo)
	- Modifying files
		- Romlist (invalidate_romlistmeta)
		- Tags (invalidate_display)
		- Fav (invalidate_display)
		- Roms (invalidate_available)

	NOTE:
	- FileIsAvailable must be used by a display for modified roms to invalidate its cache
	- Tag/Fav must be used by a display for Add/Remove to invalidate its cache
	- On first run all the debug info will report "Failed", this is normal since there's no cache

	Invalidations may cause a cascade of cache deletions!
	For example, modifying the romlist will invalidate RomlistMeta, triggering invalidation of ALL displays using that romlist

		invalidate_romlistmeta	-> invalidate_romlist
		invalidate_romlist		-> invalidate_display
		invalidate_display		-> invalidate_globalfilter
		invalidate_globalfilter	-> invalidate_filter
		invalidate_filter
		invalidate_available	-> invalidate_rominfo
		invalidate_rominfo		-> invalidate_globalfilter | invalidate_filter
		invalidate_stats		-> invalidate_rominfo (called prior)

		validate_romlistmeta 	-> invalidate_romlistmeta
		validate_display 		-> invalidate_display
		validate_available 		-> invalidate_available
*/

// Include depending on the cache mode
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
const char *FE_CACHE_EMULATOR = "emulator";
const char *FE_CACHE_AVAILABLE = "available";
const char *FE_CACHE_ROMLIST = "romlist";
const char *FE_CACHE_CONFIG = "config";
const char *FE_CACHE_GLOBALFILTER = "globalfilter";
const std::string FE_EMPTY_STRING;

std::vector<FeDisplayInfo>* FeCache::m_displays = {};
std::string FeCache::m_config_path = "";
std::map<std::string, std::map<std::string, std::vector<std::string>>> FeCache::m_stats_cache = {};
int FeCache::m_indent = 0;

// -------------------------------------------------------------------------------------

#ifdef FE_CACHE_DEBUG

//
// Debug logging with indentation
// - Invalidation methods often trigger each other, this makes it easier to find the root
//
void FeCache::debug(
	std::string value,
	std::string filename,
	bool success
)
{
	FeLog() << "FeCache: "
		<< std::string( 2 * m_indent++, ' ')
		<< value
		<< ( !filename.empty() ? " '" + filename + "'" : "" )
		<< ( !success ? " Failed" : "" )
		<< std::endl;
}

void FeCache::_debug()
{
	m_indent--;
}

#else

void FeCache::debug( std::string value, std::string filename, bool success ) {}
void FeCache::_debug() {}

#endif

// -------------------------------------------------------------------------------------

#ifndef FE_CACHE_ENABLE

// Dummy methods if cache is disabled
void FeCache::set_config_path( const std::string path ) {}
void FeCache::set_displays( std::vector<FeDisplayInfo>* displays ) {}
bool FeCache::validate_romlistmeta( FeRomList &romlist ) { return false; }
bool FeCache::save_display( FeDisplayInfo &display, FeRomList &romlist ) { return false; }
bool FeCache::validate_display( FeDisplayInfo &display, FeRomList &romlist ) { return false; }
bool FeCache::save_available( const FeRomList &romlist, const std::map<std::string, std::vector<std::string>> &emu_roms ) { return false; }
bool FeCache::load_available( const FeRomList &romlist, std::map<std::string, std::vector<std::string>> &emu_roms ) { return false; }
bool FeCache::validate_available( FeRomList &romlist, std::map<std::string, std::vector<std::string>> &emu_roms ) { return false; }
bool FeCache::save_romlist( const FeRomList &romlist ) { return false; }
bool FeCache::load_romlist( FeRomList &romlist ) { return false; }
bool FeCache::save_globalfilter( const FeDisplayInfo &display, const FeRomList &romlist ) { return false; }
bool FeCache::load_globalfilter( const FeDisplayInfo &display, FeRomList &romlist ) { return false; }
bool FeCache::save_filter( FeDisplayInfo &display, const FeFilterEntry &entry, const int filter_index ) { return false; }
bool FeCache::load_filter( FeDisplayInfo &display, FeFilterEntry &entry, const int filter_index, const std::map<int, FeRomInfo*> &lookup ) { return false; }
void FeCache::invalidate_rominfo( const FeRomList &romlist, const std::set<FeRomInfo::Index> targets ) {}
void FeCache::clear_stats() {}
bool FeCache::set_stats_info( const std::string &path, const std::vector<std::string> &rominfo ) { return false; }
bool FeCache::get_stats_info( const std::string &path, std::vector<std::string> &rominfo ) { return false; }

#else

// -------------------------------------------------------------------------------------

//
// Create path for config files
//
void FeCache::set_config_path(
	const std::string path
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

std::string FeCache::get_romlistmeta_filename(
	const FeRomList &romlist
)
{
	std::string name = romlist.get_romlist_name();
	return name.empty()
		? FE_EMPTY_STRING
		: m_config_path + FE_CACHE_SUBDIR + FE_CACHE_ROMLIST + "." + sanitize_filename( name ) + "." + FE_CACHE_CONFIG + FE_CACHE_EXT;
}

std::string FeCache::get_display_filename(
	const FeDisplayInfo &display
)
{
	std::string name = display.get_name();
	return name.empty()
		? FE_EMPTY_STRING
		: m_config_path + FE_CACHE_SUBDIR + FE_CACHE_DISPLAY + "." + sanitize_filename( name ) + "." + FE_CACHE_CONFIG + FE_CACHE_EXT;
}

std::string FeCache::get_available_filename(
	const FeRomList &romlist
)
{
	std::string name = romlist.get_romlist_name();
	return name.empty()
		? FE_EMPTY_STRING
		: m_config_path + FE_CACHE_SUBDIR + FE_CACHE_ROMLIST + "." + sanitize_filename( name ) + "." + FE_CACHE_AVAILABLE + FE_CACHE_EXT;
}

std::string FeCache::get_romlist_filename(
	const FeRomList &romlist
)
{
	std::string name = romlist.get_romlist_name();
	return name.empty()
		? FE_EMPTY_STRING
		: m_config_path + FE_CACHE_SUBDIR + FE_CACHE_ROMLIST + "." + sanitize_filename( name ) + FE_CACHE_EXT;
}

std::string FeCache::get_globalfilter_filename(
	const FeDisplayInfo &display
)
{
	std::string name = display.get_name();
	return name.empty()
		? FE_EMPTY_STRING
		: m_config_path + FE_CACHE_SUBDIR + FE_CACHE_DISPLAY + "." + sanitize_filename( name ) + "." + FE_CACHE_GLOBALFILTER + FE_CACHE_EXT;
}

std::string FeCache::get_filter_filename(
	const FeDisplayInfo &display,
	const int filter_index
)
{
	std::string name = display.get_name();
	return name.empty()
		? FE_EMPTY_STRING
		: m_config_path + FE_CACHE_SUBDIR + FE_CACHE_DISPLAY + "." + sanitize_filename( name ) + "." + FE_CACHE_FILTER + "." + as_str( filter_index ) + FE_CACHE_EXT;
}

std::string FeCache::get_stats_filename(
	const std::string &emulator
)
{
	return emulator.empty()
		? FE_EMPTY_STRING
		: m_config_path + FE_CACHE_SUBDIR + FE_CACHE_EMULATOR + "." + sanitize_filename( emulator ) + "." + FE_CACHE_STATS + FE_CACHE_EXT;
}

// -------------------------------------------------------------------------------------

template <typename T>
bool FeCache::save_cache(
	const std::string &filename,
	const T &data
)
{
	nowide::ofstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		{	// block flushes archive
			OutputArchive archive( file );
			archive( data );
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

template <typename T>
bool FeCache::load_cache(
	const std::string &filename,
	T &info
)
{
	nowide::ifstream file( filename, std::ios::binary );
	if ( !file.is_open() ) return false;

	try
	{
		{	// block flushes archive
			InputArchive archive( file );
			archive( info );
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

void FeCache::delete_cache(
	const std::string &filename
)
{
	delete_file( filename );
}

// -------------------------------------------------------------------------------------
//
// RomlistMeta cache stores the modified time of the given romlist file
// - Used to detect external changes to the romlist
//

bool FeCache::save_romlistmeta(
	const FeRomList &romlist,
	std::map<std::string, std::string> &info
)
{
	FeCacheMap cacheMap( info );
	bool success = save_cache( get_romlistmeta_filename( romlist ), cacheMap );
	debug( "Save RomlistMeta Cache", romlist.get_romlist_name(), success );
	if ( !success ) invalidate_romlistmeta( romlist );
	_debug();
	return success;
}

bool FeCache::load_romlistmeta(
	const FeRomList &romlist,
	std::map<std::string, std::string> &info
)
{
	FeCacheMap cacheMap( info );
	bool success = load_cache( get_romlistmeta_filename( romlist ), cacheMap );
	debug( "Load RomlistMeta Cache", romlist.get_romlist_name(), success );
	if ( !success ) invalidate_romlistmeta( romlist );
	_debug();
	return success;
}

void FeCache::invalidate_romlistmeta(
	const FeRomList &romlist
)
{
	debug( "Invalidate RomlistMeta", romlist.get_romlist_name() );
	delete_cache( get_romlistmeta_filename( romlist ) );

	// Invalidate Romlist if it exists (will invalidate normally later otherwise)
	if ( file_exists( get_romlist_filename( romlist ) ) )
		invalidate_romlist( romlist );
	_debug();
}

void FeCache::get_romlistmeta_data(
	FeRomList &romlist,
	std::map<std::string, std::string> &data
)
{
	data["romlist_mtime"] = as_str( file_mtime( romlist.get_romlist_path() ) );
}

// Ensure RomlistMeta cache is valid, call this before load_globalfilter or load_filter
bool FeCache::validate_romlistmeta(
	FeRomList &romlist
)
{
	debug( "Validate RomlistMeta", romlist.get_romlist_name() );
	std::map<std::string, std::string> prev;
	bool loaded = load_romlistmeta( romlist, prev );

	std::map<std::string, std::string> next;
	get_romlistmeta_data( romlist, next );
	bool valid = ( prev == next );

	if ( !valid )
	{
		if ( loaded )
		{
			debug( "Validate RomlistMeta", romlist.get_romlist_name(), false );
			invalidate_romlistmeta( romlist );
			_debug();
		}
		save_romlistmeta( romlist, next );
	}

	_debug();
	return valid;
}

// -------------------------------------------------------------------------------------
//
// Display cache stores metadata about the given display
// - Group clones, tag/fav modified times, globalfilter rules
// - Used to detect both internal and external changes to the globalfilter
//

bool FeCache::save_display(
	FeDisplayInfo &display,
	FeRomList &romlist
)
{
	std::map<std::string, std::string> next;
	get_display_metadata( display, romlist, next );
	return save_display( display, next );
}

bool FeCache::save_display(
	const FeDisplayInfo &display,
	std::map<std::string, std::string> &info
)
{
	FeCacheMap cacheMap( info );
	bool success = save_cache( get_display_filename( display ), cacheMap );
	debug( "Save Display Cache", display.get_name(), success );
	if ( !success ) invalidate_display( display );
	_debug();
	return success;
}

bool FeCache::load_display(
	const FeDisplayInfo &display,
	std::map<std::string, std::string> &info
)
{
	FeCacheMap cacheMap( info );
	bool success = load_cache( get_display_filename( display ), cacheMap );
	debug( "Load Display Cache", display.get_name(), success );
	if ( !success ) invalidate_display( display );
	_debug();
	return success;
}

void FeCache::invalidate_display(
	const FeDisplayInfo &display
)
{
	debug( "Invalidate Display", display.get_name() );
	delete_cache( get_display_filename( display ) );
	// Invalidate Globalfilter, which causes all Filters to invalidate also
	invalidate_globalfilter( display );
	_debug();
}

// Ensure display cache is valid, call this before load_globalfilter or load_filter
bool FeCache::validate_display(
	FeDisplayInfo &display,
	FeRomList &romlist
)
{
	debug( "Validate Display", display.get_name() );
	std::map<std::string, std::string> prev;
	bool loaded = load_display( display, prev );

	std::map<std::string, std::string> next;
	get_display_metadata( display, romlist, next );
	bool valid = ( prev == next );

	if ( !valid )
	{
		if ( loaded )
		{
			debug( "Validate Display", display.get_name(), false );
			invalidate_display( display );
			_debug();
		}
		save_display( display, next );
	}

	_debug();
	return valid;
}

// Returns a list of data that when changed will trigger display invalidation
void FeCache::get_display_metadata(
	FeDisplayInfo &display,
	FeRomList &romlist,
	std::map<std::string, std::string> &data
)
{
	// romlist
	data["group_clones"] = as_str( romlist.get_group_clones() );

	// favs/tags
	data["fav_mtime"] = as_str( file_mtime( romlist.get_fav_path() ) );
	std::vector<std::string> tags = romlist.get_tag_names();
	for ( std::vector<std::string>::iterator itt=tags.begin(); itt!=tags.end(); ++itt )
		data["tag_" + (*itt) + "_mtime"] = as_str( file_mtime( romlist.get_tag_path(*itt) ) );

	// filters
	data["global_filter"] = get_filter_id( display.get_global_filter() );
}

// -------------------------------------------------------------------------------------
//
// Available cache stores a list of rom names for ALL emulators used by romlist
// - Used to detect changes for FileIsAvailable rules
//

bool FeCache::save_available(
	const FeRomList &romlist,
	std::map<std::string, std::vector<std::string>> &emu_roms
)
{
	FeCacheList cacheList( emu_roms );
	bool success = save_cache( get_available_filename( romlist ), cacheList );
	debug( "Save Available Cache", romlist.get_romlist_name(), success );
	if ( !success ) invalidate_available( romlist );
	_debug();
	return success;
}

bool FeCache::load_available(
	const FeRomList &romlist,
	std::map<std::string, std::vector<std::string>> &emu_roms
)
{
	FeCacheList cacheList( emu_roms );
	bool success = load_cache( get_available_filename( romlist ), cacheList );
	debug( "Load Available Cache", romlist.get_romlist_name(), success );
	if ( !success ) invalidate_available( romlist );
	_debug();
	return success;
}

void FeCache::invalidate_available(
	const FeRomList &romlist
)
{
	debug( "Invalidate Available", romlist.get_romlist_name() );
	delete_cache( get_available_filename( romlist ) );
	// Invalidate all Displays using this Romlist with FileIsAvailable info
	invalidate_rominfo( romlist, { FeRomInfo::FileIsAvailable });
	_debug();
}

// Ensure Available cache matches file system.
// - Populates `emu_roms` with `gather_rom_names`, which may be reused by caller to avoid re-gathering valid results
bool FeCache::validate_available(
	FeRomList &romlist,
	std::map<std::string, std::vector<std::string>> &emu_roms
)
{
	debug( "Validate File Availability", romlist.get_romlist_name() );

	// Exit early if cannot load
	if ( !load_available( romlist, emu_roms ) )
	{
		_debug();
		emu_roms.clear();
		return false;
	}

	// Invalidate rominfo if any roms dont match
	for ( std::map<std::string, std::vector<std::string>>::iterator ite=emu_roms.begin(); ite != emu_roms.end(); ++ite )
	{
		FeEmulatorInfo *emu = romlist.get_emulator( (*ite).first );
		std::vector<std::string> names;
		if ( emu ) emu->gather_rom_names( names );
		if ( names != (*ite).second )
		{
			debug( "Validate File Availability", (*ite).first, false );
			invalidate_available( romlist );
			_debug();

			emu_roms.clear();
			_debug();
			return false;
		}
	}

	_debug();
	return true;
}

// -------------------------------------------------------------------------------------
//
// Romlist Cache stores a copy of the romlist file contents
// - Data is identical to `romlist.txt`, but in format that's faster to load
//

bool FeCache::save_romlist(
	const FeRomList &romlist
)
{
	bool success = save_cache( get_romlist_filename( romlist ), romlist );
	debug( "Save Romlist Cache", romlist.get_romlist_name(), success );
	if ( !success ) invalidate_romlist( romlist );
	_debug();
	return success;
}

bool FeCache::load_romlist(
	FeRomList &romlist
)
{
	bool success = load_cache( get_romlist_filename( romlist ), romlist );
	debug( "Load Romlist Cache", romlist.get_romlist_name(), success );
	if ( !success ) invalidate_romlist( romlist );
	_debug();
	return success;
}

void FeCache::invalidate_romlist(
	const FeRomList &romlist
)
{
	debug( "Invalidate Romlist", romlist.get_romlist_name() );
	delete_cache( get_romlist_filename( romlist ) );
	std::string romlist_name = romlist.get_romlist_name();

	// Invalidate ALL displays using this romlist
	for (std::vector<FeDisplayInfo>::const_iterator itd=(*m_displays).begin(); itd!=(*m_displays).end(); ++itd)
	{
		FeDisplayInfo display = (*itd);
		if ( display.get_romlist_name() == romlist_name ) invalidate_display( display );
	}
	_debug();
}

// -------------------------------------------------------------------------------------
//
// GlobalFilter Cache stores the results of the global filter
// - It contains a list of roms much like the romlist, and may be loaded in its place
//

bool FeCache::save_globalfilter(
	const FeDisplayInfo &display,
	const FeRomList &romlist
)
{
	bool success = save_cache( get_globalfilter_filename( display ), romlist );
	debug( "Save GlobalFilter Cache", display.get_name(), success );
	if ( !success ) invalidate_globalfilter( display );
	_debug();
	return success;
}

bool FeCache::load_globalfilter(
	const FeDisplayInfo &display,
	FeRomList &romlist
)
{
	bool success = load_cache( get_globalfilter_filename( display ), romlist );
	debug( "Load GlobalFilter Cache", display.get_name(), success );
	if ( !success ) invalidate_globalfilter( display );
	_debug();
	return success;
}

void FeCache::invalidate_globalfilter(
	const FeDisplayInfo &display
)
{
	debug( "Invalidate GlobalFilter", display.get_name() );
	delete_cache( get_globalfilter_filename( display ) );
	// Invalidate ALL filters for this display
	int filters_count = std::max( display.get_filter_count(), 1 );
	for ( int i=0; i<filters_count; i++ )
		invalidate_filter( display, i );
	_debug();
}

// -------------------------------------------------------------------------------------
//
// Filter Cache stores a set of indexes that reference roms in the romlist/globalfilter cache
//

bool FeCache::save_filter(
	FeDisplayInfo &display,
	const FeFilterEntry &entry,
	const int filter_index
)
{
	FeFilter *f = display.get_filter_count()
		? display.get_filter( filter_index )
		: display.get_global_filter();

	FeFilterIndexes indexes;
	indexes.entry_to_index( entry );
	indexes.set_size( f->get_size() );
	indexes.set_filter_id( get_filter_id( f ) );

	bool success = save_cache( get_filter_filename( display, filter_index ), indexes );
	debug( "Save Filter Cache", display.get_name() + ":" + as_str(filter_index), success );
	if ( !success ) invalidate_filter( display, filter_index );
	_debug();
	return success;
}

bool FeCache::load_filter(
	FeDisplayInfo &display,
	FeFilterEntry &entry,
	const int filter_index,
	const std::map<int, FeRomInfo*> &lookup
)
{
	FeFilterIndexes indexes;
	bool success = load_cache( get_filter_filename( display, filter_index ), indexes );
	debug( "Load Filter Cache", display.get_name() + ":" + as_str(filter_index), success );
	if ( success )
	{
		FeFilter *f = display.get_filter_count()
			? display.get_filter( filter_index )
			: display.get_global_filter();

		if ( indexes.get_filter_id() != get_filter_id( f )
			|| !indexes.index_to_entry( entry, lookup ))
		{
			invalidate_filter( display, filter_index );
			_debug();
			return false;
		}

		f->set_size( indexes.get_size() );
	}
	else
		invalidate_filter( display, filter_index );

	_debug();
	return success;
}

void FeCache::invalidate_filter(
	const FeDisplayInfo &display,
	const int filter_index
)
{
	debug( "Invalidate Filter", display.get_name() + ":" + as_str(filter_index) );
	delete_cache( get_filter_filename( display, filter_index ) );
	_debug();
}

// Returns a simple identifier string for the filter, by joining all its rules together
// - Used to detect rule changes
std::string FeCache::get_filter_id(
	FeFilter *filter
)
{
	std::string id = "";
	id += as_str( filter->get_reverse_order() )+ ";";
	id += as_str( filter->get_sort_by() ) + ";";

	std::vector<FeRule> &rules = filter->get_rules();
	for ( std::vector<FeRule>::iterator itr=rules.begin(); itr!=rules.end(); ++itr )
	{
		FeRule rule = *itr;
		id += as_str( rule.get_target() ) + ";";
		id += as_str( rule.get_comp() ) + ";";
		id += rule.get_what() + ";";
		id += as_str( rule.is_exception() ) + ";";
	}

	return id;
}

// -------------------------------------------------------------------------------------

//
// Invalidate all Globalfilters and Filters using the Romlist with the given RomInfo targets
// Called when rom info has changed, such as Tags and Stats
//
void FeCache::invalidate_rominfo(
	const FeRomList &romlist,
	const std::set<FeRomInfo::Index> targets
)
{
	std::string t = "";
	for (std::set<FeRomInfo::Index>::iterator itt=targets.begin(); itt!=targets.end(); ++itt)
		t += (std::distance(targets.begin(), itt) ? ", " : "") + (std::string)FeRomInfo::indexStrings[*itt];
	debug( "Invalidate Rominfo", t );

	std::string romlist_name = romlist.get_romlist_name();
	for (std::vector<FeDisplayInfo>::const_iterator itd=(*m_displays).begin(); itd!=(*m_displays).end(); ++itd)
	{
		FeDisplayInfo display = (*itd);
		if ( display.get_romlist_name() != romlist_name ) continue;

		// Clear the globalfilter if it uses the target (also clears other filters)
		FeFilter *global_filter = display.get_global_filter();
		if ( global_filter && global_filter->test_for_targets( targets ) )
		{
			invalidate_globalfilter( display );
			continue;
		}

		// Clear filters that are using the target
		int filters_count = display.get_filter_count();
		for ( int i=0; i<filters_count; i++ )
			if ( display.get_filter( i )->test_for_targets( targets ) )
				invalidate_filter( display, i );
	}
	_debug();
}

// -------------------------------------------------------------------------------------
//
// Stats Cache holds rom stats for the given emulator
//

bool FeCache::save_stats(
	const std::string &emulator
)
{
	if ( m_stats_cache.find(emulator) == m_stats_cache.end() ) return false;
	FeCacheList cacheList( m_stats_cache[emulator] );
	bool success = save_cache( get_stats_filename( emulator ), cacheList );
	debug( "Save Stats Cache", emulator, success );
	if ( !success ) invalidate_stats( emulator );
	_debug();
	return success;
}

bool FeCache::load_stats(
	const std::string &emulator
)
{
	FeCacheList cacheList( m_stats_cache[emulator] );
	bool success = load_cache( get_stats_filename( emulator ), cacheList );
	debug( "Load Stats Cache", emulator, success );
	if ( !success ) invalidate_stats( emulator );
	_debug();
	return success;
}

void FeCache::invalidate_stats(
	const std::string &emulator
)
{
	debug( "Invalidate Stats", emulator );
	delete_cache( get_stats_filename( emulator ) );
	_debug();
}

// Ensure in-memory stats entry exists for the given emulator
bool FeCache::validate_stats(
	const std::string &path,
	const std::string &emulator
) {
	// Return success if already loaded
	if ( m_stats_cache.find(emulator) != m_stats_cache.end() ) return true;

	// Return success if successfully loaded from cache
	if ( load_stats( emulator ) ) return true;

	// Abort if invalid path to load stats from
	if ( path.empty() ) return false;

	// Abort if there are no stats to load
	std::vector<std::string> file_list;
	std::string stats_path = path + emulator + "/";
	if ( !get_basename_from_extension( file_list, stats_path, FE_STAT_FILE_EXTENSION, true ) ) return false;

	// Create new stats cache object for emulator
	m_stats_cache[emulator] = {};

	// Parse ALL stat files into cache object
	std::string played_count;
	std::string played_time;
	for ( std::vector<std::string>::iterator itf=file_list.begin(); itf != file_list.end(); ++itf )
	{
		nowide::ifstream stat_file( stats_path + *itf + FE_STAT_FILE_EXTENSION );
		if ( !stat_file.is_open() ) continue;

		played_count = "0";
		played_time = "0";
		if ( stat_file.good() ) getline( stat_file, played_count );
		if ( stat_file.good() ) getline( stat_file, played_time );
		stat_file.close();

		m_stats_cache[emulator][*itf] = { played_count, played_time };
	}

	// Save stats cache for next time
	return save_stats( emulator );
}

// Clear the in-memory stats cache, called when a new romlist is loaded
// - If not cleared in-memory stats will simply grow to contain all emulator stats
void FeCache::clear_stats()
{
	m_stats_cache.clear();
}

// Load stats from cache to rominfo
// - Always returns true to prevent callee attempting to load stats that dont exist
bool FeCache::get_stats_info(
	const std::string &path,
	std::vector<std::string> &rominfo
)
{
	// Reset all info to default
	std::set<FeRomInfo::Index>::iterator index = FeRomInfo::Stats.begin();
	int size = (int)FeRomInfo::Stats.size();
	for ( int i=0; i<size; i++ )
		rominfo[ *index + i ] = "0";

	// Exit early if there are NO stats at all
	std::string emulator = rominfo[FeRomInfo::Emulator];
	if ( !validate_stats( path, emulator ) ) return true;

	// Exit early if there are NO stats for this rom
	std::string romname = rominfo[FeRomInfo::Romname];
	if ( m_stats_cache[emulator].find(romname) == m_stats_cache[emulator].end() ) return true;

	// Copy available stats to the rominfo object
	std::vector<std::string> &info = m_stats_cache[emulator][romname];
	int info_size = info.size();
	for ( int i=0; i<info_size; i++ )
		rominfo[ *index + i ] = info[i];

	return true;
}

// Copy stats from rominfo to cache, then save
bool FeCache::set_stats_info(
	const std::string &path,
	const std::vector<std::string> &rominfo
) {
	// Exit if there's an error creating cache for stat
	// - This shouldn't happen, since this method is only called AFTER get_stats_info
	std::string emulator = rominfo[FeRomInfo::Emulator];
	if ( !validate_stats( path, emulator ) ) return true;

	// Update stats in cache object
	std::string romname = rominfo[FeRomInfo::Romname];
	m_stats_cache[emulator][romname].clear();
	for ( std::set<FeRomInfo::Index>::iterator its=FeRomInfo::Stats.begin(); its != FeRomInfo::Stats.end(); ++its )
		m_stats_cache[emulator][romname].push_back( rominfo[ *its ] );

	// Save the updated stats cache
	return save_stats( emulator );
}

#endif