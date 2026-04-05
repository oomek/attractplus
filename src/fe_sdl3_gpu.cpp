#include "fe_sdl3_gpu.hpp"
#include "fe_image.hpp"
#include "fe_font.hpp"
#include "media.hpp"
#include "fe_shader.hpp"
#include "fe_blend.hpp"
#include "fe_settings.hpp"
#include "fe_util.hpp"
#include "fe_base.hpp"
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <cstring>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

namespace
{
	const int FE_MAX_CUSTOM_SHADER_UNIFORMS = 32;
	std::string join_path( const std::string &base, const std::string &suffix );
	std::string get_base_path();

	bool save_rgba_png( const std::string &filename, int width, int height, const std::uint8_t *pixels )
	{
		if ( width <= 0 || height <= 0 || !pixels )
			return false;

		SDL_Surface *surface = SDL_CreateSurfaceFrom(
			width,
			height,
			SDL_PIXELFORMAT_RGBA32,
			const_cast<std::uint8_t *>( pixels ),
			width * 4 );
		if ( !surface )
			return false;

		const bool result = IMG_SavePNG( surface, filename.c_str() );
		SDL_DestroySurface( surface );
		return result;
	}

	struct ParsedCustomUniform
	{
		std::string type;
		std::string name;
		bool sampler;
	};

	struct ParsedCustomVarying
	{
		std::string type;
		std::string name;
	};

	void replace_all( std::string &value, const std::string &from, const std::string &to )
	{
		if ( from.empty() )
			return;

		std::size_t start = 0;
		while (( start = value.find( from, start ) ) != std::string::npos )
		{
			value.replace( start, from.size(), to );
			start += to.size();
		}
	}

	struct ShaderCompileLogCapture
	{
		std::ostringstream stream;
	};

	bool shader_compile_output_callback( const char *line, void *opaque )
	{
		if ( line && line[0] )
		{
			const std::string message = trim( line );
			FeLog() << "shader_compile: " << message << std::endl;
			ShaderCompileLogCapture *capture = static_cast<ShaderCompileLogCapture *>( opaque );
			if ( capture )
				capture->stream << message << "\n";
		}
		return true;
	}

	SDL_GPUBlendFactor to_sdl_blend_factor( FeBlend::Factor factor )
	{
		switch ( factor )
		{
			case FeBlend::Zero: return SDL_GPU_BLENDFACTOR_ZERO;
			case FeBlend::One: return SDL_GPU_BLENDFACTOR_ONE;
			case FeBlend::SrcAlpha: return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
			case FeBlend::OneMinusSrcAlpha: return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
			case FeBlend::SrcColor: return SDL_GPU_BLENDFACTOR_SRC_COLOR;
			case FeBlend::OneMinusSrcColor: return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
			case FeBlend::DstColor: return SDL_GPU_BLENDFACTOR_DST_COLOR;
			default: return SDL_GPU_BLENDFACTOR_ONE;
		}
	}

	SDL_GPUBlendOp to_sdl_blend_op( FeBlend::Equation equation )
	{
		switch ( equation )
		{
			case FeBlend::ReverseSubtractEquation: return SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
			case FeBlend::AddEquation:
			default:
				return SDL_GPU_BLENDOP_ADD;
		}
	}

	SDL_GPUColorTargetBlendState make_gpu_blend_state( int mode )
	{
		const FeBlend::State state = FeBlend::get_state( mode );

		SDL_GPUColorTargetBlendState blend_state = {};
		blend_state.color_write_mask =
			SDL_GPU_COLORCOMPONENT_R |
			SDL_GPU_COLORCOMPONENT_G |
			SDL_GPU_COLORCOMPONENT_B |
			SDL_GPU_COLORCOMPONENT_A;
		blend_state.enable_color_write_mask = true;
		blend_state.enable_blend = state.enable_blend;
		blend_state.src_color_blendfactor = to_sdl_blend_factor( state.src_color_factor );
		blend_state.dst_color_blendfactor = to_sdl_blend_factor( state.dst_color_factor );
		blend_state.color_blend_op = to_sdl_blend_op( state.color_equation );
		blend_state.src_alpha_blendfactor = to_sdl_blend_factor( state.src_alpha_factor );
		blend_state.dst_alpha_blendfactor = to_sdl_blend_factor( state.dst_alpha_factor );
		blend_state.alpha_blend_op = to_sdl_blend_op( state.alpha_equation );

		return blend_state;
	}

	enum DepthPipelineMode
	{
		DepthPipelineNone = 0,
		DepthPipelineWrite = 1,
		DepthPipelineTestOnly = 2
	};

	int get_depth_pipeline_index( bool zbuffer, bool translucent )
	{
		if ( !zbuffer )
			return DepthPipelineNone;

		return translucent ? DepthPipelineTestOnly : DepthPipelineWrite;
	}

	void configure_pipeline_depth_state( SDL_GPUGraphicsPipelineCreateInfo &pipeline_info, int depth_mode )
	{
		pipeline_info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
		pipeline_info.target_info.has_depth_stencil_target = true;
		pipeline_info.depth_stencil_state.enable_depth_test = ( depth_mode != DepthPipelineNone );
		pipeline_info.depth_stencil_state.enable_depth_write = ( depth_mode == DepthPipelineWrite );
		pipeline_info.depth_stencil_state.compare_op =
			( depth_mode != DepthPipelineNone ) ? SDL_GPU_COMPAREOP_LESS_OR_EQUAL : SDL_GPU_COMPAREOP_ALWAYS;
	}

	bool geometry_uses_translucent_depth_pipeline( const FeRenderGeometry &image )
	{
		if ( !image.zbuffer )
			return false;

		return image.textured &&
			( ( image.blend_mode == FeBlend::Alpha ) || ( image.blend_mode == FeBlend::Premultiplied ) );
	}

	bool parse_custom_uniforms(
		const std::string &source,
		std::vector<ParsedCustomUniform> &uniforms,
		std::string &stripped_source )
	{
		static const std::regex uniform_regex(
			"^\\s*uniform\\s+(float|vec2|vec3|vec4|sampler2D)\\s+([^;]+)\\s*;\\s*$" );

		std::stringstream input( source );
		std::string line;
		stripped_source.clear();
		uniforms.clear();

		while ( std::getline( input, line ) )
		{
			if ( line.find( "#version" ) != std::string::npos )
				continue;

			std::smatch match;
			if ( std::regex_match( line, match, uniform_regex ) )
			{
				const std::string type = match[ 1 ].str();
				std::string names = match[ 2 ].str();
				std::stringstream names_stream( names );
				std::string name;

				while ( std::getline( names_stream, name, ',' ) )
				{
					name = trim( name );
					if ( name.empty() )
						continue;

					ParsedCustomUniform uniform = {};
					uniform.type = type;
					uniform.name = name;
					uniform.sampler = ( uniform.type == "sampler2D" );
					uniforms.push_back( uniform );
				}
				continue;
			}

			stripped_source += line;
			stripped_source += "\n";
		}

		return true;
	}

	bool parse_custom_varyings(
		const std::string &source,
		std::vector<ParsedCustomVarying> &varyings,
		std::string &stripped_source )
	{
		static const std::regex varying_regex(
			"^\\s*varying\\s+(float|vec2|vec3|vec4)\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*;\\s*$" );

		std::stringstream input( source );
		std::string line;
		stripped_source.clear();
		varyings.clear();

		while ( std::getline( input, line ) )
		{
			std::smatch match;
			if ( std::regex_match( line, match, varying_regex ) )
			{
				ParsedCustomVarying varying = {};
				varying.type = match[ 1 ].str();
				varying.name = match[ 2 ].str();
				varyings.push_back( varying );
				continue;
			}

			stripped_source += line;
			stripped_source += "\n";
		}

		return true;
	}

	std::string build_custom_uniform_macro( const std::string &name, int slot, int components )
	{
		std::ostringstream stream;
		stream << "#define " << name << " custom_ubo.values[" << slot << "].";

		switch ( components )
		{
			case 1: stream << "x"; break;
			case 2: stream << "xy"; break;
			case 3: stream << "xyz"; break;
			case 4:
			default:
				stream << "xyzw";
				break;
		}

		return stream.str();
	}

	void get_geometry_size( const FeRenderGeometry &image, float &width, float &height )
	{
		width = 0.0f;
		height = 0.0f;
		if ( image.vertices.empty() )
			return;

		float min_x = image.vertices.front().x;
		float max_x = image.vertices.front().x;
		float min_y = image.vertices.front().y;
		float max_y = image.vertices.front().y;

		for ( const FeRenderVertex &vertex : image.vertices )
		{
			min_x = std::min( min_x, vertex.x );
			max_x = std::max( max_x, vertex.x );
			min_y = std::min( min_y, vertex.y );
			max_y = std::max( max_y, vertex.y );
		}

		width = max_x - min_x;
		height = max_y - min_y;
	}

	std::uint64_t hash_combine_u64( std::uint64_t seed, std::uint64_t value )
	{
		return seed ^ ( value + 0x9e3779b97f4a7c15ULL + ( seed << 6 ) + ( seed >> 2 ) );
	}

	std::uint64_t compute_geometry_signature( const std::vector<FeRenderGeometry> &geometry )
	{
		std::uint64_t signature = 1469598103934665603ULL;
		for ( const FeRenderGeometry &image : geometry )
		{
			signature = hash_combine_u64( signature, reinterpret_cast<std::uint64_t>( image.texture_id ) );
			signature = hash_combine_u64( signature, static_cast<std::uint64_t>( image.texture_source_type ) );
			signature = hash_combine_u64( signature, static_cast<std::uint64_t>( image.texture_repeated ? 1 : 0 ) );
			signature = hash_combine_u64( signature, static_cast<std::uint64_t>( image.texture_smooth ? 1 : 0 ) );
			signature = hash_combine_u64( signature, static_cast<std::uint64_t>( image.blend_mode ) );
			signature = hash_combine_u64( signature, static_cast<std::uint64_t>( image.zbuffer ? 1 : 0 ) );
			signature = hash_combine_u64( signature, static_cast<std::uint64_t>( image.custom_shader ? 1 : 0 ) );
			signature = hash_combine_u64( signature, static_cast<std::uint64_t>( image.vertices.size() ) );

			for ( const FeRenderVertex &vertex : image.vertices )
			{
				signature = hash_combine_u64( signature, static_cast<std::uint64_t>( std::lround( vertex.x * 1024.0f ) ) );
				signature = hash_combine_u64( signature, static_cast<std::uint64_t>( std::lround( vertex.y * 1024.0f ) ) );
				signature = hash_combine_u64( signature, static_cast<std::uint64_t>( std::lround( vertex.z * 1024.0f ) ) );
				signature = hash_combine_u64( signature, static_cast<std::uint64_t>( std::lround( vertex.u * 1024.0f ) ) );
				signature = hash_combine_u64( signature, static_cast<std::uint64_t>( std::lround( vertex.v * 1024.0f ) ) );
				signature = hash_combine_u64( signature, static_cast<std::uint64_t>( vertex.r ) );
				signature = hash_combine_u64( signature, static_cast<std::uint64_t>( vertex.g ) );
				signature = hash_combine_u64( signature, static_cast<std::uint64_t>( vertex.b ) );
				signature = hash_combine_u64( signature, static_cast<std::uint64_t>( vertex.a ) );
			}
		}
		return signature;
	}

	Uint32 get_mip_level_count( Uint32 width, Uint32 height )
	{
		Uint32 levels = 1;
		while ( width > 1 || height > 1 )
		{
			width = std::max<Uint32>( 1, width / 2 );
			height = std::max<Uint32>( 1, height / 2 );
			levels++;
		}
		return levels;
	}

	std::string build_builtin_vertex_shader()
	{
		return
			"#version 450\n"
			"layout(location = 0) in vec3 in_position;\n"
			"layout(location = 1) in vec2 in_texcoord;\n"
			"layout(location = 2) in vec4 in_color;\n"
			"layout(location = 0) out vec2 frag_texcoord;\n"
			"layout(location = 1) out vec4 frag_color;\n"
			"layout(set = 1, binding = 0) uniform DrawUniforms\n"
			"{\n"
			"\tmat4 projection;\n"
			"\tfloat viewport_width;\n"
			"\tfloat viewport_height;\n"
			"\tfloat plane_distance;\n"
			"\tfloat reserved;\n"
			"} ubo;\n"
			"void main()\n"
			"{\n"
			"\tvec3 view_position = vec3(\n"
			"\t\tin_position.x - ( ubo.viewport_width * 0.5 ),\n"
			"\t\t( ubo.viewport_height * 0.5 ) - in_position.y,\n"
			"\t\t-( ubo.plane_distance - in_position.z ) );\n"
			"\tgl_Position = ubo.projection * vec4( view_position, 1.0 );\n"
			"\tfrag_texcoord = in_texcoord;\n"
			"\tfrag_color = in_color;\n"
			"}\n";
	}

	std::string build_builtin_passthrough_fragment_shader()
	{
		return
			"#version 450\n"
			"layout(location = 0) in vec2 frag_texcoord;\n"
			"layout(location = 1) in vec4 frag_color;\n"
			"layout(location = 0) out vec4 out_color;\n"
			"layout(set = 2, binding = 0) uniform sampler2D image_texture;\n"
			"void main()\n"
			"{\n"
			"\tivec2 tex_size = textureSize(image_texture, 0);\n"
			"\tvec2 uv = tex_size.x > 0 && tex_size.y > 0 ? frag_texcoord / vec2(tex_size) : vec2(0.0);\n"
			"\tvec4 pixel = texture(image_texture, uv);\n"
			"\tout_color = frag_color * pixel;\n"
			"}\n";
	}

	std::string build_alpha_prepass_fragment_shader()
	{
		return
			"#version 450\n"
			"layout(location = 0) in vec2 frag_texcoord;\n"
			"layout(location = 1) in vec4 frag_color;\n"
			"layout(location = 0) out vec4 out_color;\n"
			"layout(set = 2, binding = 0) uniform sampler2D image_texture;\n"
			"void main()\n"
			"{\n"
			"\tivec2 tex_size = textureSize(image_texture, 0);\n"
			"\tvec2 uv = tex_size.x > 0 && tex_size.y > 0 ? frag_texcoord / vec2(tex_size) : vec2(0.0);\n"
			"\tfloat alpha = (frag_color * texture(image_texture, uv)).a;\n"
			"\tif (alpha < 0.5) discard;\n"
			"\tout_color = vec4(0.0);\n"
			"}\n";
	}

	std::string build_fast_builtin_fragment_shader( int blend_mode )
	{
		const char *raw_source = FeBlend::get_default_shader_source( blend_mode );
		if ( !raw_source || !raw_source[0] )
			return {};

		std::string source( raw_source );
		source = regex_replace( source, std::regex( "\\buniform\\s+sampler2D\\s+texture\\s*;" ), "" );
		source = regex_replace( source, std::regex( "\\btexture2D\\s*\\(\\s*texture\\s*,\\s*gl_TexCoord\\[0\\]\\.xy\\s*\\)" ), "pixel_sample()" );
		source = regex_replace( source, std::regex( "\\bgl_Color\\b" ), "frag_color" );
		source = regex_replace( source, std::regex( "\\bgl_FragColor\\b" ), "out_color" );

		std::ostringstream stream;
		stream
			<< "#version 450\n"
			<< "layout(location = 0) in vec2 frag_texcoord;\n"
			<< "layout(location = 1) in vec4 frag_color;\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "layout(set = 2, binding = 0) uniform sampler2D image_texture;\n"
			<< "\n"
			<< "vec4 pixel_sample()\n"
			<< "{\n"
			<< "\tivec2 tex_size = textureSize(image_texture, 0);\n"
			<< "\tvec2 uv = tex_size.x > 0 && tex_size.y > 0 ? frag_texcoord / vec2(tex_size) : vec2(0.0);\n"
			<< "\treturn texture(image_texture, uv);\n"
			<< "}\n"
			<< "\n"
			<< source;
		return stream.str();
	}

}

FeSdl3GpuContext::FeSdl3GpuContext()
{
	m_frame_stats.executed = false;
	m_frame_stats.acquired_swapchain = false;
	m_frame_stats.depth_ready = false;
	m_frame_stats.pipeline_ready = false;
	m_frame_stats.draw_ready = false;
	m_frame_stats.submitted_frames = 0;
	m_frame_stats.last_viewport_width = 0;
	m_frame_stats.last_viewport_height = 0;

	m_sdl_ready = false;
	m_window_claimed = false;
	m_window = nullptr;
	m_device = nullptr;
	m_vertex_buffer = nullptr;
	m_vertex_buffer_size = 0;
	m_vertex_buffer_signature = 0;
	m_vertex_shader = nullptr;
	m_alpha_prepass_shader = nullptr;
	for ( int i = 0; i <= FeBlend::None; ++i )
	{
		m_fragment_shaders[i] = nullptr;
		for ( int z = 0; z < 3; ++z )
			m_blend_pipelines[z][i] = nullptr;
	}
	m_alpha_prepass_pipeline = nullptr;
	m_linear_sampler = nullptr;
	m_linear_repeat_sampler = nullptr;
	m_nearest_sampler = nullptr;
	m_nearest_repeat_sampler = nullptr;
	m_white_texture = nullptr;
	m_depth_texture = nullptr;
	m_depth_format = SDL_GPU_TEXTUREFORMAT_INVALID;
	m_depth_width = 0;
	m_depth_height = 0;
	m_swapchain_format = SDL_GPU_TEXTUREFORMAT_INVALID;
	m_pipeline_attempted = false;
	m_present_disabled = false;
	m_failed_present_frames = 0;
	m_has_submitted_frame = false;
	m_logged_successful_present = false;
	m_debug_logging_enabled = false;
}

