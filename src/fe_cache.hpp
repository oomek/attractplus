#ifndef FE_CACHE_HPP
#define FE_CACHE_HPP

#include "fe_romlist.hpp"
#include "fe_util.hpp"

#include <algorithm>

class FeCache
{
public:
	static std::vector<int> get_filter_list_indexes(
		std::vector<FeRomInfo*> &list,
		FeRomInfoListType &m_list
	);

	static std::map<std::string, std::vector<int>> get_clone_group_indexes(
		std::map<std::string, std::vector<FeRomInfo*>> &map,
		FeRomInfoListType &m_list
	);

	static void insert_filter_list_indexes(
		std::vector<int> &indexes,
		std::vector<FeRomInfo*> &list,
		FeRomInfoListType &m_list
	);

	static void insert_clone_group_indexes(
		std::map<std::string, std::vector<int>> &indexes,
		std::map<std::string, std::vector<FeRomInfo*>> &map,
		FeRomInfoListType &m_list
	);

	static bool save_cached_romlist( FeRomList &romlist );
	static bool load_cached_romlist( FeRomList &romlist );

	static bool save_cached_filters( FeRomList &romlist );
	static bool load_cached_filters( FeRomList &romlist );
};

#endif
