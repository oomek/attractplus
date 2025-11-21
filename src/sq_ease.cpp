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

// SPECIAL: Inertia expo
float SqEase::in_expo2( float t, float b, float c, float d )
{
	if (t==0) return b;
	t/=d;
	float x = t;
	x *= x;
	x *= x;
	return c*x*(x + 0.3*(1.0 - t)) + b;
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

// SPECIAL: Inertia elastic
float SqEase::in_elastic2( float t, float b, float c, float d, float p )
{
	t/=d;
	float i = t*t;
	float e = i*i;
	e *= e;
	e += 0.1 * ( i - e );
	return c * e * std::cos((1.0-t) * (1.0/p+1.0) * (2.0-e) * M_PI) + b;
}

float SqEase::in_elastic2( float t, float b, float c, float d )
{
	return in_elastic2( t, b, c, d, 0.3 );
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

// SPECIAL: Inertia back
float SqEase::in_back2( float t, float b, float c, float d )
{
	t/=d;
	return c*t*t*t*(t*3.0-2.0)*(2.0-t) + b;
}

float SqEase::in_bounce( float t, float b, float c, float d )
{
	return out_bounce(d-t, b+c, -c, d);
}

float SqEase::in_bounce2( float t, float b, float c, float d, float p )
{
	return out_bounce2(d-t, b+c, -c, d, p);
}

float SqEase::in_bounce2( float t, float b, float c, float d )
{
	return out_bounce2(d-t, b+c, -c, d);
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

float SqEase::out_expo2( float t, float b, float c, float d )
{
	return in_expo2(d-t, b+c, -c, d);
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

float SqEase::out_elastic2( float t, float b, float c, float d, float p )
{
	return in_elastic2(d-t, b+c, -c, d, p);
}

float SqEase::out_elastic2( float t, float b, float c, float d )
{
	return in_elastic2(d-t, b+c, -c, d);
}

float SqEase::out_back( float t, float b, float c, float d )
{
	return in_back(d-t, b+c, -c, d);
}

float SqEase::out_back( float t, float b, float c, float d, float s )
{
	return in_back(d-t, b+c, -c, d, s);
}

float SqEase::out_back2( float t, float b, float c, float d )
{
	return in_back2(d-t, b+c, -c, d);
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

float SqEase::out_bounce2( float t, float b, float c, float d, float p )
{
	p=1.0/p;
	t=t/d*(1.0+p);
	float n = 2.0/p + 1.0;
	float w = t - p - 1.0;
	float x = (n - (n * t) + t + 1.0) * 0.5;
	float y = std::pow(n, std::floor(std::log(x)/std::log(n)));
	return c * (1.0 + (y*p + w) * (y*(p+2.0) + w)) + b;
}

float SqEase::out_bounce2( float t, float b, float c, float d )
{
	return out_bounce2( t, b, c, d, 0.5 );
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

float SqEase::in_out_expo2( float t, float b, float c, float d )
{
	return (t < d/2) ? in_expo2(t, b, c/2, d/2) : in_expo2(d-t, b+c, -c/2, d/2);
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

float SqEase::in_out_elastic2( float t, float b, float c, float d, float p )
{
	return (t < d/2) ? in_elastic2(t, b, c/2, d/2, p) : in_elastic2(d-t, b+c, -c/2, d/2, p);
}

float SqEase::in_out_elastic2( float t, float b, float c, float d )
{
	return (t < d/2) ? in_elastic2(t, b, c/2, d/2) : in_elastic2(d-t, b+c, -c/2, d/2);
}

float SqEase::in_out_back( float t, float b, float c, float d )
{
	return (t < d/2) ? in_back(t, b, c/2, d/2) : in_back(d-t, b+c, -c/2, d/2);
}

float SqEase::in_out_back( float t, float b, float c, float d, float s )
{
	return (t < d/2) ? in_back(t, b, c/2, d/2, s) : in_back(d-t, b+c, -c/2, d/2, s);
}

float SqEase::in_out_back2( float t, float b, float c, float d )
{
	return (t < d/2) ? in_back2(t, b, c/2, d/2) : in_back2(d-t, b+c, -c/2, d/2);
}

float SqEase::in_out_bounce( float t, float b, float c, float d )
{
	return (t < d/2) ? in_bounce(t, b, c/2, d/2) : in_bounce(d-t, b+c, -c/2, d/2);
}

float SqEase::in_out_bounce2( float t, float b, float c, float d, float p )
{
	return (t < d/2) ? in_bounce2(t, b, c/2, d/2, p) : in_bounce2(d-t, b+c, -c/2, d/2, p);
}

float SqEase::in_out_bounce2( float t, float b, float c, float d )
{
	return (t < d/2) ? in_bounce2(t, b, c/2, d/2 ) : in_bounce2(d-t, b+c, -c/2, d/2 );
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

float SqEase::out_in_expo2( float t, float b, float c, float d )
{
	return (t < d/2) ? out_expo2(t, b, c/2, d/2) : out_expo2(d-t, b+c, -c/2, d/2);
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

float SqEase::out_in_elastic2( float t, float b, float c, float d, float p )
{
	return (t < d/2) ? out_elastic2(t, b, c/2, d/2, p) : out_elastic2(d-t, b+c, -c/2, d/2, p);
}

float SqEase::out_in_elastic2( float t, float b, float c, float d )
{
	return (t < d/2) ? out_elastic2(t, b, c/2, d/2) : out_elastic2(d-t, b+c, -c/2, d/2);
}

float SqEase::out_in_back( float t, float b, float c, float d )
{
	return (t < d/2) ? out_back(t, b, c/2, d/2) : out_back(d-t, b+c, -c/2, d/2);
}

float SqEase::out_in_back( float t, float b, float c, float d, float s )
{
	return (t < d/2) ? out_back(t, b, c/2, d/2, s) : out_back(d-t, b+c, -c/2, d/2, s);
}

float SqEase::out_in_back2( float t, float b, float c, float d )
{
	return (t < d/2) ? out_back2(t, b, c/2, d/2) : out_back2(d-t, b+c, -c/2, d/2);
}

float SqEase::out_in_bounce( float t, float b, float c, float d )
{
	return (t < d/2) ? out_bounce(t, b, c/2, d/2) : out_bounce(d-t, b+c, -c/2, d/2);
}

float SqEase::out_in_bounce2( float t, float b, float c, float d, float p )
{
	return (t < d/2) ? out_bounce2(t, b, c/2, d/2, p) : out_bounce2(d-t, b+c, -c/2, d/2, p);
}

float SqEase::out_in_bounce2( float t, float b, float c, float d )
{
	return (t < d/2) ? out_bounce2(t, b, c/2, d/2) : out_bounce2(d-t, b+c, -c/2, d/2);
}

float SqEase::steps( float t, float b, float c, float d, float s )
{
	return steps( t, b, c, d, s, JumpEnd );
}

float SqEase::steps( float t, float b, float c, float d, float s, int jump )
{
	if (t==d) return b+c;
	bool f = ( jump == JumpStart || jump == JumpBoth );
	bool e = ( jump == JumpEnd || jump == JumpBoth );
	float r = c / (s + (f&&e));
	return (c-(f+e)*r) * floor(t/d*s) / std::max(1.0, s-1.0) + (b+f*r);
}

// https://en.wikipedia.org/wiki/B%C3%A9zier_curve
// https://developer.mozilla.org/en-US/docs/Web/CSS/easing-function#cubic_b%C3%A9zier_easing_function
// https://github.com/gre/bezier-easing/blob/master/src/index.js
// https://cubic-bezier.com

const int NEWTON_ITERATIONS = 4;
const float NEWTON_MIN_SLOPE = 0.001;
const float SUBDIVISION_PRECISION = 0.0000001;
const int SUBDIVISION_MAX_ITERATIONS = 10;

const int K_SPLINE_TABLE_SIZE = 11;
const int K_SPLINE_TABLE_LAST = K_SPLINE_TABLE_SIZE - 1;
const float K_SAMPLE_STEP_SIZE = 1.0 / (K_SPLINE_TABLE_SIZE - 1.0);

namespace
{
	float bezA( float a, float b )
	{
		return 1.0 - 3.0 * b + 3.0 * a;
	}

	float bezB( float a, float b )
	{
		return 3.0 * b - 6.0 * a;
	}

	float bezC( float a)
	{
		return 3.0 * a;
	}

	float get_bezier( float t, float a, float b )
	{
		return ((bezA(a,b)*t + bezB(a,b))*t + bezC(a))*t;
	}

	float get_slope( float t, float a, float b)
	{
		return 3.0*bezA(a,b)*t*t + 2.0*bezB(a,b)*t + bezC(a);
	}

	float binary_subdivide( float x, float a, float b, float x1, float x2 )
	{
		float n = 0.f, t = 0.f;
		int i = 0;
		do
		{
			t = a + (b - a) / 2.0;
			n = get_bezier(t, x1, x2) - x;
			if (n > 0.0) { b = t; } else { a = t; }
		} while ((fabs(n) > SUBDIVISION_PRECISION) && (++i < SUBDIVISION_MAX_ITERATIONS));
		return t;
	}

	float newton_raphson_iterate( float x, float t, float x1, float x2 )
	{
		for ( int i = 0; i < NEWTON_ITERATIONS; i++ ) {
			float n = get_slope(t, x1, x2);
			if (n == 0.0) return t;
			t -= (get_bezier(t, x1, x2) - x) / n;
		}
		return t;
	}
}

float SqEase::cubic_bezier( float t, float b, float c, float d, float x1, float y1, float x2, float y2 )
{
	x1 = std::clamp( x1, 0.f, 1.f );
	x2 = std::clamp( x2, 0.f, 1.f );

	if (x1 == y1 && x2 == y2)
		return linear( t, b, c, d);

	std::vector<float> sample_values = {};
	for ( int i = 0; i < K_SPLINE_TABLE_SIZE; i++ )
		sample_values.push_back( get_bezier(i * K_SAMPLE_STEP_SIZE, x1, x2) );

	float xt;
	float x = t / d;
	float interval_start = 0.0;
	float current_sample = 1;

	for ( ; current_sample != K_SPLINE_TABLE_LAST && sample_values[current_sample] <= x; current_sample++ )
		interval_start += K_SAMPLE_STEP_SIZE;

	current_sample--;

	// Interpolate to provide an initial guess for t
	float dist = ( x - sample_values[current_sample] ) / ( sample_values[current_sample + 1] - sample_values[current_sample] );
	float t_guess = interval_start + dist * K_SAMPLE_STEP_SIZE;
	float slope = get_slope(t_guess, x1, x2);

	if ( slope >= NEWTON_MIN_SLOPE ) {
		xt = newton_raphson_iterate( x, t_guess, x1, x2 );
	} else if ( slope == 0.0 ) {
		xt = t_guess;
	} else {
		xt = binary_subdivide( x, interval_start, interval_start + K_SAMPLE_STEP_SIZE, x1, x2 );
	}

	return c * get_bezier( xt, y1, y2 ) + b;
}
