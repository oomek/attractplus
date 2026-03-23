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

#ifndef FE_SHADER_HPP
#define FE_SHADER_HPP

#include <string>
#include <unordered_map>
#include <vector>
class FeImage;
namespace sf
{
	class InputStream;
}

bool fe_shaders_available();

class FeShader
{
public:
	enum Type
	{
		Empty,
		VertexAndFragment,
		Vertex,
		Fragment
	};

	FeShader();
	bool load( sf::InputStream &vert, sf::InputStream &frag );
	bool load( sf::InputStream &sh, Type t );
	bool load( const std::string &vert, const std::string &frag );
	bool load( const std::string &sh, Type t );
	bool loadFromMemory( const std::string &vert, const std::string &frag );
	bool loadFromMemory( const std::string &sh, Type t );

	void set_param( const char *name, float x );
	void set_param( const char *name, float x, float y );
	void set_param( const char *name, float x, float y, float z );
	void set_param( const char *name, float x, float y, float z, float w );
	void set_texture_param( const char *name );
	void set_texture_param( const char *name, FeImage *image );
	Type get_type() const { return m_type; };
	const std::string &get_vertex_source_path() const { return m_vertex_source_path; };
	const std::string &get_fragment_source_path() const { return m_fragment_source_path; };
	const std::string &get_vertex_source_code() const { return m_vertex_source_code; };
	const std::string &get_fragment_source_code() const { return m_fragment_source_code; };
	const std::vector<float> *get_param( const char *name ) const;
	bool uses_current_texture( const char *name ) const;
	FeImage *get_texture_param_image( const char *name ) const;

private:
	FeShader( const FeShader & );
	const FeShader &operator=( const FeShader & );

	Type m_type;
	std::string m_vertex_source_path;
	std::string m_fragment_source_path;
	std::string m_vertex_source_code;
	std::string m_fragment_source_code;
	std::unordered_map<std::string, std::vector<float>> m_params;
	std::unordered_map<std::string, bool> m_current_texture_params;
	std::unordered_map<std::string, FeImage *> m_texture_image_params;
};

#endif
