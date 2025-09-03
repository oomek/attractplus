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

#ifndef FE_SQRAT_ARRAY_WRAPPER_HPP
#define FE_SQRAT_ARRAY_WRAPPER_HPP

#include <vector>

class SqratArrayWrapper
{
private:
	const std::vector<float>* m_data;

public:
	SqratArrayWrapper( const std::vector<float> *data );

	void set_data( const std::vector<float> *data );
	float _get( int index );
	int len() const;
	int _nexti( int prev_index );
};

#endif
