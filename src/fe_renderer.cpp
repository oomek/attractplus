#include "fe_renderer.hpp"

#include <cmath>

namespace
{
	constexpr float FE_PI = 3.14159265358979323846f;

	void fe_render_set_identity4( float *matrix )
	{
		if ( !matrix )
			return;

		for ( int i = 0; i < 16; ++i )
			matrix[i] = 0.0f;

		matrix[0] = 1.0f;
		matrix[5] = 1.0f;
		matrix[10] = 1.0f;
		matrix[15] = 1.0f;
	}

	void fe_render_set_identity3( float *matrix )
	{
		if ( !matrix )
			return;

		for ( int i = 0; i < 9; ++i )
			matrix[i] = 0.0f;

		matrix[0] = 1.0f;
		matrix[4] = 1.0f;
		matrix[8] = 1.0f;
	}
}

FeRenderMatrix4 FeRenderMatrix4::identity()
{
	FeRenderMatrix4 matrix = {};
	matrix.m[0] = 1.0f;
	matrix.m[5] = 1.0f;
	matrix.m[10] = 1.0f;
	matrix.m[15] = 1.0f;
	return matrix;
}

FeRenderMatrix4 FeRenderMatrix4::operator*( const FeRenderMatrix4 &other ) const
{
	FeRenderMatrix4 result = {};
	for ( int column = 0; column < 4; ++column )
		for ( int row = 0; row < 4; ++row )
			for ( int k = 0; k < 4; ++k )
				result.m[column * 4 + row] += m[k * 4 + row] * other.m[column * 4 + k];
	return result;
}

FeRenderMatrix4 &FeRenderMatrix4::translate( const Vec3f &offset )
{
	auto value = identity();
	value.m[12] = offset.x; value.m[13] = offset.y; value.m[14] = offset.z;
	return *this = *this * value;
}

FeRenderMatrix4 &FeRenderMatrix4::scale( const Vec3f &factors )
{
	auto value = identity();
	value.m[0] = factors.x; value.m[5] = factors.y; value.m[10] = factors.z;
	return *this = *this * value;
}

FeRenderMatrix4 &FeRenderMatrix4::rotate( float x, float y, float z, int order )
{
	// Match FeSprite: the named axes are applied in the listed order.
	static const int axes[6][3] = { {0,1,2}, {0,2,1}, {1,0,2}, {1,2,0}, {2,0,1}, {2,1,0} };
	const float angles[] = { x, y, z };
	auto rotation = identity();
	for ( int axis : axes[order >= 0 && order < 6 ? order : 0] )
	{
		auto value = identity();
		float angle = angles[axis] * FE_PI / 180.f;
		float c = std::cos( angle ), s = std::sin( angle );
		int a = ( axis + 1 ) % 3, b = ( axis + 2 ) % 3;
		value.m[a * 4 + a] = value.m[b * 4 + b] = c;
		value.m[b * 4 + a] = -s; value.m[a * 4 + b] = s;
		rotation = value * rotation;
	}
	return *this = *this * rotation;
}

Vec3f FeRenderMatrix4::transform_point( const Vec3f &p ) const
{
	return Vec3f( m[0]*p.x + m[4]*p.y + m[8]*p.z + m[12],
		m[1]*p.x + m[5]*p.y + m[9]*p.z + m[13],
		m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14] );
}

bool FeRenderMatrix4::normal_matrix( float *out ) const
{
	float cofactors[] = {
		m[5]*m[10]-m[9]*m[6], m[8]*m[6]-m[4]*m[10], m[4]*m[9]-m[8]*m[5],
		m[9]*m[2]-m[1]*m[10], m[0]*m[10]-m[8]*m[2], m[8]*m[1]-m[0]*m[9],
		m[1]*m[6]-m[5]*m[2], m[4]*m[2]-m[0]*m[6], m[0]*m[5]-m[4]*m[1] };
	float determinant = m[0]*cofactors[0] + m[4]*cofactors[3] + m[8]*cofactors[6];
	if ( std::abs( determinant ) < 1.e-12f ) return false;
	for ( int i = 0; i < 9; ++i ) out[i] = cofactors[i] / determinant;
	return true;
}

bool FeRenderMatrix4::is_identity() const
{
	const auto value = identity();
	for ( int i = 0; i < 16; ++i ) if ( m[i] != value.m[i] ) return false;
	return true;
}

