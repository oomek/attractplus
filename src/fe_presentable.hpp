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

#include "fe_types.hpp"
#include "fe_grid.hpp"
#include <string>
#include <vector>

class FeSettings;
class FeShader;
class FePresentableParent;

namespace Sqrat
{
	class Object;
}

class FeGridView
{
private:
	FePresentableParent *m_parent;
	FeGrid *m_settings;
	void refresh();
	void set_flag( bool &flag, Sqrat::Object value );

public:
	FeGridView( FePresentableParent *parent, FeGrid *settings );
	void set_range( const Vec2f &offset, const Vec2f &size );
	float get_left() const;
	float get_top() const;
	float get_right() const;
	float get_bottom() const;
	float get_width() const;
	float get_height() const;
	static bool get_uniform( FeGridView *grid );
	static void set_uniform( FeGridView *grid, Sqrat::Object value );
	static bool get_centered( FeGridView *grid );
	static void set_centered( FeGridView *grid, Sqrat::Object value );
	static bool get_pixel_snap( FeGridView *grid );
	static void set_pixel_snap( FeGridView *grid, Sqrat::Object value );
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

class FeBasePresentable
{
public:
	enum RotationOrder
	{
		XYZ = 0,
		XZY,
		YXZ,
		YZX,
		ZXY,
		ZYX
	};

protected:
	FePresentableParent *m_parent;
	bool m_snap_x;
	bool m_snap_y;
	bool m_snap_width;
	bool m_snap_height;
	Vec2f m_snap_offset;
	Vec2f snap_draw_position( const Vec2f &pos ) const;
	void snap_draw_geometry( Vec2f &pos, Vec2f &size, bool endpoint=false ) const;

private:
	FeShader *m_shader;
	bool m_visible;
	bool m_zbuffer;
	float m_z;
	float m_rotation_x;
	float m_rotation_y;
	RotationOrder m_rotation_order;
	int m_zorder;
	Vec2f m_script_pos;
	Vec2f m_script_size;
	int m_script_id;
	bool m_script_geometry_set;
	void set_snap( bool &property, bool value );

protected:
	FeGridTransform grid_transform() const;
	float pixels_to_grid_height( float value ) const;
	float pixels_to_grid_width( float value ) const;
	Vec2f snap_position( const Vec2f &p, bool snap=true ) const;
	Vec2f snap_size( const Vec2f &s, bool snap=true ) const;

public:
	FeBasePresentable( FePresentableParent &p );
	virtual ~FeBasePresentable();

	virtual void on_new_selection( FeSettings * );
	virtual void on_new_list( FeSettings * );
	virtual void set_scale_factor( float, float );

protected:
	virtual void on_transform_update();

public:
	virtual Vec2f getPosition() const=0;
	virtual void setPosition( const Vec2f & )=0;
	virtual Vec2f getSize() const=0;
	virtual void setSize( const Vec2f & )=0;
	virtual float getRotation() const=0;
	virtual void setRotation( float )=0;
	virtual Color getColor() const=0;
	virtual void setColor( Color )=0;
	virtual float get_anchor_x() const;
	virtual float get_anchor_y() const;
	virtual float get_origin_x() const;
	virtual float get_origin_y() const;
	virtual int getIndexOffset() const;
	virtual void setIndexOffset( int io );
	virtual int getFilterOffset() const;
	virtual void setFilterOffset( int io );

	//
	// Accessor functions used in scripting implementation
	//
	float get_x() const;
	float get_y() const;
	float get_z() const;
	void set_x( float x );
	void set_y( float y );
	void set_z( float z );

	float get_width() const;
	float get_height() const;
	void set_width( float w );
	void set_height( float h );

	void set_pos(float x, float y);
	void set_pos(float x, float y, float w, float h);
	bool set_animated_property( const std::string &name, float value, bool snap=false );
	float snap_destination_to_pixels( const std::string &name, float destination ) const;

	bool get_pixel_snap() const;
	void set_parent( FePresentableParent &p );
	void set_script_geometry( float x, float y, float w, float h );
	virtual void refresh_script_geometry();

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
	bool get_zbuffer() const;
	void set_zbuffer( bool );

	FeShader *get_shader() const;
	FeShader *script_get_shader() const;
	void script_set_shader( FeShader *s );

	float get_rotation_x() const;
	void set_rotation_x( float rotation );
	float get_rotation_y() const;
	void set_rotation_y( float rotation );
	float get_rotation_z() const;
	void set_rotation_z( float rotation );
	int get_rotation_order() const;
	void set_rotation_order( int order );

	int get_zorder();
	void set_zorder( int );
	virtual bool get_magic() const;
	virtual int get_type() const;
};

class FeImage;
class FeModel3D;
class FeText;
class FeListBox;
class FeRectangle;

class FePresentableParent
{
public:
	FePresentableParent();
	virtual ~FePresentableParent();

	std::vector< FeBasePresentable * > elements;
	FeGrid grid;
	FeGridView get_grid_view();
	virtual Vec2f get_grid_extent() const;
	virtual const FeGrid &get_grid( int script_id ) const;

	int m_nesting_level;
	int get_nesting_level();
	void set_nesting_level( int );
	virtual Vec2f snap_position_to_pixel( const Vec2f &p ) const;
	virtual Vec2f snap_size_to_pixel( const Vec2f &s ) const;
	void refresh_script_geometry();

	FeImage *add_image(const char *,float, float, float, float);
	FeImage *add_image(const char *, float, float);
	FeImage *add_image(const char *);
	FeImage *add_artwork(const char *,float, float, float, float);
	FeImage *add_artwork(const char *, float, float);
	FeImage *add_artwork(const char *);
	FeModel3D *add_model_3d(const char *);
	FeImage *add_clone(FeImage *);
	FeModel3D *add_clone(FeModel3D *);
	FeText *add_text(const char *,float, float, float, float);
	FeListBox *add_listbox(float, float, float, float);
	FeRectangle *add_rectangle(float, float, float, float);
	FeImage *add_surface(float, float, float, float);
	FeImage *add_surface(float, float, float, float, int, int);
	FeImage *add_surface(float, float);
};

#endif
