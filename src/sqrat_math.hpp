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

#ifndef FE_SQRAT_MATH_HPP
#define FE_SQRAT_MATH_HPP

#include <cmath>
#include <algorithm>

int sq_math_sign( float x );
int sq_math_round( float x );
int sq_math_round2( float x );
int sq_math_floor2( float x );
int sq_math_ceil2( float x );
float sq_math_fract( float x );
float sq_math_clamp( float x, float min, float max );
float sq_math_min( float a, float b );
float sq_math_max( float a, float b );
float sq_math_mix( float a, float b, float x );
int sq_math_random( float min, float max );
float sq_math_modulo( float v, float m );

#endif
