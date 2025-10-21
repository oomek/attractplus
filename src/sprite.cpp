/*
 *  Portions of this file are from the SFML Project and are subject to the
 *  SFML copyright notice and license terms set out below.
 *
 *  All modifications to this file are subject to the copyright notice and license terms set out below.
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
m_vertices( sf::PrimitiveType::TriangleStrip, 4 ),
m_texture    (NULL),
m_textureRect(),
m_pinch( 0.f, 0.f ),
m_skew( 0.f, 0.f ),
m_border( 0, 0, 0, 0 ),
m_padding( 0, 0, 0, 0 ),
m_crop( 0.f, 0.f, 0.f, 0.f ),
m_border_scale( 1.f )
{
}


////////////////////////////////////////////////////////////
FeSprite::FeSprite(const sf::Texture& texture) :
m_vertices( sf::PrimitiveType::TriangleStrip, 4 ),
m_texture    (NULL),
m_textureRect(),
m_pinch( 0.f, 0.f ),
m_skew( 0.f, 0.f ),
m_border( 0, 0, 0, 0 ),
m_padding( 0, 0, 0, 0 ),
m_crop( 0.f, 0.f, 0.f, 0.f ),
m_border_scale( 1.f )
{
    setTexture(texture);
}


////////////////////////////////////////////////////////////
FeSprite::FeSprite(const sf::Texture& texture, const sf::FloatRect& rectangle) :
m_vertices( sf::PrimitiveType::TriangleStrip, 4 ),
m_texture    (NULL),
m_textureRect(),
m_pinch( 0.f, 0.f ),
m_skew( 0.f, 0.f ),
m_border( 0, 0, 0, 0 ),
m_padding( 0, 0, 0, 0 ),
m_crop( 0.f, 0.f, 0.f, 0.f ),
m_border_scale( 1.f )
{
    setTexture(texture);
    setTextureRect(rectangle);
}


////////////////////////////////////////////////////////////
void FeSprite::setTexture(const sf::Texture& texture, bool resetRect)
{
    // Recompute the texture area if requested, or if there was no valid texture & rect before
    if (resetRect || (!m_texture && (m_textureRect == sf::FloatRect())))
        setTextureRect( sf::FloatRect({ 0, 0 }, { static_cast<float>( texture.getSize().x ), static_cast<float>( texture.getSize().y )}));

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
void FeSprite::setColor( sf::Color color)
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
sf::Color FeSprite::getColor() const
{
    return m_vertices[0].color;
}


////////////////////////////////////////////////////////////
sf::IntRect FeSprite::getLocalBounds() const
{
    int width = std::abs( m_textureRect.size.x );
    int height = std::abs( m_textureRect.size.y );

    return sf::IntRect({ 0, 0 }, { width, height });
}

void FeSprite::setCrop( FloatEdges crop )
{
    if ( m_crop != crop )
    {
        m_crop = crop;
        updateGeometry();
    }
}

FloatEdges FeSprite::getCrop()
{
	return m_crop;
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

const IntEdges& FeSprite::getBorder() const
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

const IntEdges& FeSprite::getPadding() const
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

float FeSprite::getBorderScale() const
{
	return m_border_scale;
}

void FeSprite::setBorderScale( float s )
{
	if ( s != m_border_scale )
	{
		m_border_scale = s;
		updateGeometry();
	}
}

////////////////////////////////////////////////////////////
void FeSprite::updateGeometry()
{
	FloatEdges pos = FloatEdges( getLocalBounds() );
	FloatEdges tex = FloatEdges( m_textureRect.position, m_textureRect.position + m_textureRect.size );

	sf::Vector2f s = getScale();
	sf::Vector2f scale = sf::Vector2f( std::abs( s.x ), std::abs( s.y ) );
	sf::Vector2f sskew = sf::Vector2f( m_skew.x / scale.x, m_skew.y / scale.y );
	bool has_border = m_border.left || m_border.top || m_border.right || m_border.bottom;

	// Cropping is disallowed when a border is set
	if ( !has_border )
	{
		FloatEdges scrop = FloatEdges( m_crop.left / scale.x, m_crop.top / scale.y, m_crop.right / scale.x, m_crop.bottom / scale.y );
		pos.left += scrop.left;
		pos.top += scrop.top;
		pos.right -= scrop.right;
		pos.bottom -= scrop.bottom;
		tex.left += scrop.left;
		tex.top += scrop.top;
		tex.right -= scrop.right;
		tex.bottom -= scrop.bottom;
	}

	// Padding enlarges the position to overlap the image bounds
	FloatEdges spadding = FloatEdges( m_padding.left / scale.x, m_padding.top / scale.y, m_padding.right / scale.x, m_padding.bottom / scale.y );
	pos.left -= spadding.left;
	pos.top -= spadding.top;
	pos.right = std::max( pos.left, pos.right + spadding.right );
	pos.bottom = std::max( pos.top, pos.bottom + spadding.bottom );

	if ( has_border )
	{
		// ----------------------------------------
		// 9-slice mode
		// ----------------------------------------

		sf::Vector2f tex_size = m_textureRect.size;
		sf::Vector2f total_size = sf::Vector2f( pos.right - pos.left, pos.bottom - pos.top );
		FloatEdges sborder = FloatEdges( m_border.left / scale.x, m_border.top / scale.y, m_border.right / scale.x, m_border.bottom / scale.y );

		// Run twice, once to scale x, once to scale y (or vice-versa)
		float s = m_border_scale;
		for ( int i=0; i<2; i++)
			s *= std::min(
				total_size.x / std::max( ( sborder.left + sborder.right ) * s, total_size.x ),
				total_size.y / std::max( ( sborder.top + sborder.bottom ) * s, total_size.y )
			);

		FloatEdges border = FloatEdges( sborder.left * s, sborder.top * s, sborder.right * s, sborder.bottom * s );

		// Prepare pos/tex points for the slices
		float px[4] = { pos.left, pos.left + border.left, pos.right - border.right, pos.right };
		float py[4] = { pos.top, pos.top + border.top, pos.bottom - border.bottom, pos.bottom };
		float tx[4] = { tex.left, tex.left + ( m_border.left / tex_size.x ) * ( tex.right - tex.left ), tex.right - ( m_border.right / tex_size.x ) * ( tex.right - tex.left ), tex.right };
		float ty[4] = { tex.top, tex.top + ( m_border.top / tex_size.y ) * ( tex.bottom - tex.top ), tex.bottom - ( m_border.bottom / tex_size.y ) * ( tex.bottom - tex.top ), tex.bottom };
		float sx[4] = { 0, border.top / total_size.y * sskew.x, (total_size.y - border.bottom) / total_size.y * sskew.x, sskew.x };
		float sy[4] = { 0, border.left / total_size.y * sskew.y, (total_size.x - border.right) / total_size.x * sskew.y, sskew.y };
		px[2] = std::max( px[2], px[1] );
		py[2] = std::max( py[2], py[1] );

		// Create the grid of slice points
		m_vertices.resize( 28 );
		m_vertices.setPrimitiveType( sf::PrimitiveType::TriangleStrip );
		std::pair<int, int> slice[28] = {
			{ 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }, { 2, 0 }, { 2, 1 }, { 3, 0 }, { 3, 1 }, { 3, 1 }, { 3, 1 },
			{ 3, 1 }, { 3, 2 }, { 2, 1 }, { 2, 2 }, { 1, 1 }, { 1, 2 }, { 0, 1 }, { 0, 2 }, { 0, 2 }, { 0, 2 },
			{ 0, 2 }, { 0, 3 }, { 1, 2 }, { 1, 3 }, { 2, 2 }, { 2, 3 }, { 3, 2 }, { 3, 3 }
		};

		// Fill all vertices
		int idx = 0;
		for ( auto coord : slice )
		{
			m_vertices[idx].position = sf::Vector2f( px[coord.first] + sx[coord.second], py[coord.second] + sy[coord.first] );
			m_vertices[idx].texCoords = sf::Vector2f( tx[coord.first], ty[coord.second] );
			idx++;
		}
	}
	else if (( m_pinch.x != 0.f ) || ( m_pinch.y != 0.f ))
	{
		// ----------------------------------------
		// Pinch mode, uses a left-to-right triangle-strip
		// ----------------------------------------

		const int edges = 3;
		const int slices = 256 - edges; // slices must be odd
		m_vertices.resize( slices + edges );
		m_vertices.setPrimitiveType( sf::PrimitiveType::TriangleStrip );

		sf::Vector2f spinch = sf::Vector2f( m_pinch.x / scale.x, m_pinch.y / scale.y );
		float bws = (pos.right - pos.left) / slices;
		float pys = spinch.y / slices;
		float sys = sskew.y / slices;
		float bps = bws - spinch.x * 2 / slices;
		float tws = (tex.right - tex.left) / slices;

		for ( int i=0; i<slices + edges; i++ )
		{
			int j = std::min( std::max( 0, i - 1 ), slices );
			m_vertices[i].position = i % 2
				? sf::Vector2f( pos.left + bps * j + sskew.x + spinch.x, pos.bottom - ( pys - sys ) * j )
				: sf::Vector2f( pos.left + bws * j, pos.top + ( pys + sys ) * j );
			m_vertices[i].texCoords = i % 2
				? sf::Vector2f( tex.left + tws * j, tex.bottom )
				: sf::Vector2f( tex.left + tws * j, tex.top );
		}
	}
	else
	{
		// ----------------------------------------
		// Flat mode - draw using two triangles
		// ----------------------------------------

		m_vertices.resize( 4 );
		m_vertices.setPrimitiveType( sf::PrimitiveType::TriangleStrip );

		m_vertices[0].position = sf::Vector2f( pos.left, pos.top );
		m_vertices[1].position = sf::Vector2f( pos.left + sskew.x, pos.bottom );
		m_vertices[2].position = sf::Vector2f( pos.right, pos.top + sskew.y );
		m_vertices[3].position = sf::Vector2f( pos.right + sskew.x, pos.bottom + sskew.y );

		m_vertices[0].texCoords = sf::Vector2f( tex.left, tex.top );
		m_vertices[1].texCoords = sf::Vector2f( tex.left, tex.bottom );
		m_vertices[2].texCoords = sf::Vector2f( tex.right, tex.top );
		m_vertices[3].texCoords = sf::Vector2f( tex.right, tex.bottom );
	}

	// Update the vertex colour
	sf::Color vert_colour = m_vertices[0].color;
	for ( size_t i=0; i<m_vertices.getVertexCount(); i++ )
		m_vertices[i].color = vert_colour;
}
