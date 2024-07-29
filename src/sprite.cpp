/*
 *  Portions of this file are from the SFML Project and are subject to the
 *  SFML copyright notice and license terms set out below.
 *
 *  All modifications to this file for the Attract-Mode frontend are subject
 *  to the Attract-Mode copyright notice and license terms set out below.
 *
 */

////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2013 Laurent Gomila (laurent.gom@gmail.com)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

/*
 *  Attract-Mode frontend
 *  Copyright (C) 2013-2014 Andrew Mickelson
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

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "sprite.hpp"
#include "fe_present.hpp"
#include <SFML/OpenGL.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <cmath>

#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#  define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#  define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif

#ifndef GL_TEXTURE_LOD_BIAS
#  define GL_TEXTURE_LOD_BIAS 0x8501
#endif

////////////////////////////////////////////////////////////
FeSprite::FeSprite() :
m_vertices( sf::TrianglesStrip, 4 ),
m_texture    (NULL),
m_textureRect(),
m_pinch( 0.f, 0.f ),
m_skew( 0.f, 0.f ),
m_top_left( 0.f, 0.f ),
m_top_right( 0.f, 0.f ),
m_bottom_left( 0.f, 0.f ),
m_bottom_right( 0.f, 0.f ),
m_texture_perspective( false ),
m_perspective_coefficient( 0.f )
{
}


////////////////////////////////////////////////////////////
FeSprite::FeSprite(const sf::Texture& texture) :
m_vertices( sf::TrianglesStrip, 4 ),
m_texture    (NULL),
m_textureRect(),
m_pinch( 0.f, 0.f ),
m_skew( 0.f, 0.f ),
m_top_left( 0.f, 0.f ),
m_top_right( 0.f, 0.f ),
m_bottom_left( 0.f, 0.f ),
m_bottom_right( 0.f, 0.f ),
m_texture_perspective( false ),
m_perspective_coefficient( 0.f )
{
    setTexture(texture);
}


////////////////////////////////////////////////////////////
FeSprite::FeSprite(const sf::Texture& texture, const sf::FloatRect& rectangle) :
m_vertices( sf::TrianglesStrip, 4 ),
m_texture    (NULL),
m_textureRect(),
m_pinch( 0.f, 0.f ),
m_skew( 0.f, 0.f ),
m_top_left( 0.f, 0.f ),
m_top_right( 0.f, 0.f ),
m_bottom_left( 0.f, 0.f ),
m_bottom_right( 0.f, 0.f ),
m_texture_perspective( false ),
m_perspective_coefficient( 0.f )
{
    setTexture(texture);
    setTextureRect(rectangle);
}


////////////////////////////////////////////////////////////
void FeSprite::setTexture(const sf::Texture& texture, bool resetRect)
{
    // Recompute the texture area if requested, or if there was no valid texture & rect before
    if (resetRect || (!m_texture && (m_textureRect == sf::FloatRect())))
        setTextureRect(sf::FloatRect(0, 0, texture.getSize().x, texture.getSize().y));

    // Assign the new texture
    m_texture = &texture;
}


////////////////////////////////////////////////////////////
void FeSprite::setTextureRect(const sf::FloatRect& rectangle)
{
    if (rectangle != m_textureRect)
    {
        m_textureRect = rectangle;
        updateGeometry();
    }
}


////////////////////////////////////////////////////////////
void FeSprite::setColor(const sf::Color& color)
{
    // Update the vertices' color
    for ( unsigned int i=0; i < m_vertices.getVertexCount(); i++ )
		m_vertices[i].color = color;
}


////////////////////////////////////////////////////////////
const sf::Texture* FeSprite::getTexture() const
{
    return m_texture;
}


////////////////////////////////////////////////////////////
const sf::FloatRect& FeSprite::getTextureRect() const
{
    return m_textureRect;
}


////////////////////////////////////////////////////////////
const sf::Color& FeSprite::getColor() const
{
    return m_vertices[0].color;
}


////////////////////////////////////////////////////////////
sf::IntRect FeSprite::getLocalBounds() const
{
    float width = std::abs(m_textureRect.width);
    float height = std::abs(m_textureRect.height);

    return sf::IntRect(0.f, 0.f, width, height);
}

////////////////////////////////////////////////////////////
void FeSprite::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
	if (m_texture)
	{
		states.transform *= getTransform();
		states.texture = m_texture;
		if ( m_vertices[0].color.a > 0 )
			target.draw( m_vertices, states );

		// Set anisotropic filtering
		FePresent *fep = FePresent::script_get_fep();
		if ( fep )
		{
			int af_mode = fep->get_fes()->get_anisotropic();
			if ( af_mode > 0 )
			{
				GLfloat aniso_max;
				glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso_max );
				glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, (GLfloat)std::min( (int)aniso_max, af_mode ));
				glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -0.25 );
			}
		}
	}
}

float FeSprite::getSkewX() const
{
	return m_skew.x;
}

float FeSprite::getSkewY() const
{
	return m_skew.y;
}

float FeSprite::getPinchX() const
{
	return m_pinch.x;
}

float FeSprite::getPinchY() const
{
	return m_pinch.y;
}

void FeSprite::setSkewX( float x )
{
	if ( x != m_skew.x )
	{
		m_skew.x = x;
		updateGeometry();
	}
}

void FeSprite::setSkewY( float y )
{
	if ( y != m_skew.y )
	{
		m_skew.y = y;
		updateGeometry();
	}
}

void FeSprite::setPinchX( float x )
{
	if ( x != m_pinch.x )
	{
		m_pinch.x = x;
		updateGeometry();
	}
}

void FeSprite::setPinchY( float y )
{
	if ( y != m_pinch.y )
	{
		m_pinch.y = y;
		updateGeometry();
	}
}

void FeSprite::setScale( const sf::Vector2f &s )
{
	sf::Transformable::setScale( s );
	updateGeometry();
}

void FeSprite::setCorners( float tl_x, float tl_y, float tr_x, float tr_y, float bl_x, float bl_y, float br_x, float br_y )
{
	m_top_left = sf::Vector2f(tl_x, tl_y);
	m_top_right = sf::Vector2f(tr_x, tr_y);
	m_bottom_left = sf::Vector2f(bl_x, bl_y);
	m_bottom_right = sf::Vector2f(br_x, br_y);
	updateGeometry();
}

bool FeSprite::getTexturePerspective() const
{
	return m_texture_perspective;
}

void FeSprite::setTexturePerspective( bool texture_perspective )
{
	m_texture_perspective = texture_perspective;
	updateGeometry();
}

float FeSprite::getPerspectiveCoefficient() const
{
	return m_perspective_coefficient;
}

void FeSprite::setPerspectiveCoefficient( float c )
{
	m_perspective_coefficient = c;
	updateGeometry();
}

////////////////////////////////////////////////////////////
void FeSprite::updateGeometry()
{
	sf::IntRect bounds = getLocalBounds();

	//
	// Compute some values that we will use for applying the
	// texture coordinates.
	//
	float left   = m_textureRect.left;
	float right  = left + m_textureRect.width;
	float top    = m_textureRect.top;
	float bottom = top + m_textureRect.height;
	sf::Color vert_colour = m_vertices[0].color;

	sf::Vector2f scale = getScale();
    sf::Vector2f sskew(m_skew.x / scale.x, m_skew.y / scale.y);
    sf::Vector2f spinch(m_pinch.x / scale.x, m_pinch.y / scale.y);

	//
	// Barycentric coordinates texture mapping
	// by Chadnaut
	//
	if ( m_texture_perspective )
	{
		sf::Vector2f tl(0.0f, 0.0f);
		sf::Vector2f bl((sskew.x + spinch.x) / bounds.width, 1.0f);
		sf::Vector2f tr(1.0f, (sskew.y + spinch.y) / bounds.height);
		sf::Vector2f br(1.0f + (sskew.x - spinch.x) / bounds.width, 1.0f + (sskew.y - spinch.y) / bounds.height);

		std::array<float, 4> z = {1.0f, 1.0f, 1.0f, 1.0f};
		sf::Vector2f a = bl - tr;
		sf::Vector2f b = tl - br;
		float cp = a.x * b.y - a.y * b.x;
		if (cp != 0.0f)
		{
			sf::Vector2f c = tr - br;
			float s = (a.x * c.y - a.y * c.x) / cp;
			float t = (b.x * c.y - b.y * c.x) / cp;
			if (s > 0.0f && s < 1.0f && t > 0.0f && t < 1.0f)
				z = {s, t, 1.0f - t, 1.0f - s};
		}

		m_vertices.resize(4);
		m_vertices.setPrimitiveType(sf::TrianglesStrip);

		std::array<sf::Vector2f, 4> positions =
		{
			sf::Vector2f(0, 0),
			sf::Vector2f(sskew.x + spinch.x, bounds.height),
			sf::Vector2f(bounds.width, sskew.y + spinch.y),
			sf::Vector2f(bounds.width + sskew.x - spinch.x, bounds.height + sskew.y - spinch.y)
		};

		std::array<sf::Vector2f, 4> texCoords =
		{
			sf::Vector2f(left, top),
			sf::Vector2f(left, bottom),
			sf::Vector2f(right, top),
			sf::Vector2f(right, bottom)
		};

		for (int i = 0; i < 4; ++i)
		{
			m_vertices[i].position = positions[i];
			m_vertices[i].texCoords = texCoords[i] / z[i];
			m_vertices[i].texProj = sf::Vector2f(1.0f, 1.0f / z[i]);
			m_vertices[i].color = m_vertices[0].color;
		}
	}

	//
	// Legacy tessellated texture mapping
	//
	else if (( m_pinch.x != 0.f ) || ( m_pinch.y != 0.f ))
	{
		//
		// If we are pinching the image, then we slice it up into
		// a triangle strip going from left to right across the image.
		// This gives a smooth transition for the image texture
		// across the whole surface.  There is probably a better way
		// to do this...
		//

		// SLICES needs to be an odd number... We draw our surface using
		// SLICES+3 vertices
		//
		const int SLICES = 253;

		float bws = (float)bounds.width / SLICES;
		float pys = (float)spinch.y / SLICES;
		float sys = (float)sskew.y / SLICES;
		float bpxs = bws - (float)spinch.x * 2 / SLICES;

		m_vertices.resize( SLICES + 3 );
		m_vertices.setPrimitiveType( sf::TrianglesStrip );

		//
		// First do the vertex coordinates
		//
		m_vertices[0].position = sf::Vector2f(0, 0 );
		m_vertices[1].position = sf::Vector2f(sskew.x + spinch.x, bounds.height );

		for ( int i=1; i<SLICES; i++ )
		{
			if ( i%2 )
			{
				m_vertices[1 + i].position = sf::Vector2f(
						bws * i, (pys + sys) * i );
			}
			else
			{
				m_vertices[1 + i].position = sf::Vector2f(
						sskew.x + spinch.x + bpxs * i, bounds.height - ( pys - sys ) * i );
			}
		}
		m_vertices[SLICES + 1].position = sf::Vector2f( bounds.width, spinch.y + sskew.y );
		m_vertices[SLICES + 2].position = sf::Vector2f(
						sskew.x + bounds.width - spinch.x, bounds.height - spinch.y + sskew.y );

		//
		// Now do the texture coordinates
		//
		float tws = m_textureRect.width / (float)SLICES;

		m_vertices[0].texCoords = sf::Vector2f(left, top );
		m_vertices[1].texCoords = sf::Vector2f(left, bottom );

		for ( int i=1; i<SLICES; i++ )
			m_vertices[1 + i].texCoords = sf::Vector2f(
						left + tws * i,
						( i % 2 ) ? top : bottom );

		m_vertices[SLICES + 1].texCoords = sf::Vector2f(right, top );
		m_vertices[SLICES + 2].texCoords = sf::Vector2f(right, bottom );
	}
	else
	{
		//
		// If we aren't pinching the image, then we draw it on two triangles.
		//
		m_vertices.resize( 4 );
		m_vertices.setPrimitiveType( sf::TrianglesStrip );

		m_vertices[0].position = sf::Vector2f(0, 0);
		m_vertices[1].position = sf::Vector2f(sskew.x, bounds.height);
		m_vertices[2].position = sf::Vector2f(bounds.width, sskew.y );
		m_vertices[3].position = sf::Vector2f(bounds.width + sskew.x, bounds.height + sskew.y);

		m_vertices[0].texCoords = sf::Vector2f(left, top);
		m_vertices[1].texCoords = sf::Vector2f(left, bottom);
		m_vertices[2].texCoords = sf::Vector2f(right, top);
		m_vertices[3].texCoords = sf::Vector2f(right, bottom);
	}
}
