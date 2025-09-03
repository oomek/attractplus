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

#include "sqrat_array_wrapper.hpp"

SqratArrayWrapper::SqratArrayWrapper( const std::vector<float> *data )
	: m_data( data )
{
}

void SqratArrayWrapper::set_data( const std::vector<float> *data )
{
	m_data = data;
}

float SqratArrayWrapper::_get( int index )
{
	if ( m_data && index >= 0 && index < static_cast<int>( m_data->size() ))
		return ( *m_data )[index];

	return 0.0f;
}

int SqratArrayWrapper::len() const
{
	return m_data ? static_cast<int>( m_data->size() ) : 0;
}

int SqratArrayWrapper::_nexti( int prev_index )
{
	if ( !m_data ) return -1;

	if ( prev_index < 0 ) // First call
	{
		return m_data->empty() ? -1 : 0;
	}

	int next_index = prev_index + 1;
	return ( next_index < static_cast<int>( m_data->size() )) ? next_index : -1;
}
