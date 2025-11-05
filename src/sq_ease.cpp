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

/*
	Penner easings
	http://robertpenner.com/easing/

	- All algorithms updated to shift post increments outside the expression
	- All algorithms updated to use their in/out inverse (rather than inlining inverse math)
	- Lots of MAGIC numbers in here...

	Post increment in C++ is not guaranteed to occur inline - it may update AFTER the expression
	https://stackoverflow.com/questions/4445706/post-increment-and-pre-increment-concept

	Inline multiplication is faster than pow?
	https://stackoverflow.com/questions/2940367/what-is-more-efficient-using-pow-to-square-or-just-multiply-it-with-itself
 */

#include "sq_ease.hpp"

// -------------------------------------------------------------------------------------

float SqEase::linear( float t, float b, float c, float d )
{
	t=t/d;
	return c*t + b;
}

// -------------------------------------------------------------------------------------

float SqEase::in_quad( float t, float b, float c, float d )
{
	t=t/d;
	return c*t*t + b;
}

float SqEase::in_cubic( float t, float b, float c, float d )
{
	t=t/d;
	return c*t*t*t + b;
}

float SqEase::in_quart( float t, float b, float c, float d )
{
	t/=d;
	return c*t*t*t*t + b;
}

float SqEase::in_quint( float t, float b, float c, float d )
{
	t/=d;
	return c*t*t*t*t*t + b;
}

float SqEase::in_sine( float t, float b, float c, float d )
{
	return -c*cos(t/d*(M_PI/2)) + c + b;
}

float SqEase::in_expo( float t, float b, float c, float d )
{
	return (t==0) ? b : c*pow(2, 10*(t/d - 1)) + b;
}

float SqEase::in_circ( float t, float b, float c, float d )
{
	t/=d;
	return -c*(sqrt(1 - t*t) - 1) + b;
}

float SqEase::in_elastic( float t, float b, float c, float d )
{
	return in_elastic( t, b, c, d, 0.0, d*0.3 );
}

float SqEase::in_elastic( float t, float b, float c, float d, float a, float p )
{
	float s;
	if (t==0) return b;
	t/=d;
	if (t==1) return b+c;
	if (a < fabs(c)) { a=c; s=p/4; } else { s = p/(2*M_PI)*asin(c/a); }
	t-=1;
	return -(a*pow(2, 10*t)*sin((t*d-s)*(2*M_PI)/p)) + b;
}

float SqEase::in_back( float t, float b, float c, float d )
{
	return in_back( t, b, c, d, 1.70158 );
}

float SqEase::in_back( float t, float b, float c, float d, float s )
{
	t/=d;
	return c*t*t*((s+1)*t - s) + b;
}

float SqEase::in_bounce( float t, float b, float c, float d )
{
	return out_bounce(d-t, b+c, -c, d);
}

// -------------------------------------------------------------------------------------

float SqEase::out_quad( float t, float b, float c, float d )
{
	return in_quad(d-t, b+c, -c, d);
}

float SqEase::out_cubic( float t, float b, float c, float d )
{
	return in_cubic(d-t, b+c, -c, d);
}

float SqEase::out_quart( float t, float b, float c, float d )
{
	return in_quart(d-t, b+c, -c, d);
}

float SqEase::out_quint( float t, float b, float c, float d )
{
	return in_quint(d-t, b+c, -c, d);
}

float SqEase::out_sine( float t, float b, float c, float d )
{
	return in_sine(d-t, b+c, -c, d);
}

float SqEase::out_expo( float t, float b, float c, float d )
{
	return in_expo(d-t, b+c, -c, d);
}

float SqEase::out_circ( float t, float b, float c, float d )
{
	return in_circ(d-t, b+c, -c, d);
}

float SqEase::out_elastic( float t, float b, float c, float d )
{
	return in_elastic(d-t, b+c, -c, d);
}

float SqEase::out_elastic( float t, float b, float c, float d, float a, float p )
{
	return in_elastic(d-t, b+c, -c, d, a, p);
}

float SqEase::out_back( float t, float b, float c, float d )
{
	return in_back(d-t, b+c, -c, d);
}

float SqEase::out_back( float t, float b, float c, float d, float s )
{
	return in_back(d-t, b+c, -c, d, s);
}

float SqEase::out_bounce( float t, float b, float c, float d )
{
	t/=d;
	if (t < (1.0/2.75)) { return c*(7.5625*t*t) + b; }
	if (t < (2.0/2.75)) { t-=1.5/2.75; return c*(7.5625*t*t + 0.75) + b; }
	if (t < (2.5/2.75)) { t-=2.25/2.75; return c*(7.5625*t*t + 0.9375) + b; }
	t-=2.625/2.75;
	return c*(7.5625*t*t + 0.984375) + b;
}

// -------------------------------------------------------------------------------------

float SqEase::in_out_quad(float t, float b, float c, float d )
{
	return (t < d/2) ? in_quad(t, b, c/2, d/2) : in_quad(d-t, b+c, -c/2, d/2);
}