FeSdl3GpuContext::~FeSdl3GpuContext()
{
	shutdown();
}

void FeSdl3GpuContext::submit_frame( const FeRenderFrame &frame )
{
	m_frame = frame;
	m_has_submitted_frame = true;
	if ( m_debug_logging_enabled )
		build_prepared_images();
	else
	{
		m_prepared_images.clear();
		m_vertex_stream.clear();
	}

	if ( m_debug_logging_enabled )
	{
		std::size_t surface_geometry_count = 0;
		std::size_t custom_shader_count = 0;
		std::size_t surface_custom_shader_count = 0;
		std::set<std::string> shader_paths;
		for ( const FeRenderGeometry &image : m_frame.images )
		{
			if ( image.custom_shader )
			{
				++custom_shader_count;
				if ( image.shader && !image.shader->get_fragment_source_path().empty() )
					shader_paths.insert( image.shader->get_fragment_source_path() );
			}
		}
		for ( const FeRenderSurfaceFrame &surface : m_frame.surfaces )
		{
			surface_geometry_count += surface.geometry.size();
			for ( const FeRenderGeometry &image : surface.geometry )
			{
				if ( image.custom_shader )
				{
					++surface_custom_shader_count;
					if ( image.shader && !image.shader->get_fragment_source_path().empty() )
						shader_paths.insert( image.shader->get_fragment_source_path() );
				}
			}
		}

		std::ostringstream stream;
		stream
			<< "submit_frame:"
			<< " images=" << m_frame.images.size()
			<< " surfaces=" << m_frame.surfaces.size()
			<< " surface_geometry=" << surface_geometry_count
			<< " prepared_images=" << m_prepared_images.size()
			<< " vertices=" << m_vertex_stream.size()
			<< " custom_shaders=" << custom_shader_count
			<< " surface_custom_shaders=" << surface_custom_shader_count
			<< " viewport=" << m_frame.viewport_width << "x" << m_frame.viewport_height;
		write_debug_log( stream.str().c_str() );

		if ( !shader_paths.empty() )
		{
			std::ostringstream shader_stream;
			shader_stream << "submit_frame: shader_paths=";
			bool first = true;
			for ( const std::string &path : shader_paths )
			{
				if ( !first )
					shader_stream << ";";
				first = false;
				shader_stream << path;
			}
			write_debug_log( shader_stream.str().c_str() );
		}
	}

}

const FeRenderFrame &FeSdl3GpuContext::get_frame() const
{
	return m_frame;
}

std::size_t FeSdl3GpuContext::get_texture_count() const
{
	return m_textures.size();
}

const FeSdl3GpuContext::FrameStats &FeSdl3GpuContext::get_frame_stats() const
{
	return m_frame_stats;
}

void FeSdl3GpuContext::clear_layout_resources()
{
	release_surfaces();
	clear_textures();
}

void FeSdl3GpuContext::sync_textures( const std::vector<FeRenderGeometry> *extra_geometry )
{
	std::size_t geometry_count = m_frame.images.size();
	for ( const FeRenderSurfaceFrame &surface : m_frame.surfaces )
		geometry_count += surface.geometry.size();
	if ( extra_geometry )
		geometry_count += extra_geometry->size();

	if ( geometry_count == 0 )
	{
		clear_textures();
		return;
	}

	auto sync_geometry = [&]( const FeRenderGeometry &image )
	{
		if ( !image.texture_id )
			return;

		if ( image.texture_source_type == FeRenderTextureSourceContainer )
		{
			const FeBaseTextureContainer *container = static_cast<const FeBaseTextureContainer *>( image.texture_id );
			if ( dynamic_cast<const FeSurfaceTextureContainer *>( container ) != nullptr )
				return;
		}

		TextureEntry &entry = m_textures[ image.texture_id ];
		const float previous_width = entry.width;
		const float previous_height = entry.height;
		const bool previous_mipmapped = entry.mipmapped;
		const unsigned long long content_version =
			( image.texture_content_version != 0 )
				? image.texture_content_version
				: m_frame.frame_number;
		entry.width = image.texture_width;
		entry.height = image.texture_height;
		entry.mipmapped = image.texture_mipmap;
		entry.last_seen_frame = m_frame.frame_number;
		if ( is_available() )
		{
			const bool had_texture = ( entry.gpu_texture != nullptr );
			const FeBaseTextureContainer *source_container =
				( image.texture_source_type == FeRenderTextureSourceContainer )
					? static_cast<const FeBaseTextureContainer *>( image.texture_id )
					: nullptr;
			const bool waiting_for_first_dynamic_frame =
				( source_container != nullptr ) &&
				( dynamic_cast<const FeTextureContainer *>( source_container ) != nullptr ) &&
				( static_cast<const FeTextureContainer *>( source_container )->get_media() != nullptr ) &&
				image.texture_dynamic &&
				!had_texture &&
				( image.texture_content_version == 0 );
			const bool has_explicit_content_version =
				( image.texture_content_version != 0 );
			const bool needs_upload =
				!had_texture ||
				( previous_width != image.texture_width ) ||
				( previous_height != image.texture_height ) ||
				( previous_mipmapped != image.texture_mipmap ) ||
				( has_explicit_content_version &&
					entry.last_upload_content_version != content_version ) ||
				( image.texture_dynamic && entry.last_upload_content_version != content_version );

			if ( needs_upload && !waiting_for_first_dynamic_frame )
			{
				if ( upload_texture( image.texture_id, image.texture_source_type, entry ) )
				{
					entry.last_upload_frame = m_frame.frame_number;
					entry.last_upload_content_version = content_version;
				}
			}
		}
	};

	for ( const FeRenderGeometry &image : m_frame.images )
		sync_geometry( image );

	for ( const FeRenderSurfaceFrame &surface : m_frame.surfaces )
		for ( const FeRenderGeometry &image : surface.geometry )
			sync_geometry( image );

	if ( extra_geometry )
		for ( const FeRenderGeometry &image : *extra_geometry )
			sync_geometry( image );

	for ( auto it = m_textures.begin(); it != m_textures.end(); )
	{
		if ( it->second.last_seen_frame != m_frame.frame_number )
		{
			release_texture( it->second );
			it = m_textures.erase( it );
		}
		else
			++it;
	}
}

void FeSdl3GpuContext::clear_textures()
{
	for ( auto &entry : m_textures )
	{
		release_texture( entry.second );
	}

	m_textures.clear();
}

void FeSdl3GpuContext::build_prepared_images()
{
	m_prepared_images.clear();
	m_vertex_stream.clear();
	m_prepared_images.reserve( m_frame.images.size() );

	for ( const FeRenderGeometry &image : m_frame.images )
	{
		PreparedImage prepared = {};
		prepared.geometry = &image;
		prepared.first_vertex = m_vertex_stream.size();
		prepared.vertex_count = image.vertices.size();
		prepared.blend_mode = image.blend_mode;
		if ( image.textured )
		{
			prepared.gpu_texture = nullptr;
			auto surface_it = m_surfaces.find( image.texture_id );
			if ( surface_it != m_surfaces.end() )
				prepared.gpu_texture = surface_it->second.color_texture;

			if ( !prepared.gpu_texture )
			{
				auto it = m_textures.find( image.texture_id );
				prepared.gpu_texture = ( it != m_textures.end() ) ? it->second.gpu_texture : nullptr;
			}
		}
		else
			prepared.gpu_texture = ensure_white_texture() ? m_white_texture : nullptr;

		m_vertex_stream.insert( m_vertex_stream.end(), image.vertices.begin(), image.vertices.end() );

		m_prepared_images.push_back( prepared );
	}
}

bool FeSdl3GpuContext::is_available() const
{
	return ( m_device != nullptr ) && ( m_window != nullptr ) && m_window_claimed;
}

bool FeSdl3GpuContext::should_present() const
{
	return is_available() && !m_present_disabled && has_submitted_frame() && has_frame_content();
}

bool FeSdl3GpuContext::has_submitted_frame() const
{
	return m_has_submitted_frame;
}

bool FeSdl3GpuContext::has_frame_content() const
{
	if ( !m_frame.images.empty() )
		return true;

	for ( const FeRenderSurfaceFrame &surface : m_frame.surfaces )
		if ( !surface.geometry.empty() )
			return true;

	return false;
}

bool FeSdl3GpuContext::capture_frame_rgba( std::vector<std::uint8_t> &pixels, int &width, int &height )
{
	if ( !is_available() || !has_submitted_frame() )
		return false;

	width = m_frame.viewport_width;
	height = m_frame.viewport_height;
	if (( width <= 0 ) || ( height <= 0 ))
	{
		int sdl_width = 0;
		int sdl_height = 0;
		SDL_GetWindowSizeInPixels( m_window, &sdl_width, &sdl_height );
		width = sdl_width;
		height = sdl_height;
	}

	if (( width <= 0 ) || ( height <= 0 ))
		return false;

	SDL_GPUTextureFormat target_format = m_swapchain_format;
	if ( target_format == SDL_GPU_TEXTUREFORMAT_INVALID )
		target_format = SDL_GetGPUSwapchainTextureFormat( m_device, m_window );

	switch ( target_format )
	{
	case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
	case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB:
	case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
	case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB:
		break;

	default:
		FeLog() << "Could not capture GPU frame: unsupported GPU texture format." << std::endl;
		return false;
	}

	if ( !ensure_depth_target( width, height ) )
		return false;

	if (( m_swapchain_format != target_format ) || !m_blend_pipelines[ 0 ][ FeBlend::Alpha ] )
	{
		release_custom_shaders();
		release_image_pipeline();
		m_swapchain_format = target_format;
		m_pipeline_attempted = false;
	}

	if ( !m_pipeline_attempted )
	{
		initialize_image_pipeline( target_format );
		m_pipeline_attempted = true;
	}

	if ( !m_blend_pipelines[ 0 ][ FeBlend::Alpha ] )
		return false;

	build_prepared_images();

	SDL_GPUTextureCreateInfo texture_info = {};
	texture_info.type = SDL_GPU_TEXTURETYPE_2D;
	texture_info.format = target_format;
	texture_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
	texture_info.width = static_cast<Uint32>( width );
	texture_info.height = static_cast<Uint32>( height );
	texture_info.layer_count_or_depth = 1;
	texture_info.num_levels = 1;
	texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;

	SDL_GPUTexture *color_texture = SDL_CreateGPUTexture( m_device, &texture_info );
	if ( !color_texture )
		return false;

	SDL_GPUTransferBufferCreateInfo transfer_info = {};
	transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
	transfer_info.size = static_cast<Uint32>( width * height * 4 );

	SDL_GPUTransferBuffer *transfer_buffer = SDL_CreateGPUTransferBuffer( m_device, &transfer_info );
	if ( !transfer_buffer )
	{
		SDL_ReleaseGPUTexture( m_device, color_texture );
		return false;
	}

	SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer( m_device );
	if ( !command_buffer )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		SDL_ReleaseGPUTexture( m_device, color_texture );
		return false;
	}

	if ( !render_surface_frames( command_buffer ) )
	{
		SDL_CancelGPUCommandBuffer( command_buffer );
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		SDL_ReleaseGPUTexture( m_device, color_texture );
		return false;
	}

	SDL_GPUColorTargetInfo color_target = {};
	color_target.texture = color_texture;
	color_target.mip_level = 0;
	color_target.layer_or_depth_plane = 0;
	color_target.clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 1.0f };
	color_target.load_op = SDL_GPU_LOADOP_CLEAR;
	color_target.store_op = SDL_GPU_STOREOP_STORE;

	SDL_GPUDepthStencilTargetInfo depth_target = {};
	if ( m_depth_texture )
	{
		depth_target.texture = m_depth_texture;
		depth_target.clear_depth = 1.0f;
		depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
		depth_target.store_op = SDL_GPU_STOREOP_STORE;
		depth_target.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
		depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
		depth_target.cycle = false;
		depth_target.clear_stencil = 0;
	}

	SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(
		command_buffer,
		&color_target,
		1,
		m_depth_texture ? &depth_target : nullptr );
	if ( !render_pass )
	{
		SDL_CancelGPUCommandBuffer( command_buffer );
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		SDL_ReleaseGPUTexture( m_device, color_texture );
		return false;
	}

	bool drew_anything = false;
	if ( !m_frame.images.empty() )
	{
		if ( !render_geometry_batch(
				render_pass,
				command_buffer,
				m_frame.camera,
				width,
				height,
				m_frame.images,
				compute_geometry_signature( m_frame.images ),
				true,
				&m_vertex_buffer,
				&m_vertex_buffer_size,
				&m_vertex_buffer_signature,
				drew_anything ) )
		{
			SDL_EndGPURenderPass( render_pass );
			SDL_CancelGPUCommandBuffer( command_buffer );
			SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
			SDL_ReleaseGPUTexture( m_device, color_texture );
			return false;
		}
	}

	SDL_EndGPURenderPass( render_pass );

	SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass( command_buffer );
	if ( !copy_pass )
	{
		SDL_CancelGPUCommandBuffer( command_buffer );
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		SDL_ReleaseGPUTexture( m_device, color_texture );
		return false;
	}

	SDL_GPUTextureRegion source = {};
	source.texture = color_texture;
	source.mip_level = 0;
	source.layer = 0;
	source.x = 0;
	source.y = 0;
	source.z = 0;
	source.w = static_cast<Uint32>( width );
	source.h = static_cast<Uint32>( height );
	source.d = 1;

	SDL_GPUTextureTransferInfo destination = {};
	destination.transfer_buffer = transfer_buffer;
	destination.offset = 0;
	destination.pixels_per_row = static_cast<Uint32>( width );
	destination.rows_per_layer = static_cast<Uint32>( height );

	SDL_DownloadFromGPUTexture( copy_pass, &source, &destination );
	SDL_EndGPUCopyPass( copy_pass );

	if ( !SDL_SubmitGPUCommandBuffer( command_buffer ) )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		SDL_ReleaseGPUTexture( m_device, color_texture );
		return false;
	}

	if ( !SDL_WaitForGPUIdle( m_device ) )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		SDL_ReleaseGPUTexture( m_device, color_texture );
		return false;
	}

	const std::uint8_t *mapped = static_cast<const std::uint8_t *>( SDL_MapGPUTransferBuffer( m_device, transfer_buffer, false ) );
	if ( !mapped )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		SDL_ReleaseGPUTexture( m_device, color_texture );
		return false;
	}

	pixels.resize( static_cast<std::size_t>( width ) * static_cast<std::size_t>( height ) * 4 );
	if ( target_format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM ||
		 target_format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB )
	{
		for ( int i = 0; i < ( width * height ); ++i )
		{
			const int src = i * 4;
			pixels[ src + 0 ] = mapped[ src + 2 ];
			pixels[ src + 1 ] = mapped[ src + 1 ];
			pixels[ src + 2 ] = mapped[ src + 0 ];
			pixels[ src + 3 ] = mapped[ src + 3 ];
		}
	}
	else
	{
		std::memcpy( pixels.data(), mapped, pixels.size() );
	}

	SDL_UnmapGPUTransferBuffer( m_device, transfer_buffer );
	SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
	SDL_ReleaseGPUTexture( m_device, color_texture );

	return true;
}

bool FeSdl3GpuContext::capture_surface_rgba( const void *surface_texture_id, std::vector<std::uint8_t> &pixels, int &width, int &height )
{
	if ( !is_available() || !surface_texture_id )
		return false;

	auto it = m_surfaces.find( surface_texture_id );
	if ( it == m_surfaces.end() || !it->second.color_texture || it->second.width <= 0 || it->second.height <= 0 )
		return false;

	const SurfaceEntry &entry = it->second;
	width = entry.width;
	height = entry.height;

	SDL_GPUTransferBufferCreateInfo transfer_info = {};
	transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
	transfer_info.size = static_cast<Uint32>( width * height * 4 );

	SDL_GPUTransferBuffer *transfer_buffer = SDL_CreateGPUTransferBuffer( m_device, &transfer_info );
	if ( !transfer_buffer )
		return false;

	SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer( m_device );
	if ( !command_buffer )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		return false;
	}

	SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass( command_buffer );
	if ( !copy_pass )
	{
		SDL_CancelGPUCommandBuffer( command_buffer );
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		return false;
	}

	SDL_GPUTextureRegion source = {};
	source.texture = entry.color_texture;
	source.mip_level = 0;
	source.layer = 0;
	source.x = 0;
	source.y = 0;
	source.z = 0;
	source.w = static_cast<Uint32>( width );
	source.h = static_cast<Uint32>( height );
	source.d = 1;

	SDL_GPUTextureTransferInfo destination = {};
	destination.transfer_buffer = transfer_buffer;
	destination.offset = 0;
	destination.pixels_per_row = static_cast<Uint32>( width );
	destination.rows_per_layer = static_cast<Uint32>( height );

	SDL_DownloadFromGPUTexture( copy_pass, &source, &destination );
	SDL_EndGPUCopyPass( copy_pass );

	if ( !SDL_SubmitGPUCommandBuffer( command_buffer ) )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		return false;
	}

	if ( !SDL_WaitForGPUIdle( m_device ) )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		return false;
	}

	const std::uint8_t *mapped = static_cast<const std::uint8_t *>( SDL_MapGPUTransferBuffer( m_device, transfer_buffer, false ) );
	if ( !mapped )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		return false;
	}

	const std::size_t pixel_count =
		static_cast<std::size_t>( width ) *
		static_cast<std::size_t>( height ) * 4;
	pixels.assign( mapped, mapped + pixel_count );

	SDL_UnmapGPUTransferBuffer( m_device, transfer_buffer );
	SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
	return true;
}

