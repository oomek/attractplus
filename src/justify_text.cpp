////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2025 Laurent Gomila (laurent@sfml-dev.org)
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

//
// Originally SFML 3.0.1 /include/SFML/Graphics/Text.cpp
//
// - Additions marked with 'AM+' comments
//

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include "justify_text.hpp" // AM+
#include <SFML/Graphics/Texture.hpp>

#include <algorithm>
#include <utility>

#include <cmath>
#include <cstddef>
#include <cstdint>


namespace
{
// Add an underline or strikethrough line to the vertex array
void addLine(sf::VertexArray& vertices,
             float            lineLength,
             float            lineTop,
             sf::Color        color,
             float            offset,
             float            thickness,
             float            outlineThickness = 0)
{
    const float top    = std::floor(lineTop + offset - (thickness / 2) + 0.5f);
    const float bottom = top + std::floor(thickness + 0.5f);

    vertices.append({{-outlineThickness, top - outlineThickness}, color, {1.0f, 1.0f}});
    vertices.append({{lineLength + outlineThickness, top - outlineThickness}, color, {1.0f, 1.0f}});
    vertices.append({{-outlineThickness, bottom + outlineThickness}, color, {1.0f, 1.0f}});
    vertices.append({{-outlineThickness, bottom + outlineThickness}, color, {1.0f, 1.0f}});
    vertices.append({{lineLength + outlineThickness, top - outlineThickness}, color, {1.0f, 1.0f}});
    vertices.append({{lineLength + outlineThickness, bottom + outlineThickness}, color, {1.0f, 1.0f}});
}

// Add a glyph quad to the vertex array
void addGlyphQuad(sf::VertexArray& vertices, sf::Vector2f position, sf::Color color, const sf::Glyph& glyph, float italicShear)
{
    const sf::Vector2f padding(1.f, 1.f);

    const sf::Vector2f p1 = glyph.bounds.position - padding;
    const sf::Vector2f p2 = glyph.bounds.position + glyph.bounds.size + padding;

    const auto uv1 = sf::Vector2f(glyph.textureRect.position) - padding;
    const auto uv2 = sf::Vector2f(glyph.textureRect.position + glyph.textureRect.size) + padding;

    vertices.append({position + sf::Vector2f(p1.x - italicShear * p1.y, p1.y), color, {uv1.x, uv1.y}});
    vertices.append({position + sf::Vector2f(p2.x - italicShear * p1.y, p1.y), color, {uv2.x, uv1.y}});
    vertices.append({position + sf::Vector2f(p1.x - italicShear * p2.y, p2.y), color, {uv1.x, uv2.y}});
    vertices.append({position + sf::Vector2f(p1.x - italicShear * p2.y, p2.y), color, {uv1.x, uv2.y}});
    vertices.append({position + sf::Vector2f(p2.x - italicShear * p1.y, p1.y), color, {uv2.x, uv1.y}});
    vertices.append({position + sf::Vector2f(p2.x - italicShear * p2.y, p2.y), color, {uv2.x, uv2.y}});
}
} // namespace


