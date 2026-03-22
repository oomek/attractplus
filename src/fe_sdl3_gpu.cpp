#include "fe_sdl3_gpu.hpp"
#include "fe_image.hpp"
#include "media.hpp"
#include "fe_shader.hpp"
#include "fe_util.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <cstring>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

#ifdef USE_SDL3_GPU
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>
#endif

namespace
{
	const int FE_MAX_CUSTOM_SHADER_UNIFORMS = 32;
	std::string join_path( const std::string &base, const std::string &suffix );
	std::string get_base_path();
	void append_debug_log( const std::string &message );

	struct ParsedCustomUniform
	{
		std::string type;
		std::string name;
		bool sampler;
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

	bool shader_compile_output_callback( const char *line, void * )
	{
		if ( line && line[0] )
			append_debug_log( std::string( "shader_compile: " ) + trim( line ) );
		return true;
	}

	SDL_GPUColorTargetBlendState make_gpu_blend_state( int mode )
	{
		SDL_GPUColorTargetBlendState blend_state = {};
		blend_state.color_write_mask =
			SDL_GPU_COLORCOMPONENT_R |
			SDL_GPU_COLORCOMPONENT_G |
			SDL_GPU_COLORCOMPONENT_B |
			SDL_GPU_COLORCOMPONENT_A;
		blend_state.enable_color_write_mask = true;
		blend_state.enable_blend = true;
		blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
		blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
		blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
		blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

		switch ( mode )
		{
			case FeBlend::Add:
				blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
				blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				break;
			case FeBlend::Subtract:
				blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
				blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend_state.color_blend_op = SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
				blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend_state.alpha_blend_op = SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
				break;
			case FeBlend::Screen:
				blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
				break;
			case FeBlend::Multiply:
				blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_COLOR;
				blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case FeBlend::Overlay:
				blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_COLOR;
				blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_COLOR;
				blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				break;
			case FeBlend::Premultiplied:
				blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case FeBlend::None:
				blend_state.enable_blend = false;
				blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
				blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
				break;
			case FeBlend::Alpha:
			default:
				blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
				blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
				break;
		}

		return blend_state;
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

	bool compile_custom_shader_to_spirv(
		const std::string &source_id,
		const std::string &translated_source,
		bool vertex_stage,
		std::string &spirv_path )
	{
		std::string cache_root = join_path( get_base_path(), "cache/" );
		confirm_directory( cache_root, "" );
		confirm_directory( cache_root, "shaders/" );
		cache_root = join_path( cache_root, "shaders/" );
		confirm_directory( cache_root, "sdl3/" );
		cache_root = join_path( cache_root, "sdl3/" );

		const std::string cache_name =
			sanitize_filename( source_id ) + "-" + std::to_string( std::hash<std::string>{}( translated_source ) );
		const std::string stage_name = vertex_stage ? "vert" : "frag";
		const std::string glsl_path = join_path( cache_root, cache_name + "." + stage_name + ".glsl" );
		spirv_path = join_path( cache_root, cache_name + "." + stage_name + ".spv" );

		if ( !file_exists( spirv_path ) )
		{
			if ( !write_file_content( glsl_path, translated_source ) )
			{
				append_debug_log( std::string( "custom_shader: failed to write translated shader " ) + glsl_path );
				return false;
			}

			const char *compiler = SDL_getenv( "FE_SDL3_GPU_GLSLANG" );
			const std::string compiler_name =
				( compiler && compiler[ 0 ] ) ? compiler : "glslangValidator";
			const std::string args =
				"-V -S " + stage_name + " -o \"" + spirv_path + "\" \"" + glsl_path + "\"";

			append_debug_log( std::string( "custom_shader: compiling " ) + source_id );
			if ( !run_program( compiler_name, args, cache_root, shader_compile_output_callback, nullptr, true, nullptr ) )
			{
				append_debug_log( std::string( "custom_shader: glslang compile failed for " ) + source_id );
				return false;
			}
		}

		return file_exists( spirv_path );
	}
}

bool fe_sdl3_gpu_present_requested()
{
	static const bool s_present_requested = []() -> bool
	{
		if ( SDL_getenv( "FE_SDL3_GPU_PRESENT" ) != nullptr )
			return true;

		if ( std::ifstream( "fe_sdl3_gpu_present.txt" ) )
			return true;

		const std::string base_path = get_base_path();
		if ( !base_path.empty() )
			return std::ifstream( join_path( base_path, "fe_sdl3_gpu_present.txt" ).c_str() ).good();

		return false;
	}();

	return s_present_requested;
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

#ifdef USE_SDL3_GPU
	m_sdl_ready = false;
	m_window_claimed = false;
	m_window = nullptr;
	m_device = nullptr;
	m_vertex_buffer = nullptr;
	m_vertex_buffer_size = 0;
	m_vertex_buffer_signature = 0;
	m_vertex_shader = nullptr;
	for ( int i = 0; i <= FeBlend::None; ++i )
	{
		m_fragment_shaders[i] = nullptr;
		m_blend_pipelines[i] = nullptr;
	}
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
#endif
}

FeSdl3GpuContext::~FeSdl3GpuContext()
{
#ifdef USE_SDL3_GPU
	shutdown();
#endif
}

void FeSdl3GpuContext::submit_frame( const FeRenderFrame &frame )
{
	m_frame = frame;
	m_has_submitted_frame = true;
	sync_texture_cache();
	if ( m_debug_logging_enabled )
		build_prepared_images();
	else
	{
		m_prepared_images.clear();
		m_vertex_stream.clear();
	}

#ifdef USE_SDL3_GPU
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
#endif

}

const FeRenderFrame &FeSdl3GpuContext::get_frame() const
{
	return m_frame;
}

std::size_t FeSdl3GpuContext::get_cached_texture_count() const
{
	return m_texture_cache.size();
}

const FeSdl3GpuContext::FrameStats &FeSdl3GpuContext::get_frame_stats() const
{
	return m_frame_stats;
}

void FeSdl3GpuContext::sync_texture_cache()
{
	std::size_t geometry_count = m_frame.images.size();
	for ( const FeRenderSurfaceFrame &surface : m_frame.surfaces )
		geometry_count += surface.geometry.size();

	if ( geometry_count == 0 )
	{
		clear_texture_cache();
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

		TextureCacheEntry &entry = m_texture_cache[ image.texture_id ];
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

#ifdef USE_SDL3_GPU
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
			const bool needs_upload =
				!had_texture ||
				( previous_width != image.texture_width ) ||
				( previous_height != image.texture_height ) ||
				( previous_mipmapped != image.texture_mipmap ) ||
				( image.texture_dynamic && entry.last_upload_content_version != content_version );

			if ( needs_upload && !waiting_for_first_dynamic_frame )
			{
				if ( upload_gpu_texture( image.texture_id, image.texture_source_type, entry ) )
				{
					entry.last_upload_frame = m_frame.frame_number;
					entry.last_upload_content_version = content_version;
				}
			}
		}
#endif
	};

