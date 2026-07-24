/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-15 Andrew Mickelson
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

#include "fe_blend.hpp"

namespace
{
	const char *DEFAULT_SHADER_GLSL_ALPHA = \
		"uniform sampler2D texture;" \
		"void main(){" \
		"vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);" \
		"gl_FragColor = gl_Color * pixel;}";

	const char *DEFAULT_SHADER_GLSL_MULTIPLIED = \
		"uniform sampler2D texture;" \
		"void main(){" \
		"vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);" \
		"gl_FragColor = gl_Color * pixel;" \
		"gl_FragColor.xyz *= gl_FragColor.w;}";

	const char *DEFAULT_SHADER_GLSL_OVERLAY = \
		"uniform sampler2D texture;" \
		"void main(){" \
		"vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);" \
		"gl_FragColor = gl_Color * pixel;" \
		"gl_FragColor = mix(vec4(0.5,0.5,0.5,1.0), gl_FragColor, gl_FragColor.w);}";

	const char *DEFAULT_SHADER_GLSL_PREMULTIPLIED = \
		"uniform sampler2D texture;" \
		"void main(){" \
		"vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);" \
		"gl_FragColor = gl_Color * pixel;" \
		"gl_FragColor.xyz *= gl_Color.w;}";
};

FeBlend::State FeBlend::get_state( int blend_mode )
{
	State state;

	switch( blend_mode )
	{
		case FeBlend::Add:
			state.dst_color_factor = One;
			state.dst_alpha_factor = One;
			break;

		case FeBlend::Subtract:
			state.dst_color_factor = One;
			state.color_equation = ReverseSubtractEquation;
			state.dst_alpha_factor = One;
			state.alpha_equation = ReverseSubtractEquation;
			break;

		case FeBlend::Screen:
			state.src_color_factor = One;
			state.dst_color_factor = OneMinusSrcColor;
			state.dst_alpha_factor = OneMinusSrcColor;
			break;

		case FeBlend::Multiply:
			state.src_color_factor = DstColor;
			state.dst_color_factor = OneMinusSrcAlpha;
			state.src_alpha_factor = DstColor;
			break;

		case FeBlend::Overlay:
			state.src_color_factor = DstColor;
			state.dst_color_factor = SrcColor;
			state.src_alpha_factor = DstColor;
			state.dst_alpha_factor = SrcColor;
			break;

		case FeBlend::Premultiplied:
			state.src_color_factor = One;
			break;

		case FeBlend::IgnoreAlpha:
			state.src_color_factor = DstAlpha;
			state.dst_color_factor = Zero;
			state.src_alpha_factor = Zero;
			state.dst_alpha_factor = One;
			break;

		case FeBlend::InvertAlpha:
			state.src_color_factor = OneMinusDstAlpha;
			state.dst_color_factor = OneMinusSrcAlpha;
			state.src_alpha_factor = OneMinusDstAlpha;
			state.dst_alpha_factor = OneMinusSrcAlpha;
			break;

		case FeBlend::InvertRGB:
			state.src_color_factor = OneMinusDstColor;
			state.dst_color_factor = OneMinusSrcAlpha;
			state.src_alpha_factor = One;
			state.dst_alpha_factor = OneMinusSrcAlpha;
			break;

		case FeBlend::None:
			state.enable_blend = false;
			state.src_color_factor = One;
			state.dst_color_factor = Zero;
			state.src_alpha_factor = One;
			state.dst_alpha_factor = Zero;
			break;

		case FeBlend::Alpha:
		default:
			break;
	}

	return state;
}

bool FeBlend::uses_default_shader( int blend_mode )
{
	return ( get_default_shader_source( blend_mode ) != nullptr );
}

const char *FeBlend::get_default_shader_source( int blend_mode )
{
	switch( blend_mode )
	{
		case FeBlend::Alpha:
			return DEFAULT_SHADER_GLSL_ALPHA;
		case FeBlend::Screen:
		case FeBlend::Multiply:
		case FeBlend::IgnoreAlpha:
		case FeBlend::InvertAlpha:
		case FeBlend::InvertRGB:
			return DEFAULT_SHADER_GLSL_MULTIPLIED;
		case FeBlend::Overlay:
			return DEFAULT_SHADER_GLSL_OVERLAY;
		case FeBlend::Premultiplied:
			return DEFAULT_SHADER_GLSL_PREMULTIPLIED;
		default:
			return nullptr;
	}
}

void FeBlend::clear_default_shaders()
{
}
