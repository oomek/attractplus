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

#ifndef FE_PRESENTABLE_HPP
#define FE_PRESENTABLE_HPP

#include <SFML/System/Vector2.hpp>
#include <vector>

class FeSettings;
class FeShader;
class FePresentableParent;

namespace sf
{
	class Drawable;
	class Color;
};

enum FePresentableType
{
	FePresentableTypeImage = 1 << 0,
	FePresentableTypeArtwork = 1 << 1,
	FePresentableTypeSurface = 1 << 2,
	FePresentableTypeText = 1 << 3,
	FePresentableTypeListbox = 1 << 4,
	FePresentableTypeRectangle = 1 << 5
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

class FeBasePresentable
{
private:
	FePresentableParent *m_parent;
	FeShader *m_shader;
	bool m_visible;
	int m_zorder;
	sf::Vector2f m_script_pos;
	sf::Vector2f m_script_size;
	int m_grid;
	bool m_grid_uniform;
	bool m_script_geometry_set;

	sf::Vector2f convert_position( const sf::Vector2f &p ) const;
	sf::Vector2f convert_size( const sf::Vector2f &s ) const;

public:
	FeBasePresentable( FePresentableParent &p );
	virtual ~FeBasePresentable();

	virtual void on_new_selection( FeSettings * );
	virtual void on_new_list( FeSettings * );
	virtual void set_scale_factor( float, float );

	virtual const sf::Drawable &drawable() const=0;
	virtual sf::Vector2f getPosition() const=0;
	virtual void setPosition( const sf::Vector2f & )=0;
	virtual sf::Vector2f getSize() const=0;
	virtual void setSize( const sf::Vector2f & )=0;
	virtual float getRotation() const=0;
	virtual void setRotation( float )=0;
	virtual sf::Color getColor() const=0;
	virtual void setColor( sf::Color )=0;
	virtual int getIndexOffset() const;
	virtual void setIndexOffset( int io );
	virtual int getFilterOffset() const;
	virtual void setFilterOffset( int io );

	//
	// Accessor functions used in scripting implementation
	//
	float get_x() const;
	float get_y() const;
	void set_x( float x );
	void set_y( float y );

	float get_width() const;
	float get_height() const;
	void set_width( float w );
	void set_height( float h );

	void set_pos(float x, float y);
	void set_pos(float x, float y, float w, float h);

	int get_grid() const;
	void set_grid( int g );
	bool get_grid_uniform() const;
	void set_grid_uniform( bool u );
	void set_parent( FePresentableParent &p );
	void set_script_geometry( float x, float y, float w, float h );
	void refresh_script_geometry();

	int get_r() const;
	int get_g() const;
	int get_b() const;
	int get_a() const;
	void set_r(int r);
	void set_g(int g);
	void set_b(int b);
	void set_a(int a);
	void set_rgb(int r, int g, int b);
	virtual void set_rgb(int r, int g, int b, int a);

	virtual bool get_visible() const;
	void set_visible( bool );

	FeShader *get_shader() const;
	FeShader *script_get_shader() const;
	void script_set_shader( FeShader *s );

	int get_zorder();
	void set_zorder( int );
	virtual bool get_magic() const;
	virtual int get_type() const;
};

class FeImage;
class FeText;
class FeListBox;
class FeRectangle;

class FePresentableParent
{
public:
	FePresentableParent();
	virtual ~FePresentableParent();

	std::vector< FeBasePresentable * > elements;

	int m_nesting_level;
	int get_nesting_level();
	void set_nesting_level( int );
	virtual FeCoordinateSpace get_coordinate_space( bool uniform=true ) const;
	virtual sf::Vector2f get_grid_offset( bool uniform=true ) const;
	void refresh_script_geometry();

	FeImage *add_image(const char *,float, float, float, float);
	FeImage *add_image(const char *, float, float);
	FeImage *add_image(const char *);
	FeImage *add_artwork(const char *,float, float, float, float);
	FeImage *add_artwork(const char *, float, float);
	FeImage *add_artwork(const char *);
	FeImage *add_clone(FeImage *);
	FeText *add_text(const char *,int, int, int, int);
	FeListBox *add_listbox(int, int, int, int);
	FeRectangle *add_rectangle(float, float, float, float);
	FeImage *add_surface(float, float, float, float);
	FeImage *add_surface(float, float, float, float, int, int);
	FeImage *add_surface(float, float);
};

#endif
