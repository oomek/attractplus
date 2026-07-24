/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-24 Andrew Mickelson
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

#ifndef FE_ALIGN_HPP
#define FE_ALIGN_HPP

enum FeAlign : int
{
	None = 0,
	Left = 1,
	Centre = 2,
	Right = 4,
	Top = 8,
	Bottom = 16,
	TopLeft = Top | Left,
	TopCentre = Top | Centre,
	TopRight = Top | Right,
	MiddleLeft = Centre | Left,
	MiddleCentre = Centre,
	MiddleRight = Centre | Right,
	BottomLeft = Bottom | Left,
	BottomCentre = Bottom | Centre,
	BottomRight = Bottom | Right
};

constexpr FeAlign operator|( FeAlign a, FeAlign b )
{
	return static_cast<FeAlign>( static_cast<int>( a ) | static_cast<int>( b ) );
}

constexpr FeAlign operator&( FeAlign a, FeAlign b )
{
	return static_cast<FeAlign>( static_cast<int>( a ) & static_cast<int>( b ) );
}

#endif
