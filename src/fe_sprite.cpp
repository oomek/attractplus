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

	void fe_sprite_store_vertex( std::vector<FeRenderVertex> &out, const FeLocalVertex &vertex )
	{
		FeRenderVertex render_vertex = {};
		render_vertex.x = vertex.x;
		render_vertex.y = vertex.y;
		render_vertex.z = vertex.z;
		render_vertex.u = vertex.u;
		render_vertex.v = vertex.v;
		out.push_back( render_vertex );
	}

	void fe_sprite_store_triangle(
		std::vector<FeRenderVertex> &out,
		const FeLocalVertex &a,
		const FeLocalVertex &b,
		const FeLocalVertex &c )
	{
		fe_sprite_store_vertex( out, a );
		fe_sprite_store_vertex( out, b );
		fe_sprite_store_vertex( out, c );
	}

	void fe_sprite_store_quad(
		std::vector<FeRenderVertex> &out,
		const FeLocalVertex &top_left,
		const FeLocalVertex &bottom_left,
		const FeLocalVertex &top_right,
		const FeLocalVertex &bottom_right )
	{
		fe_sprite_store_triangle( out, top_left, bottom_left, top_right );
		fe_sprite_store_triangle( out, top_right, bottom_left, bottom_right );
	}
}

FeSprite::FeSprite()
	: m_vertices(),
	  m_textureRect( 0.f, 0.f, 0.f, 0.f ),
	  m_crop( 0.f, 0.f, 0.f, 0.f ),
	  m_border( 0, 0, 0, 0 ),
	  m_padding( 0, 0, 0, 0 ),
	  m_scale( 1.f, 1.f ),
	  m_position( 0.f, 0.f ),
	  m_origin( 0.f, 0.f, 0.f ),
	  m_color( Color::White ),
	  m_rotation_x( 0.f ),
	  m_rotation_y( 0.f ),
	  m_rotation_z( 0.f ),
	  m_rotation_order( 0 ),
	  m_border_scale( 1.f )
{
}

void FeSprite::setTextureRect( const FloatRect &rectangle )
{
	if ( rectangle != m_textureRect )
	{
		m_textureRect = rectangle;
		updateGeometry();
	}
}

const FloatRect &FeSprite::getTextureRect() const
{
	return m_textureRect;
}

void FeSprite::setColor( Color color )
{
	m_color = color;
}

Color FeSprite::getColor() const
{
	return m_color;
}

FloatRect FeSprite::getLocalBounds() const
{
	return FloatRect( 0, 0, std::abs( m_textureRect.size.x ), std::abs( m_textureRect.size.y ) );
}

void FeSprite::setCrop( FloatEdges crop )
{
	if ( crop != m_crop )
	{
		m_crop = crop;
		updateGeometry();
	}
}

FloatEdges FeSprite::getCrop() const
{
	return m_crop;
}

const IntEdges &FeSprite::getBorder() const
{
	return m_border;
}

void FeSprite::setBorder( const IntEdges &border )
{
	if ( border != m_border )
	{
		m_border = border;
		updateGeometry();
	}
}

const IntEdges &FeSprite::getPadding() const
{
	return m_padding;
}

void FeSprite::setPadding( const IntEdges &padding )
{
	if ( padding != m_padding )
	{
		m_padding = padding;
		updateGeometry();
	}
}

void FeSprite::setScale( const Vec2f &scale )
{
	if ( scale != m_scale )
	{
		m_scale = scale;
		updateGeometry();
	}
}

const Vec2f &FeSprite::getScale() const
{
	return m_scale;
}

void FeSprite::setPosition( const Vec2f &position )
{
	m_position = position;
}

const Vec2f &FeSprite::getPosition() const
{
	return m_position;
}

void FeSprite::setOrigin( const Vec3f &origin )
{
	m_origin = origin;
}

const Vec3f &FeSprite::getOrigin() const
{
	return m_origin;
}

void FeSprite::setRotation( float rotation )
{
	m_rotation_z = rotation;
}

float FeSprite::getRotation() const
{
	return m_rotation_z;
}

void FeSprite::setRotationX( float rotation )
{
	m_rotation_x = rotation;
}

float FeSprite::getRotationX() const
{
	return m_rotation_x;
}

void FeSprite::setRotationY( float rotation )
{
	m_rotation_y = rotation;
}

float FeSprite::getRotationY() const
{
	return m_rotation_y;
}

void FeSprite::setRotationOrder( int order )
{
	m_rotation_order = order;
}

int FeSprite::getRotationOrder() const
{
	return m_rotation_order;
}

void FeSprite::setBorderScale( float scale )
{
	if ( scale != m_border_scale )
	{
		m_border_scale = scale;
		updateGeometry();
	}
}

float FeSprite::getBorderScale() const
{
	return m_border_scale;
}

