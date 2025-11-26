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

#ifndef SQ_MATH_HPP
#define SQ_MATH_HPP

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <random>

class SqMath
{
public:
	static const int sign( const float x );
	static const int round( const float x );
	static const int round2( const float x );
	static const int floor2( const float x );
	static const int ceil2( const float x );
	static const float fract( const float x );
	static const float clamp( const float x, const float min, const float max );
	static const float min( const float a, const float b );
	static const float max( const float a, const float b );
	static const float mix( const float a, const float b, const float x );
	static const float randomf( const float min, const float max );
	static const int random( const float min, const float max );
	static const float modulo( const float v, const float m );
	static const float hypot( const float x, const float y );
	static const float degrees( const float r );
	static const float radians( const float d );
	static const float exp2( const float x );
	static const float log2( const float x );
	static const float short_dist( const float a, const float b, const float m );
	static const float short_mix( const float a, const float b, const float m, const float t );
};

#endif
