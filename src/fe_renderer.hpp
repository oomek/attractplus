#ifndef FE_RENDERER_HPP
#define FE_RENDERER_HPP

#include <cstdint>
#include <vector>
class FeShader;

struct FeRenderMatrix4
{
	float m[16];

	static FeRenderMatrix4 identity();
	static FeRenderMatrix4 perspective( float fov_y_degrees, float aspect_ratio, float near_plane, float far_plane );
};

struct FePerspectiveCamera
{
	float fov_y_degrees;
	float near_plane;
	float far_plane;
	float default_plane_z;
	FeRenderMatrix4 projection;

	FePerspectiveCamera();
	void update_projection( float viewport_width, float viewport_height );
};

struct FeRenderVertex
{
	float x;
	float y;
	float z;
	float u;
	float v;
	std::uint8_t r;
	std::uint8_t g;
	std::uint8_t b;
	std::uint8_t a;
};

enum FeRenderTextureSourceType
{
	FeRenderTextureSourceNone = 0,
	FeRenderTextureSourceContainer,
	FeRenderTextureSourceFontPage,
	FeRenderTextureSourceRawRgba
};

struct FeRenderRawTextureSource
{
	const unsigned char *pixels;
	unsigned int width;
	unsigned int height;
};

struct FeRenderGeometry
{
	std::vector<FeRenderVertex> vertices;
	const void *texture_id;
	int texture_source_type;
	bool texture_repeated;
	bool texture_smooth;
	bool texture_mipmap;
	float texture_width;
	float texture_height;
	int blend_mode;
	bool zbuffer;
	const FeShader *shader;
	bool custom_shader;
	bool textured;
	bool texture_dynamic;
	std::uint64_t texture_content_version;

	FeRenderGeometry();
	void clear();
};

struct FeRenderSurfaceFrame
{
	const void *surface_id;
	const void *surface_texture_id;
	int surface_texture_source_type;
	int nesting_level;
	FePerspectiveCamera camera;
	std::vector<FeRenderGeometry> geometry;
	std::vector<const void *> dependencies;
	int width;
	int height;
	bool mipmapped;
	bool clear;
	bool redraw;
	bool dynamic_content;
	std::uint64_t geometry_signature;
	std::uint64_t content_signature;

	FeRenderSurfaceFrame();
	void clear_frame();
};

struct FeRenderFrame
{
	FePerspectiveCamera camera;
	std::vector<FeRenderGeometry> images;
	std::vector<FeRenderSurfaceFrame> surfaces;
	int viewport_width;
	int viewport_height;
	int antialiasing;
	unsigned long long frame_number;
	unsigned long long image_count;

	FeRenderFrame();
	void clear();
};

#endif
