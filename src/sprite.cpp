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
m_top_left( 0.f, 0.f ),
m_top_right( 0.f, 0.f ),
m_bottom_left( 0.f, 0.f ),
m_bottom_right( 0.f, 0.f ),
m_texture_perspective( false ),
m_perspective_coefficient( 0.f ),
m_rotation_x( 0.f ),
m_rotation_y( 0.f ),
m_rotation_z( 0.f ),
m_orientation( Quaternion( 1, 0, 0, 0 ) ),
m_update_corners( true ),
m_origin_z( 0.f ),
m_back_facing( false ),
m_size( 0.f, 0.f )
{
}


////////////////////////////////////////////////////////////
FeSprite::FeSprite(const sf::Texture& texture) :
m_vertices( sf::PrimitiveType::TriangleStrip, 4 ),
m_texture    (NULL),
m_textureRect(),
m_pinch( 0.f, 0.f ),
m_skew( 0.f, 0.f ),
m_top_left( 0.f, 0.f ),
m_top_right( 0.f, 0.f ),
m_bottom_left( 0.f, 0.f ),
m_bottom_right( 0.f, 0.f ),
m_texture_perspective( false ),
m_perspective_coefficient( 0.f ),
m_rotation_x( 0.f ),
m_rotation_y( 0.f ),
m_rotation_z( 0.f ),
m_orientation( Quaternion( 1, 0, 0, 0 ) ),
m_update_corners( true ),
m_origin_z( 0.f ),
m_back_facing( false ),
m_size( 0.f, 0.f )
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
m_top_left( 0.f, 0.f ),
m_top_right( 0.f, 0.f ),
m_bottom_left( 0.f, 0.f ),
m_bottom_right( 0.f, 0.f ),
m_texture_perspective( false ),
m_perspective_coefficient( 0.f ),
m_rotation_x( 0.f ),
m_rotation_y( 0.f ),
m_rotation_z( 0.f ),
m_orientation( Quaternion( 1, 0, 0, 0 ) ),
m_update_corners( true ),
m_origin_z( 0.f ),
m_back_facing( false ),
m_size( 0.f, 0.f )
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
	m_update_corners = true;
}


////////////////////////////////////////////////////////////
void FeSprite::setTextureRect(const sf::FloatRect& rectangle)
{
    if (rectangle != m_textureRect)
    {
        m_textureRect = rectangle;
		m_update_corners = true;
    }
}


////////////////////////////////////////////////////////////
void FeSprite::setColor( sf::Color color)
{
	if ( color != m_vertices[0].color )
	{
		m_vertices[0].color = color;
		updateGeometry();
	}
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
	sf::Vector2f target_center = sf::Vector2f( static_cast<float>( target.getSize().x ) / 2.0f, static_cast<float>( target.getSize().y ) / 2.0f );

	if ( m_update_corners )
	{
		if ( m_texture_perspective )
			const_cast<FeSprite*>( this )->updateCornersWithRotation( target_center );
		else
			const_cast<FeSprite*>( this )->updateCorners();

		const_cast<FeSprite*>( this )->m_update_corners = false;
	}

	if (m_texture)
	{
		states.transform *= getTransform();
		states.texture = m_texture;
		if ( m_vertices[0].color.a > 0 && ( !const_cast<FeSprite*>( this )->m_back_facing ) )
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
		m_update_corners = true;
	}
}

void FeSprite::setSkewY( float y )
{
	if ( y != m_skew.y )
	{
		m_skew.y = y;
		m_update_corners = true;
	}
}

void FeSprite::setPinchX( float x )
{
	if ( x != m_pinch.x )
	{
		m_pinch.x = x;
		m_update_corners = true;
	}
}

void FeSprite::setPinchY( float y )
{
	if ( y != m_pinch.y )
	{
		m_pinch.y = y;
		m_update_corners = true;
	}
}

void FeSprite::setScale( const sf::Vector2f &s )
{
	sf::Transformable::setScale( s );
	updateGeometry();
}

void FeSprite::setCorners( float tl_x, float tl_y, float tr_x, float tr_y, float bl_x, float bl_y, float br_x, float br_y )
{
	m_texture_perspective = true;
	sf::IntRect bounds = getLocalBounds();
	m_top_left = sf::Vector2f(tl_x, tl_y);
	m_top_right = sf::Vector2f(tr_x + bounds.size.x, tr_y);
	m_bottom_left = sf::Vector2f(bl_x, bl_y + bounds.size.y);
	m_bottom_right = sf::Vector2f(br_x + bounds.size.x, br_y + bounds.size.y);

	updateGeometry();
}

