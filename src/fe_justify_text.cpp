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
 * Adapted from SFML 3.0.1 Text.cpp.
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
#include "fe_justify_text.hpp"
#include "fe_types.hpp"

#include <algorithm>
#include <utility>

#include <cmath>
#include <cstddef>
#include <cstdint>

void append_vertex(
	std::vector<FeRenderVertex> &vertices,
	float x,
	float y,
	const Color &color,
	float u,
	float v )
{
	FeRenderVertex vertex = {};
	vertex.x = x;
	vertex.y = y;
	vertex.z = 0.0f;
	vertex.u = u;
	vertex.v = v;
	vertex.r = color.r;
	vertex.g = color.g;
	vertex.b = color.b;
	vertex.a = color.a;
	vertices.push_back( vertex );
}

// Add an underline or strikethrough line to the vertex array
void addLine(
	std::vector<FeRenderVertex> &vertices,
	float lineLength,
	float lineTop,
	Color color,
	float offset,
	float thickness,
	float outlineThickness = 0)
{
	const float top = std::floor( lineTop + offset - ( thickness / 2 ) + 0.5f );
	const float bottom = top + std::floor( thickness + 0.5f );

	append_vertex( vertices, -outlineThickness, top - outlineThickness, color, 1.0f, 1.0f );
	append_vertex( vertices, lineLength + outlineThickness, top - outlineThickness, color, 1.0f, 1.0f );
	append_vertex( vertices, -outlineThickness, bottom + outlineThickness, color, 1.0f, 1.0f );
	append_vertex( vertices, -outlineThickness, bottom + outlineThickness, color, 1.0f, 1.0f );
	append_vertex( vertices, lineLength + outlineThickness, top - outlineThickness, color, 1.0f, 1.0f );
	append_vertex( vertices, lineLength + outlineThickness, bottom + outlineThickness, color, 1.0f, 1.0f );
}

// Add a glyph quad to the vertex array
void addGlyphQuad(
	std::vector<FeRenderVertex> &vertices,
	const Vec2f &position,
	Color color,
	const FeGlyph &glyph,
	float italicShear )
{
	const Vec2f padding( 1.f, 1.f );

	const Vec2f p1( glyph.bounds.position.x - padding.x, glyph.bounds.position.y - padding.y );
	const Vec2f p2(
		glyph.bounds.position.x + glyph.bounds.size.x + padding.x,
		glyph.bounds.position.y + glyph.bounds.size.y + padding.y );

	const Vec2f uv1(
		static_cast<float>( glyph.textureRect.position.x ) - padding.x,
		static_cast<float>( glyph.textureRect.position.y ) - padding.y );
	const Vec2f uv2(
		static_cast<float>( glyph.textureRect.position.x + glyph.textureRect.size.x ) + padding.x,
		static_cast<float>( glyph.textureRect.position.y + glyph.textureRect.size.y ) + padding.y );

	append_vertex( vertices, position.x + p1.x - italicShear * p1.y, position.y + p1.y, color, uv1.x, uv1.y );
	append_vertex( vertices, position.x + p2.x - italicShear * p1.y, position.y + p1.y, color, uv2.x, uv1.y );
	append_vertex( vertices, position.x + p1.x - italicShear * p2.y, position.y + p2.y, color, uv1.x, uv2.y );
	append_vertex( vertices, position.x + p1.x - italicShear * p2.y, position.y + p2.y, color, uv1.x, uv2.y );
	append_vertex( vertices, position.x + p2.x - italicShear * p1.y, position.y + p1.y, color, uv2.x, uv1.y );
	append_vertex( vertices, position.x + p2.x - italicShear * p2.y, position.y + p2.y, color, uv2.x, uv2.y );
}


////////////////////////////////////////////////////////////
FeJustifyText::FeJustifyText(
	const FeFont &font,
	std::basic_string<std::uint32_t> string,
	unsigned int characterSize ) :
m_string(std::move(string)),
m_font(&font),
m_characterSize(characterSize)
{
}


