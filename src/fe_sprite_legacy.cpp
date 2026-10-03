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

#include "fe_sprite_legacy.hpp"

#include <algorithm>
#include <cmath>

void FeSpriteLegacy::append_render_vertices( const FeSprite &sprite,
	std::vector<FeRenderVertex> &out, float z ) const
{
	out.clear();
	const FloatRect &rect = sprite.getTextureRect();
	const Vec2f size( std::abs( rect.size.x ), std::abs( rect.size.y ) );
	const Vec2f &s = sprite.getScale();
	const Vec2f scale( std::abs( s.x ), std::abs( s.y ) );
	if ( size.x == 0.f || size.y == 0.f || scale.x == 0.f || scale.y == 0.f )
		return;

	FloatEdges pos( 0.f, 0.f, size.x, size.y );
	FloatEdges tex( rect.position, rect.position + rect.size );
	const Vec2f skew( m_skew.x / scale.x, m_skew.y / scale.y );
	const IntEdges &b = sprite.getBorder();
	const bool has_border = b.left || b.top || b.right || b.bottom;

	// Cropping is disallowed when a border is set.
	if ( !has_border )
	{
		const FloatEdges crop = sprite.getCrop();
		const FloatEdges scrop( crop.left / scale.x, crop.top / scale.y,
			crop.right / scale.x, crop.bottom / scale.y );
		pos.left += scrop.left;
		pos.top += scrop.top;
		pos.right -= scrop.right;
		pos.bottom -= scrop.bottom;
		tex.left += scrop.left;
		tex.top += scrop.top;
		tex.right -= scrop.right;
		tex.bottom -= scrop.bottom;
	}

	const float padding_scale = sprite.getPaddingScale( Vec2f(
		( pos.right - pos.left ) * scale.x,
		( pos.bottom - pos.top ) * scale.y ) );
	const IntEdges &p = sprite.getPadding();
	const FloatEdges padding( p.left * padding_scale / scale.x,
		p.top * padding_scale / scale.y, p.right * padding_scale / scale.x,
		p.bottom * padding_scale / scale.y );
	pos.left -= padding.left;
	pos.top -= padding.top;
	pos.right = std::max( pos.left, pos.right + padding.right );
	pos.bottom = std::max( pos.top, pos.bottom + padding.bottom );

	FeRenderVertex strip[256] = {};
	int count;
	if ( has_border )
	{
		const Vec2f total_size( pos.right - pos.left, pos.bottom - pos.top );
		if ( total_size.x == 0.f || total_size.y == 0.f )
			return;
		const FloatEdges border( b.left / scale.x * padding_scale,
			b.top / scale.y * padding_scale, b.right / scale.x * padding_scale,
			b.bottom / scale.y * padding_scale );
		float px[4] = { pos.left, pos.left + border.left, pos.right - border.right, pos.right };
		float py[4] = { pos.top, pos.top + border.top, pos.bottom - border.bottom, pos.bottom };
		const float tx[4] = { tex.left, tex.left + ( b.left / rect.size.x ) * ( tex.right - tex.left ),
			tex.right - ( b.right / rect.size.x ) * ( tex.right - tex.left ), tex.right };
		const float ty[4] = { tex.top, tex.top + ( b.top / rect.size.y ) * ( tex.bottom - tex.top ),
			tex.bottom - ( b.bottom / rect.size.y ) * ( tex.bottom - tex.top ), tex.bottom };
		const float sx[4] = { 0.f, border.top / total_size.y * skew.x,
			( total_size.y - border.bottom ) / total_size.y * skew.x, skew.x };
		// Preserve the legacy asymmetric-border skew calculation, including its denominator.
		const float sy[4] = { 0.f, border.left / total_size.y * skew.y,
			( total_size.x - border.right ) / total_size.x * skew.y, skew.y };
		px[2] = std::max( px[2], px[1] );
		py[2] = std::max( py[2], py[1] );

		const int slice[28][2] = {
			{ 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }, { 2, 0 }, { 2, 1 }, { 3, 0 }, { 3, 1 }, { 3, 1 }, { 3, 1 },
			{ 3, 1 }, { 3, 2 }, { 2, 1 }, { 2, 2 }, { 1, 1 }, { 1, 2 }, { 0, 1 }, { 0, 2 }, { 0, 2 }, { 0, 2 },
			{ 0, 2 }, { 0, 3 }, { 1, 2 }, { 1, 3 }, { 2, 2 }, { 2, 3 }, { 3, 2 }, { 3, 3 }
		};
		count = 28;
		for ( int i = 0; i < count; ++i )
		{
			const int x = slice[i][0];
			const int y = slice[i][1];
			strip[i].x = px[x] + sx[y];
			strip[i].y = py[y] + sy[x];
			strip[i].u = tx[x];
			strip[i].v = ty[y];
		}
	}
	else if ( m_pinch.x != 0.f || m_pinch.y != 0.f )
	{
		const int slices = 253; // Legacy 256-vertex strip; slices must be odd.
		const Vec2f pinch( m_pinch.x / scale.x, m_pinch.y / scale.y );
		const float bws = ( pos.right - pos.left ) / slices;
		const float pys = pinch.y / slices;
		const float sys = skew.y / slices;
		const float bps = bws - pinch.x * 2 / slices;
		const float tws = ( tex.right - tex.left ) / slices;
		count = 256;
		for ( int i = 0; i < count; ++i )
		{
			const int j = std::min( std::max( 0, i - 1 ), slices );
			strip[i].x = i % 2 ? pos.left + bps * j + skew.x + pinch.x : pos.left + bws * j;
			strip[i].y = i % 2 ? pos.bottom - ( pys - sys ) * j : pos.top + ( pys + sys ) * j;
			strip[i].u = tex.left + tws * j;
			strip[i].v = i % 2 ? tex.bottom : tex.top;
		}
	}
	else
	{
		count = 4;
		strip[0].x = pos.left;
		strip[0].y = pos.top;
		strip[1].x = pos.left + skew.x;
		strip[1].y = pos.bottom;
		strip[2].x = pos.right;
		strip[2].y = pos.top + skew.y;
		strip[3].x = pos.right + skew.x;
		strip[3].y = pos.bottom + skew.y;
		for ( int i = 0; i < count; ++i )
		{
			strip[i].u = i < 2 ? tex.left : tex.right;
			strip[i].v = i % 2 ? tex.bottom : tex.top;
		}
	}

	const float radians = sprite.getRotation() * 3.14159265358979323846f / 180.f;
	const float cos_rotation = std::cos( radians );
	const float sin_rotation = std::sin( radians );
	const Vec3f &origin = sprite.getOrigin();
	const Vec2f &position = sprite.getPosition();
	const Color color = sprite.getColor();
	for ( int i = 0; i < count; ++i )
	{
		FeRenderVertex &vertex = strip[i];
		const float x = ( vertex.x - origin.x ) * s.x;
		const float y = ( vertex.y - origin.y ) * s.y;
		vertex.x = x * cos_rotation - y * sin_rotation + position.x;
		vertex.y = x * sin_rotation + y * cos_rotation + position.y;
		vertex.z = z;
		vertex.r = color.r;
		vertex.g = color.g;
		vertex.b = color.b;
		vertex.a = color.a;
		if ( !std::isfinite( vertex.x ) || !std::isfinite( vertex.y ) || !std::isfinite( vertex.z )
			|| !std::isfinite( vertex.u ) || !std::isfinite( vertex.v ) )
			return;
	}

	// Keep strip diagonals and alternate winding when expanding to triangle lists.
	out.reserve( ( count - 2 ) * 3 );
	for ( int i = 0; i < count - 2; ++i )
	{
		out.push_back( strip[i + ( i % 2 )] );
		out.push_back( strip[i + 1 - ( i % 2 )] );
		out.push_back( strip[i + 2] );
	}
}
