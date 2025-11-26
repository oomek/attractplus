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

#include "sq_math.hpp"

namespace {
	std::mt19937 rnd{ std::random_device{}() };
	std::uniform_real_distribution<double> dis_excl( 0.0, 1.0 );
	std::uniform_real_distribution<double> dis_incl( 0.0, 1.0 + std::numeric_limits<double>::epsilon() );
}

// Returns 1 when x > 0, returns -1 when x < 0, returns 0 when x == 0
const int SqMath::sign( const float x )
{
	return (x > 0.f) ? 1 : ((x < 0.f) ? -1 : 0);
}

// Rounds up to the nearest integer
const int SqMath::round( const float x )
{
	return std::floor( x + 0.5 );
}

// Rounds up to the nearest even integer
const int SqMath::round2( const float x )
{
	return std::floor( x / 2.0 + 0.5 ) * 2;
}

// Floors to the nearest even integer
const int SqMath::floor2( const float x )
{
	return std::floor( x / 2.0 ) * 2;
}

// Ceils to the nearest even integer
const int SqMath::ceil2( const float x )
{
	return std::ceil( x / 2.0 ) * 2;
}

// Returns a fractional part of x
const float SqMath::fract( const float x )
{
	return modulo( x, 1.0 );
}

// Clamps x between min and max
const float SqMath::clamp( const float x, const float min, const float max )
{
	return std::clamp( x, min, max );
}

// Returns the smallest a or b
const float SqMath::min( const float a, const float b )
{
	return std::min( a, b );
}

// Returns the largest a or b
const float SqMath::max( const float a, const float b )
{
	return std::max( a, b );
}

// Returns a blend between a and b with using a mixing ratio x
const float SqMath::mix( const float a, const float b, const float x )
{
	return a * ( 1.0 - x ) + b * x;
}

// Returns a random const float in a range defined by min and max (inclusive)
const float SqMath::randomf( const float min, const float max )
{
	float a = std::min( min, max );
	float b = std::max( min, max );
	return a + dis_incl( rnd ) * (b - a);
}

// Returns a random integer in a range defined by min and max (inclusive)
const int SqMath::random( const float min, const float max )
{
	float a = std::min( min, max );
	float b = std::max( min, max );
	return std::floor( a + dis_excl( rnd ) * (b - a + 1.0) );
}

// Modulo with correct handling of negative numbers
const float SqMath::modulo( const float v, const float m )
{
	return m ? fmod( fmod(v, m) + m, m ) : 0.f;
}

// Return hypotenuse of x, y
const float SqMath::hypot( const float x, const float y )
{
	return std::hypot( x, y );
}

// Convert radians to degrees
const float SqMath::degrees( const float r )
{
	return 180.f * r / M_PI;
}

// Convert degrees to radians
const float SqMath::radians( const float d )
{
	return M_PI * d / 180.f;
}

// Return 2 raised to the power of x (more performant than `pow(2, x)`)
const float SqMath::exp2( const float x )
{
	return std::exp2f( x );
}

// Returns the base 2 logarithm of a number
const float SqMath::log2( const float x )
{
	return std::log2f( x );
}

// Returns the shortest *wrapped* distance between a and b
const float SqMath::short_dist( const float a, const float b, const float m )
{
	const float d = b - a;
	const float w = d + (d > 0 ? -m : m);
	return std::fabs(d) <= std::fabs(w) ? d : w;
}

// Mixes a and b by t, using shortest *wrapped* distance
const float SqMath::short_mix( const float a, const float b, const float m, const float t )
{
	return modulo(mix(a, a + short_dist(a, b, m), t), m);
}