FeRenderMatrix4 FeRenderMatrix4::perspective( float fov_y_degrees, float aspect_ratio, float near_plane, float far_plane )
{
	FeRenderMatrix4 matrix = {};
	const float safe_aspect_ratio = ( aspect_ratio > 0.0f ) ? aspect_ratio : 1.0f;
	const float fov_radians = fov_y_degrees * FE_PI / 180.0f;
	const float tan_half_fov = std::tan( fov_radians * 0.5f );
	const float safe_tan_half_fov = ( tan_half_fov > 0.0f ) ? tan_half_fov : 1.0f;

	matrix.m[0] = 1.0f / ( safe_aspect_ratio * safe_tan_half_fov );
	matrix.m[5] = 1.0f / safe_tan_half_fov;
	matrix.m[10] = -( far_plane + near_plane ) / ( far_plane - near_plane );
	matrix.m[11] = -1.0f;
	matrix.m[14] = -( 2.0f * far_plane * near_plane ) / ( far_plane - near_plane );
	return matrix;
}

FePerspectiveCamera::FePerspectiveCamera()
	: fov_y_degrees( 45.0f ),
	near_plane( 1.0f ),
	far_plane( 10000.0f ),
	default_plane_z( 0.0f ),
	flip_y( false ),
	projection( FeRenderMatrix4::identity() )
{
}

void FePerspectiveCamera::update_projection( float viewport_width, float viewport_height )
{
	const float aspect_ratio = ( viewport_height > 0.0f ) ? viewport_width / viewport_height : 1.0f;
	projection = FeRenderMatrix4::perspective( fov_y_degrees, aspect_ratio, near_plane, far_plane );
	if ( flip_y )
		projection.m[5] = -projection.m[5];
}

FeRenderTextureBinding::FeRenderTextureBinding()
{
	clear();
}

void FeRenderTextureBinding::clear()
{
	texture_id = nullptr;
	texture_source_type = FeRenderTextureSourceNone;
	repeated = false;
	smooth = true;
	mipmap = false;
	aniso = true;
	dynamic = false;
	blocking = false;
	width = 0.0f;
	height = 0.0f;
	content_version = 0;
	offset_u = 0.0f;
	offset_v = 0.0f;
	scale_u = 1.0f;
	scale_v = 1.0f;
	fit_scale_u = 1.0f;
	fit_scale_v = 1.0f;
	texcoord_set = 0;
}

FeRenderPbrLight::FeRenderPbrLight()
{
	clear();
}

void FeRenderPbrLight::clear()
{
	type = FeRenderPbrLightDirectional;
	color[0] = 1.0f;
	color[1] = 1.0f;
	color[2] = 1.0f;
	intensity = 1.0f;
	position[0] = 0.0f;
	position[1] = 0.0f;
	position[2] = 0.0f;
	direction[0] = 0.0f;
	direction[1] = 0.0f;
	direction[2] = -1.0f;
	range = 0.0f;
	inner_cone_cos = 0.0f;
	outer_cone_cos = 0.0f;
	radius = 0.0f;
}

FeRenderPbrMaterial::FeRenderPbrMaterial()
{
	clear();
}

void FeRenderPbrMaterial::clear()
{
	base_color_texture.clear();
	metallic_roughness_texture.clear();
	normal_texture.clear();
	occlusion_texture.clear();
	emissive_texture.clear();
	cubemap_texture.clear();
	artwork_shader = nullptr;
	base_color_factor[0] = 1.0f;
	base_color_factor[1] = 1.0f;
	base_color_factor[2] = 1.0f;
	base_color_factor[3] = 1.0f;
	emissive_factor[0] = 0.0f;
	emissive_factor[1] = 0.0f;
	emissive_factor[2] = 0.0f;
	metallic_factor = 1.0f;
	roughness_factor = 1.0f;
	ior = 1.5f;
	normal_scale = 1.0f;
	occlusion_strength = 1.0f;
	alpha_cutoff = 0.5f;
	alpha_mode = FeRenderPbrAlphaOpaque;
	artwork_shader_emissive = false;
	use_base_color_alpha = false;
	unlit = false;
	double_sided = false;
}

FeRenderPbrInstance::FeRenderPbrInstance()
{
	clear();
}

void FeRenderPbrInstance::clear()
{
	fe_render_set_identity4( model_matrix );
	fe_render_set_identity3( normal_matrix );
}

