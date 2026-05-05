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
	float u1;
	float v1;
	float nx;
	float ny;
	float nz;
	float tx;
	float ty;
	float tz;
	float tw;
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

enum FeRenderGeometryKind
{
	FeRenderGeometryLegacy2d = 0,
	FeRenderGeometryObjectPbr
};

enum FeRenderPbrAlphaMode
{
	FeRenderPbrAlphaOpaque = 0,
	FeRenderPbrAlphaMask,
	FeRenderPbrAlphaBlend
};

enum FeRenderPbrLightType
{
	FeRenderPbrLightDirectional = 0,
	FeRenderPbrLightPoint,
	FeRenderPbrLightSpot
};

struct FeRenderTextureBinding
{
	const void *texture_id;
	int texture_source_type;
	bool repeated;
	bool smooth;
	bool mipmap;
	bool dynamic;
	float width;
	float height;
	std::uint64_t content_version;
	float offset_u;
	float offset_v;
	float scale_u;
	float scale_v;
	float fit_scale_u;
	float fit_scale_v;
	int texcoord_set;

	FeRenderTextureBinding();
	void clear();
};

struct FeRenderPbrLight
{
	int type;
	float color[3];
	float intensity;
	float position[3];
	float direction[3];
	float range;
	float inner_cone_cos;
	float outer_cone_cos;
	float radius;

	FeRenderPbrLight();
	void clear();
};

struct FeRenderPbrMaterial
{
	FeRenderTextureBinding base_color_texture;
	FeRenderTextureBinding metallic_roughness_texture;
	FeRenderTextureBinding normal_texture;
	FeRenderTextureBinding occlusion_texture;
	FeRenderTextureBinding emissive_texture;
	const FeShader *artwork_shader;
	float base_color_factor[4];
	float emissive_factor[3];
	float metallic_factor;
	float roughness_factor;
	float normal_scale;
	float occlusion_strength;
	float alpha_cutoff;
	int alpha_mode;
	bool artwork_shader_emissive;
	bool use_base_color_alpha;
	bool unlit;
	bool double_sided;

	FeRenderPbrMaterial();
	void clear();
};

struct FeRenderPbrInstance
{
	float model_matrix[16];
	float normal_matrix[9];

	FeRenderPbrInstance();
	void clear();
};

struct FeRenderGeometry
{
	std::vector<FeRenderVertex> vertices;
	std::vector<FeRenderPbrInstance> pbr_instances;
	const FeRenderVertex *external_vertices;
	std::size_t external_vertex_count;
	const void *external_vertex_id;
	int geometry_kind;
	const void *texture_id;
	int texture_source_type;
	bool texture_repeated;
	bool texture_smooth;
	bool texture_mipmap;
	float texture_width;
	float texture_height;
	int blend_mode;
	bool zbuffer;
	bool translucent_depth_prepass;
	std::uintptr_t pbr_collapse_group;
	const FeShader *shader;
	bool custom_shader;
	bool textured;
	bool texture_dynamic;
	std::uint64_t texture_content_version;
	FeRenderPbrMaterial pbr_material;
	int light_count;
	FeRenderPbrLight lights[4];
	float ambient_color[3];
	float ambient_light;
	float model_matrix[16];
	float normal_matrix[9];

	FeRenderGeometry();
	void clear();
	bool has_external_vertices() const;
	bool has_pbr_instances() const;
	const FeRenderVertex *get_vertex_data() const;
	std::size_t get_vertex_count() const;
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
	int anisotropic;
	unsigned long long frame_number;
	unsigned long long image_count;

	FeRenderFrame();
	void clear();
};

#endif