	for ( const FeRenderGeometry &image : m_frame.images )
		sync_geometry( image );

	for ( const FeRenderSurfaceFrame &surface : m_frame.surfaces )
		for ( const FeRenderGeometry &image : surface.geometry )
			sync_geometry( image );

	for ( auto it = m_texture_cache.begin(); it != m_texture_cache.end(); )
	{
		if ( it->second.last_seen_frame != m_frame.frame_number )
		{
#ifdef USE_SDL3_GPU
			release_gpu_texture( it->second );
#endif
			it = m_texture_cache.erase( it );
		}
		else
			++it;
	}
}

void FeSdl3GpuContext::clear_texture_cache()
{
	for ( auto &entry : m_texture_cache )
	{
#ifdef USE_SDL3_GPU
		release_gpu_texture( entry.second );
#endif
	}

	m_texture_cache.clear();
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

#ifdef USE_SDL3_GPU
		if ( image.textured )
		{
			prepared.gpu_texture = nullptr;
			auto surface_it = m_surface_cache.find( image.texture_id );
			if ( surface_it != m_surface_cache.end() )
				prepared.gpu_texture = surface_it->second.color_texture;

			if ( !prepared.gpu_texture )
			{
				auto it = m_texture_cache.find( image.texture_id );
				prepared.gpu_texture = ( it != m_texture_cache.end() ) ? it->second.gpu_texture : nullptr;
			}
		}
		else
			prepared.gpu_texture = ensure_white_texture() ? m_white_texture : nullptr;
#endif

		m_vertex_stream.insert( m_vertex_stream.end(), image.vertices.begin(), image.vertices.end() );

		m_prepared_images.push_back( prepared );
	}
}

