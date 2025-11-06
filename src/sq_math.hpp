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
	static int sign( float x );
	static int round( float x );
	static int round2( float x );
	static int floor2( float x );
	static int ceil2( float x );
	static float fract( float x );
	static float clamp( float x, float min, float max );
	static float min( float a, float b );
	static float max( float a, float b );
	static float mix( float a, float b, float x );
	static float randomf( float min, float max );
	static int random( float min, float max );
	static float modulo( float v, float m );
	static float hypot( float x, float y );
	static float degrees( float r );
	static float radians( float d );
	static float exp2( float x );
	static float log2( float x );
};

#endif
