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

#include "sq_vec2.hpp"

SqVec2::SqVec2():
	m_x( 0.0 ),
	m_y( 0.0 )
{}

SqVec2::SqVec2( const float x, const float y ):
	m_x( x ),
	m_y( y )
{}

SqVec2::SqVec2( const SqVec2 &p ):
	m_x( p.get_x() ),
	m_y( p.get_y() )
{}

SqVec2 SqVec2::operator+( const SqVec2 &p ) const
{
	return SqVec2( m_x + p.get_x(), m_y + p.get_y() );
}

SqVec2 SqVec2::operator-( const SqVec2 &p ) const
{
	return SqVec2( m_x - p.get_x(), m_y - p.get_y() );
}

SqVec2 SqVec2::operator*( const float &n ) const
{
	return SqVec2( m_x * n, m_y * n );
}

SqVec2 SqVec2::operator/( const float &n ) const
{
	return SqVec2( m_x / n, m_y / n );
}

const bool SqVec2::operator==( const SqVec2 &p ) const
{
	return m_x == p.get_x() && m_y == p.get_y();
}

const SqVec2 SqVec2::componentMul( SqVec2 &p ) const
{
	return SqVec2( m_x * p.get_x(), m_y * p.get_y() );
}

const SqVec2 SqVec2::componentDiv( SqVec2 &p ) const
{
	return SqVec2( m_x / p.get_x(), m_y / p.get_y() );
}

float SqVec2::get_len() const
{
	return std::hypot( m_x, m_y );
}

void SqVec2::set_len( const float n )
{
	SqVec2 p = SqVec2( n, get_angle() ).cartesian();
	m_x = p.get_x();
	m_y = p.get_y();
}

float SqVec2::get_angle() const
{
	return std::atan2( m_y, m_x );
}

void SqVec2::set_angle( const float r )
{
	SqVec2 p = SqVec2( get_len(), r ).cartesian();
	m_x = p.get_x();
	m_y = p.get_y();
}

const std::string SqVec2::_tostring() const
{
	return as_str( m_x ) + ", " + as_str( m_y );
}

const float SqVec2::distance( const SqVec2 &p ) const
{
	SqVec2 t = *this;
	return ( p - t ).get_len();
}

const float SqVec2::dot( const SqVec2 &p ) const
{
	return m_x * p.get_x() + m_y * p.get_y();
}

const float SqVec2::cross( const SqVec2 &p ) const
{
	return m_x * p.get_y() - m_y * p.get_x();
}

const SqVec2 SqVec2::mix( const SqVec2 &p, const float &n ) const
{
	return SqVec2( SqMath::mix( m_x, p.get_x(), n ), SqMath::mix( m_y, p.get_y(), n ) );
}

const SqVec2 SqVec2::invert() const
{
	return SqVec2( -m_x, -m_y );
}

const SqVec2 SqVec2::polar() const
{
	return SqVec2( get_len(), get_angle() );
}

const SqVec2 SqVec2::cartesian() const
{
	return SqVec2( std::cos( m_y ), std::sin( m_y ) ) * m_x;
}

const SqVec2 SqVec2::normalize() const
{
	SqVec2 t = *this;
	float n = get_len();
	return n ? t / n : SqVec2( 0, 0 );
}

const SqVec2 SqVec2::perpendicular() const
{
	return SqVec2( -m_y, m_x );
}

const float SqVec2::lengthSquared() const
{
	SqVec2 t = *this;
	return dot( t );
}

const SqVec2 SqVec2::projectedOnto( const SqVec2 &axis ) const
{
	return axis * ( dot(axis) / axis.lengthSquared() );
}

const float SqVec2::angleTo( const SqVec2 &p ) const
{
	return std::atan2( cross( p ), dot( p ) );
}