void FeSprite::append_render_vertices( std::vector<FeRenderVertex> &out, float zorder ) const
{
	out.clear();
	out.reserve( m_vertices.size() );

	const float radians_x = m_rotation_x * FE_PI / 180.0f;
	const float radians_y = m_rotation_y * FE_PI / 180.0f;
	const float radians_z = m_rotation_z * FE_PI / 180.0f;
	const float cos_x = std::cos( radians_x );
	const float sin_x = std::sin( radians_x );
	const float cos_y = std::cos( radians_y );
	const float sin_y = std::sin( radians_y );
	const float cos_z = std::cos( radians_z );
	const float sin_z = std::sin( radians_z );

	for ( const FeRenderVertex &vertex : m_vertices )
	{
		const float local_x = ( vertex.x - m_origin.x ) * m_scale.x;
		const float local_y = ( vertex.y - m_origin.y ) * m_scale.y;
		const float local_z = vertex.z - m_origin.z;

		FeRotationState rotated{
			local_x,
			local_y,
			local_z
		};

		switch ( m_rotation_order )
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

		FeRenderVertex render_vertex = {};
		render_vertex.x = rotated.x + m_position.x;
		render_vertex.y = rotated.y + m_position.y;
		render_vertex.z = zorder + rotated.z + m_origin.z;
		render_vertex.u = vertex.u;
		render_vertex.v = vertex.v;
		render_vertex.r = m_color.r;
		render_vertex.g = m_color.g;
		render_vertex.b = m_color.b;
		render_vertex.a = m_color.a;
		out.push_back( render_vertex );
	}
}

void FeSprite::updateGeometry()
{
	m_vertices.clear();

	const Vec2f tex_size( std::abs( m_textureRect.size.x ), std::abs( m_textureRect.size.y ) );
	if ( tex_size.x == 0.f || tex_size.y == 0.f )
		return;

	FloatEdges pos( 0.f, 0.f, tex_size.x, tex_size.y );
	FloatEdges tex(
		m_textureRect.position.x,
		m_textureRect.position.y,
		m_textureRect.position.x + m_textureRect.size.x,
		m_textureRect.position.y + m_textureRect.size.y );
	const Vec2f scale_abs( fe_sprite_safe_abs( m_scale.x ), fe_sprite_safe_abs( m_scale.y ) );
	const bool has_border =
		m_border.left || m_border.top || m_border.right || m_border.bottom;

	if ( !has_border )
	{
		const FloatEdges scaled_crop(
			m_crop.left / scale_abs.x,
			m_crop.top / scale_abs.y,
			m_crop.right / scale_abs.x,
			m_crop.bottom / scale_abs.y );
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
		static_cast<float>( m_padding.left ) / scale_abs.x,
		static_cast<float>( m_padding.top ) / scale_abs.y,
		static_cast<float>( m_padding.right ) / scale_abs.x,
		static_cast<float>( m_padding.bottom ) / scale_abs.y );
	pos.left -= scaled_padding.left;
	pos.top -= scaled_padding.top;
	pos.right = std::max( pos.left, pos.right + scaled_padding.right );
	pos.bottom = std::max( pos.top, pos.bottom + scaled_padding.bottom );

	const FeLocalVertex top_left{ pos.left, pos.top, 0.0f, tex.left, tex.top };
	const FeLocalVertex bottom_left{ pos.left, pos.bottom, 0.0f, tex.left, tex.bottom };
	const FeLocalVertex top_right{ pos.right, pos.top, 0.0f, tex.right, tex.top };
	const FeLocalVertex bottom_right{ pos.right, pos.bottom, 0.0f, tex.right, tex.bottom };

	if ( has_border )
	{
		const Vec2f total_size( pos.right - pos.left, pos.bottom - pos.top );
		FloatEdges scaled_border(
			static_cast<float>( m_border.left ) / scale_abs.x,
			static_cast<float>( m_border.top ) / scale_abs.y,
			static_cast<float>( m_border.right ) / scale_abs.x,
			static_cast<float>( m_border.bottom ) / scale_abs.y );

		float border_scale = m_border_scale;
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
		const float tx[4] = {
			tex.left,
			tex.left + ( m_border.left / tex_size.x ) * ( tex.right - tex.left ),
			tex.right - ( m_border.right / tex_size.x ) * ( tex.right - tex.left ),
			tex.right
		};
		const float ty[4] = {
			tex.top,
			tex.top + ( m_border.top / tex_size.y ) * ( tex.bottom - tex.top ),
			tex.bottom - ( m_border.bottom / tex_size.y ) * ( tex.bottom - tex.top ),
			tex.bottom
		};
		const float px[4] = {
			pos.left,
			pos.left + border.left,
			std::max( pos.left + border.left, pos.right - border.right ),
			pos.right
		};
		const float py[4] = {
			pos.top,
			pos.top + border.top,
			std::max( pos.top + border.top, pos.bottom - border.bottom ),
			pos.bottom
		};

		FeLocalVertex grid[4][4];
		for ( int yi = 0; yi < 4; ++yi )
		{
			for ( int xi = 0; xi < 4; ++xi )
			{
				grid[yi][xi] = FeLocalVertex{
					px[xi],
					py[yi],
					0.0f,
					tx[xi],
					ty[yi]
				};
			}
		}

		for ( int yi = 0; yi < 3; ++yi )
		{
			for ( int xi = 0; xi < 3; ++xi )
			{
				fe_sprite_store_quad(
					m_vertices,
					grid[yi][xi],
					grid[yi + 1][xi],
					grid[yi][xi + 1],
					grid[yi + 1][xi + 1] );
			}
		}
	}
	else
	{
		fe_sprite_store_quad( m_vertices, top_left, bottom_left, top_right, bottom_right );
	}
}
