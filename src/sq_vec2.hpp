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

#ifndef SQ_VEC2_HPP
#define SQ_VEC2_HPP

#include "fe_util.hpp"
#include "sq_math.hpp"

class SqVec2
{
public:
	SqVec2();
	SqVec2( const float, const float );
	SqVec2( const SqVec2 & );

	float get_x() const { return m_x; };
	void set_x( const float x ) { m_x = x; };

	float get_y() const { return m_y; };
	void set_y( const float y ) { m_y = y; };

	float get_len() const;
	void set_len( const float );

	float get_angle() const;
	void set_angle( const float );

	SqVec2 operator+( const SqVec2 & ) const;
	SqVec2 operator-( const SqVec2 & ) const;
	SqVec2 operator*( const float & ) const;
	SqVec2 operator/( const float & ) const;
	const bool operator==( const SqVec2 & ) const;

	const std::string _tostring() const;
	const SqVec2 componentMul( SqVec2 & ) const;
	const SqVec2 componentDiv( SqVec2 & ) const;
	const SqVec2 invert() const;
	const SqVec2 polar() const;
	const SqVec2 cartesian() const;
	const SqVec2 normalize() const;
	const SqVec2 perpendicular() const;
	const SqVec2 projectedOnto( const SqVec2 & ) const;
	const SqVec2 mix( const SqVec2 &, const float & ) const;
	const float lengthSquared() const;
	const float angleTo( const SqVec2 & ) const;
	const float distance( const SqVec2 & ) const;
	const float dot( const SqVec2 & ) const;
	const float cross( const SqVec2 & ) const;

private:
	float m_x;
	float m_y;
};

#endif
