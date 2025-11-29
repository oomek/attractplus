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

#ifndef SQ_VECTOR_HPP
#define SQ_VECTOR_HPP

#include "fe_util.hpp"
#include "sq_math.hpp"

class SqVector
{
public:
	SqVector();
	SqVector( const float, const float );
	SqVector( const SqVector & );

	float get_x() const { return m_x; };
	void set_x( const float x ) { m_x = x; };

	float get_y() const { return m_y; };
	void set_y( const float y ) { m_y = y; };

	float get_len() const;
	void set_len( const float );

	float get_angle() const;
	void set_angle( const float );

	SqVector operator+( const SqVector & ) const;
	SqVector operator-( const SqVector & ) const;
	SqVector operator*( const float & ) const;
	SqVector operator/( const float & ) const;
	const bool operator==( const SqVector & ) const;

	const std::string _tostring() const;
	const SqVector componentMul( SqVector & ) const;
	const SqVector componentDiv( SqVector & ) const;
	const SqVector polar() const;
	const SqVector cartesian() const;
	const SqVector normalize() const;
	const SqVector perpendicular() const;
	const SqVector projectedOnto( const SqVector & ) const;
	const SqVector mix( const SqVector &, const float & ) const;
	const float lengthSquared() const;
	const float angleTo( const SqVector & ) const;
	const float distance( const SqVector & ) const;
	const float dot( const SqVector & ) const;
	const float cross( const SqVector & ) const;

private:
	float m_x;
	float m_y;
};

#endif