namespace sf
{
////////////////////////////////////////////////////////////
JustifyText::JustifyText(const Font& font, String string, unsigned int characterSize) :
m_string(std::move(string)),
m_font(&font),
m_characterSize(characterSize)
{
}


////////////////////////////////////////////////////////////
void JustifyText::setWidth(unsigned int width) // AM+
{
    if (m_width != width)
    {
        m_width              = width;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void JustifyText::setJustify(std::uint32_t justify) // AM+
{
    if (m_justify != justify)
    {
        m_justify            = justify;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void JustifyText::setString(const String& string)
{
    if (m_string != string)
    {
        m_string             = string;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void JustifyText::setFont(const Font& font)
{
    if (m_font != &font)
    {
        m_font               = &font;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void JustifyText::setCharacterSize(unsigned int size)
{
    if (m_characterSize != size)
    {
        m_characterSize      = size;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void JustifyText::setLetterSpacing(float spacingFactor)
{
    if (m_letterSpacingFactor != spacingFactor)
    {
        m_letterSpacingFactor = spacingFactor;
        m_geometryNeedUpdate  = true;
    }
}


////////////////////////////////////////////////////////////
void JustifyText::setLineSpacing(float spacingFactor)
{
    if (m_lineSpacingFactor != spacingFactor)
    {
        m_lineSpacingFactor  = spacingFactor;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void JustifyText::setStyle(std::uint32_t style)
{
    if (m_style != style)
    {
        m_style              = style;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
void JustifyText::setFillColor(Color color)
{
    if (color != m_fillColor)
    {
        m_fillColor = color;

        // Change vertex colors directly, no need to update whole geometry
        // (if geometry is updated anyway, we can skip this step)
        if (!m_geometryNeedUpdate)
        {
            for (std::size_t i = 0; i < m_vertices.getVertexCount(); ++i)
                m_vertices[i].color = m_fillColor;
        }
    }
}


////////////////////////////////////////////////////////////
void JustifyText::setOutlineColor(Color color)
{
    if (color != m_outlineColor)
    {
        m_outlineColor = color;

        // Change vertex colors directly, no need to update whole geometry
        // (if geometry is updated anyway, we can skip this step)
        if (!m_geometryNeedUpdate)
        {
            for (std::size_t i = 0; i < m_outlineVertices.getVertexCount(); ++i)
                m_outlineVertices[i].color = m_outlineColor;
        }
    }
}


////////////////////////////////////////////////////////////
void JustifyText::setOutlineThickness(float thickness)
{
    if (thickness != m_outlineThickness)
    {
        m_outlineThickness   = thickness;
        m_geometryNeedUpdate = true;
    }
}


////////////////////////////////////////////////////////////
unsigned int JustifyText::getWidth() const // AM+
{
    return m_width;
}


////////////////////////////////////////////////////////////
std::uint32_t JustifyText::getJustify() const // AM+
{
    return m_justify;
}


////////////////////////////////////////////////////////////
const String& JustifyText::getString() const
{
    return m_string;
}


////////////////////////////////////////////////////////////
const Font& JustifyText::getFont() const
{
    return *m_font;
}


////////////////////////////////////////////////////////////
unsigned int JustifyText::getCharacterSize() const
{
    return m_characterSize;
}


////////////////////////////////////////////////////////////
float JustifyText::getLetterSpacing() const
{
    return m_letterSpacingFactor;
}


////////////////////////////////////////////////////////////
float JustifyText::getLineSpacing() const
{
    return m_lineSpacingFactor;
}


////////////////////////////////////////////////////////////
std::uint32_t JustifyText::getStyle() const
{
    return m_style;
}


////////////////////////////////////////////////////////////
Color JustifyText::getFillColor() const
{
    return m_fillColor;
}


////////////////////////////////////////////////////////////
Color JustifyText::getOutlineColor() const
{
    return m_outlineColor;
}


////////////////////////////////////////////////////////////
float JustifyText::getOutlineThickness() const
{
    return m_outlineThickness;
}


////////////////////////////////////////////////////////////
Vector2f JustifyText::findCharacterPos(std::size_t index) const
{
    // Adjust the index if it's out of range
    index = std::min(index, m_string.getSize());

    // Precompute the variables needed by the algorithm
    const bool  isBold          = m_style & Bold;
    float       whitespaceWidth = m_font->getGlyph(U' ', m_characterSize, isBold).advance;
    const float letterSpacing   = (whitespaceWidth / 3.f) * (m_letterSpacingFactor - 1.f);
    whitespaceWidth += letterSpacing;
    const float lineSpacing = m_font->getLineSpacing(m_characterSize) * m_lineSpacingFactor;

    // Compute the position
    Vector2f      position;
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
    return getTransform().transformPoint(position);
}


////////////////////////////////////////////////////////////
FloatRect JustifyText::getLocalBounds() const
{
    ensureGeometryUpdate();

    return m_bounds;
}


////////////////////////////////////////////////////////////
FloatRect JustifyText::getGlobalBounds() const
{
    return getTransform().transformRect(getLocalBounds());
}


////////////////////////////////////////////////////////////
void JustifyText::draw(RenderTarget& target, RenderStates states) const
{
    ensureGeometryUpdate();

    states.transform *= getTransform();
    states.texture        = &m_font->getTexture(m_characterSize);
    states.coordinateType = CoordinateType::Pixels;

    // Only draw the outline if there is something to draw
    if (m_outlineThickness != 0)
        target.draw(m_outlineVertices, states);

    target.draw(m_vertices, states);
}

// Use the difference between bounds and width to update the spacing
void JustifyText::justifySpacing(float &whitespaceWidth, float &letterSpacing, bool isBold, float italicShear) const
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
        const Glyph& glyph = m_font->getGlyph(curChar, m_characterSize, isBold);
        const Vector2f p1 = glyph.bounds.position;
        const Vector2f p2 = glyph.bounds.position + glyph.bounds.size;
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
void JustifyText::ensureGeometryUpdate() const
{
    // Do nothing, if geometry has not changed and the font texture has not changed
    if (!m_geometryNeedUpdate && m_font->getTexture(m_characterSize).getNativeHandle() == m_fontTextureId)
        return;

    // Save the current fonts texture id
    m_fontTextureId = m_font->getTexture(m_characterSize).getNativeHandle();

    // Mark geometry as updated
    m_geometryNeedUpdate = false;

    // Clear the previous geometry
    m_vertices.clear();
    m_outlineVertices.clear();
    m_bounds = FloatRect();

    // No text: nothing to draw
    if (m_string.isEmpty())
        return;

    // Compute values related to the text style
    const bool  isBold             = m_style & Bold;
    const bool  isUnderlined       = m_style & Underlined;
    const bool  isStrikeThrough    = m_style & StrikeThrough;
    const float italicShear        = (m_style & Italic) ? degrees(12).asRadians() : 0.f;
    const float underlineOffset    = m_font->getUnderlinePosition(m_characterSize);
    const float underlineThickness = m_font->getUnderlineThickness(m_characterSize);

    // Compute the location of the strike through dynamically
    // We use the center point of the lowercase 'x' glyph as the reference
    // We reuse the underline thickness as the thickness of the strike through as well
    const float strikeThroughOffset = m_font->getGlyph(U'x', m_characterSize, isBold).bounds.getCenter().y;

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
            const Glyph& glyph = m_font->getGlyph(curChar, m_characterSize, isBold, m_outlineThickness);

            // Add the outline glyph to the vertices
            addGlyphQuad(m_outlineVertices, Vector2f(px, y), m_outlineColor, glyph, italicShear);
        }

        // Extract the current glyph's description
        const Glyph& glyph = m_font->getGlyph(curChar, m_characterSize, isBold);

        // Add the glyph to the vertices
        addGlyphQuad(m_vertices, Vector2f(px, y), m_fillColor, glyph, italicShear);

        // Update the current bounds
        const Vector2f p1 = glyph.bounds.position;
        const Vector2f p2 = glyph.bounds.position + glyph.bounds.size;

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
    m_bounds.position = Vector2f(minX, minY);
    m_bounds.size     = Vector2f(maxX, maxY) - Vector2f(minX, minY);
}

} // namespace sf