bool FeSdl3GpuContext::save_screenshot( const std::string &filename )
{
	int width = 0;
	int height = 0;
	std::vector<std::uint8_t> pixels;
	if ( !capture_frame_rgba( pixels, width, height ) )
		return false;

	return save_rgba_png( filename, width, height, pixels.data() );
}

namespace
{
	int clamp_blend_mode( int blend_mode )
	{
		if ( blend_mode < FeBlend::Alpha || blend_mode > FeBlend::None )
			return FeBlend::Alpha;
		return blend_mode;
	}

	std::string join_path( const std::string &base, const std::string &suffix )
	{
		if ( base.empty() )
			return suffix;

		const char last = base[ base.size() - 1 ];
		if ( last == '/' || last == '\\' )
			return base + suffix;

		return base + "/" + suffix;
	}

	std::string get_base_path()
	{
		return get_program_path();
	}

	std::string get_shader_cache_base_path()
	{
#if !defined( SFML_SYSTEM_WINDOWS )
		return absolute_path( clean_path( std::string( FE_DEFAULT_CFG_PATH ) + FE_CACHE_SUBDIR ) );
#else
		return join_path( get_base_path(), FE_CACHE_SUBDIR );
#endif
	}

	struct ShaderBlob
	{
		SDL_GPUShaderFormat format;
		std::vector<Uint8> code;
		const char *entrypoint;
	};

	struct ShaderCompileCacheEntry
	{
		bool compile_failed;
		ShaderBlob blob;

		ShaderCompileCacheEntry()
			: compile_failed( false )
		{
		}
	};

	struct FeSdl3GpuDrawUniforms
	{
		float projection[16];
		float viewport_width;
		float viewport_height;
		float plane_distance;
		float reserved;
	};

	struct FeSdl3GpuCustomVertexUniforms
	{
		float projection[16];
		float viewport_width;
		float viewport_height;
		float plane_distance;
		float reserved;
		float values[FE_MAX_CUSTOM_SHADER_UNIFORMS * 4];
	};

	SDL_GPUShaderFormat get_default_shader_formats()
	{
		SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_SPIRV;

#if defined( SFML_SYSTEM_WINDOWS )
		formats |= SDL_GPU_SHADERFORMAT_DXIL;
#elif defined( SFML_SYSTEM_MACOS )
		formats |= SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB;
#endif

		return formats;
	}

	const char *get_default_driver_name( const char *driver_name )
	{
		if ( driver_name && driver_name[0] )
			return driver_name;

#if defined( SFML_SYSTEM_WINDOWS )
		return "vulkan";
#else
		return nullptr;
#endif
	}

	bool file_exists( const std::string &path )
	{
		std::ifstream stream( path.c_str(), std::ios::binary );
		return stream.good();
	}

	bool read_binary_file( const std::string &path, std::vector<Uint8> &code )
	{
		std::ifstream stream( path.c_str(), std::ios::binary );
		if ( !stream )
			return false;

		stream.seekg( 0, std::ios::end );
		const std::streamoff size = stream.tellg();
		stream.seekg( 0, std::ios::beg );

		if ( size <= 0 )
			return false;

		code.resize( static_cast<std::size_t>( size ) );
		stream.read( reinterpret_cast<char *>( code.data() ), size );
		return stream.good();
	}

	std::unordered_map<std::string, ShaderCompileCacheEntry> &get_shader_compile_cache()
	{
		static std::unordered_map<std::string, ShaderCompileCacheEntry> cache;
		return cache;
	}

	bool compile_shader_blob(
		const std::string &source_id,
		const std::string &translated_source,
		bool vertex_stage,
		ShaderBlob &blob )
	{
		const std::string cache_key =
			std::string( vertex_stage ? "vert|" : "frag|" ) +
			std::to_string( std::hash<std::string>{}( translated_source ) );
		auto &cache = get_shader_compile_cache();
		auto cache_it = cache.find( cache_key );
		if ( cache_it != cache.end() )
		{
			if ( cache_it->second.compile_failed )
				return false;

			blob = cache_it->second.blob;
			return !blob.code.empty();
		}

		std::string cache_root = get_shader_cache_base_path();
		confirm_directory( cache_root, "" );
		confirm_directory( cache_root, "shaders/" );
		cache_root = join_path( cache_root, "shaders/" );
		confirm_directory( cache_root, "sdl3/" );
		cache_root = join_path( cache_root, "sdl3/" );

		const std::string cache_name =
			sanitize_filename( source_id ) + "-" + std::to_string( std::hash<std::string>{}( translated_source ) );
		const std::string stage_name = vertex_stage ? "vert" : "frag";
		const std::string glsl_path = join_path( cache_root, cache_name + "." + stage_name + ".glsl" );
		const std::string spirv_path = join_path( cache_root, cache_name + "." + stage_name + ".spv" );

		if ( !file_exists( spirv_path ) )
		{
			if ( !write_file_content( glsl_path, translated_source ) )
			{
				FeLog() << "shader_blob: failed to write translated shader " << glsl_path << std::endl;
				cache[ cache_key ].compile_failed = true;
				return false;
			}

			const char *compiler = SDL_getenv( "FE_SDL3_GPU_GLSLANG" );
			const std::string compiler_name =
				( compiler && compiler[ 0 ] ) ? compiler : "glslangValidator";
			const std::string args =
				"-V -S " + stage_name + " -o \"" + spirv_path + "\" \"" + glsl_path + "\"";
			ShaderCompileLogCapture capture = {};

			FeLog() << "shader_blob: compiling " << source_id << std::endl;
			if ( !run_program( compiler_name, args, cache_root, shader_compile_output_callback, &capture, true, nullptr ) )
			{
				FeLog() << "shader_blob: glslang compile failed for " << source_id << std::endl;
				FeLog() << "FeSdl3GpuContext: "
					<< ( vertex_stage ? "vertex" : "fragment" )
					<< " shader compile failed: "
					<< source_id
					<< std::endl;
				if ( !capture.stream.str().empty() )
					FeLog() << capture.stream.str();
				cache[ cache_key ].compile_failed = true;
				return false;
			}
		}

		if ( !read_binary_file( spirv_path, blob.code ) || blob.code.empty() )
		{
			cache[ cache_key ].compile_failed = true;
			return false;
		}

		blob.format = SDL_GPU_SHADERFORMAT_SPIRV;
		blob.entrypoint = "main";

		ShaderCompileCacheEntry &entry = cache[ cache_key ];
		entry.compile_failed = false;
		entry.blob = blob;
		return true;
	}
}

bool FeSdl3GpuContext::execute_frame( const std::vector<FeRenderGeometry> *overlay_geometry )
{
	m_frame_stats.executed = false;
	m_frame_stats.acquired_swapchain = false;
	m_frame_stats.depth_ready = false;
	m_frame_stats.pipeline_ready = false;
	m_frame_stats.draw_ready = false;

	if ( !is_available() )
	{
		write_debug_log( "execute_frame: GPU context is not available" );
		m_failed_present_frames++;
		return false;
	}

	sync_textures( overlay_geometry );

	SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer( m_device );
	if ( !command_buffer )
	{
		write_debug_log( "execute_frame: SDL_AcquireGPUCommandBuffer failed" );
		m_failed_present_frames++;
		return false;
	}

	SDL_GPUTexture *swapchain_texture = nullptr;
	Uint32 swapchain_width = 0;
	Uint32 swapchain_height = 0;
	if ( !SDL_WaitAndAcquireGPUSwapchainTexture( command_buffer, m_window, &swapchain_texture, &swapchain_width, &swapchain_height ) )
	{
		write_debug_log( "execute_frame: SDL_WaitAndAcquireGPUSwapchainTexture failed" );
		m_failed_present_frames++;
		return false;
	}

	if ( !swapchain_texture )
	{
		write_debug_log( "execute_frame: swapchain texture was null" );
		m_failed_present_frames++;
		return false;
	}

	m_frame_stats.acquired_swapchain = true;
	m_frame_stats.last_viewport_width = static_cast<int>( swapchain_width );
	m_frame_stats.last_viewport_height = static_cast<int>( swapchain_height );
	const SDL_GPUTextureFormat swapchain_format = SDL_GetGPUSwapchainTextureFormat( m_device, m_window );
	m_frame_stats.depth_ready = ensure_depth_target( static_cast<int>( swapchain_width ), static_cast<int>( swapchain_height ) );

	if ( ( m_swapchain_format != swapchain_format ) || !m_blend_pipelines[0][FeBlend::Alpha] )
	{
		release_custom_shaders();
		release_image_pipeline();
		m_swapchain_format = swapchain_format;
		m_pipeline_attempted = false;
	}

	if ( !m_pipeline_attempted )
	{
		initialize_image_pipeline( swapchain_format );
		m_pipeline_attempted = true;
	}

	m_frame_stats.pipeline_ready = ( m_blend_pipelines[0][FeBlend::Alpha] != nullptr );
	if ( !m_frame_stats.pipeline_ready )
		write_debug_log( "execute_frame: image pipeline not ready" );

	if ( m_frame_stats.pipeline_ready && !render_surface_frames( command_buffer ) )
	{
		write_debug_log( "execute_frame: render_surface_frames failed" );
		m_failed_present_frames++;
		return false;
	}

	SDL_GPUColorTargetInfo color_target = {};
	color_target.texture = swapchain_texture;
	color_target.mip_level = 0;
	color_target.layer_or_depth_plane = 0;
	color_target.clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 1.0f };
	color_target.load_op = SDL_GPU_LOADOP_CLEAR;
	color_target.store_op = SDL_GPU_STOREOP_STORE;

	SDL_GPUDepthStencilTargetInfo depth_target = {};
	if ( m_depth_texture )
	{
		depth_target.texture = m_depth_texture;
		depth_target.clear_depth = 1.0f;
		depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
		depth_target.store_op = SDL_GPU_STOREOP_STORE;
		depth_target.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
		depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
		depth_target.cycle = false;
		depth_target.clear_stencil = 0;
	}

	SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(
		command_buffer,
		&color_target,
		1,
		m_depth_texture ? &depth_target : nullptr );
	if ( !render_pass )
	{
		write_debug_log( "execute_frame: SDL_BeginGPURenderPass failed" );
		m_failed_present_frames++;
		return false;
	}

	if ( m_blend_pipelines[0][FeBlend::Alpha] && !m_frame.images.empty() )
	{
		if ( !render_geometry_batch(
				render_pass,
				command_buffer,
				m_frame.camera,
				m_frame.viewport_width,
				m_frame.viewport_height,
				m_frame.images,
				compute_geometry_signature( m_frame.images ),
				true,
				&m_vertex_buffer,
				&m_vertex_buffer_size,
				&m_vertex_buffer_signature,
				m_frame_stats.draw_ready ) )
		{
			SDL_EndGPURenderPass( render_pass );
			write_debug_log( "execute_frame: render_geometry_batch failed" );
			m_failed_present_frames++;
			return false;
		}
	}

	if ( m_blend_pipelines[0][FeBlend::Alpha] && overlay_geometry && !overlay_geometry->empty() )
	{
		bool overlay_drew = false;
		if ( !render_geometry_batch(
				render_pass,
				command_buffer,
				m_frame.camera,
				m_frame.viewport_width,
				m_frame.viewport_height,
				*overlay_geometry,
				compute_geometry_signature( *overlay_geometry ),
				false,
				nullptr,
				nullptr,
				nullptr,
				overlay_drew ) )
		{
			SDL_EndGPURenderPass( render_pass );
			write_debug_log( "execute_frame: render_overlay_geometry failed" );
			m_failed_present_frames++;
			return false;
		}
		m_frame_stats.draw_ready = m_frame_stats.draw_ready || overlay_drew;
	}

	SDL_EndGPURenderPass( render_pass );

	if ( !SDL_SubmitGPUCommandBuffer( command_buffer ) )
	{
		write_debug_log( "execute_frame: SDL_SubmitGPUCommandBuffer failed" );
		m_failed_present_frames++;
		return false;
	}

	m_frame_stats.executed = true;
	m_frame_stats.submitted_frames++;
	if ( m_frame_stats.pipeline_ready && m_frame_stats.draw_ready )
	{
		m_failed_present_frames = 0;
		if ( !m_logged_successful_present )
		{
			std::ostringstream stream;
			stream
				<< "execute_frame: drew frame"
				<< " prepared_images=" << m_prepared_images.size()
				<< " vertex_count=" << m_vertex_stream.size()
				<< " viewport=" << swapchain_width << "x" << swapchain_height;
			write_debug_log( stream.str().c_str() );

			if ( !m_prepared_images.empty() && m_prepared_images[0].geometry )
			{
				const FeRenderGeometry &geometry = *m_prepared_images[0].geometry;
				std::ostringstream geo_stream;
				geo_stream
					<< "execute_frame: first_geometry"
					<< " textured=" << ( geometry.textured ? 1 : 0 )
					<< " texture_size=" << geometry.texture_width << "x" << geometry.texture_height
					<< " vertices=" << geometry.vertices.size();

				const std::size_t sample_count = std::min<std::size_t>( geometry.vertices.size(), 3 );
				for ( std::size_t i = 0; i < sample_count; ++i )
				{
					const FeRenderVertex &vertex = geometry.vertices[i];
					geo_stream
						<< " v" << i
						<< "=(" << vertex.x << "," << vertex.y << "," << vertex.z << ";"
						<< vertex.u << "," << vertex.v << ")";
				}

				write_debug_log( geo_stream.str().c_str() );
			}
			m_logged_successful_present = true;
		}
	}
	if ( !m_frame_stats.draw_ready )
	{
		std::ostringstream stream;
		stream
			<< "execute_frame: submitted without draws"
			<< " pipeline_ready=" << ( m_frame_stats.pipeline_ready ? 1 : 0 )
			<< " prepared_images=" << m_prepared_images.size()
			<< " vertex_count=" << m_vertex_stream.size();
		write_debug_log( stream.str().c_str() );
		if ( has_frame_content() )
			m_failed_present_frames++;
	}

	if ( m_failed_present_frames >= 3 )
	{
		m_present_disabled = true;
		write_debug_log( "execute_frame: disabling SDL present after repeated failed/no-draw frames" );
	}
	return true;
}

bool FeSdl3GpuContext::initialize( bool debug_mode, const char *driver_name )
{
	if ( m_device )
		return true;

	const char *debug_env = SDL_getenv( "FE_SDL3_GPU_DEBUG_LOG" );
	m_debug_logging_enabled = debug_mode || ( debug_env && debug_env[0] && debug_env[0] != '0' );

	if ( !ensure_video_subsystem() )
		return false;

	const char *preferred_driver = get_default_driver_name( driver_name );
	const SDL_GPUShaderFormat shader_formats = get_default_shader_formats();
	m_device = SDL_CreateGPUDevice( shader_formats, debug_mode, preferred_driver );
	if ( !m_device && preferred_driver )
	{
		const std::string message = std::string( "initialize: preferred GPU driver failed: " ) + preferred_driver;
		write_debug_log( message.c_str() );
		m_device = SDL_CreateGPUDevice( shader_formats, debug_mode, nullptr );
	}
	if ( !m_device )
		write_debug_log( "initialize: SDL_CreateGPUDevice failed" );
	else
	{
		std::ostringstream stream;
		stream << "initialize: SDL_CreateGPUDevice succeeded";
		if ( preferred_driver )
			stream << " preferred_driver=" << preferred_driver;
		stream << " shader_formats=" << static_cast<unsigned int>( SDL_GetGPUShaderFormats( m_device ) );
		write_debug_log( stream.str().c_str() );
	}
	return ( m_device != nullptr );
}

bool FeSdl3GpuContext::ensure_video_subsystem()
{
	if ( m_sdl_ready )
		return true;

	if ( !SDL_InitSubSystem( SDL_INIT_VIDEO ) )
	{
		FeLog() << "WARNING: SDL3 video initialization failed: " << SDL_GetError() << std::endl;
		return false;
	}

	m_sdl_ready = true;
	return true;
}

void FeSdl3GpuContext::shutdown()
{
	release_window();
	release_surfaces();
	clear_textures();
	release_white_texture();
	release_vertex_buffer();
	release_depth_target();
	release_custom_shaders();
	release_image_pipeline();

	if ( m_device )
	{
		SDL_DestroyGPUDevice( m_device );
		m_device = nullptr;
	}

	m_debug_logging_enabled = false;

	if ( m_sdl_ready )
	{
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
		m_sdl_ready = false;
	}
}