FeRenderGeometry::FeRenderGeometry()
	: external_vertices( nullptr ),
	  external_vertex_count( 0 ),
	  external_vertex_id( nullptr ),
	  geometry_kind( FeRenderGeometryLegacy2d ),
	  texture_id( nullptr ),
	  texture_source_type( FeRenderTextureSourceNone ),
	  texture_repeated( false ),
	  texture_smooth( true ),
	texture_mipmap( false ),
	texture_aniso( true ),
	texture_width( 0.0f ),
	texture_height( 0.0f ),
	clip_enabled( false ),
	clip_x( 0 ),
	clip_y( 0 ),
	clip_width( 0 ),
	clip_height( 0 ),
	blend_mode( 0 ),
	cutout_mode( 0 ),
	zbuffer( false ),
	translucent_depth_prepass( false ),
	pbr_collapse_group( 0 ),
	shader( nullptr ),
	custom_shader( false ),
	textured( false ),
	texture_dynamic( false ),
	texture_blocking( false ),
	texture_content_version( 0 ),
	light_count( 0 )
{
	ambient_color[0] = 0.0f;
	ambient_color[1] = 0.0f;
	ambient_color[2] = 0.0f;
	ambient_light = 0.0f;
	fe_render_set_identity4( model_matrix );
	fe_render_set_identity3( normal_matrix );
}

void FeRenderGeometry::clear()
{
	vertices.clear();
	pbr_instances.clear();
	external_vertices = nullptr;
	external_vertex_count = 0;
	external_vertex_id = nullptr;
	geometry_kind = FeRenderGeometryLegacy2d;
	texture_id = nullptr;
	texture_source_type = FeRenderTextureSourceNone;
	texture_repeated = false;
	texture_smooth = true;
	texture_mipmap = false;
	texture_aniso = true;
	texture_width = 0.0f;
	texture_height = 0.0f;
	clip_enabled = false;
	clip_x = 0;
	clip_y = 0;
	clip_width = 0;
	clip_height = 0;
	blend_mode = 0;
	cutout_mode = 0;
	image_plane = {};
	cutout_plane = {};
	image_mask_plane = {};
	cutout_mask_plane = {};
	zbuffer = false;
	translucent_depth_prepass = false;
	pbr_collapse_group = 0;
	shader = nullptr;
	custom_shader = false;
	textured = false;
	texture_dynamic = false;
	texture_blocking = false;
	texture_content_version = 0;
	mask_enabled = false;
	mask_fit = true;
	mask_texture.clear();
	for ( int i = 0; i < 4; ++i ) mask_rect[i] = mask_border[i] = 0.f;
	mask_scale = 1.f;
	for ( float &value : mask_points ) value = 0.f;
	for ( float &value : mask_size ) value = 0.f;
	pbr_material.clear();
	light_count = 0;
	for ( FeRenderPbrLight &light : lights )
		light.clear();
	ambient_color[0] = 0.0f;
	ambient_color[1] = 0.0f;
	ambient_color[2] = 0.0f;
	ambient_light = 0.0f;
	fe_render_set_identity4( model_matrix );
	fe_render_set_identity3( normal_matrix );
}

bool FeRenderGeometry::has_external_vertices() const
{
	return external_vertices != nullptr && external_vertex_count > 0;
}

bool FeRenderGeometry::has_pbr_instances() const
{
	return !pbr_instances.empty();
}

const FeRenderVertex *FeRenderGeometry::get_vertex_data() const
{
	if ( has_external_vertices() )
		return external_vertices;

	return vertices.empty() ? nullptr : vertices.data();
}

std::size_t FeRenderGeometry::get_vertex_count() const
{
	return has_external_vertices() ? external_vertex_count : vertices.size();
}

FeRenderSurfaceFrame::FeRenderSurfaceFrame()
	: surface_id( nullptr ),
	  surface_texture_id( nullptr ),
	  surface_texture_source_type( FeRenderTextureSourceNone ),
	  nesting_level( 0 ),
	width( 0 ),
	height( 0 ),
	mipmapped( false ),
	msaa( true ),
	clear( true ),
	  redraw( true ),
	  dynamic_content( false ),
	  geometry_signature( 0 ),
	  content_signature( 0 )
{
}

void FeRenderSurfaceFrame::clear_frame()
{
	surface_id = nullptr;
	surface_texture_id = nullptr;
	surface_texture_source_type = FeRenderTextureSourceNone;
	nesting_level = 0;
	camera = FePerspectiveCamera();
	geometry.clear();
	dependencies.clear();
	width = 0;
	height = 0;
	mipmapped = false;
	msaa = true;
	clear = true;
	redraw = true;
	dynamic_content = false;
	geometry_signature = 0;
	content_signature = 0;
}

FeRenderFrame::FeRenderFrame()
	: viewport_width( 0 ),
	  viewport_height( 0 ),
	  antialiasing( 0 ),
	  anisotropic( 0 ),
	  frame_number( 0 ),
	  image_count( 0 )
{
}

void FeRenderFrame::clear()
{
	camera = FePerspectiveCamera();
	images.clear();
	surfaces.clear();
	preloads.clear();
	viewport_width = 0;
	viewport_height = 0;
	antialiasing = 0;
	anisotropic = 0;
	frame_number = 0;
	image_count = 0;
}
