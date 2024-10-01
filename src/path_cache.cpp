/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2020 Andrew Mickelson
 *
 *  This file is part of Attract-Mode.
 *
 *  Attract-Mode is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Attract-Mode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Attract-Mode.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "path_cache.hpp"

#include "fe_base.hpp" // logging
#include "fe_util.hpp"

#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>

namespace
{
	bool my_comp( const std::string &a, const std::string &b )
	{
		return ( strncasecmp( a.c_str(), b.c_str(), a.size() ) < 0 );
	}
};

FePathCache::FePathCache()
{
}

FePathCache::~FePathCache()
{
}

void FePathCache::clear()
{
	m_cache.clear();
	FeDebug() << "Cleared artwork path cache." << std::endl;
}

// from fe_util
bool FePathCache::get_filename_from_base(
	std::vector<std::string> &in_list,
	std::vector<std::string> &out_list,
	const std::string &path,
	const std::string &base_name,
	const char **filter )
{
	std::vector<std::string> &cache = get_cache( path );

	std::vector< std::string >::const_iterator itr;
	itr = std::lower_bound( cache.begin(), cache.end(), base_name, my_comp );

	if ( itr == cache.end() )
		return false;

	while ( ( itr != cache.end() )
		&& ( strncasecmp( (*itr).c_str(), base_name.c_str(), base_name.size() ) == 0 ))
	{
		if ( filter && !(tail_compare( *itr, filter )) )
			out_list.push_back( path + *itr );
		else
			in_list.push_back( path + *itr );

		++itr;
	}

	return !(in_list.empty());
}

std::vector < std::string > &FePathCache::get_cache( const std::string &path )
{
	std::map< std::string, std::vector<std::string> >::iterator itr;

	itr = m_cache.find( path );
	if ( itr != m_cache.end() )
		return (*itr).second;

	std::vector < std::string > temp;
	temp.reserve(100);  // Reserve some space to avoid small reallocations

#ifdef SFML_SYSTEM_WINDOWS
	std::string search_path = path + "*";

	struct _wfinddata_t t;
	intptr_t srch = _wfindfirst( FeUtil::widen( search_path ).c_str(), &t );

	if ( srch < 0 )
	{
		FeDebug() << "dir_cache: Error opening directory: " << path << std::endl;
	}
	else
	{
		do
		{
			std::string filename = FeUtil::narrow( t.name );
			if (( filename != "." ) && ( filename != ".." ))
				temp.emplace_back( std::move( filename ));
		} while ( _wfindnext( srch, &t ) == 0 );
		_findclose( srch );
	}
#else
	DIR *dir;
	struct dirent *ent;

	if ( (dir = opendir( path.c_str() )) == NULL )
	{
		FeDebug() << "dir_cache: Error opening directory: " << path << std::endl;
	}
	else
	{
		while ((ent = readdir( dir )) != NULL )
		{
			std::string filename = ent->d_name;
			if (( filename != "." ) && ( filename != ".." ))
				temp.emplace_back( std::move( filename ));
		}
		closedir( dir );
	}
#endif

	std::sort( temp.begin(), temp.end(), my_comp );

	FeDebug() << "Caching contents of artwork path: " << path << " (" << temp.size() << " entries)." << std::endl;

	std::pair<std::map<std::string, std::vector<std::string> >::iterator, bool> ret;

	ret = m_cache.insert(
		std::pair< std::string, std::vector<std::string> >( path, std::vector<std::string>() ));

	if ( ret.second )
		ret.first->second.swap( temp );

	return ret.first->second;
}