////////////////////////////////////////////////////////////
void FeJustifyText::setWidth(unsigned int width) // AM+
{
    if (m_width != width)
    {
        m_width              = width;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void FeJustifyText::setJustify(std::uint32_t justify) // AM+
{
    if (m_justify != justify)
    {
        m_justify            = justify;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void FeJustifyText::setString( const std::basic_string<std::uint32_t> &string )
{
    if (m_string != string)
    {
        m_string             = string;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void FeJustifyText::setFont(const FeFont& font)
{
    if (m_font != &font)
    {
        m_font               = &font;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void FeJustifyText::setCharacterSize(unsigned int size)
{
    if (m_characterSize != size)
    {
        m_characterSize      = size;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void FeJustifyText::setLetterSpacing(float spacingFactor)
{
    if (m_letterSpacingFactor != spacingFactor)
    {
        m_letterSpacingFactor = spacingFactor;
        m_geometryNeedUpdate  = true;
    }
}


////////////////////////////////////////////////////////////
void FeJustifyText::setLineSpacing(float spacingFactor)
{
    if (m_lineSpacingFactor != spacingFactor)
    {
        m_lineSpacingFactor  = spacingFactor;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void FeJustifyText::setStyle(std::uint32_t style)
{
    if (m_style != style)
    {
        m_style              = style;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void FeJustifyText::setFillColor( Color color )
{
    if (color != m_fillColor)
    {
        m_fillColor = color;

        // Change vertex colors directly, no need to update whole geometry
        // (if geometry is updated anyway, we can skip this step)
        if (!m_geometryNeedUpdate)
        {
            for ( FeRenderVertex &vertex : m_vertices )
            {
                vertex.r = m_fillColor.r;
                vertex.g = m_fillColor.g;
                vertex.b = m_fillColor.b;
                vertex.a = m_fillColor.a;
            }
        }
    }
}


////////////////////////////////////////////////////////////
void FeJustifyText::setOutlineColor( Color color )
{
    if (color != m_outlineColor)
    {
        m_outlineColor = color;

        // Change vertex colors directly, no need to update whole geometry
        // (if geometry is updated anyway, we can skip this step)
        if (!m_geometryNeedUpdate)
        {
            for ( FeRenderVertex &vertex : m_outlineVertices )
            {
                vertex.r = m_outlineColor.r;
                vertex.g = m_outlineColor.g;
                vertex.b = m_outlineColor.b;
                vertex.a = m_outlineColor.a;
            }
        }
    }
}


////////////////////////////////////////////////////////////
void FeJustifyText::setOutlineThickness(float thickness)
{
    if (thickness != m_outlineThickness)
    {
        m_outlineThickness   = thickness;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
unsigned int FeJustifyText::getWidth() const // AM+
{
    return m_width;
}


////////////////////////////////////////////////////////////
std::uint32_t FeJustifyText::getJustify() const // AM+
{
    return m_justify;
}


////////////////////////////////////////////////////////////
const std::basic_string<std::uint32_t> &FeJustifyText::getString() const
{
    return m_string;
}


////////////////////////////////////////////////////////////
const FeFont& FeJustifyText::getFont() const
{
    return *m_font;
}


////////////////////////////////////////////////////////////
unsigned int FeJustifyText::getCharacterSize() const
{
    return m_characterSize;
}


////////////////////////////////////////////////////////////
float FeJustifyText::getLetterSpacing() const
{
    return m_letterSpacingFactor;
}


////////////////////////////////////////////////////////////
float FeJustifyText::getLineSpacing() const
{
    return m_lineSpacingFactor;
}


////////////////////////////////////////////////////////////
std::uint32_t FeJustifyText::getStyle() const
{
    return m_style;
}


////////////////////////////////////////////////////////////
Color FeJustifyText::getFillColor() const
{
    return m_fillColor;
}


////////////////////////////////////////////////////////////
Color FeJustifyText::getOutlineColor() const
{
    return m_outlineColor;
}


////////////////////////////////////////////////////////////
float FeJustifyText::getOutlineThickness() const
{
    return m_outlineThickness;
}


////////////////////////////////////////////////////////////
void FeJustifyText::setPosition( const Vec2f &position )
{
	m_position = position;
}


////////////////////////////////////////////////////////////
void FeJustifyText::setRotation( float angle )
{
	m_rotation = angle;
}


////////////////////////////////////////////////////////////
void FeJustifyText::setScale( const Vec2f &scale )
{
	m_scale = scale;
}


////////////////////////////////////////////////////////////
void FeJustifyText::setOrigin( const Vec2f &origin )
{
	m_origin = origin;
}


////////////////////////////////////////////////////////////
Vec2f FeJustifyText::getPosition() const
{
	return m_position;
}


////////////////////////////////////////////////////////////
float FeJustifyText::getRotation() const
{
	return m_rotation;
}


////////////////////////////////////////////////////////////
Vec2f FeJustifyText::getScale() const
{
	return m_scale;
}


////////////////////////////////////////////////////////////
Vec2f FeJustifyText::getOrigin() const
{
	return m_origin;
}


////////////////////////////////////////////////////////////
Vec2f FeJustifyText::transformPoint( const Vec2f &point ) const
{
	const Vec2f local_point = point - m_origin;
	const Vec2f scaled_point( local_point.x * m_scale.x, local_point.y * m_scale.y );

	const float radians = m_rotation * ( 3.141592654f / 180.0f );
	const float sine = std::sin( radians );
	const float cosine = std::cos( radians );

	return Vec2f(
		( scaled_point.x * cosine ) - ( scaled_point.y * sine ) + m_position.x,
		( scaled_point.x * sine ) + ( scaled_point.y * cosine ) + m_position.y );
}


////////////////////////////////////////////////////////////
Vec2f FeJustifyText::findCharacterPos( std::size_t index ) const
{
    // Adjust the index if it's out of range
    index = std::min(index, m_string.size());

    // Precompute the variables needed by the algorithm
    const bool  isBold          = m_style & Bold;
    float       whitespaceWidth = m_font->getGlyph(U' ', m_characterSize, isBold).advance;
    const float letterSpacing   = (whitespaceWidth / 3.f) * (m_letterSpacingFactor - 1.f);
    whitespaceWidth += letterSpacing;
    const float lineSpacing = m_font->getLineSpacing(m_characterSize) * m_lineSpacingFactor;

    // Compute the position
    Vec2f       position;
    std::uint32_t prevChar = 0;
    for (std::size_t i = 0; i < index; ++i)
    {
        const std::uint32_t curChar = m_string[i];

        // Apply the kerning offset
        position.x += m_font->getKerning(prevChar, curChar, m_characterSize, isBold);
        prevChar = curChar;

        // Handle special characters
        switch (curChar)
        {
            case U' ':
                position.x += whitespaceWidth;
                continue;
            case U'\t':
                position.x += whitespaceWidth * 4;
                continue;
            case U'\n':
                position.y += lineSpacing;
                position.x = 0;
                continue;
        }

        // For regular characters, add the advance offset of the glyph
        position.x += m_font->getGlyph(curChar, m_characterSize, isBold).advance + letterSpacing;
    }

    // Transform the position to global coordinates
    return transformPoint( position );
}


////////////////////////////////////////////////////////////
FloatRect FeJustifyText::getLocalBounds() const
{
    ensureGeometryUpdate();

    return m_bounds;
}


////////////////////////////////////////////////////////////
FloatRect FeJustifyText::getGlobalBounds() const
{
	return transformRect( getLocalBounds() );
}


////////////////////////////////////////////////////////////
const std::vector<FeRenderVertex> &FeJustifyText::getFillGeometry() const
{
    ensureGeometryUpdate();
    return m_vertices;
}


////////////////////////////////////////////////////////////
const std::vector<FeRenderVertex> &FeJustifyText::getOutlineGeometry() const
{
    ensureGeometryUpdate();
    return m_outlineVertices;
}


////////////////////////////////////////////////////////////
const FeFont::TexturePageId* FeJustifyText::getTexturePageId() const
{
    ensureGeometryUpdate();
    return m_font->getTexturePageId(m_characterSize);
}


////////////////////////////////////////////////////////////
Vec2u FeJustifyText::getTextureSize() const
{
    ensureGeometryUpdate();

    unsigned int width = 0;
    unsigned int height = 0;
    m_font->getTextureSize(m_characterSize, width, height);
    return Vec2u( width, height );
}


////////////////////////////////////////////////////////////
std::uint64_t FeJustifyText::getTextureVersion() const
{
    ensureGeometryUpdate();
    return m_fontTextureId;
}


////////////////////////////////////////////////////////////
FloatRect FeJustifyText::transformRect( const FloatRect &rect ) const
{
	const Vec2f p0 = transformPoint( rect.position );
	const Vec2f p1 = transformPoint( Vec2f( rect.position.x + rect.size.x, rect.position.y ) );
	const Vec2f p2 = transformPoint( Vec2f( rect.position.x, rect.position.y + rect.size.y ) );
	const Vec2f p3 = transformPoint( rect.position + rect.size );

	const float min_x = std::min( std::min( p0.x, p1.x ), std::min( p2.x, p3.x ) );
	const float min_y = std::min( std::min( p0.y, p1.y ), std::min( p2.y, p3.y ) );
	const float max_x = std::max( std::max( p0.x, p1.x ), std::max( p2.x, p3.x ) );
	const float max_y = std::max( std::max( p0.y, p1.y ), std::max( p2.y, p3.y ) );

	return FloatRect( min_x, min_y, max_x - min_x, max_y - min_y );
}


////////////////////////////////////////////////////////////
// Use the difference between bounds and width to update the spacing
void FeJustifyText::justifySpacing(float &whitespaceWidth, float &letterSpacing, bool isBold, float italicShear) const
{
    if (m_width <= 0 ) return;

    int spaces = 0;
    int chars = 0;
    std::uint32_t prevChar = 0;
    auto minX = static_cast<float>(m_characterSize);
    float maxX = 0.f;
    float x = 0.f;

    // Calculate maxX using the same method as `ensureGeometryUpdate`
    for (const std::uint32_t curChar : m_string)
    {
        if (curChar == U'\r') continue;
        chars += 1;
        x += m_font->getKerning(prevChar, curChar, m_characterSize, isBold);
        prevChar = curChar;
        if ((curChar == U' ') || (curChar == U'\n') || (curChar == U'\t'))
        {
            minX = std::min(minX, x);
            switch (curChar)
            {
                case U' ':
                    spaces += 1;
                    x += whitespaceWidth;
                    break;
                case U'\t':
                    chars += 3;
                    spaces += 4;
                    x += whitespaceWidth * 4;
                    break;
                case U'\n':
                    chars = 0;
                    spaces = 0;
                    x = 0;
                    break;
            }
            maxX = std::max(maxX, x);
            continue;
        }
        const FeGlyph &glyph = m_font->getGlyph(curChar, m_characterSize, isBold);
        const Vec2f p1 = glyph.bounds.position;
        const Vec2f p2 = glyph.bounds.position + glyph.bounds.size;
        minX = std::min(minX, x + p1.x - italicShear * p2.y);
        maxX = std::max(maxX, x + p2.x - italicShear * p1.y);
        x += glyph.advance + letterSpacing;
    }

    float gap = m_width - ( maxX - minX );

    if ( m_justify == Word && spaces > 0 )
    {
        // For "Word" justify increase whitespace to fill gap
        whitespaceWidth += gap / spaces;

    } else if ( m_justify == Character && chars > 1 )
    {
        // For "Character" justify increase both spacings to fill gap
        // (chars - 1) prevents trailing space on last letter
        letterSpacing += gap / (chars - 1);
        whitespaceWidth += gap / (chars - 1);
    }
}


////////////////////////////////////////////////////////////
void FeJustifyText::ensureGeometryUpdate() const
{
    // Do nothing, if geometry has not changed and the font texture has not changed
    if (!m_geometryNeedUpdate && m_font->getTextureVersion(m_characterSize) == m_fontTextureId)
        return;

    // Save the current fonts texture id
    m_fontTextureId = m_font->getTextureVersion(m_characterSize);

    // Mark geometry as updated
    m_geometryNeedUpdate = false;

    // Clear the previous geometry
    m_vertices.clear();
    m_outlineVertices.clear();
    m_bounds = FloatRect();

    // No text: nothing to draw
    if (m_string.empty())
        return;

    // Compute values related to the text style
    const bool  isBold             = m_style & Bold;
    const bool  isUnderlined       = m_style & Underlined;
    const bool  isStrikeThrough    = m_style & StrikeThrough;
    const float italicShear        = (m_style & Italic) ? ( 12.0f * ( 3.141592654f / 180.0f ) ) : 0.f;
    const float underlineOffset    = m_font->getUnderlinePosition(m_characterSize);
    const float underlineThickness = m_font->getUnderlineThickness(m_characterSize);

    // Compute the location of the strike through dynamically
    // We use the center point of the lowercase 'x' glyph as the reference
    // We reuse the underline thickness as the thickness of the strike through as well
    const FeGlyph &strike_through_glyph = m_font->getGlyph(U'x', m_characterSize, isBold);
    const float strikeThroughOffset =
        strike_through_glyph.bounds.position.y + ( strike_through_glyph.bounds.size.y * 0.5f );

    // Precompute the variables needed by the algorithm
    float       whitespaceWidth = m_font->getGlyph(U' ', m_characterSize, isBold).advance;
    float letterSpacing   = (whitespaceWidth / 3.f) * (m_letterSpacingFactor - 1.f); // AM+
    whitespaceWidth += letterSpacing;
    const float lineSpacing = m_font->getLineSpacing(m_characterSize) * m_lineSpacingFactor;
    float       x           = 0.f;
    auto        y           = static_cast<float>(m_characterSize);

    // Create one quad for each character
    auto          minX     = static_cast<float>(m_characterSize);
    auto          minY     = static_cast<float>(m_characterSize);
    float         maxX     = 0.f;
    float         maxY     = 0.f;
    std::uint32_t prevChar = 0;

    // --- AM+
    float px = 0.f;
    bool is_justify = ( m_justify == Word || m_justify == Character );
    if (is_justify) justifySpacing(whitespaceWidth, letterSpacing, isBold, italicShear);
    // --- AM+

    for (const std::uint32_t curChar : m_string)
    {
        // Skip the \r char to avoid weird graphical issues
        if (curChar == U'\r')
            continue;

        // Apply the kerning offset
        x += m_font->getKerning(prevChar, curChar, m_characterSize, isBold);

        // Justify may result is sub-pixel positions - round to prevent blur
        px = is_justify ? std::round(x) : x; // AM+

        // If we're using the underlined style and there's a new line, draw a line
        if (isUnderlined && (curChar == U'\n' && prevChar != U'\n'))
        {
            addLine(m_vertices, px, y, m_fillColor, underlineOffset, underlineThickness);

            if (m_outlineThickness != 0)
                addLine(m_outlineVertices, px, y, m_outlineColor, underlineOffset, underlineThickness, m_outlineThickness);
        }

        // If we're using the strike through style and there's a new line, draw a line across all characters
        if (isStrikeThrough && (curChar == U'\n' && prevChar != U'\n'))
        {
            addLine(m_vertices, px, y, m_fillColor, strikeThroughOffset, underlineThickness);

            if (m_outlineThickness != 0)
                addLine(m_outlineVertices, px, y, m_outlineColor, strikeThroughOffset, underlineThickness, m_outlineThickness);
        }

        prevChar = curChar;

        // Handle special characters
        if ((curChar == U' ') || (curChar == U'\n') || (curChar == U'\t'))
        {
            // Update the current bounds (min coordinates)
            minX = std::min(minX, x);
            minY = std::min(minY, y);

            switch (curChar)
            {
                case U' ':
                    x += whitespaceWidth;
                    break;
                case U'\t':
                    x += whitespaceWidth * 4;
                    break;
                case U'\n':
                    y += lineSpacing;
                    x = 0;
                    break;
            }

            // Update the current bounds (max coordinates)
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);

            // Next glyph, no need to create a quad for whitespace
            continue;
        }

        // Apply the outline
        if (m_outlineThickness != 0)
        {
            const FeGlyph &glyph = m_font->getGlyph(curChar, m_characterSize, isBold, m_outlineThickness);

            // Add the outline glyph to the vertices
            addGlyphQuad(m_outlineVertices, Vec2f(px, y), m_outlineColor, glyph, italicShear);
        }

        // Extract the current glyph's description
        const FeGlyph &glyph = m_font->getGlyph(curChar, m_characterSize, isBold);

        // Add the glyph to the vertices
        addGlyphQuad(m_vertices, Vec2f(px, y), m_fillColor, glyph, italicShear);

        // Update the current bounds
        const Vec2f p1 = glyph.bounds.position;
        const Vec2f p2 = glyph.bounds.position + glyph.bounds.size;

        minX = std::min(minX, x + p1.x - italicShear * p2.y);
        maxX = std::max(maxX, x + p2.x - italicShear * p1.y);
        minY = std::min(minY, y + p1.y);
        maxY = std::max(maxY, y + p2.y);

        // Advance to the next character
        x += glyph.advance + letterSpacing;
    }

    // If we're using outline, update the current bounds
    if (m_outlineThickness != 0)
    {
        const float outline = std::abs(std::ceil(m_outlineThickness));
        minX -= outline;
        maxX += outline;
        minY -= outline;
        maxY += outline;
    }

    // Justify may result is sub-pixel positions - round to prevent blur
    px = is_justify ? std::round(x) : x; // AM+

    // If we're using the underlined style, add the last line
    if (isUnderlined && (x > 0))
    {
        addLine(m_vertices, px, y, m_fillColor, underlineOffset, underlineThickness);

        if (m_outlineThickness != 0)
            addLine(m_outlineVertices, px, y, m_outlineColor, underlineOffset, underlineThickness, m_outlineThickness);
    }

    // If we're using the strike through style, add the last line across all characters
    if (isStrikeThrough && (x > 0))
    {
        addLine(m_vertices, px, y, m_fillColor, strikeThroughOffset, underlineThickness);

        if (m_outlineThickness != 0)
            addLine(m_outlineVertices, px, y, m_outlineColor, strikeThroughOffset, underlineThickness, m_outlineThickness);
    }

    // Update the bounding rectangle
    m_bounds.position = Vec2f( minX, minY );
    m_bounds.size = Vec2f( maxX - minX, maxY - minY );
}
