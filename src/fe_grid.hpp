/*
 *
 *  Attract-Mode Plus frontend
 *  Copyright (C) 2026 Andrew Mickelson & Radek Dutkiewicz
 *
 *  This file is part of Attract-Mode Plus.
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

#ifndef FE_GRID_HPP
#define FE_GRID_HPP

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <vector>

class FeVM;

namespace Sqrat
{
	class Table;
};

enum FeGrid
{
	GridPixel = 1,
	GridPercent,
	GridNormalised
};

struct FeCoordinateSpace
{
	sf::Vector2f origin;
	sf::Vector2f size;

	FeCoordinateSpace()
		: origin( 0, 0 ),
		size( 0, 0 )
	{
	}

	FeCoordinateSpace( const sf::Vector2f &o, const sf::Vector2f &s )
		: origin( o ),
		size( s )
	{
	}
};

class FeGridContext
{
public:
	explicit FeGridContext( FeVM &vm );

	void bind( Sqrat::Table &fe );
	void clear_plugins();
	void reset_plugins( std::size_t count );

	int get_mode() const;
	void set_mode( int mode );
	bool get_uniform() const;
	void set_uniform( bool uniform );
	bool get_pixel_snap() const;
	void set_pixel_snap( bool pixel_snap );
	float get_offset_x() const;
	void set_offset_x( float x );
	float get_offset_y() const;
	void set_offset_y( float y );
	float get_left() const;
	float get_right() const;
	float get_top() const;
	float get_bottom() const;
	float get_width() const;
	float get_height() const;
	float get_margin_left() const;
	float get_margin_right() const;
	float get_margin_top() const;
	float get_margin_bottom() const;

	int get_plugin_mode( int script_id ) const;
	bool get_plugin_uniform( int script_id ) const;
	bool get_plugin_pixel_snap( int script_id ) const;
	sf::Vector2f get_plugin_offset( int script_id, bool uniform ) const;
	sf::Vector2f window_to_pos( const sf::Vector2i &pos ) const;

private:
	struct Settings
	{
		Settings()
			: mode( GridPixel ),
			uniform( true ),
			pixel_snap( false ),
			offset( 0, 0 )
		{
		}

		int mode;
		bool uniform;
		bool pixel_snap;
		sf::Vector2f offset;
	};

	Settings *get_plugin_settings( int script_id );
	const Settings *get_plugin_settings( int script_id ) const;
	FeCoordinateSpace get_bounds() const;
	sf::Vector2f get_extent() const;

	FeVM &m_vm;
	std::vector<Settings> m_plugin_settings;
};

#endif
