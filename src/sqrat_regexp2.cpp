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

#include "sqrat_regexp2.hpp"

//
// regexp2 class for squirrel
// - strings get widened so regex can match "naïve"
// - results get narrowed since squirrel does not use wstrings
// - ie: "naïve".len() == 6
//
Regexp2::Regexp2( const std::string pattern )
{
	try {
		m_regex = std::wregex( FeUtil::widen( pattern ), std::regex::ECMAScript );
		m_compiled = true;
	}
	catch ( const std::regex_error& e )
	{
		FeLog() << "Error compiling regular expression \"" << pattern << "\": " << e.what() << std::endl;
	}
}

Sqrat::Array Regexp2::capture( const std::string str )
{
	return capture( str, 0 );
}

Sqrat::Array Regexp2::capture( const std::string str, const int start )
{
	if ( !m_compiled )
		return Sqrat::Object(); // null

	std::wsmatch matches;
	std::wstring text = FeUtil::widen( str.substr( start ) );
	if ( !std::regex_search( text, matches, m_regex ) )
		return Sqrat::Object(); // null

	HSQUIRRELVM vm = Sqrat::RootTable().GetVM();
	Sqrat::Array results( vm );
	for (size_t i=0; i<matches.size(); i++)
	{
		int pos = matches.position(i);
		int len = matches.length(i);
		int begin = start + FeUtil::narrow( text.substr( 0, pos ) ).size();
		int end = begin + FeUtil::narrow( text.substr( pos, len ) ).size();

		Sqrat::Table result( vm );
		result.SetValue( _SC("begin"), begin );
		result.SetValue( _SC("end"), end );
		results.Append( result );
	}

	return results;
}

Sqrat::Table Regexp2::search( const std::string str )
{
	return search( str, 0 );
}

Sqrat::Table Regexp2::search( const std::string str, const int start )
{
	if ( !m_compiled )
		return Sqrat::Object(); // null

	std::wsmatch match;
	std::wstring text = FeUtil::widen( str.substr( start ) );
	if ( !std::regex_search( text, match, m_regex ) )
		return Sqrat::Object(); // null

	int pos = match.position(0);
	int len = match.length(0);
	int begin = start + FeUtil::narrow( text.substr( 0, pos ) ).size();
	int end = begin + FeUtil::narrow( text.substr( pos, len ) ).size();

	HSQUIRRELVM vm = Sqrat::RootTable().GetVM();
	Sqrat::Table result( vm );
	result.SetValue( _SC("begin"), begin );
	result.SetValue( _SC("end"), end );
	return result;
}

bool Regexp2::match( const std::string str )
{
	return m_compiled && std::regex_match( FeUtil::widen( str ), m_regex );
}
