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

#ifndef FE_JOYSTICK_HPP
#define FE_JOYSTICK_HPP

#include <cstddef>
#include <cstdint>

namespace FeJoystick
{
	constexpr std::size_t Count = 8;
	constexpr std::size_t ButtonCount = 32;

	enum class Axis : std::uint8_t
	{
		X = 0,
		Y,
		Z,
		R,
		U,
		V,
		PovX,
		PovY
	};
}

#endif
