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

#ifndef FE_SPRITE_LEGACY_HPP
#define FE_SPRITE_LEGACY_HPP

#include "fe_sprite.hpp"

class FeSpriteLegacy
{
public:
	FeSpriteLegacy() : m_pinch( 0.f, 0.f ), m_skew( 0.f, 0.f ) {}

	float getSkewX() const { return m_skew.x; }
	float getSkewY() const { return m_skew.y; }
	float getPinchX() const { return m_pinch.x; }
	float getPinchY() const { return m_pinch.y; }
	void setSkewX( float x ) { m_skew.x = x; }
	void setSkewY( float y ) { m_skew.y = y; }
	void setPinchX( float x ) { m_pinch.x = x; }
	void setPinchY( float y ) { m_pinch.y = y; }

	bool is_active() const
	{
		return m_pinch.x != 0.f || m_pinch.y != 0.f
			|| m_skew.x != 0.f || m_skew.y != 0.f;
	}

	void append_render_vertices( const FeSprite &sprite,
		std::vector<FeRenderVertex> &out, float z ) const;

private:
	Vec2f m_pinch;
	Vec2f m_skew;
};

#endif
