/*
 *
 *  Attract-Mode Plus frontend
 *  Copyright (C) 2026 Radek Dutkiewicz
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

#ifndef FE_TRANSFORM_HPP
#define FE_TRANSFORM_HPP

#include "fe_types.hpp"

#include <cmath>

class FeTransform
{
public:
	constexpr FeTransform()
		: m00( 1.0f ),
		m01( 0.0f ),
		m02( 0.0f ),
		m10( 0.0f ),
		m11( 1.0f ),
		m12( 0.0f )
	{
	}

	static const FeTransform &identity()
	{
		static const FeTransform transform;
		return transform;
	}

	[[nodiscard]] bool isIdentity() const
	{
		return m00 == 1.0f && m01 == 0.0f && m02 == 0.0f
			&& m10 == 0.0f && m11 == 1.0f && m12 == 0.0f;
	}

	[[nodiscard]] Vec2f transformPoint( const Vec2f &point ) const
	{
		return Vec2f(
			( m00 * point.x ) + ( m01 * point.y ) + m02,
			( m10 * point.x ) + ( m11 * point.y ) + m12 );
	}

	FeTransform &translate( const Vec2f &offset )
	{
		return combine( FeTransform( 1.0f, 0.0f, offset.x, 0.0f, 1.0f, offset.y ) );
	}

	FeTransform &scale( const Vec2f &factors )
	{
		return combine( FeTransform( factors.x, 0.0f, 0.0f, 0.0f, factors.y, 0.0f ) );
	}

	FeTransform &rotate( float degrees )
	{
		const float radians = degrees * ( 3.14159265358979323846f / 180.0f );
		const float cosine = std::cos( radians );
		const float sine = std::sin( radians );
		return combine( FeTransform( cosine, -sine, 0.0f, sine, cosine, 0.0f ) );
	}

	FeTransform &rotate( float degrees, const Vec2f &center )
	{
		translate( center );
		rotate( degrees );
		translate( -center );
		return *this;
	}

	[[nodiscard]] FeTransform operator*( const FeTransform &other ) const
	{
		FeTransform result( *this );
		result.combine( other );
		return result;
	}

	FeTransform &operator*=( const FeTransform &other )
	{
		return combine( other );
	}

private:
	float m00;
	float m01;
	float m02;
	float m10;
	float m11;
	float m12;

	constexpr FeTransform( float a00, float a01, float a02, float a10, float a11, float a12 )
		: m00( a00 ),
		m01( a01 ),
		m02( a02 ),
		m10( a10 ),
		m11( a11 ),
		m12( a12 )
	{
	}

	FeTransform &combine( const FeTransform &other )
	{
		const float r00 = ( m00 * other.m00 ) + ( m01 * other.m10 );
		const float r01 = ( m00 * other.m01 ) + ( m01 * other.m11 );
		const float r02 = ( m00 * other.m02 ) + ( m01 * other.m12 ) + m02;
		const float r10 = ( m10 * other.m00 ) + ( m11 * other.m10 );
		const float r11 = ( m10 * other.m01 ) + ( m11 * other.m11 );
		const float r12 = ( m10 * other.m02 ) + ( m11 * other.m12 ) + m12;

		m00 = r00;
		m01 = r01;
		m02 = r02;
		m10 = r10;
		m11 = r11;
		m12 = r12;
		return *this;
	}
};

#endif
