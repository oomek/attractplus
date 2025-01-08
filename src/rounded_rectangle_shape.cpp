////////////////////////////////////////////////////////////
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
// you must not claim that you wrote the original software.
// If you use this software in a product, an acknowledgment
// in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
// and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

//
// https://github.com/SFML/SFML/wiki/Source%3A-Draw-Rounded-Rectangle
//
// ALTERATIONS FROM ORIGINAL
// - Added pragma to silence conversion warnings
// - Radius as Vector2f
// - Fallback to rectangle when corner point count is 1
// - Math in radians
//

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "rounded_rectangle_shape.hpp"
#include <cmath>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"

namespace sf
{
////////////////////////////////////////////////////////////
RoundedRectangleShape::RoundedRectangleShape(const Vector2f& size, const Vector2f& radius, unsigned int cornerPointCount)
{
    m_size = size;
    m_radius = radius;
    m_corner_point_count = cornerPointCount;
    update();
}

////////////////////////////////////////////////////////////
void RoundedRectangleShape::setSize(const Vector2f& size)
{
    m_size = size;
    update();
}

////////////////////////////////////////////////////////////
const Vector2f& RoundedRectangleShape::getSize() const
{
    return m_size;
}

////////////////////////////////////////////////////////////
void RoundedRectangleShape::setCornerRadius(const Vector2f& radius)
{
    m_radius = radius;
    update();
}

////////////////////////////////////////////////////////////
const Vector2f& RoundedRectangleShape::getCornerRadius() const
{
    return m_radius;
}

////////////////////////////////////////////////////////////
void RoundedRectangleShape::setCornerPointCount(unsigned int count)
{
    m_corner_point_count = count;
    update();
}

////////////////////////////////////////////////////////////
std::size_t RoundedRectangleShape::getPointCount() const
{
    return m_corner_point_count*4;
}

////////////////////////////////////////////////////////////
sf::Vector2f RoundedRectangleShape::getPoint(std::size_t index) const
{
    if(m_corner_point_count == 1)
    {
        switch (index)
        {
            default:
            case 0: return Vector2f(0, 0);
            case 1: return Vector2f(m_size.x, 0);
            case 2: return Vector2f(m_size.x, m_size.y);
            case 3: return Vector2f(0, m_size.y);
        }
    }

    if(index >= m_corner_point_count*4)
        return sf::Vector2f(0,0);

    sf::Vector2f center;
    unsigned int centerIndex = index/m_corner_point_count;
    static const float half_pi = 3.141592654f / 2.0f;
    float angle = (index-centerIndex) * half_pi / (m_corner_point_count-1);

    switch(centerIndex)
    {
        case 0: center.x = m_size.x - m_radius.x; center.y = m_radius.y; break;
        case 1: center.x = m_radius.x; center.y = m_radius.y; break;
        case 2: center.x = m_radius.x; center.y = m_size.y - m_radius.y; break;
        case 3: center.x = m_size.x - m_radius.x; center.y = m_size.y - m_radius.y; break;
    }

    return sf::Vector2f(m_radius.x*cos(angle)+center.x, -m_radius.y*sin(angle)+center.y);
}
} // namespace sf

#pragma GCC diagnostic pop
