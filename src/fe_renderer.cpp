#include "fe_renderer.hpp"

#include <cmath>

namespace
{
	constexpr float FE_PI = 3.14159265358979323846f;
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

FeRenderGeometry::FeRenderGeometry()
	: texture_id( nullptr ),
	  texture_source_type( FeRenderTextureSourceNone ),
	  texture_repeated( false ),
	  texture_smooth( true ),
	  texture_mipmap( false ),
	  texture_width( 0.0f ),
	texture_height( 0.0f ),
	blend_mode( 0 ),
	shader( nullptr ),
	custom_shader( false ),
	textured( false ),
	texture_dynamic( false ),
	texture_content_version( 0 )
{
}

void FeRenderGeometry::clear()
{
	vertices.clear();
	texture_id = nullptr;
	texture_source_type = FeRenderTextureSourceNone;
	texture_repeated = false;
	texture_smooth = true;
	texture_mipmap = false;
	texture_width = 0.0f;
	texture_height = 0.0f;
	blend_mode = 0;
	shader = nullptr;
	custom_shader = false;
	textured = false;
	texture_dynamic = false;
	texture_content_version = 0;
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
	frame_number = 0;
	image_count = 0;
}
