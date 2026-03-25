/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-2026
 *
 *  This file is part of Attract-Mode.
 *
 *  Attract-Mode is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Attract-Mode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Attract-Mode.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "fe_sprite.hpp"

#include <algorithm>
#include <cmath>

namespace
{
	struct FeRotationState
	{
		float x;
		float y;
		float z;
	};

	struct FeLocalVertex
	{
		float x;
		float y;
		float z;
		float u;
		float v;
	};

	constexpr float FE_PI = 3.14159265358979323846f;

	float fe_sprite_safe_abs( float value )
	{
		const float magnitude = std::abs( value );
		return magnitude > 0.00001f ? magnitude : 1.0f;
	}

	FeLocalVertex fe_sprite_lerp( const FeLocalVertex &a, const FeLocalVertex &b, float t )
	{
		return FeLocalVertex{
			a.x + ( b.x - a.x ) * t,
			a.y + ( b.y - a.y ) * t,
			a.z + ( b.z - a.z ) * t,
			a.u + ( b.u - a.u ) * t,
			a.v + ( b.v - a.v ) * t
		};
	}

	FeLocalVertex fe_sprite_bilerp(
		const FeLocalVertex &top_left,
		const FeLocalVertex &top_right,
		const FeLocalVertex &bottom_left,
		const FeLocalVertex &bottom_right,
		float tx,
		float ty )
	{
		const FeLocalVertex top = fe_sprite_lerp( top_left, top_right, tx );
		const FeLocalVertex bottom = fe_sprite_lerp( bottom_left, bottom_right, tx );
		return fe_sprite_lerp( top, bottom, ty );
	}

	void fe_sprite_rotate_x( FeRotationState &value, float cos_x, float sin_x )
	{
		const float next_y = ( value.y * cos_x ) - ( value.z * sin_x );
		const float next_z = ( value.y * sin_x ) + ( value.z * cos_x );
		value.y = next_y;
		value.z = next_z;
	}

	void fe_sprite_rotate_y( FeRotationState &value, float cos_y, float sin_y )
	{
		const float next_x = ( value.x * cos_y ) + ( value.z * sin_y );
		const float next_z = -( value.x * sin_y ) + ( value.z * cos_y );
		value.x = next_x;
		value.z = next_z;
	}

	void fe_sprite_rotate_z( FeRotationState &value, float cos_z, float sin_z )
	{
		const float next_x = ( value.x * cos_z ) - ( value.y * sin_z );
		const float next_y = ( value.x * sin_z ) + ( value.y * cos_z );
		value.x = next_x;
		value.y = next_y;
	}

	FeRenderVertex fe_sprite_transform_vertex(
		const FeLocalVertex &vertex,
		const FeSpriteGeometry &geometry,
		float base_z )
	{
		const float local_x = ( vertex.x - geometry.origin.x ) * geometry.scale.x;
		const float local_y = ( vertex.y - geometry.origin.y ) * geometry.scale.y;
		const float local_z = vertex.z - geometry.origin.z;

		const float radians_x = geometry.rotation_x * FE_PI / 180.0f;
		const float radians_y = geometry.rotation_y * FE_PI / 180.0f;
		const float radians_z = geometry.rotation_z * FE_PI / 180.0f;

		const float cos_x = std::cos( radians_x );
		const float sin_x = std::sin( radians_x );
		const float cos_y = std::cos( radians_y );
		const float sin_y = std::sin( radians_y );
		const float cos_z = std::cos( radians_z );
		const float sin_z = std::sin( radians_z );

		FeRotationState rotated{ local_x, local_y, local_z };

		switch ( geometry.rotation_order )
		{
		default:
		case 0:
			fe_sprite_rotate_x( rotated, cos_x, sin_x );
			fe_sprite_rotate_y( rotated, cos_y, sin_y );
			fe_sprite_rotate_z( rotated, cos_z, sin_z );
			break;
		case 1:
			fe_sprite_rotate_x( rotated, cos_x, sin_x );
			fe_sprite_rotate_z( rotated, cos_z, sin_z );
			fe_sprite_rotate_y( rotated, cos_y, sin_y );
			break;
		case 2:
			fe_sprite_rotate_y( rotated, cos_y, sin_y );
			fe_sprite_rotate_x( rotated, cos_x, sin_x );
			fe_sprite_rotate_z( rotated, cos_z, sin_z );
			break;
		case 3:
			fe_sprite_rotate_y( rotated, cos_y, sin_y );
			fe_sprite_rotate_z( rotated, cos_z, sin_z );
			fe_sprite_rotate_x( rotated, cos_x, sin_x );
			break;
		case 4:
			fe_sprite_rotate_z( rotated, cos_z, sin_z );
			fe_sprite_rotate_x( rotated, cos_x, sin_x );
			fe_sprite_rotate_y( rotated, cos_y, sin_y );
			break;
		case 5:
			fe_sprite_rotate_z( rotated, cos_z, sin_z );
			fe_sprite_rotate_y( rotated, cos_y, sin_y );
			fe_sprite_rotate_x( rotated, cos_x, sin_x );
			break;
		}

		return FeRenderVertex{
			rotated.x + geometry.position.x,
			rotated.y + geometry.position.y,
			base_z + rotated.z + geometry.origin.z,
			vertex.u,
			vertex.v,
			geometry.color.r,
			geometry.color.g,
			geometry.color.b,
			geometry.color.a
		};
	}