bool FeSdl3GpuContext::wrap_native_window( void *native_window_handle, int width, int height )
{
	release_window();

	if ( !native_window_handle )
		return false;

	if ( !initialize() )
		return false;

	SDL_PropertiesID props = SDL_CreateProperties();
	if ( !props )
		return false;

	SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width );
	SDL_SetNumberProperty( props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height );
#if defined( SFML_SYSTEM_WINDOWS )
	SDL_SetPointerProperty( props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, native_window_handle );
#else
	SDL_DestroyProperties( props );
	return false;
#endif

	m_window = SDL_CreateWindowWithProperties( props );
	SDL_DestroyProperties( props );

	if ( !m_window )
		return false;

	m_window_claimed = SDL_ClaimWindowForGPUDevice( m_device, m_window );
	if ( !m_window_claimed )
	{
		SDL_DestroyWindow( m_window );
		m_window = nullptr;
		write_debug_log( "wrap_native_window: SDL_ClaimWindowForGPUDevice failed" );
		return false;
	}

	write_debug_log( "wrap_native_window: success" );
	m_present_disabled = false;
	m_failed_present_frames = 0;
	return true;
}

bool FeSdl3GpuContext::claim_window( SDL_Window *window )
{
	release_window();

	if ( !window )
		return false;

	m_window = window;
	if ( !initialize() )
	{
		m_window = nullptr;
		return false;
	}

	m_window_claimed = SDL_ClaimWindowForGPUDevice( m_device, m_window );
	if ( !m_window_claimed )
	{
		m_window = nullptr;
		write_debug_log( "claim_window: SDL_ClaimWindowForGPUDevice failed" );
		return false;
	}

	write_debug_log( "claim_window: success" );
	m_present_disabled = false;
	m_failed_present_frames = 0;
	return true;
}

void FeSdl3GpuContext::release_window()
{
	if ( m_device && m_window && m_window_claimed )
	{
		SDL_ReleaseWindowFromGPUDevice( m_device, m_window );
		m_window_claimed = false;
	}

	if ( m_window )
	{
		SDL_DestroyWindow( m_window );
		m_window = nullptr;
	}

	m_present_disabled = false;
	m_failed_present_frames = 0;
	m_has_submitted_frame = false;
	m_logged_successful_present = false;
}

void FeSdl3GpuContext::release_texture( TextureEntry &entry )
{
	if ( entry.gpu_texture )
	{
		SDL_ReleaseGPUTexture( m_device, entry.gpu_texture );
		entry.gpu_texture = nullptr;
	}
}

void FeSdl3GpuContext::release_white_texture()
{
	if ( m_white_texture )
	{
		SDL_ReleaseGPUTexture( m_device, m_white_texture );
		m_white_texture = nullptr;
	}
}

bool FeSdl3GpuContext::ensure_white_texture()
{
	if ( !m_device )
		return false;

	if ( m_white_texture )
		return true;

	SDL_GPUTextureCreateInfo texture_info = {};
	texture_info.type = SDL_GPU_TEXTURETYPE_2D;
	texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
	texture_info.width = 1;
	texture_info.height = 1;
	texture_info.layer_count_or_depth = 1;
	texture_info.num_levels = 1;
	texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
	texture_info.props = 0;

	m_white_texture = SDL_CreateGPUTexture( m_device, &texture_info );
	if ( !m_white_texture )
		return false;

	const Uint32 white_pixel = 0xFFFFFFFFu;
	SDL_GPUTransferBufferCreateInfo transfer_info = {};
	transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transfer_info.size = sizeof( white_pixel );
	transfer_info.props = 0;

	SDL_GPUTransferBuffer *transfer_buffer = SDL_CreateGPUTransferBuffer( m_device, &transfer_info );
	if ( !transfer_buffer )
	{
		release_white_texture();
		return false;
	}

	void *mapped = SDL_MapGPUTransferBuffer( m_device, transfer_buffer, false );
	if ( !mapped )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		release_white_texture();
		return false;
	}

	std::memcpy( mapped, &white_pixel, sizeof( white_pixel ) );
	SDL_UnmapGPUTransferBuffer( m_device, transfer_buffer );

	SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer( m_device );
	if ( !command_buffer )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		release_white_texture();
		return false;
	}

	SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass( command_buffer );

	SDL_GPUTextureTransferInfo source = {};
	source.transfer_buffer = transfer_buffer;
	source.offset = 0;
	source.pixels_per_row = 1;
	source.rows_per_layer = 1;

	SDL_GPUTextureRegion destination = {};
	destination.texture = m_white_texture;
	destination.mip_level = 0;
	destination.layer = 0;
	destination.x = 0;
	destination.y = 0;
	destination.z = 0;
	destination.w = 1;
	destination.h = 1;
	destination.d = 1;

	SDL_UploadToGPUTexture( copy_pass, &source, &destination, false );
	SDL_EndGPUCopyPass( copy_pass );

	const bool submitted = SDL_SubmitGPUCommandBuffer( command_buffer );
	SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
	if ( !submitted )
	{
		release_white_texture();
		return false;
	}

	return true;
}

bool FeSdl3GpuContext::upload_texture( const void *texture_id, int texture_source_type, TextureEntry &entry )
{
	if ( !m_device || !texture_id )
		return false;

	std::vector<unsigned char> pixel_data;
	unsigned int source_width = 0;
	unsigned int source_height = 0;
	const FeTextureContainer *media_container = nullptr;
	bool direct_media_upload = false;
	bool black_media_placeholder = false;
	const FeBaseTextureContainer *direct_container = nullptr;
	const FeFont::TexturePageId *direct_font_page = nullptr;

	if ( texture_source_type == FeRenderTextureSourceContainer )
	{
		const FeBaseTextureContainer *source_container = static_cast<const FeBaseTextureContainer *>( texture_id );
		media_container = dynamic_cast<const FeTextureContainer *>( source_container );
		if ( media_container &&
			media_container->get_media() &&
			media_container->get_media()->get_video_frame_serial() != 0 &&
			media_container->get_media()->get_video_frame_dimensions( source_width, source_height ) )
		{
			direct_media_upload = true;
		}
		else if ( media_container &&
			media_container->get_media() &&
			media_container->get_media()->get_video_frame_dimensions( source_width, source_height ) )
		{
			black_media_placeholder = true;
		}
		else if ( source_container && source_container->copy_pixels_rgba_to( nullptr, 0, source_width, source_height ) )
		{
			direct_container = source_container;
		}
		else if ( !source_container || !source_container->copy_pixels_rgba( pixel_data, source_width, source_height ) )
			return false;
	}
	else if ( texture_source_type == FeRenderTextureSourceFontPage )
	{
		const FeFont::TexturePageId *font_page = static_cast<const FeFont::TexturePageId *>( texture_id );
		if ( !font_page || !font_page->font )
			return false;

		if ( !font_page->font->getTextureSize( font_page->character_size, source_width, source_height ) )
			return false;
		direct_font_page = font_page;
	}
	else if ( texture_source_type == FeRenderTextureSourceRawRgba )
	{
		const FeRenderRawTextureSource *raw = static_cast<const FeRenderRawTextureSource *>( texture_id );
		if ( !raw || !raw->pixels || raw->width == 0 || raw->height == 0 )
			return false;
		source_width = raw->width;
		source_height = raw->height;
		const std::size_t pixel_count =
			static_cast<std::size_t>( raw->width ) *
			static_cast<std::size_t>( raw->height ) * 4;
		pixel_data.assign( raw->pixels, raw->pixels + pixel_count );
	}
	else
		return false;

	release_texture( entry );

	SDL_GPUTextureCreateInfo texture_info = {};
	texture_info.type = SDL_GPU_TEXTURETYPE_2D;
	texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
	if ( entry.mipmapped )
		texture_info.usage |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
	texture_info.width = source_width;
	texture_info.height = source_height;
	texture_info.layer_count_or_depth = 1;
	texture_info.num_levels = entry.mipmapped ? get_mip_level_count( source_width, source_height ) : 1;
	texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
	texture_info.props = 0;

	entry.gpu_texture = SDL_CreateGPUTexture( m_device, &texture_info );
	if ( !entry.gpu_texture )
		return false;

	const Uint32 upload_size = source_width * source_height * 4;
	SDL_GPUTransferBufferCreateInfo transfer_info = {};
	transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transfer_info.size = upload_size;
	transfer_info.props = 0;

	SDL_GPUTransferBuffer *transfer_buffer = SDL_CreateGPUTransferBuffer( m_device, &transfer_info );
	if ( !transfer_buffer )
	{
		release_texture( entry );
		return false;
	}

	void *mapped = SDL_MapGPUTransferBuffer( m_device, transfer_buffer, false );
	if ( !mapped )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		release_texture( entry );
		return false;
	}

	if ( direct_media_upload )
	{
		unsigned int copied_width = 0;
		unsigned int copied_height = 0;
		if ( !media_container ||
			!media_container->get_media() ||
			!media_container->get_media()->copy_video_frame_rgba_to( mapped, upload_size, copied_width, copied_height ) ||
			( copied_width != source_width ) ||
			( copied_height != source_height ) )
		{
			SDL_UnmapGPUTransferBuffer( m_device, transfer_buffer );
			SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
			release_texture( entry );
			return false;
		}
	}
	else if ( black_media_placeholder )
	{
		const std::uint32_t black_pixel = 0xFF000000u;
		const std::size_t pixel_count = upload_size / sizeof( black_pixel );
		std::uint32_t *dst = static_cast<std::uint32_t *>( mapped );
		for ( std::size_t i = 0; i < pixel_count; ++i )
			dst[i] = black_pixel;
	}
	else if ( direct_container )
	{
		unsigned int copied_width = 0;
		unsigned int copied_height = 0;
		if ( !direct_container->copy_pixels_rgba_to( mapped, upload_size, copied_width, copied_height ) ||
			( copied_width != source_width ) ||
			( copied_height != source_height ) )
		{
			SDL_UnmapGPUTransferBuffer( m_device, transfer_buffer );
			SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
			release_texture( entry );
			return false;
		}
	}
	else if ( direct_font_page )
	{
		unsigned int copied_width = 0;
		unsigned int copied_height = 0;
		if ( !direct_font_page->font->copyTexturePixelsTo( direct_font_page->character_size, mapped, upload_size, copied_width, copied_height ) ||
			( copied_width != source_width ) ||
			( copied_height != source_height ) )
		{
			SDL_UnmapGPUTransferBuffer( m_device, transfer_buffer );
			SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
			release_texture( entry );
			return false;
		}
	}
	else
		std::memcpy( mapped, pixel_data.data(), upload_size );
	SDL_UnmapGPUTransferBuffer( m_device, transfer_buffer );

	SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer( m_device );
	if ( !command_buffer )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		release_texture( entry );
		return false;
	}

	SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass( command_buffer );

	SDL_GPUTextureTransferInfo source = {};
	source.transfer_buffer = transfer_buffer;
	source.offset = 0;
	source.pixels_per_row = source_width;
	source.rows_per_layer = source_height;

	SDL_GPUTextureRegion destination = {};
	destination.texture = entry.gpu_texture;
	destination.mip_level = 0;
	destination.layer = 0;
	destination.x = 0;
	destination.y = 0;
	destination.z = 0;
	destination.w = source_width;
	destination.h = source_height;
	destination.d = 1;

	SDL_UploadToGPUTexture( copy_pass, &source, &destination, false );
	SDL_EndGPUCopyPass( copy_pass );

	const bool submitted = SDL_SubmitGPUCommandBuffer( command_buffer );
	SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );

	if ( !submitted )
	{
		release_texture( entry );
		return false;
	}

	if ( entry.mipmapped && texture_info.num_levels > 1 )
	{
		SDL_GPUCommandBuffer *mipmap_command_buffer = SDL_AcquireGPUCommandBuffer( m_device );
		if ( !mipmap_command_buffer )
			return false;

		SDL_GenerateMipmapsForGPUTexture( mipmap_command_buffer, entry.gpu_texture );
		if ( !SDL_SubmitGPUCommandBuffer( mipmap_command_buffer ) )
			return false;
	}

	return true;
}

void FeSdl3GpuContext::release_vertex_buffer()
{
	if ( m_vertex_buffer )
	{
		SDL_ReleaseGPUBuffer( m_device, m_vertex_buffer );
		m_vertex_buffer = nullptr;
		m_vertex_buffer_size = 0;
	}

	m_vertex_buffer_signature = 0;
}

void FeSdl3GpuContext::release_image_pipeline()
{
	release_builtin_shaders();

	if ( m_alpha_prepass_pipeline )
	{
		SDL_ReleaseGPUGraphicsPipeline( m_device, m_alpha_prepass_pipeline );
		m_alpha_prepass_pipeline = nullptr;
	}

	for ( int i = 0; i <= FeBlend::None; ++i )
	{
		for ( int z = 0; z < 3; ++z )
		{
			if ( m_blend_pipelines[z][i] )
			{
				SDL_ReleaseGPUGraphicsPipeline( m_device, m_blend_pipelines[z][i] );
				m_blend_pipelines[z][i] = nullptr;
			}
		}
	}

	if ( m_linear_sampler )
	{
		SDL_ReleaseGPUSampler( m_device, m_linear_sampler );
		m_linear_sampler = nullptr;
	}

	if ( m_linear_repeat_sampler )
	{
		SDL_ReleaseGPUSampler( m_device, m_linear_repeat_sampler );
		m_linear_repeat_sampler = nullptr;
	}

	if ( m_nearest_sampler )
	{
		SDL_ReleaseGPUSampler( m_device, m_nearest_sampler );
		m_nearest_sampler = nullptr;
	}

	if ( m_nearest_repeat_sampler )
	{
		SDL_ReleaseGPUSampler( m_device, m_nearest_repeat_sampler );
		m_nearest_repeat_sampler = nullptr;
	}

	for ( int i = 0; i <= FeBlend::None; ++i )
	{
		if ( m_fragment_shaders[i] )
		{
			SDL_ReleaseGPUShader( m_device, m_fragment_shaders[i] );
			m_fragment_shaders[i] = nullptr;
		}
	}

	if ( m_vertex_shader )
	{
		SDL_ReleaseGPUShader( m_device, m_vertex_shader );
		m_vertex_shader = nullptr;
	}

	if ( m_alpha_prepass_shader )
	{
		SDL_ReleaseGPUShader( m_device, m_alpha_prepass_shader );
		m_alpha_prepass_shader = nullptr;
	}
}

void FeSdl3GpuContext::release_builtin_shader( BuiltinShaderEntry &entry )
{
	for ( int i = 0; i <= FeBlend::None; ++i )
	{
		for ( int z = 0; z < 3; ++z )
		{
			if ( entry.blend_pipelines[z][i] )
			{
				SDL_ReleaseGPUGraphicsPipeline( m_device, entry.blend_pipelines[z][i] );
				entry.blend_pipelines[z][i] = nullptr;
			}
		}
	}

	if ( entry.fragment_shader )
	{
		SDL_ReleaseGPUShader( m_device, entry.fragment_shader );
		entry.fragment_shader = nullptr;
	}

	entry.source_id.clear();
}

void FeSdl3GpuContext::release_builtin_shaders()
{
	for ( auto &entry : m_builtin_shaders )
		release_builtin_shader( entry.second );

	m_builtin_shaders.clear();
}

void FeSdl3GpuContext::release_custom_shader( CustomShaderEntry &entry )
{
	for ( int i = 0; i <= FeBlend::None; ++i )
	{
		for ( int z = 0; z < 3; ++z )
		{
			if ( entry.blend_pipelines[ z ][ i ] )
			{
				SDL_ReleaseGPUGraphicsPipeline( m_device, entry.blend_pipelines[ z ][ i ] );
				entry.blend_pipelines[ z ][ i ] = nullptr;
			}
		}
	}

	if ( entry.vertex_shader )
	{
		SDL_ReleaseGPUShader( m_device, entry.vertex_shader );
		entry.vertex_shader = nullptr;
	}

	if ( entry.fragment_shader )
	{
		SDL_ReleaseGPUShader( m_device, entry.fragment_shader );
		entry.fragment_shader = nullptr;
	}

	entry.vertex_uniforms.clear();
	entry.fragment_uniforms.clear();
	entry.fragment_samplers.clear();
	entry.uses_current_texture = false;
	entry.has_custom_vertex = false;
	entry.fast_current_texture_only = false;
	entry.compile_failed = false;
	entry.source_stamp = 0;
	entry.vertex_source_stamp = 0;
}

void FeSdl3GpuContext::release_custom_shaders()
{
	for ( auto &entry : m_custom_shaders )
		release_custom_shader( entry.second );

	m_custom_shaders.clear();
	m_custom_shader_sources.clear();
}

void FeSdl3GpuContext::release_depth_target()
{
	if ( m_depth_texture )
	{
		SDL_ReleaseGPUTexture( m_device, m_depth_texture );
		m_depth_texture = nullptr;
	}

	m_depth_width = 0;
	m_depth_height = 0;
	m_depth_format = SDL_GPU_TEXTUREFORMAT_INVALID;
}