bool FeSdl3GpuContext::is_available() const
{
#ifdef USE_SDL3_GPU
	return ( m_device != nullptr ) && ( m_window != nullptr ) && m_window_claimed;
#else
	return false;
#endif
}

bool FeSdl3GpuContext::should_present() const
{
#ifdef USE_SDL3_GPU
	return is_available() && !m_present_disabled && has_submitted_frame() && has_frame_content();
#else
	return false;
#endif
}

bool FeSdl3GpuContext::has_submitted_frame() const
{
#ifdef USE_SDL3_GPU
	return m_has_submitted_frame;
#else
	return false;
#endif
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

#ifdef USE_SDL3_GPU
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

	std::string get_debug_log_path()
	{
		const std::string base_path = get_base_path();
		if ( !base_path.empty() )
			return join_path( base_path, "fe_sdl3_gpu.log" );

		return "fe_sdl3_gpu.log";
	}

	void append_debug_log( const std::string &message )
	{
		std::ofstream log_file( get_debug_log_path().c_str(), std::ios::app );
		if ( log_file )
			log_file << message << std::endl;
	}

	struct ShaderBlob
	{
		SDL_GPUShaderFormat format;
		std::vector<Uint8> code;
		const char *entrypoint;
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

	bool load_shader_blob( SDL_GPUDevice *device, const char *env_name, const char *default_base, ShaderBlob &blob )
	{
		struct Candidate
		{
			SDL_GPUShaderFormat format;
			const char *suffix;
			const char *entrypoint;
		};

		static const Candidate candidates[] =
		{
			{ SDL_GPU_SHADERFORMAT_SPIRV, ".spv", "main" },
#if defined( SFML_SYSTEM_WINDOWS )
			{ SDL_GPU_SHADERFORMAT_DXIL, ".dxil", "main" },
#endif
#if defined( SFML_SYSTEM_MACOS )
			{ SDL_GPU_SHADERFORMAT_METALLIB, ".metallib", "main0" },
			{ SDL_GPU_SHADERFORMAT_MSL, ".msl", "main0" },
#endif
		};

		const SDL_GPUShaderFormat supported_formats = SDL_GetGPUShaderFormats( device );
		const char *env_path = SDL_getenv( env_name );
		if ( env_path && env_path[0] )
		{
			std::string path( env_path );
			for ( const Candidate &candidate : candidates )
			{
				if ( !( supported_formats & candidate.format ) )
					continue;

				if ( path.size() < std::strlen( candidate.suffix ) )
					continue;

				if ( path.compare( path.size() - std::strlen( candidate.suffix ), std::strlen( candidate.suffix ), candidate.suffix ) != 0 )
					continue;

				if ( !read_binary_file( path, blob.code ) )
					return false;

				blob.format = candidate.format;
				blob.entrypoint = candidate.entrypoint;
				return true;
			}

			return false;
		}

		std::vector<std::string> search_roots;
		search_roots.push_back( "" );

		const std::string base_path = get_base_path();
		if ( !base_path.empty() )
			search_roots.push_back( base_path );

		for ( const Candidate &candidate : candidates )
		{
			if ( !( supported_formats & candidate.format ) )
				continue;

			for ( const std::string &root : search_roots )
			{
				const std::string path = join_path( root, std::string( default_base ) + candidate.suffix );
				append_debug_log( std::string( "load_shader_blob: try " ) + path );
				if ( !file_exists( path ) )
					continue;

				if ( !read_binary_file( path, blob.code ) )
					return false;

				blob.format = candidate.format;
				blob.entrypoint = candidate.entrypoint;
				return true;
			}
		}

		return false;
	}
}

bool FeSdl3GpuContext::execute_frame()
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
	if ( !SDL_AcquireGPUSwapchainTexture( command_buffer, m_window, &swapchain_texture, &swapchain_width, &swapchain_height ) )
	{
		write_debug_log( "execute_frame: SDL_AcquireGPUSwapchainTexture failed" );
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

	if ( ( m_swapchain_format != swapchain_format ) || !m_blend_pipelines[FeBlend::Alpha] )
	{
		release_custom_shader_cache();
		release_image_pipeline();
		m_swapchain_format = swapchain_format;
		m_pipeline_attempted = false;
	}

	if ( !m_pipeline_attempted )
	{
		initialize_image_pipeline( swapchain_format );
		m_pipeline_attempted = true;
	}

	m_frame_stats.pipeline_ready = ( m_blend_pipelines[FeBlend::Alpha] != nullptr );
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

	if ( m_blend_pipelines[FeBlend::Alpha] && !m_frame.images.empty() )
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

	if ( !m_sdl_ready )
	{
		if ( !SDL_InitSubSystem( SDL_INIT_VIDEO ) )
			return false;

		m_sdl_ready = true;
	}

	const char *preferred_driver = get_default_driver_name( driver_name );
	m_device = SDL_CreateGPUDevice( get_default_shader_formats(), debug_mode, preferred_driver );
	if ( !m_device && preferred_driver )
	{
		const std::string message = std::string( "initialize: preferred GPU driver failed: " ) + preferred_driver;
		write_debug_log( message.c_str() );
		m_device = SDL_CreateGPUDevice( get_default_shader_formats(), debug_mode, nullptr );
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

void FeSdl3GpuContext::shutdown()
{
	release_window();
	release_surface_cache();
	clear_texture_cache();
	release_white_texture();
	release_vertex_buffer();
	release_depth_target();
	release_custom_shader_cache();
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

void FeSdl3GpuContext::release_gpu_texture( TextureCacheEntry &entry )
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

bool FeSdl3GpuContext::upload_gpu_texture( const void *texture_id, int texture_source_type, TextureCacheEntry &entry )
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
	else if ( texture_source_type == FeRenderTextureSourceSfTexture )
	{
		const sf::Texture *source_texture = static_cast<const sf::Texture *>( texture_id );
		const sf::Image source_image = source_texture->copyToImage();
		const sf::Vector2u source_size = source_image.getSize();
		const unsigned char *pixels = source_image.getPixelsPtr();
		if ( !pixels || source_size.x == 0 || source_size.y == 0 )
			return false;

		source_width = source_size.x;
		source_height = source_size.y;
		pixel_data.assign(
			pixels,
			pixels + static_cast<std::size_t>( source_width ) * static_cast<std::size_t>( source_height ) * 4 );
	}
	else
		return false;

	release_gpu_texture( entry );

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
		release_gpu_texture( entry );
		return false;
	}

	void *mapped = SDL_MapGPUTransferBuffer( m_device, transfer_buffer, false );
	if ( !mapped )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		release_gpu_texture( entry );
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
			release_gpu_texture( entry );
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
			release_gpu_texture( entry );
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
		release_gpu_texture( entry );
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
		release_gpu_texture( entry );
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
	for ( int i = 0; i <= FeBlend::None; ++i )
	{
		if ( m_blend_pipelines[i] )
		{
			SDL_ReleaseGPUGraphicsPipeline( m_device, m_blend_pipelines[i] );
			m_blend_pipelines[i] = nullptr;
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
}

void FeSdl3GpuContext::release_custom_shader( CustomShaderEntry &entry )
{
	for ( int i = 0; i <= FeBlend::None; ++i )
	{
		if ( entry.blend_pipelines[ i ] )
		{
			SDL_ReleaseGPUGraphicsPipeline( m_device, entry.blend_pipelines[ i ] );
			entry.blend_pipelines[ i ] = nullptr;
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
	entry.source_stamp = 0;
	entry.vertex_source_stamp = 0;
}

void FeSdl3GpuContext::release_custom_shader_cache()
{
	for ( auto &entry : m_custom_shader_cache )
		release_custom_shader( entry.second );

	m_custom_shader_cache.clear();
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

void FeSdl3GpuContext::release_surface_target( SurfaceCacheEntry &entry )
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

void FeSdl3GpuContext::release_surface_cache()
{
	for ( auto &entry : m_surface_cache )
		release_surface_target( entry.second );

	m_surface_cache.clear();
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

	CustomShaderEntry &entry = m_custom_shader_cache[ cache_key ];
	if ( entry.fragment_shader &&
		entry.source_stamp == fragment_source_stamp &&
		entry.vertex_source_stamp == vertex_source_stamp )
		return &entry;

	release_custom_shader( entry );
	entry.source_path = cache_key;
	if ( !create_custom_shader_entry( image, entry ) )
	{
		m_custom_shader_cache.erase( cache_key );
		return nullptr;
	}

	entry.source_stamp = fragment_source_stamp;
	entry.vertex_source_stamp = vertex_source_stamp;
	return &entry;
}

bool FeSdl3GpuContext::create_custom_shader_entry( const FeRenderGeometry &image, CustomShaderEntry &entry )
{
	std::string fragment_source_code;
	std::vector<CustomUniformBinding> fragment_uniforms;
	std::vector<CustomSamplerBinding> fragment_samplers;
	if ( !build_custom_fragment_shader( image, fragment_source_code, fragment_uniforms, fragment_samplers ) )
		return false;

	std::string fragment_spirv_path;
	const std::string fragment_source_id =
		!image.shader->get_fragment_source_path().empty()
			? image.shader->get_fragment_source_path()
			: entry.source_path + "|fragment";
	if ( !compile_custom_shader_to_spirv( fragment_source_id, fragment_source_code, false, fragment_spirv_path ) )
		return false;

	ShaderBlob fragment_blob = {};
	if ( !read_binary_file( fragment_spirv_path, fragment_blob.code ) || fragment_blob.code.empty() )
	{
		write_debug_log( ( std::string( "custom_shader: failed reading compiled blob " ) + fragment_spirv_path ).c_str() );
		return false;
	}
	fragment_blob.format = SDL_GPU_SHADERFORMAT_SPIRV;
	fragment_blob.entrypoint = "main";

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

		std::string vertex_spirv_path;
		const std::string vertex_source_id =
			!image.shader->get_vertex_source_path().empty()
				? image.shader->get_vertex_source_path()
				: entry.source_path + "|vertex";
		if ( !compile_custom_shader_to_spirv( vertex_source_id, vertex_source_code, true, vertex_spirv_path ) )
			return false;

		ShaderBlob vertex_blob = {};
		if ( !read_binary_file( vertex_spirv_path, vertex_blob.code ) || vertex_blob.code.empty() )
		{
			write_debug_log( ( std::string( "custom_shader: failed reading compiled blob " ) + vertex_spirv_path ).c_str() );
			return false;
		}
		vertex_blob.format = SDL_GPU_SHADERFORMAT_SPIRV;
		vertex_blob.entrypoint = "main";

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
	pipeline_info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
	pipeline_info.target_info.has_depth_stencil_target = true;
	pipeline_info.depth_stencil_state.enable_depth_test = true;
	pipeline_info.depth_stencil_state.enable_depth_write = true;
	pipeline_info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
	pipeline_info.fragment_shader = entry.fragment_shader;

	for ( int blend_mode = FeBlend::Alpha; blend_mode <= FeBlend::None; ++blend_mode )
	{
		color_target.blend_state = make_gpu_blend_state( blend_mode );
		entry.blend_pipelines[ blend_mode ] = SDL_CreateGPUGraphicsPipeline( m_device, &pipeline_info );
		if ( !entry.blend_pipelines[ blend_mode ] )
		{
			write_debug_log( ( std::string( "custom_shader: SDL_CreateGPUGraphicsPipeline failed for " ) + entry.source_path ).c_str() );
			release_custom_shader( entry );
			return false;
		}
	}

	entry.vertex_uniforms = vertex_uniforms;
	entry.fragment_uniforms = fragment_uniforms;
	entry.fragment_samplers = fragment_samplers;
	entry.uses_current_texture = false;
	for ( const CustomSamplerBinding &sampler : fragment_samplers )
		if ( sampler.current_texture )
			entry.uses_current_texture = true;
	entry.has_custom_vertex = has_custom_vertex;
	write_debug_log( ( std::string( "custom_shader: ready " ) + entry.source_path ).c_str() );
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

	std::vector<ParsedCustomUniform> parsed_uniforms;
	std::string stripped_source;
	if ( !parse_custom_uniforms( raw_source, parsed_uniforms, stripped_source ) )
		return false;

	for ( const ParsedCustomUniform &uniform : parsed_uniforms )
	{
		if ( uniform.sampler )
		{
			CustomSamplerBinding binding = {};
			binding.name = uniform.name;
			binding.slot = static_cast<int>( samplers.size() );
			binding.current_texture = image.shader->uses_current_texture( uniform.name.c_str() );
			binding.image = image.shader->get_texture_param_image( uniform.name.c_str() );
			if ( !binding.current_texture && !binding.image )
				binding.current_texture = true;
			samplers.push_back( binding );
			continue;
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
	std::string stripped_source;
	if ( !parse_custom_uniforms( raw_source, parsed_uniforms, stripped_source ) )
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

bool FeSdl3GpuContext::ensure_surface_target( const FeRenderSurfaceFrame &surface, SurfaceCacheEntry &entry )
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
	if ( !render_pass || !command_buffer || !m_blend_pipelines[FeBlend::Alpha] || geometry.empty() )
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
		bool texture_repeated;
		bool texture_smooth;
		CustomShaderEntry *custom_shader;
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
		prepared.texture_repeated = image.texture_repeated;
		prepared.texture_smooth = image.texture_smooth;
		prepared.custom_shader = nullptr;

		if ( image.textured )
		{
			prepared.gpu_texture = nullptr;
			if ( use_surface_targets )
			{
				auto surface_it = m_surface_cache.find( image.texture_id );
				if ( surface_it != m_surface_cache.end() )
					prepared.gpu_texture = surface_it->second.color_texture;
			}

			if ( !prepared.gpu_texture )
			{
				auto it = m_texture_cache.find( image.texture_id );
				prepared.gpu_texture = ( it != m_texture_cache.end() ) ? it->second.gpu_texture : nullptr;
			}
		}
		else
			prepared.gpu_texture = ensure_white_texture() ? m_white_texture : nullptr;

		if ( image.custom_shader )
			prepared.custom_shader = get_custom_shader_entry( image );

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

	for ( const LocalPreparedImage &image : prepared_images )
	{
		if ( !image.gpu_texture || image.vertex_count == 0 )
			continue;

		const int blend_mode = clamp_blend_mode( image.blend_mode );
		SDL_GPUGraphicsPipeline *pipeline = nullptr;
		if ( image.custom_shader )
		{
			pipeline = image.custom_shader->blend_pipelines[ blend_mode ];
			if ( !pipeline )
				pipeline = image.custom_shader->blend_pipelines[ FeBlend::Alpha ];

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

				std::vector<float> fragment_uniform_data;
				build_custom_uniform_data( *image.geometry, image.custom_shader->fragment_uniforms, fragment_uniform_data );
				SDL_PushGPUFragmentUniformData(
					command_buffer,
					0,
					fragment_uniform_data.data(),
					static_cast<Uint32>( fragment_uniform_data.size() * sizeof( float ) ) );
			}
		}

		if ( !pipeline )
		{
			SDL_PushGPUVertexUniformData( command_buffer, 0, &uniforms, sizeof( uniforms ) );
			pipeline = m_blend_pipelines[ blend_mode ];
			if ( !pipeline )
				pipeline = m_blend_pipelines[ FeBlend::Alpha ];
		}
		SDL_BindGPUGraphicsPipeline( render_pass, pipeline );

		std::vector<SDL_GPUTextureSamplerBinding> sampler_bindings;
		if ( image.custom_shader && !image.custom_shader->fragment_samplers.empty() )
		{
			sampler_bindings.resize( image.custom_shader->fragment_samplers.size() );
			bool samplers_ready = true;
			for ( const CustomSamplerBinding &sampler : image.custom_shader->fragment_samplers )
			{
				SDL_GPUTexture *sampler_texture = nullptr;
				bool sampler_repeated = image.texture_repeated;
				bool sampler_smooth = image.texture_smooth;

				if ( sampler.current_texture )
				{
					sampler_texture = image.gpu_texture;
				}
				else if ( sampler.image )
				{
					const FeBaseTextureContainer *container = sampler.image->get_texture_container();
					if ( container )
					{
						auto surface_it = m_surface_cache.find( container );
						if ( surface_it != m_surface_cache.end() )
							sampler_texture = surface_it->second.color_texture;

						if ( !sampler_texture )
						{
							TextureCacheEntry &cache_entry = m_texture_cache[ container ];
							cache_entry.width = static_cast<float>( sampler.image->get_texture_width() );
							cache_entry.height = static_cast<float>( sampler.image->get_texture_height() );
							cache_entry.mipmapped = sampler.image->get_mipmap();
							cache_entry.last_seen_frame = m_frame.frame_number;
							if ( !cache_entry.gpu_texture )
							{
								if ( !upload_gpu_texture( container, FeRenderTextureSourceContainer, cache_entry ) )
									samplers_ready = false;
							}
							sampler_texture = cache_entry.gpu_texture;
						}

						sampler_repeated = container->get_repeat();
						sampler_smooth = container->get_smooth();
					}
					else
					{
						const sf::Texture *external_texture = sampler.image->get_texture();
						if ( external_texture )
						{
							TextureCacheEntry &cache_entry = m_texture_cache[ external_texture ];
							cache_entry.width = static_cast<float>( external_texture->getSize().x );
							cache_entry.height = static_cast<float>( external_texture->getSize().y );
							cache_entry.mipmapped = sampler.image->get_mipmap();
							cache_entry.last_seen_frame = m_frame.frame_number;
							if ( !cache_entry.gpu_texture )
							{
								if ( !upload_gpu_texture( external_texture, FeRenderTextureSourceSfTexture, cache_entry ) )
									samplers_ready = false;
							}
							sampler_texture = cache_entry.gpu_texture;
							sampler_repeated = external_texture->isRepeated();
							sampler_smooth = external_texture->isSmooth();
						}
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
	if ( !command_buffer || !m_blend_pipelines[ FeBlend::Alpha ] )
		return true;

	for ( const FeRenderSurfaceFrame &surface : m_frame.surfaces )
	{
		if ( !surface.surface_texture_id )
			continue;

		SurfaceCacheEntry &entry = m_surface_cache[ surface.surface_texture_id ];
		entry.last_seen_frame = m_frame.frame_number;
		if ( !ensure_surface_target( surface, entry ) )
			return false;

		const bool needs_render =
			!entry.rendered_once ||
			surface.dynamic_content ||
			( entry.last_signature != surface.content_signature ) ||
			( surface.redraw && !surface.clear );
		if ( !needs_render )
			continue;

		SDL_GPUColorTargetInfo color_target = {};
		color_target.texture = entry.color_texture;
		color_target.mip_level = 0;
		color_target.layer_or_depth_plane = 0;
		color_target.clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 0.0f };
		color_target.load_op = surface.clear ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
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
		const bool rendered = render_geometry_batch(
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

		if ( !rendered )
			return false;

		if ( surface.mipmapped && entry.color_texture )
		{
			SDL_GenerateMipmapsForGPUTexture( command_buffer, entry.color_texture );
		}

		entry.rendered_once = true;
		entry.last_signature = surface.content_signature;
	}

	for ( auto it = m_surface_cache.begin(); it != m_surface_cache.end(); )
	{
		if ( it->second.last_seen_frame != m_frame.frame_number )
		{
			release_surface_target( it->second );
			it = m_surface_cache.erase( it );
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
	struct BlendShaderDesc
	{
		int mode;
		const char *env_name;
		const char *default_base;
	};
	static const BlendShaderDesc blend_shader_descs[] =
	{
		{ FeBlend::Alpha, "FE_SDL3_GPU_ALPHA_FRAGMENT_SHADER", "config/shaders/sdl3/alpha.frag" },
		{ FeBlend::Add, "FE_SDL3_GPU_ADD_FRAGMENT_SHADER", "config/shaders/sdl3/add.frag" },
		{ FeBlend::Subtract, "FE_SDL3_GPU_SUBTRACT_FRAGMENT_SHADER", "config/shaders/sdl3/subtract.frag" },
		{ FeBlend::Screen, "FE_SDL3_GPU_SCREEN_FRAGMENT_SHADER", "config/shaders/sdl3/screen.frag" },
		{ FeBlend::Multiply, "FE_SDL3_GPU_MULTIPLY_FRAGMENT_SHADER", "config/shaders/sdl3/multiply.frag" },
		{ FeBlend::Overlay, "FE_SDL3_GPU_OVERLAY_FRAGMENT_SHADER", "config/shaders/sdl3/overlay.frag" },
		{ FeBlend::Premultiplied, "FE_SDL3_GPU_PREMULTIPLIED_FRAGMENT_SHADER", "config/shaders/sdl3/premultiplied.frag" },
		{ FeBlend::None, "FE_SDL3_GPU_NONE_FRAGMENT_SHADER", "config/shaders/sdl3/none.frag" }
	};
	ShaderBlob fragment_blobs[FeBlend::None + 1] = {};
	if ( !load_shader_blob( m_device, "FE_SDL3_GPU_VERTEX_SHADER", "config/shaders/sdl3/image.vert", vertex_blob ) )
	{
		write_debug_log( "initialize_image_pipeline: failed to load vertex shader blob" );
		return false;
	}

	for ( const BlendShaderDesc &desc : blend_shader_descs )
	{
		if ( !load_shader_blob( m_device, desc.env_name, desc.default_base, fragment_blobs[ desc.mode ] ) )
		{
			std::ostringstream stream;
			stream << "initialize_image_pipeline: failed to load fragment shader blob for blend mode " << desc.mode;
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

	for ( const BlendShaderDesc &desc : blend_shader_descs )
	{
		const ShaderBlob &blob = fragment_blobs[ desc.mode ];
		fragment_info.code_size = blob.code.size();
		fragment_info.code = blob.code.data();
		fragment_info.entrypoint = blob.entrypoint;
		fragment_info.format = blob.format;
		m_fragment_shaders[ desc.mode ] = SDL_CreateGPUShader( m_device, &fragment_info );
		if ( !m_fragment_shaders[ desc.mode ] )
		{
			std::ostringstream stream;
			stream << "initialize_image_pipeline: SDL_CreateGPUShader failed for blend mode " << desc.mode;
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

	auto make_blend_state = []( int mode )
	{
		SDL_GPUColorTargetBlendState blend_state = {};
		blend_state.color_write_mask =
			SDL_GPU_COLORCOMPONENT_R |
			SDL_GPU_COLORCOMPONENT_G |
			SDL_GPU_COLORCOMPONENT_B |
			SDL_GPU_COLORCOMPONENT_A;
		blend_state.enable_color_write_mask = true;
		blend_state.enable_blend = true;
		blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
		blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
		blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
		blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

		switch ( mode )
		{
			case FeBlend::Add:
				blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
				blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				break;
			case FeBlend::Subtract:
				blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
				blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend_state.color_blend_op = SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
				blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend_state.alpha_blend_op = SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
				break;
			case FeBlend::Screen:
				blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
				break;
			case FeBlend::Multiply:
				blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_COLOR;
				blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case FeBlend::Overlay:
				blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_COLOR;
				blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_COLOR;
				blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				break;
			case FeBlend::Premultiplied:
				blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			case FeBlend::None:
				blend_state.enable_blend = false;
				blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
				blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
				blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
				break;
			case FeBlend::Alpha:
			default:
				blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
				blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
				break;
		}

		return blend_state;
	};

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
	pipeline_info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
	pipeline_info.target_info.has_depth_stencil_target = true;
	pipeline_info.depth_stencil_state.enable_depth_test = true;
	pipeline_info.depth_stencil_state.enable_depth_write = true;
	pipeline_info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

	for ( const BlendShaderDesc &desc : blend_shader_descs )
	{
		color_target.blend_state = make_blend_state( desc.mode );
		pipeline_info.fragment_shader = m_fragment_shaders[ desc.mode ];
		pipeline_info.target_info.color_target_descriptions = &color_target;
		m_blend_pipelines[ desc.mode ] = SDL_CreateGPUGraphicsPipeline( m_device, &pipeline_info );
		if ( !m_blend_pipelines[ desc.mode ] )
		{
			std::ostringstream stream;
			stream << "initialize_image_pipeline: SDL_CreateGPUGraphicsPipeline failed for blend mode " << desc.mode;
			write_debug_log( stream.str().c_str() );
			release_image_pipeline();
			return false;
		}
	}

	write_debug_log( "initialize_image_pipeline: success" );
	return true;
}

void FeSdl3GpuContext::write_debug_log( const char *message ) const
{
	if ( m_debug_logging_enabled && message && message[0] )
		append_debug_log( message );
}

SDL_Window *FeSdl3GpuContext::get_window() const
{
	return m_window;
}

SDL_GPUDevice *FeSdl3GpuContext::get_device() const
{
	return m_device;
}
#endif