void FeSprite::updateCorners()
{
	sf::Vector2f scale = getScale();
    sf::Vector2f sskew(m_skew.x / scale.x, m_skew.y / scale.y);
    sf::Vector2f spinch(m_pinch.x / scale.x, m_pinch.y / scale.y);

	sf::IntRect bounds = getLocalBounds();
	m_top_left = sf::Vector2f(0, 0);
	m_bottom_left = sf::Vector2f(sskew.x + spinch.x, bounds.size.y);
	m_top_right = sf::Vector2f(bounds.size.x, sskew.y + spinch.y);
	m_bottom_right = sf::Vector2f(bounds.size.x + sskew.x - spinch.x, bounds.size.y + sskew.y - spinch.y);

	updateGeometry();
}

void FeSprite::updateCornersWithRotation( sf::Vector2f screen_center )
{
	float rx = m_rotation_x * M_PI / 180.0f;
	float ry = m_rotation_y * M_PI / 180.0f;
	float rz = m_rotation_z * M_PI / 180.0f;

	Quaternion qx( rx, sf::Vector3f( 1, 0, 0 ) );
	Quaternion qy( ry, sf::Vector3f( 0, 1, 0 ) );
	Quaternion qz( rz, sf::Vector3f( 0, 0, 1 ) );

	// Z is most local, X is most global, to be configurable later
	m_orientation = qx * qy * qz;

	m_texture_perspective = true;
	sf::IntRect bounds = getLocalBounds();
	float w = static_cast< float >( bounds.size.x );
	float h = static_cast< float >( bounds.size.y );

	float z_scale = m_size.y / h * 2.0f; // TODO: only works for square textures for now

	sf::Vector3f anchor = sf::Vector3f( getOrigin().x, getOrigin().y, m_origin_z / z_scale );
	sf::Transform inv_transform = getInverseTransform();
	sf::Vector2f local_vp_2d = inv_transform.transformPoint( screen_center );
	sf::Vector3f local_vp = sf::Vector3f( local_vp_2d.x, local_vp_2d.y, 0.0f );
	sf::Vector3f relative_vp = local_vp - anchor;

	std::array< sf::Vector3f, 4 > corners =
	{
		sf::Vector3f( -anchor.x, -anchor.y, -anchor.z ),
		sf::Vector3f( w - anchor.x, -anchor.y, -anchor.z ),
		sf::Vector3f( -anchor.x, h - anchor.y, -anchor.z ),
		sf::Vector3f( w - anchor.x, h - anchor.y, -anchor.z )
	};

	float max_coeff = 0.001f;
	float persp = m_perspective_coefficient * max_coeff;

	for ( auto &v : corners )
	{
		v = m_orientation.rotate( v );

		if ( persp != 0.0f )
		{
			float factor = 1.0f + v.z * persp;
			if ( factor != 0.0f )
			{
				v.x = relative_vp.x + ( v.x - relative_vp.x ) / factor;
				v.y = relative_vp.y + ( v.y - relative_vp.y ) / factor;
				v.z = relative_vp.z + ( v.z - relative_vp.z ) / factor;
			}
		}

		v.x += anchor.x;
		v.y += anchor.y;
	}

	// Compute normal to determine if back face is showing
	sf::Vector3f v0 = corners[0];
	sf::Vector3f v1 = corners[1];
	sf::Vector3f v2 = corners[2];

	sf::Vector3f edge1( v1.x - v0.x, v1.y - v0.y, v1.z - v0.z );
	sf::Vector3f edge2( v2.x - v0.x, v2.y - v0.y, v2.z - v0.z );
	sf::Vector3f normal(
		edge1.y * edge2.z - edge1.z * edge2.y,
		edge1.z * edge2.x - edge1.x * edge2.z,
		edge1.x * edge2.y - edge1.y * edge2.x
	);

	m_back_facing = ( normal.z < 0.0f );

	setCorners(
		corners[0].x, corners[0].y,
		corners[1].x - w, corners[1].y,
		corners[2].x, corners[2].y - h,
		corners[3].x - w, corners[3].y - h );

	updateGeometry();
}

bool FeSprite::getTexturePerspective() const
{
	return m_texture_perspective;
}

void FeSprite::setTexturePerspective( bool texture_perspective )
{
	m_texture_perspective = texture_perspective;
	m_update_corners = true;
}