bool FeSdl3GpuContext::ensure_depth_target( int width, int height )
{
	if ( !m_device || width <= 0 || height <= 0 )
		return false;

	if ( m_depth_texture && m_depth_width == width && m_depth_height == height )
		return true;

	release_depth_target();

	SDL_GPUTextureCreateInfo texture_info = {};
	texture_info.type = SDL_GPU_TEXTURETYPE_2D;
	texture_info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
	texture_info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
	texture_info.width = static_cast<Uint32>( width );
	texture_info.height = static_cast<Uint32>( height );
	texture_info.layer_count_or_depth = 1;
	texture_info.num_levels = 1;
	texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
	texture_info.props = 0;

	m_depth_texture = SDL_CreateGPUTexture( m_device, &texture_info );
	if ( !m_depth_texture )
		return false;

	m_depth_width = width;
	m_depth_height = height;
	m_depth_format = texture_info.format;
	return true;
}

void FeSdl3GpuContext::release_surface_target( SurfaceEntry &entry )
{
	if ( entry.color_texture )
	{
		SDL_ReleaseGPUTexture( m_device, entry.color_texture );
		entry.color_texture = nullptr;
	}

	if ( entry.depth_texture )
	{
		SDL_ReleaseGPUTexture( m_device, entry.depth_texture );
		entry.depth_texture = nullptr;
	}

	entry.width = 0;
	entry.height = 0;
	entry.rendered_once = false;
	entry.vertex_signature = 0;
	entry.last_signature = 0;

	if ( entry.vertex_buffer )
	{
		SDL_ReleaseGPUBuffer( m_device, entry.vertex_buffer );
		entry.vertex_buffer = nullptr;
		entry.vertex_buffer_size = 0;
	}
}

void FeSdl3GpuContext::release_surfaces()
{
	for ( auto &entry : m_surfaces )
		release_surface_target( entry.second );

	m_surfaces.clear();
}

FeSdl3GpuContext::CustomShaderEntry *FeSdl3GpuContext::get_custom_shader_entry( const FeRenderGeometry &image )
{
	if ( !m_device || !m_vertex_shader || !image.shader || !image.custom_shader )
		return nullptr;

	const FeShader::Type shader_type = image.shader->get_type();
	const std::string &fragment_source_path = image.shader->get_fragment_source_path();
	const std::string &vertex_source_path = image.shader->get_vertex_source_path();
	const std::string &fragment_source_code = image.shader->get_fragment_source_code();
	const std::string &vertex_source_code = image.shader->get_vertex_source_code();
	if ( ( fragment_source_path.empty() && fragment_source_code.empty() ) ||
		( shader_type != FeShader::Fragment && shader_type != FeShader::VertexAndFragment ) )
		return nullptr;

	const std::string source_identity_key =
		std::string( "f:" ) +
		( fragment_source_path.empty()
			? std::string( "mem:" ) + std::to_string( std::hash<std::string>{}( fragment_source_code ) )
			: fragment_source_path ) +
		"|v:" +
		( vertex_source_path.empty()
			? ( vertex_source_code.empty()
				? std::string()
				: std::string( "mem:" ) + std::to_string( std::hash<std::string>{}( vertex_source_code ) ) )
			: vertex_source_path );

	auto source_cache_it = m_custom_shader_sources.find( source_identity_key );
	if ( source_cache_it != m_custom_shader_sources.end() )
	{
		auto cache_it = m_custom_shaders.find( source_cache_it->second );
		if ( cache_it != m_custom_shaders.end() && cache_it->second.fragment_shader )
			return &cache_it->second;

		m_custom_shader_sources.erase( source_cache_it );
	}

	const std::string absolute_fragment_source =
		fragment_source_path.empty()
			? std::string( "memory-fragment|" ) + std::to_string( std::hash<std::string>{}( fragment_source_code ) )
			: absolute_path( fragment_source_path );
	const std::string absolute_vertex_source =
		vertex_source_path.empty()
			? ( vertex_source_code.empty() ? std::string() : std::string( "memory-vertex|" ) + std::to_string( std::hash<std::string>{}( vertex_source_code ) ) )
			: absolute_path( vertex_source_path );
	const std::string cache_key =
		absolute_vertex_source.empty()
			? absolute_fragment_source
			: absolute_vertex_source + "|" + absolute_fragment_source;
	const unsigned long long fragment_source_stamp =
		fragment_source_path.empty()
			? static_cast<unsigned long long>( std::hash<std::string>{}( fragment_source_code ) )
			: static_cast<unsigned long long>( get_file_mtime( fragment_source_path ) );
	const unsigned long long vertex_source_stamp =
		vertex_source_path.empty()
			? ( vertex_source_code.empty() ? 0ULL : static_cast<unsigned long long>( std::hash<std::string>{}( vertex_source_code ) ) )
			: static_cast<unsigned long long>( get_file_mtime( vertex_source_path ) );

	CustomShaderEntry &entry = m_custom_shaders[ cache_key ];
	if ( entry.source_stamp == fragment_source_stamp &&
		entry.vertex_source_stamp == vertex_source_stamp )
	{
		if ( entry.fragment_shader )
			return &entry;
		if ( entry.compile_failed )
			return nullptr;
	}

	release_custom_shader( entry );
	entry.source_path = cache_key;
	if ( !create_custom_shader_entry( image, entry ) )
	{
		entry.compile_failed = true;
		entry.source_stamp = fragment_source_stamp;
		entry.vertex_source_stamp = vertex_source_stamp;
		m_custom_shader_sources[ source_identity_key ] = cache_key;
		return nullptr;
	}

	entry.compile_failed = false;
	entry.source_stamp = fragment_source_stamp;
	entry.vertex_source_stamp = vertex_source_stamp;
	m_custom_shader_sources[ source_identity_key ] = cache_key;
	return &entry;
}

FeSdl3GpuContext::CustomShaderEntry *FeSdl3GpuContext::get_builtin_blend_shader_entry( int blend_mode, const FeRenderGeometry &image )
{
	if ( !m_device || !m_vertex_shader || !image.textured )
		return nullptr;

	if ( blend_mode != FeBlend::Multiply )
		return nullptr;

	const char *raw_source = FeBlend::get_default_shader_source( blend_mode );
	if ( !raw_source || !raw_source[0] )
		return nullptr;

	const std::string cache_key = "__builtin_blend_" + std::to_string( blend_mode ) + "__";
	const unsigned long long source_stamp = 1ULL;

	CustomShaderEntry &entry = m_custom_shaders[ cache_key ];
	if ( entry.fragment_shader && entry.source_stamp == source_stamp )
		return &entry;

	release_custom_shader( entry );
	entry.source_path = cache_key;
	if ( !create_builtin_blend_shader_entry( blend_mode, image, entry ) )
	{
		m_custom_shaders.erase( cache_key );
		return nullptr;
	}

	entry.source_stamp = source_stamp;
	entry.vertex_source_stamp = 0ULL;
	return &entry;
}

FeSdl3GpuContext::BuiltinShaderEntry *FeSdl3GpuContext::get_fast_builtin_shader_entry( int blend_mode )
{
	if ( !m_device || !m_vertex_shader )
		return nullptr;

	if ( blend_mode != FeBlend::Screen &&
		blend_mode != FeBlend::Multiply &&
		blend_mode != FeBlend::Overlay &&
		blend_mode != FeBlend::Premultiplied )
		return nullptr;

	BuiltinShaderEntry &entry = m_builtin_shaders[ blend_mode ];
	if ( entry.fragment_shader )
		return &entry;

	release_builtin_shader( entry );
	if ( !create_fast_builtin_shader_entry( blend_mode, entry ) )
	{
		m_builtin_shaders.erase( blend_mode );
		return nullptr;
	}

	return &entry;
}

bool FeSdl3GpuContext::create_fast_builtin_shader_entry( int blend_mode, BuiltinShaderEntry &entry )
{
	const std::string source_code = build_fast_builtin_fragment_shader( blend_mode );
	if ( source_code.empty() )
		return false;

	entry.source_id = "__builtin_fast_blend_" + std::to_string( blend_mode ) + "__";

	ShaderBlob fragment_blob = {};
	if ( !compile_shader_blob( entry.source_id, source_code, false, fragment_blob ) )
		return false;

	SDL_GPUShaderCreateInfo fragment_info = {};
	fragment_info.code_size = fragment_blob.code.size();
	fragment_info.code = fragment_blob.code.data();
	fragment_info.entrypoint = fragment_blob.entrypoint;
	fragment_info.format = fragment_blob.format;
	fragment_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	fragment_info.num_uniform_buffers = 0;
	fragment_info.num_samplers = 1;

	entry.fragment_shader = SDL_CreateGPUShader( m_device, &fragment_info );
	if ( !entry.fragment_shader )
	{
		write_debug_log( ( std::string( "builtin_shader: SDL_CreateGPUShader failed for " ) + entry.source_id ).c_str() );
		return false;
	}

	SDL_GPUVertexBufferDescription vertex_buffer = {};
	vertex_buffer.slot = 0;
	vertex_buffer.pitch = sizeof( FeRenderVertex );
	vertex_buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	vertex_buffer.instance_step_rate = 0;

	SDL_GPUVertexAttribute attributes[3] = {};
	attributes[0].location = 0;
	attributes[0].buffer_slot = 0;
	attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	attributes[0].offset = offsetof( FeRenderVertex, x );
	attributes[1].location = 1;
	attributes[1].buffer_slot = 0;
	attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	attributes[1].offset = offsetof( FeRenderVertex, u );
	attributes[2].location = 2;
	attributes[2].buffer_slot = 0;
	attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
	attributes[2].offset = offsetof( FeRenderVertex, r );

	SDL_GPUColorTargetDescription color_target = {};
	color_target.format = m_swapchain_format;

	SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.vertex_shader = m_vertex_shader;
	pipeline_info.fragment_shader = entry.fragment_shader;
	pipeline_info.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer;
	pipeline_info.vertex_input_state.num_vertex_buffers = 1;
	pipeline_info.vertex_input_state.vertex_attributes = attributes;
	pipeline_info.vertex_input_state.num_vertex_attributes = 3;
	pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
	pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
	pipeline_info.rasterizer_state.enable_depth_clip = true;
	pipeline_info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
	pipeline_info.multisample_state.sample_mask = 0;
	pipeline_info.target_info.color_target_descriptions = &color_target;
	pipeline_info.target_info.num_color_targets = 1;

	for ( int z = 0; z < 3; ++z )
	{
		configure_pipeline_depth_state( pipeline_info, z );
		for ( int mode = FeBlend::Alpha; mode <= FeBlend::None; ++mode )
		{
			color_target.blend_state = make_gpu_blend_state( mode );
			entry.blend_pipelines[z][mode] = SDL_CreateGPUGraphicsPipeline( m_device, &pipeline_info );
			if ( !entry.blend_pipelines[z][mode] )
			{
				write_debug_log( ( std::string( "builtin_shader: SDL_CreateGPUGraphicsPipeline failed for " ) + entry.source_id ).c_str() );
				release_builtin_shader( entry );
				return false;
			}
		}
	}

	return true;
}

bool FeSdl3GpuContext::create_custom_shader_entry( const FeRenderGeometry &image, CustomShaderEntry &entry )
{
	std::string fragment_source_code;
	std::vector<CustomUniformBinding> fragment_uniforms;
	std::vector<CustomSamplerBinding> fragment_samplers;
	if ( !build_custom_fragment_shader( image, fragment_source_code, fragment_uniforms, fragment_samplers ) )
		return false;

	const std::string fragment_source_id =
		!image.shader->get_fragment_source_path().empty()
			? image.shader->get_fragment_source_path()
			: entry.source_path + "|fragment";
	ShaderBlob fragment_blob = {};
	if ( !compile_shader_blob( fragment_source_id, fragment_source_code, false, fragment_blob ) )
		return false;

	SDL_GPUShader *vertex_shader = m_vertex_shader;
	std::vector<CustomUniformBinding> vertex_uniforms;
	const bool has_custom_vertex =
		( image.shader->get_type() == FeShader::VertexAndFragment ) &&
		( !image.shader->get_vertex_source_path().empty() || !image.shader->get_vertex_source_code().empty() );
	if ( has_custom_vertex )
	{
		std::string vertex_source_code;
		if ( !build_custom_vertex_shader( image, vertex_source_code, vertex_uniforms ) )
			return false;

		const std::string vertex_source_id =
			!image.shader->get_vertex_source_path().empty()
				? image.shader->get_vertex_source_path()
				: entry.source_path + "|vertex";
		ShaderBlob vertex_blob = {};
		if ( !compile_shader_blob( vertex_source_id, vertex_source_code, true, vertex_blob ) )
			return false;

		SDL_GPUShaderCreateInfo vertex_info = {};
		vertex_info.code_size = vertex_blob.code.size();
		vertex_info.code = vertex_blob.code.data();
		vertex_info.entrypoint = vertex_blob.entrypoint;
		vertex_info.format = vertex_blob.format;
		vertex_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
		vertex_info.num_uniform_buffers = 1;

		entry.vertex_shader = SDL_CreateGPUShader( m_device, &vertex_info );
		if ( !entry.vertex_shader )
		{
			write_debug_log( ( std::string( "custom_shader: SDL_CreateGPUShader failed for vertex shader " ) + vertex_source_id ).c_str() );
			return false;
		}
		vertex_shader = entry.vertex_shader;
	}

	SDL_GPUShaderCreateInfo fragment_info = {};
	fragment_info.code_size = fragment_blob.code.size();
	fragment_info.code = fragment_blob.code.data();
	fragment_info.entrypoint = fragment_blob.entrypoint;
	fragment_info.format = fragment_blob.format;
	fragment_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	fragment_info.num_uniform_buffers = fragment_uniforms.empty() ? 0 : 1;
	fragment_info.num_samplers = static_cast<Uint32>( fragment_samplers.size() );

	entry.fragment_shader = SDL_CreateGPUShader( m_device, &fragment_info );
	if ( !entry.fragment_shader )
	{
		write_debug_log( ( std::string( "custom_shader: SDL_CreateGPUShader failed for " ) + entry.source_path ).c_str() );
		return false;
	}

	SDL_GPUVertexBufferDescription vertex_buffer = {};
	vertex_buffer.slot = 0;
	vertex_buffer.pitch = sizeof( FeRenderVertex );
	vertex_buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	vertex_buffer.instance_step_rate = 0;

	SDL_GPUVertexAttribute attributes[3] = {};
	attributes[0].location = 0;
	attributes[0].buffer_slot = 0;
	attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	attributes[0].offset = offsetof( FeRenderVertex, x );
	attributes[1].location = 1;
	attributes[1].buffer_slot = 0;
	attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	attributes[1].offset = offsetof( FeRenderVertex, u );
	attributes[2].location = 2;
	attributes[2].buffer_slot = 0;
	attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
	attributes[2].offset = offsetof( FeRenderVertex, r );

	SDL_GPUColorTargetDescription color_target = {};
	color_target.format = m_swapchain_format;

	SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.vertex_shader = vertex_shader;
	pipeline_info.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer;
	pipeline_info.vertex_input_state.num_vertex_buffers = 1;
	pipeline_info.vertex_input_state.vertex_attributes = attributes;
	pipeline_info.vertex_input_state.num_vertex_attributes = 3;
	pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
	pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
	pipeline_info.rasterizer_state.enable_depth_clip = true;
	pipeline_info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
	pipeline_info.multisample_state.sample_mask = 0;
	pipeline_info.target_info.color_target_descriptions = &color_target;
	pipeline_info.target_info.num_color_targets = 1;
	pipeline_info.fragment_shader = entry.fragment_shader;

	for ( int z = 0; z < 3; ++z )
	{
		configure_pipeline_depth_state( pipeline_info, z );
		for ( int blend_mode = FeBlend::Alpha; blend_mode <= FeBlend::None; ++blend_mode )
		{
			color_target.blend_state = make_gpu_blend_state( blend_mode );
			entry.blend_pipelines[ z ][ blend_mode ] = SDL_CreateGPUGraphicsPipeline( m_device, &pipeline_info );
			if ( !entry.blend_pipelines[ z ][ blend_mode ] )
			{
				write_debug_log( ( std::string( "custom_shader: SDL_CreateGPUGraphicsPipeline failed for " ) + entry.source_path ).c_str() );
				release_custom_shader( entry );
				return false;
			}
		}
	}

	entry.vertex_uniforms = vertex_uniforms;
	entry.fragment_uniforms = fragment_uniforms;
	entry.fragment_samplers = fragment_samplers;
	entry.uses_current_texture = false;
	for ( const CustomSamplerBinding &sampler : fragment_samplers )
		if ( sampler.name == "texture" )
			entry.uses_current_texture = true;
	entry.has_custom_vertex = has_custom_vertex;
	entry.fast_current_texture_only = false;
	write_debug_log( ( std::string( "custom_shader: ready " ) + entry.source_path ).c_str() );
	return true;
}

