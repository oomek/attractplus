/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2017 Andrew Mickelson
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

#ifndef FE_FILE_HPP
#define FE_FILE_HPP

#include <string>
#include <SFML/System/InputStream.hpp>
#include <cstdio>

class FeFileInputStream : public sf::InputStream
{
public:
	FeFileInputStream( const std::string &fn );
	~FeFileInputStream();

	std::optional<std::size_t> read( void *data, std::size_t size );
	std::optional<std::size_t> seek( std::size_t pos );
	std::optional<std::size_t> tell();
	std::optional<std::size_t> getSize();

private:
	FILE *m_file;
};

#endif
