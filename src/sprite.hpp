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

#ifndef FE_SPRITE_HPP
#define FE_SPRITE_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Transformable.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector3.hpp>

#define _USE_MATH_DEFINES
#include <cmath>

namespace sf
{
	class Texture;
};

struct Quaternion
{
	float w, x, y, z;

	Quaternion( float angle, sf::Vector3f axis )
	{
		float half = angle * 0.5f;
		float s = sinf( half );
		w = cosf( half );
		x = axis.x * s;
		y = axis.y * s;
		z = axis.z * s;
	}

	Quaternion operator*( const Quaternion &q ) const
	{
		return Quaternion
		{
			w * q.w - x * q.x - y * q.y - z * q.z,
			w * q.x + x * q.w + y * q.z - z * q.y,
			w * q.y - x * q.z + y * q.w + z * q.x,
			w * q.z + x * q.y - y * q.x + z * q.w
		};
	}

	sf::Vector3f rotate( const sf::Vector3f &v ) const
	{
		Quaternion qv{ 0, v.x, v.y, v.z };
		Quaternion inv{ w, -x, -y, -z };
		Quaternion res = ( *this ) * qv * inv;
		return sf::Vector3f( res.x, res.y, res.z );
	}
	Quaternion( float w_, float x_, float y_, float z_ ) : w( w_ ), x( x_ ), y( y_ ), z( z_ ) {}
};

////////////////////////////////////////////////////////////
/// \brief Drawable representation of a texture, with its
///        own transformations, color, etc.
///
////////////////////////////////////////////////////////////
class FeSprite : public sf::Drawable, public sf::Transformable
{
public :

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// Creates an empty sprite with no source texture.
    ///
    ////////////////////////////////////////////////////////////
    FeSprite();

    ////////////////////////////////////////////////////////////
    /// \brief Construct the sprite from a source texture
    ///
    /// \param texture Source texture
    ///
    /// \see setTexture
    ///
    ////////////////////////////////////////////////////////////
    explicit FeSprite(const sf::Texture& texture);

    ////////////////////////////////////////////////////////////
    /// \brief Construct the sprite from a sub-rectangle of a source texture
    ///
    /// \param texture   Source texture
    /// \param rectangle Sub-rectangle of the texture to assign to the sprite
    ///
    /// \see setTexture, setTextureRect
    ///
    ////////////////////////////////////////////////////////////
    FeSprite(const sf::Texture& texture, const sf::FloatRect& rectangle);

    ////////////////////////////////////////////////////////////
    /// \brief Change the source texture of the sprite
    ///
    /// The \a texture argument refers to a texture that must
    /// exist as long as the sprite uses it. Indeed, the sprite
    /// doesn't store its own copy of the texture, but rather keeps
    /// a pointer to the one that you passed to this function.
    /// If the source texture is destroyed and the sprite tries to
    /// use it, the behaviour is undefined.
    /// If \a resetRect is true, the TextureRect property of
    /// the sprite is automatically adjusted to the size of the new
    /// texture. If it is false, the texture rect is left unchanged.
    ///
    /// \param texture   New texture
    /// \param resetRect Should the texture rect be reset to the size of the new texture?
    ///
    /// \see getTexture, setTextureRect
    ///
    ////////////////////////////////////////////////////////////
    void setTexture(const sf::Texture& texture, bool resetRect = false);

    ////////////////////////////////////////////////////////////
    /// \brief Set the sub-rectangle of the texture that the sprite will display
    ///
    /// The texture rect is useful when you don't want to display
    /// the whole texture, but rather a part of it.
    /// By default, the texture rect covers the entire texture.
    ///
    /// \param rectangle Rectangle defining the region of the texture to display
    ///
    /// \see getTextureRect, setTexture
    ///
    ////////////////////////////////////////////////////////////
    void setTextureRect(const sf::FloatRect& rectangle);

