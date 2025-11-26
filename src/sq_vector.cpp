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

#include "sq_vector.hpp"

SqVector::SqVector():
	m_x( 0.0 ),
	m_y( 0.0 )
{}

SqVector::SqVector( const float x, const float y ):
	m_x( x ),
	m_y( y )
{}

SqVector::SqVector( const SqVector &p ):
	m_x( p.get_x() ),
	m_y( p.get_y() )
{}

SqVector SqVector::operator+( const SqVector &p ) const
{
	return SqVector( m_x + p.get_x(), m_y + p.get_y() );
}

SqVector SqVector::operator-( const SqVector &p ) const
{
	return SqVector( m_x - p.get_x(), m_y - p.get_y() );
}

SqVector SqVector::operator*( const float &n ) const
{
	return SqVector( m_x * n, m_y * n );
}

SqVector SqVector::operator/( const float &n ) const
{
	return SqVector( m_x / n, m_y / n );
}

const bool SqVector::operator==( const SqVector &p ) const
{
	return m_x == p.get_x() && m_y == p.get_y();
}

const SqVector SqVector::componentMul( SqVector &p ) const
{
	return SqVector( m_x * p.get_x(), m_y * p.get_y() );
}

const SqVector SqVector::componentDiv( SqVector &p ) const
{
	return SqVector( m_x / p.get_x(), m_y / p.get_y() );
}

float SqVector::get_len() const
{
	return std::hypot( m_x, m_y );
}

void SqVector::set_len( const float n )
{
	SqVector p = SqVector( n, get_angle() ).cartesian();
	m_x = p.get_x();
	m_y = p.get_y();
}

float SqVector::get_angle() const
{
	return std::atan2( m_y, m_x );
}

void SqVector::set_angle( const float r )
{
	SqVector p = SqVector( get_len(), r ).cartesian();
	m_x = p.get_x();
	m_y = p.get_y();
}

const std::string SqVector::_tostring() const
{
	return as_str( m_x ) + ", " + as_str( m_y );
}

const float SqVector::distance( const SqVector &p ) const
{
	SqVector t = *this;
	return ( p - t ).get_len();
}

const float SqVector::dot( const SqVector &p ) const
{
	return m_x * p.get_x() + m_y * p.get_y();
}

const float SqVector::cross( const SqVector &p ) const
{
	return m_x * p.get_y() - m_y * p.get_x();
}

const SqVector SqVector::mix( const SqVector &p, const float &n ) const
{
	return SqVector( SqMath::mix( m_x, p.get_x(), n ), SqMath::mix( m_y, p.get_y(), n ) );
}

const SqVector SqVector::polar() const
{
	return SqVector( get_len(), get_angle() );
}

const SqVector SqVector::cartesian() const
{
	return SqVector( std::cos( m_y ), std::sin( m_y ) ) * m_x;
}

const SqVector SqVector::normalize() const
{
	SqVector t = *this;
	float n = get_len();
	return n ? t / n : SqVector( 0, 0 );
}

const SqVector SqVector::perpendicular() const
{
	return SqVector( -m_y, m_x );
}

const float SqVector::lengthSquared() const
{
	SqVector t = *this;
	return dot( t );
}

const SqVector SqVector::projectedOnto( const SqVector &axis ) const
{
	return axis * ( dot(axis) / axis.lengthSquared() );
}

const float SqVector::angleTo( const SqVector &p ) const
{
	return std::atan2( cross( p ), dot( p ) );
}
