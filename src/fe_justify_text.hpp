/*
 *
 *  Attract-Mode Plus frontend
 *  Copyright (C) 2026 Chadnaut & Radek Dutkiewicz
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

/*
 * Adapted from SFML 3.0.1 Text.hpp / Text.cpp.
 * Original SFML notice:
 *
 * SFML - Simple and Fast Multimedia Library
 * Copyright (C) 2007-2025 Laurent Gomila (laurent@sfml-dev.org)
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the original SFML notice and restrictions.
 */

#ifndef FE_JUSTIFY_TEXT_HPP
#define FE_JUSTIFY_TEXT_HPP

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/VertexArray.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

#include "fe_font.hpp"
#include "fe_types.hpp"

class FeJustifyText
{
public:
	enum Style
	{
		Regular = 0,
		Bold = 1 << 0,
		Italic = 1 << 1,
		Underlined = 1 << 2,
		StrikeThrough = 1 << 3
	};

	enum Justify
	{
		None = 0,
		Word = 1,
		Character = 2
	};

	explicit FeJustifyText(
		const FeFont &font,
		std::basic_string<std::uint32_t> string = std::basic_string<std::uint32_t>(),
		unsigned int characterSize = 30 );
	FeJustifyText(
		const FeFont &&font,
		std::basic_string<std::uint32_t> string = std::basic_string<std::uint32_t>(),
		unsigned int characterSize = 30 ) = delete;

	void setWidth( unsigned int width );
	void setJustify( std::uint32_t justify );
	void setString( const std::basic_string<std::uint32_t> &string );
	void setFont( const FeFont &font );
	void setFont( const FeFont &&font ) = delete;
	void setCharacterSize( unsigned int size );
	void setLetterSpacing( float spacingFactor );
	void setLineSpacing( float spacingFactor );
	void setStyle( std::uint32_t style );
	void setFillColor( Color color );
	void setOutlineColor( Color color );
	void setOutlineThickness( float thickness );
	void setPosition( const Vec2f &position );
	void setPosition( float x, float y ) { setPosition( Vec2f( x, y ) ); }
	void setRotation( float angle );
	void setScale( const Vec2f &scale );
	void setScale( float x, float y ) { setScale( Vec2f( x, y ) ); }
	void setOrigin( const Vec2f &origin );
	void setOrigin( float x, float y ) { setOrigin( Vec2f( x, y ) ); }

	[[nodiscard]] unsigned int getWidth() const;
	[[nodiscard]] std::uint32_t getJustify() const;
	[[nodiscard]] const std::basic_string<std::uint32_t> &getString() const;
	[[nodiscard]] const FeFont &getFont() const;
	[[nodiscard]] unsigned int getCharacterSize() const;
	[[nodiscard]] float getLetterSpacing() const;
	[[nodiscard]] float getLineSpacing() const;
	[[nodiscard]] std::uint32_t getStyle() const;
	[[nodiscard]] Color getFillColor() const;
	[[nodiscard]] Color getOutlineColor() const;
	[[nodiscard]] float getOutlineThickness() const;
	[[nodiscard]] Vec2f getPosition() const;
	[[nodiscard]] float getRotation() const;
	[[nodiscard]] Vec2f getScale() const;
	[[nodiscard]] Vec2f getOrigin() const;
	[[nodiscard]] Vec2f findCharacterPos( std::size_t index ) const;
	[[nodiscard]] FloatRect getLocalBounds() const;
	[[nodiscard]] FloatRect getGlobalBounds() const;
	[[nodiscard]] const sf::VertexArray &getFillGeometry() const;
	[[nodiscard]] const sf::VertexArray &getOutlineGeometry() const;
	[[nodiscard]] const FeFont::TexturePageId *getTexturePageId() const;
	[[nodiscard]] Vec2u getTextureSize() const;
	[[nodiscard]] std::uint64_t getTextureVersion() const;
	[[nodiscard]] Vec2f transformPoint( const Vec2f &point ) const;

private:
	void justifySpacing( float &whitespaceWidth, float &letterSpacing, bool isBold, float italicShear ) const;
	void ensureGeometryUpdate() const;
	[[nodiscard]] FloatRect transformRect( const FloatRect &rect ) const;

	unsigned int m_width{ 0 };
	std::uint32_t m_justify{ None };
	std::basic_string<std::uint32_t> m_string;
	const FeFont *m_font{};
	unsigned int m_characterSize{ 30 };
	float m_letterSpacingFactor{ 1.0f };
	float m_lineSpacingFactor{ 1.0f };
	std::uint32_t m_style{ Regular };
	Color m_fillColor{ Color::White };
	Color m_outlineColor{ Color::Black };
	float m_outlineThickness{ 0.0f };
	mutable sf::VertexArray m_vertices{ sf::PrimitiveType::Triangles };
	mutable sf::VertexArray m_outlineVertices{ sf::PrimitiveType::Triangles };
	mutable FloatRect m_bounds;
	mutable bool m_geometryNeedUpdate{};
	mutable std::uint64_t m_fontTextureId{};
	Vec2f m_position;
	float m_rotation{ 0.0f };
	Vec2f m_scale{ 1.0f, 1.0f };
	Vec2f m_origin;
};

#endif
