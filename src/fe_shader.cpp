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

bool FeShader::load( sf::InputStream &vert, sf::InputStream &frag )
{
	m_type = VertexAndFragment;
	return m_shader.loadFromStream( vert, frag );
}

bool FeShader::load( sf::InputStream &sh, Type t )
{
	m_type = t;
	sf::Shader::Type type = (t == Fragment) ? sf::Shader::Type::Fragment : sf::Shader::Type::Vertex;
	return m_shader.loadFromStream( sh, type );
}

bool FeShader::load( const std::string &vert, const std::string &frag )
{
	if ( !file_exists( vert ) ) {
		FeLog() << " ! Cannot find shader file: " << vert << std::endl;
		return false;
	}
	if ( !file_exists( frag ) ) {
		FeLog() << " ! Cannot find shader file: " << frag << std::endl;
		return false;
	}
	m_type = VertexAndFragment;
	return m_shader.loadFromFile( vert, frag );
}

bool FeShader::load( const std::string &sh, Type t )
{
	if ( !file_exists( sh ) ) {
		FeLog() << " ! Cannot find shader file: " << sh << std::endl;
		return false;
	}
	m_type = t;
	sf::Shader::Type type = (t == Fragment) ? sf::Shader::Type::Fragment : sf::Shader::Type::Vertex;
	return m_shader.loadFromFile( sh, type );
}

bool FeShader::loadFromMemory( const std::string &vert, const std::string &frag )
{
	m_type = VertexAndFragment;
	return m_shader.loadFromMemory( vert, frag );
}

bool FeShader::loadFromMemory( const std::string &sh, Type t )
{
	m_type = t;
	sf::Shader::Type type = (t == Fragment) ? sf::Shader::Type::Fragment : sf::Shader::Type::Vertex;
	return m_shader.loadFromMemory( sh, type );
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
