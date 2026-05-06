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
	projection( FeRenderMatrix4::identity() )
{
}

void FePerspectiveCamera::update_projection( float viewport_width, float viewport_height )
{
	const float aspect_ratio = ( viewport_height > 0.0f ) ? viewport_width / viewport_height : 1.0f;
	projection = FeRenderMatrix4::perspective( fov_y_degrees, aspect_ratio, near_plane, far_plane );
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
	dynamic = false;
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
	hdri_texture.clear();
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
	texture_width( 0.0f ),
	texture_height( 0.0f ),
	blend_mode( 0 ),
	zbuffer( false ),
	translucent_depth_prepass( false ),
	pbr_collapse_group( 0 ),
	shader( nullptr ),
	custom_shader( false ),
	textured( false ),
	texture_dynamic( false ),
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
	texture_width = 0.0f;
	texture_height = 0.0f;
	blend_mode = 0;
	zbuffer = false;
	translucent_depth_prepass = false;
	pbr_collapse_group = 0;
	shader = nullptr;
	custom_shader = false;
	textured = false;
	texture_dynamic = false;
	texture_content_version = 0;
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
	viewport_width = 0;
	viewport_height = 0;
	antialiasing = 0;
	anisotropic = 0;
	frame_number = 0;
	image_count = 0;
}
