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

#include "sqrat_math.hpp"

// Returns 1 when x > 0, returns -1 when x < 0, returns 0 when x == 0
int sq_math_sign( float x )
{
	return (x > 0.f) ? 1 : ((x < 0.f) ? -1 : 0);
}

// Rounds up to the nearest integer
int sq_math_round( float x )
{
	return std::floor( x + 0.5 );
}

// Rounds up to the nearest even integer
int sq_math_round2( float x )
{
	return std::floor( x / 2.0 + 0.5 ) * 2;
}

// Floors to the nearest even integer
int sq_math_floor2( float x )
{
	return std::floor( x / 2.0 ) * 2;
}

// Ceils to the nearest even integer
int sq_math_ceil2( float x )
{
	return std::ceil( x / 2.0 ) * 2;
}

// Returns a fractional part of x
float sq_math_fract( float x )
{
	return sq_math_modulo( x, 1.0 );
}

// Clamps x between min and max
float sq_math_clamp( float x, float min, float max )
{
	return std::clamp( x, min, max );
}

// Returns the smallest a or b
float sq_math_min( float a, float b )
{
	return std::min( a, b );
}

// Returns the largest a or b
float sq_math_max( float a, float b )
{
	return std::max( a, b );
}

// Returns a blend between a and b with using a mixing ratio x
float sq_math_mix( float a, float b, float x )
{
	return x * ( a - b ) + b;
}

// Returns a random integer in a range defined by min and max
int sq_math_random( float min, float max )
{
	return std::floor( (float)rand() / (float)(RAND_MAX + 1) * ( max - ( min - 1.0 )) + min );
}

// Modulo with correct handling of negative numbers
float sq_math_modulo( float v, float m )
{
	return m ? fmod( fmod(v, m) + m, m ) : 0.f;
}