	void fe_sprite_append_triangle(
		std::vector<FeRenderVertex> &out,
		const FeSpriteGeometry &geometry,
		float base_z,
		const FeLocalVertex &a,
		const FeLocalVertex &b,
		const FeLocalVertex &c )
	{
		out.push_back( fe_sprite_transform_vertex( a, geometry, base_z ) );
		out.push_back( fe_sprite_transform_vertex( b, geometry, base_z ) );
		out.push_back( fe_sprite_transform_vertex( c, geometry, base_z ) );
	}

	void fe_sprite_append_quad(
		std::vector<FeRenderVertex> &out,
		const FeSpriteGeometry &geometry,
		float base_z,
		const FeLocalVertex &top_left,
		const FeLocalVertex &bottom_left,
		const FeLocalVertex &top_right,
		const FeLocalVertex &bottom_right )
	{
		fe_sprite_append_triangle( out, geometry, base_z, top_left, bottom_left, top_right );
		fe_sprite_append_triangle( out, geometry, base_z, top_right, bottom_left, bottom_right );
	}
}

FeSpriteGeometry::FeSpriteGeometry()
	: texture_rect(),
	  crop(),
	  border(),
	  padding(),
	  scale( 1.f, 1.f ),
	  position( 0.f, 0.f ),
	  origin( 0.f, 0.f, 0.f ),
	  color( sf::Color::White ),
	  rotation_x( 0.f ),
	  rotation_y( 0.f ),
	  rotation_z( 0.f ),
	  rotation_order( 0 ),
	  border_scale( 1.f )
{
}

