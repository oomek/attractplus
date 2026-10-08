#ifndef FE_GRID_HPP
#define FE_GRID_HPP

#include "fe_types.hpp"
#include <algorithm>

// Conversion between authored grid coordinates and a parent's pixel space.
struct FeGridTransform
{
	Vec2f scale;
	Vec2f offset;

	Vec2f position( const Vec2f &p ) const
	{
		return Vec2f( p.x * scale.x + offset.x, p.y * scale.y + offset.y );
	}
	Vec2f size( const Vec2f &s ) const
	{
		return Vec2f( s.x * scale.x, s.y * scale.y );
	}
	Vec2f inverse_position( const Vec2f &p ) const
	{
		return inverse_size( p - offset );
	}
	Vec2f inverse_size( const Vec2f &s ) const
	{
		return Vec2f( s.x / scale.x, s.y / scale.y );
	}
};

struct FeGrid
{
	Vec2f size = Vec2f( 0, 0 );
	Vec2f offset = Vec2f( 0, 0 );
	bool uniform = false;
	bool center = false;
	bool pixel_snap = false;

	FeGridTransform transform( const Vec2f &extent ) const
	{
		FeGridTransform result = { Vec2f( 1, 1 ), Vec2f( 0, 0 ) };
		if ( size.x > 0 && size.y > 0 && extent.x > 0 && extent.y > 0 )
		{
			result.scale = Vec2f( extent.x / size.x, extent.y / size.y );
			if ( uniform )
			{
				// Keep half-pixel centers exact before converting to float geometry.
				double unit = std::min( static_cast<double>( extent.x ) / size.x,
					static_cast<double>( extent.y ) / size.y );
				if ( center )
					result.offset = Vec2f(
						result.scale.x <= result.scale.y ? 0 : ( extent.x - size.x * unit ) / 2.0,
						result.scale.y <= result.scale.x ? 0 : ( extent.y - size.y * unit ) / 2.0 );
				result.scale = Vec2f( unit, unit );
			}
		}
		result.offset -= result.size( offset );
		return result;
	}
};

#endif
