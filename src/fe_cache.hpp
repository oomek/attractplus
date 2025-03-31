#ifndef FE_CACHE_HPP
#define FE_CACHE_HPP

#ifndef FE_VERSION_NUM
#define FE_VERSION_NUM 1
#endif

#include "fe_romlist.hpp"
#include "fe_util.hpp"

#include <algorithm>

#include "cereal/cereal.hpp"
#include <cereal/types/map.hpp>

// A single stat item
class FeStat
{
public:
	std::string played_count;
	std::string played_time;

	FeStat() {}
	FeStat( std::string count, std::string time ):
		played_count( count ),
		played_time( time )
	{}

	template<class Archive>
	void serialize( Archive &archive, std::uint32_t const version )
	{
		if ( version != FE_VERSION_NUM ) throw "Invalid FeStat cache";
		archive( played_count, played_time );
	}
};

CEREAL_CLASS_VERSION( FeStat, FE_VERSION_NUM );

// Holds a list of named stats
class FeListStats
{
public:
	std::map<std::string, FeStat> list = std::map<std::string, FeStat>{};

	template<class Archive>
	void serialize( Archive &archive, std::uint32_t const version )
	{
		if ( version != FE_VERSION_NUM ) throw "Invalid FeListStats cache";
		archive( list );
	}
};

CEREAL_CLASS_VERSION( FeListStats, FE_VERSION_NUM );

class FeCache
{
private:

	static std::vector<FeDisplayInfo>* m_displays;
	static std::string m_config_path;
	static std::string m_romlist_args;
	static std::map<std::string, FeListStats> stats_cache;

public:

	static void set_config_path(
		std::string path
	);

	static void set_displays(
		std::vector<FeDisplayInfo>* displays
	);

	// ----------------------------------------------------------------------------------

	static std::string get_romhash_cache_filename(
		const std::string &romlist_name
	);

	static std::string get_romlist_cache_filename(
		const std::string &romlist_name
	);

	static std::string get_globalfilter_cache_filename(
		FeDisplayInfo &display
	);

	static std::string get_filter_cache_filename(
		FeDisplayInfo &display,
		int filter_index
	);

	static std::string get_stats_cache_filename(
		std::string &emulator
	);

	// ----------------------------------------------------------------------------------

	static void invalidate_romlist(
		const std::string &romlist_name
	);

	static void invalidate_display(
		FeDisplayInfo &display
	);

	static void invalidate_globalfilter(
		FeDisplayInfo &display
	);

	static void invalidate_filter(
		FeDisplayInfo &display,
		int filter_index
	);

	static void invalidate_rominfo(
		const std::string &romlist_name,
		FeRomInfo::Index target
	);

	static void invalidate_romlist_args();

	// ----------------------------------------------------------------------------------

	static bool save_romhash_cache(
		const std::string &romlist_name,
		std::set<std::string> &emu_set,
		size_t &hash
	);

	static bool load_romhash_cache(
		const std::string &romlist_name,
		std::set<std::string> &emu_set,
		size_t &hash
	);

	// ----------------------------------------------------------------------------------

	static bool save_romlist_cache(
		const std::string &romlist_name,
		FeRomList &romlist
	);

	static bool load_romlist_cache(
		const std::string &romlist_name,
		FeRomList &romlist
	);

	// ----------------------------------------------------------------------------------

	static bool save_globalfilter_cache(
		FeDisplayInfo &display,
		FeRomList &romlist
	);

	static bool load_globalfilter_cache(
		FeDisplayInfo &display,
		FeRomList &romlist
	);

	// ----------------------------------------------------------------------------------

	static bool save_filter_cache(
		FeDisplayInfo &display,
		FeFilterEntry &entry,
		int filter_index
	);

	static bool load_filter_cache(
		FeDisplayInfo &display,
		FeFilterEntry &entry,
		int filter_index,
		std::map<int, FeRomInfo*> &lookup
	);

	// ----------------------------------------------------------------------------------

	static bool save_stats_cache(
		std::string &emulator
	);

	static bool load_stats_cache(
		std::string &emulator
	);

	// ----------------------------------------------------------------------------------

	static bool confirm_stats_cache(
		const std::string &path,
		std::string &emulator
	);

	static bool set_stats_info(
		const std::string &path,
		std::vector<std::string> &rominfo
	);

	static bool get_stats_info(
		const std::string &path,
		std::vector<std::string> &rominfo
	);

	// ----------------------------------------------------------------------------------

	static bool set_romlist_args(
		const std::string &path,
		const std::string &romlist_name,
		FeDisplayInfo &display,
		bool group_clones,
		bool load_stats
	);

};

#endif
