/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013 Andrew Mickelson
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

#ifndef FE_BLEND_HPP
#define FE_BLEND_HPP

class FeBlend
{
public:
	enum Mode
	{
		Alpha,
		Add,
		Subtract,
		Screen,
		Multiply,
		Overlay,
		Premultiplied,
		None
	};

	enum Factor
	{
		Zero,
		One,
		SrcAlpha,
		OneMinusSrcAlpha,
		SrcColor,
		OneMinusSrcColor,
		DstColor
	};

	enum Equation
	{
		AddEquation,
		ReverseSubtractEquation
	};

	struct State
	{
		bool enable_blend = true;
		Factor src_color_factor = SrcAlpha;
		Factor dst_color_factor = OneMinusSrcAlpha;
		Equation color_equation = AddEquation;
		Factor src_alpha_factor = One;
		Factor dst_alpha_factor = OneMinusSrcAlpha;
		Equation alpha_equation = AddEquation;
	};

	static State get_state( int blend_mode );
	static bool uses_default_shader( int blend_mode );
	static const char *get_default_shader_source( int blend_mode );

	static void clear_default_shaders();
};

#endif
