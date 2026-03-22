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

namespace
{
	bool read_stream_to_string( sf::InputStream &stream, std::string &out )
	{
		out.clear();

		const std::optional<std::size_t> original_pos = stream.tell();
		const std::optional<std::size_t> size = stream.getSize();
		if ( !original_pos || !size )
			return false;

		if ( !stream.seek( 0 ) )
			return false;

		std::string buffer;
		buffer.resize( *size );
		std::size_t total_read = 0;
		while ( total_read < *size )
		{
			std::optional<std::size_t> read =
				stream.read( buffer.data() + total_read, *size - total_read );
			if ( !read )
				break;
			if ( *read == 0 )
				break;
			total_read += *read;
		}

		std::ignore = stream.seek( *original_pos );
		if ( total_read == 0 && *size > 0 )
			return false;

		buffer.resize( total_read );
		out.swap( buffer );
		return true;
	}
}

FeShader::FeShader()
	: m_type( Empty )
{
}

bool FeShader::load( sf::InputStream &vert, sf::InputStream &frag )
{
	m_type = VertexAndFragment;
	m_vertex_source_path.clear();
	m_fragment_source_path.clear();
	if ( !read_stream_to_string( vert, m_vertex_source_code ) )
		m_vertex_source_code.clear();
	if ( !read_stream_to_string( frag, m_fragment_source_code ) )
		m_fragment_source_code.clear();
	return m_shader.loadFromStream( vert, frag );
}

bool FeShader::load( sf::InputStream &sh, Type t )
{
	m_type = t;
	m_vertex_source_path.clear();
	m_fragment_source_path.clear();
	if ( t == Fragment )
	{
		m_vertex_source_code.clear();
		if ( !read_stream_to_string( sh, m_fragment_source_code ) )
			m_fragment_source_code.clear();
	}
	else
	{
		if ( !read_stream_to_string( sh, m_vertex_source_code ) )
			m_vertex_source_code.clear();
		m_fragment_source_code.clear();
	}
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
	m_vertex_source_path = vert;
	m_fragment_source_path = frag;
	m_vertex_source_code.clear();
	m_fragment_source_code.clear();
	return m_shader.loadFromFile( vert, frag );
}

bool FeShader::load( const std::string &sh, Type t )
{
	if ( !file_exists( sh ) ) {
		FeLog() << " ! Cannot find shader file: " << sh << std::endl;
		return false;
	}
	m_type = t;
	if ( t == Fragment )
	{
		m_vertex_source_path.clear();
		m_fragment_source_path = sh;
	}
	else
	{
		m_vertex_source_path = sh;
		m_fragment_source_path.clear();
	}
	m_vertex_source_code.clear();
	m_fragment_source_code.clear();
	sf::Shader::Type type = (t == Fragment) ? sf::Shader::Type::Fragment : sf::Shader::Type::Vertex;
	return m_shader.loadFromFile( sh, type );
}

bool FeShader::loadFromMemory( const std::string &vert, const std::string &frag )
{
	m_type = VertexAndFragment;
	m_vertex_source_path.clear();
	m_fragment_source_path.clear();
	m_vertex_source_code = vert;
	m_fragment_source_code = frag;
	return m_shader.loadFromMemory( vert, frag );
}

bool FeShader::loadFromMemory( const std::string &sh, Type t )
{
	m_type = t;
	m_vertex_source_path.clear();
	m_fragment_source_path.clear();
	if ( t == Fragment )
	{
		m_vertex_source_code.clear();
		m_fragment_source_code = sh;
	}
	else
	{
		m_vertex_source_code = sh;
		m_fragment_source_code.clear();
	}
	sf::Shader::Type type = (t == Fragment) ? sf::Shader::Type::Fragment : sf::Shader::Type::Vertex;
	return m_shader.loadFromMemory( sh, type );
}

void FeShader::set_param( const char *name, float x )
{
	if ( m_type != Empty )
	{
		m_params[ name ] = { x };
		m_shader.setUniform( name, x );
		FePresent::script_flag_redraw();
	}
}

void FeShader::set_param( const char *name, float x, float y )
{
	if ( m_type != Empty )
	{
		m_params[ name ] = { x, y };
		m_shader.setUniform( name, sf::Glsl::Vec2( x, y ) );
		FePresent::script_flag_redraw();
	}
}

void FeShader::set_param( const char *name, float x, float y, float z )
{
	if ( m_type != Empty )
	{
		m_params[ name ] = { x, y, z };
		m_shader.setUniform( name, sf::Glsl::Vec3( x, y, z ) );
		FePresent::script_flag_redraw();
	}
}

void FeShader::set_param( const char *name, float x, float y, float z, float w )
{
	if ( m_type != Empty )
	{
		m_params[ name ] = { x, y, z, w };
		m_shader.setUniform( name, sf::Glsl::Vec4( x, y, z, w ) );
		FePresent::script_flag_redraw();
	}
}

void FeShader::set_texture_param( const char *name )
{
	if ( m_type != Empty )
	{
		m_current_texture_params[ name ] = true;
		m_texture_image_params.erase( name );
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
			m_current_texture_params[ name ] = false;
			m_texture_image_params[ name ] = image;
			m_shader.setUniform( name, *texture );
			FePresent::script_flag_redraw();
		}
	}
}

const std::vector<float> *FeShader::get_param( const char *name ) const
{
	if ( !name )
		return nullptr;

	auto it = m_params.find( name );
	return ( it != m_params.end() ) ? &it->second : nullptr;
}

bool FeShader::uses_current_texture( const char *name ) const
{
	if ( !name )
		return false;

	auto it = m_current_texture_params.find( name );
	return ( it != m_current_texture_params.end() ) ? it->second : false;
}

FeImage *FeShader::get_texture_param_image( const char *name ) const
{
	if ( !name )
		return nullptr;

	auto it = m_texture_image_params.find( name );
	return ( it != m_texture_image_params.end() ) ? it->second : nullptr;
}
