/*
 *
 *  Attract-Mode Plus frontend
 *  Copyright (C) 2025 Andrew Mickelson & Radek Dutkiewicz
 *
 *  This file is part of Attract-Mode Plus
 *
 *  Attract-Mode Plus is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Attract-Mode Plus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Attract-Mode Plus.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef FE_SQRAT_REGEXP2_HPP
#define FE_SQRAT_REGEXP2_HPP

#include <regex>
#include <sqrat.h>
#include "fe_base.hpp"
#include "fe_util.hpp"

class Regexp2
{
private:
	std::wregex m_regex;
	bool m_compiled;

public:
	Regexp2( const std::string pattern );
	Regexp2( const std::string pattern, const std::string flags );
	Sqrat::Array capture( const std::string str );
	Sqrat::Array capture( const std::string str, const int start );
	Sqrat::Table search( const std::string str );
	Sqrat::Table search( const std::string str, const int start );
	bool match( const std::string str );
};

#endif
