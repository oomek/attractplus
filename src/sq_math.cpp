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
	std::uniform_real_distribution<double> dis( 0.0, 1.0 + std::numeric_limits<double>::epsilon() );
}

// Returns 1 when x > 0, returns -1 when x < 0, returns 0 when x == 0
int SqMath::sign( float x )
{
	return (x > 0.f) ? 1 : ((x < 0.f) ? -1 : 0);
}

// Rounds up to the nearest integer
int SqMath::round( float x )
{
	return std::floor( x + 0.5 );
}

// Rounds up to the nearest even integer
int SqMath::round2( float x )
{
	return std::floor( x / 2.0 + 0.5 ) * 2;
}

// Floors to the nearest even integer
int SqMath::floor2( float x )
{
	return std::floor( x / 2.0 ) * 2;
}

// Ceils to the nearest even integer
int SqMath::ceil2( float x )
{
	return std::ceil( x / 2.0 ) * 2;
}

// Returns a fractional part of x
float SqMath::fract( float x )
{
	return modulo( x, 1.0 );
}

// Clamps x between min and max
float SqMath::clamp( float x, float min, float max )
{
	return std::clamp( x, min, max );
}

// Returns the smallest a or b
float SqMath::min( float a, float b )
{
	return std::min( a, b );
}

// Returns the largest a or b
float SqMath::max( float a, float b )
{
	return std::max( a, b );
}

// Returns a blend between a and b with using a mixing ratio x
float SqMath::mix( float a, float b, float x )
{
	return a * ( 1.0 - x ) + b * x;
}

// Returns a random float in a range defined by min and max (inclusive)
float SqMath::randomf( float min, float max )
{
	return min + dis( rnd ) * (max - min);
}

// Returns a random integer in a range defined by min and max (inclusive)
int SqMath::random( float min, float max )
{
	return std::floor( randomf( min, max ) );
}

// Modulo with correct handling of negative numbers
float SqMath::modulo( float v, float m )
{
	return m ? fmod( fmod(v, m) + m, m ) : 0.f;
}

// Return hypotenuse of x, y
float SqMath::hypot( float x, float y )
{
	return std::hypot( x, y );
}

// Convert radians to degrees
float SqMath::degrees( float r )
{
	return 180.f * r / M_PI;
}

// Convert degrees to radians
float SqMath::radians( float d )
{
	return M_PI * d / 180.f;
}

// Return 2 raised to the power of x (more performant than `pow(2, x)`)
float SqMath::exp2( float x )
{
	return std::exp2f( x );
}

// Returns the base 2 logarithm of a number
float SqMath::log2( float x )
{
	return std::log2f( x );
}
