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

namespace
{
	const char *DEFAULT_SHADER_GLSL_ALPHA = \
		"uniform sampler2D texture;" \
		"void main(){" \
		"vec4 pixel = texture2DProj(texture, gl_TexCoord[0]);" \
		"gl_FragColor = gl_Color * pixel;}";

	const char *DEFAULT_SHADER_GLSL_MULTIPLIED = \
		"uniform sampler2D texture;" \
		"void main(){" \
		"vec4 pixel = texture2DProj(texture, gl_TexCoord[0]);" \
		"gl_FragColor = gl_Color * pixel;" \
		"gl_FragColor.xyz *= gl_FragColor.w;}";

	const char *DEFAULT_SHADER_GLSL_OVERLAY = \
		"uniform sampler2D texture;" \
		"void main(){" \
		"vec4 pixel = texture2DProj(texture, gl_TexCoord[0]);" \
		"gl_FragColor = gl_Color * pixel;" \
		"gl_FragColor = mix(vec4(0.5,0.5,0.5,1.0), gl_FragColor, gl_FragColor.w);}";

	const char *DEFAULT_SHADER_GLSL_PREMULTIPLIED = \
		"uniform sampler2D texture;" \
		"void main(){" \
		"vec4 pixel = texture2DProj(texture, gl_TexCoord[0]);" \
		"gl_FragColor = gl_Color * pixel;" \
		"gl_FragColor.xyz *= gl_Color.w;}";

	sf::Shader *default_shader_alpha=NULL;
	sf::Shader *default_shader_multiplied=NULL;
	sf::Shader *default_shader_overlay=NULL;
	sf::Shader *default_shader_premultiplied=NULL;


	const char *DEFAULT_SHADER_GLSL_ALPHA_RECTANGLE = \
		"void main(){" \
		"gl_FragColor = gl_Color;}";

	const char *DEFAULT_SHADER_GLSL_MULTIPLIED_RECTANGLE = \
		"void main(){" \
		"gl_FragColor = gl_Color;" \
		"gl_FragColor.xyz *= gl_FragColor.w;}";

	const char *DEFAULT_SHADER_GLSL_OVERLAY_RECTANGLE = \
		"void main(){" \
		"gl_FragColor = gl_Color;" \
		"gl_FragColor = mix(vec4(0.5,0.5,0.5,1.0), gl_FragColor, gl_FragColor.w);}";

	const char *DEFAULT_SHADER_GLSL_PREMULTIPLIED_RECTANGLE = \
		"void main(){" \
		"gl_FragColor = gl_Color;" \
		"gl_FragColor.xyz *= gl_Color.w;}";

	sf::Shader *default_shader_alpha_rectangle=NULL;
	sf::Shader *default_shader_multiplied_rectangle=NULL;
	sf::Shader *default_shader_overlay_rectangle=NULL;
	sf::Shader *default_shader_premultiplied_rectangle=NULL;
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

sf::Shader* FeBlend::get_default_shader( int blend_mode )
{
	switch( blend_mode )
	{
		case FeBlend::Alpha:
		case FeBlend::Add:
		case FeBlend::Subtract:
		case FeBlend::None:
			if ( !default_shader_alpha )
			{
				default_shader_alpha = new sf::Shader();
				default_shader_alpha->loadFromMemory( DEFAULT_SHADER_GLSL_ALPHA, sf::Shader::Type::Fragment );
			}
			return default_shader_alpha;
		case FeBlend::Screen:
		case FeBlend::Multiply:
			if ( !default_shader_multiplied )
			{
				default_shader_multiplied = new sf::Shader();
				std::ignore = default_shader_multiplied->loadFromMemory( DEFAULT_SHADER_GLSL_MULTIPLIED, sf::Shader::Type::Fragment );
			}
			return default_shader_multiplied;
		case FeBlend::Overlay:
			if ( !default_shader_overlay )
			{
				default_shader_overlay = new sf::Shader();
				std::ignore = default_shader_overlay->loadFromMemory( DEFAULT_SHADER_GLSL_OVERLAY, sf::Shader::Type::Fragment );
			}
			return default_shader_overlay;
		case FeBlend::Premultiplied:
			if ( !default_shader_premultiplied )
			{
				default_shader_premultiplied = new sf::Shader();
				std::ignore = default_shader_premultiplied->loadFromMemory( DEFAULT_SHADER_GLSL_PREMULTIPLIED, sf::Shader::Type::Fragment );
			}
			return default_shader_premultiplied;
		default:
			return NULL;
	}
}

sf::Shader* FeBlend::get_default_shader_rectangle( int blend_mode )
{
	switch( blend_mode )
	{
		case FeBlend::Alpha:
		case FeBlend::Add:
		case FeBlend::Subtract:
		case FeBlend::None:
			if ( !default_shader_alpha_rectangle )
			{
				default_shader_alpha_rectangle = new sf::Shader();
				default_shader_alpha_rectangle->loadFromMemory( DEFAULT_SHADER_GLSL_ALPHA_RECTANGLE, sf::Shader::Type::Fragment );
			}
			return default_shader_alpha_rectangle;
		case FeBlend::Screen:
		case FeBlend::Multiply:
			if ( !default_shader_multiplied_rectangle )
			{
				default_shader_multiplied_rectangle = new sf::Shader();
				default_shader_multiplied_rectangle->loadFromMemory( DEFAULT_SHADER_GLSL_MULTIPLIED_RECTANGLE, sf::Shader::Type::Fragment );
			}
			return default_shader_multiplied_rectangle;
		case FeBlend::Overlay:
			if ( !default_shader_overlay_rectangle )
			{
				default_shader_overlay_rectangle = new sf::Shader();
				default_shader_overlay_rectangle->loadFromMemory( DEFAULT_SHADER_GLSL_OVERLAY_RECTANGLE, sf::Shader::Type::Fragment );
			}
			return default_shader_overlay_rectangle;
		case FeBlend::Premultiplied:
			if ( !default_shader_premultiplied_rectangle )
			{
				default_shader_premultiplied_rectangle = new sf::Shader();
				default_shader_premultiplied_rectangle->loadFromMemory( DEFAULT_SHADER_GLSL_PREMULTIPLIED_RECTANGLE, sf::Shader::Type::Fragment );
			}
			return default_shader_premultiplied_rectangle;
		default:
			return NULL;
	}
}

void FeBlend::clear_default_shaders()
{
	if ( default_shader_alpha )
	{
		delete default_shader_alpha;
		default_shader_alpha = NULL;
	}
	if ( default_shader_multiplied )
	{
		delete default_shader_multiplied;
		default_shader_multiplied = NULL;
	}

	if ( default_shader_overlay )
	{
		delete default_shader_overlay;
		default_shader_overlay = NULL;
	}

	if ( default_shader_premultiplied )
	{
		delete default_shader_premultiplied;
		default_shader_premultiplied = NULL;
	}


	if ( default_shader_alpha_rectangle )
	{
		delete default_shader_alpha_rectangle;
		default_shader_alpha_rectangle = NULL;
	}
	if ( default_shader_multiplied_rectangle )
	{
		delete default_shader_multiplied_rectangle;
		default_shader_multiplied_rectangle = NULL;
	}

	if ( default_shader_overlay_rectangle )
	{
		delete default_shader_overlay_rectangle;
		default_shader_overlay_rectangle = NULL;
	}

	if ( default_shader_premultiplied_rectangle )
	{
		delete default_shader_premultiplied_rectangle;
		default_shader_premultiplied_rectangle = NULL;
	}
}
