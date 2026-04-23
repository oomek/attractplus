#include "fe_sdl3_gpu.hpp"
#include "fe_glslang.hpp"
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
#include <sstream>
#include <utility>

namespace
{
	const int FE_MAX_CUSTOM_SHADER_UNIFORMS = 32;
	std::string join_path( const std::string &base, const std::string &suffix );
	std::string get_base_path();
	bool read_binary_file( const std::string &path, std::vector<Uint8> &code );
	bool write_binary_file( const std::string &path, const std::vector<Uint8> &code );

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


	struct ShaderBlob
	{
		SDL_GPUShaderFormat format;
		std::vector<Uint8> code;
		const char *entrypoint;
	};

	const char *get_builtin_image_fragment_shader_name( int mode )
	{
		switch ( mode )
		{
			case FeBlend::Alpha: return "__alpha_fragment__";
			case FeBlend::Add: return "__add_fragment__";
			case FeBlend::Subtract: return "__subtract_fragment__";
			case FeBlend::Screen: return "__screen_fragment__";
			case FeBlend::Multiply: return "__multiply_fragment__";
			case FeBlend::Overlay: return "__overlay_fragment__";
			case FeBlend::Premultiplied: return "__premultiplied_fragment__";
			case FeBlend::None: return "__none_fragment__";
			default: return "__fragment__";
		}
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

		if ( image.geometry_kind == FeRenderGeometryObjectPbr )
			return image.pbr_material.alpha_mode == FeRenderPbrAlphaBlend;

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
		const FeRenderVertex *vertices = image.get_vertex_data();
		const std::size_t vertex_count = image.get_vertex_count();
		if ( !vertices || vertex_count == 0 )
			return;

		float min_x = vertices[0].x;
		float max_x = vertices[0].x;
		float min_y = vertices[0].y;
		float max_y = vertices[0].y;

		for ( std::size_t i = 0; i < vertex_count; ++i )
		{
			const FeRenderVertex &vertex = vertices[i];
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

	std::uint64_t compute_vertex_stream_hash( const std::vector<FeRenderVertex> &vertices )
	{
		std::uint64_t hash = 1469598103934665603ULL;
		hash = hash_combine_u64( hash, static_cast<std::uint64_t>( vertices.size() ) );
		for ( const FeRenderVertex &vertex : vertices )
		{
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( std::lround( vertex.x * 1024.0f ) ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( std::lround( vertex.y * 1024.0f ) ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( std::lround( vertex.z * 1024.0f ) ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( std::lround( vertex.u * 1024.0f ) ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( std::lround( vertex.v * 1024.0f ) ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( std::lround( vertex.u1 * 1024.0f ) ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( std::lround( vertex.v1 * 1024.0f ) ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( std::lround( vertex.nx * 1024.0f ) ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( std::lround( vertex.ny * 1024.0f ) ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( std::lround( vertex.nz * 1024.0f ) ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( std::lround( vertex.tx * 1024.0f ) ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( std::lround( vertex.ty * 1024.0f ) ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( std::lround( vertex.tz * 1024.0f ) ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( std::lround( vertex.tw * 1024.0f ) ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( vertex.r ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( vertex.g ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( vertex.b ) );
			hash = hash_combine_u64( hash, static_cast<std::uint64_t>( vertex.a ) );
		}
		return hash;
	}

	std::uint64_t hash_float_bits( std::uint64_t seed, float value )
	{
		std::uint32_t bits = 0;
		std::memcpy( &bits, &value, sizeof( bits ) );
		return hash_combine_u64( seed, static_cast<std::uint64_t>( bits ) );
	}

	float compute_plane_distance( const FePerspectiveCamera &camera, int viewport_height )
	{
		const float safe_height = ( viewport_height > 0 ) ? static_cast<float>( viewport_height ) : 1.0f;
		const float fov_radians = camera.fov_y_degrees * 3.14159265358979323846f / 180.0f;
		const float tan_half_fov = std::tan( fov_radians * 0.5f );
		const float auto_plane_distance = ( tan_half_fov > 0.0f ) ? ( safe_height * 0.5f ) / tan_half_fov : safe_height;
		float plane_distance = ( camera.default_plane_z > camera.near_plane ) ? camera.default_plane_z : auto_plane_distance;
		if ( plane_distance <= camera.near_plane )
			plane_distance = camera.near_plane + 1.0f;
		return plane_distance;
	}

	Vec3f to_view_position( const Vec3f &world_position, int viewport_width, int viewport_height, float plane_distance )
	{
		return Vec3f(
			world_position.x - ( static_cast<float>( viewport_width ) * 0.5f ),
			( static_cast<float>( viewport_height ) * 0.5f ) - world_position.y,
			world_position.z - plane_distance );
	}

	Vec3f to_view_direction( const Vec3f &world_direction )
	{
		Vec3f direction( world_direction.x, -world_direction.y, world_direction.z );
		const float length_sq =
			direction.x * direction.x +
			direction.y * direction.y +
			direction.z * direction.z;
		if ( length_sq <= 1.0e-8f )
			return Vec3f( 0.0f, 0.0f, -1.0f );

		const float inv_length = 1.0f / std::sqrt( length_sq );
		return Vec3f( direction.x * inv_length, direction.y * inv_length, direction.z * inv_length );
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

	std::string build_pbr_vertex_shader()
	{
		return
			"#version 450\n"
			"layout(location = 0) in vec3 in_position;\n"
			"layout(location = 1) in vec2 in_texcoord0;\n"
			"layout(location = 2) in vec4 in_color;\n"
			"layout(location = 3) in vec2 in_texcoord1;\n"
			"layout(location = 4) in vec3 in_normal;\n"
			"layout(location = 5) in vec4 in_tangent;\n"
			"layout(location = 6) in vec4 in_model_0;\n"
			"layout(location = 7) in vec4 in_model_1;\n"
			"layout(location = 8) in vec4 in_model_2;\n"
			"layout(location = 9) in vec4 in_model_3;\n"
			"layout(location = 10) in vec4 in_normal_row_0;\n"
			"layout(location = 11) in vec4 in_normal_row_1;\n"
			"layout(location = 12) in vec4 in_normal_row_2;\n"
			"layout(location = 0) out vec2 frag_uv0;\n"
			"layout(location = 1) out vec2 frag_uv1;\n"
			"layout(location = 2) out vec4 frag_color;\n"
			"layout(location = 3) out vec3 frag_view_position;\n"
			"layout(location = 4) out vec3 frag_view_normal;\n"
			"layout(location = 5) out vec4 frag_view_tangent;\n"
			"layout(set = 1, binding = 0) uniform PbrVertexUniforms\n"
			"{\n"
			"\tmat4 projection;\n"
			"\tfloat viewport_width;\n"
			"\tfloat viewport_height;\n"
			"\tfloat plane_distance;\n"
			"\tfloat reserved;\n"
			"} ubo;\n"
			"void main()\n"
			"{\n"
			"\tmat4 model = mat4( in_model_0, in_model_1, in_model_2, in_model_3 );\n"
			"\tvec4 world_position = model * vec4( in_position, 1.0 );\n"
			"\tvec3 view_position = vec3(\n"
			"\t\tworld_position.x - ( ubo.viewport_width * 0.5 ),\n"
			"\t\t( ubo.viewport_height * 0.5 ) - world_position.y,\n"
			"\t\tworld_position.z - ubo.plane_distance );\n"
			"\tvec3 world_normal = normalize( vec3(\n"
			"\t\tdot( in_normal_row_0.xyz, in_normal ),\n"
			"\t\tdot( in_normal_row_1.xyz, in_normal ),\n"
			"\t\tdot( in_normal_row_2.xyz, in_normal ) ) );\n"
			"\tvec3 world_tangent = normalize( ( model * vec4( in_tangent.xyz, 0.0 ) ).xyz );\n"
			"\tgl_Position = ubo.projection * vec4( view_position, 1.0 );\n"
			"\tfrag_uv0 = in_texcoord0;\n"
			"\tfrag_uv1 = in_texcoord1;\n"
			"\tfrag_color = in_color;\n"
			"\tfrag_view_position = view_position;\n"
			"\tfrag_view_normal = normalize( vec3( world_normal.x, -world_normal.y, world_normal.z ) );\n"
			"\tfrag_view_tangent = vec4( normalize( vec3( world_tangent.x, -world_tangent.y, world_tangent.z ) ), in_tangent.w );\n"
			"}\n";
	}

	std::string build_pbr_fragment_shader()
	{
		return
			"#version 450\n"
			"layout(location = 0) in vec2 frag_uv0;\n"
			"layout(location = 1) in vec2 frag_uv1;\n"
			"layout(location = 2) in vec4 frag_color;\n"
			"layout(location = 3) in vec3 frag_view_position;\n"
			"layout(location = 4) in vec3 frag_view_normal;\n"
			"layout(location = 5) in vec4 frag_view_tangent;\n"
			"layout(location = 0) out vec4 out_color;\n"
			"layout(set = 2, binding = 0) uniform sampler2D base_color_texture;\n"
			"layout(set = 2, binding = 1) uniform sampler2D metallic_roughness_texture;\n"
			"layout(set = 2, binding = 2) uniform sampler2D normal_texture;\n"
			"layout(set = 2, binding = 3) uniform sampler2D occlusion_texture;\n"
			"layout(set = 2, binding = 4) uniform sampler2D emissive_texture;\n"
			"struct LightUniform\n"
			"{\n"
			"\tvec4 color_intensity;\n"
			"\tvec4 position_range;\n"
			"\tvec4 direction_inner;\n"
			"\tvec4 outer_type;\n"
			"};\n"
			"layout(set = 3, binding = 0, std140) uniform PbrUniforms\n"
			"{\n"
			"\tvec4 base_color_factor;\n"
			"\tvec4 emissive_factor;\n"
			"\tvec4 material_params;\n"
			"\tvec4 control;\n"
			"\tvec4 ambient_color;\n"
			"\tvec4 artwork_control;\n"
			"\tvec4 transforms_offset_scale[5];\n"
			"\tvec4 transforms_texcoord_fit[5];\n"
			"\tLightUniform lights[4];\n"
			"} pbr;\n"
			"const float PI = 3.14159265358979323846;\n"
			"vec3 srgb_to_linear( vec3 value )\n"
			"{\n"
			"\treturn pow( max( value, vec3( 0.0 ) ), vec3( 2.2 ) );\n"
			"}\n"
			"vec2 select_uv( float texcoord_set )\n"
			"{\n"
			"\treturn texcoord_set > 0.5 ? frag_uv1 : frag_uv0;\n"
			"}\n"
			"vec2 transform_uv( vec2 uv, int index, out float fit_alpha )\n"
			"{\n"
			"\tvec4 offset_scale = pbr.transforms_offset_scale[index];\n"
			"\tvec4 texcoord_fit = pbr.transforms_texcoord_fit[index];\n"
			"\tvec2 fit_scale = max( texcoord_fit.yz, vec2( 0.0001 ) );\n"
			"\tfit_alpha = 1.0;\n"
			"\tif ( fit_scale.x < 0.9999 || fit_scale.y < 0.9999 )\n"
			"\t{\n"
			"\t\tvec2 fit_offset = ( vec2( 1.0 ) - fit_scale ) * 0.5;\n"
			"\t\tvec2 fit_min = fit_offset;\n"
			"\t\tvec2 fit_max = fit_offset + fit_scale;\n"
			"\t\tvec2 edge_width = max( fwidth( uv ), vec2( 0.0001 ) );\n"
			"\t\tvec2 min_alpha = smoothstep( fit_min - edge_width, fit_min + edge_width, uv );\n"
			"\t\tvec2 max_alpha = 1.0 - smoothstep( fit_max - edge_width, fit_max + edge_width, uv );\n"
			"\t\tfit_alpha = clamp( min_alpha.x * min_alpha.y * max_alpha.x * max_alpha.y, 0.0, 1.0 );\n"
			"\t\tuv = clamp( ( uv - fit_offset ) / fit_scale, vec2( 0.0 ), vec2( 1.0 ) );\n"
			"\t}\n"
			"\tuv *= offset_scale.zw;\n"
			"\treturn uv + offset_scale.xy;\n"
			"}\n"
			"vec4 sample_base_color()\n"
			"{\n"
			"\tfloat fit_alpha = 1.0;\n"
			"\tvec2 uv = transform_uv( select_uv( pbr.transforms_texcoord_fit[0].x ), 0, fit_alpha );\n"
			"\tvec4 base_sample_color = texture( base_color_texture, uv );\n"
			"\tbase_sample_color *= fit_alpha;\n"
			"\treturn base_sample_color;\n"
			"}\n"
			"vec4 sample_metallic_roughness()\n"
			"{\n"
			"\tfloat fit_alpha = 1.0;\n"
			"\treturn texture( metallic_roughness_texture, transform_uv( select_uv( pbr.transforms_texcoord_fit[1].x ), 1, fit_alpha ) );\n"
			"}\n"
			"vec3 sample_normal_tex()\n"
			"{\n"
			"\tfloat fit_alpha = 1.0;\n"
			"\treturn texture( normal_texture, transform_uv( select_uv( pbr.transforms_texcoord_fit[2].x ), 2, fit_alpha ) ).xyz;\n"
			"}\n"
			"float sample_occlusion()\n"
			"{\n"
			"\tfloat fit_alpha = 1.0;\n"
			"\treturn texture( occlusion_texture, transform_uv( select_uv( pbr.transforms_texcoord_fit[3].x ), 3, fit_alpha ) ).r;\n"
			"}\n"
			"vec3 sample_emissive()\n"
			"{\n"
			"\tfloat fit_alpha = 1.0;\n"
			"\tvec2 uv = transform_uv( select_uv( pbr.transforms_texcoord_fit[4].x ), 4, fit_alpha );\n"
			"\treturn texture( emissive_texture, uv ).rgb * fit_alpha;\n"
			"}\n"
			"float distribution_ggx( vec3 N, vec3 H, float roughness )\n"
			"{\n"
			"\tfloat a = roughness * roughness;\n"
			"\tfloat a2 = a * a;\n"
			"\tfloat NdotH = max( dot( N, H ), 0.0 );\n"
			"\tfloat NdotH2 = NdotH * NdotH;\n"
			"\tfloat denom = ( NdotH2 * ( a2 - 1.0 ) + 1.0 );\n"
			"\treturn a2 / max( PI * denom * denom, 0.0001 );\n"
			"}\n"
			"float geometry_schlick_ggx( float NdotV, float roughness )\n"
			"{\n"
			"\tfloat r = roughness + 1.0;\n"
			"\tfloat k = ( r * r ) / 8.0;\n"
			"\treturn NdotV / max( NdotV * ( 1.0 - k ) + k, 0.0001 );\n"
			"}\n"
			"float geometry_smith( vec3 N, vec3 V, vec3 L, float roughness )\n"
			"{\n"
			"\tfloat ggx1 = geometry_schlick_ggx( max( dot( N, V ), 0.0 ), roughness );\n"
			"\tfloat ggx2 = geometry_schlick_ggx( max( dot( N, L ), 0.0 ), roughness );\n"
			"\treturn ggx1 * ggx2;\n"
			"}\n"
			"vec3 fresnel_schlick( float cosTheta, vec3 F0 )\n"
			"{\n"
			"\treturn F0 + ( 1.0 - F0 ) * pow( 1.0 - cosTheta, 5.0 );\n"
			"}\n"
			"void main()\n"
			"{\n"
			"\tvec4 base_sample = sample_base_color();\n"
			"\tvec4 base_color = vec4( srgb_to_linear( base_sample.rgb ), base_sample.a ) * pbr.base_color_factor * frag_color;\n"
			"\tfloat alpha_mode = pbr.control.y;\n"
			"\tif ( alpha_mode > 0.5 && alpha_mode < 1.5 && base_color.a < pbr.control.x )\n"
			"\t\tdiscard;\n"
			"\tif ( alpha_mode > 1.5 && base_color.a <= 0.001 )\n"
			"\t\tdiscard;\n"
			"\n"
			"\tvec3 N = normalize( frag_view_normal );\n"
			"\tif ( pbr.emissive_factor.w > 0.5 )\n"
			"\t{\n"
			"\t\tvec3 tangent = normalize( frag_view_tangent.xyz );\n"
			"\t\tvec3 bitangent = normalize( cross( N, tangent ) ) * frag_view_tangent.w;\n"
			"\t\tmat3 tbn = mat3( tangent, bitangent, N );\n"
			"\t\tvec3 mapped = sample_normal_tex() * 2.0 - 1.0;\n"
			"\t\tmapped.xy *= pbr.material_params.z;\n"
			"\t\tN = normalize( tbn * mapped );\n"
			"\t}\n"
			"\n"
			"\tvec4 mr_sample = sample_metallic_roughness();\n"
			"\tfloat metallic = clamp( pbr.material_params.x * mr_sample.b, 0.0, 1.0 );\n"
			"\tfloat roughness = clamp( pbr.material_params.y * mr_sample.g, 0.045, 1.0 );\n"
			"\tfloat occlusion = 1.0 + pbr.material_params.w * ( sample_occlusion() - 1.0 );\n"
			"\tvec3 emissive = srgb_to_linear( sample_emissive() ) * pbr.emissive_factor.rgb;\n"
			"\n"
			"\tif ( pbr.control.z > 0.5 )\n"
			"\t{\n"
			"\t\tout_color = vec4( base_color.rgb * occlusion + emissive, base_color.a );\n"
			"\t\treturn;\n"
			"\t}\n"
			"\n"
			"\tvec3 V = normalize( -frag_view_position );\n"
			"\tvec3 F0 = mix( vec3( 0.04 ), base_color.rgb, metallic );\n"
			"\tvec3 Lo = vec3( 0.0 );\n"
			"\tint light_count = int( pbr.control.w + 0.5 );\n"
			"\tfor ( int light_index = 0; light_index < light_count; ++light_index )\n"
			"\t{\n"
			"\t\tLightUniform light = pbr.lights[light_index];\n"
			"\t\tfloat type = light.outer_type.y;\n"
			"\t\tvec3 L = vec3( 0.0 );\n"
			"\t\tfloat attenuation = 1.0;\n"
			"\n"
			"\t\tif ( type < 0.5 )\n"
			"\t\t{\n"
			"\t\t\tL = normalize( -light.direction_inner.xyz );\n"
			"\t\t\tattenuation = light.color_intensity.w;\n"
			"\t\t}\n"
			"\t\telse\n"
			"\t\t{\n"
			"\t\t\tvec3 to_light = light.position_range.xyz - frag_view_position;\n"
			"\t\t\tfloat distance_to_light = length( to_light );\n"
			"\t\t\tif ( distance_to_light <= 0.0001 )\n"
			"\t\t\t\tcontinue;\n"
			"\t\t\tL = to_light / distance_to_light;\n"
			"\t\t\tattenuation = light.color_intensity.w / max( distance_to_light * distance_to_light, 0.0001 );\n"
			"\t\t\tif ( light.position_range.w > 0.0 )\n"
			"\t\t\t{\n"
			"\t\t\t\tfloat range_factor = clamp( 1.0 - distance_to_light / light.position_range.w, 0.0, 1.0 );\n"
			"\t\t\t\tattenuation *= range_factor * range_factor;\n"
			"\t\t\t}\n"
			"\t\t\tif ( type > 1.5 )\n"
			"\t\t\t{\n"
			"\t\t\t\tfloat cone_cos = dot( normalize( -L ), normalize( light.direction_inner.xyz ) );\n"
			"\t\t\t\tfloat cone = smoothstep( light.outer_type.x, light.direction_inner.w, cone_cos );\n"
			"\t\t\t\tattenuation *= cone;\n"
			"\t\t\t}\n"
			"\t\t}\n"
			"\n"
			"\t\tvec3 H = normalize( V + L );\n"
			"\t\tfloat NdotL = max( dot( N, L ), 0.0 );\n"
			"\t\tfloat NdotV = max( dot( N, V ), 0.0 );\n"
			"\t\tif ( NdotL <= 0.0 || NdotV <= 0.0 )\n"
			"\t\t\tcontinue;\n"
			"\n"
			"\t\tfloat D = distribution_ggx( N, H, roughness );\n"
			"\t\tfloat G = geometry_smith( N, V, L, roughness );\n"
			"\t\tvec3 F = fresnel_schlick( max( dot( H, V ), 0.0 ), F0 );\n"
			"\t\tvec3 numerator = D * G * F;\n"
			"\t\tfloat denominator = max( 4.0 * NdotV * NdotL, 0.0001 );\n"
			"\t\tvec3 specular = numerator / denominator;\n"
			"\t\tvec3 kS = F;\n"
			"\t\tvec3 kD = ( vec3( 1.0 ) - kS ) * ( 1.0 - metallic );\n"
			"\t\tvec3 radiance = light.color_intensity.rgb * attenuation;\n"
			"\t\tLo += ( kD * base_color.rgb / PI + specular ) * radiance * NdotL;\n"
			"\t}\n"
			"\n"
			"\tvec3 ambient = pbr.ambient_color.rgb * base_color.rgb * occlusion;\n"
			"\tfloat camera_fill_factor = max( pbr.ambient_color.w, 0.0 ) * 0.01;\n"
			"\tvec3 camera_fill = ( base_color.rgb * 0.8 + vec3( 0.2 ) ) * occlusion * camera_fill_factor;\n"
			"\tout_color = vec4( ambient + camera_fill + Lo * occlusion + emissive, base_color.a );\n"
			"}\n";
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
	m_owns_sdl_video = false;
	m_window_claimed = false;
	m_window = nullptr;
	m_device = nullptr;
	m_vertex_buffer = nullptr;
	m_pbr_instance_buffer = nullptr;
	m_vertex_buffer_size = 0;
	m_pbr_instance_buffer_size = 0;
	m_vertex_buffer_signature = 0;
	m_vertex_shader = nullptr;
	m_alpha_prepass_shader = nullptr;
	m_pbr_vertex_shader = nullptr;
	m_pbr_fragment_shader = nullptr;
	for ( int i = 0; i <= FeBlend::None; ++i )
	{
		m_fragment_shaders[i] = nullptr;
		for ( int z = 0; z < 3; ++z )
			m_blend_pipelines[z][i] = nullptr;
	}
	for ( int z = 0; z < 3; ++z )
		for ( int a = 0; a < 2; ++a )
			for ( int d = 0; d < 2; ++d )
				m_pbr_pipelines[z][a][d] = nullptr;
	m_alpha_prepass_pipeline = nullptr;
	m_linear_sampler = nullptr;
	m_linear_repeat_sampler = nullptr;
	m_linear_mipmap_sampler = nullptr;
	m_linear_mipmap_repeat_sampler = nullptr;
	m_nearest_sampler = nullptr;
	m_nearest_repeat_sampler = nullptr;
	m_nearest_mipmap_sampler = nullptr;
	m_nearest_mipmap_repeat_sampler = nullptr;
	m_white_texture = nullptr;
	m_color_target_texture = nullptr;
	m_color_target_width = 0;
	m_color_target_height = 0;
	m_depth_texture = nullptr;
	m_depth_format = SDL_GPU_TEXTUREFORMAT_INVALID;
	m_depth_width = 0;
	m_depth_height = 0;
	m_anisotropy = 0;
	m_sample_count = SDL_GPU_SAMPLECOUNT_1;
	m_swapchain_format = SDL_GPU_TEXTUREFORMAT_INVALID;
	m_pipeline_attempted = false;
	m_present_disabled = false;
	m_failed_present_frames = 0;
	m_has_submitted_frame = false;
}

FeSdl3GpuContext::~FeSdl3GpuContext()
{
	shutdown();
}

void FeSdl3GpuContext::submit_frame( FeRenderFrame frame )
{
	m_frame = std::move( frame );
	m_has_submitted_frame = true;
	m_prepared_images.clear();
	m_vertex_stream.clear();
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
	clear_geometry_buffers();
}

bool FeSdl3GpuContext::present_blank_frame()
{
	if ( !is_available() )
		return false;

	SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer( m_device );
	if ( !command_buffer )
		return false;

	SDL_GPUTexture *swapchain_texture = nullptr;
	Uint32 swapchain_width = 0;
	Uint32 swapchain_height = 0;
	if ( !SDL_WaitAndAcquireGPUSwapchainTexture( command_buffer, m_window, &swapchain_texture, &swapchain_width, &swapchain_height ) )
	{
		SDL_CancelGPUCommandBuffer( command_buffer );
		return false;
	}

	if ( !swapchain_texture )
	{
		SDL_CancelGPUCommandBuffer( command_buffer );
		return false;
	}

	SDL_GPUColorTargetInfo color_target = {};
	color_target.texture = swapchain_texture;
	color_target.mip_level = 0;
	color_target.layer_or_depth_plane = 0;
	color_target.clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 1.0f };
	color_target.load_op = SDL_GPU_LOADOP_CLEAR;
	color_target.store_op = SDL_GPU_STOREOP_STORE;

	SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass( command_buffer, &color_target, 1, nullptr );
	if ( !render_pass )
	{
		SDL_CancelGPUCommandBuffer( command_buffer );
		return false;
	}

	SDL_EndGPURenderPass( render_pass );
	return SDL_SubmitGPUCommandBuffer( command_buffer );
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
		auto sync_binding = [&]( const void *texture_id,
			int texture_source_type,
			float texture_width,
			float texture_height,
			bool texture_mipmap,
			bool texture_dynamic,
			unsigned long long texture_content_version )
		{
			if ( !texture_id )
				return;

			if ( texture_source_type == FeRenderTextureSourceContainer )
			{
				const FeBaseTextureContainer *container = static_cast<const FeBaseTextureContainer *>( texture_id );
				if ( dynamic_cast<const FeSurfaceTextureContainer *>( container ) != nullptr )
					return;
			}

			TextureEntry &entry = m_textures[ texture_id ];
			const float previous_width = entry.width;
			const float previous_height = entry.height;
			const bool previous_mipmapped = entry.mipmapped;
			const unsigned long long content_version =
				( texture_content_version != 0 )
					? texture_content_version
					: m_frame.frame_number;
			entry.width = texture_width;
			entry.height = texture_height;
			entry.mipmapped = texture_mipmap;
			entry.last_seen_frame = m_frame.frame_number;
			if ( is_available() )
			{
				const bool had_texture = ( entry.gpu_texture != nullptr );
				const bool has_explicit_content_version = ( texture_content_version != 0 );
				const bool needs_upload =
					!had_texture ||
					( previous_width != texture_width ) ||
					( previous_height != texture_height ) ||
					( previous_mipmapped != texture_mipmap ) ||
					( has_explicit_content_version &&
						entry.last_upload_content_version != content_version ) ||
					( texture_dynamic && entry.last_upload_content_version != content_version );

				if ( needs_upload )
				{
					if ( upload_texture( texture_id, texture_source_type, entry ) )
					{
						entry.last_upload_frame = m_frame.frame_number;
						entry.last_upload_content_version = content_version;
					}
				}
			}
		};

		sync_binding(
			image.texture_id,
			image.texture_source_type,
			image.texture_width,
			image.texture_height,
			image.texture_mipmap,
			image.texture_dynamic,
			image.texture_content_version );

		if ( image.geometry_kind == FeRenderGeometryObjectPbr )
		{
			const FeRenderTextureBinding *bindings[] =
			{
				&image.pbr_material.base_color_texture,
				&image.pbr_material.metallic_roughness_texture,
				&image.pbr_material.normal_texture,
				&image.pbr_material.occlusion_texture,
				&image.pbr_material.emissive_texture
			};
			for ( const FeRenderTextureBinding *binding : bindings )
			{
				if ( !binding )
					continue;

				sync_binding(
					binding->texture_id,
					binding->texture_source_type,
					binding->width,
					binding->height,
					binding->mipmap,
					binding->dynamic,
					binding->content_version );
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

void FeSdl3GpuContext::release_geometry_buffer( GeometryBufferEntry &entry )
{
	if ( entry.vertex_buffer )
	{
		SDL_ReleaseGPUBuffer( m_device, entry.vertex_buffer );
		entry.vertex_buffer = nullptr;
		entry.vertex_buffer_size = 0;
		entry.vertex_count = 0;
	}
}

void FeSdl3GpuContext::clear_geometry_buffers()
{
	for ( auto &entry : m_geometry_buffers )
		release_geometry_buffer( entry.second );

	m_geometry_buffers.clear();
}

bool FeSdl3GpuContext::ensure_geometry_vertex_buffer( const PreparedImage &image, SDL_GPUBuffer *&buffer )
{
	buffer = nullptr;
	if ( !image.external_vertex_id || !image.external_vertices || image.vertex_count == 0 )
		return false;

	GeometryBufferEntry &entry = m_geometry_buffers[ image.external_vertex_id ];
	entry.last_seen_frame = m_frame.frame_number;
	if ( !entry.vertex_buffer || entry.vertex_count != image.vertex_count )
	{
		if ( !upload_vertex_buffer( image.external_vertices, image.vertex_count, entry.vertex_buffer, entry.vertex_buffer_size ) )
			return false;

		entry.vertex_count = image.vertex_count;
	}

	buffer = entry.vertex_buffer;
	return buffer != nullptr;
}

void FeSdl3GpuContext::build_prepared_images()
{
	prepare_geometry_batch( m_frame.images, true, m_prepared_images, m_vertex_stream );
}

void FeSdl3GpuContext::prepare_geometry_batch(
	const std::vector<FeRenderGeometry> &geometry,
	bool use_surface_targets,
	std::vector<PreparedImage> &prepared_images,
	std::vector<FeRenderVertex> &vertex_stream )
{
	prepared_images.clear();
	vertex_stream.clear();
	prepared_images.reserve( geometry.size() );
	vertex_stream.reserve( geometry.size() * 6 );

	for ( const FeRenderGeometry &image : geometry )
	{
		PreparedImage prepared = {};
		prepared.geometry = &image;
		prepared.gpu_texture = nullptr;
		for ( SDL_GPUTexture *&texture : prepared.pbr_textures )
			texture = nullptr;
		prepared.external_vertices = nullptr;
		prepared.external_vertex_id = nullptr;
		prepared.first_vertex = vertex_stream.size();
		prepared.vertex_count = image.get_vertex_count();
		prepared.blend_mode = image.blend_mode;
		prepared.zbuffer = image.zbuffer;
		prepared.translucent_depth = geometry_uses_translucent_depth_pipeline( image );
		prepared.object_pbr = ( image.geometry_kind == FeRenderGeometryObjectPbr );
		prepared.texture_repeated = image.texture_repeated;
		prepared.texture_smooth = image.texture_smooth;
		prepared.texture_mipmap = image.texture_mipmap;
		prepared.custom_shader = nullptr;
		prepared.builtin_shader = nullptr;
		prepared.pbr_custom_shader = nullptr;
		prepared.pbr_instance_first = 0;
		prepared.pbr_instance_count = image.has_pbr_instances()
			? static_cast<Uint32>( image.pbr_instances.size() )
			: 1u;
		prepared.pbr_instance_head = true;

		if ( image.textured )
		{
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

		if ( prepared.object_pbr )
		{
			const FeRenderTextureBinding *bindings[] =
			{
				&image.pbr_material.base_color_texture,
				&image.pbr_material.metallic_roughness_texture,
				&image.pbr_material.normal_texture,
				&image.pbr_material.occlusion_texture,
				&image.pbr_material.emissive_texture
			};

			for ( int binding_index = 0; binding_index < 5; ++binding_index )
			{
				const FeRenderTextureBinding &binding = *bindings[binding_index];
				SDL_GPUTexture *binding_texture = nullptr;
				if ( binding.texture_id && use_surface_targets )
				{
					auto surface_it = m_surfaces.find( binding.texture_id );
					if ( surface_it != m_surfaces.end() )
						binding_texture = surface_it->second.color_texture;
				}
				if ( !binding_texture && binding.texture_id )
				{
					auto it = m_textures.find( binding.texture_id );
					binding_texture = ( it != m_textures.end() ) ? it->second.gpu_texture : nullptr;
				}
				prepared.pbr_textures[binding_index] =
					binding_texture ? binding_texture : ( ensure_white_texture() ? m_white_texture : nullptr );
			}

			if ( !prepared.gpu_texture )
				prepared.gpu_texture = ensure_white_texture() ? m_white_texture : nullptr;

			if ( image.pbr_material.artwork_shader )
				prepared.pbr_custom_shader = get_pbr_custom_shader_entry( image );
		}

		if ( !prepared.object_pbr && image.custom_shader )
			prepared.custom_shader = get_custom_shader_entry( image );
		else if ( !prepared.object_pbr && image.textured &&
			( image.blend_mode == FeBlend::Screen ||
				image.blend_mode == FeBlend::Multiply ||
				image.blend_mode == FeBlend::Overlay ||
				image.blend_mode == FeBlend::Premultiplied ) )
			prepared.builtin_shader = get_fast_builtin_shader_entry( image.blend_mode );

		if ( image.geometry_kind == FeRenderGeometryObjectPbr && image.has_external_vertices() )
		{
			prepared.external_vertices = image.external_vertices;
			prepared.external_vertex_id = image.external_vertex_id;
			prepared_images.push_back( prepared );
			continue;
		}

		const FeRenderVertex *vertex_data = image.get_vertex_data();
		const std::size_t vertex_count = image.get_vertex_count();
		for ( std::size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index )
		{
			FeRenderVertex vertex = vertex_data[vertex_index];
			if ( prepared.custom_shader && image.textured && image.texture_width > 0.0f && image.texture_height > 0.0f )
			{
				vertex.u /= image.texture_width;
				vertex.v /= image.texture_height;
			}
			vertex_stream.push_back( vertex );
		}

		prepared_images.push_back( prepared );
	}
}

bool FeSdl3GpuContext::can_instance_pbr_images( const PreparedImage &lhs, const PreparedImage &rhs ) const
{
	if ( !lhs.geometry || !rhs.geometry )
		return false;

	if ( lhs.vertex_count != rhs.vertex_count
		|| lhs.external_vertex_id != rhs.external_vertex_id
		|| lhs.zbuffer != rhs.zbuffer
		|| lhs.translucent_depth != rhs.translucent_depth )
	{
		return false;
	}

	const FeRenderPbrMaterial &lhs_material = lhs.geometry->pbr_material;
	const FeRenderPbrMaterial &rhs_material = rhs.geometry->pbr_material;
	if ( lhs_material.alpha_mode != rhs_material.alpha_mode
		|| lhs_material.unlit != rhs_material.unlit
		|| lhs_material.double_sided != rhs_material.double_sided
		|| lhs_material.artwork_shader != rhs_material.artwork_shader
		|| lhs_material.artwork_shader_emissive != rhs_material.artwork_shader_emissive
		|| lhs_material.metallic_factor != rhs_material.metallic_factor
		|| lhs_material.roughness_factor != rhs_material.roughness_factor
		|| lhs_material.normal_scale != rhs_material.normal_scale
		|| lhs_material.occlusion_strength != rhs_material.occlusion_strength
		|| lhs_material.alpha_cutoff != rhs_material.alpha_cutoff
		|| lhs.geometry->camera_light != rhs.geometry->camera_light
		|| lhs.geometry->light_count != rhs.geometry->light_count )
	{
		return false;
	}

	for ( int i = 0; i < 4; ++i )
	{
		if ( lhs_material.base_color_factor[i] != rhs_material.base_color_factor[i] )
			return false;
	}

	for ( int i = 0; i < 3; ++i )
	{
		if ( lhs_material.emissive_factor[i] != rhs_material.emissive_factor[i]
			|| lhs.geometry->ambient_color[i] != rhs.geometry->ambient_color[i] )
		{
			return false;
		}
	}

	auto binding_matches = []( const FeRenderTextureBinding &a, const FeRenderTextureBinding &b ) -> bool
	{
		return a.texture_id == b.texture_id
			&& a.texture_source_type == b.texture_source_type
			&& a.repeated == b.repeated
			&& a.smooth == b.smooth
			&& a.mipmap == b.mipmap
			&& a.width == b.width
			&& a.height == b.height
			&& a.content_version == b.content_version
			&& a.offset_u == b.offset_u
			&& a.offset_v == b.offset_v
			&& a.scale_u == b.scale_u
			&& a.scale_v == b.scale_v
			&& a.fit_scale_u == b.fit_scale_u
			&& a.fit_scale_v == b.fit_scale_v
			&& a.texcoord_set == b.texcoord_set;
	};

	if ( !binding_matches( lhs_material.base_color_texture, rhs_material.base_color_texture )
		|| !binding_matches( lhs_material.metallic_roughness_texture, rhs_material.metallic_roughness_texture )
		|| !binding_matches( lhs_material.normal_texture, rhs_material.normal_texture )
		|| !binding_matches( lhs_material.occlusion_texture, rhs_material.occlusion_texture )
		|| !binding_matches( lhs_material.emissive_texture, rhs_material.emissive_texture ) )
	{
		return false;
	}

	for ( int i = 0; i < 5; ++i )
	{
		if ( lhs.pbr_textures[i] != rhs.pbr_textures[i] )
			return false;
	}

	for ( int light_index = 0; light_index < lhs.geometry->light_count; ++light_index )
	{
		const FeRenderPbrLight &lhs_light = lhs.geometry->lights[light_index];
		const FeRenderPbrLight &rhs_light = rhs.geometry->lights[light_index];
		if ( lhs_light.type != FeRenderPbrLightDirectional
			|| rhs_light.type != FeRenderPbrLightDirectional
			|| lhs_light.type != rhs_light.type
			|| lhs_light.intensity != rhs_light.intensity
			|| lhs_light.range != rhs_light.range
			|| lhs_light.inner_cone_cos != rhs_light.inner_cone_cos
			|| lhs_light.outer_cone_cos != rhs_light.outer_cone_cos )
		{
			return false;
		}

		for ( int i = 0; i < 3; ++i )
		{
			if ( lhs_light.color[i] != rhs_light.color[i]
				|| lhs_light.direction[i] != rhs_light.direction[i] )
			{
				return false;
			}
		}
	}

	return true;
}

std::uint64_t FeSdl3GpuContext::compute_pbr_instance_batch_hash( const PreparedImage &image ) const
{
	if ( !image.geometry )
		return 0;

	std::uint64_t hash = 1469598103934665603ULL;
	hash = hash_combine_u64( hash, static_cast<std::uint64_t>( image.vertex_count ) );
	hash = hash_combine_u64( hash, reinterpret_cast<std::uint64_t>( image.external_vertex_id ) );
	hash = hash_combine_u64( hash, static_cast<std::uint64_t>( image.zbuffer ? 1 : 0 ) );
	hash = hash_combine_u64( hash, static_cast<std::uint64_t>( image.translucent_depth ? 1 : 0 ) );

	const FeRenderGeometry &geometry = *image.geometry;
	const FeRenderPbrMaterial &material = geometry.pbr_material;
	hash = hash_combine_u64( hash, static_cast<std::uint64_t>( material.alpha_mode ) );
	hash = hash_combine_u64( hash, static_cast<std::uint64_t>( material.unlit ? 1 : 0 ) );
	hash = hash_combine_u64( hash, static_cast<std::uint64_t>( material.double_sided ? 1 : 0 ) );
	hash = hash_combine_u64( hash, reinterpret_cast<std::uint64_t>( material.artwork_shader ) );
	hash = hash_combine_u64( hash, static_cast<std::uint64_t>( material.artwork_shader_emissive ? 1 : 0 ) );
	hash = hash_float_bits( hash, material.metallic_factor );
	hash = hash_float_bits( hash, material.roughness_factor );
	hash = hash_float_bits( hash, material.normal_scale );
	hash = hash_float_bits( hash, material.occlusion_strength );
	hash = hash_float_bits( hash, material.alpha_cutoff );
	hash = hash_float_bits( hash, geometry.camera_light );
	hash = hash_combine_u64( hash, static_cast<std::uint64_t>( geometry.light_count ) );

	for ( int i = 0; i < 4; ++i )
		hash = hash_float_bits( hash, material.base_color_factor[i] );
	for ( int i = 0; i < 3; ++i )
	{
		hash = hash_float_bits( hash, material.emissive_factor[i] );
		hash = hash_float_bits( hash, geometry.ambient_color[i] );
	}

	auto hash_binding = [&]( const FeRenderTextureBinding &binding )
	{
		hash = hash_combine_u64( hash, reinterpret_cast<std::uint64_t>( binding.texture_id ) );
		hash = hash_combine_u64( hash, static_cast<std::uint64_t>( binding.texture_source_type ) );
		hash = hash_combine_u64( hash, static_cast<std::uint64_t>( binding.repeated ? 1 : 0 ) );
		hash = hash_combine_u64( hash, static_cast<std::uint64_t>( binding.smooth ? 1 : 0 ) );
		hash = hash_combine_u64( hash, static_cast<std::uint64_t>( binding.mipmap ? 1 : 0 ) );
		hash = hash_float_bits( hash, binding.width );
		hash = hash_float_bits( hash, binding.height );
		hash = hash_combine_u64( hash, binding.content_version );
		hash = hash_float_bits( hash, binding.offset_u );
		hash = hash_float_bits( hash, binding.offset_v );
		hash = hash_float_bits( hash, binding.scale_u );
		hash = hash_float_bits( hash, binding.scale_v );
		hash = hash_float_bits( hash, binding.fit_scale_u );
		hash = hash_float_bits( hash, binding.fit_scale_v );
		hash = hash_combine_u64( hash, static_cast<std::uint64_t>( binding.texcoord_set ) );
	};

	hash_binding( material.base_color_texture );
	hash_binding( material.metallic_roughness_texture );
	hash_binding( material.normal_texture );
	hash_binding( material.occlusion_texture );
	hash_binding( material.emissive_texture );

	for ( int i = 0; i < 5; ++i )
		hash = hash_combine_u64( hash, reinterpret_cast<std::uint64_t>( image.pbr_textures[i] ) );

	for ( int light_index = 0; light_index < geometry.light_count; ++light_index )
	{
		const FeRenderPbrLight &light = geometry.lights[light_index];
		hash = hash_combine_u64( hash, static_cast<std::uint64_t>( light.type ) );
		hash = hash_float_bits( hash, light.intensity );
		hash = hash_float_bits( hash, light.range );
		hash = hash_float_bits( hash, light.inner_cone_cos );
		hash = hash_float_bits( hash, light.outer_cone_cos );
		for ( int i = 0; i < 3; ++i )
		{
			hash = hash_float_bits( hash, light.color[i] );
			hash = hash_float_bits( hash, light.direction[i] );
		}
	}

	return hash;
}

void FeSdl3GpuContext::build_pbr_instance_batches(
	std::vector<PreparedImage> &prepared_images,
	std::vector<PbrInstanceEntry> &instance_data ) const
{
	instance_data.clear();
	instance_data.reserve( prepared_images.size() );

	for ( PreparedImage &image : prepared_images )
	{
		image.pbr_instance_first = 0;
		image.pbr_instance_count = 0;
		image.pbr_instance_head = false;
	}

	auto append_instance = [&]( PreparedImage &image )
	{
		if ( !image.geometry )
			return;

		PbrInstanceEntry instance = {};
		std::memcpy( instance.model, image.geometry->model_matrix, sizeof( instance.model ) );
		for ( int row = 0; row < 3; ++row )
		{
			instance.normal_matrix[row][0] = image.geometry->normal_matrix[row * 3 + 0];
			instance.normal_matrix[row][1] = image.geometry->normal_matrix[row * 3 + 1];
			instance.normal_matrix[row][2] = image.geometry->normal_matrix[row * 3 + 2];
			instance.normal_matrix[row][3] = 0.0f;
		}

		image.pbr_instance_first = static_cast<Uint32>( instance_data.size() );
		image.pbr_instance_count = 1;
		image.pbr_instance_head = true;
		instance_data.push_back( instance );
	};

	for ( std::size_t image_index = 0; image_index < prepared_images.size(); ++image_index )
	{
		PreparedImage &image = prepared_images[image_index];
		if ( !image.object_pbr || !image.geometry || image.pbr_instance_head || image.pbr_instance_count != 0 )
			continue;

		if ( image.geometry->has_pbr_instances() )
		{
			image.pbr_instance_first = static_cast<Uint32>( instance_data.size() );
			image.pbr_instance_count = static_cast<Uint32>( image.geometry->pbr_instances.size() );
			image.pbr_instance_head = true;
			for ( const FeRenderPbrInstance &instance : image.geometry->pbr_instances )
			{
				PbrInstanceEntry entry = {};
				std::memcpy( entry.model, instance.model_matrix, sizeof( entry.model ) );
				for ( int row = 0; row < 3; ++row )
				{
					entry.normal_matrix[row][0] = instance.normal_matrix[row * 3 + 0];
					entry.normal_matrix[row][1] = instance.normal_matrix[row * 3 + 1];
					entry.normal_matrix[row][2] = instance.normal_matrix[row * 3 + 2];
					entry.normal_matrix[row][3] = 0.0f;
				}
				instance_data.push_back( entry );
			}
			continue;
		}

		const bool batchable_object =
			image.zbuffer &&
			!image.translucent_depth &&
			( image.geometry->pbr_material.alpha_mode != FeRenderPbrAlphaBlend ) &&
			( image.external_vertex_id != nullptr ) &&
			( image.external_vertices != nullptr );
		if ( !batchable_object )
		{
			append_instance( image );
			continue;
		}

		std::size_t run_end = image_index;
		while ( run_end < prepared_images.size() )
		{
			const PreparedImage &run_image = prepared_images[run_end];
			if ( !run_image.object_pbr
				|| !run_image.geometry
				|| !run_image.zbuffer
				|| run_image.translucent_depth
				|| run_image.geometry->pbr_material.alpha_mode == FeRenderPbrAlphaBlend
				|| !run_image.external_vertex_id
				|| !run_image.external_vertices )
			{
				break;
			}

			++run_end;
		}

		struct BatchRange
		{
			std::uint64_t hash;
			std::vector<std::size_t> indices;
		};

		std::vector<BatchRange> batches;
		batches.reserve( run_end - image_index );
		std::unordered_map<std::uint64_t, std::vector<std::size_t>> batch_lookup;
		batch_lookup.reserve( run_end - image_index );
		for ( std::size_t run_index = image_index; run_index < run_end; ++run_index )
		{
			PreparedImage &run_image = prepared_images[run_index];
			const std::uint64_t batch_hash = compute_pbr_instance_batch_hash( run_image );
			bool appended = false;
			auto lookup_it = batch_lookup.find( batch_hash );
			if ( lookup_it != batch_lookup.end() )
			{
				for ( std::size_t batch_lookup_index : lookup_it->second )
				{
					BatchRange &batch = batches[batch_lookup_index];
					if ( batch.indices.empty() )
						continue;

					if ( can_instance_pbr_images( prepared_images[batch.indices[0]], run_image ) )
					{
						batch.indices.push_back( run_index );
						appended = true;
						break;
					}
				}
			}

			if ( !appended )
			{
				BatchRange batch;
				batch.hash = batch_hash;
				batch.indices.push_back( run_index );
				batches.push_back( std::move( batch ) );
				batch_lookup[batch_hash].push_back( batches.size() - 1 );
			}
		}

		for ( const BatchRange &batch : batches )
		{
			if ( batch.indices.empty() )
				continue;

			PreparedImage &head = prepared_images[batch.indices[0]];
			head.pbr_instance_first = static_cast<Uint32>( instance_data.size() );
			head.pbr_instance_count = static_cast<Uint32>( batch.indices.size() );
			head.pbr_instance_head = true;

			for ( std::size_t batch_pos = 0; batch_pos < batch.indices.size(); ++batch_pos )
			{
				PreparedImage &batch_image = prepared_images[batch.indices[batch_pos]];
				PbrInstanceEntry instance = {};
				std::memcpy( instance.model, batch_image.geometry->model_matrix, sizeof( instance.model ) );
				for ( int row = 0; row < 3; ++row )
				{
					instance.normal_matrix[row][0] = batch_image.geometry->normal_matrix[row * 3 + 0];
					instance.normal_matrix[row][1] = batch_image.geometry->normal_matrix[row * 3 + 1];
					instance.normal_matrix[row][2] = batch_image.geometry->normal_matrix[row * 3 + 2];
					instance.normal_matrix[row][3] = 0.0f;
				}
				instance_data.push_back( instance );

				if ( batch_pos == 0 )
					continue;

				batch_image.pbr_instance_first = head.pbr_instance_first;
				batch_image.pbr_instance_count = 0;
				batch_image.pbr_instance_head = false;
			}
		}

		image_index = run_end - 1;
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
		FeLog() << "SDL: Could not capture GPU frame: unsupported GPU texture format." << std::endl;
		return false;
	}

	const bool sample_count_changed = update_sample_count( target_format );
	const bool anisotropy_changed = update_anisotropy();

	if ( sample_count_changed || anisotropy_changed || ( m_swapchain_format != target_format ) || !m_blend_pipelines[ 0 ][ FeBlend::Alpha ] )
	{
		release_surfaces();
		release_custom_shaders();
		release_image_pipeline();
		release_pbr_pipeline();
		release_color_target();
		release_depth_target();
		m_swapchain_format = target_format;
		m_pipeline_attempted = false;
	}

	if ( !ensure_depth_target( width, height ) )
		return false;

	if ( !m_pipeline_attempted )
	{
		initialize_image_pipeline( target_format );
		initialize_pbr_pipeline( target_format );
		m_pipeline_attempted = true;
	}

	if ( !m_blend_pipelines[ 0 ][ FeBlend::Alpha ] || !m_pbr_pipelines[ 0 ][ 0 ][ 0 ] )
		return false;

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

	SDL_GPUTexture *msaa_color_texture = nullptr;
	if ( uses_multisampling() )
	{
		SDL_GPUTextureCreateInfo msaa_texture_info = texture_info;
		msaa_texture_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
		msaa_texture_info.sample_count = m_sample_count;

		msaa_color_texture = SDL_CreateGPUTexture( m_device, &msaa_texture_info );
		if ( !msaa_color_texture )
		{
			SDL_ReleaseGPUTexture( m_device, color_texture );
			return false;
		}
	}

	SDL_GPUTransferBufferCreateInfo transfer_info = {};
	transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
	transfer_info.size = static_cast<Uint32>( width * height * 4 );

	SDL_GPUTransferBuffer *transfer_buffer = SDL_CreateGPUTransferBuffer( m_device, &transfer_info );
	if ( !transfer_buffer )
	{
		if ( msaa_color_texture )
			SDL_ReleaseGPUTexture( m_device, msaa_color_texture );
		SDL_ReleaseGPUTexture( m_device, color_texture );
		return false;
	}

	SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer( m_device );
	if ( !command_buffer )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		if ( msaa_color_texture )
			SDL_ReleaseGPUTexture( m_device, msaa_color_texture );
		SDL_ReleaseGPUTexture( m_device, color_texture );
		return false;
	}

	std::vector<SDL_GPUBuffer *> temporary_pbr_buffers;
	auto release_temporary_pbr_buffers = [&]()
	{
		for ( SDL_GPUBuffer *buffer : temporary_pbr_buffers )
			SDL_ReleaseGPUBuffer( m_device, buffer );

		temporary_pbr_buffers.clear();
	};

	if ( !render_surface_frames( command_buffer, &temporary_pbr_buffers ) )
	{
		release_temporary_pbr_buffers();
		SDL_CancelGPUCommandBuffer( command_buffer );
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		if ( msaa_color_texture )
			SDL_ReleaseGPUTexture( m_device, msaa_color_texture );
		SDL_ReleaseGPUTexture( m_device, color_texture );
		return false;
	}

	build_prepared_images();
	std::vector<PbrInstanceEntry> pbr_instance_data;
	build_pbr_instance_batches( m_prepared_images, pbr_instance_data );
	if ( !pbr_instance_data.empty() )
	{
		if ( !upload_gpu_vertex_buffer_data(
			command_buffer,
			pbr_instance_data.data(),
			static_cast<Uint32>( pbr_instance_data.size() * sizeof( PbrInstanceEntry ) ),
			m_pbr_instance_buffer,
			m_pbr_instance_buffer_size ) )
		{
			release_temporary_pbr_buffers();
			SDL_CancelGPUCommandBuffer( command_buffer );
			SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
			if ( msaa_color_texture )
				SDL_ReleaseGPUTexture( m_device, msaa_color_texture );
			SDL_ReleaseGPUTexture( m_device, color_texture );
			return false;
		}
	}
	else
		release_pbr_instance_buffer();

	SDL_GPUColorTargetInfo color_target = {};
	color_target.texture = uses_multisampling() ? msaa_color_texture : color_texture;
	color_target.mip_level = 0;
	color_target.layer_or_depth_plane = 0;
	color_target.clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 1.0f };
	color_target.load_op = SDL_GPU_LOADOP_CLEAR;
	color_target.store_op = uses_multisampling() ? SDL_GPU_STOREOP_RESOLVE : SDL_GPU_STOREOP_STORE;
	color_target.resolve_texture = uses_multisampling() ? color_texture : nullptr;
	color_target.resolve_mip_level = 0;
	color_target.resolve_layer = 0;

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
		release_temporary_pbr_buffers();
		SDL_CancelGPUCommandBuffer( command_buffer );
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		if ( msaa_color_texture )
			SDL_ReleaseGPUTexture( m_device, msaa_color_texture );
		SDL_ReleaseGPUTexture( m_device, color_texture );
		return false;
	}

	bool drew_anything = false;
	if ( !m_frame.images.empty() )
	{
		if ( !render_prepared_geometry_batch(
				render_pass,
				command_buffer,
				m_frame.camera,
				width,
				height,
				m_prepared_images,
				m_vertex_stream,
				&m_vertex_buffer,
				&m_vertex_buffer_size,
				&m_vertex_buffer_signature,
				true,
				&temporary_pbr_buffers,
				drew_anything ) )
		{
			SDL_EndGPURenderPass( render_pass );
			release_temporary_pbr_buffers();
			SDL_CancelGPUCommandBuffer( command_buffer );
			SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
			if ( msaa_color_texture )
				SDL_ReleaseGPUTexture( m_device, msaa_color_texture );
			SDL_ReleaseGPUTexture( m_device, color_texture );
			return false;
		}
	}

	SDL_EndGPURenderPass( render_pass );

	SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass( command_buffer );
	if ( !copy_pass )
	{
		release_temporary_pbr_buffers();
		SDL_CancelGPUCommandBuffer( command_buffer );
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		if ( msaa_color_texture )
			SDL_ReleaseGPUTexture( m_device, msaa_color_texture );
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
		release_temporary_pbr_buffers();
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		if ( msaa_color_texture )
			SDL_ReleaseGPUTexture( m_device, msaa_color_texture );
		SDL_ReleaseGPUTexture( m_device, color_texture );
		return false;
	}
	release_temporary_pbr_buffers();

	if ( !SDL_WaitForGPUIdle( m_device ) )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		if ( msaa_color_texture )
			SDL_ReleaseGPUTexture( m_device, msaa_color_texture );
		SDL_ReleaseGPUTexture( m_device, color_texture );
		return false;
	}

	const std::uint8_t *mapped = static_cast<const std::uint8_t *>( SDL_MapGPUTransferBuffer( m_device, transfer_buffer, false ) );
	if ( !mapped )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		if ( msaa_color_texture )
			SDL_ReleaseGPUTexture( m_device, msaa_color_texture );
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
	if ( msaa_color_texture )
		SDL_ReleaseGPUTexture( m_device, msaa_color_texture );
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
#if !defined( SDL_PLATFORM_WINDOWS )
		return absolute_path( clean_path( std::string( FE_DEFAULT_CFG_PATH ) + FE_CACHE_SUBDIR ) );
#else
		return join_path( get_base_path(), FE_CACHE_SUBDIR );
#endif
	}

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

	struct FeSdl3GpuPbrVertexUniforms
	{
		float projection[16];
		float viewport_width;
		float viewport_height;
		float plane_distance;
		float reserved;
	};

	struct FeSdl3GpuPbrLightUniform
	{
		float color_intensity[4];
		float position_range[4];
		float direction_inner[4];
		float outer_type[4];
	};

	struct FeSdl3GpuPbrFragmentUniforms
	{
		float base_color_factor[4];
		float emissive_factor[4];
		float material_params[4];
		float control[4];
		float ambient_color[4];
		float artwork_control[4];
		float transforms_offset_scale[5][4];
		float transforms_texcoord_fit[5][4];
		FeSdl3GpuPbrLightUniform lights[4];
	};

	SDL_GPUShaderFormat get_default_shader_formats()
	{
		SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_SPIRV;

#if defined( SDL_PLATFORM_WINDOWS )
		formats |= SDL_GPU_SHADERFORMAT_DXIL;
#elif defined( SDL_PLATFORM_MACOS )
		formats |= SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB;
#endif

		return formats;
	}

	const char *get_default_driver_name( const char *driver_name )
	{
		if ( driver_name && driver_name[0] )
			return driver_name;

#if defined( SDL_PLATFORM_WINDOWS )
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

	bool write_binary_file( const std::string &path, const std::vector<Uint8> &code )
	{
		if ( code.empty() )
			return false;

		std::ofstream stream( path.c_str(), std::ios::binary | std::ios::trunc );
		if ( !stream )
			return false;

		stream.write( reinterpret_cast<const char *>( code.data() ), static_cast<std::streamsize>( code.size() ) );
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

		std::string shader_name = path_filename( source_id );
		if ( shader_name.empty() )
			shader_name = source_id;

		const std::string cache_name =
			sanitize_filename( shader_name ) + "-" + std::to_string( std::hash<std::string>{}( translated_source ) );
		const std::string stage_name = vertex_stage ? "vert" : "frag";
		const std::string spirv_path = join_path( cache_root, cache_name + "." + stage_name + ".spv" );
		bool compiled = false;
		if ( !file_exists( spirv_path ) || !read_binary_file( spirv_path, blob.code ) || blob.code.empty() )
		{
			std::string diagnostics;
			blob.code.clear();
			compiled = true;

			if ( !fe_glslang_compile_to_spirv( shader_name, translated_source, vertex_stage, blob.code, diagnostics ) || blob.code.empty() )
			{
				FeLog() << "SDL: shader_compile: error " << shader_name << std::endl;
				if ( !diagnostics.empty() )
					FeLog() << "SDL: " << diagnostics << std::endl;
				cache[ cache_key ].compile_failed = true;
				return false;
			}

			if ( !diagnostics.empty() )
				FeDebug() << "SDL: " << diagnostics << std::endl;

			write_binary_file( spirv_path, blob.code );
		}

		if ( compiled )
			FeDebug() << "SDL: shader_compile: ok " << shader_name << std::endl;

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
		m_failed_present_frames++;
		return false;
	}

	sync_textures( overlay_geometry );

	SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer( m_device );
	if ( !command_buffer )
	{
		m_failed_present_frames++;
		return false;
	}

	std::vector<SDL_GPUBuffer *> temporary_pbr_buffers;
	auto release_temporary_pbr_buffers = [&]()
	{
		for ( SDL_GPUBuffer *buffer : temporary_pbr_buffers )
			SDL_ReleaseGPUBuffer( m_device, buffer );

		temporary_pbr_buffers.clear();
	};

	SDL_GPUTexture *swapchain_texture = nullptr;
	Uint32 swapchain_width = 0;
	Uint32 swapchain_height = 0;
	if ( !SDL_WaitAndAcquireGPUSwapchainTexture( command_buffer, m_window, &swapchain_texture, &swapchain_width, &swapchain_height ) )
	{
		release_temporary_pbr_buffers();
		m_failed_present_frames++;
		return false;
	}

	if ( !swapchain_texture )
	{
		release_temporary_pbr_buffers();
		m_failed_present_frames++;
		return false;
	}

	m_frame_stats.acquired_swapchain = true;
	m_frame_stats.last_viewport_width = static_cast<int>( swapchain_width );
	m_frame_stats.last_viewport_height = static_cast<int>( swapchain_height );
	const SDL_GPUTextureFormat swapchain_format = SDL_GetGPUSwapchainTextureFormat( m_device, m_window );
	const bool sample_count_changed = update_sample_count( swapchain_format );
	const bool anisotropy_changed = update_anisotropy();

	if ( sample_count_changed || anisotropy_changed || ( m_swapchain_format != swapchain_format ) || !m_blend_pipelines[0][FeBlend::Alpha] )
	{
		release_surfaces();
		release_custom_shaders();
		release_image_pipeline();
		release_pbr_pipeline();
		release_color_target();
		release_depth_target();
		m_swapchain_format = swapchain_format;
		m_pipeline_attempted = false;
	}

	m_frame_stats.depth_ready = ensure_depth_target( static_cast<int>( swapchain_width ), static_cast<int>( swapchain_height ) )
		&& ensure_color_target( static_cast<int>( swapchain_width ), static_cast<int>( swapchain_height ) );
	if ( !m_frame_stats.depth_ready )
	{
		release_temporary_pbr_buffers();
		m_failed_present_frames++;
		return false;
	}

	if ( !m_pipeline_attempted )
	{
		initialize_image_pipeline( swapchain_format );
		initialize_pbr_pipeline( swapchain_format );
		m_pipeline_attempted = true;
	}

	m_frame_stats.pipeline_ready =
		( m_blend_pipelines[0][FeBlend::Alpha] != nullptr )
		&& ( m_pbr_pipelines[0][0][0] != nullptr );

	if ( m_frame_stats.pipeline_ready && !render_surface_frames( command_buffer, &temporary_pbr_buffers ) )
	{
		release_temporary_pbr_buffers();
		m_failed_present_frames++;
		return false;
	}

	SDL_GPUColorTargetInfo color_target = {};
	color_target.texture = uses_multisampling() ? m_color_target_texture : swapchain_texture;
	color_target.mip_level = 0;
	color_target.layer_or_depth_plane = 0;
	color_target.clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 1.0f };
	color_target.load_op = SDL_GPU_LOADOP_CLEAR;
	color_target.store_op = uses_multisampling() ? SDL_GPU_STOREOP_RESOLVE : SDL_GPU_STOREOP_STORE;
	color_target.resolve_texture = uses_multisampling() ? swapchain_texture : nullptr;
	color_target.resolve_mip_level = 0;
	color_target.resolve_layer = 0;

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
		release_temporary_pbr_buffers();
		m_failed_present_frames++;
		return false;
	}

	if ( m_blend_pipelines[0][FeBlend::Alpha] && !m_frame.images.empty() )
	{
		build_prepared_images();
		std::vector<PbrInstanceEntry> pbr_instance_data;
		build_pbr_instance_batches( m_prepared_images, pbr_instance_data );
		if ( !pbr_instance_data.empty() )
		{
			if ( !upload_gpu_vertex_buffer_data(
				command_buffer,
				pbr_instance_data.data(),
				static_cast<Uint32>( pbr_instance_data.size() * sizeof( PbrInstanceEntry ) ),
				m_pbr_instance_buffer,
				m_pbr_instance_buffer_size ) )
			{
				SDL_EndGPURenderPass( render_pass );
				release_temporary_pbr_buffers();
				m_failed_present_frames++;
				return false;
			}
		}
		else
			release_pbr_instance_buffer();

		if ( !render_prepared_geometry_batch(
				render_pass,
				command_buffer,
				m_frame.camera,
				m_frame.viewport_width,
				m_frame.viewport_height,
				m_prepared_images,
				m_vertex_stream,
				&m_vertex_buffer,
				&m_vertex_buffer_size,
				&m_vertex_buffer_signature,
				true,
				&temporary_pbr_buffers,
				m_frame_stats.draw_ready ) )
		{
			SDL_EndGPURenderPass( render_pass );
			release_temporary_pbr_buffers();
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
				false,
				nullptr,
				nullptr,
				nullptr,
				&temporary_pbr_buffers,
				overlay_drew ) )
		{
			SDL_EndGPURenderPass( render_pass );
			release_temporary_pbr_buffers();
			m_failed_present_frames++;
			return false;
		}
		m_frame_stats.draw_ready = m_frame_stats.draw_ready || overlay_drew;
	}

	SDL_EndGPURenderPass( render_pass );

	if ( !SDL_SubmitGPUCommandBuffer( command_buffer ) )
	{
		release_temporary_pbr_buffers();
		m_failed_present_frames++;
		return false;
	}
	release_temporary_pbr_buffers();

	m_frame_stats.executed = true;
	m_frame_stats.submitted_frames++;
	if ( m_frame_stats.pipeline_ready && m_frame_stats.draw_ready )
		m_failed_present_frames = 0;
	if ( !m_frame_stats.draw_ready )
	{
		if ( has_frame_content() )
			m_failed_present_frames++;
	}

	if ( m_failed_present_frames >= 3 )
		m_present_disabled = true;
	return true;
}

bool FeSdl3GpuContext::initialize( bool debug_mode, const char *driver_name )
{
	if ( m_device )
		return true;

	if ( !ensure_video_subsystem() )
		return false;

	const char *preferred_driver = get_default_driver_name( driver_name );
	const SDL_GPUShaderFormat shader_formats = get_default_shader_formats();
	m_device = SDL_CreateGPUDevice( shader_formats, debug_mode, preferred_driver );
	if ( !m_device && preferred_driver )
	{
		const std::string message = std::string( "initialize: preferred GPU driver failed: " ) + preferred_driver;
		FeDebug() << "SDL: " << message << std::endl;
		m_device = SDL_CreateGPUDevice( shader_formats, debug_mode, nullptr );
	}
	if ( !m_device )
		FeLog() << "SDL: initialize: SDL_CreateGPUDevice failed" << std::endl;
	else
	{
		std::ostringstream stream;
		stream << "initialize: SDL_CreateGPUDevice succeeded";
		if ( preferred_driver )
			stream << " preferred_driver=" << preferred_driver;
		stream << " shader_formats=" << static_cast<unsigned int>( SDL_GetGPUShaderFormats( m_device ) );
		FeDebug() << "SDL: " << stream.str() << std::endl;
	}
	return ( m_device != nullptr );
}

bool FeSdl3GpuContext::ensure_video_subsystem()
{
	if ( m_sdl_ready )
		return true;

	if (( SDL_WasInit( SDL_INIT_VIDEO ) & SDL_INIT_VIDEO ) != 0 )
	{
		m_sdl_ready = true;
		m_owns_sdl_video = false;
		return true;
	}

	if ( !SDL_InitSubSystem( SDL_INIT_VIDEO ) )
	{
		FeLog() << "SDL: WARNING: SDL3 video initialization failed: " << SDL_GetError() << std::endl;
		return false;
	}

	m_sdl_ready = true;
	m_owns_sdl_video = true;
	return true;
}

void FeSdl3GpuContext::shutdown()
{
	release_window();
	release_surfaces();
	clear_textures();
	clear_geometry_buffers();
	release_white_texture();
	release_vertex_buffer();
	release_pbr_instance_buffer();
	release_color_target();
	release_depth_target();
	release_custom_shaders();
	release_image_pipeline();
	release_pbr_pipeline();

	if ( m_device )
	{
		SDL_DestroyGPUDevice( m_device );
		m_device = nullptr;
	}

	if ( m_sdl_ready && m_owns_sdl_video )
	{
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
	}

	m_sdl_ready = false;
	m_owns_sdl_video = false;
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
#if defined( SDL_PLATFORM_WINDOWS )
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
		FeLog() << "SDL: wrap_native_window: SDL_ClaimWindowForGPUDevice failed" << std::endl;
		return false;
	}

	FeDebug() << "SDL: wrap_native_window: success" << std::endl;
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
		FeLog() << "SDL: claim_window: SDL_ClaimWindowForGPUDevice failed" << std::endl;
		return false;
	}

	FeDebug() << "SDL: claim_window: success" << std::endl;
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

void FeSdl3GpuContext::release_pbr_instance_buffer()
{
	if ( m_pbr_instance_buffer )
	{
		SDL_ReleaseGPUBuffer( m_device, m_pbr_instance_buffer );
		m_pbr_instance_buffer = nullptr;
	}

	m_pbr_instance_buffer_size = 0;
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

	if ( m_linear_mipmap_sampler )
	{
		SDL_ReleaseGPUSampler( m_device, m_linear_mipmap_sampler );
		m_linear_mipmap_sampler = nullptr;
	}

	if ( m_linear_mipmap_repeat_sampler )
	{
		SDL_ReleaseGPUSampler( m_device, m_linear_mipmap_repeat_sampler );
		m_linear_mipmap_repeat_sampler = nullptr;
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

	if ( m_nearest_mipmap_sampler )
	{
		SDL_ReleaseGPUSampler( m_device, m_nearest_mipmap_sampler );
		m_nearest_mipmap_sampler = nullptr;
	}

	if ( m_nearest_mipmap_repeat_sampler )
	{
		SDL_ReleaseGPUSampler( m_device, m_nearest_mipmap_repeat_sampler );
		m_nearest_mipmap_repeat_sampler = nullptr;
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

void FeSdl3GpuContext::release_pbr_pipeline()
{
	release_pbr_custom_shaders();

	for ( int z = 0; z < 3; ++z )
	{
		for ( int blend = 0; blend < 2; ++blend )
		{
			for ( int sided = 0; sided < 2; ++sided )
			{
				if ( m_pbr_pipelines[z][blend][sided] )
				{
					SDL_ReleaseGPUGraphicsPipeline( m_device, m_pbr_pipelines[z][blend][sided] );
					m_pbr_pipelines[z][blend][sided] = nullptr;
				}
			}
		}
	}

	if ( m_pbr_fragment_shader )
	{
		SDL_ReleaseGPUShader( m_device, m_pbr_fragment_shader );
		m_pbr_fragment_shader = nullptr;
	}

	if ( m_pbr_vertex_shader )
	{
		SDL_ReleaseGPUShader( m_device, m_pbr_vertex_shader );
		m_pbr_vertex_shader = nullptr;
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

void FeSdl3GpuContext::release_pbr_custom_shader( PbrCustomShaderEntry &entry )
{
	for ( int depth_mode = 0; depth_mode < 3; ++depth_mode )
	{
		for ( int blend_mode = 0; blend_mode < 2; ++blend_mode )
		{
			for ( int sided = 0; sided < 2; ++sided )
			{
				if ( entry.pbr_pipelines[depth_mode][blend_mode][sided] )
				{
					SDL_ReleaseGPUGraphicsPipeline( m_device, entry.pbr_pipelines[depth_mode][blend_mode][sided] );
					entry.pbr_pipelines[depth_mode][blend_mode][sided] = nullptr;
				}
			}
		}
	}

	if ( entry.fragment_shader )
	{
		SDL_ReleaseGPUShader( m_device, entry.fragment_shader );
		entry.fragment_shader = nullptr;
	}

	entry.fragment_uniforms.clear();
	entry.fragment_samplers.clear();
	entry.compile_failed = false;
	entry.source_stamp = 0;
	entry.source_path.clear();
}

void FeSdl3GpuContext::release_custom_shaders()
{
	for ( auto &entry : m_custom_shaders )
		release_custom_shader( entry.second );

	m_custom_shaders.clear();
	m_custom_shader_sources.clear();
}

void FeSdl3GpuContext::release_pbr_custom_shaders()
{
	for ( auto &entry : m_pbr_custom_shaders )
		release_pbr_custom_shader( entry.second );

	m_pbr_custom_shaders.clear();
}

int FeSdl3GpuContext::get_requested_anisotropy() const
{
	if ( m_frame.anisotropic >= 2 )
		return std::min( m_frame.anisotropic, 16 );

	return 0;
}

bool FeSdl3GpuContext::update_anisotropy()
{
	const int anisotropy = get_requested_anisotropy();
	if ( m_anisotropy == anisotropy )
		return false;

	m_anisotropy = anisotropy;
	return true;
}

SDL_GPUSampler *FeSdl3GpuContext::create_sampler(
	SDL_GPUFilter filter,
	SDL_GPUSamplerMipmapMode mipmap_mode,
	SDL_GPUSamplerAddressMode address_mode,
	bool mipmapped,
	bool smooth )
{
	SDL_GPUSamplerCreateInfo sampler_info = {};
	sampler_info.min_filter = filter;
	sampler_info.mag_filter = filter;
	sampler_info.mipmap_mode = mipmapped ? mipmap_mode : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
	sampler_info.address_mode_u = address_mode;
	sampler_info.address_mode_v = address_mode;
	sampler_info.address_mode_w = address_mode;
	sampler_info.mip_lod_bias = 0.0f;
	sampler_info.max_anisotropy = 1.0f;
	sampler_info.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
	sampler_info.min_lod = 0.0f;
	sampler_info.max_lod = mipmapped ? 1000.0f : 0.0f;
	sampler_info.enable_anisotropy = false;
	sampler_info.enable_compare = false;
	sampler_info.padding1 = 0;
	sampler_info.padding2 = 0;
	sampler_info.props = 0;

	if ( smooth && mipmapped && m_anisotropy > 1 )
	{
		for ( int anisotropy = m_anisotropy; anisotropy >= 2; anisotropy /= 2 )
		{
			sampler_info.enable_anisotropy = true;
			sampler_info.max_anisotropy = static_cast<float>( anisotropy );
			SDL_GPUSampler *sampler = SDL_CreateGPUSampler( m_device, &sampler_info );
			if ( sampler )
				return sampler;
		}

		sampler_info.enable_anisotropy = false;
		sampler_info.max_anisotropy = 1.0f;
	}

	return SDL_CreateGPUSampler( m_device, &sampler_info );
}

SDL_GPUSampler *FeSdl3GpuContext::get_image_sampler( bool smooth, bool repeated, bool mipmapped ) const
{
	if ( smooth )
	{
		if ( mipmapped )
			return repeated ? m_linear_mipmap_repeat_sampler : m_linear_mipmap_sampler;

		return repeated ? m_linear_repeat_sampler : m_linear_sampler;
	}

	if ( mipmapped )
		return repeated ? m_nearest_mipmap_repeat_sampler : m_nearest_mipmap_sampler;

	return repeated ? m_nearest_repeat_sampler : m_nearest_sampler;
}

SDL_GPUSampleCount FeSdl3GpuContext::get_requested_sample_count() const
{
	switch ( m_frame.antialiasing )
	{
	case 8:
		return SDL_GPU_SAMPLECOUNT_8;
	case 4:
		return SDL_GPU_SAMPLECOUNT_4;
	case 2:
		return SDL_GPU_SAMPLECOUNT_2;
	default:
		return SDL_GPU_SAMPLECOUNT_1;
	}
}

SDL_GPUSampleCount FeSdl3GpuContext::pick_sample_count( SDL_GPUTextureFormat swapchain_format ) const
{
	if ( !m_device || swapchain_format == SDL_GPU_TEXTUREFORMAT_INVALID )
		return SDL_GPU_SAMPLECOUNT_1;

	SDL_GPUSampleCount sample_count = get_requested_sample_count();
	while ( sample_count != SDL_GPU_SAMPLECOUNT_1 )
	{
		if ( SDL_GPUTextureSupportsSampleCount( m_device, swapchain_format, sample_count )
			&& SDL_GPUTextureSupportsSampleCount( m_device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, sample_count )
			&& SDL_GPUTextureSupportsSampleCount( m_device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, sample_count ) )
		{
			return sample_count;
		}

		sample_count =
			( sample_count == SDL_GPU_SAMPLECOUNT_8 ) ? SDL_GPU_SAMPLECOUNT_4 :
			( sample_count == SDL_GPU_SAMPLECOUNT_4 ) ? SDL_GPU_SAMPLECOUNT_2 :
			SDL_GPU_SAMPLECOUNT_1;
	}

	return SDL_GPU_SAMPLECOUNT_1;
}

bool FeSdl3GpuContext::update_sample_count( SDL_GPUTextureFormat swapchain_format )
{
	const SDL_GPUSampleCount sample_count = pick_sample_count( swapchain_format );
	if ( m_sample_count == sample_count )
		return false;

	m_sample_count = sample_count;
	return true;
}

bool FeSdl3GpuContext::uses_multisampling() const
{
	return m_sample_count != SDL_GPU_SAMPLECOUNT_1;
}

void FeSdl3GpuContext::release_color_target()
{
	if ( m_color_target_texture )
	{
		SDL_ReleaseGPUTexture( m_device, m_color_target_texture );
		m_color_target_texture = nullptr;
	}

	m_color_target_width = 0;
	m_color_target_height = 0;
}

bool FeSdl3GpuContext::ensure_color_target( int width, int height )
{
	if ( !uses_multisampling() )
	{
		release_color_target();
		return true;
	}

	if ( !m_device || width <= 0 || height <= 0 || m_swapchain_format == SDL_GPU_TEXTUREFORMAT_INVALID )
		return false;

	if ( m_color_target_texture && m_color_target_width == width && m_color_target_height == height )
		return true;

	release_color_target();

	SDL_GPUTextureCreateInfo texture_info = {};
	texture_info.type = SDL_GPU_TEXTURETYPE_2D;
	texture_info.format = m_swapchain_format;
	texture_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
	texture_info.width = static_cast<Uint32>( width );
	texture_info.height = static_cast<Uint32>( height );
	texture_info.layer_count_or_depth = 1;
	texture_info.num_levels = 1;
	texture_info.sample_count = m_sample_count;
	texture_info.props = 0;

	m_color_target_texture = SDL_CreateGPUTexture( m_device, &texture_info );
	if ( !m_color_target_texture )
		return false;

	m_color_target_width = width;
	m_color_target_height = height;
	return true;
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
	texture_info.sample_count = m_sample_count;
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

	if ( entry.msaa_color_texture )
	{
		SDL_ReleaseGPUTexture( m_device, entry.msaa_color_texture );
		entry.msaa_color_texture = nullptr;
	}

	if ( entry.depth_texture )
	{
		SDL_ReleaseGPUTexture( m_device, entry.depth_texture );
		entry.depth_texture = nullptr;
	}

	entry.width = 0;
	entry.height = 0;
	entry.sample_count = SDL_GPU_SAMPLECOUNT_1;
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

FeSdl3GpuContext::PbrCustomShaderEntry *FeSdl3GpuContext::get_pbr_custom_shader_entry( const FeRenderGeometry &image )
{
	if ( !m_device || !m_pbr_vertex_shader || !image.pbr_material.artwork_shader )
		return nullptr;

	const FeShader *shader = image.pbr_material.artwork_shader;
	const FeShader::Type shader_type = shader->get_type();
	const std::string &fragment_source_path = shader->get_fragment_source_path();
	const std::string &fragment_source_code = shader->get_fragment_source_code();
	if ( ( fragment_source_path.empty() && fragment_source_code.empty() ) ||
		( shader_type != FeShader::Fragment && shader_type != FeShader::VertexAndFragment ) )
	{
		return nullptr;
	}

	const std::string cache_key =
		fragment_source_path.empty()
			? std::string( "memory-pbr-fragment|" ) + std::to_string( std::hash<std::string>{}( fragment_source_code ) )
			: absolute_path( fragment_source_path );
	const unsigned long long source_stamp =
		fragment_source_path.empty()
			? static_cast<unsigned long long>( std::hash<std::string>{}( fragment_source_code ) )
			: static_cast<unsigned long long>( get_file_mtime( fragment_source_path ) );

	PbrCustomShaderEntry &entry = m_pbr_custom_shaders[ cache_key ];
	if ( entry.source_stamp == source_stamp )
	{
		if ( entry.fragment_shader )
			return &entry;
		if ( entry.compile_failed )
			return nullptr;
	}

	release_pbr_custom_shader( entry );
	entry.source_path = cache_key;
	if ( !create_pbr_custom_shader_entry( image, entry ) )
	{
		entry.compile_failed = true;
		entry.source_stamp = source_stamp;
		return nullptr;
	}

	entry.compile_failed = false;
	entry.source_stamp = source_stamp;
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
	entry.source_id = "__builtin_fast_blend_" + std::to_string( blend_mode ) + "__";

	ShaderBlob fragment_blob = {};
	const std::string source_code = build_fast_builtin_fragment_shader( blend_mode );
	if ( source_code.empty() || !compile_shader_blob( entry.source_id, source_code, false, fragment_blob ) )
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
		FeLog() << "SDL: builtin_shader: SDL_CreateGPUShader failed for " << entry.source_id << std::endl;
		return false;
	}

	SDL_GPUVertexBufferDescription vertex_buffer = {};
	vertex_buffer.slot = 0;
	vertex_buffer.pitch = sizeof( FeRenderVertex );
	vertex_buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	vertex_buffer.instance_step_rate = 0;

	SDL_GPUVertexAttribute attributes[6] = {};
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
	attributes[3].location = 3;
	attributes[3].buffer_slot = 0;
	attributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	attributes[3].offset = offsetof( FeRenderVertex, u1 );
	attributes[4].location = 4;
	attributes[4].buffer_slot = 0;
	attributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	attributes[4].offset = offsetof( FeRenderVertex, nx );
	attributes[5].location = 5;
	attributes[5].buffer_slot = 0;
	attributes[5].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[5].offset = offsetof( FeRenderVertex, tx );

	SDL_GPUColorTargetDescription color_target = {};
	color_target.format = m_swapchain_format;

	SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.vertex_shader = m_vertex_shader;
	pipeline_info.fragment_shader = entry.fragment_shader;
	pipeline_info.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer;
	pipeline_info.vertex_input_state.num_vertex_buffers = 1;
	pipeline_info.vertex_input_state.vertex_attributes = attributes;
	pipeline_info.vertex_input_state.num_vertex_attributes = 6;
	pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
	pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
	pipeline_info.rasterizer_state.enable_depth_clip = true;
	pipeline_info.multisample_state.sample_count = m_sample_count;
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
				FeLog() << "SDL: builtin_shader: SDL_CreateGPUGraphicsPipeline failed for " << entry.source_id << std::endl;
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
			FeLog() << "SDL: custom_shader: SDL_CreateGPUShader failed for vertex shader " << vertex_source_id << std::endl;
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
		FeLog() << "SDL: custom_shader: SDL_CreateGPUShader failed for " << entry.source_path << std::endl;
		return false;
	}

	SDL_GPUVertexBufferDescription vertex_buffer = {};
	vertex_buffer.slot = 0;
	vertex_buffer.pitch = sizeof( FeRenderVertex );
	vertex_buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	vertex_buffer.instance_step_rate = 0;

	SDL_GPUVertexAttribute attributes[6] = {};
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
	attributes[3].location = 3;
	attributes[3].buffer_slot = 0;
	attributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	attributes[3].offset = offsetof( FeRenderVertex, u1 );
	attributes[4].location = 4;
	attributes[4].buffer_slot = 0;
	attributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	attributes[4].offset = offsetof( FeRenderVertex, nx );
	attributes[5].location = 5;
	attributes[5].buffer_slot = 0;
	attributes[5].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[5].offset = offsetof( FeRenderVertex, tx );

	SDL_GPUColorTargetDescription color_target = {};
	color_target.format = m_swapchain_format;

	SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.vertex_shader = vertex_shader;
	pipeline_info.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer;
	pipeline_info.vertex_input_state.num_vertex_buffers = 1;
	pipeline_info.vertex_input_state.vertex_attributes = attributes;
	pipeline_info.vertex_input_state.num_vertex_attributes = 6;
	pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
	pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
	pipeline_info.rasterizer_state.enable_depth_clip = true;
	pipeline_info.multisample_state.sample_count = m_sample_count;
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
				FeLog() << "SDL: custom_shader: SDL_CreateGPUGraphicsPipeline failed for " << entry.source_path << std::endl;
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
	FeDebug() << "SDL: custom_shader: ready " << entry.source_path << std::endl;
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
		FeLog() << "SDL: custom_shader: SDL_CreateGPUShader failed for " << entry.source_path << std::endl;
		return false;
	}

	SDL_GPUVertexBufferDescription vertex_buffer = {};
	vertex_buffer.slot = 0;
	vertex_buffer.pitch = sizeof( FeRenderVertex );
	vertex_buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	vertex_buffer.instance_step_rate = 0;

	SDL_GPUVertexAttribute attributes[6] = {};
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
	attributes[3].location = 3;
	attributes[3].buffer_slot = 0;
	attributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	attributes[3].offset = offsetof( FeRenderVertex, u1 );
	attributes[4].location = 4;
	attributes[4].buffer_slot = 0;
	attributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	attributes[4].offset = offsetof( FeRenderVertex, nx );
	attributes[5].location = 5;
	attributes[5].buffer_slot = 0;
	attributes[5].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[5].offset = offsetof( FeRenderVertex, tx );

	SDL_GPUColorTargetDescription color_target = {};
	color_target.format = m_swapchain_format;

	SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.vertex_shader = m_vertex_shader;
	pipeline_info.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer;
	pipeline_info.vertex_input_state.num_vertex_buffers = 1;
	pipeline_info.vertex_input_state.vertex_attributes = attributes;
	pipeline_info.vertex_input_state.num_vertex_attributes = 6;
	pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
	pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
	pipeline_info.rasterizer_state.enable_depth_clip = true;
	pipeline_info.multisample_state.sample_count = m_sample_count;
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
				FeLog() << "SDL: custom_shader: SDL_CreateGPUGraphicsPipeline failed for " << entry.source_path << std::endl;
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
	FeDebug() << "SDL: custom_shader: ready " << entry.source_path << std::endl;
	return true;
}

bool FeSdl3GpuContext::create_pbr_custom_shader_entry( const FeRenderGeometry &image, PbrCustomShaderEntry &entry )
{
	if ( !image.pbr_material.artwork_shader )
		return false;

	const FeShader *shader = image.pbr_material.artwork_shader;
	std::string raw_source;
	const std::string &source_path = shader->get_fragment_source_path();
	if ( !source_path.empty() )
	{
		if ( !read_file_content( source_path, raw_source ) )
		{
			FeLog() << "SDL: pbr_custom_shader: failed to read source " << source_path << std::endl;
			return false;
		}
	}
	else
	{
		raw_source = shader->get_fragment_source_code();
		if ( raw_source.empty() )
			return false;
	}

	std::string fragment_source_code;
	std::vector<CustomUniformBinding> fragment_uniforms;
	std::vector<CustomSamplerBinding> fragment_samplers;
	const std::string source_id =
		source_path.empty() ? entry.source_path : source_path;
	if ( !build_pbr_custom_fragment_shader(
		image,
		source_id,
		raw_source,
		fragment_source_code,
		fragment_uniforms,
		fragment_samplers ) )
	{
		return false;
	}

	ShaderBlob fragment_blob = {};
	if ( !compile_shader_blob( source_id, fragment_source_code, false, fragment_blob ) )
		return false;

	SDL_GPUShaderCreateInfo fragment_info = {};
	fragment_info.code_size = fragment_blob.code.size();
	fragment_info.code = fragment_blob.code.data();
	fragment_info.entrypoint = fragment_blob.entrypoint;
	fragment_info.format = fragment_blob.format;
	fragment_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	fragment_info.num_uniform_buffers = 2;
	fragment_info.num_samplers = static_cast<Uint32>( 5 + fragment_samplers.size() );

	entry.fragment_shader = SDL_CreateGPUShader( m_device, &fragment_info );
	if ( !entry.fragment_shader )
	{
		FeLog() << "SDL: pbr_custom_shader: SDL_CreateGPUShader failed for " << entry.source_path << std::endl;
		return false;
	}

	SDL_GPUVertexBufferDescription vertex_buffers[2] = {};
	vertex_buffers[0].slot = 0;
	vertex_buffers[0].pitch = sizeof( FeRenderVertex );
	vertex_buffers[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	vertex_buffers[0].instance_step_rate = 0;
	vertex_buffers[1].slot = 1;
	vertex_buffers[1].pitch = sizeof( PbrInstanceEntry );
	vertex_buffers[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
	vertex_buffers[1].instance_step_rate = 0;

	SDL_GPUVertexAttribute attributes[13] = {};
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
	attributes[3].location = 3;
	attributes[3].buffer_slot = 0;
	attributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	attributes[3].offset = offsetof( FeRenderVertex, u1 );
	attributes[4].location = 4;
	attributes[4].buffer_slot = 0;
	attributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	attributes[4].offset = offsetof( FeRenderVertex, nx );
	attributes[5].location = 5;
	attributes[5].buffer_slot = 0;
	attributes[5].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[5].offset = offsetof( FeRenderVertex, tx );
	attributes[6].location = 6;
	attributes[6].buffer_slot = 1;
	attributes[6].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[6].offset = offsetof( PbrInstanceEntry, model ) + ( 0 * sizeof( float ) * 4 );
	attributes[7].location = 7;
	attributes[7].buffer_slot = 1;
	attributes[7].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[7].offset = offsetof( PbrInstanceEntry, model ) + ( 1 * sizeof( float ) * 4 );
	attributes[8].location = 8;
	attributes[8].buffer_slot = 1;
	attributes[8].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[8].offset = offsetof( PbrInstanceEntry, model ) + ( 2 * sizeof( float ) * 4 );
	attributes[9].location = 9;
	attributes[9].buffer_slot = 1;
	attributes[9].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[9].offset = offsetof( PbrInstanceEntry, model ) + ( 3 * sizeof( float ) * 4 );
	attributes[10].location = 10;
	attributes[10].buffer_slot = 1;
	attributes[10].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[10].offset = offsetof( PbrInstanceEntry, normal_matrix ) + ( 0 * sizeof( float ) * 4 );
	attributes[11].location = 11;
	attributes[11].buffer_slot = 1;
	attributes[11].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[11].offset = offsetof( PbrInstanceEntry, normal_matrix ) + ( 1 * sizeof( float ) * 4 );
	attributes[12].location = 12;
	attributes[12].buffer_slot = 1;
	attributes[12].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[12].offset = offsetof( PbrInstanceEntry, normal_matrix ) + ( 2 * sizeof( float ) * 4 );

	SDL_GPUColorTargetDescription color_target = {};
	color_target.format = m_swapchain_format;

	SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.vertex_shader = m_pbr_vertex_shader;
	pipeline_info.fragment_shader = entry.fragment_shader;
	pipeline_info.vertex_input_state.vertex_buffer_descriptions = vertex_buffers;
	pipeline_info.vertex_input_state.num_vertex_buffers = 2;
	pipeline_info.vertex_input_state.vertex_attributes = attributes;
	pipeline_info.vertex_input_state.num_vertex_attributes = 13;
	pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
	pipeline_info.rasterizer_state.enable_depth_clip = true;
	pipeline_info.multisample_state.sample_count = m_sample_count;
	pipeline_info.multisample_state.sample_mask = 0;
	pipeline_info.target_info.color_target_descriptions = &color_target;
	pipeline_info.target_info.num_color_targets = 1;

	for ( int depth_mode = 0; depth_mode < 3; ++depth_mode )
	{
		configure_pipeline_depth_state( pipeline_info, depth_mode );
		for ( int blend_mode = 0; blend_mode < 2; ++blend_mode )
		{
			color_target.blend_state = make_gpu_blend_state( blend_mode == 1 ? FeBlend::Alpha : FeBlend::None );
			for ( int sided = 0; sided < 2; ++sided )
			{
				pipeline_info.rasterizer_state.cull_mode =
					sided ? SDL_GPU_CULLMODE_NONE : SDL_GPU_CULLMODE_BACK;
				entry.pbr_pipelines[depth_mode][blend_mode][sided] =
					SDL_CreateGPUGraphicsPipeline( m_device, &pipeline_info );
				if ( !entry.pbr_pipelines[depth_mode][blend_mode][sided] )
				{
					FeLog() << "SDL: pbr_custom_shader: SDL_CreateGPUGraphicsPipeline failed for " << entry.source_path << std::endl;
					release_pbr_custom_shader( entry );
					return false;
				}
			}
		}
	}

	entry.fragment_uniforms = fragment_uniforms;
	entry.fragment_samplers = fragment_samplers;
	FeDebug() << "SDL: pbr_custom_shader: ready " << entry.source_path << std::endl;
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
			FeLog() << "SDL: custom_shader: too many numeric uniforms in " << source_id << std::endl;
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
			FeLog() << "SDL: custom_shader: failed to read source " << source_path << std::endl;
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

bool FeSdl3GpuContext::build_pbr_custom_fragment_shader(
	const FeRenderGeometry &image,
	const std::string &source_id,
	const std::string &raw_source,
	std::string &source_code,
	std::vector<CustomUniformBinding> &uniforms,
	std::vector<CustomSamplerBinding> &samplers )
{
	const FeShader *shader = image.pbr_material.artwork_shader;
	if ( !shader )
		return false;

	std::vector<ParsedCustomVarying> parsed_varyings;
	std::string varying_stripped_source;
	if ( !parse_custom_varyings( raw_source, parsed_varyings, varying_stripped_source ) )
		return false;

	if ( !parsed_varyings.empty() )
	{
		FeLog() << "SDL: pbr_custom_shader: varying declarations are not supported in " << source_id << std::endl;
		return false;
	}

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
			binding.slot = 5 + static_cast<int>( samplers.size() );
			binding.current_texture = false;
			binding.image = nullptr;
			samplers.push_back( binding );
			continue;
		}

		if ( static_cast<int>( uniforms.size() ) >= FE_MAX_CUSTOM_SHADER_UNIFORMS )
		{
			FeLog() << "SDL: pbr_custom_shader: too many numeric uniforms in " << source_id << std::endl;
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
		const std::string to = "fe_frag_texcoord" + std::to_string( i );
		replace_all( stripped_source, from, to );
	}
	replace_all( stripped_source, "gl_Color", "fe_frag_color" );
	replace_all( stripped_source, "gl_FragColor", "fe_artwork_out_color" );

	const std::string renamed_source = std::regex_replace(
		stripped_source,
		std::regex( "\\bvoid\\s+main\\s*\\(" ),
		"void fe_artwork_main(",
		std::regex_constants::format_first_only );
	if ( renamed_source == stripped_source )
	{
		FeLog() << "SDL: pbr_custom_shader: missing main() in " << source_id << std::endl;
		return false;
	}

	std::ostringstream sampler_code;
	for ( const CustomSamplerBinding &sampler : samplers )
	{
		sampler_code
			<< "layout(set = 2, binding = " << sampler.slot << ") uniform sampler2D artwork_shader_texture"
			<< sampler.slot << ";\n";
	}

	std::ostringstream macro_code;
	for ( const CustomSamplerBinding &sampler : samplers )
		macro_code << "#define " << sampler.name << " artwork_shader_texture" << sampler.slot << "\n";
	for ( const CustomUniformBinding &uniform : uniforms )
		macro_code << build_custom_uniform_macro( uniform.name, uniform.slot, uniform.components ) << "\n";

	std::ostringstream undef_code;
	for ( const CustomSamplerBinding &sampler : samplers )
		undef_code << "#undef " << sampler.name << "\n";
	for ( const CustomUniformBinding &uniform : uniforms )
		undef_code << "#undef " << uniform.name << "\n";

	std::ostringstream helper_code;
	helper_code
		<< sampler_code.str()
		<< "layout(set = 3, binding = 1, std140) uniform CustomFragmentUniforms\n"
		<< "{\n"
		<< "\tvec4 values[" << FE_MAX_CUSTOM_SHADER_UNIFORMS << "];\n"
		<< "} custom_ubo;\n"
		<< "vec2 artwork_uv;\n"
		<< "vec2 material_uv0;\n"
		<< "vec2 material_uv1;\n"
		<< "vec2 artwork_size;\n"
		<< "vec4 fe_frag_texcoord0;\n"
		<< "vec4 fe_frag_texcoord1;\n"
		<< "vec4 fe_frag_texcoord2;\n"
		<< "vec4 fe_frag_texcoord3;\n"
		<< "vec4 fe_frag_texcoord4;\n"
		<< "vec4 fe_frag_texcoord5;\n"
		<< "vec4 fe_frag_texcoord6;\n"
		<< "vec4 fe_frag_texcoord7;\n"
		<< "vec4 fe_frag_color;\n"
		<< "float fit_alpha;\n"
		<< "vec4 fe_artwork_out_color;\n"
		<< "vec4 fe_texture2D( sampler2D sampler_value, vec2 uv )\n"
		<< "{\n"
		<< "\treturn texture( sampler_value, uv );\n"
		<< "}\n"
		<< "vec4 fe_texture2D( sampler2D sampler_value, vec2 uv, float bias )\n"
		<< "{\n"
		<< "\treturn texture( sampler_value, uv, bias );\n"
		<< "}\n"
		<< "vec4 fe_sample_artwork()\n"
		<< "{\n"
		<< "\treturn texture( base_color_texture, artwork_uv ) * fit_alpha;\n"
		<< "}\n"
		<< "void fe_artwork_main();\n"
		<< "vec4 fe_run_artwork_shader( vec2 uv, float current_fit_alpha )\n"
		<< "{\n"
		<< "\tartwork_uv = uv;\n"
		<< "\tmaterial_uv0 = frag_uv0;\n"
		<< "\tmaterial_uv1 = frag_uv1;\n"
		<< "\tartwork_size = vec2( textureSize( base_color_texture, 0 ) );\n"
		<< "\tfe_frag_texcoord0 = vec4( artwork_uv, 0.0, 0.0 );\n"
		<< "\tfe_frag_texcoord1 = vec4( material_uv0, 0.0, 0.0 );\n"
		<< "\tfe_frag_texcoord2 = vec4( material_uv1, 0.0, 0.0 );\n"
		<< "\tfe_frag_texcoord3 = vec4( 0.0 );\n"
		<< "\tfe_frag_texcoord4 = vec4( 0.0 );\n"
		<< "\tfe_frag_texcoord5 = vec4( 0.0 );\n"
		<< "\tfe_frag_texcoord6 = vec4( 0.0 );\n"
		<< "\tfe_frag_texcoord7 = vec4( 0.0 );\n"
		<< "\tfe_frag_color = vec4( 1.0 );\n"
		<< "\tfit_alpha = current_fit_alpha;\n"
		<< "\tfe_artwork_out_color = fe_texture2D( base_color_texture, artwork_uv );\n"
		<< "\tfe_artwork_main();\n"
		<< "\treturn fe_artwork_out_color * current_fit_alpha;\n"
		<< "}\n";

	const std::string user_shader_code =
		macro_code.str() +
		renamed_source + "\n" +
		undef_code.str();

	const std::string sample_base_color_block =
		"vec4 sample_base_color()\n"
		"{\n"
		"\tfloat fit_alpha = 1.0;\n"
		"\tvec2 uv = transform_uv( select_uv( pbr.transforms_texcoord_fit[0].x ), 0, fit_alpha );\n"
		"\tvec4 base_sample_color = texture( base_color_texture, uv );\n"
		"\tbase_sample_color *= fit_alpha;\n"
		"\treturn base_sample_color;\n"
		"}\n";
	const std::string sample_emissive_block =
		"vec3 sample_emissive()\n"
		"{\n"
		"\tfloat fit_alpha = 1.0;\n"
		"\tvec2 uv = transform_uv( select_uv( pbr.transforms_texcoord_fit[4].x ), 4, fit_alpha );\n"
		"\treturn texture( emissive_texture, uv ).rgb * fit_alpha;\n"
		"}\n";
	const std::string custom_sample_base_color_block =
		"vec4 sample_base_color()\n"
		"{\n"
		"\tfloat fit_alpha = 1.0;\n"
		"\tvec2 uv = transform_uv( select_uv( pbr.transforms_texcoord_fit[0].x ), 0, fit_alpha );\n"
		"\treturn fe_run_artwork_shader( uv, fit_alpha );\n"
		"}\n";
	const std::string custom_sample_emissive_block =
		"vec3 sample_emissive()\n"
		"{\n"
		"\tfloat fit_alpha = 1.0;\n"
		"\tvec2 uv = transform_uv( select_uv( pbr.transforms_texcoord_fit[4].x ), 4, fit_alpha );\n"
		"\tif ( pbr.artwork_control.x > 0.5 )\n"
		"\t\treturn fe_run_artwork_shader( uv, fit_alpha ).rgb;\n"
		"\treturn texture( emissive_texture, uv ).rgb * fit_alpha;\n"
		"}\n";

	source_code = build_pbr_fragment_shader();
	const std::string helper_insert_needle =
		"const float PI = 3.14159265358979323846;\n";
	const std::size_t helper_insert_pos = source_code.find( helper_insert_needle );
	if ( helper_insert_pos == std::string::npos )
	{
		FeLog() << "SDL: pbr_custom_shader: failed to inject helpers in " << source_id << std::endl;
		return false;
	}
	source_code.insert( helper_insert_pos, helper_code.str() );
	source_code.insert( helper_insert_pos + helper_code.str().size(), user_shader_code );

	const std::size_t base_color_pos = source_code.find( sample_base_color_block );
	if ( base_color_pos == std::string::npos )
	{
		FeLog() << "SDL: pbr_custom_shader: failed to patch sample_base_color in " << source_id << std::endl;
		return false;
	}
	source_code.replace(
		base_color_pos,
		sample_base_color_block.size(),
		custom_sample_base_color_block );

	const std::size_t emissive_pos = source_code.find( sample_emissive_block );
	if ( emissive_pos == std::string::npos )
	{
		FeLog() << "SDL: pbr_custom_shader: failed to patch sample_emissive in " << source_id << std::endl;
		return false;
	}
	source_code.replace(
		emissive_pos,
		sample_emissive_block.size(),
		custom_sample_emissive_block );

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
			FeLog() << "SDL: custom_shader: failed to read source " << source_path << std::endl;
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
			FeLog() << "SDL: custom_shader: samplers are not supported in custom vertex shaders " << source_path << std::endl;
			return false;
		}

		if ( static_cast<int>( uniforms.size() ) >= FE_MAX_CUSTOM_SHADER_UNIFORMS )
		{
			FeLog() << "SDL: custom_shader: too many numeric uniforms in " << source_path << std::endl;
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
	std::vector<float> &data,
	const FeShader *shader ) const
{
	data.assign( FE_MAX_CUSTOM_SHADER_UNIFORMS * 4, 0.0f );

	float image_width = 0.0f;
	float image_height = 0.0f;
	get_geometry_size( image, image_width, image_height );
	const FeShader *uniform_shader = shader ? shader : image.shader;

	for ( const CustomUniformBinding &uniform : uniforms )
	{
		const std::vector<float> *values =
			uniform_shader ? uniform_shader->get_param( uniform.name.c_str() ) : nullptr;
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
		entry.mipmapped == surface.mipmapped &&
		entry.sample_count == m_sample_count &&
		( uses_multisampling() ? ( entry.msaa_color_texture != nullptr ) : ( entry.msaa_color_texture == nullptr ) ) )
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

	if ( uses_multisampling() )
	{
		SDL_GPUTextureCreateInfo msaa_color_info = color_info;
		msaa_color_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
		msaa_color_info.num_levels = 1;
		msaa_color_info.sample_count = m_sample_count;

		entry.msaa_color_texture = SDL_CreateGPUTexture( m_device, &msaa_color_info );
		if ( !entry.msaa_color_texture )
		{
			release_surface_target( entry );
			return false;
		}
	}

	SDL_GPUTextureCreateInfo depth_info = {};
	depth_info.type = SDL_GPU_TEXTURETYPE_2D;
	depth_info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
	depth_info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
	depth_info.width = static_cast<Uint32>( surface.width );
	depth_info.height = static_cast<Uint32>( surface.height );
	depth_info.layer_count_or_depth = 1;
	depth_info.num_levels = 1;
	depth_info.sample_count = m_sample_count;
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
	entry.sample_count = m_sample_count;
	entry.rendered_once = false;
	entry.vertex_signature = 0;
	entry.last_signature = 0;
	return true;
}

bool FeSdl3GpuContext::upload_vertex_buffer( const std::vector<FeRenderVertex> &vertices, SDL_GPUBuffer *&buffer, Uint32 &buffer_size )
{
	return upload_vertex_buffer(
		vertices.empty() ? nullptr : vertices.data(),
		vertices.size(),
	buffer,
	buffer_size );
}

bool FeSdl3GpuContext::upload_gpu_vertex_buffer_data(
	SDL_GPUCommandBuffer *command_buffer,
	const void *data,
	Uint32 size,
	SDL_GPUBuffer *&buffer,
	Uint32 &buffer_size )
{
	if ( !m_device )
		return false;

	if ( !data || size == 0 )
	{
		if ( buffer )
		{
			SDL_ReleaseGPUBuffer( m_device, buffer );
			buffer = nullptr;
		}
		buffer_size = 0;
		return true;
	}

	if ( !buffer || buffer_size < size )
	{
		if ( buffer )
			SDL_ReleaseGPUBuffer( m_device, buffer );

		SDL_GPUBufferCreateInfo buffer_info = {};
		buffer_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
		buffer_info.size = size;
		buffer_info.props = 0;

		buffer = SDL_CreateGPUBuffer( m_device, &buffer_info );
		if ( !buffer )
			return false;

		buffer_size = size;
	}

	SDL_GPUTransferBufferCreateInfo transfer_info = {};
	transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transfer_info.size = size;
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

	std::memcpy( mapped, data, size );
	SDL_UnmapGPUTransferBuffer( m_device, transfer_buffer );

	SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass( command_buffer );
	if ( !copy_pass )
	{
		SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
		return false;
	}

	SDL_GPUTransferBufferLocation source = {};
	source.transfer_buffer = transfer_buffer;
	source.offset = 0;

	SDL_GPUBufferRegion destination = {};
	destination.buffer = buffer;
	destination.offset = 0;
	destination.size = size;

	SDL_UploadToGPUBuffer( copy_pass, &source, &destination, false );
	SDL_EndGPUCopyPass( copy_pass );
	SDL_ReleaseGPUTransferBuffer( m_device, transfer_buffer );
	return true;
}

bool FeSdl3GpuContext::upload_gpu_vertex_buffer_data(
	const void *data,
	Uint32 size,
	SDL_GPUBuffer *&buffer,
	Uint32 &buffer_size )
{
	SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer( m_device );
	if ( !command_buffer )
		return false;

	const bool uploaded =
		upload_gpu_vertex_buffer_data( command_buffer, data, size, buffer, buffer_size );
	if ( !uploaded )
	{
		SDL_CancelGPUCommandBuffer( command_buffer );
		return false;
	}

	return SDL_SubmitGPUCommandBuffer( command_buffer );
}

bool FeSdl3GpuContext::upload_vertex_buffer(
	const FeRenderVertex *vertices,
	std::size_t vertex_count,
	SDL_GPUBuffer *&buffer,
	Uint32 &buffer_size )
{
	const Uint32 required_size = static_cast<Uint32>( vertex_count * sizeof( FeRenderVertex ) );
	return upload_gpu_vertex_buffer_data( vertices, required_size, buffer, buffer_size );
}

bool FeSdl3GpuContext::upload_vertex_buffer()
{
	return upload_vertex_buffer( m_vertex_stream, m_vertex_buffer, m_vertex_buffer_size );
}

bool FeSdl3GpuContext::render_prepared_geometry_batch(
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
	bool &drew_anything )
{
	if ( !render_pass || !command_buffer || !m_blend_pipelines[0][FeBlend::Alpha] || prepared_images.empty() )
		return true;

	SDL_GPUBuffer *inline_vertex_buffer = nullptr;
	Uint32 inline_vertex_buffer_size = 0;
	const bool use_cached_vertex_buffer =
		cached_vertex_buffer && cached_vertex_buffer_size && cached_vertex_signature;
	if ( !vertex_stream.empty() )
	{
		const std::uint64_t vertex_stream_hash = compute_vertex_stream_hash( vertex_stream );
		if ( use_cached_vertex_buffer )
		{
			inline_vertex_buffer = *cached_vertex_buffer;
			inline_vertex_buffer_size = *cached_vertex_buffer_size;
			if ( !inline_vertex_buffer || ( *cached_vertex_signature != vertex_stream_hash ) )
			{
				if ( !upload_vertex_buffer( vertex_stream, inline_vertex_buffer, inline_vertex_buffer_size ) )
					return false;

				*cached_vertex_buffer = inline_vertex_buffer;
				*cached_vertex_buffer_size = inline_vertex_buffer_size;
				*cached_vertex_signature = vertex_stream_hash;
			}
		}
		else if ( !upload_vertex_buffer( vertex_stream, inline_vertex_buffer, inline_vertex_buffer_size ) )
			return false;
	}
	else if ( use_cached_vertex_buffer )
	{
		*cached_vertex_signature = 0;
	}

	SDL_GPUBuffer *currently_bound_vertex_buffer = nullptr;
	auto bind_vertex_buffer = [&]( SDL_GPUBuffer *buffer ) -> bool
	{
		if ( !buffer )
			return false;

		if ( currently_bound_vertex_buffer == buffer )
			return true;

		SDL_GPUBufferBinding vertex_binding = {};
		vertex_binding.buffer = buffer;
		vertex_binding.offset = 0;
		SDL_BindGPUVertexBuffers( render_pass, 0, &vertex_binding, 1 );
		currently_bound_vertex_buffer = buffer;
		return true;
	};

	if ( inline_vertex_buffer )
		bind_vertex_buffer( inline_vertex_buffer );

	FeSdl3GpuDrawUniforms uniforms = {};
	for ( int i = 0; i < 16; ++i )
		uniforms.projection[ i ] = camera.projection.m[ i ];
	uniforms.viewport_width = static_cast<float>( viewport_width );
	uniforms.viewport_height = static_cast<float>( viewport_height );
	uniforms.plane_distance = compute_plane_distance( camera, viewport_height );
	SDL_PushGPUVertexUniformData( command_buffer, 0, &uniforms, sizeof( uniforms ) );

	if ( m_alpha_prepass_pipeline )
	{
		SDL_BindGPUGraphicsPipeline( render_pass, m_alpha_prepass_pipeline );

		for ( const PreparedImage &image : prepared_images )
		{
			if ( image.object_pbr || !image.translucent_depth || !image.gpu_texture || image.vertex_count == 0 || image.custom_shader )
				continue;

			SDL_GPUTextureSamplerBinding sampler_binding = {};
			sampler_binding.texture = image.gpu_texture;
			sampler_binding.sampler = get_image_sampler( image.texture_smooth, image.texture_repeated, image.texture_mipmap );
			SDL_BindGPUFragmentSamplers( render_pass, 0, &sampler_binding, 1 );
			if ( !bind_vertex_buffer( inline_vertex_buffer ) )
				continue;
			SDL_DrawGPUPrimitives(
				render_pass,
				static_cast<Uint32>( image.vertex_count ),
				1,
				static_cast<Uint32>( image.first_vertex ),
				0 );
		}
	}

	FeSdl3GpuPbrVertexUniforms pbr_vertex_uniforms = {};
	for ( int i = 0; i < 16; ++i )
		pbr_vertex_uniforms.projection[i] = uniforms.projection[i];
	pbr_vertex_uniforms.viewport_width = uniforms.viewport_width;
	pbr_vertex_uniforms.viewport_height = uniforms.viewport_height;
	pbr_vertex_uniforms.plane_distance = uniforms.plane_distance;
	pbr_vertex_uniforms.reserved = uniforms.reserved;

	auto fill_pbr_fragment_state =
		[&]( const PreparedImage &image,
			FeSdl3GpuPbrFragmentUniforms &fragment_uniforms,
			SDL_GPUTextureSamplerBinding (&sampler_bindings)[5] )
		{
			const FeRenderTextureBinding *bindings[] =
			{
				&image.geometry->pbr_material.base_color_texture,
				&image.geometry->pbr_material.metallic_roughness_texture,
				&image.geometry->pbr_material.normal_texture,
				&image.geometry->pbr_material.occlusion_texture,
				&image.geometry->pbr_material.emissive_texture
			};

			for ( int i = 0; i < 4; ++i )
				fragment_uniforms.base_color_factor[i] = image.geometry->pbr_material.base_color_factor[i];
			for ( int i = 0; i < 3; ++i )
			{
				fragment_uniforms.emissive_factor[i] = image.geometry->pbr_material.emissive_factor[i];
				fragment_uniforms.ambient_color[i] = image.geometry->ambient_color[i];
				fragment_uniforms.artwork_control[i] = 0.0f;
			}
			fragment_uniforms.ambient_color[3] = image.geometry->camera_light;
			fragment_uniforms.artwork_control[3] = 0.0f;
			fragment_uniforms.artwork_control[0] =
				image.geometry->pbr_material.artwork_shader_emissive ? 1.0f : 0.0f;
			fragment_uniforms.emissive_factor[3] =
				( image.geometry->pbr_material.normal_texture.texture_id != nullptr ) ? 1.0f : 0.0f;
			fragment_uniforms.material_params[0] = image.geometry->pbr_material.metallic_factor;
			fragment_uniforms.material_params[1] = image.geometry->pbr_material.roughness_factor;
			fragment_uniforms.material_params[2] = image.geometry->pbr_material.normal_scale;
			fragment_uniforms.material_params[3] = image.geometry->pbr_material.occlusion_strength;
			fragment_uniforms.control[0] = image.geometry->pbr_material.alpha_cutoff;
			fragment_uniforms.control[1] = static_cast<float>( image.geometry->pbr_material.alpha_mode );
			fragment_uniforms.control[2] = image.geometry->pbr_material.unlit ? 1.0f : 0.0f;
			fragment_uniforms.control[3] = static_cast<float>( image.geometry->light_count );

			for ( int binding_index = 0; binding_index < 5; ++binding_index )
			{
				const FeRenderTextureBinding &binding = *bindings[binding_index];
				fragment_uniforms.transforms_offset_scale[binding_index][0] = binding.offset_u;
				fragment_uniforms.transforms_offset_scale[binding_index][1] = binding.offset_v;
				fragment_uniforms.transforms_offset_scale[binding_index][2] = binding.scale_u;
				fragment_uniforms.transforms_offset_scale[binding_index][3] = binding.scale_v;
				fragment_uniforms.transforms_texcoord_fit[binding_index][0] = static_cast<float>( binding.texcoord_set );
				fragment_uniforms.transforms_texcoord_fit[binding_index][1] = binding.fit_scale_u;
				fragment_uniforms.transforms_texcoord_fit[binding_index][2] = binding.fit_scale_v;
				fragment_uniforms.transforms_texcoord_fit[binding_index][3] = 0.0f;
				sampler_bindings[binding_index].texture =
					image.pbr_textures[binding_index] ? image.pbr_textures[binding_index] : m_white_texture;
				sampler_bindings[binding_index].sampler =
					get_image_sampler(
						binding.smooth,
						binding.repeated,
						binding.mipmap );
			}

			for ( int light_index = 0; light_index < image.geometry->light_count; ++light_index )
			{
				const FeRenderPbrLight &light = image.geometry->lights[light_index];
				FeSdl3GpuPbrLightUniform &target = fragment_uniforms.lights[light_index];
				target.color_intensity[0] = light.color[0];
				target.color_intensity[1] = light.color[1];
				target.color_intensity[2] = light.color[2];
				target.color_intensity[3] = light.intensity;
				const Vec3f view_position = to_view_position(
					Vec3f( light.position[0], light.position[1], light.position[2] ),
					viewport_width,
					viewport_height,
					uniforms.plane_distance );
				const Vec3f view_direction = to_view_direction(
					Vec3f( light.direction[0], light.direction[1], light.direction[2] ) );
				target.position_range[0] = view_position.x;
				target.position_range[1] = view_position.y;
				target.position_range[2] = view_position.z;
				target.position_range[3] = light.range;
				target.direction_inner[0] = view_direction.x;
				target.direction_inner[1] = view_direction.y;
				target.direction_inner[2] = view_direction.z;
				target.direction_inner[3] = light.inner_cone_cos;
				target.outer_type[0] = light.outer_cone_cos;
				target.outer_type[1] = static_cast<float>( light.type );
			}
		};

	auto draw_pbr_images = [&]( const std::vector<const PreparedImage *> &images ) -> bool
	{
		if ( images.empty() || !images[0] || !images[0]->geometry )
			return true;

		const PreparedImage &prototype = *images[0];
		SDL_GPUBuffer *draw_vertex_buffer = nullptr;
		const bool uses_external_vertices =
			( prototype.external_vertex_id != nullptr ) && ( prototype.external_vertices != nullptr );
		if ( uses_external_vertices )
		{
			if ( !ensure_geometry_vertex_buffer( prototype, draw_vertex_buffer ) )
				return true;
		}
		else
			draw_vertex_buffer = inline_vertex_buffer;

		if ( !bind_vertex_buffer( draw_vertex_buffer ) )
			return true;

		const int depth_pipeline = get_depth_pipeline_index( prototype.zbuffer, prototype.translucent_depth );
		const int blend_index =
			( prototype.geometry->pbr_material.alpha_mode == FeRenderPbrAlphaBlend ) ? 1 : 0;
		const int double_sided_index = prototype.geometry->pbr_material.double_sided ? 1 : 0;
		SDL_GPUGraphicsPipeline *pipeline =
			prototype.pbr_custom_shader
				? prototype.pbr_custom_shader->pbr_pipelines[ depth_pipeline ][ blend_index ][ double_sided_index ]
				: m_pbr_pipelines[ depth_pipeline ][ blend_index ][ double_sided_index ];
		if ( !pipeline )
			return true;

		FeSdl3GpuPbrFragmentUniforms fragment_uniforms = {};
		SDL_GPUTextureSamplerBinding base_sampler_bindings[5] = {};
		fill_pbr_fragment_state( prototype, fragment_uniforms, base_sampler_bindings );
		std::vector<float> custom_fragment_uniform_data;
		std::vector<SDL_GPUTextureSamplerBinding> sampler_bindings;
		sampler_bindings.assign( base_sampler_bindings, base_sampler_bindings + 5 );
		if ( prototype.pbr_custom_shader )
		{
			build_custom_uniform_data(
				*prototype.geometry,
				prototype.pbr_custom_shader->fragment_uniforms,
				custom_fragment_uniform_data,
				prototype.geometry->pbr_material.artwork_shader );
			sampler_bindings.resize( 5 + prototype.pbr_custom_shader->fragment_samplers.size() );

			for ( const CustomSamplerBinding &sampler : prototype.pbr_custom_shader->fragment_samplers )
			{
				SDL_GPUTexture *sampler_texture = nullptr;
				bool sampler_repeated = prototype.geometry->pbr_material.base_color_texture.repeated;
				bool sampler_smooth = prototype.geometry->pbr_material.base_color_texture.smooth;
				bool sampler_mipmap = prototype.geometry->pbr_material.base_color_texture.mipmap;
				const FeShader *artwork_shader = prototype.geometry->pbr_material.artwork_shader;
				const bool shader_uses_current_texture =
					artwork_shader ? artwork_shader->uses_current_texture( sampler.name.c_str() ) : false;
				FeImage *shader_image =
					artwork_shader ? artwork_shader->get_texture_param_image( sampler.name.c_str() ) : nullptr;
				const bool use_current_texture = shader_uses_current_texture || ( shader_image == nullptr );

				if ( use_current_texture )
				{
					sampler_texture = prototype.pbr_textures[0];
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
							return true;
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
									return true;
							}
							sampler_texture = cache_entry.gpu_texture;
						}

						sampler_repeated = container->get_repeat();
						sampler_smooth = container->get_smooth();
						sampler_mipmap = container->get_mipmap();
					}
				}

				if ( !sampler_texture )
					return true;

				SDL_GPUTextureSamplerBinding sampler_binding = {};
				sampler_binding.texture = sampler_texture;
				sampler_binding.sampler = get_image_sampler( sampler_smooth, sampler_repeated, sampler_mipmap );
				sampler_bindings[sampler.slot] = sampler_binding;
			}
		}

		SDL_GPUBuffer *instance_buffer = nullptr;
		Uint32 instance_offset = 0;
		Uint32 first_instance = 0;
		Uint32 instance_count = static_cast<Uint32>( images.size() );
		if ( use_preuploaded_pbr_instances && m_pbr_instance_buffer )
		{
			instance_buffer = m_pbr_instance_buffer;
			instance_count = prototype.pbr_instance_count;
			first_instance = prototype.pbr_instance_first;
		}
		else
		{
			const std::vector<FeRenderPbrInstance> *prototype_instances =
				( prototype.geometry && prototype.geometry->has_pbr_instances() )
					? &prototype.geometry->pbr_instances
					: nullptr;
			if ( prototype_instances )
				instance_count = static_cast<Uint32>( prototype_instances->size() );

			std::vector<PbrInstanceEntry> instance_data(
				prototype_instances ? prototype_instances->size() : images.size() );
			for ( std::size_t instance_index = 0; instance_index < instance_data.size(); ++instance_index )
			{
				PbrInstanceEntry &instance = instance_data[instance_index];
				if ( prototype_instances )
				{
					const FeRenderPbrInstance &source = ( *prototype_instances )[instance_index];
					std::memcpy( instance.model, source.model_matrix, sizeof( instance.model ) );
					for ( int row = 0; row < 3; ++row )
					{
						instance.normal_matrix[row][0] = source.normal_matrix[row * 3 + 0];
						instance.normal_matrix[row][1] = source.normal_matrix[row * 3 + 1];
						instance.normal_matrix[row][2] = source.normal_matrix[row * 3 + 2];
						instance.normal_matrix[row][3] = 0.0f;
					}
				}
				else
				{
					const FeRenderGeometry &geometry = *images[instance_index]->geometry;
					std::memcpy( instance.model, geometry.model_matrix, sizeof( instance.model ) );
					for ( int row = 0; row < 3; ++row )
					{
						instance.normal_matrix[row][0] = geometry.normal_matrix[row * 3 + 0];
						instance.normal_matrix[row][1] = geometry.normal_matrix[row * 3 + 1];
						instance.normal_matrix[row][2] = geometry.normal_matrix[row * 3 + 2];
						instance.normal_matrix[row][3] = 0.0f;
					}
				}
			}

			Uint32 temporary_buffer_size = 0;
			if ( !upload_gpu_vertex_buffer_data(
				instance_data.data(),
				static_cast<Uint32>( instance_data.size() * sizeof( PbrInstanceEntry ) ),
				instance_buffer,
				temporary_buffer_size ) )
			{
				return false;
			}

			if ( temporary_pbr_buffers )
				temporary_pbr_buffers->push_back( instance_buffer );
		}

		SDL_GPUBufferBinding vertex_bindings[2] = {};
		vertex_bindings[0].buffer = draw_vertex_buffer;
		vertex_bindings[0].offset = 0;
		vertex_bindings[1].buffer = instance_buffer;
		vertex_bindings[1].offset = instance_offset;
		SDL_BindGPUVertexBuffers( render_pass, 0, vertex_bindings, 2 );
		currently_bound_vertex_buffer = draw_vertex_buffer;

		SDL_PushGPUVertexUniformData( command_buffer, 0, &pbr_vertex_uniforms, sizeof( pbr_vertex_uniforms ) );
		SDL_PushGPUFragmentUniformData(
			command_buffer,
			0,
			&fragment_uniforms,
			static_cast<Uint32>( sizeof( fragment_uniforms ) ) );
		if ( prototype.pbr_custom_shader )
		{
			SDL_PushGPUFragmentUniformData(
				command_buffer,
				1,
				custom_fragment_uniform_data.data(),
				static_cast<Uint32>( custom_fragment_uniform_data.size() * sizeof( float ) ) );
		}
		SDL_BindGPUGraphicsPipeline( render_pass, pipeline );
		SDL_BindGPUFragmentSamplers(
			render_pass,
			0,
			sampler_bindings.data(),
			static_cast<Uint32>( sampler_bindings.size() ) );
		SDL_DrawGPUPrimitives(
			render_pass,
			static_cast<Uint32>( prototype.vertex_count ),
			instance_count,
			uses_external_vertices ? 0u : static_cast<Uint32>( prototype.first_vertex ),
			first_instance );
		drew_anything = true;
		return true;
	};

	for ( std::size_t image_index = 0; image_index < prepared_images.size(); ++image_index )
	{
		const PreparedImage &image = prepared_images[image_index];
		if ( !image.gpu_texture || image.vertex_count == 0 )
			continue;

		if ( image.object_pbr )
		{
			if ( !image.geometry )
				continue;
			if ( use_preuploaded_pbr_instances && !image.pbr_instance_head )
				continue;

			std::vector<const PreparedImage *> image_batch;
			image_batch.push_back( &image );
			if ( !use_preuploaded_pbr_instances && image.pbr_instance_count > 1 )
			{
				for ( std::size_t batch_index = image_index + 1; batch_index < prepared_images.size(); ++batch_index )
				{
					const PreparedImage &batch_image = prepared_images[batch_index];
					if ( !batch_image.object_pbr
						|| batch_image.pbr_instance_head
						|| batch_image.pbr_instance_first != image.pbr_instance_first )
					{
						continue;
					}

					image_batch.push_back( &batch_image );
					if ( image_batch.size() >= image.pbr_instance_count )
						break;
				}
			}

			if ( !draw_pbr_images( image_batch ) )
				return false;
			continue;
		}

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
		if ( !bind_vertex_buffer( inline_vertex_buffer ) )
			continue;

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
			sampler_binding.sampler = get_image_sampler( image.texture_smooth, image.texture_repeated, image.texture_mipmap );
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
				bool sampler_mipmap = image.texture_mipmap;
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
						sampler_mipmap = container->get_mipmap();
					}
				}

				if ( !samplers_ready || !sampler_texture )
				{
					samplers_ready = false;
					break;
				}

				SDL_GPUTextureSamplerBinding sampler_binding = {};
				sampler_binding.texture = sampler_texture;
				sampler_binding.sampler = get_image_sampler( sampler_smooth, sampler_repeated, sampler_mipmap );
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
			sampler_binding.sampler = get_image_sampler( image.texture_smooth, image.texture_repeated, image.texture_mipmap );
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

	if ( inline_vertex_buffer && !use_cached_vertex_buffer )
		SDL_ReleaseGPUBuffer( m_device, inline_vertex_buffer );

	return true;
}

bool FeSdl3GpuContext::render_geometry_batch(
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
	bool &drew_anything )
{
	if ( geometry.empty() )
		return true;

	std::vector<PreparedImage> prepared_images;
	std::vector<FeRenderVertex> vertex_stream;
	prepare_geometry_batch( geometry, use_surface_targets, prepared_images, vertex_stream );
	return render_prepared_geometry_batch(
		render_pass,
		command_buffer,
		camera,
		viewport_width,
		viewport_height,
		prepared_images,
		vertex_stream,
		cached_vertex_buffer,
		cached_vertex_buffer_size,
		cached_vertex_signature,
		false,
		temporary_pbr_buffers,
		drew_anything );
}

bool FeSdl3GpuContext::render_surface_frames( SDL_GPUCommandBuffer *command_buffer, std::vector<SDL_GPUBuffer *> *temporary_pbr_buffers )
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
				( surface.redraw &&
					( surface.dynamic_content ||
					( entry.last_signature != surface.content_signature ) ||
					!surface.clear ) );

			if ( needs_render )
			{
				SDL_GPUColorTargetInfo color_target = {};
				color_target.texture = entry.msaa_color_texture ? entry.msaa_color_texture : entry.color_texture;
				color_target.mip_level = 0;
				color_target.layer_or_depth_plane = 0;
				color_target.clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 0.0f };
				color_target.load_op =
					( surface.clear || !entry.rendered_once )
						? SDL_GPU_LOADOP_CLEAR
						: SDL_GPU_LOADOP_LOAD;
				color_target.store_op =
					entry.msaa_color_texture
						? SDL_GPU_STOREOP_RESOLVE_AND_STORE
						: SDL_GPU_STOREOP_STORE;
				color_target.resolve_texture = entry.msaa_color_texture ? entry.color_texture : nullptr;
				color_target.resolve_mip_level = 0;
				color_target.resolve_layer = 0;

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
					true,
					&entry.vertex_buffer,
					&entry.vertex_buffer_size,
					&entry.vertex_signature,
					temporary_pbr_buffers,
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
					( surface.redraw &&
						( surface.dynamic_content ||
						( entry.last_signature != surface.content_signature ) ||
						!surface.clear ) );

				if ( needs_render )
				{
					SDL_GPUColorTargetInfo color_target = {};
					color_target.texture = entry.msaa_color_texture ? entry.msaa_color_texture : entry.color_texture;
					color_target.mip_level = 0;
					color_target.layer_or_depth_plane = 0;
					color_target.clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 0.0f };
					color_target.load_op =
						( surface.clear || !entry.rendered_once )
							? SDL_GPU_LOADOP_CLEAR
							: SDL_GPU_LOADOP_LOAD;
					color_target.store_op =
						entry.msaa_color_texture
							? SDL_GPU_STOREOP_RESOLVE_AND_STORE
							: SDL_GPU_STOREOP_STORE;
					color_target.resolve_texture = entry.msaa_color_texture ? entry.color_texture : nullptr;
					color_target.resolve_mip_level = 0;
					color_target.resolve_layer = 0;

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
						true,
						&entry.vertex_buffer,
						&entry.vertex_buffer_size,
						&entry.vertex_signature,
						temporary_pbr_buffers,
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
	if ( !compile_shader_blob( "__vertex__", build_builtin_vertex_shader(), true, vertex_blob ) )
	{
		FeLog() << "SDL: initialize_image_pipeline: failed to compile internal vertex shader" << std::endl;
		return false;
	}

	if ( !compile_shader_blob( "__alpha_prepass_fragment__", build_alpha_prepass_fragment_shader(), false, alpha_prepass_blob ) )
	{
		FeLog() << "SDL: initialize_image_pipeline: failed to compile internal alpha prepass shader" << std::endl;
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
				get_builtin_image_fragment_shader_name( mode ),
				fragment_source,
				false,
				fragment_blobs[ mode ] ) )
		{
			std::ostringstream stream;
			stream << "initialize_image_pipeline: failed to compile internal fragment shader for blend mode " << mode;
			FeLog() << "SDL: " << stream.str() << std::endl;
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
		FeLog() << "SDL: initialize_image_pipeline: SDL_CreateGPUShader failed for vertex shader" << std::endl;
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
		FeLog() << "SDL: initialize_image_pipeline: SDL_CreateGPUShader failed for alpha prepass shader" << std::endl;
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
			FeLog() << "SDL: " << stream.str() << std::endl;
			release_image_pipeline();
			return false;
		}
	}

	m_linear_sampler = create_sampler(
		SDL_GPU_FILTER_LINEAR,
		SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
		SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		false,
		true );
	if ( !m_linear_sampler )
	{
		FeLog() << "SDL: initialize_image_pipeline: SDL_CreateGPUSampler failed for linear sampler" << std::endl;
		release_image_pipeline();
		return false;
	}

	m_linear_repeat_sampler = create_sampler(
		SDL_GPU_FILTER_LINEAR,
		SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
		SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		false,
		true );
	if ( !m_linear_repeat_sampler )
	{
		FeLog() << "SDL: initialize_image_pipeline: SDL_CreateGPUSampler failed for linear repeat sampler" << std::endl;
		release_image_pipeline();
		return false;
	}

	m_linear_mipmap_sampler = create_sampler(
		SDL_GPU_FILTER_LINEAR,
		SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
		SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		true,
		true );
	if ( !m_linear_mipmap_sampler )
	{
		FeLog() << "SDL: initialize_image_pipeline: SDL_CreateGPUSampler failed for linear mipmap sampler" << std::endl;
		release_image_pipeline();
		return false;
	}

	m_linear_mipmap_repeat_sampler = create_sampler(
		SDL_GPU_FILTER_LINEAR,
		SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
		SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		true,
		true );
	if ( !m_linear_mipmap_repeat_sampler )
	{
		FeLog() << "SDL: initialize_image_pipeline: SDL_CreateGPUSampler failed for linear mipmap repeat sampler" << std::endl;
		release_image_pipeline();
		return false;
	}

	m_nearest_sampler = create_sampler(
		SDL_GPU_FILTER_NEAREST,
		SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		false,
		false );
	if ( !m_nearest_sampler )
	{
		FeLog() << "SDL: initialize_image_pipeline: SDL_CreateGPUSampler failed for nearest sampler" << std::endl;
		release_image_pipeline();
		return false;
	}

	m_nearest_repeat_sampler = create_sampler(
		SDL_GPU_FILTER_NEAREST,
		SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		false,
		false );
	if ( !m_nearest_repeat_sampler )
	{
		FeLog() << "SDL: initialize_image_pipeline: SDL_CreateGPUSampler failed for nearest repeat sampler" << std::endl;
		release_image_pipeline();
		return false;
	}

	m_nearest_mipmap_sampler = create_sampler(
		SDL_GPU_FILTER_NEAREST,
		SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		true,
		false );
	if ( !m_nearest_mipmap_sampler )
	{
		FeLog() << "SDL: initialize_image_pipeline: SDL_CreateGPUSampler failed for nearest mipmap sampler" << std::endl;
		release_image_pipeline();
		return false;
	}

	m_nearest_mipmap_repeat_sampler = create_sampler(
		SDL_GPU_FILTER_NEAREST,
		SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		true,
		false );
	if ( !m_nearest_mipmap_repeat_sampler )
	{
		FeLog() << "SDL: initialize_image_pipeline: SDL_CreateGPUSampler failed for nearest mipmap repeat sampler" << std::endl;
		release_image_pipeline();
		return false;
	}

	SDL_GPUVertexBufferDescription vertex_buffer = {};
	vertex_buffer.slot = 0;
	vertex_buffer.pitch = sizeof( FeRenderVertex );
	vertex_buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	vertex_buffer.instance_step_rate = 0;

	SDL_GPUVertexAttribute attributes[6] = {};
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
	attributes[3].location = 3;
	attributes[3].buffer_slot = 0;
	attributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	attributes[3].offset = offsetof( FeRenderVertex, u1 );
	attributes[4].location = 4;
	attributes[4].buffer_slot = 0;
	attributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	attributes[4].offset = offsetof( FeRenderVertex, nx );
	attributes[5].location = 5;
	attributes[5].buffer_slot = 0;
	attributes[5].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[5].offset = offsetof( FeRenderVertex, tx );

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
	pipeline_info.vertex_input_state.num_vertex_attributes = 6;
	pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
	pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
	pipeline_info.rasterizer_state.enable_depth_clip = true;
	pipeline_info.multisample_state.sample_count = m_sample_count;
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
		FeLog() << "SDL: initialize_image_pipeline: SDL_CreateGPUGraphicsPipeline failed for alpha prepass" << std::endl;
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
				FeLog() << "SDL: " << stream.str() << std::endl;
				release_image_pipeline();
				return false;
			}
		}
	}

	FeDebug() << "SDL: initialize_image_pipeline: success" << std::endl;
	return true;
}

bool FeSdl3GpuContext::initialize_pbr_pipeline( SDL_GPUTextureFormat swapchain_format )
{
	if ( !m_device || ( swapchain_format == SDL_GPU_TEXTUREFORMAT_INVALID ) )
		return false;

	ShaderBlob vertex_blob = {};
	ShaderBlob fragment_blob = {};
	if ( !compile_shader_blob( "__pbr_vertex__", build_pbr_vertex_shader(), true, vertex_blob ) )
	{
		FeLog() << "SDL: initialize_pbr_pipeline: failed to compile internal vertex shader" << std::endl;
		return false;
	}

	if ( !compile_shader_blob( "__pbr_fragment__", build_pbr_fragment_shader(), false, fragment_blob ) )
	{
		FeLog() << "SDL: initialize_pbr_pipeline: failed to compile internal fragment shader" << std::endl;
		return false;
	}

	SDL_GPUShaderCreateInfo vertex_info = {};
	vertex_info.code_size = vertex_blob.code.size();
	vertex_info.code = vertex_blob.code.data();
	vertex_info.entrypoint = vertex_blob.entrypoint;
	vertex_info.format = vertex_blob.format;
	vertex_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
	vertex_info.num_uniform_buffers = 1;

	m_pbr_vertex_shader = SDL_CreateGPUShader( m_device, &vertex_info );
	if ( !m_pbr_vertex_shader )
	{
		FeLog() << "SDL: initialize_pbr_pipeline: SDL_CreateGPUShader failed for vertex shader" << std::endl;
		release_pbr_pipeline();
		return false;
	}

	SDL_GPUShaderCreateInfo fragment_info = {};
	fragment_info.code_size = fragment_blob.code.size();
	fragment_info.code = fragment_blob.code.data();
	fragment_info.entrypoint = fragment_blob.entrypoint;
	fragment_info.format = fragment_blob.format;
	fragment_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	fragment_info.num_uniform_buffers = 1;
	fragment_info.num_samplers = 5;

	m_pbr_fragment_shader = SDL_CreateGPUShader( m_device, &fragment_info );
	if ( !m_pbr_fragment_shader )
	{
		FeLog() << "SDL: initialize_pbr_pipeline: SDL_CreateGPUShader failed for fragment shader" << std::endl;
		release_pbr_pipeline();
		return false;
	}

	SDL_GPUVertexBufferDescription vertex_buffers[2] = {};
	vertex_buffers[0].slot = 0;
	vertex_buffers[0].pitch = sizeof( FeRenderVertex );
	vertex_buffers[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	vertex_buffers[0].instance_step_rate = 0;
	vertex_buffers[1].slot = 1;
	vertex_buffers[1].pitch = sizeof( PbrInstanceEntry );
	vertex_buffers[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
	vertex_buffers[1].instance_step_rate = 0;

	SDL_GPUVertexAttribute attributes[13] = {};
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
	attributes[3].location = 3;
	attributes[3].buffer_slot = 0;
	attributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	attributes[3].offset = offsetof( FeRenderVertex, u1 );
	attributes[4].location = 4;
	attributes[4].buffer_slot = 0;
	attributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	attributes[4].offset = offsetof( FeRenderVertex, nx );
	attributes[5].location = 5;
	attributes[5].buffer_slot = 0;
	attributes[5].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[5].offset = offsetof( FeRenderVertex, tx );
	attributes[6].location = 6;
	attributes[6].buffer_slot = 1;
	attributes[6].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[6].offset = offsetof( PbrInstanceEntry, model ) + ( 0 * sizeof( float ) * 4 );
	attributes[7].location = 7;
	attributes[7].buffer_slot = 1;
	attributes[7].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[7].offset = offsetof( PbrInstanceEntry, model ) + ( 1 * sizeof( float ) * 4 );
	attributes[8].location = 8;
	attributes[8].buffer_slot = 1;
	attributes[8].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[8].offset = offsetof( PbrInstanceEntry, model ) + ( 2 * sizeof( float ) * 4 );
	attributes[9].location = 9;
	attributes[9].buffer_slot = 1;
	attributes[9].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[9].offset = offsetof( PbrInstanceEntry, model ) + ( 3 * sizeof( float ) * 4 );
	attributes[10].location = 10;
	attributes[10].buffer_slot = 1;
	attributes[10].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[10].offset = offsetof( PbrInstanceEntry, normal_matrix ) + ( 0 * sizeof( float ) * 4 );
	attributes[11].location = 11;
	attributes[11].buffer_slot = 1;
	attributes[11].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[11].offset = offsetof( PbrInstanceEntry, normal_matrix ) + ( 1 * sizeof( float ) * 4 );
	attributes[12].location = 12;
	attributes[12].buffer_slot = 1;
	attributes[12].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	attributes[12].offset = offsetof( PbrInstanceEntry, normal_matrix ) + ( 2 * sizeof( float ) * 4 );

	SDL_GPUColorTargetDescription color_target = {};
	color_target.format = swapchain_format;

	SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.vertex_shader = m_pbr_vertex_shader;
	pipeline_info.fragment_shader = m_pbr_fragment_shader;
	pipeline_info.vertex_input_state.vertex_buffer_descriptions = vertex_buffers;
	pipeline_info.vertex_input_state.num_vertex_buffers = 2;
	pipeline_info.vertex_input_state.vertex_attributes = attributes;
	pipeline_info.vertex_input_state.num_vertex_attributes = 13;
	pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
	pipeline_info.rasterizer_state.enable_depth_clip = true;
	pipeline_info.multisample_state.sample_count = m_sample_count;
	pipeline_info.multisample_state.sample_mask = 0;
	pipeline_info.target_info.color_target_descriptions = &color_target;
	pipeline_info.target_info.num_color_targets = 1;

	for ( int depth_mode = 0; depth_mode < 3; ++depth_mode )
	{
		configure_pipeline_depth_state( pipeline_info, depth_mode );
		for ( int blend_index = 0; blend_index < 2; ++blend_index )
		{
			color_target.blend_state = make_gpu_blend_state( blend_index == 1 ? FeBlend::Alpha : FeBlend::None );
			pipeline_info.target_info.color_target_descriptions = &color_target;
			for ( int double_sided = 0; double_sided < 2; ++double_sided )
			{
				pipeline_info.rasterizer_state.cull_mode =
					double_sided ? SDL_GPU_CULLMODE_NONE : SDL_GPU_CULLMODE_BACK;
				m_pbr_pipelines[ depth_mode ][ blend_index ][ double_sided ] =
					SDL_CreateGPUGraphicsPipeline( m_device, &pipeline_info );
				if ( !m_pbr_pipelines[ depth_mode ][ blend_index ][ double_sided ] )
				{
					FeLog() << "SDL: initialize_pbr_pipeline: SDL_CreateGPUGraphicsPipeline failed" << std::endl;
					release_pbr_pipeline();
					return false;
				}
			}
		}
	}

	FeDebug() << "SDL: initialize_pbr_pipeline: success" << std::endl;
	return true;
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

#if defined( SDL_PLATFORM_WINDOWS ) || defined( SDL_PLATFORM_LINUX )
	SDL_PropertiesID props = SDL_GetWindowProperties( m_window );
	if ( !props )
		return nullptr;

#if defined( SDL_PLATFORM_WINDOWS )
	return SDL_GetPointerProperty( props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr );
#else
	return reinterpret_cast<void *>(
		static_cast<std::uintptr_t>(
			SDL_GetNumberProperty( props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0 ) ) );
#endif
#else
	return nullptr;
#endif
}
