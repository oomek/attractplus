#ifndef FE_SDL3_GPU_HPP
#define FE_SDL3_GPU_HPP

#include "fe_renderer.hpp"
#include "fe_blend.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
class FeImage;

class FeSdl3GpuContext
{
public:
	struct FrameStats
	{
		bool executed;
		bool acquired_swapchain;
		bool depth_ready;
		bool pipeline_ready;
		bool draw_ready;
		unsigned long long submitted_frames;
		int last_viewport_width;
		int last_viewport_height;
	};

	FeSdl3GpuContext();
	~FeSdl3GpuContext();

	void submit_frame( FeRenderFrame frame );
	const FeRenderFrame &get_frame() const;
	std::size_t get_texture_count() const;
	bool execute_frame( const std::vector<FeRenderGeometry> *overlay_geometry = nullptr );
	const FrameStats &get_frame_stats() const;
	bool is_available() const;
	bool should_present() const;
	bool has_submitted_frame() const;
	bool has_frame_content() const;
	bool capture_frame_rgba( std::vector<std::uint8_t> &pixels, int &width, int &height );
	bool capture_surface_rgba( const void *surface_texture_id, std::vector<std::uint8_t> &pixels, int &width, int &height );
	bool save_screenshot( const std::string &filename );
	void clear_layout_resources();
	bool present_blank_frame();
	bool ensure_video_subsystem();
	bool initialize( bool debug_mode = false, const char *driver_name = nullptr );
	void shutdown();
	bool wrap_native_window( void *native_window_handle, int width, int height );
	bool claim_window( SDL_Window *window );
	void release_window();
	SDL_Window *get_window() const;
	SDL_GPUDevice *get_device() const;
	void *get_native_window_handle() const;

private:
	struct CustomShaderEntry;
	struct BuiltinShaderEntry;

	struct TextureEntry
	{
		float width;
		float height;
		bool mipmapped;
		unsigned long long last_seen_frame;
		unsigned long long last_upload_frame;
		unsigned long long last_upload_content_version;
		SDL_GPUTexture *gpu_texture;
	};

	struct GeometryBufferEntry
	{
		unsigned long long last_seen_frame;
		std::size_t vertex_count;
		SDL_GPUBuffer *vertex_buffer;
		Uint32 vertex_buffer_size;

		GeometryBufferEntry()
			: last_seen_frame( 0 ),
			  vertex_count( 0 ),
			  vertex_buffer( nullptr ),
			  vertex_buffer_size( 0 )
		{
		}
	};

	struct PreparedImage
	{
		const FeRenderGeometry *geometry;
		SDL_GPUTexture *gpu_texture;
		SDL_GPUTexture *pbr_textures[5];
		std::size_t first_vertex;
		std::size_t vertex_count;
		int blend_mode;
		bool zbuffer;
		bool translucent_depth;
		bool object_pbr;
		bool texture_repeated;
		bool texture_smooth;
		bool texture_mipmap;
		CustomShaderEntry *custom_shader;
		BuiltinShaderEntry *builtin_shader;
		const FeRenderVertex *external_vertices;
		const void *external_vertex_id;
		Uint32 pbr_instance_first;
		Uint32 pbr_instance_count;
		bool pbr_instance_head;
	};

	struct PbrInstanceEntry
	{
		float model[16];
		float normal_matrix[3][4];
	};

	struct SurfaceEntry
	{
		unsigned long long last_seen_frame;
		int width;
		int height;
		bool mipmapped;
		bool rendered_once;
		std::uint64_t vertex_signature;
		std::uint64_t last_signature;
		SDL_GPUTexture *color_texture;
		SDL_GPUTexture *msaa_color_texture;
		SDL_GPUTexture *depth_texture;
		SDL_GPUSampleCount sample_count;
		SDL_GPUBuffer *vertex_buffer;
		Uint32 vertex_buffer_size;
	};

	struct CustomUniformBinding
	{
		std::string name;
		int slot;
		int components;

		CustomUniformBinding() : slot( 0 ), components( 0 ) {}
	};

	struct CustomVaryingBinding
	{
		std::string type;
		std::string name;
		int location;

		CustomVaryingBinding() : location( 0 ) {}
	};

	struct CustomSamplerBinding
	{
		std::string name;
		int slot;
		bool current_texture;
		FeImage *image;

		CustomSamplerBinding() : slot( 0 ), current_texture( false ), image( nullptr ) {}
	};

	struct CustomShaderEntry
	{
		std::string source_path;
		unsigned long long source_stamp;
		unsigned long long vertex_source_stamp;
		bool uses_current_texture;
		bool has_custom_vertex;
		bool fast_current_texture_only;
		bool compile_failed;
		SDL_GPUShader *vertex_shader;
		SDL_GPUShader *fragment_shader;
		SDL_GPUGraphicsPipeline *blend_pipelines[3][FeBlend::None + 1];
		std::vector<CustomUniformBinding> vertex_uniforms;
		std::vector<CustomUniformBinding> fragment_uniforms;
		std::vector<CustomSamplerBinding> fragment_samplers;