void fe_sprite_append_render_vertices(
	std::vector<FeRenderVertex> &out,
	const FeSpriteGeometry &geometry,
	float zorder )
{
	out.clear();

	const sf::Vector2f tex_size(
		std::abs( geometry.texture_rect.right - geometry.texture_rect.left ),
		std::abs( geometry.texture_rect.bottom - geometry.texture_rect.top ) );
	if ( tex_size.x == 0.f || tex_size.y == 0.f )
		return;

	FloatEdges pos( 0.f, 0.f, tex_size.x, tex_size.y );
	FloatEdges tex( geometry.texture_rect );
	const sf::Vector2f scale_abs( fe_sprite_safe_abs( geometry.scale.x ), fe_sprite_safe_abs( geometry.scale.y ) );
	const bool has_border =
		geometry.border.left || geometry.border.top || geometry.border.right || geometry.border.bottom;

	if ( !has_border )
	{
		const FloatEdges scaled_crop(
			geometry.crop.left / scale_abs.x,
			geometry.crop.top / scale_abs.y,
			geometry.crop.right / scale_abs.x,
			geometry.crop.bottom / scale_abs.y );
		pos.left += scaled_crop.left;
		pos.top += scaled_crop.top;
		pos.right -= scaled_crop.right;
		pos.bottom -= scaled_crop.bottom;
		tex.left += scaled_crop.left;
		tex.top += scaled_crop.top;
		tex.right -= scaled_crop.right;
		tex.bottom -= scaled_crop.bottom;
	}

	const FloatEdges scaled_padding(
		static_cast<float>( geometry.padding.left ) / scale_abs.x,
		static_cast<float>( geometry.padding.top ) / scale_abs.y,
		static_cast<float>( geometry.padding.right ) / scale_abs.x,
		static_cast<float>( geometry.padding.bottom ) / scale_abs.y );
	pos.left -= scaled_padding.left;
	pos.top -= scaled_padding.top;
	pos.right = std::max( pos.left, pos.right + scaled_padding.right );
	pos.bottom = std::max( pos.top, pos.bottom + scaled_padding.bottom );

	const FeLocalVertex top_left{
		pos.left,
		pos.top,
		0.0f,
		tex.left,
		tex.top
	};
	const FeLocalVertex bottom_left{
		pos.left,
		pos.bottom,
		0.0f,
		tex.left,
		tex.bottom
	};
	const FeLocalVertex top_right{
		pos.right,
		pos.top,
		0.0f,
		tex.right,
		tex.top
	};
	const FeLocalVertex bottom_right{
		pos.right,
		pos.bottom,
		0.0f,
		tex.right,
		tex.bottom
	};

	if ( has_border )
	{
		const sf::Vector2f total_size( pos.right - pos.left, pos.bottom - pos.top );
		FloatEdges scaled_border(
			static_cast<float>( geometry.border.left ) / scale_abs.x,
			static_cast<float>( geometry.border.top ) / scale_abs.y,
			static_cast<float>( geometry.border.right ) / scale_abs.x,
			static_cast<float>( geometry.border.bottom ) / scale_abs.y );

		float border_scale = geometry.border_scale;
		for ( int i = 0; i < 2; ++i )
		{
			border_scale *= std::min(
				total_size.x / std::max( ( scaled_border.left + scaled_border.right ) * border_scale, total_size.x ),
				total_size.y / std::max( ( scaled_border.top + scaled_border.bottom ) * border_scale, total_size.y ) );
		}

		const FloatEdges border(
			scaled_border.left * border_scale,
			scaled_border.top * border_scale,
			scaled_border.right * border_scale,
			scaled_border.bottom * border_scale );
		const sf::Vector2f grid_size(
			std::max( 1.0f, total_size.x ),
			std::max( 1.0f, total_size.y ) );
		const float tx[4] = {
			tex.left,
			tex.left + ( geometry.border.left / tex_size.x ) * ( tex.right - tex.left ),
			tex.right - ( geometry.border.right / tex_size.x ) * ( tex.right - tex.left ),
			tex.right
		};
		const float ty[4] = {
			tex.top,
			tex.top + ( geometry.border.top / tex_size.y ) * ( tex.bottom - tex.top ),
			tex.bottom - ( geometry.border.bottom / tex_size.y ) * ( tex.bottom - tex.top ),
			tex.bottom
		};
		const float fx[4] = {
			0.f,
			border.left / grid_size.x,
			1.f - ( border.right / grid_size.x ),
			1.f
		};
		const float fy[4] = {
			0.f,
			border.top / grid_size.y,
			1.f - ( border.bottom / grid_size.y ),
			1.f
		};

		FeLocalVertex grid[4][4];
		for ( int y = 0; y < 4; ++y )
		{
			for ( int x = 0; x < 4; ++x )
			{
				grid[y][x] = fe_sprite_bilerp( top_left, top_right, bottom_left, bottom_right, fx[x], fy[y] );
				grid[y][x].u = tx[x];
				grid[y][x].v = ty[y];
			}
		}

		out.reserve( 54 );
		for ( int y = 0; y < 3; ++y )
		{
			for ( int x = 0; x < 3; ++x )
			{
				fe_sprite_append_quad(
					out,
					geometry,
					zorder,
					grid[y][x],
					grid[y + 1][x],
					grid[y][x + 1],
					grid[y + 1][x + 1] );
			}
		}

		return;
	}

	out.reserve( 6 );
	fe_sprite_append_quad( out, geometry, zorder, top_left, bottom_left, top_right, bottom_right );
}
