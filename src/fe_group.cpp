#include "fe_group.hpp"
#include "fe_present.hpp"
#include "fe_animation.hpp"
#include "fe_image.hpp"
#include "fe_text.hpp"
#include "fe_listbox.hpp"
#include "fe_rectangle.hpp"
#include "fe_model_3d.hpp"
#include "fe_util.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

FeGroup::FeGroup( FePresentableParent &parent, float x, float y, float w, float h )
	: FeBasePresentable( parent ), m_pos( x, y ), m_size( w, h )
{
	m_nesting_level = parent.get_nesting_level();
	set_script_geometry( x, y, w, h );
}

FeGroup::~FeGroup()
{
	while ( !elements.empty() )
	{
		auto *item = elements.back();
		elements.pop_back();
		delete item;
	}
}

void FeGroup::setPosition( const Vec2f &pos )
{
	if ( m_pos == pos ) return;
	m_pos = pos;
	on_transform_update();
}

void FeGroup::setSize( const Vec2f &size )
{
	if ( m_size == size ) return;
	m_size = size;
	on_transform_update();
}

void FeGroup::setRotation( float rotation )
{
	if ( m_rotation == rotation ) return;
	m_rotation = rotation;
	on_transform_update();
}

void FeGroup::setColor( Color color )
{
	if ( m_color == color ) return;
	m_color = color;
	FePresent::script_flag_redraw();
}

void FeGroup::on_transform_update()
{
	FePresentableParent::refresh_script_geometry();
	FePresent::script_flag_redraw();
}

void FeGroup::refresh_script_geometry()
{
	if ( refresh_script_snap() )
	{
		const Vec2f pos = snap_position( m_script_pos ), size = snap_size( m_script_size );
		if ( pos != m_pos || size != m_size ) FePresent::script_flag_redraw();
		m_pos = pos;
		m_size = size;
	}
	FePresentableParent::refresh_script_geometry();
}

void FeGroup::set_scale_factor( float x, float y )
{
	for ( auto *item : elements ) item->set_scale_factor( x, y );
}

void FeGroup::on_group_update( bool update )
{
	for ( auto *item : elements ) item->on_group_update( update );
	FePresent::script_flag_redraw();
}

static Vec2f group_grid_size( const Vec2f &size )
{
	// A collapsed axis is handled by the draw transform; keep layout finite.
	return Vec2f( size.x == 0 ? 1.f : std::abs( size.x ),
		size.y == 0 ? 1.f : std::abs( size.y ) );
}

Vec2f FeGroup::get_grid_size() const
{
	Vec2f pos = m_pos, size = m_size;
	snap_draw_geometry( pos, size );
	return group_grid_size( size );
}

Vec2f FeGroup::get_style_reference_size() const
{
	if ( m_detached ) return get_grid_size();
	const auto parent = m_parent->get_grid( m_script_id ).transform( m_parent->get_style_reference_size() );
	return group_grid_size( parent.size( Vec2f( get_width(), get_height() ) ) );
}

FeRenderMatrix4 FeGroup::get_items_transform() const
{
	return get_items_transform( *get_draw_target() );
}

FeRenderMatrix4 FeGroup::get_items_transform( const FePresentableParent &target ) const
{
	const auto inherited = get_draw_transform( &target );
	Vec2f pos = m_pos, size = m_size;
	if ( get_pixel_snap() )
	{
		const bool mapped = m_detached || m_parent != &target;
		snap_draw_geometry( pos, size, false, PixelSnap{ target, mapped ? &inherited : nullptr, m_detached ? get_z() : 0.f } );
	}
	const Vec2f grid_size = group_grid_size( size );
	auto matrix = FeRenderMatrix4::identity();
	matrix.translate( Vec3f( pos.x + ( m_rotation_origin.x - m_anchor.x ) * size.x,
		pos.y + ( m_rotation_origin.y - m_anchor.y ) * size.y,
		get_z() - m_anchor.z + m_rotation_origin.z ) );
	matrix.rotate( get_rotation_x(), get_rotation_y(), m_rotation, get_rotation_order() );
	// Size is resolved by the child grid before geometry is built.
	matrix.scale( Vec3f( size.x < 0 ? -1.f : size.x > 0 ? 1.f : 0.f,
		size.y < 0 ? -1.f : size.y > 0 ? 1.f : 0.f, 1.f ) );
	matrix.translate( Vec3f( -m_rotation_origin.x * grid_size.x,
		-m_rotation_origin.y * grid_size.y, -m_rotation_origin.z ) );
	return inherited * matrix;
}