bool FeSdl3GpuContext::create_builtin_blend_shader_entry( int blend_mode, const FeRenderGeometry &image, CustomShaderEntry &entry )
{
	const char *raw_source = FeBlend::get_default_shader_source( blend_mode );
	if ( !raw_source || !raw_source[0] )
		return false;

	std::string fragment_source_code;
	std::vector<CustomUniformBinding> fragment_uniforms;
	std::vector<CustomSamplerBinding> fragment_samplers;
	if ( !build_custom_fragment_shader_from_source(
		image,
		entry.source_path,
		std::string( raw_source ),
		fragment_source_code,
		fragment_uniforms,
		fragment_samplers ) )
		return false;

	ShaderBlob fragment_blob = {};
	if ( !compile_shader_blob( entry.source_path, fragment_source_code, false, fragment_blob ) )
		return false;

	SDL_GPUShaderCreateInfo fragment_info = {};
	fragment_info.code_size = fragment_blob.code.size();
	fragment_info.code = fragment_blob.code.data();
	fragment_info.entrypoint = fragment_blob.entrypoint;
	fragment_info.format = fragment_blob.format;
	fragment_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	fragment_info.num_uniform_buffers = 1;
	fragment_info.num_samplers = static_cast<Uint32>( fragment_samplers.size() );

	entry.fragment_shader = SDL_CreateGPUShader( m_device, &fragment_info );
	if ( !entry.fragment_shader )
	{
		write_debug_log( ( std::string( "custom_shader: SDL_CreateGPUShader failed for " ) + entry.source_path ).c_str() );
		return false;
	}

	SDL_GPUVertexBufferDescription vertex_buffer = {};
	vertex_buffer.slot = 0;
	vertex_buffer.pitch = sizeof( FeRenderVertex );
	vertex_buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	vertex_buffer.instance_step_rate = 0;

	SDL_GPUVertexAttribute attributes[3] = {};
	attributes[0].location = 0;
	attributes[0].buffer_slot = 0;
	attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	attributes[0].offset = offsetof( FeRenderVertex, x );
	attributes[1].location = 1;
	attributes[1].buffer_slot = 0;
	attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	attributes[1].offset = offsetof( FeRenderVertex, u );
	attributes[2].location = 2;
	attributes[2].buffer_slot = 0;
	attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
	attributes[2].offset = offsetof( FeRenderVertex, r );

	SDL_GPUColorTargetDescription color_target = {};
	color_target.format = m_swapchain_format;

	SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.vertex_shader = m_vertex_shader;
	pipeline_info.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer;
	pipeline_info.vertex_input_state.num_vertex_buffers = 1;
	pipeline_info.vertex_input_state.vertex_attributes = attributes;
	pipeline_info.vertex_input_state.num_vertex_attributes = 3;
	pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
	pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
	pipeline_info.rasterizer_state.enable_depth_clip = true;
	pipeline_info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
	pipeline_info.multisample_state.sample_mask = 0;
	pipeline_info.target_info.color_target_descriptions = &color_target;
	pipeline_info.target_info.num_color_targets = 1;
	pipeline_info.fragment_shader = entry.fragment_shader;

	for ( int z = 0; z < 3; ++z )
	{
		configure_pipeline_depth_state( pipeline_info, z );
		for ( int mode = FeBlend::Alpha; mode <= FeBlend::None; ++mode )
		{
			color_target.blend_state = make_gpu_blend_state( mode );
			entry.blend_pipelines[ z ][ mode ] = SDL_CreateGPUGraphicsPipeline( m_device, &pipeline_info );
			if ( !entry.blend_pipelines[ z ][ mode ] )
			{
				write_debug_log( ( std::string( "custom_shader: SDL_CreateGPUGraphicsPipeline failed for " ) + entry.source_path ).c_str() );
				release_custom_shader( entry );
				return false;
			}
		}
	}

	entry.vertex_uniforms.clear();
	entry.fragment_uniforms = fragment_uniforms;
	entry.fragment_samplers = fragment_samplers;
	entry.uses_current_texture = true;
	entry.has_custom_vertex = false;
	write_debug_log( ( std::string( "custom_shader: ready " ) + entry.source_path ).c_str() );
	return true;
}

bool FeSdl3GpuContext::build_custom_fragment_shader_from_source(
	const FeRenderGeometry &image,
	const std::string &source_id,
	const std::string &raw_source,
	std::string &source_code,
	std::vector<CustomUniformBinding> &uniforms,
	std::vector<CustomSamplerBinding> &samplers )
{
	std::vector<ParsedCustomVarying> parsed_varyings;
	std::string varying_stripped_source;
	if ( !parse_custom_varyings( raw_source, parsed_varyings, varying_stripped_source ) )
		return false;

	std::vector<ParsedCustomUniform> parsed_uniforms;
	std::string stripped_source;
	if ( !parse_custom_uniforms( varying_stripped_source, parsed_uniforms, stripped_source ) )
		return false;

	uniforms.clear();
	samplers.clear();

	for ( const ParsedCustomUniform &uniform : parsed_uniforms )
	{
		if ( uniform.sampler )
		{
			CustomSamplerBinding binding = {};
			binding.name = uniform.name;
			binding.slot = static_cast<int>( samplers.size() );
			binding.current_texture = false;
			binding.image = nullptr;
			samplers.push_back( binding );
			continue;
		}

		if ( static_cast<int>( uniforms.size() ) >= FE_MAX_CUSTOM_SHADER_UNIFORMS )
		{
			write_debug_log( ( std::string( "custom_shader: too many numeric uniforms in " ) + source_id ).c_str() );
			return false;
		}

		CustomUniformBinding binding = {};
		binding.name = uniform.name;
		binding.slot = static_cast<int>( uniforms.size() );
		binding.components =
			( uniform.type == "float" ) ? 1 :
			( uniform.type == "vec2" ) ? 2 :
			( uniform.type == "vec3" ) ? 3 : 4;
		uniforms.push_back( binding );
	}

	replace_all( stripped_source, "texture2D(", "fe_texture2D(" );
	for ( int i = 0; i < 8; ++i )
	{
		const std::string from = "gl_TexCoord[" + std::to_string( i ) + "]";
		const std::string to = "frag_texcoord" + std::to_string( i );
		replace_all( stripped_source, from, to );
	}
	replace_all( stripped_source, "gl_Color", "frag_color" );
	replace_all( stripped_source, "gl_FragColor", "out_color" );

	std::ostringstream shader;
	shader << "#version 450\n\n";
	shader << "layout(location = 0) in vec4 frag_texcoord0;\n";
	shader << "layout(location = 1) in vec4 frag_color;\n";
	shader << "layout(location = 2) in vec4 frag_texcoord1;\n";
	shader << "layout(location = 3) in vec4 frag_texcoord2;\n";
	shader << "layout(location = 4) in vec4 frag_texcoord3;\n";
	shader << "layout(location = 5) in vec4 frag_texcoord4;\n";
	shader << "layout(location = 6) in vec4 frag_texcoord5;\n";
	shader << "layout(location = 7) in vec4 frag_texcoord6;\n";
	shader << "layout(location = 8) in vec4 frag_texcoord7;\n";
	for ( std::size_t i = 0; i < parsed_varyings.size(); ++i )
		shader << "layout(location = " << ( 9 + i ) << ") in " << parsed_varyings[ i ].type << " " << parsed_varyings[ i ].name << ";\n";
	shader << "layout(location = 0) out vec4 out_color;\n";
	for ( const CustomSamplerBinding &sampler : samplers )
	{
		if ( sampler.current_texture )
			shader << "layout(set = 2, binding = " << sampler.slot << ") uniform sampler2D image_texture" << sampler.slot << ";\n";
		else
			shader << "layout(set = 2, binding = " << sampler.slot << ") uniform sampler2D external_texture" << sampler.slot << ";\n";
	}
	shader << "layout(set = 3, binding = 0, std140) uniform CustomFragmentUniforms\n";
	shader << "{\n\tvec4 values[" << FE_MAX_CUSTOM_SHADER_UNIFORMS << "];\n} custom_ubo;\n\n";
	shader << "vec4 fe_texture2D( sampler2D sampler_value, vec2 uv )\n{\n\treturn texture( sampler_value, uv );\n}\n\n";
	shader << "vec4 fe_texture2D( sampler2D sampler_value, vec2 uv, float bias )\n{\n\treturn texture( sampler_value, uv, bias );\n}\n\n";

	for ( const CustomSamplerBinding &sampler : samplers )
	{
		if ( sampler.current_texture )
			shader << "#define " << sampler.name << " image_texture" << sampler.slot << "\n";
		else
			shader << "#define " << sampler.name << " external_texture" << sampler.slot << "\n";
	}

	for ( const CustomUniformBinding &uniform : uniforms )
		shader << build_custom_uniform_macro( uniform.name, uniform.slot, uniform.components ) << "\n";

	shader << "\n" << stripped_source;
	source_code = shader.str();
	return true;
}

bool FeSdl3GpuContext::build_custom_fragment_shader(
	const FeRenderGeometry &image,
	std::string &source_code,
	std::vector<CustomUniformBinding> &uniforms,
	std::vector<CustomSamplerBinding> &samplers )
{
	uniforms.clear();
	samplers.clear();

	if ( !image.shader )
		return false;

	std::string raw_source;
	const std::string &source_path = image.shader->get_fragment_source_path();
	if ( !source_path.empty() )
	{
		if ( !read_file_content( source_path, raw_source ) )
		{
			write_debug_log( ( std::string( "custom_shader: failed to read source " ) + source_path ).c_str() );
			return false;
		}
	}
	else
	{
		raw_source = image.shader->get_fragment_source_code();
		if ( raw_source.empty() )
			return false;
	}

	return build_custom_fragment_shader_from_source(
		image,
		source_path.empty() ? std::string( "memory-fragment" ) : source_path,
		raw_source,
		source_code,
		uniforms,
		samplers );
}

bool FeSdl3GpuContext::build_custom_vertex_shader(
	const FeRenderGeometry &image,
	std::string &source_code,
	std::vector<CustomUniformBinding> &uniforms )
{
	uniforms.clear();

	if ( !image.shader )
		return false;

	std::string raw_source;
	const std::string &source_path = image.shader->get_vertex_source_path();
	if ( !source_path.empty() )
	{
		if ( !read_file_content( source_path, raw_source ) )
		{
			write_debug_log( ( std::string( "custom_shader: failed to read source " ) + source_path ).c_str() );
			return false;
		}
	}
	else
	{
		raw_source = image.shader->get_vertex_source_code();
		if ( raw_source.empty() )
			return false;
	}

	std::vector<ParsedCustomUniform> parsed_uniforms;
	std::string varying_stripped_source;
	std::vector<ParsedCustomVarying> parsed_varyings;
	if ( !parse_custom_varyings( raw_source, parsed_varyings, varying_stripped_source ) )
		return false;

	std::string stripped_source;
	if ( !parse_custom_uniforms( varying_stripped_source, parsed_uniforms, stripped_source ) )
		return false;

	for ( const ParsedCustomUniform &uniform : parsed_uniforms )
	{
		if ( uniform.sampler )
		{
			write_debug_log( ( std::string( "custom_shader: samplers are not supported in custom vertex shaders " ) + source_path ).c_str() );
			return false;
		}

		if ( static_cast<int>( uniforms.size() ) >= FE_MAX_CUSTOM_SHADER_UNIFORMS )
		{
			write_debug_log( ( std::string( "custom_shader: too many numeric uniforms in " ) + source_path ).c_str() );
			return false;
		}

		CustomUniformBinding binding = {};
		binding.name = uniform.name;
		binding.slot = static_cast<int>( uniforms.size() );
		binding.components =
			( uniform.type == "float" ) ? 1 :
			( uniform.type == "vec2" ) ? 2 :
			( uniform.type == "vec3" ) ? 3 : 4;
		uniforms.push_back( binding );
	}

	replace_all( stripped_source, "gl_ModelViewProjectionMatrix", "custom_ubo.projection" );
	replace_all( stripped_source, "gl_Vertex", "fe_vertex" );
	replace_all( stripped_source, "gl_MultiTexCoord0", "vec4(in_texcoord, 0.0, 1.0)" );
	replace_all( stripped_source, "gl_Color", "in_color" );
	replace_all( stripped_source, "gl_FrontColor", "out_color" );
	for ( int i = 0; i < 8; ++i )
	{
		const std::string from = "gl_TexCoord[" + std::to_string( i ) + "]";
		const std::string to = "out_texcoord" + std::to_string( i );
		replace_all( stripped_source, from, to );
	}
	replace_all( stripped_source, "gl_TextureMatrix[0]", "mat4(1.0)" );

	std::ostringstream shader;
	shader << "#version 450\n\n";
	shader << "layout(location = 0) in vec3 in_position;\n";
	shader << "layout(location = 1) in vec2 in_texcoord;\n";
	shader << "layout(location = 2) in vec4 in_color;\n";
	shader << "layout(location = 0) out vec4 out_texcoord0;\n";
	shader << "layout(location = 1) out vec4 out_color;\n";
	shader << "layout(location = 2) out vec4 out_texcoord1;\n";
	shader << "layout(location = 3) out vec4 out_texcoord2;\n";
	shader << "layout(location = 4) out vec4 out_texcoord3;\n";
	shader << "layout(location = 5) out vec4 out_texcoord4;\n";
	shader << "layout(location = 6) out vec4 out_texcoord5;\n";
	shader << "layout(location = 7) out vec4 out_texcoord6;\n";
	shader << "layout(location = 8) out vec4 out_texcoord7;\n";
	for ( std::size_t i = 0; i < parsed_varyings.size(); ++i )
		shader << "layout(location = " << ( 9 + i ) << ") out " << parsed_varyings[ i ].type << " " << parsed_varyings[ i ].name << ";\n";
	shader << "layout(set = 1, binding = 0, std140) uniform CustomVertexUniforms\n";
	shader << "{\n";
	shader << "\tmat4 projection;\n";
	shader << "\tfloat viewport_width;\n";
	shader << "\tfloat viewport_height;\n";
	shader << "\tfloat plane_distance;\n";
	shader << "\tfloat reserved;\n";
	shader << "\tvec4 values[" << FE_MAX_CUSTOM_SHADER_UNIFORMS << "];\n";
	shader << "} custom_ubo;\n\n";
	for ( const CustomUniformBinding &uniform : uniforms )
		shader << build_custom_uniform_macro( uniform.name, uniform.slot, uniform.components ) << "\n";
	shader << "#define fe_vertex vec4(vec3(in_position.x - ( custom_ubo.viewport_width * 0.5 ), (( custom_ubo.viewport_height * 0.5 ) - in_position.y), -( custom_ubo.plane_distance - in_position.z )), 1.0)\n";
	shader << "\n" << stripped_source;

	source_code = shader.str();
	return true;
}

void FeSdl3GpuContext::build_custom_uniform_data(
	const FeRenderGeometry &image,
	const std::vector<CustomUniformBinding> &uniforms,
	std::vector<float> &data ) const
{
	data.assign( FE_MAX_CUSTOM_SHADER_UNIFORMS * 4, 0.0f );

	float image_width = 0.0f;
	float image_height = 0.0f;
	get_geometry_size( image, image_width, image_height );

	for ( const CustomUniformBinding &uniform : uniforms )
	{
		const std::vector<float> *values = image.shader ? image.shader->get_param( uniform.name.c_str() ) : nullptr;
		float *slot = data.data() + ( uniform.slot * 4 );

		if ( values && !values->empty() )
		{
			const int count = std::min( uniform.components, static_cast<int>( values->size() ) );
			for ( int i = 0; i < count; ++i )
				slot[ i ] = ( *values )[ i ];
			continue;
		}

		if ( uniform.name == "image_size" )
		{
			slot[ 0 ] = image_width;
			slot[ 1 ] = image_height;
		}
		else if ( uniform.name == "texture_size" )
		{
			slot[ 0 ] = image.texture_width;
			slot[ 1 ] = image.texture_height;
		}
	}
}

bool FeSdl3GpuContext::ensure_surface_target( const FeRenderSurfaceFrame &surface, SurfaceEntry &entry )
{
	if ( !m_device || surface.width <= 0 || surface.height <= 0 || !surface.surface_texture_id )
		return false;

	if ( entry.color_texture &&
		entry.depth_texture &&
		entry.width == surface.width &&
		entry.height == surface.height &&
		entry.mipmapped == surface.mipmapped )
		return true;

	release_surface_target( entry );

	SDL_GPUTextureCreateInfo color_info = {};
	color_info.type = SDL_GPU_TEXTURETYPE_2D;
	color_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	color_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
	color_info.width = static_cast<Uint32>( surface.width );
	color_info.height = static_cast<Uint32>( surface.height );
	color_info.layer_count_or_depth = 1;
	color_info.num_levels = surface.mipmapped
		? get_mip_level_count( color_info.width, color_info.height )
		: 1;
	color_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
	color_info.props = 0;

	entry.color_texture = SDL_CreateGPUTexture( m_device, &color_info );
	if ( !entry.color_texture )
	{
		release_surface_target( entry );
		return false;
	}

	SDL_GPUTextureCreateInfo depth_info = {};
	depth_info.type = SDL_GPU_TEXTURETYPE_2D;
	depth_info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
	depth_info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
	depth_info.width = static_cast<Uint32>( surface.width );
	depth_info.height = static_cast<Uint32>( surface.height );
	depth_info.layer_count_or_depth = 1;
	depth_info.num_levels = 1;
	depth_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
	depth_info.props = 0;

	entry.depth_texture = SDL_CreateGPUTexture( m_device, &depth_info );
	if ( !entry.depth_texture )
	{
		release_surface_target( entry );
		return false;
	}

	entry.width = surface.width;
	entry.height = surface.height;
	entry.mipmapped = surface.mipmapped;
	entry.rendered_once = false;
	entry.vertex_signature = 0;
	entry.last_signature = 0;
	return true;
}

