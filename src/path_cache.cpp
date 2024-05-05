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

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include "path_cache.hpp"

#include "fe_base.hpp" // logging
#include "fe_util.hpp"
#include "fe_vm.hpp"

#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <SFML/System/Clock.hpp>

#include "nowide/fstream.hpp"
#include "nowide/cstdio.hpp"
#include "nowide/cstdlib.hpp"
#include "nowide/convert.hpp"

std::map<std::string, std::map<std::string, int>> FePathCache::m_cache;

void FePathCache::clear()
{
	m_cache.clear();
	FeLog() << "Cleared artwork path cache." << std::endl;
}

bool FePathCache::get_filename_from_base(
	std::vector<std::string> &in_list,
	std::vector<std::string> &out_list,
	const std::string &full_path,
	const std::string &base_name,
	const char **filter )
{
	std::string path = full_path;
	std::replace( path.begin(), path.end(), '\\', '/' );
	std::map<std::string, int>& cache = get_cache(path);
	std::map<std::string, int>::iterator itr = cache.lower_bound(base_name);

	if (itr == cache.end() || !boost::iequals(itr->first.substr(0, base_name.size()), base_name))
		return false;

	while (itr != cache.end() && boost::iequals(itr->first.substr(0, base_name.size()), base_name))
	{
		if (filter && !(tail_compare(itr->first, filter)))
			out_list.push_back(path + itr->first);
		else
			in_list.push_back(path + itr->first);

		++itr;
	}

	return !(in_list.empty());
}

bool FePathCache::file_exists( const std::string &file )
{
	return check_path( file ) & ( FeVM::IsFile | FeVM::IsDirectory );
}

bool FePathCache::directory_exists( const std::string &file )
{
	return check_path( file ) & FeVM::IsDirectory;
}

std::map<std::string, int>& FePathCache::get_cache( const std::string& full_path )
{
	sf::Clock clk;
	std::string path = full_path;
	std::replace( path.begin(), path.end(), '\\', '/' );
	if ( path.back() == '/' ) path.pop_back();
	std::map<std::string, std::map<std::string, int>>::iterator itr;

	itr = m_cache.find( path );
	if ( itr != m_cache.end() )
		return (*itr).second;

	std::map<std::string, int> temp;

	try
	{
		for ( boost::filesystem::directory_iterator it( path ); it != boost::filesystem::directory_iterator(); ++it )
		{
			std::string entryName = it->path().filename().string();
			if ( entryName != "." && entryName != ".." )
			{
				int entryType;
				if ( boost::filesystem::is_directory( it->status() ))
					entryType = FeVM::IsDirectory;
				else
					entryType = FeVM::IsFile;

				temp.insert( std::make_pair( entryName, entryType ));
			}
		}
	}
	catch ( const boost::filesystem::filesystem_error& e )
	{
		FeLog() << "dir_cache: Error listing directory: " << path << std::endl;
	}

	m_cache.insert( std::make_pair( path, temp ));
	FeLog() << "Caching " << path << " took " << clk.getElapsedTime().asMicroseconds() << "us" << std::endl;
	return m_cache[ path ];
}

int FePathCache::check_path( const std::string &full_path )
{
	if ( full_path.empty() )
		return FeVM::IsNotFound;

	std::string path = full_path;
	std::replace( path.begin(), path.end(), '\\', '/' );
	if ( path.back() == '/' ) path.pop_back();
	size_t pos = path.find_last_of( "/" );
	std::string temp_path = path.substr( 0, pos );
	std::string temp_name = path.substr ( pos + 1 );

	std::map<std::string, int>& cache = get_cache(temp_path);
	std::map<std::string, int>::iterator itr = cache.find(temp_name);

	if ( itr == cache.end() )
		return FeVM::IsNotFound;

	if ( itr->first == temp_name )
	{
		if ( itr->second == FeVM::IsDirectory )
			return FeVM::IsDirectory;
		else if ( itr->second == FeVM::IsFile )
			return FeVM::IsFile;
	}

	FeLog() << "check_path: " << temp_name << " not found" << std::endl;
	return FeVM::IsNotFound;
}
