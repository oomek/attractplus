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

#ifndef SQ_EASE_HPP
#define SQ_EASE_HPP

#define _USE_MATH_DEFINES
#include <cmath>

class SqEase
{
public:
	static float linear( float t, float b, float c, float d );

	static float in_quad( float t, float b, float c, float d );
	static float in_cubic( float t, float b, float c, float d );
	static float in_quart( float t, float b, float c, float d );
	static float in_quint( float t, float b, float c, float d );
	static float in_sine( float t, float b, float c, float d );
	static float in_expo( float t, float b, float c, float d );
	static float in_circ( float t, float b, float c, float d );
	static float in_elastic( float t, float b, float c, float d );
	static float in_elastic( float t, float b, float c, float d, float a, float p );
	static float in_back( float t, float b, float c, float d );
	static float in_back( float t, float b, float c, float d, float s );
	static float in_bounce( float t, float b, float c, float d );

	static float out_quad( float t, float b, float c, float d );
	static float out_cubic( float t, float b, float c, float d );
	static float out_quart( float t, float b, float c, float d );
	static float out_quint( float t, float b, float c, float d );
	static float out_sine( float t, float b, float c, float d );
	static float out_expo( float t, float b, float c, float d );
	static float out_circ( float t, float b, float c, float d );
	static float out_elastic( float t, float b, float c, float d );
	static float out_elastic( float t, float b, float c, float d, float a, float p );
	static float out_back( float t, float b, float c, float d );
	static float out_back( float t, float b, float c, float d, float s );
	static float out_bounce( float t, float b, float c, float d );

	static float in_out_quad( float t, float b, float c, float d );
	static float in_out_cubic( float t, float b, float c, float d );
	static float in_out_quart( float t, float b, float c, float d );
	static float in_out_quint( float t, float b, float c, float d );
	static float in_out_sine( float t, float b, float c, float d );
	static float in_out_expo( float t, float b, float c, float d );
	static float in_out_circ( float t, float b, float c, float d );
	static float in_out_elastic( float t, float b, float c, float d );
	static float in_out_elastic( float t, float b, float c, float d, float a, float p );
	static float in_out_back( float t, float b, float c, float d );
	static float in_out_back( float t, float b, float c, float d, float s );
	static float in_out_bounce( float t, float b, float c, float d );

	static float out_in_quad( float t, float b, float c, float d );
	static float out_in_cubic( float t, float b, float c, float d );
	static float out_in_quart( float t, float b, float c, float d );
	static float out_in_quint( float t, float b, float c, float d );
	static float out_in_sine( float t, float b, float c, float d );
	static float out_in_expo( float t, float b, float c, float d );
	static float out_in_circ( float t, float b, float c, float d );
	static float out_in_elastic( float t, float b, float c, float d );
	static float out_in_elastic( float t, float b, float c, float d, float a, float p );
	static float out_in_back( float t, float b, float c, float d );
	static float out_in_back( float t, float b, float c, float d, float s );
	static float out_in_bounce( float t, float b, float c, float d );
};

#endif