bool FeSdl3GpuContext::upload_vertex_buffer( const std::vector<FeRenderVertex> &vertices, SDL_GPUBuffer *&buffer, Uint32 &buffer_size )
{
	if ( !m_device )
		return false;

	if ( vertices.empty() )
	{
		if ( buffer )
		{
			SDL_ReleaseGPUBuffer( m_device, buffer );
			buffer = nullptr;
		}
		buffer_size = 0;
		return true;
	}

	const Uint32 required_size = static_cast<Uint32>( vertices.size() * sizeof( FeRenderVertex ) );

	if ( !buffer || buffer_size < required_size )
	{
		if ( buffer )
			SDL_ReleaseGPUBuffer( m_device, buffer );

		SDL_GPUBufferCreateInfo buffer_info = {};
		buffer_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
		buffer_info.size = required_size;
		buffer_info.props = 0;

		buffer = SDL_CreateGPUBuffer( m_device, &buffer_info );
		if ( !buffer )
			return false;

		buffer_size = required_size;
	}

	SDL_GPUTransferBufferCreateInfo transfer_info = {};
	transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transfer_info.size = required_size;
	transfer_info.props = 0;

	SDL_GPUTransferBuffer *transfer_buffer = SDL_CreateGPUTransferBuffer( m_device, &transfer_info );
	if ( !transfer_buffer )
		return false;

	void *mapped = SDL_MapGPUTransferBuffer( m_device, transfer_buffer, false );
	if ( !mapped )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		return false;
	}

	std::memcpy( mapped, vertices.data(), required_size );
	SDL_UnmapGPUTransferBuffer( m_device, transfer_buffer );

	SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer( m_device );
	if ( !command_buffer )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		return false;
	}

	SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass( command_buffer );

	SDL_GPUTransferBufferLocation source = {};
	source.transfer_buffer = transfer_buffer;
	source.offset = 0;

	SDL_GPUBufferRegion destination = {};
	destination.buffer = buffer;
	destination.offset = 0;
	destination.size = required_size;

	SDL_UploadToGPUBuffer( copy_pass, &source, &destination, false );
	SDL_EndGPUCopyPass( copy_pass );

	const bool submitted = SDL_SubmitGPUCommandBuffer( command_buffer );
	SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
	return submitted;
}

bool FeSdl3GpuContext::upload_vertex_buffer()
{
	return upload_vertex_buffer( m_vertex_stream, m_vertex_buffer, m_vertex_buffer_size );
}

bool FeSdl3GpuContext::render_geometry_batch(
	SDL_GPURenderPass *render_pass,
	SDL_GPUCommandBuffer *command_buffer,
	const FePerspectiveCamera &camera,
	int viewport_width,
	int viewport_height,
	const std::vector<FeRenderGeometry> &geometry,
	std::uint64_t geometry_signature,
	bool use_surface_targets,
	SDL_GPUBuffer **cached_vertex_buffer,
	Uint32 *cached_vertex_buffer_size,
	std::uint64_t *cached_vertex_signature,
	bool &drew_anything )
{
	if ( !render_pass || !command_buffer || !m_blend_pipelines[0][FeBlend::Alpha] || geometry.empty() )
		return true;

	std::vector<FeRenderVertex> vertex_stream;
	vertex_stream.reserve( geometry.size() * 6 );

	struct LocalPreparedImage
	{
		const FeRenderGeometry *geometry;
		SDL_GPUTexture *gpu_texture;
		std::size_t first_vertex;
		std::size_t vertex_count;
		int blend_mode;
		bool zbuffer;
		bool translucent_depth;
		bool texture_repeated;
		bool texture_smooth;
		CustomShaderEntry *custom_shader;
		BuiltinShaderEntry *builtin_shader;
	};
	std::vector<LocalPreparedImage> prepared_images;
	prepared_images.reserve( geometry.size() );

	for ( const FeRenderGeometry &image : geometry )
	{
		LocalPreparedImage prepared = {};
		prepared.geometry = &image;
		prepared.first_vertex = vertex_stream.size();
		prepared.vertex_count = image.vertices.size();
		prepared.blend_mode = image.blend_mode;
		prepared.zbuffer = image.zbuffer;
		prepared.translucent_depth = geometry_uses_translucent_depth_pipeline( image );
		prepared.texture_repeated = image.texture_repeated;
		prepared.texture_smooth = image.texture_smooth;
		prepared.custom_shader = nullptr;
		prepared.builtin_shader = nullptr;

		if ( image.textured )
		{
			prepared.gpu_texture = nullptr;
			if ( use_surface_targets )
			{
				auto surface_it = m_surfaces.find( image.texture_id );
				if ( surface_it != m_surfaces.end() )
					prepared.gpu_texture = surface_it->second.color_texture;
			}

			if ( !prepared.gpu_texture )
			{
				auto it = m_textures.find( image.texture_id );
				prepared.gpu_texture = ( it != m_textures.end() ) ? it->second.gpu_texture : nullptr;
			}
		}
		else
			prepared.gpu_texture = ensure_white_texture() ? m_white_texture : nullptr;

		if ( image.custom_shader )
			prepared.custom_shader = get_custom_shader_entry( image );
		else if ( image.textured &&
			( image.blend_mode == FeBlend::Screen ||
				image.blend_mode == FeBlend::Multiply ||
				image.blend_mode == FeBlend::Overlay ||
				image.blend_mode == FeBlend::Premultiplied ) )
			prepared.builtin_shader = get_fast_builtin_shader_entry( image.blend_mode );

		for ( FeRenderVertex vertex : image.vertices )
		{
			if ( prepared.custom_shader && image.textured && image.texture_width > 0.0f && image.texture_height > 0.0f )
			{
				vertex.u /= image.texture_width;
				vertex.v /= image.texture_height;
			}
			vertex_stream.push_back( vertex );
		}
		prepared_images.push_back( prepared );
	}

	SDL_GPUBuffer *vertex_buffer = nullptr;
	Uint32 vertex_buffer_size = 0;
	const bool use_cached_vertex_buffer =
		cached_vertex_buffer && cached_vertex_buffer_size && cached_vertex_signature;
	if ( use_cached_vertex_buffer )
	{
		vertex_buffer = *cached_vertex_buffer;
		vertex_buffer_size = *cached_vertex_buffer_size;
		if ( !vertex_buffer || ( *cached_vertex_signature != geometry_signature ) )
		{
			if ( !upload_vertex_buffer( vertex_stream, vertex_buffer, vertex_buffer_size ) )
				return false;

			*cached_vertex_buffer = vertex_buffer;
			*cached_vertex_buffer_size = vertex_buffer_size;
			*cached_vertex_signature = geometry_signature;
		}
	}
	else if ( !upload_vertex_buffer( vertex_stream, vertex_buffer, vertex_buffer_size ) )
		return false;

	SDL_GPUBufferBinding vertex_binding = {};
	vertex_binding.buffer = vertex_buffer;
	vertex_binding.offset = 0;
	SDL_BindGPUVertexBuffers( render_pass, 0, &vertex_binding, 1 );

	FeSdl3GpuDrawUniforms uniforms = {};
	for ( int i = 0; i < 16; ++i )
		uniforms.projection[ i ] = camera.projection.m[ i ];
	uniforms.viewport_width = static_cast<float>( viewport_width );
	uniforms.viewport_height = static_cast<float>( viewport_height );
	const float safe_height = ( uniforms.viewport_height > 0.0f ) ? uniforms.viewport_height : 1.0f;
	const float fov_radians = camera.fov_y_degrees * 3.14159265358979323846f / 180.0f;
	const float tan_half_fov = std::tan( fov_radians * 0.5f );
	const float auto_plane_distance = ( tan_half_fov > 0.0f ) ? ( safe_height * 0.5f ) / tan_half_fov : safe_height;
	uniforms.plane_distance = ( camera.default_plane_z > camera.near_plane ) ? camera.default_plane_z : auto_plane_distance;
	if ( uniforms.plane_distance <= camera.near_plane )
		uniforms.plane_distance = camera.near_plane + 1.0f;
	SDL_PushGPUVertexUniformData( command_buffer, 0, &uniforms, sizeof( uniforms ) );

	if ( m_alpha_prepass_pipeline )
	{
		SDL_BindGPUGraphicsPipeline( render_pass, m_alpha_prepass_pipeline );

		for ( const LocalPreparedImage &image : prepared_images )
		{
			if ( !image.translucent_depth || !image.gpu_texture || image.vertex_count == 0 || image.custom_shader )
				continue;

			SDL_GPUTextureSamplerBinding sampler_binding = {};
			sampler_binding.texture = image.gpu_texture;
			if ( image.texture_smooth )
				sampler_binding.sampler = image.texture_repeated ? m_linear_repeat_sampler : m_linear_sampler;
			else
				sampler_binding.sampler = image.texture_repeated ? m_nearest_repeat_sampler : m_nearest_sampler;
			SDL_BindGPUFragmentSamplers( render_pass, 0, &sampler_binding, 1 );
			SDL_DrawGPUPrimitives(
				render_pass,
				static_cast<Uint32>( image.vertex_count ),
				1,
				static_cast<Uint32>( image.first_vertex ),
				0 );
		}
	}

	for ( const LocalPreparedImage &image : prepared_images )
	{
		if ( !image.gpu_texture || image.vertex_count == 0 )
			continue;

		const int blend_mode = clamp_blend_mode( image.blend_mode );
		const int depth_pipeline = get_depth_pipeline_index( image.zbuffer, image.translucent_depth );
		SDL_GPUGraphicsPipeline *pipeline = nullptr;
		if ( image.custom_shader )
		{
			pipeline = image.custom_shader->blend_pipelines[ depth_pipeline ][ blend_mode ];
			if ( !pipeline )
				pipeline = image.custom_shader->blend_pipelines[ depth_pipeline ][ FeBlend::Alpha ];

			if ( pipeline )
			{
				if ( image.custom_shader->has_custom_vertex )
				{
					std::vector<float> vertex_uniform_data;
					FeSdl3GpuCustomVertexUniforms custom_uniforms = {};
					for ( int i = 0; i < 16; ++i )
						custom_uniforms.projection[ i ] = uniforms.projection[ i ];
					custom_uniforms.viewport_width = uniforms.viewport_width;
					custom_uniforms.viewport_height = uniforms.viewport_height;
					custom_uniforms.plane_distance = uniforms.plane_distance;
					custom_uniforms.reserved = uniforms.reserved;
					build_custom_uniform_data( *image.geometry, image.custom_shader->vertex_uniforms, vertex_uniform_data );
					const std::size_t value_count = std::min<std::size_t>( FE_MAX_CUSTOM_SHADER_UNIFORMS * 4, vertex_uniform_data.size() );
					std::memcpy( custom_uniforms.values, vertex_uniform_data.data(), value_count * sizeof( float ) );
					SDL_PushGPUVertexUniformData(
						command_buffer,
						0,
						&custom_uniforms,
						static_cast<Uint32>( sizeof( custom_uniforms ) ) );
				}
				else
					SDL_PushGPUVertexUniformData( command_buffer, 0, &uniforms, sizeof( uniforms ) );

				if ( !image.custom_shader->fragment_uniforms.empty() )
				{
					std::vector<float> fragment_uniform_data;
					build_custom_uniform_data( *image.geometry, image.custom_shader->fragment_uniforms, fragment_uniform_data );
					SDL_PushGPUFragmentUniformData(
						command_buffer,
						0,
						fragment_uniform_data.data(),
						static_cast<Uint32>( fragment_uniform_data.size() * sizeof( float ) ) );
				}
			}
		}
		else if ( image.builtin_shader )
		{
			SDL_PushGPUVertexUniformData( command_buffer, 0, &uniforms, sizeof( uniforms ) );
			pipeline = image.builtin_shader->blend_pipelines[ depth_pipeline ][ blend_mode ];
			if ( !pipeline )
				pipeline = image.builtin_shader->blend_pipelines[ depth_pipeline ][ FeBlend::Alpha ];
		}

		if ( !pipeline )
		{
			SDL_PushGPUVertexUniformData( command_buffer, 0, &uniforms, sizeof( uniforms ) );
			pipeline = m_blend_pipelines[ depth_pipeline ][ blend_mode ];
			if ( !pipeline )
				pipeline = m_blend_pipelines[ depth_pipeline ][ FeBlend::Alpha ];
		}
		SDL_BindGPUGraphicsPipeline( render_pass, pipeline );

		std::vector<SDL_GPUTextureSamplerBinding> sampler_bindings;
		bool fast_current_texture_only = false;
		if ( image.custom_shader )
		{
			fast_current_texture_only =
				!image.custom_shader->has_custom_vertex &&
				image.custom_shader->vertex_uniforms.empty() &&
				image.custom_shader->fragment_uniforms.empty() &&
				image.custom_shader->fragment_samplers.size() == 1;

			if ( fast_current_texture_only )
			{
				const CustomSamplerBinding &sampler = image.custom_shader->fragment_samplers[ 0 ];
				const bool shader_uses_current_texture =
					image.geometry && image.geometry->shader
						? image.geometry->shader->uses_current_texture( sampler.name.c_str() )
						: false;
				FeImage *shader_image =
					image.geometry && image.geometry->shader
						? image.geometry->shader->get_texture_param_image( sampler.name.c_str() )
						: nullptr;
				fast_current_texture_only = shader_uses_current_texture || !shader_image;
			}
		}

		if ( image.custom_shader && fast_current_texture_only )
		{
			SDL_GPUTextureSamplerBinding sampler_binding = {};
			sampler_binding.texture = image.gpu_texture;
			if ( image.texture_smooth )
				sampler_binding.sampler = image.texture_repeated ? m_linear_repeat_sampler : m_linear_sampler;
			else
				sampler_binding.sampler = image.texture_repeated ? m_nearest_repeat_sampler : m_nearest_sampler;
			SDL_BindGPUFragmentSamplers( render_pass, 0, &sampler_binding, 1 );
		}
		else if ( image.custom_shader && !image.custom_shader->fragment_samplers.empty() )
		{
			sampler_bindings.resize( image.custom_shader->fragment_samplers.size() );
			bool samplers_ready = true;
			for ( const CustomSamplerBinding &sampler : image.custom_shader->fragment_samplers )
			{
				SDL_GPUTexture *sampler_texture = nullptr;
				bool sampler_repeated = image.texture_repeated;
				bool sampler_smooth = image.texture_smooth;
				const bool shader_uses_current_texture =
					image.geometry && image.geometry->shader
						? image.geometry->shader->uses_current_texture( sampler.name.c_str() )
						: false;
				FeImage *shader_image =
					image.geometry && image.geometry->shader
						? image.geometry->shader->get_texture_param_image( sampler.name.c_str() )
						: nullptr;
				const bool use_current_texture = shader_uses_current_texture || !shader_image;

				if ( use_current_texture )
				{
					sampler_texture = image.gpu_texture;
				}
				else if ( shader_image )
				{
					const FeBaseTextureContainer *container = shader_image->get_texture_container();
					if ( container )
					{
						auto surface_it = m_surfaces.find( container );
						if ( surface_it != m_surfaces.end() )
							sampler_texture = surface_it->second.color_texture;

						if ( !sampler_texture &&
							dynamic_cast<const FeSurfaceTextureContainer *>( container ) != nullptr )
						{
							samplers_ready = false;
							break;
						}

						if ( !sampler_texture )
						{
							TextureEntry &cache_entry = m_textures[ container ];
							cache_entry.width = static_cast<float>( shader_image->get_texture_width() );
							cache_entry.height = static_cast<float>( shader_image->get_texture_height() );
							cache_entry.mipmapped = shader_image->get_mipmap();
							cache_entry.last_seen_frame = m_frame.frame_number;
							if ( !cache_entry.gpu_texture )
							{
								if ( !upload_texture( container, FeRenderTextureSourceContainer, cache_entry ) )
									samplers_ready = false;
							}
							sampler_texture = cache_entry.gpu_texture;
						}

						sampler_repeated = container->get_repeat();
						sampler_smooth = container->get_smooth();
					}
				}

				if ( !samplers_ready || !sampler_texture )
				{
					samplers_ready = false;
					break;
				}

				SDL_GPUTextureSamplerBinding sampler_binding = {};
				sampler_binding.texture = sampler_texture;
				if ( sampler_smooth )
					sampler_binding.sampler = sampler_repeated ? m_linear_repeat_sampler : m_linear_sampler;
				else
					sampler_binding.sampler = sampler_repeated ? m_nearest_repeat_sampler : m_nearest_sampler;
				sampler_bindings[sampler.slot] = sampler_binding;
			}

			if ( !samplers_ready )
				continue;

			SDL_BindGPUFragmentSamplers(
				render_pass,
				0,
				sampler_bindings.data(),
				static_cast<Uint32>( sampler_bindings.size() ) );
		}
		else
		{
			SDL_GPUTextureSamplerBinding sampler_binding = {};
			sampler_binding.texture = image.gpu_texture;
			if ( image.texture_smooth )
				sampler_binding.sampler = image.texture_repeated ? m_linear_repeat_sampler : m_linear_sampler;
			else
				sampler_binding.sampler = image.texture_repeated ? m_nearest_repeat_sampler : m_nearest_sampler;
			SDL_BindGPUFragmentSamplers( render_pass, 0, &sampler_binding, 1 );
		}
		SDL_DrawGPUPrimitives(
			render_pass,
			static_cast<Uint32>( image.vertex_count ),
			1,
			static_cast<Uint32>( image.first_vertex ),
			0 );
		drew_anything = true;
	}

	if ( vertex_buffer && !use_cached_vertex_buffer )
		SDL_ReleaseGPUBuffer( m_device, vertex_buffer );

	return true;
}

