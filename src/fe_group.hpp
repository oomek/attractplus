#ifndef FE_GROUP_HPP
#define FE_GROUP_HPP

#include "fe_presentable.hpp"
#include "fe_align.hpp"
#include <sqrat.h>

class FeGroup : public FeBasePresentable, public FePresentableParent
{
public:
	FeGroup( FePresentableParent &parent, float x, float y, float w, float h );
	~FeGroup() override;
	Vec2f getPosition() const override { return m_pos; }
	void setPosition( const Vec2f &pos ) override;
	Vec2f getSize() const override { return m_size; }
	void setSize( const Vec2f &size ) override;
	float getRotation() const override { return m_rotation; }
	void setRotation( float rotation ) override;
	Color getColor() const override { return m_color; }
	void setColor( Color color ) override;
	int get_type() const override { return FePresentableTypeGroup; }
	Vec2f get_grid_size() const override;
	Vec2f get_style_reference_size() const override;
	FeGridView get_grid_view() { return FePresentableParent::get_grid_view(); }
	Vec2f snap_position_to_pixel( const Vec2f &pos ) const override;
	Vec2f snap_size_to_pixel( const Vec2f &size ) const override;
	void refresh_script_geometry() override;
	void set_scale_factor( float x, float y ) override;
	void on_group_update( bool update=true ) override;
	FeRenderMatrix4 get_items_transform() const;
	void add_item( FeBasePresentable *item );
	void remove_item( FeBasePresentable *item );
	void detach_item( FeBasePresentable *item );
	Sqrat::Array get_items();
	float get_anchor_x() const override { return m_anchor.x; }
	float get_anchor_y() const override { return m_anchor.y; }
	float get_anchor_z() const { return m_anchor.z; }
	float get_rotation_origin_x() const { return m_rotation_origin.x; }
	float get_rotation_origin_y() const { return m_rotation_origin.y; }
	float get_rotation_origin_z() const { return m_rotation_origin.z; }
	float get_transform_origin_x() const { return m_transform_origin.x; }
	float get_transform_origin_y() const { return m_transform_origin.y; }
	int get_anchor_type() const { return m_anchor_type; }
	int get_rotation_origin_type() const { return m_rotation_origin_type; }
	int get_transform_origin_type() const { return m_transform_origin_type; }
	void set_anchor( float x, float y );
	void set_anchor( float x, float y, float z );
	void set_rotation_origin( float x, float y );
	void set_rotation_origin( float x, float y, float z );
	void set_transform_origin( float x, float y );
	void set_anchor_x( float x );
	void set_anchor_y( float y );
	void set_anchor_z( float z );
	void set_rotation_origin_x( float x );
	void set_rotation_origin_y( float y );
	void set_rotation_origin_z( float z );
	void set_transform_origin_x( float x );
	void set_transform_origin_y( float y );
	void set_anchor_type( int value );
	void set_rotation_origin_type( int value );
	void set_transform_origin_type( int value );
	static Sqrat::Object item_object( FeBasePresentable *item );
	static void script_add_item( FeGroup *group, Sqrat::Object value );

private:
	friend class FeBasePresentable;
	FeRenderMatrix4 get_items_transform( const FePresentableParent &target ) const;
	Vec2f m_pos;
	Vec2f m_size;
	Vec3f m_anchor = Vec3f( 0, 0, 0 );
	Vec3f m_rotation_origin = Vec3f( 0, 0, 0 );
	Vec2f m_transform_origin = Vec2f( 0, 0 );
	int m_anchor_type = FeAlign::TopLeft;
	int m_rotation_origin_type = FeAlign::TopLeft;
	int m_transform_origin_type = FeAlign::TopLeft;
	float m_rotation = 0;
	Color m_color = Color::White;
	void on_transform_update() override;
};

#endif
