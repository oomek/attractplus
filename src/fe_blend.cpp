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
#include "fe_util.hpp" // for FE_VERSION_INT macro
#include "fe_base.hpp"

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

sf::BlendMode FeBlend::get_blend_mode( int blend_mode )
{
	switch( blend_mode )
	{
		case FeBlend::Add:
			return sf::BlendAdd;

		case FeBlend::Subtract:
			return sf::BlendMode(sf::BlendMode::Factor::SrcAlpha, sf::BlendMode::Factor::One, sf::BlendMode::Equation::ReverseSubtract,
								 sf::BlendMode::Factor::One, sf::BlendMode::Factor::One, sf::BlendMode::Equation::ReverseSubtract);
		case FeBlend::Screen:
			return sf::BlendMode(sf::BlendMode::Factor::One, sf::BlendMode::Factor::OneMinusSrcColor);

		case FeBlend::Multiply:
			return sf::BlendMode(sf::BlendMode::Factor::DstColor, sf::BlendMode::Factor::OneMinusSrcAlpha);

		case FeBlend::Overlay:
			return sf::BlendMode(sf::BlendMode::Factor::DstColor, sf::BlendMode::Factor::SrcColor);

		case FeBlend::Premultiplied:
			return sf::BlendMode(sf::BlendMode::Factor::One, sf::BlendMode::Factor::OneMinusSrcAlpha);

		case FeBlend::None:
			return sf::BlendNone;

		case FeBlend::Alpha:
		default:
			return sf::BlendAlpha;
	}
}

bool FeBlend::uses_default_shader( int blend_mode )
{
	return ( get_default_shader_source( blend_mode ) != NULL );
}

const char *FeBlend::get_default_shader_source( int blend_mode )
{
	switch( blend_mode )
	{
		case FeBlend::Alpha:
			return DEFAULT_SHADER_GLSL_ALPHA;
		case FeBlend::Screen:
		case FeBlend::Multiply:
			return DEFAULT_SHADER_GLSL_MULTIPLIED;
		case FeBlend::Overlay:
			return DEFAULT_SHADER_GLSL_OVERLAY;
		case FeBlend::Premultiplied:
			return DEFAULT_SHADER_GLSL_PREMULTIPLIED;
		default:
			return NULL;
	}
}

void FeBlend::clear_default_shaders()
{
}