		CustomShaderEntry()
			: source_stamp( 0 ),
			  vertex_source_stamp( 0 ),
			  uses_current_texture( false ),
			  has_custom_vertex( false ),
			  fast_current_texture_only( false ),
			  compile_failed( false ),
			  vertex_shader( nullptr ),
			  fragment_shader( nullptr )
		{
			for ( int z = 0; z < 3; ++z )
				for ( int i = 0; i <= FeBlend::None; ++i )
					blend_pipelines[ z ][ i ] = nullptr;
		}
	};

	struct BuiltinShaderEntry
	{
		std::string source_id;
		SDL_GPUShader *fragment_shader;
		SDL_GPUGraphicsPipeline *blend_pipelines[3][FeBlend::None + 1];

		BuiltinShaderEntry()
			: fragment_shader( nullptr )
		{
			for ( int z = 0; z < 3; ++z )
				for ( int i = 0; i <= FeBlend::None; ++i )
					blend_pipelines[z][i] = nullptr;
		}
	};

	void sync_textures( const std::vector<FeRenderGeometry> *extra_geometry = nullptr );
	void clear_textures();
	void clear_geometry_buffers();
	void build_prepared_images();
	void prepare_geometry_batch(
		const std::vector<FeRenderGeometry> &geometry,
		bool use_surface_targets,
		std::vector<PreparedImage> &prepared_images,
		std::vector<FeRenderVertex> &vertex_stream );
	void release_texture( TextureEntry &entry );
	bool upload_texture( const void *texture_id, int texture_source_type, TextureEntry &entry );
	void release_white_texture();
	bool ensure_white_texture();
	void release_vertex_buffer();
	void release_pbr_instance_buffer();
	bool upload_gpu_vertex_buffer_data( const void *data, Uint32 size, SDL_GPUBuffer *&buffer, Uint32 &buffer_size );
	bool upload_gpu_vertex_buffer_data( SDL_GPUCommandBuffer *command_buffer, const void *data, Uint32 size, SDL_GPUBuffer *&buffer, Uint32 &buffer_size );
	bool upload_vertex_buffer();
	bool upload_vertex_buffer( const FeRenderVertex *vertices, std::size_t vertex_count, SDL_GPUBuffer *&buffer, Uint32 &buffer_size );
	bool upload_vertex_buffer( const std::vector<FeRenderVertex> &vertices, SDL_GPUBuffer *&buffer, Uint32 &buffer_size );
	void release_geometry_buffer( GeometryBufferEntry &entry );
	bool ensure_geometry_vertex_buffer( const PreparedImage &image, SDL_GPUBuffer *&buffer );
	bool can_instance_pbr_images( const PreparedImage &lhs, const PreparedImage &rhs ) const;
	std::uint64_t compute_pbr_instance_batch_hash( const PreparedImage &image ) const;
	void build_pbr_instance_batches( std::vector<PreparedImage> &prepared_images, std::vector<PbrInstanceEntry> &instance_data ) const;
	int get_requested_anisotropy() const;
	bool update_anisotropy();
	SDL_GPUSampler *create_sampler( SDL_GPUFilter filter, SDL_GPUSamplerMipmapMode mipmap_mode, SDL_GPUSamplerAddressMode address_mode, bool mipmapped, bool smooth );
	SDL_GPUSampler *get_image_sampler( bool smooth, bool repeated, bool mipmapped ) const;
	SDL_GPUSampleCount get_requested_sample_count() const;
	SDL_GPUSampleCount pick_sample_count( SDL_GPUTextureFormat swapchain_format ) const;
	bool update_sample_count( SDL_GPUTextureFormat swapchain_format );
	bool uses_multisampling() const;
	void release_color_target();
	bool ensure_color_target( int width, int height );
	void release_depth_target();
	bool ensure_depth_target( int width, int height );
	void release_surfaces();
	void release_surface_target( SurfaceEntry &entry );
	bool ensure_surface_target( const FeRenderSurfaceFrame &surface, SurfaceEntry &entry );
	void release_custom_shaders();
	void release_custom_shader( CustomShaderEntry &entry );
	void release_builtin_shaders();
	void release_builtin_shader( BuiltinShaderEntry &entry );
	CustomShaderEntry *get_custom_shader_entry( const FeRenderGeometry &image );
	CustomShaderEntry *get_builtin_blend_shader_entry( int blend_mode, const FeRenderGeometry &image );
	BuiltinShaderEntry *get_fast_builtin_shader_entry( int blend_mode );
	bool create_custom_shader_entry( const FeRenderGeometry &image, CustomShaderEntry &entry );
	bool create_builtin_blend_shader_entry( int blend_mode, const FeRenderGeometry &image, CustomShaderEntry &entry );
	bool create_fast_builtin_shader_entry( int blend_mode, BuiltinShaderEntry &entry );
	bool build_custom_fragment_shader_from_source(
		const FeRenderGeometry &image,
		const std::string &source_id,
		const std::string &raw_source,
		std::string &source_code,
		std::vector<CustomUniformBinding> &uniforms,
		std::vector<CustomSamplerBinding> &samplers );
	bool build_custom_vertex_shader(
		const FeRenderGeometry &image,
		std::string &source_code,
		std::vector<CustomUniformBinding> &uniforms );
	bool build_custom_fragment_shader(
		const FeRenderGeometry &image,
		std::string &source_code,
		std::vector<CustomUniformBinding> &uniforms,
		std::vector<CustomSamplerBinding> &samplers );
	void build_custom_uniform_data(
		const FeRenderGeometry &image,
		const std::vector<CustomUniformBinding> &uniforms,
		std::vector<float> &data ) const;
	bool render_surface_frames( SDL_GPUCommandBuffer *command_buffer, std::vector<SDL_GPUBuffer *> *temporary_pbr_buffers );
	bool render_prepared_geometry_batch(
		SDL_GPURenderPass *render_pass,
		SDL_GPUCommandBuffer *command_buffer,
		const FePerspectiveCamera &camera,
		int viewport_width,
		int viewport_height,
		const std::vector<PreparedImage> &prepared_images,
		const std::vector<FeRenderVertex> &vertex_stream,
		SDL_GPUBuffer **cached_vertex_buffer,
		Uint32 *cached_vertex_buffer_size,
		std::uint64_t *cached_vertex_signature,
		bool use_preuploaded_pbr_instances,
		std::vector<SDL_GPUBuffer *> *temporary_pbr_buffers,
		bool &drew_anything );
	bool render_geometry_batch(
		SDL_GPURenderPass *render_pass,
		SDL_GPUCommandBuffer *command_buffer,
		const FePerspectiveCamera &camera,
		int viewport_width,
		int viewport_height,
		const std::vector<FeRenderGeometry> &geometry,
		bool use_surface_targets,
		SDL_GPUBuffer **cached_vertex_buffer,
		Uint32 *cached_vertex_buffer_size,
		std::uint64_t *cached_vertex_signature,
		std::vector<SDL_GPUBuffer *> *temporary_pbr_buffers,
		bool &drew_anything );
	void release_image_pipeline();
	bool initialize_image_pipeline( SDL_GPUTextureFormat swapchain_format );
	void release_pbr_pipeline();
	bool initialize_pbr_pipeline( SDL_GPUTextureFormat swapchain_format );

