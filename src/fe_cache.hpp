#ifndef FE_CACHE_HPP
#define FE_CACHE_HPP

#include "fe_romlist.hpp"
#include "fe_util.hpp"

#include <algorithm>

class FeCache
{
public:

	static void filter_list_to_indexes(
		std::vector<int> &indexes,
		std::vector<FeRomInfo*> &list,
		FeRomInfoListType &m_list
	);

	static void clone_group_to_indexes(
		std::map<std::string, std::vector<int>> &indexes,
		std::map<std::string, std::vector<FeRomInfo*>> &map,
		FeRomInfoListType &m_list
	);

	static void indexes_to_filter_list(
		std::vector<FeRomInfo*> &list,
		std::vector<int> &indexes,
		FeRomInfoListType &m_list
	);

	static void indexes_to_clone_group(
		std::map<std::string, std::vector<FeRomInfo*>> &map,
		std::map<std::string, std::vector<int>> &indexes,
		FeRomInfoListType &m_list
	);

	// ----------------------------------------------------------------------------------

	static bool save_display_cache(
		const std::string config_path,
		FeDisplayInfo &display,
		FeRomList &romlist
	);

	static bool load_display_cache(
		const std::string config_path,
		FeDisplayInfo &display,
		FeRomList &romlist
	);

	static bool clear_display_cache(
		const std::string config_path,
		FeDisplayInfo &display
	);

	// ----------------------------------------------------------------------------------

	static bool save_filters_cache(
		const std::string config_path,
		FeDisplayInfo &display,
		FeRomList &romlist
	);

	static bool load_filters_cache(
		const std::string config_path,
		FeDisplayInfo &display,
		FeRomList &romlist
	);

	static bool clear_filters_cache(
		const std::string config_path,
		FeDisplayInfo &display
	);

	// ----------------------------------------------------------------------------------

	static bool fix_cache(
		const std::string config_path,
		FeDisplayInfo &display,
		FeRomInfo::Index target
	);

};

#endif
