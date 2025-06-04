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
m_vertices( sf::PrimitiveType::TriangleStrip, 4 ),
m_texture    (NULL),
m_textureRect(),
m_pinch( 0.f, 0.f ),
m_skew( 0.f, 0.f ),
m_border( 0, 0, 0, 0 ),
m_padding( 0, 0, 0, 0 ),
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
	sf::IntRect bounds = getLocalBounds();

	//
	// Compute some values that we will use for applying the
	// texture coordinates.
	//
	float left   = m_textureRect.position.x;
	float right  = left + m_textureRect.size.x;
	float top    = m_textureRect.position.y;
	float bottom = top + m_textureRect.size.y;
	sf::Color vert_colour = m_vertices[0].color;

	sf::Vector2f scale = getScale();
	sf::Vector2f sskew = m_skew;
	sskew.x /= scale.x;
	sskew.y /= scale.y;
	if (( m_border.left > 0 ) ||
	    ( m_border.top > 0 ) ||
	    ( m_border.right > 0 ) ||
	    ( m_border.bottom > 0 ))
	{
		float x[4], y[4];

		float total_width = m_textureRect.size.x + ( m_padding.left + m_padding.right ) / scale.x;
		float total_height = m_textureRect.size.y + ( m_padding.top + m_padding.bottom ) / scale.y;

		float min_scale = 1.0f;
		float border_width = ( m_border.left + m_border.right ) / scale.x;
		float border_height = ( m_border.top + m_border.bottom ) / scale.y;
		if ( border_width * m_border_scale > total_width || border_height * m_border_scale > total_height )
			if ( border_width > 0.0f && border_height > 0.0f )
				min_scale = std::min( total_width / ( border_width * m_border_scale ), total_height / ( border_height * m_border_scale ));

		float border_left = ( m_border.left / scale.x ) * min_scale * m_border_scale;
		float border_right = ( m_border.right / scale.x ) * min_scale * m_border_scale;
		float border_top = ( m_border.top / scale.y ) * min_scale * m_border_scale;
		float border_bottom = ( m_border.bottom / scale.y ) * min_scale * m_border_scale;

		if ( border_left + border_right > total_width || border_top + border_bottom > total_height )
		{
			float clamp_ratio = std::min(
				border_left + border_right > 0.0f ? total_width / ( border_left + border_right ) : 1.0f,
				border_top + border_bottom > 0.0f ? total_height / ( border_top + border_bottom ) : 1.0f );

			border_left *= clamp_ratio;
			border_right *= clamp_ratio;
			border_top *= clamp_ratio;
			border_bottom *= clamp_ratio;
		}

		x[0] = -m_padding.left / scale.x;
		x[1] = x[0] + border_left;
		x[2] = x[0] + total_width - border_right;
		x[3] = x[0] + total_width;

		y[0] = -m_padding.top / scale.y;
		y[1] = y[0] + border_top;
		y[2] = y[0] + total_height - border_bottom;
		y[3] = y[0] + total_height;

		x[2] = std::max( x[2], x[1] );
		y[2] = std::max( y[2], y[1] );

		float tx[4] = { left, left + ( m_border.left / m_textureRect.size.x ) * ( right - left) , right - ( m_border.right / m_textureRect.size.x ) * ( right - left ), right };
		float ty[4] = { top, top + ( m_border.top / m_textureRect.size.y ) * ( bottom - top ), bottom - ( m_border.bottom / m_textureRect.size.y ) * ( bottom - top ), bottom };

		const int rows = 4;
		const int cols = 4;
		const int vertex_count = ( rows - 1 ) * ( 2 * cols + 2 ) - 2;
		m_vertices.resize( vertex_count );
		m_vertices.setPrimitiveType( sf::PrimitiveType::TriangleStrip );
		int idx = 0;
		for ( int row = 0; row < rows - 1; row++ )
		{
			if ( row % 2 == 0 )
			{
				for ( int col = 0; col < cols; col++ )
				{
					m_vertices[idx].position = sf::Vector2f( x[col], y[row] );
					m_vertices[idx].texCoords = sf::Vector2f( tx[col], ty[row] );
					idx++;
					m_vertices[idx].position = sf::Vector2f( x[col], y[row+1] );
					m_vertices[idx].texCoords = sf::Vector2f( tx[col], ty[row+1] );
					idx++;
				}
				if ( row < rows - 2 )
				{
					// degenerate: repeat last vertex
					m_vertices[idx] = m_vertices[idx-1];
					idx++;
					// degenerate: next row starts from right, so connect to rightmost vertex
					m_vertices[idx].position = sf::Vector2f( x[cols-1], y[row+1] );
					m_vertices[idx].texCoords = sf::Vector2f( tx[cols-1], ty[row+1] );
					idx++;
				}
			}
			else
			{
				for ( int col = cols - 1; col >= 0; col-- )
				{
					m_vertices[idx].position = sf::Vector2f( x[col], y[row] );
					m_vertices[idx].texCoords = sf::Vector2f( tx[col], ty[row] );
					idx++;
					m_vertices[idx].position = sf::Vector2f( x[col], y[row+1] );
					m_vertices[idx].texCoords = sf::Vector2f( tx[col], ty[row+1] );
					idx++;
				}
				if ( row < rows - 2 )
				{
					// degenerate: repeat last vertex
					m_vertices[idx] = m_vertices[idx-1];
					idx++;
					// degenerate: next row starts from left, so connect to leftmost vertex
					m_vertices[idx].position = sf::Vector2f( x[0], y[row+1] );
					m_vertices[idx].texCoords = sf::Vector2f( tx[0], ty[row+1] );
					idx++;
				}
			}
		}
	}
	else if (( m_pinch.x != 0.f ) || ( m_pinch.y != 0.f ))
	{
		sf::Vector2f spinch = m_pinch;
		spinch.x /= scale.x;
		spinch.y /= scale.y;

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

		float bws = (float)bounds.size.x / SLICES;
		float pys = (float)spinch.y / SLICES;
		float sys = (float)sskew.y / SLICES;
		float bpxs = bws - (float)spinch.x * 2 / SLICES;

		m_vertices.resize( SLICES + 3 );
		m_vertices.setPrimitiveType( sf::PrimitiveType::TriangleStrip );

		//
		// First do the vertex coordinates
		//
		m_vertices[0].position = sf::Vector2f(0, 0 );
		m_vertices[1].position = sf::Vector2f(sskew.x + spinch.x, bounds.size.y );

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
						sskew.x + spinch.x + bpxs * i, bounds.size.y - ( pys - sys ) * i );
			}
		}
		m_vertices[SLICES + 1].position = sf::Vector2f( bounds.size.x, spinch.y + sskew.y );
		m_vertices[SLICES + 2].position = sf::Vector2f(
						sskew.x + bounds.size.x - spinch.x, bounds.size.y - spinch.y + sskew.y );

		//
		// Now do the texture coordinates
		//
		float tws = m_textureRect.size.x / (float)SLICES;

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
		m_vertices.setPrimitiveType( sf::PrimitiveType::TriangleStrip );

		m_vertices[0].position = sf::Vector2f(0, 0);
		m_vertices[1].position = sf::Vector2f(sskew.x, bounds.size.y);
		m_vertices[2].position = sf::Vector2f(bounds.size.x, sskew.y );
		m_vertices[3].position = sf::Vector2f(bounds.size.x + sskew.x, bounds.size.y + sskew.y);

		m_vertices[0].texCoords = sf::Vector2f(left, top);
		m_vertices[1].texCoords = sf::Vector2f(left, bottom);
		m_vertices[2].texCoords = sf::Vector2f(right, top);
		m_vertices[3].texCoords = sf::Vector2f(right, bottom);
	}

	//
	// Finally, update the vertex colour
	//
	for ( unsigned int i=0; i< m_vertices.getVertexCount(); i++ )
		m_vertices[i].color = vert_colour;

}