Vec2f FeGroup::snap_position_to_pixel( const Vec2f &pos ) const
{
	const auto &target = *get_draw_target();
	const auto matrix = get_items_transform( target );
	return PixelSnap{ target, &matrix }.position( pos );
}

Vec2f FeGroup::snap_size_to_pixel( const Vec2f &size ) const
{
	const auto &target = *get_draw_target();
	const auto matrix = get_items_transform( target );
	return PixelSnap{ target, &matrix }.size( size );
}

void FeGroup::add_item( FeBasePresentable *item )
{
	if ( item ) item->set_group( this );
}

void FeGroup::remove_item( FeBasePresentable *item )
{
	if ( item && item->get_group() == this ) item->set_group( nullptr );
}

void FeGroup::detach_item( FeBasePresentable *item )
{
	if ( item && item->get_group() == this ) item->detach();
}

Sqrat::Object FeGroup::item_object( FeBasePresentable *item )
{
	auto vm = Sqrat::DefaultVM::Get();
	if ( auto *p = dynamic_cast<FeGroup *>( item ) ) return Sqrat::Object( p, vm );
	if ( auto *p = dynamic_cast<FeImage *>( item ) ) return Sqrat::Object( p, vm );
	if ( auto *p = dynamic_cast<FeText *>( item ) ) return Sqrat::Object( p, vm );
	if ( auto *p = dynamic_cast<FeListBox *>( item ) ) return Sqrat::Object( p, vm );
	if ( auto *p = dynamic_cast<FeRectangle *>( item ) ) return Sqrat::Object( p, vm );
	if ( auto *p = dynamic_cast<FeModel3D *>( item ) ) return Sqrat::Object( p, vm );
	return Sqrat::Object();
}

Sqrat::Array FeGroup::get_items()
{
	FePresent::script_get_fep()->sort_zorder();
	Sqrat::Array result( Sqrat::DefaultVM::Get() );
	for ( auto *item : elements ) result.Append( item->get_script_object() );
	return result;
}

void FeBasePresentable::remember_script_object( const Sqrat::Object &object )
{
	if ( !m_script_object ) m_script_object = std::make_shared<Sqrat::Object>( object );
}

Sqrat::Object FeBasePresentable::get_script_object()
{
	if ( !m_script_object )
	{
		auto object = FeGroup::item_object( this );
		sq_pop( object.GetVM(), 1 );
		remember_script_object( object );
	}
	return *m_script_object;
}

Sqrat::Object FeBasePresentable::script_get_group( FeBasePresentable *item )
{
	auto *group = item->get_group();
	return group ? group->get_script_object() : Sqrat::Object();
}

void FeBasePresentable::script_set_group( FeBasePresentable *item, Sqrat::Object value )
{
	auto *group = value.GetType() == OT_NULL ? nullptr : value.Cast<FeGroup *>();
	if ( Sqrat::Error::Instance().Occurred( value.GetVM() ) ) return;
	HSQOBJECT receiver;
	sq_getstackobj( value.GetVM(), 1, &receiver );
	item->remember_script_object( Sqrat::Object( receiver, value.GetVM() ) );
	if ( group ) group->remember_script_object( value );
	item->set_group( group );
}

void FeGroup::script_add_item( FeGroup *group, Sqrat::Object value )
{
	auto *item = value.GetType() == OT_NULL ? nullptr : value.Cast<FeBasePresentable *>();
	if ( Sqrat::Error::Instance().Occurred( value.GetVM() ) || !item ) return;
	item->remember_script_object( value );
	group->add_item( item );
}

FeGroup *FeBasePresentable::get_group() const
{
	return dynamic_cast<FeGroup *>( m_parent );
}