	FeRenderFrame m_frame;
	FrameStats m_frame_stats;
	std::unordered_map<const void *, TextureEntry> m_textures;
	std::unordered_map<const void *, GeometryBufferEntry> m_geometry_buffers;
	std::unordered_map<const void *, SurfaceEntry> m_surfaces;
	std::vector<PreparedImage> m_prepared_images;
	std::vector<FeRenderVertex> m_vertex_stream;
	std::unordered_map<std::string, CustomShaderEntry> m_custom_shaders;
	std::unordered_map<std::string, std::string> m_custom_shader_sources;
	std::unordered_map<int, BuiltinShaderEntry> m_builtin_shaders;
	bool m_sdl_ready;
	bool m_owns_sdl_video;
	bool m_window_claimed;
	SDL_Window *m_window;
	SDL_GPUDevice *m_device;
	SDL_GPUBuffer *m_vertex_buffer;
	SDL_GPUBuffer *m_pbr_instance_buffer;
	Uint32 m_vertex_buffer_size;
	Uint32 m_pbr_instance_buffer_size;
	std::uint64_t m_vertex_buffer_signature;
	SDL_GPUShader *m_vertex_shader;
	SDL_GPUShader *m_alpha_prepass_shader;
	SDL_GPUShader *m_fragment_shaders[FeBlend::None + 1];
	SDL_GPUShader *m_pbr_vertex_shader;
	SDL_GPUShader *m_pbr_fragment_shader;
	SDL_GPUSampler *m_linear_sampler;
	SDL_GPUSampler *m_linear_repeat_sampler;
	SDL_GPUSampler *m_linear_mipmap_sampler;
	SDL_GPUSampler *m_linear_mipmap_repeat_sampler;
	SDL_GPUSampler *m_nearest_sampler;
	SDL_GPUSampler *m_nearest_repeat_sampler;
	SDL_GPUSampler *m_nearest_mipmap_sampler;
	SDL_GPUSampler *m_nearest_mipmap_repeat_sampler;
	SDL_GPUGraphicsPipeline *m_blend_pipelines[3][FeBlend::None + 1];
	SDL_GPUGraphicsPipeline *m_alpha_prepass_pipeline;
	SDL_GPUGraphicsPipeline *m_pbr_pipelines[3][2][2];
	SDL_GPUTexture *m_white_texture;
	SDL_GPUTexture *m_color_target_texture;
	int m_color_target_width;
	int m_color_target_height;
	SDL_GPUTexture *m_depth_texture;
	SDL_GPUTextureFormat m_depth_format;
	int m_depth_width;
	int m_depth_height;
	int m_anisotropy;
	SDL_GPUSampleCount m_sample_count;
	SDL_GPUTextureFormat m_swapchain_format;
	bool m_pipeline_attempted;
	bool m_present_disabled;
	unsigned int m_failed_present_frames;
	bool m_has_submitted_frame;
};

#endif