float SqEase::in_out_cubic( float t, float b, float c, float d )
{
	return (t < d/2) ? in_cubic(t, b, c/2, d/2) : in_cubic(d-t, b+c, -c/2, d/2);
}

float SqEase::in_out_quart( float t, float b, float c, float d )
{
	return (t < d/2) ? in_quart(t, b, c/2, d/2) : in_quart(d-t, b+c, -c/2, d/2);
}

float SqEase::in_out_quint( float t, float b, float c, float d )
{
	return (t < d/2) ? in_quint(t, b, c/2, d/2) : in_quint(d-t, b+c, -c/2, d/2);
}

float SqEase::in_out_sine( float t, float b, float c, float d )
{
	return (t < d/2) ? in_sine(t, b, c/2, d/2) : in_sine(d-t, b+c, -c/2, d/2);
}

float SqEase::in_out_expo( float t, float b, float c, float d )
{
	return (t < d/2) ? in_expo(t, b, c/2, d/2) : in_expo(d-t, b+c, -c/2, d/2);
}

float SqEase::in_out_circ( float t, float b, float c, float d )
{
	return (t < d/2) ? in_circ(t, b, c/2, d/2) : in_circ(d-t, b+c, -c/2, d/2);
}

float SqEase::in_out_elastic( float t, float b, float c, float d )
{
	return (t < d/2) ? in_elastic(t, b, c/2, d/2) : in_elastic(d-t, b+c, -c/2, d/2);
}

float SqEase::in_out_elastic( float t, float b, float c, float d, float a, float p )
{
	return (t < d/2) ? in_elastic(t, b, c/2, d/2, a, p) : in_elastic(d-t, b+c, -c/2, d/2, a, p);
}

float SqEase::in_out_back( float t, float b, float c, float d )
{
	return (t < d/2) ? in_back(t, b, c/2, d/2) : in_back(d-t, b+c, -c/2, d/2);
}

float SqEase::in_out_back( float t, float b, float c, float d, float s )
{
	return (t < d/2) ? in_back(t, b, c/2, d/2, s) : in_back(d-t, b+c, -c/2, d/2, s);
}

float SqEase::in_out_bounce( float t, float b, float c, float d )
{
	return (t < d/2) ? in_bounce(t, b, c/2, d/2) : in_bounce(d-t, b+c, -c/2, d/2);
}

// -------------------------------------------------------------------------------------

float SqEase::out_in_quad( float t, float b, float c, float d )
{
	return (t < d/2) ? out_quad(t, b, c/2, d/2) : out_quad(d-t, b+c, -c/2, d/2);
}

float SqEase::out_in_cubic( float t, float b, float c, float d )
{
	return (t < d/2) ? out_cubic(t, b, c/2, d/2) : out_cubic(d-t, b+c, -c/2, d/2);
}

float SqEase::out_in_quart( float t, float b, float c, float d )
{
	return (t < d/2) ? out_quart(t, b, c/2, d/2) : out_quart(d-t, b+c, -c/2, d/2);
}

float SqEase::out_in_quint( float t, float b, float c, float d )
{
	return (t < d/2) ? out_quint(t, b, c/2, d/2) : out_quint(d-t, b+c, -c/2, d/2);
}

float SqEase::out_in_sine( float t, float b, float c, float d )
{
	return (t < d/2) ? out_sine(t, b, c/2, d/2) : out_sine(d-t, b+c, -c/2, d/2);
}

float SqEase::out_in_expo( float t, float b, float c, float d )
{
	return (t < d/2) ? out_expo(t, b, c/2, d/2) : out_expo(d-t, b+c, -c/2, d/2);
}

float SqEase::out_in_circ( float t, float b, float c, float d )
{
	return (t < d/2) ? out_circ(t, b, c/2, d/2) : out_circ(d-t, b+c, -c/2, d/2);
}

float SqEase::out_in_elastic( float t, float b, float c, float d )
{
	return (t < d/2) ? out_elastic(t, b, c/2, d/2) : out_elastic(d-t, b+c, -c/2, d/2);
}

float SqEase::out_in_elastic( float t, float b, float c, float d, float a, float p )
{
	return (t < d/2) ? out_elastic(t, b, c/2, d/2, a, p) : out_elastic(d-t, b+c, -c/2, d/2, a, p);
}

float SqEase::out_in_back( float t, float b, float c, float d )
{
	return (t < d/2) ? out_back(t, b, c/2, d/2) : out_back(d-t, b+c, -c/2, d/2);
}

float SqEase::out_in_back( float t, float b, float c, float d, float s )
{
	return (t < d/2) ? out_back(t, b, c/2, d/2, s) : out_back(d-t, b+c, -c/2, d/2, s);
}

float SqEase::out_in_bounce( float t, float b, float c, float d )
{
	return (t < d/2) ? out_bounce(t, b, c/2, d/2) : out_bounce(d-t, b+c, -c/2, d/2);
}