FePresentableParent *FeBasePresentable::get_draw_target() const
{
	auto *parent = m_parent;
	while ( auto *group = dynamic_cast<FeGroup *>( parent ) ) parent = group->m_parent;
	return parent;
}

FeRenderMatrix4 FeBasePresentable::get_draw_transform( const FePresentableParent *target ) const
{
	auto matrix = FeRenderMatrix4::identity();
	if ( m_detached )
	{
		const Vec2f pos = getPosition();
		matrix.translate( Vec3f( pos.x - m_detached_target.x, pos.y - m_detached_target.y,
			get_z() - m_detached_target.z ) );
		matrix = matrix * m_detached_transform;
		matrix.translate( Vec3f( m_detached_source.x - pos.x, m_detached_source.y - pos.y,
			m_detached_source.z - get_z() ) );
	}
	if ( auto *group = get_group() )
		matrix = group->get_items_transform( target ? *target : *get_draw_target() ) * matrix;
	return matrix;
}

Color FeBasePresentable::get_inherited_color() const
{
	Color color = m_detached_color;
	if ( auto *group = get_group() ) color = group->get_inherited_color() * group->getColor() * color;
	return color;
}

int FeBasePresentable::get_effective_index_offset() const
{
	auto *group = get_group();
	return getIndexOffset() + ( group ? group->get_effective_index_offset() : 0 );
}

int FeBasePresentable::get_effective_filter_offset() const
{
	auto *group = get_group();
	return getFilterOffset() + ( group ? group->get_effective_filter_offset() : 0 );
}

void FeBasePresentable::on_group_update( bool update )
{
	if ( update ) FePresent::script_do_update( this );
}

void FeBasePresentable::set_group( FeGroup *group ) { change_group( group, false ); }
void FeBasePresentable::detach() { change_group( nullptr, true ); }

void FeBasePresentable::change_group( FeGroup *group, bool preserve )
{
	if ( group == get_group() ) return;
	for ( auto *ancestor = group; ancestor; ancestor = ancestor->get_group() )
		if ( ancestor == this )
		{
			Sqrat::Error::Instance().Throw( Sqrat::DefaultVM::Get(), "group cannot contain itself or form a cycle" );
			return;
		}
	auto *target = get_draw_target();
	if ( group && group->get_draw_target() != target )
	{
		Sqrat::Error::Instance().Throw( Sqrat::DefaultVM::Get(), "group requires an item in the same draw target" );
		return;
	}
	FeGroup *outer = get_group();
	while ( outer && outer->get_group() ) outer = outer->get_group();
	Vec3f destination;
	if ( preserve )
	{
		if ( !m_script_geometry_set ) set_script_size( Vec2f( get_width(), get_height() ) );
		const Vec2f pos = getPosition();
		const Vec3f source( pos.x, pos.y, get_z() );
		const auto transform = get_draw_transform();
		destination = transform.transform_point( source );
		m_detached_grid = grid_transform();
		m_detached_pixel_snap = get_pixel_snap();
		m_detached_color = get_inherited_color();
		for ( auto *ancestor = get_group(); ancestor; ancestor = ancestor->get_group() )
			m_visible = m_visible && ancestor->m_visible;
		m_index_offset = get_effective_index_offset();
		m_filter_offset = get_effective_filter_offset();
		m_detached_transform = transform;
		m_detached_source = source;
		m_detached_target = destination;
		m_detached = true;
	}
	auto &old = m_parent->elements;
	old.erase( std::remove( old.begin(), old.end(), this ), old.end() );
	m_parent = group ? static_cast<FePresentableParent *>( group ) : target;
	auto &items = m_parent->elements;
	if ( !group && outer )
	{
		auto next = std::find( items.begin(), items.end(), outer );
		if ( next != items.end() ) ++next;
		items.insert( next, this );
		m_zorder = outer->m_zorder;
	}
	else items.push_back( this );
	if ( preserve )
	{
		const auto grid = m_parent->get_grid( m_script_id ).transform( m_parent->get_grid_size() );
		Vec2f pos = grid.inverse_position( Vec2f( destination.x, destination.y ) );
		m_script_pos = pos;
		m_script_geometry_set = true;
		m_z = destination.z;
		for ( const char *name : { "x", "y", "z" } ) FeAnimation::stop( this, name );
	}
	refresh_script_geometry();
	on_group_update();
	FePresent::script_flag_sort_zorder();
	FePresent::script_flag_redraw();
}