bool FeSdl3GpuContext::render_surface_frames( SDL_GPUCommandBuffer *command_buffer )
{
	if ( !command_buffer || !m_blend_pipelines[0][ FeBlend::Alpha ] )
		return true;

	std::unordered_map<const void *, std::size_t> surface_indices;
	surface_indices.reserve( m_frame.surfaces.size() );
	for ( std::size_t i = 0; i < m_frame.surfaces.size(); ++i )
	{
		if ( m_frame.surfaces[i].surface_texture_id )
			surface_indices[ m_frame.surfaces[i].surface_texture_id ] = i;
	}

	std::vector<bool> rendered( m_frame.surfaces.size(), false );
	std::size_t rendered_count = 0;
	while ( rendered_count < m_frame.surfaces.size() )
	{
		bool progress = false;
		for ( std::size_t surface_index = 0; surface_index < m_frame.surfaces.size(); ++surface_index )
		{
			if ( rendered[surface_index] )
				continue;

			const FeRenderSurfaceFrame &surface = m_frame.surfaces[surface_index];
			if ( !surface.surface_texture_id )
			{
				rendered[surface_index] = true;
				++rendered_count;
				progress = true;
				continue;
			}

			bool waiting_on_dependency = false;
			for ( const void *dependency_id : surface.dependencies )
			{
				auto dependency_it = surface_indices.find( dependency_id );
				if ( dependency_it == surface_indices.end() )
					continue;

				const std::size_t dependency_index = dependency_it->second;
				if ( dependency_index != surface_index && !rendered[dependency_index] )
				{
					waiting_on_dependency = true;
					break;
				}
			}

			if ( waiting_on_dependency )
				continue;

			SurfaceEntry &entry = m_surfaces[ surface.surface_texture_id ];
			entry.last_seen_frame = m_frame.frame_number;
			if ( !ensure_surface_target( surface, entry ) )
				return false;

			const bool needs_render =
				!entry.rendered_once ||
				surface.dynamic_content ||
				( entry.last_signature != surface.content_signature ) ||
				( surface.redraw && !surface.clear );

			if ( needs_render )
			{
				SDL_GPUColorTargetInfo color_target = {};
				color_target.texture = entry.color_texture;
				color_target.mip_level = 0;
				color_target.layer_or_depth_plane = 0;
				color_target.clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 0.0f };
				color_target.load_op =
					( surface.clear || !entry.rendered_once )
						? SDL_GPU_LOADOP_CLEAR
						: SDL_GPU_LOADOP_LOAD;
				color_target.store_op = SDL_GPU_STOREOP_STORE;

				SDL_GPUDepthStencilTargetInfo depth_target = {};
				depth_target.texture = entry.depth_texture;
				depth_target.clear_depth = 1.0f;
				depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
				depth_target.store_op = SDL_GPU_STOREOP_STORE;
				depth_target.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
				depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
				depth_target.cycle = false;
				depth_target.clear_stencil = 0;

				SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(
					command_buffer,
					&color_target,
					1,
					&depth_target );
				if ( !render_pass )
					return false;

				bool drew_anything = false;
				const bool rendered_surface = render_geometry_batch(
					render_pass,
					command_buffer,
					surface.camera,
					surface.width,
					surface.height,
					surface.geometry,
					surface.geometry_signature,
					true,
					&entry.vertex_buffer,
					&entry.vertex_buffer_size,
					&entry.vertex_signature,
					drew_anything );
				SDL_EndGPURenderPass( render_pass );

				if ( !rendered_surface )
					return false;

				if ( surface.mipmapped && entry.color_texture )
					SDL_GenerateMipmapsForGPUTexture( command_buffer, entry.color_texture );

				entry.rendered_once = true;
				entry.last_signature = surface.content_signature;
			}

			rendered[surface_index] = true;
			++rendered_count;
			progress = true;
		}

		if ( !progress )
		{
			for ( std::size_t surface_index = 0; surface_index < m_frame.surfaces.size(); ++surface_index )
			{
				if ( rendered[surface_index] )
					continue;

				const FeRenderSurfaceFrame &surface = m_frame.surfaces[surface_index];
				if ( !surface.surface_texture_id )
				{
					rendered[surface_index] = true;
					++rendered_count;
					continue;
				}

				SurfaceEntry &entry = m_surfaces[ surface.surface_texture_id ];
				entry.last_seen_frame = m_frame.frame_number;
				if ( !ensure_surface_target( surface, entry ) )
					return false;

				const bool needs_render =
					!entry.rendered_once ||
					surface.dynamic_content ||
					( entry.last_signature != surface.content_signature ) ||
					( surface.redraw && !surface.clear );

				if ( needs_render )
				{
					SDL_GPUColorTargetInfo color_target = {};
					color_target.texture = entry.color_texture;
					color_target.mip_level = 0;
					color_target.layer_or_depth_plane = 0;
					color_target.clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 0.0f };
					color_target.load_op =
						( surface.clear || !entry.rendered_once )
							? SDL_GPU_LOADOP_CLEAR
							: SDL_GPU_LOADOP_LOAD;
					color_target.store_op = SDL_GPU_STOREOP_STORE;

					SDL_GPUDepthStencilTargetInfo depth_target = {};
					depth_target.texture = entry.depth_texture;
					depth_target.clear_depth = 1.0f;
					depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
					depth_target.store_op = SDL_GPU_STOREOP_STORE;
					depth_target.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
					depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
					depth_target.cycle = false;
					depth_target.clear_stencil = 0;

					SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(
						command_buffer,
						&color_target,
						1,
						&depth_target );
					if ( !render_pass )
						return false;

					bool drew_anything = false;
					const bool rendered_surface = render_geometry_batch(
						render_pass,
						command_buffer,
						surface.camera,
						surface.width,
						surface.height,
						surface.geometry,
						surface.geometry_signature,
						true,
						&entry.vertex_buffer,
						&entry.vertex_buffer_size,
						&entry.vertex_signature,
						drew_anything );
					SDL_EndGPURenderPass( render_pass );

					if ( !rendered_surface )
						return false;

					if ( surface.mipmapped && entry.color_texture )
						SDL_GenerateMipmapsForGPUTexture( command_buffer, entry.color_texture );

					entry.rendered_once = true;
					entry.last_signature = surface.content_signature;
				}

				rendered[surface_index] = true;
				++rendered_count;
			}
		}
	}

	for ( auto it = m_surfaces.begin(); it != m_surfaces.end(); )
	{
		if ( it->second.last_seen_frame != m_frame.frame_number )
		{
			release_surface_target( it->second );
			it = m_surfaces.erase( it );
		}
		else
			++it;
	}

	return true;
}

bool FeSdl3GpuContext::initialize_image_pipeline( SDL_GPUTextureFormat swapchain_format )
{
	if ( !m_device || ( swapchain_format == SDL_GPU_TEXTUREFORMAT_INVALID ) )
		return false;

	ShaderBlob vertex_blob = {};
	ShaderBlob alpha_prepass_blob = {};
	ShaderBlob fragment_blobs[FeBlend::None + 1] = {};
	if ( !compile_shader_blob( "__builtin_image_vertex__", build_builtin_vertex_shader(), true, vertex_blob ) )
	{
		write_debug_log( "initialize_image_pipeline: failed to compile internal vertex shader" );
		return false;
	}

	if ( !compile_shader_blob( "__builtin_image_alpha_prepass__", build_alpha_prepass_fragment_shader(), false, alpha_prepass_blob ) )
	{
		write_debug_log( "initialize_image_pipeline: failed to compile internal alpha prepass shader" );
		return false;
	}

	for ( int mode = FeBlend::Alpha; mode <= FeBlend::None; ++mode )
	{
		std::string fragment_source;
		if ( FeBlend::uses_default_shader( mode ) )
			fragment_source = build_fast_builtin_fragment_shader( mode );
		else
			fragment_source = build_builtin_passthrough_fragment_shader();

		if ( fragment_source.empty() ||
			!compile_shader_blob(
				std::string( "__builtin_image_fragment_" ) + std::to_string( mode ) + "__",
				fragment_source,
				false,
				fragment_blobs[ mode ] ) )
		{
			std::ostringstream stream;
			stream << "initialize_image_pipeline: failed to compile internal fragment shader for blend mode " << mode;
			write_debug_log( stream.str().c_str() );
			return false;
		}
	}

	SDL_GPUShaderCreateInfo vertex_info = {};
	vertex_info.code_size = vertex_blob.code.size();
	vertex_info.code = vertex_blob.code.data();
	vertex_info.entrypoint = vertex_blob.entrypoint;
	vertex_info.format = vertex_blob.format;
	vertex_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
	vertex_info.num_uniform_buffers = 1;

	m_vertex_shader = SDL_CreateGPUShader( m_device, &vertex_info );
	if ( !m_vertex_shader )
	{
		write_debug_log( "initialize_image_pipeline: SDL_CreateGPUShader failed for vertex shader" );
		return false;
	}

	SDL_GPUShaderCreateInfo fragment_info = {};
	fragment_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	fragment_info.num_samplers = 1;

	fragment_info.code_size = alpha_prepass_blob.code.size();
	fragment_info.code = alpha_prepass_blob.code.data();
	fragment_info.entrypoint = alpha_prepass_blob.entrypoint;
	fragment_info.format = alpha_prepass_blob.format;
	m_alpha_prepass_shader = SDL_CreateGPUShader( m_device, &fragment_info );
	if ( !m_alpha_prepass_shader )
	{
		write_debug_log( "initialize_image_pipeline: SDL_CreateGPUShader failed for alpha prepass shader" );
		release_image_pipeline();
		return false;
	}

	for ( int mode = FeBlend::Alpha; mode <= FeBlend::None; ++mode )
	{
		const ShaderBlob &blob = fragment_blobs[ mode ];
		fragment_info.code_size = blob.code.size();
		fragment_info.code = blob.code.data();
		fragment_info.entrypoint = blob.entrypoint;
		fragment_info.format = blob.format;
		m_fragment_shaders[ mode ] = SDL_CreateGPUShader( m_device, &fragment_info );
		if ( !m_fragment_shaders[ mode ] )
		{
			std::ostringstream stream;
			stream << "initialize_image_pipeline: SDL_CreateGPUShader failed for blend mode " << mode;
			write_debug_log( stream.str().c_str() );
			release_image_pipeline();
			return false;
		}
	}

	SDL_GPUSamplerCreateInfo sampler_info = {};
	sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
	sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
	sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
	sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	sampler_info.max_anisotropy = 1.0f;
	sampler_info.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
	sampler_info.min_lod = 0.0f;
	sampler_info.max_lod = 1000.0f;
	m_linear_sampler = SDL_CreateGPUSampler( m_device, &sampler_info );
	if ( !m_linear_sampler )
	{
		write_debug_log( "initialize_image_pipeline: SDL_CreateGPUSampler failed for linear sampler" );
		release_image_pipeline();
		return false;
	}

	sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
	sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
	sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
	m_linear_repeat_sampler = SDL_CreateGPUSampler( m_device, &sampler_info );
	if ( !m_linear_repeat_sampler )
	{
		write_debug_log( "initialize_image_pipeline: SDL_CreateGPUSampler failed for linear repeat sampler" );
		release_image_pipeline();
		return false;
	}

	sampler_info.min_filter = SDL_GPU_FILTER_NEAREST;
	sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
	sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	m_nearest_sampler = SDL_CreateGPUSampler( m_device, &sampler_info );
	if ( !m_nearest_sampler )
	{
		write_debug_log( "initialize_image_pipeline: SDL_CreateGPUSampler failed for nearest sampler" );
		release_image_pipeline();
		return false;
	}

	sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
	sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
	sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
	m_nearest_repeat_sampler = SDL_CreateGPUSampler( m_device, &sampler_info );
	if ( !m_nearest_repeat_sampler )
	{
		write_debug_log( "initialize_image_pipeline: SDL_CreateGPUSampler failed for nearest repeat sampler" );
		release_image_pipeline();
		return false;
	}

	SDL_GPUVertexBufferDescription vertex_buffer = {};
	vertex_buffer.slot = 0;
	vertex_buffer.pitch = sizeof( FeRenderVertex );
	vertex_buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	vertex_buffer.instance_step_rate = 0;

	SDL_GPUVertexAttribute attributes[3] = {};
	attributes[0].location = 0;
	attributes[0].buffer_slot = 0;
	attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	attributes[0].offset = offsetof( FeRenderVertex, x );
	attributes[1].location = 1;
	attributes[1].buffer_slot = 0;
	attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	attributes[1].offset = offsetof( FeRenderVertex, u );
	attributes[2].location = 2;
	attributes[2].buffer_slot = 0;
	attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
	attributes[2].offset = offsetof( FeRenderVertex, r );

	SDL_GPUColorTargetDescription color_target = {};
	color_target.format = swapchain_format;

	SDL_GPUColorTargetDescription alpha_prepass_target = {};
	alpha_prepass_target.format = swapchain_format;
	alpha_prepass_target.blend_state.enable_color_write_mask = true;
	alpha_prepass_target.blend_state.color_write_mask = 0;
	alpha_prepass_target.blend_state.enable_blend = false;

	SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.vertex_shader = m_vertex_shader;
	pipeline_info.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer;
	pipeline_info.vertex_input_state.num_vertex_buffers = 1;
	pipeline_info.vertex_input_state.vertex_attributes = attributes;
	pipeline_info.vertex_input_state.num_vertex_attributes = 3;
	pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
	pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
	pipeline_info.rasterizer_state.enable_depth_clip = true;
	pipeline_info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
	pipeline_info.multisample_state.sample_mask = 0;
	pipeline_info.target_info.color_target_descriptions = &color_target;
	pipeline_info.target_info.num_color_targets = 1;

	SDL_GPUGraphicsPipelineCreateInfo alpha_prepass_info = pipeline_info;
	alpha_prepass_info.fragment_shader = m_alpha_prepass_shader;
	alpha_prepass_info.target_info.color_target_descriptions = &alpha_prepass_target;
	configure_pipeline_depth_state( alpha_prepass_info, DepthPipelineWrite );
	m_alpha_prepass_pipeline = SDL_CreateGPUGraphicsPipeline( m_device, &alpha_prepass_info );
	if ( !m_alpha_prepass_pipeline )
	{
		write_debug_log( "initialize_image_pipeline: SDL_CreateGPUGraphicsPipeline failed for alpha prepass" );
		release_image_pipeline();
		return false;
	}

	for ( int z = 0; z < 3; ++z )
	{
		configure_pipeline_depth_state( pipeline_info, z );
		for ( int mode = FeBlend::Alpha; mode <= FeBlend::None; ++mode )
		{
			color_target.blend_state = make_gpu_blend_state( mode );
			pipeline_info.fragment_shader = m_fragment_shaders[ mode ];
			pipeline_info.target_info.color_target_descriptions = &color_target;
			m_blend_pipelines[ z ][ mode ] = SDL_CreateGPUGraphicsPipeline( m_device, &pipeline_info );
			if ( !m_blend_pipelines[ z ][ mode ] )
			{
				std::ostringstream stream;
				stream << "initialize_image_pipeline: SDL_CreateGPUGraphicsPipeline failed for blend mode " << mode;
				write_debug_log( stream.str().c_str() );
				release_image_pipeline();
				return false;
			}
		}
	}

	write_debug_log( "initialize_image_pipeline: success" );
	return true;
}

void FeSdl3GpuContext::write_debug_log( const char *message ) const
{
	if ( m_debug_logging_enabled && message && message[0] )
		FeLog() << message << std::endl;
}

SDL_Window *FeSdl3GpuContext::get_window() const
{
	return m_window;
}

SDL_GPUDevice *FeSdl3GpuContext::get_device() const
{
	return m_device;
}

void *FeSdl3GpuContext::get_native_window_handle() const
{
	if ( !m_window )
		return nullptr;

#if defined( SFML_SYSTEM_WINDOWS )
	SDL_PropertiesID props = SDL_GetWindowProperties( m_window );
	if ( !props )
		return nullptr;

	return SDL_GetPointerProperty( props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr );
#else
	return nullptr;
#endif
}