    ////////////////////////////////////////////////////////////
    /// \brief Set the global color of the sprite
    ///
    /// This color is modulated (multiplied) with the sprite's
    /// texture. It can be used to colorize the sprite, or change
    /// its global opacity.
    /// By default, the sprite's color is opaque white.
    ///
    /// \param color New color of the sprite
    ///
    /// \see getColor
    ///
    ////////////////////////////////////////////////////////////
    void setColor(sf::Color color);

    ////////////////////////////////////////////////////////////
    /// \brief Get the source texture of the sprite
    ///
    /// If the sprite has no source texture, a NULL pointer is returned.
    /// The returned pointer is const, which means that you can't
    /// modify the texture when you retrieve it with this function.
    ///
    /// \return Pointer to the sprite's texture
    ///
    /// \see setTexture
    ///
    ////////////////////////////////////////////////////////////
    const sf::Texture* getTexture() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the sub-rectangle of the texture displayed by the sprite
    ///
    /// \return Texture rectangle of the sprite
    ///
    /// \see setTextureRect
    ///
    ////////////////////////////////////////////////////////////
    const sf::FloatRect& getTextureRect() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the global color of the sprite
    ///
    /// \return Global color of the sprite
    ///
    /// \see setColor
    ///
    ////////////////////////////////////////////////////////////
    sf::Color getColor() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the local bounding rectangle of the entity
    ///
    /// The returned rectangle is in local coordinates, which means
    /// that it ignores the transformations (translation, rotation,
    /// scale, ...) that are applied to the entity.
    /// In other words, this function returns the bounds of the
    /// entity in the entity's coordinate system.
    ///
    /// \return Local bounding rectangle of the entity
    ///
    ////////////////////////////////////////////////////////////
    sf::IntRect getLocalBounds() const;

	float getSkewX() const;
	float getSkewY() const;
	float getPinchX() const;
	float getPinchY() const;
	bool getTexturePerspective() const;
	float getPerspectiveCoefficient() const;
	float getRotationX() const;
	float getRotationY() const;
	float getRotationZ() const;

	void setSkewX( float x );
	void setSkewY( float y );
	void setPinchX( float x );
	void setPinchY( float y );
	void setScale( const sf::Vector2f &s );
	void setCorners( float tl_x, float tl_y, float tr_x, float tr_y, float bl_x, float bl_y, float br_x, float br_y );
	void setTexturePerspective( bool texture_perspective );
	void setPerspectiveCoefficient( float perspective_coefficient );
	void setRotationX( float r );
	void setRotationY( float r );
	void setRotationZ( float r );

	using sf::Transformable::getRotation;
	using sf::Transformable::setRotation;
	void setOriginZ( float z );
	void setSize( const sf::Vector2f &size );

    virtual void setPosition( float x, float y );
    virtual void setPosition( const sf::Vector2f &position );

private :

    ////////////////////////////////////////////////////////////
    /// \brief Draw the sprite to a render target
    ///
    /// \param target Render target to draw to
    /// \param states Current render states
    ///
    ////////////////////////////////////////////////////////////
    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;

    ////////////////////////////////////////////////////////////
    /// \brief Update the vertices' geometry and texture coordinates
    ///
    ////////////////////////////////////////////////////////////
    void updateGeometry();
    void updateCorners();
    void updateCornersWithRotation( sf::Vector2f screen_center );

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
	sf::VertexArray m_vertices;
	const sf::Texture* m_texture;     ///< Texture of the sprite
	sf::FloatRect m_textureRect;      ///< Rectangle defining the area of the source texture to display
	sf::Vector2f m_pinch;
	sf::Vector2f m_skew;
	sf::Vector2f m_top_left;
	sf::Vector2f m_top_right;
	sf::Vector2f m_bottom_left;
	sf::Vector2f m_bottom_right;
	bool m_texture_perspective;
	float m_perspective_coefficient;
	float m_rotation_x;
	float m_rotation_y;
	float m_rotation_z;
	Quaternion m_orientation;
	bool m_update_corners;
	float m_origin_z;
	bool m_back_facing;
	sf::Vector2f m_size;
};


#endif // FE_SPRITE_HPP