void FeBasePresentable::apply_group_geometry( FeRenderGeometry &geometry ) const
{
	if ( !m_detached && !get_group() ) return;
	const auto matrix = get_draw_transform();
	const Color tint = get_inherited_color();
	if ( matrix.is_identity() && tint == Color::White ) return;
	if ( geometry.geometry_kind == FeRenderGeometryObjectPbr )
	{
		FeRenderMatrix4 model;
		std::memcpy( model.m, geometry.model_matrix, sizeof( model.m ) );
		model = matrix * model;
		std::memcpy( geometry.model_matrix, model.m, sizeof( model.m ) );
		model.normal_matrix( geometry.normal_matrix );
		const float colors[] = { tint.r / 255.f, tint.g / 255.f, tint.b / 255.f, tint.a / 255.f };
		for ( int i = 0; i < 4; ++i ) geometry.pbr_material.base_color_factor[i] *= colors[i];
		if ( tint.a < 255 )
		{
			geometry.pbr_material.alpha_mode = FeRenderPbrAlphaBlend;
			geometry.blend_mode = FeBlend::Alpha;
		}
		for ( int i = 0; i < geometry.light_count; ++i )
		{
			auto &light = geometry.lights[i];
			Vec3f p = matrix.transform_point( Vec3f( light.position[0], light.position[1], light.position[2] ) );
			light.position[0] = p.x; light.position[1] = p.y; light.position[2] = p.z;
			Vec3f origin = matrix.transform_point( Vec3f( 0, 0, 0 ) );
			Vec3f direction = matrix.transform_point( Vec3f( light.direction[0], light.direction[1], light.direction[2] ) );
			direction.x -= origin.x; direction.y -= origin.y; direction.z -= origin.z;
			float length = std::sqrt( direction.x*direction.x + direction.y*direction.y + direction.z*direction.z );
			const float scale_x = std::sqrt( matrix.m[0]*matrix.m[0] + matrix.m[1]*matrix.m[1] + matrix.m[2]*matrix.m[2] );
			const float scale_y = std::sqrt( matrix.m[4]*matrix.m[4] + matrix.m[5]*matrix.m[5] + matrix.m[6]*matrix.m[6] );
			light.radius *= ( scale_x + scale_y ) * 0.5f;
			light.range *= ( scale_x + scale_y ) * 0.5f;
			if ( length > 0 )
			{
				light.direction[0] = direction.x / length; light.direction[1] = direction.y / length; light.direction[2] = direction.z / length;
			}
		}
		return;
	}
	if ( geometry.has_external_vertices() )
	{
		geometry.vertices.assign( geometry.external_vertices, geometry.external_vertices + geometry.external_vertex_count );
		geometry.external_vertices = nullptr;
		geometry.external_vertex_count = 0;
	}
	for ( auto &vertex : geometry.vertices )
	{
		Vec3f p = matrix.transform_point( Vec3f( vertex.x, vertex.y, vertex.z ) );
		vertex.x = p.x; vertex.y = p.y; vertex.z = p.z;
		Color color = Color( vertex.r, vertex.g, vertex.b, vertex.a ) * tint;
		vertex.r = color.r; vertex.g = color.g; vertex.b = color.b; vertex.a = color.a;
	}
	if ( geometry.mask_enabled && geometry.letterbox )
		for ( int i = 0; i < 3; ++i )
		{
			float *point = geometry.mask_points + i * 3;
			Vec3f p = matrix.transform_point( Vec3f( point[0], point[1], point[2] ) );
			point[0] = p.x; point[1] = p.y; point[2] = p.z;
		}
}

Vec2f FeBasePresentable::snap_parent_position( const Vec2f &pos ) const
{
	if ( !m_detached ) return m_parent->snap_position_to_pixel( pos );
	const auto &target = *get_draw_target();
	const auto matrix = get_draw_transform( &target );
	return PixelSnap{ target, &matrix, get_z() }.position( pos );
}

