/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2014-2015 Andrew Mickelson
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

#include "fe_shader.hpp"
#include "fe_presentable.hpp"
#include "fe_image.hpp"
#include "fe_present.hpp"
#include <iostream>

FeShader::FeShader()
	: m_type( Empty )
{
}

bool FeShader::load( sf::InputStream &vert_shader,
		sf::InputStream &frag_shader )
{
	m_type = VertexAndFragment;
	return m_shader.loadFromStream( vert_shader, frag_shader );
}

bool FeShader::load( sf::InputStream &sh,
		Type t )
{
	m_type = t;
	return m_shader.loadFromStream( sh,
		(t == Fragment) ? sf::Shader::Fragment : sf::Shader::Vertex );
}

bool FeShader::load( const std::string &vert_shader,
		const std::string &frag_shader )
{
	m_type = VertexAndFragment;
	return m_shader.loadFromFile( vert_shader, frag_shader );
}

bool FeShader::load( const std::string &sh,
		Type t )
{
	m_type = t;
	return m_shader.loadFromFile( sh,
		(t == Fragment) ? sf::Shader::Fragment : sf::Shader::Vertex );
}

void FeShader::set_param( const char *name, float x )
{
	if ( m_type != Empty )
	{
		m_shader.setUniform( name, x );
		FePresent::script_flag_redraw();
	}
}

void FeShader::set_param( const char *name, float x, float y )
{
	if ( m_type != Empty )
	{
		m_shader.setUniform( name, sf::Glsl::Vec2( x, y ) );
		FePresent::script_flag_redraw();
	}
}

void FeShader::set_param( const char *name, float x, float y, float z )
{
	if ( m_type != Empty )
	{
		m_shader.setUniform( name, sf::Glsl::Vec3( x, y, z ) );
		FePresent::script_flag_redraw();
	}
}

void FeShader::set_param( const char *name, float x, float y, float z, float w )
{
	if ( m_type != Empty )
	{
		m_shader.setUniform( name, sf::Glsl::Vec4( x, y, z, w ) );
		FePresent::script_flag_redraw();
	}
}

void FeShader::set_texture_param( const char *name )
{
	if ( m_type != Empty )
	{
		m_shader.setUniform( name, sf::Shader::CurrentTexture );
		FePresent::script_flag_redraw();
	}
}

void FeShader::set_texture_param( const char *name, FeImage *image )
{
	if (( m_type != Empty ) && ( image ))
	{
		const sf::Texture *texture = image->get_texture();

		if ( texture )
		{
			m_shader.setUniform( name, *texture );
			FePresent::script_flag_redraw();
		}
	}
}
