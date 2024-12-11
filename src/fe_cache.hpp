#ifndef FE_CACHE_HPP
#define FE_CACHE_HPP

#include "fe_romlist.hpp"
#include "fe_util.hpp"

#include <algorithm>

class FeCache
{
private:

	static std::string m_config_path;

public:

	static void set_config_path(
		std::string path
	);

	static std::string get_romlist_cache_filename(
		FeDisplayInfo &display
	);

	static std::string get_filter_cache_filename(
		FeDisplayInfo &display,
		int filter_index
	);

	// ----------------------------------------------------------------------------------

	static void filter_list_to_indexes(
		std::vector<int> &indexes,
		std::vector<FeRomInfo*> &list
	);

	static void clone_group_to_indexes(
		std::map<std::string, std::vector<int>> &indexes,
		std::map<std::string, std::vector<FeRomInfo*>> &map
	);

	static void indexes_to_filter_list(
		std::vector<FeRomInfo*> &list,
		std::vector<int> &indexes,
		std::vector<FeRomInfo*> &lookup
	);

	static void indexes_to_clone_group(
		std::map<std::string, std::vector<FeRomInfo*>> &map,
		std::map<std::string, std::vector<int>> &indexes,
		std::vector<FeRomInfo*> &lookup
	);

	static std::vector<FeRomInfo*> get_romlist_lookup(
		FeRomInfoListType &m_list
	);

	// ----------------------------------------------------------------------------------

	static void clear_display_cache(
		FeDisplayInfo &display
	);

	static void clear_romlist_cache(
		FeDisplayInfo &display
	);

	static void clear_filter_cache(
		FeDisplayInfo &display,
		int filter_index
	);

	static void invalidate(
		FeDisplayInfo &display,
		FeRomInfo::Index target
	);

	// ----------------------------------------------------------------------------------

	static bool save_romlist_cache(
		FeDisplayInfo &display,
		FeRomList &romlist
	);

	static bool load_romlist_cache(
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
		std::vector<FeRomInfo*> &lookup
	);

};

#endif