Vec2f FeBasePresentable::snap_parent_size( const Vec2f &size ) const
{
	if ( !m_detached ) return m_parent->snap_size_to_pixel( size );
	const auto &target = *get_draw_target();
	const auto matrix = get_draw_transform( &target );
	return PixelSnap{ target, &matrix }.size( size );
}

FeGroup *FePresentableParent::add_group()
{
	const auto transform = get_grid( FePresent::script_get_fep()->get_script_id() ).transform( get_grid_size() );
	Vec2f size = transform.inverse_size( get_grid_size() );
	return add_group( 0, 0, size.x, size.y );
}

Sqrat::Object FePresentableParent::script_add_clone( FeBasePresentable *item )
{
	FeBasePresentable *clone = nullptr;
	if ( auto *image = dynamic_cast<FeImage *>( item ) ) clone = add_clone( image );
	else if ( auto *model = dynamic_cast<FeModel3D *>( item ) ) clone = add_clone( model );
	else
		Sqrat::Error::Instance().Throw( Sqrat::DefaultVM::Get(), "add_clone requires an image or 3D model" );
	return clone ? clone->get_script_object() : Sqrat::Object();
}

FeGroup *FePresentableParent::add_group( float w, float h ) { return add_group( 0, 0, w, h ); }

FeGroup *FePresentableParent::add_group( float x, float y, float w, float h )
{
	if ( !std::isfinite( w ) || !std::isfinite( h ) || w <= 0 || h <= 0 )
	{
		Sqrat::Error::Instance().Throw( Sqrat::DefaultVM::Get(), "group dimensions must be positive" );
		return nullptr;
	}
	auto *present = FePresent::script_get_fep();
	present->lock_layout_version();
	auto *group = new FeGroup( *this, x, y, w, h );
	elements.push_back( group );
	if ( present->get_script_id() < 0 ) present->m_layout_has_content = true;
	FePresent::script_flag_sort_zorder();
	FePresent::script_flag_redraw();
	return group;
}

void FeGroup::set_anchor( float x, float y ) { set_anchor( x, y, m_anchor.z ); }
void FeGroup::set_anchor( float x, float y, float z ) { m_anchor = Vec3f( x, y, z ); on_transform_update(); }
void FeGroup::set_rotation_origin( float x, float y ) { set_rotation_origin( x, y, m_rotation_origin.z ); }
void FeGroup::set_rotation_origin( float x, float y, float z ) { m_rotation_origin = Vec3f( x, y, z ); on_transform_update(); }
void FeGroup::set_transform_origin( float x, float y )
{
	m_transform_origin = Vec2f( x, y );
	m_anchor.x = m_rotation_origin.x = x;
	m_anchor.y = m_rotation_origin.y = y;
	on_transform_update();
}
void FeGroup::set_anchor_x( float x ) { set_anchor( x, m_anchor.y ); }
void FeGroup::set_anchor_y( float y ) { set_anchor( m_anchor.x, y ); }
void FeGroup::set_anchor_z( float z ) { set_anchor( m_anchor.x, m_anchor.y, z ); }
void FeGroup::set_rotation_origin_x( float x ) { set_rotation_origin( x, m_rotation_origin.y ); }
void FeGroup::set_rotation_origin_y( float y ) { set_rotation_origin( m_rotation_origin.x, y ); }
void FeGroup::set_rotation_origin_z( float z ) { set_rotation_origin( m_rotation_origin.x, m_rotation_origin.y, z ); }
void FeGroup::set_transform_origin_x( float x ) { set_transform_origin( x, m_transform_origin.y ); }
void FeGroup::set_transform_origin_y( float y ) { set_transform_origin( m_transform_origin.x, y ); }
void FeGroup::set_anchor_type( int value ) { m_anchor_type = value; auto p = align_type_to_vector( value ); set_anchor( p.x, p.y ); }
void FeGroup::set_rotation_origin_type( int value ) { m_rotation_origin_type = value; auto p = align_type_to_vector( value ); set_rotation_origin( p.x, p.y ); }
void FeGroup::set_transform_origin_type( int value ) { m_transform_origin_type = value; auto p = align_type_to_vector( value ); set_transform_origin( p.x, p.y ); }
