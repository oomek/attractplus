#ifndef FE_CACHE_HPP
#define FE_CACHE_HPP

#include "fe_romlist.hpp"
#include "fe_util.hpp"

#include <algorithm>

class FeCache
{
private:

	static std::vector<FeDisplayInfo>* m_displays;
	static std::string m_config_path;
	static std::map<std::string, std::map<std::string, std::vector<std::string>>> m_stats_cache;
	static int m_indent;

	static void debug(
		std::string value,
		std::string filename = "",
		bool success = true
	);

	static void _debug();

	static std::string get_romlistmeta_filename(
		const FeRomList &romlist
	);

	static std::string get_display_filename(
		const FeDisplayInfo &display
	);

	static std::string get_available_filename(
		const FeRomList &romlist
	);

	static std::string get_romlist_filename(
		const FeRomList &romlist
	);

	static std::string get_globalfilter_filename(
		const FeDisplayInfo &display
	);

	static std::string get_filter_filename(
		const FeDisplayInfo &display,
		const int filter_index
	);

	static std::string get_stats_filename(
		const std::string &emulator
	);

	// ----------------------------------------------------------------------------------

	template <typename T>
	static bool save_cache(
		const std::string &filename,
		const T &info
	);

	template <typename T>
	static bool load_cache(
		const std::string &filename,
		T &info
	);

	static void delete_cache(
		const std::string &filename
	);

	// ----------------------------------------------------------------------------------

	static bool save_romlistmeta(
		const FeRomList &romlist,
		const std::map<std::string, std::string> &info
	);

	static bool load_romlistmeta(
		const FeRomList &romlist,
		std::map<std::string, std::string> &info
	);

	static void invalidate_romlistmeta(
		const FeRomList &romlist
	);

	static void get_romlistmeta_data(
		FeRomList &romlist,
		std::map<std::string, std::string> &data
	);

	// ----------------------------------------------------------------------------------

	static bool save_display(
		const FeDisplayInfo &display,
		const std::map<std::string, std::string> &info
	);

	static bool load_display(
		const FeDisplayInfo &display,
		std::map<std::string, std::string> &info
	);

	static void invalidate_display(
		const FeDisplayInfo &display
	);

	static void get_display_metadata(
		FeDisplayInfo &display,
		FeRomList &romlist,
		std::map<std::string, std::string> &data
	);

	// ----------------------------------------------------------------------------------

	static void invalidate_available(
		const FeRomList &romlist
	);

	static void invalidate_romlist(
		const FeRomList &romlist
	);

	static void invalidate_globalfilter(
		const FeDisplayInfo &display
	);

	static void invalidate_filter(
		const FeDisplayInfo &display,
		const int filter_index
	);

	static std::string get_filter_id(
		FeFilter *filter
	);

	// ----------------------------------------------------------------------------------

	static bool save_stats(
		const std::string &emulator
	);

	static bool load_stats(
		const std::string &emulator
	);

	static void invalidate_stats(
		const std::string &emulator
	);

	static bool validate_stats(
		const std::string &path,
		const std::string &emulator
	);

public:

	static void set_config_path(
		const std::string path
	);

	static void set_displays(
		std::vector<FeDisplayInfo>* displays
	);

	// ----------------------------------------------------------------------------------

	static bool validate_romlistmeta(
		FeRomList &romlist
	);

	// ----------------------------------------------------------------------------------

	static bool save_display(
		FeDisplayInfo &display,
		FeRomList &romlist
	);

	static bool validate_display(
		FeDisplayInfo &display,
		FeRomList &romlist
	);

	// ----------------------------------------------------------------------------------

	static bool save_available(
		const FeRomList &romlist,
		const std::map<std::string, std::vector<std::string>> &emu_roms
	);

	static bool load_available(
		const FeRomList &romlist,
		std::map<std::string, std::vector<std::string>> &emu_roms
	);

	static bool validate_available(
		FeRomList &romlist,
		std::map<std::string, std::vector<std::string>> &emu_roms
	);

	// ----------------------------------------------------------------------------------

	static bool save_romlist(
		const FeRomList &romlist
	);

	static bool load_romlist(
		FeRomList &romlist
	);

	// ----------------------------------------------------------------------------------

	static bool save_globalfilter(
		const FeDisplayInfo &display,
		const FeRomList &romlist
	);

	static bool load_globalfilter(
		const FeDisplayInfo &display,
		FeRomList &romlist
	);

	// ----------------------------------------------------------------------------------

	static bool save_filter(
		FeDisplayInfo &display,
		const FeFilterEntry &entry,
		const int filter_index
	);

	static bool load_filter(
		FeDisplayInfo &display,
		FeFilterEntry &entry,
		const int filter_index,
		const std::map<int, FeRomInfo*> &lookup
	);

	// ----------------------------------------------------------------------------------

	static void invalidate_rominfo(
		const FeRomList &romlist,
		const std::set<FeRomInfo::Index> targets
	);

	// ----------------------------------------------------------------------------------

	static void clear_stats();

	static bool set_stats_info(
		const std::string &path,
		const std::vector<std::string> &rominfo
	);

	static bool get_stats_info(
		const std::string &path,
		std::vector<std::string> &rominfo
	);

};

#endif