float FeSprite::getPerspectiveCoefficient() const
{
	return m_perspective_coefficient;
}

void FeSprite::setPerspectiveCoefficient( float c )
{
	if ( c < 0.0f )
		c = 0.0f;
	else if ( c > 1.0f )
		c = 1.0f;

	m_perspective_coefficient = c;
	m_update_corners = true;
}

float FeSprite::getRotationX() const
{
	return m_rotation_x;
}

void FeSprite::setRotationX( float r )
{
	if ( r == m_rotation_x ) return;
	m_rotation_x = r;
	m_update_corners = true;
}

float FeSprite::getRotationY() const
{
	return m_rotation_y;
}

void FeSprite::setRotationY( float r )
{
	if ( r == m_rotation_y ) return;
	m_rotation_y = r;
	m_update_corners = true;
}


float FeSprite::getRotationZ() const
{
	return m_rotation_z;
}

void FeSprite::setRotationZ( float r )
{
	if ( r == m_rotation_z ) return;
	m_rotation_z = r;
	m_update_corners = true;
}

void FeSprite::setOriginZ( float z )
{
	if ( z == m_origin_z ) return;
	m_origin_z = z;
	m_update_corners = true;
}

void FeSprite::setSize( const sf::Vector2f &size )
{
	m_size = size;
}

void FeSprite::setPosition( float x, float y )
{
	sf::Transformable::setPosition( sf::Vector2f( x, y ) );
	m_update_corners = true;
}

void FeSprite::setPosition( const sf::Vector2f &position )
{
	sf::Transformable::setPosition( position );
	m_update_corners = true;
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

	sf::Vector2f scale = getScale();
    sf::Vector2f sskew(m_skew.x / scale.x, m_skew.y / scale.y);
    sf::Vector2f spinch(m_pinch.x / scale.x, m_pinch.y / scale.y);

	//
	// Barycentric coordinates texture mapping
	// by Chadnaut
	//
	if ( m_texture_perspective )
	{
		sf::Vector2f tl(m_top_left.x / bounds.size.x, m_top_left.y / bounds.size.y);
		sf::Vector2f bl(m_bottom_left.x / bounds.size.x, m_bottom_left.y / bounds.size.y);
		sf::Vector2f tr(m_top_right.x / bounds.size.x, m_top_right.y / bounds.size.y);
		sf::Vector2f br(m_bottom_right.x / bounds.size.x, m_bottom_right.y / bounds.size.y);

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
		m_vertices.setPrimitiveType(sf::PrimitiveType::TriangleStrip);

		m_vertices[0].position = m_top_left;
		m_vertices[1].position = m_bottom_left;
		m_vertices[2].position = m_top_right;
		m_vertices[3].position = m_bottom_right;

		m_vertices[0].texCoords = sf::Vector2f(left, top) / z[0];
		m_vertices[1].texCoords = sf::Vector2f(left, bottom) / z[1];
		m_vertices[2].texCoords = sf::Vector2f(right, top) / z[2];
		m_vertices[3].texCoords = sf::Vector2f(right, bottom) / z[3];

		m_vertices[0].texProj = sf::Vector2f(1.0f, 1.0f / z[0]);
		m_vertices[1].texProj = sf::Vector2f(1.0f, 1.0f / z[1]);
		m_vertices[2].texProj = sf::Vector2f(1.0f, 1.0f / z[2]);
		m_vertices[3].texProj = sf::Vector2f(1.0f, 1.0f / z[3]);
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

		m_vertices[0].position = m_top_left;
		m_vertices[1].position = m_bottom_left;
		m_vertices[2].position = m_top_right;
		m_vertices[3].position = m_bottom_right;

		m_vertices[0].texCoords = sf::Vector2f(left, top);
		m_vertices[1].texCoords = sf::Vector2f(left, bottom);
		m_vertices[2].texCoords = sf::Vector2f(right, top);
		m_vertices[3].texCoords = sf::Vector2f(right, bottom);

		m_vertices[0].texProj = sf::Vector2f(1.0f, 1.0f);
		m_vertices[1].texProj = sf::Vector2f(1.0f, 1.0f);
		m_vertices[2].texProj = sf::Vector2f(1.0f, 1.0f);
		m_vertices[3].texProj = sf::Vector2f(1.0f, 1.0f);
	}

	//
	// Finally, update the vertex colour
	//
	for ( unsigned int i=1; i< m_vertices.getVertexCount(); i++ )
		m_vertices[i].color = m_vertices[0].color;
}
