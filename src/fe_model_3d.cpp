#include "fe_model_3d.hpp"

#include "fe_base.hpp"
#include "fe_present.hpp"
#include "fe_settings.hpp"
#include "fe_util.hpp"
#include "base64.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

namespace
{
	constexpr float FE_PI = 3.14159265358979323846f;
	constexpr float FE_EPSILON = 1.0e-6f;
	constexpr float FE_DEFAULT_MODEL_3D_SCALE = 100.0f;

	struct ModelTangent
	{
		float x;
		float y;
		float z;
		float w;

		ModelTangent()
			: x( 1.0f ),
			  y( 0.0f ),
			  z( 0.0f ),
			  w( 1.0f )
		{
		}
	};

	struct ModelTextureRef
	{
		FeBaseTextureContainer *container;
		bool repeated;
		bool smooth;
		bool mipmap;
		int texcoord_set;
		float offset_u;
		float offset_v;
		float scale_u;
		float scale_v;

		ModelTextureRef()
			: container( nullptr ),
			  repeated( true ),
			  smooth( true ),
			  mipmap( true ),
			  texcoord_set( 0 ),
			  offset_u( 0.0f ),
			  offset_v( 0.0f ),
			  scale_u( 1.0f ),
			  scale_v( 1.0f )
		{
		}
	};

	struct ModelMaterial
	{
		std::string name;
		ModelTextureRef base_color_texture;
		ModelTextureRef metallic_roughness_texture;
		ModelTextureRef normal_texture;
		ModelTextureRef occlusion_texture;
		ModelTextureRef emissive_texture;
		float base_color_factor[4];
		float emissive_factor[3];
		float metallic_factor;
		float roughness_factor;
		float normal_scale;
		float occlusion_strength;
		float alpha_cutoff;
		int alpha_mode;
		bool unlit;
		bool double_sided;

		ModelMaterial()
			: metallic_factor( 1.0f ),
			  roughness_factor( 1.0f ),
			  normal_scale( 1.0f ),
			  occlusion_strength( 1.0f ),
			  alpha_cutoff( 0.5f ),
			  alpha_mode( FeRenderPbrAlphaOpaque ),
			  unlit( false ),
			  double_sided( false )
		{
			base_color_factor[0] = 1.0f;
			base_color_factor[1] = 1.0f;
			base_color_factor[2] = 1.0f;
			base_color_factor[3] = 1.0f;
			emissive_factor[0] = 0.0f;
			emissive_factor[1] = 0.0f;
			emissive_factor[2] = 0.0f;
		}
	};

	struct ModelPrimitive
	{
		std::vector<FeRenderVertex> vertices;
		ModelMaterial material;
		float bounds_aspect;
		float target_aspect[2];

		ModelPrimitive()
			: bounds_aspect( 1.0f )
		{
			target_aspect[0] = 1.0f;
			target_aspect[1] = 1.0f;
		}
	};

	bool model_texture_ref_matches( const ModelTextureRef &lhs, const ModelTextureRef &rhs )
	{
		return lhs.container == rhs.container
			&& lhs.repeated == rhs.repeated
			&& lhs.smooth == rhs.smooth
			&& lhs.mipmap == rhs.mipmap
			&& lhs.texcoord_set == rhs.texcoord_set
			&& lhs.offset_u == rhs.offset_u
			&& lhs.offset_v == rhs.offset_v
			&& lhs.scale_u == rhs.scale_u
			&& lhs.scale_v == rhs.scale_v;
	}

	bool model_material_matches( const ModelMaterial &lhs, const ModelMaterial &rhs )
	{
		if ( lhs.name != rhs.name
			|| !model_texture_ref_matches( lhs.base_color_texture, rhs.base_color_texture )
			|| !model_texture_ref_matches( lhs.metallic_roughness_texture, rhs.metallic_roughness_texture )
			|| !model_texture_ref_matches( lhs.normal_texture, rhs.normal_texture )
			|| !model_texture_ref_matches( lhs.occlusion_texture, rhs.occlusion_texture )
			|| !model_texture_ref_matches( lhs.emissive_texture, rhs.emissive_texture )
			|| lhs.metallic_factor != rhs.metallic_factor
			|| lhs.roughness_factor != rhs.roughness_factor
			|| lhs.normal_scale != rhs.normal_scale
			|| lhs.occlusion_strength != rhs.occlusion_strength
			|| lhs.alpha_cutoff != rhs.alpha_cutoff
			|| lhs.alpha_mode != rhs.alpha_mode
			|| lhs.unlit != rhs.unlit
			|| lhs.double_sided != rhs.double_sided )
		{
			return false;
		}

		for ( int i = 0; i < 4; ++i )
		{
			if ( lhs.base_color_factor[i] != rhs.base_color_factor[i] )
				return false;
		}
		for ( int i = 0; i < 3; ++i )
		{
			if ( lhs.emissive_factor[i] != rhs.emissive_factor[i] )
				return false;
		}

		return true;
	}

	std::uint64_t hash_combine_model_u64( std::uint64_t seed, std::uint64_t value )
	{
		return seed ^ ( value + 0x9e3779b97f4a7c15ULL + ( seed << 6 ) + ( seed >> 2 ) );
	}

	std::uint64_t hash_model_float( std::uint64_t seed, float value )
	{
		std::uint32_t bits = 0;
		std::memcpy( &bits, &value, sizeof( bits ) );
		return hash_combine_model_u64( seed, static_cast<std::uint64_t>( bits ) );
	}

	std::uint64_t hash_model_texture_ref( const ModelTextureRef &value, std::uint64_t seed )
	{
		seed = hash_combine_model_u64( seed, reinterpret_cast<std::uint64_t>( value.container ) );
		seed = hash_combine_model_u64( seed, static_cast<std::uint64_t>( value.repeated ? 1 : 0 ) );
		seed = hash_combine_model_u64( seed, static_cast<std::uint64_t>( value.smooth ? 1 : 0 ) );
		seed = hash_combine_model_u64( seed, static_cast<std::uint64_t>( value.mipmap ? 1 : 0 ) );
		seed = hash_combine_model_u64( seed, static_cast<std::uint64_t>( value.texcoord_set ) );
		seed = hash_model_float( seed, value.offset_u );
		seed = hash_model_float( seed, value.offset_v );
		seed = hash_model_float( seed, value.scale_u );
		seed = hash_model_float( seed, value.scale_v );
		return seed;
	}

	std::uint64_t compute_model_material_hash( const ModelMaterial &material )
	{
		std::uint64_t hash = std::hash<std::string>{}( material.name );
		hash = hash_model_texture_ref( material.base_color_texture, hash );
		hash = hash_model_texture_ref( material.metallic_roughness_texture, hash );
		hash = hash_model_texture_ref( material.normal_texture, hash );
		hash = hash_model_texture_ref( material.occlusion_texture, hash );
		hash = hash_model_texture_ref( material.emissive_texture, hash );
		for ( int i = 0; i < 4; ++i )
			hash = hash_model_float( hash, material.base_color_factor[i] );
		for ( int i = 0; i < 3; ++i )
			hash = hash_model_float( hash, material.emissive_factor[i] );
		hash = hash_model_float( hash, material.metallic_factor );
		hash = hash_model_float( hash, material.roughness_factor );
		hash = hash_model_float( hash, material.normal_scale );
		hash = hash_model_float( hash, material.occlusion_strength );
		hash = hash_model_float( hash, material.alpha_cutoff );
		hash = hash_combine_model_u64( hash, static_cast<std::uint64_t>( material.alpha_mode ) );
		hash = hash_combine_model_u64( hash, static_cast<std::uint64_t>( material.unlit ? 1 : 0 ) );
		hash = hash_combine_model_u64( hash, static_cast<std::uint64_t>( material.double_sided ? 1 : 0 ) );
		return hash;
	}

	struct ModelLight
	{
		int type;
		Vec3f color;
		float intensity;
		Vec3f position;
		Vec3f direction;
		float range;
		float inner_cone_cos;
		float outer_cone_cos;
	};

	class FeModel3DTextureContainer : public FeBaseTextureContainer
	{
	public:
		FeModel3DTextureContainer(
			std::vector<unsigned char> pixels,
			unsigned int width,
			unsigned int height )
			: m_pixels( std::move( pixels ) ),
			  m_size( width, height ),
			  m_smooth( true ),
			  m_mipmap( true ),
			  m_repeat( true )
		{
		}

		Vec2u get_texture_size() const override
		{
			return m_size;
		}

		bool copy_pixels_rgba( std::vector<unsigned char> &pixels, unsigned int &width, unsigned int &height ) const override
		{
			width = m_size.x;
			height = m_size.y;
			if ( m_pixels.empty() || width == 0 || height == 0 )
				return false;

			pixels = m_pixels;
			return true;
		}

		bool copy_pixels_rgba_to( void *pixels, std::size_t pixel_count, unsigned int &width, unsigned int &height ) const override
		{
			width = m_size.x;
			height = m_size.y;
			if ( m_pixels.empty() || width == 0 || height == 0 )
				return false;

			if ( !pixels )
				return true;

			const std::size_t required = static_cast<std::size_t>( width ) * static_cast<std::size_t>( height ) * 4u;
			if ( pixel_count < required )
				return false;

			std::memcpy( pixels, m_pixels.data(), required );
			return true;
		}

		void on_new_selection( FeSettings * ) override {}
		void on_end_navigation( FeSettings * ) override {}
		void on_new_list( FeSettings *, bool ) override {}

		void set_smooth( bool smooth ) override { m_smooth = smooth; }
		bool get_smooth() const override { return m_smooth; }
		void set_mipmap( bool mipmap ) override { m_mipmap = mipmap; }
		bool get_mipmap() const override { return m_mipmap; }
		void set_repeat( bool repeat ) override { m_repeat = repeat; }
		bool get_repeat() const override { return m_repeat; }

	private:
		std::vector<unsigned char> m_pixels;
		Vec2u m_size;
		bool m_smooth;
		bool m_mipmap;
		bool m_repeat;
	};

	Vec3f operator+( const Vec3f &lhs, const Vec3f &rhs )
	{
		return Vec3f( lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z );
	}

	Vec3f operator-( const Vec3f &lhs, const Vec3f &rhs )
	{
		return Vec3f( lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z );
	}

	Vec3f operator*( const Vec3f &lhs, float scalar )
	{
		return Vec3f( lhs.x * scalar, lhs.y * scalar, lhs.z * scalar );
	}

	Vec3f operator/( const Vec3f &lhs, float scalar )
	{
		return Vec3f( lhs.x / scalar, lhs.y / scalar, lhs.z / scalar );
	}

	Vec3f flip_layout_y( const Vec3f &value )
	{
		return Vec3f( value.x, -value.y, value.z );
	}

	float dot( const Vec3f &lhs, const Vec3f &rhs )
	{
		return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
	}

	Vec3f cross( const Vec3f &lhs, const Vec3f &rhs )
	{
		return Vec3f(
			lhs.y * rhs.z - lhs.z * rhs.y,
			lhs.z * rhs.x - lhs.x * rhs.z,
			lhs.x * rhs.y - lhs.y * rhs.x );
	}

	float length_squared( const Vec3f &value )
	{
		return dot( value, value );
	}

	float length( const Vec3f &value )
	{
		return std::sqrt( length_squared( value ) );
	}

	Vec3f normalize( const Vec3f &value, const Vec3f &fallback = Vec3f( 0.0f, 0.0f, 1.0f ) )
	{
		const float len = length( value );
		if ( len <= FE_EPSILON )
			return fallback;

		return value / len;
	}

	float safe_extent( float value )
	{
		return ( std::fabs( value ) > FE_EPSILON ) ? value : 1.0f;
	}

	Vec3f transform_point( const float *matrix, const Vec3f &point )
	{
		return Vec3f(
			point.x * matrix[0] + point.y * matrix[4] + point.z * matrix[8] + matrix[12],
			point.x * matrix[1] + point.y * matrix[5] + point.z * matrix[9] + matrix[13],
			point.x * matrix[2] + point.y * matrix[6] + point.z * matrix[10] + matrix[14] );
	}

	Vec3f transform_direction( const float *matrix, const Vec3f &value )
	{
		return Vec3f(
			value.x * matrix[0] + value.y * matrix[4] + value.z * matrix[8],
			value.x * matrix[1] + value.y * matrix[5] + value.z * matrix[9],
			value.x * matrix[2] + value.y * matrix[6] + value.z * matrix[10] );
	}

	bool invert_linear_3x3( const float *matrix, float *out_inverse )
	{
		const float a00 = matrix[0];
		const float a01 = matrix[4];
		const float a02 = matrix[8];
		const float a10 = matrix[1];
		const float a11 = matrix[5];
		const float a12 = matrix[9];
		const float a20 = matrix[2];
		const float a21 = matrix[6];
		const float a22 = matrix[10];

		const float det =
			a00 * ( a11 * a22 - a12 * a21 ) -
			a01 * ( a10 * a22 - a12 * a20 ) +
			a02 * ( a10 * a21 - a11 * a20 );
		if ( std::fabs( det ) <= FE_EPSILON )
			return false;

		const float inv_det = 1.0f / det;
		out_inverse[0] = ( a11 * a22 - a12 * a21 ) * inv_det;
		out_inverse[1] = ( a02 * a21 - a01 * a22 ) * inv_det;
		out_inverse[2] = ( a01 * a12 - a02 * a11 ) * inv_det;
		out_inverse[3] = ( a12 * a20 - a10 * a22 ) * inv_det;
		out_inverse[4] = ( a00 * a22 - a02 * a20 ) * inv_det;
		out_inverse[5] = ( a02 * a10 - a00 * a12 ) * inv_det;
		out_inverse[6] = ( a10 * a21 - a11 * a20 ) * inv_det;
		out_inverse[7] = ( a01 * a20 - a00 * a21 ) * inv_det;
		out_inverse[8] = ( a00 * a11 - a01 * a10 ) * inv_det;
		return true;
	}

	Vec3f transform_normal( const float *matrix, const Vec3f &normal )
	{
		float inverse[9] = {};
		if ( !invert_linear_3x3( matrix, inverse ) )
			return normalize( transform_direction( matrix, normal ) );

		return normalize(
			Vec3f(
				inverse[0] * normal.x + inverse[3] * normal.y + inverse[6] * normal.z,
				inverse[1] * normal.x + inverse[4] * normal.y + inverse[7] * normal.z,
				inverse[2] * normal.x + inverse[5] * normal.y + inverse[8] * normal.z ) );
	}

	Vec3f rotate_x( const Vec3f &value, float radians )
	{
		const float c = std::cos( radians );
		const float s = std::sin( radians );
		return Vec3f(
			value.x,
			value.y * c - value.z * s,
			value.y * s + value.z * c );
	}

	Vec3f rotate_y( const Vec3f &value, float radians )
	{
		const float c = std::cos( radians );
		const float s = std::sin( radians );
		return Vec3f(
			value.x * c + value.z * s,
			value.y,
			-value.x * s + value.z * c );
	}

	Vec3f rotate_z( const Vec3f &value, float radians )
	{
		const float c = std::cos( radians );
		const float s = std::sin( radians );
		return Vec3f(
			value.x * c - value.y * s,
			value.x * s + value.y * c,
			value.z );
	}

	Vec3f apply_rotation_order(
		Vec3f value,
		float rotation_x,
		float rotation_y,
		float rotation_z,
		int rotation_order )
	{
		const float rx = rotation_x * FE_PI / 180.0f;
		const float ry = rotation_y * FE_PI / 180.0f;
		const float rz = rotation_z * FE_PI / 180.0f;

		switch ( rotation_order )
		{
			case FeBasePresentable::XYZ:
				value = rotate_x( value, rx );
				value = rotate_y( value, ry );
				value = rotate_z( value, rz );
				break;
			case FeBasePresentable::XZY:
				value = rotate_x( value, rx );
				value = rotate_z( value, rz );
				value = rotate_y( value, ry );
				break;
			case FeBasePresentable::YXZ:
				value = rotate_y( value, ry );
				value = rotate_x( value, rx );
				value = rotate_z( value, rz );
				break;
			case FeBasePresentable::YZX:
				value = rotate_y( value, ry );
				value = rotate_z( value, rz );
				value = rotate_x( value, rx );
				break;
			case FeBasePresentable::ZXY:
				value = rotate_z( value, rz );
				value = rotate_x( value, rx );
				value = rotate_y( value, ry );
				break;
			case FeBasePresentable::ZYX:
			default:
				value = rotate_z( value, rz );
				value = rotate_y( value, ry );
				value = rotate_x( value, rx );
				break;
		}

		return value;
	}

	Vec3f transform_instance_position(
		const Vec3f &position,
		const Vec3f &source_center,
		const Vec3f &target_center,
		const Vec3f &scale,
		float rotation_x,
		float rotation_y,
		float rotation_z,
		int rotation_order )
	{
		Vec3f value = position - source_center;
		value.x *= scale.x;
		value.y *= scale.y;
		value.z *= scale.z;
		value = apply_rotation_order( value, rotation_x, rotation_y, rotation_z, rotation_order );
		return value + target_center;
	}

	Vec3f transform_instance_normal(
		const Vec3f &normal,
		const Vec3f &scale,
		float rotation_x,
		float rotation_y,
		float rotation_z,
		int rotation_order )
	{
		Vec3f value(
			normal.x / safe_extent( scale.x ),
			normal.y / safe_extent( scale.y ),
			normal.z / safe_extent( scale.z ) );
		value = apply_rotation_order( value, rotation_x, rotation_y, rotation_z, rotation_order );
		return normalize( value );
	}

	Vec3f transform_instance_tangent(
		const Vec3f &tangent,
		const Vec3f &scale,
		float rotation_x,
		float rotation_y,
		float rotation_z,
		int rotation_order )
	{
		Vec3f value(
			tangent.x * scale.x,
			tangent.y * scale.y,
			tangent.z * scale.z );
		value = apply_rotation_order( value, rotation_x, rotation_y, rotation_z, rotation_order );
		return normalize( value, Vec3f( 1.0f, 0.0f, 0.0f ) );
	}

	void set_identity4( float *matrix )
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

	void multiply_matrix4( const float *lhs, const float *rhs, float *out )
	{
		float result[16] = {};
		for ( int column = 0; column < 4; ++column )
		{
			for ( int row = 0; row < 4; ++row )
			{
				float value = 0.0f;
				for ( int k = 0; k < 4; ++k )
					value += lhs[ k * 4 + row ] * rhs[ column * 4 + k ];

				result[ column * 4 + row ] = value;
			}
		}

		std::memcpy( out, result, sizeof( result ) );
	}

	void multiply_matrix4_inplace( float *lhs, const float *rhs )
	{
		if ( !lhs || !rhs )
			return;

		float result[16] = {};
		multiply_matrix4( lhs, rhs, result );
		std::memcpy( lhs, result, sizeof( result ) );
	}

	void append_translation( float *matrix, float x, float y, float z )
	{
		float translation[16];
		set_identity4( translation );
		translation[12] = x;
		translation[13] = y;
		translation[14] = z;
		multiply_matrix4_inplace( matrix, translation );
	}

	void append_scale( float *matrix, float x, float y, float z )
	{
		float scale_matrix[16];
		set_identity4( scale_matrix );
		scale_matrix[0] = x;
		scale_matrix[5] = y;
		scale_matrix[10] = z;
		multiply_matrix4_inplace( matrix, scale_matrix );
	}

	void append_rotation_x( float *matrix, float radians )
	{
		float rotation[16];
		set_identity4( rotation );
		const float c = std::cos( radians );
		const float s = std::sin( radians );
		rotation[5] = c;
		rotation[9] = -s;
		rotation[6] = s;
		rotation[10] = c;
		multiply_matrix4_inplace( matrix, rotation );
	}

	void append_rotation_y( float *matrix, float radians )
	{
		float rotation[16];
		set_identity4( rotation );
		const float c = std::cos( radians );
		const float s = std::sin( radians );
		rotation[0] = c;
		rotation[8] = s;
		rotation[2] = -s;
		rotation[10] = c;
		multiply_matrix4_inplace( matrix, rotation );
	}

	void append_rotation_z( float *matrix, float radians )
	{
		float rotation[16];
		set_identity4( rotation );
		const float c = std::cos( radians );
		const float s = std::sin( radians );
		rotation[0] = c;
		rotation[4] = -s;
		rotation[1] = s;
		rotation[5] = c;
		multiply_matrix4_inplace( matrix, rotation );
	}

	void append_rotation_order(
		float *matrix,
		float rotation_x,
		float rotation_y,
		float rotation_z,
		int rotation_order )
	{
		const float rx = rotation_x * FE_PI / 180.0f;
		const float ry = rotation_y * FE_PI / 180.0f;
		const float rz = rotation_z * FE_PI / 180.0f;

		switch ( rotation_order )
		{
			case FeBasePresentable::XYZ:
				append_rotation_x( matrix, rx );
				append_rotation_y( matrix, ry );
				append_rotation_z( matrix, rz );
				break;
			case FeBasePresentable::XZY:
				append_rotation_x( matrix, rx );
				append_rotation_z( matrix, rz );
				append_rotation_y( matrix, ry );
				break;
			case FeBasePresentable::YXZ:
				append_rotation_y( matrix, ry );
				append_rotation_x( matrix, rx );
				append_rotation_z( matrix, rz );
				break;
			case FeBasePresentable::YZX:
				append_rotation_y( matrix, ry );
				append_rotation_z( matrix, rz );
				append_rotation_x( matrix, rx );
				break;
			case FeBasePresentable::ZXY:
				append_rotation_z( matrix, rz );
				append_rotation_x( matrix, rx );
				append_rotation_y( matrix, ry );
				break;
			case FeBasePresentable::ZYX:
			default:
				append_rotation_z( matrix, rz );
				append_rotation_y( matrix, ry );
				append_rotation_x( matrix, rx );
				break;
		}
	}

	void build_model_model_matrix(
		float *matrix,
		const Vec3f &source_center,
		const Vec3f &target_center,
		const Vec3f &scale,
		float rotation_x,
		float rotation_y,
		float rotation_z,
		int rotation_order )
	{
		set_identity4( matrix );
		append_translation( matrix, target_center.x, target_center.y, target_center.z );
		append_rotation_order( matrix, rotation_x, rotation_y, rotation_z, rotation_order );
		append_scale( matrix, scale.x, scale.y, scale.z );
		append_translation( matrix, -source_center.x, -source_center.y, -source_center.z );
	}

	bool build_normal_matrix3( const float *model_matrix, float *out_normal_matrix )
	{
		float inverse[9] = {};
		if ( !invert_linear_3x3( model_matrix, inverse ) )
			return false;

		out_normal_matrix[0] = inverse[0];
		out_normal_matrix[1] = inverse[3];
		out_normal_matrix[2] = inverse[6];
		out_normal_matrix[3] = inverse[1];
		out_normal_matrix[4] = inverse[4];
		out_normal_matrix[5] = inverse[7];
		out_normal_matrix[6] = inverse[2];
		out_normal_matrix[7] = inverse[5];
		out_normal_matrix[8] = inverse[8];
		return true;
	}

	std::string get_parent_path( const std::string &path )
	{
		const std::size_t slash = path.find_last_of( "/\\" );
		if ( slash == std::string::npos )
			return std::string();

		return path.substr( 0, slash + 1 );
	}

	bool read_binary_file_content( const std::string &path, std::vector<unsigned char> &bytes )
	{
		std::string content;
		if ( !read_file_content( path, content ) )
			return false;

		bytes.assign( content.begin(), content.end() );
		return true;
	}

	std::string decode_uri( const std::string &uri )
	{
		std::string decoded;
		decoded.reserve( uri.size() );

		for ( std::size_t i = 0; i < uri.size(); ++i )
		{
			if ( uri[i] == '%' && i + 2 < uri.size() )
			{
				const std::string hex = uri.substr( i + 1, 2 );
				char *end = nullptr;
				const long value = std::strtol( hex.c_str(), &end, 16 );
				if ( end && *end == '\0' )
				{
					decoded.push_back( static_cast<char>( value ) );
					i += 2;
					continue;
				}
			}

			decoded.push_back( uri[i] );
		}

		return decoded;
	}

	bool is_data_uri( const std::string &uri )
	{
		return uri.rfind( "data:", 0 ) == 0;
	}

	std::vector<unsigned char> decode_data_uri( const std::string &uri )
	{
		const std::size_t comma = uri.find( ',' );
		if ( comma == std::string::npos || comma + 1 >= uri.size() )
			return {};

		return base64_decode( uri.substr( comma + 1 ) );
	}

	bool decode_image_rgba(
		const std::vector<unsigned char> &image_bytes,
		std::vector<unsigned char> &pixels,
		unsigned int &width,
		unsigned int &height )
	{
		width = 0;
		height = 0;
		if ( image_bytes.empty() )
			return false;

		SDL_IOStream *stream = SDL_IOFromConstMem( image_bytes.data(), image_bytes.size() );
		if ( !stream )
			return false;

		SDL_Surface *surface = IMG_Load_IO( stream, true );
		if ( !surface )
			return false;

		SDL_Surface *rgba_surface = SDL_ConvertSurface( surface, SDL_PIXELFORMAT_RGBA32 );
		SDL_DestroySurface( surface );
		if ( !rgba_surface )
			return false;

		width = static_cast<unsigned int>( rgba_surface->w );
		height = static_cast<unsigned int>( rgba_surface->h );
		const std::size_t pixel_count = static_cast<std::size_t>( width ) * static_cast<std::size_t>( height ) * 4u;
		pixels.resize( pixel_count );
		std::memcpy( pixels.data(), rgba_surface->pixels, pixel_count );
		SDL_DestroySurface( rgba_surface );
		return true;
	}

	bool sampler_uses_smooth_filter( const cgltf_sampler *sampler )
	{
		if ( !sampler )
			return true;

		switch ( sampler->mag_filter )
		{
			case cgltf_filter_type_nearest:
			case cgltf_filter_type_nearest_mipmap_nearest:
			case cgltf_filter_type_nearest_mipmap_linear:
				return false;
			default:
				return true;
		}
	}

	bool sampler_uses_mipmap( const cgltf_sampler *sampler )
	{
		if ( !sampler )
			return true;

		switch ( sampler->min_filter )
		{
			case cgltf_filter_type_nearest_mipmap_nearest:
			case cgltf_filter_type_linear_mipmap_nearest:
			case cgltf_filter_type_nearest_mipmap_linear:
			case cgltf_filter_type_linear_mipmap_linear:
				return true;
			case cgltf_filter_type_nearest:
			case cgltf_filter_type_linear:
				return false;
			case cgltf_filter_type_undefined:
			default:
				return true;
		}
	}

	bool sampler_uses_repeat( const cgltf_sampler *sampler )
	{
		if ( !sampler )
			return true;

		return sampler->wrap_s != cgltf_wrap_mode_clamp_to_edge
			|| sampler->wrap_t != cgltf_wrap_mode_clamp_to_edge;
	}

	const cgltf_accessor *find_attribute_accessor(
		const cgltf_primitive &primitive,
		cgltf_attribute_type type,
		int index )
	{
		for ( cgltf_size i = 0; i < primitive.attributes_count; ++i )
		{
			const cgltf_attribute &attribute = primitive.attributes[i];
			if ( attribute.type == type && attribute.index == index )
				return attribute.data;
		}

		return nullptr;
	}

	void build_triangle_index_list(
		const cgltf_primitive &primitive,
		cgltf_size vertex_count,
		std::vector<cgltf_uint> &indices )
	{
		indices.clear();
		std::vector<cgltf_uint> source_indices;
		const cgltf_size index_count = primitive.indices ? primitive.indices->count : vertex_count;
		source_indices.reserve( static_cast<std::size_t>( index_count ) );

		if ( primitive.indices )
		{
			for ( cgltf_size i = 0; i < primitive.indices->count; ++i )
				source_indices.push_back( static_cast<cgltf_uint>( cgltf_accessor_read_index( primitive.indices, i ) ) );
		}
		else
		{
			for ( cgltf_uint i = 0; i < vertex_count; ++i )
				source_indices.push_back( i );
		}

		switch ( primitive.type )
		{
			case cgltf_primitive_type_triangles:
				indices = source_indices;
				break;

			case cgltf_primitive_type_triangle_strip:
				if ( source_indices.size() < 3 )
					return;

				indices.reserve( ( source_indices.size() - 2 ) * 3 );
				for ( std::size_t i = 0; i + 2 < source_indices.size(); ++i )
				{
					if ( i % 2 == 0 )
					{
						indices.push_back( source_indices[i] );
						indices.push_back( source_indices[i + 1] );
						indices.push_back( source_indices[i + 2] );
					}
					else
					{
						indices.push_back( source_indices[i + 1] );
						indices.push_back( source_indices[i] );
						indices.push_back( source_indices[i + 2] );
					}
				}
				break;

			case cgltf_primitive_type_triangle_fan:
				if ( source_indices.size() < 3 )
					return;

				indices.reserve( ( source_indices.size() - 2 ) * 3 );
				for ( std::size_t i = 1; i + 1 < source_indices.size(); ++i )
				{
					indices.push_back( source_indices[0] );
					indices.push_back( source_indices[i] );
					indices.push_back( source_indices[i + 1] );
				}
				break;

			default:
				break;
		}
	}

	void reverse_triangle_winding( std::vector<cgltf_uint> &indices )
	{
		for ( std::size_t i = 0; i + 2 < indices.size(); i += 3 )
			std::swap( indices[i + 1], indices[i + 2] );
	}

	void compute_normals(
		const std::vector<Vec3f> &positions,
		const std::vector<cgltf_uint> &indices,
		std::vector<Vec3f> &normals )
	{
		normals.assign( positions.size(), Vec3f( 0.0f, 0.0f, 0.0f ) );
		for ( std::size_t i = 0; i + 2 < indices.size(); i += 3 )
		{
			const cgltf_uint i0 = indices[i];
			const cgltf_uint i1 = indices[i + 1];
			const cgltf_uint i2 = indices[i + 2];
			if ( i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size() )
				continue;

			const Vec3f edge_0 = positions[i1] - positions[i0];
			const Vec3f edge_1 = positions[i2] - positions[i0];
			const Vec3f face_normal = cross( edge_0, edge_1 );
			if ( length_squared( face_normal ) <= FE_EPSILON )
				continue;

			normals[i0] = normals[i0] + face_normal;
			normals[i1] = normals[i1] + face_normal;
			normals[i2] = normals[i2] + face_normal;
		}

		for ( Vec3f &normal : normals )
			normal = normalize( normal );
	}

	bool compute_tangents(
		const std::vector<Vec3f> &positions,
		const std::vector<Vec3f> &normals,
		const std::vector<Vec2f> &uvs,
		const std::vector<cgltf_uint> &indices,
		std::vector<ModelTangent> &tangents )
	{
		if ( positions.size() != normals.size() || positions.size() != uvs.size() )
			return false;

		std::vector<Vec3f> tangent_sum( positions.size(), Vec3f( 0.0f, 0.0f, 0.0f ) );
		std::vector<Vec3f> bitangent_sum( positions.size(), Vec3f( 0.0f, 0.0f, 0.0f ) );
		bool valid_triangle = false;

		for ( std::size_t i = 0; i + 2 < indices.size(); i += 3 )
		{
			const cgltf_uint i0 = indices[i];
			const cgltf_uint i1 = indices[i + 1];
			const cgltf_uint i2 = indices[i + 2];
			if ( i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size() )
				continue;

			const Vec3f edge_0 = positions[i1] - positions[i0];
			const Vec3f edge_1 = positions[i2] - positions[i0];
			const Vec2f delta_uv_0 = uvs[i1] - uvs[i0];
			const Vec2f delta_uv_1 = uvs[i2] - uvs[i0];

			const float divisor = delta_uv_0.x * delta_uv_1.y - delta_uv_1.x * delta_uv_0.y;
			if ( std::fabs( divisor ) <= FE_EPSILON )
				continue;

			const float inv_divisor = 1.0f / divisor;
			const Vec3f tangent =
				( edge_0 * delta_uv_1.y - edge_1 * delta_uv_0.y ) * inv_divisor;
			const Vec3f bitangent =
				( edge_1 * delta_uv_0.x - edge_0 * delta_uv_1.x ) * inv_divisor;
			tangent_sum[i0] = tangent_sum[i0] + tangent;
			tangent_sum[i1] = tangent_sum[i1] + tangent;
			tangent_sum[i2] = tangent_sum[i2] + tangent;
			bitangent_sum[i0] = bitangent_sum[i0] + bitangent;
			bitangent_sum[i1] = bitangent_sum[i1] + bitangent;
			bitangent_sum[i2] = bitangent_sum[i2] + bitangent;
			valid_triangle = true;
		}

		if ( !valid_triangle )
			return false;

		tangents.resize( positions.size() );
		for ( std::size_t i = 0; i < positions.size(); ++i )
		{
			const Vec3f normal = normalize( normals[i] );
			Vec3f tangent = tangent_sum[i] - normal * dot( normal, tangent_sum[i] );
			tangent = normalize( tangent, Vec3f( 1.0f, 0.0f, 0.0f ) );
			const Vec3f handedness = cross( normal, tangent );
			const float sign = dot( handedness, bitangent_sum[i] ) < 0.0f ? -1.0f : 1.0f;
			tangents[i].x = tangent.x;
			tangents[i].y = tangent.y;
			tangents[i].z = tangent.z;
			tangents[i].w = sign;
		}

		return true;
	}

	bool binding_is_dynamic( const FeRenderTextureBinding &binding )
	{
		return binding.texture_id != nullptr && binding.dynamic;
	}

	void fill_render_texture_binding(
		FeRenderTextureBinding &binding,
		const ModelTextureRef &source,
		const FeBaseTextureContainer *override_container = nullptr )
	{
		binding.clear();
		const FeBaseTextureContainer *container = override_container ? override_container : source.container;
		if ( !container )
			return;

		const Vec2u texture_size = container->get_texture_size();
		if ( texture_size.x == 0 || texture_size.y == 0 )
			return;

		binding.texture_id = container->get_texture_source_id();
		binding.texture_source_type = container->get_texture_source_type();
		binding.repeated = override_container ? container->get_repeat() : source.repeated;
		binding.smooth = override_container ? container->get_smooth() : source.smooth;
		binding.mipmap = override_container ? container->get_mipmap() : source.mipmap;
		binding.dynamic = container->is_volatile_texture();
		binding.width = static_cast<float>( texture_size.x );
		binding.height = static_cast<float>( texture_size.y );
		binding.content_version = container->get_texture_content_version();
		binding.offset_u = source.offset_u;
		binding.offset_v = source.offset_v;
		binding.scale_u = source.scale_u;
		binding.scale_v = source.scale_v;
		binding.texcoord_set = source.texcoord_set;
	}

	void log_material_extension_ignored( const char *extension_name, bool &logged )
	{
		if ( logged )
			return;

		logged = true;
		FeDebug() << "FeModel3D: ignoring unsupported optional glTF material extension " << extension_name << std::endl;
	}

	bool has_nonzero_emissive_factor( const ModelMaterial &material )
	{
		return std::fabs( material.emissive_factor[0] ) > FE_EPSILON
			|| std::fabs( material.emissive_factor[1] ) > FE_EPSILON
			|| std::fabs( material.emissive_factor[2] ) > FE_EPSILON;
	}

	bool material_name_suggests_glass( const std::string &name )
	{
		if ( name.empty() )
			return false;

		std::string lower_name = name;
		std::transform(
			lower_name.begin(),
			lower_name.end(),
			lower_name.begin(),
			[]( unsigned char value ) { return static_cast<char>( std::tolower( value ) ); } );
		return lower_name.find( "glass" ) != std::string::npos
			|| lower_name.find( "cristal" ) != std::string::npos
			|| lower_name.find( "crystal" ) != std::string::npos;
	}

	void normalize_emissive_override_factor( float *emissive_factor )
	{
		if ( !emissive_factor )
			return;

		const float max_value =
			std::max( emissive_factor[0], std::max( emissive_factor[1], emissive_factor[2] ) );
		if ( max_value <= FE_EPSILON )
		{
			emissive_factor[0] = 1.0f;
			emissive_factor[1] = 1.0f;
			emissive_factor[2] = 1.0f;
			return;
		}

		if ( max_value <= 1.0f )
			return;

		emissive_factor[0] /= max_value;
		emissive_factor[1] /= max_value;
		emissive_factor[2] /= max_value;
	}

	Vec2f get_vertex_texcoord( const FeRenderVertex &vertex, int texcoord_set )
	{
		if ( texcoord_set == 1 )
			return Vec2f( vertex.u1, vertex.v1 );

		return Vec2f( vertex.u, vertex.v );
	}

	float estimate_primitive_bounds_aspect( const ModelPrimitive &primitive )
	{
		if ( primitive.vertices.empty() )
			return 1.0f;

		float min_x = primitive.vertices[0].x;
		float max_x = primitive.vertices[0].x;
		float min_y = primitive.vertices[0].y;
		float max_y = primitive.vertices[0].y;
		float min_z = primitive.vertices[0].z;
		float max_z = primitive.vertices[0].z;

		for ( const FeRenderVertex &vertex : primitive.vertices )
		{
			min_x = std::min( min_x, vertex.x );
			max_x = std::max( max_x, vertex.x );
			min_y = std::min( min_y, vertex.y );
			max_y = std::max( max_y, vertex.y );
			min_z = std::min( min_z, vertex.z );
			max_z = std::max( max_z, vertex.z );
		}

		float extents[3] =
		{
			max_x - min_x,
			max_y - min_y,
			max_z - min_z
		};
		std::sort( extents, extents + 3 );
		return ( extents[1] > FE_EPSILON ) ? ( extents[2] / extents[1] ) : 1.0f;
	}

	float estimate_primitive_target_aspect( const ModelPrimitive &primitive, int texcoord_set )
	{
		if ( primitive.vertices.size() < 3 )
			return estimate_primitive_bounds_aspect( primitive );

		float min_u = std::numeric_limits<float>::max();
		float max_u = -std::numeric_limits<float>::max();
		float min_v = std::numeric_limits<float>::max();
		float max_v = -std::numeric_limits<float>::max();
		float tangent_length_sum = 0.0f;
		float bitangent_length_sum = 0.0f;
		float weight_sum = 0.0f;

		for ( std::size_t i = 0; i < primitive.vertices.size(); i += 3 )
		{
			if ( i + 2 >= primitive.vertices.size() )
				break;

			const FeRenderVertex &v0 = primitive.vertices[i];
			const FeRenderVertex &v1 = primitive.vertices[i + 1];
			const FeRenderVertex &v2 = primitive.vertices[i + 2];
			const Vec2f uv0 = get_vertex_texcoord( v0, texcoord_set );
			const Vec2f uv1 = get_vertex_texcoord( v1, texcoord_set );
			const Vec2f uv2 = get_vertex_texcoord( v2, texcoord_set );
			min_u = std::min( min_u, std::min( uv0.x, std::min( uv1.x, uv2.x ) ) );
			max_u = std::max( max_u, std::max( uv0.x, std::max( uv1.x, uv2.x ) ) );
			min_v = std::min( min_v, std::min( uv0.y, std::min( uv1.y, uv2.y ) ) );
			max_v = std::max( max_v, std::max( uv0.y, std::max( uv1.y, uv2.y ) ) );

			const Vec3f p0( v0.x, v0.y, v0.z );
			const Vec3f p1( v1.x, v1.y, v1.z );
			const Vec3f p2( v2.x, v2.y, v2.z );
			const Vec3f delta_pos1 = p1 - p0;
			const Vec3f delta_pos2 = p2 - p0;
			const Vec2f delta_uv1( uv1.x - uv0.x, uv1.y - uv0.y );
			const Vec2f delta_uv2( uv2.x - uv0.x, uv2.y - uv0.y );
			const float determinant = ( delta_uv1.x * delta_uv2.y ) - ( delta_uv2.x * delta_uv1.y );
			const float triangle_weight = length( cross( delta_pos1, delta_pos2 ) );
			if ( std::fabs( determinant ) <= FE_EPSILON || triangle_weight <= FE_EPSILON )
				continue;

			const float inv_determinant = 1.0f / determinant;
			const Vec3f tangent =
				( delta_pos1 * delta_uv2.y - delta_pos2 * delta_uv1.y ) * inv_determinant;
			const Vec3f bitangent =
				( delta_pos2 * delta_uv1.x - delta_pos1 * delta_uv2.x ) * inv_determinant;
			tangent_length_sum += length( tangent ) * triangle_weight;
			bitangent_length_sum += length( bitangent ) * triangle_weight;
			weight_sum += triangle_weight;
		}

		const float uv_width = max_u - min_u;
		const float uv_height = max_v - min_v;
		if ( weight_sum > FE_EPSILON && uv_width > FE_EPSILON && uv_height > FE_EPSILON )
		{
			const float width = ( tangent_length_sum / weight_sum ) * uv_width;
			const float height = ( bitangent_length_sum / weight_sum ) * uv_height;
			if ( width > FE_EPSILON && height > FE_EPSILON )
				return width / height;
		}

		return estimate_primitive_bounds_aspect( primitive );
	}

	void apply_preserve_aspect_ratio_fit(
		FeRenderTextureBinding &binding,
		const ModelPrimitive &primitive,
		float sample_aspect_ratio )
	{
		if ( !binding.texture_id || binding.width <= FE_EPSILON || binding.height <= FE_EPSILON )
			return;

		float target_aspect = primitive.target_aspect[ binding.texcoord_set == 1 ? 1 : 0 ];
		if ( target_aspect <= FE_EPSILON )
			target_aspect = primitive.bounds_aspect;
		const float safe_sample_aspect_ratio =
			( sample_aspect_ratio > FE_EPSILON ) ? sample_aspect_ratio : 1.0f;
		const float source_aspect =
			( binding.width * safe_sample_aspect_ratio ) / std::max( binding.height, FE_EPSILON );
		if ( target_aspect <= FE_EPSILON || source_aspect <= FE_EPSILON )
			return;

		binding.fit_scale_u = 1.0f;
		binding.fit_scale_v = 1.0f;

		if ( source_aspect > target_aspect + FE_EPSILON )
			binding.fit_scale_v = target_aspect / source_aspect;
		else if ( source_aspect + FE_EPSILON < target_aspect )
			binding.fit_scale_u = source_aspect / target_aspect;
	}

}

struct FeModel3D::ModelData
{
	std::vector<std::unique_ptr<FeBaseTextureContainer>> image_containers;
	std::vector<ModelPrimitive> primitives;
	std::vector<ModelLight> lights;
	Vec3f bounds_min;
	Vec3f bounds_max;

	ModelData()
		: bounds_min( std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() ),
		  bounds_max( -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() )
	{
	}

	bool has_bounds() const
	{
		return bounds_min.x <= bounds_max.x
			&& bounds_min.y <= bounds_max.y
			&& bounds_min.z <= bounds_max.z;
	}
};

struct FeModel3D::MaterialOverride
{
	enum SourceType
	{
		SourceNone = 0,
		SourceArtwork,
		SourceFile
	};

	std::string material_name;
	FeTextureContainer *container;
	std::unique_ptr<FeModel3DMaterialArtwork> handle;
	FeShader *shader;
	SourceType source_type;
	std::string source_value;
	int index_offset;
	int filter_offset;
	int video_flags;
	int trigger;
	bool preserve_aspect_ratio;
	bool smooth;
	bool mipmap;
	bool repeat;
	float volume;
	float pan;
	bool play_state;
	bool use_play_state;

	MaterialOverride()
		: container( nullptr ),
		  shader( nullptr ),
		  source_type( SourceNone ),
		  index_offset( 0 ),
		  filter_offset( 0 ),
		  video_flags( VF_Normal ),
		  trigger( ToNewSelection ),
		  preserve_aspect_ratio( false ),
		  smooth( false ),
		  mipmap( true ),
		  repeat( false ),
		  volume( 100.0f ),
		  pan( 0.0f ),
		  play_state( true ),
		  use_play_state( false )
	{
	}
};

FeModel3DMaterialArtwork::FeModel3DMaterialArtwork( FeModel3D &model, const std::string &material_name )
	: m_model( &model ),
	  m_material_name( material_name )
{
}

const char *FeModel3DMaterialArtwork::get_material_name() const
{
	return m_material_name.c_str();
}

int FeModel3DMaterialArtwork::get_index_offset() const
{
	const FeModel3D::MaterialOverride *entry =
		m_model ? m_model->find_override( m_material_name ) : nullptr;
	return entry ? entry->index_offset : 0;
}

void FeModel3DMaterialArtwork::set_index_offset( int io )
{
	if ( !m_model )
		return;

	FeModel3D::MaterialOverride *entry = m_model->find_or_create_override( m_material_name );
	entry->index_offset = io;
	if ( entry->container )
		entry->container->set_index_offset( io, true );
}

int FeModel3DMaterialArtwork::get_filter_offset() const
{
	const FeModel3D::MaterialOverride *entry =
		m_model ? m_model->find_override( m_material_name ) : nullptr;
	return entry ? entry->filter_offset : 0;
}

void FeModel3DMaterialArtwork::set_filter_offset( int fo )
{
	if ( !m_model )
		return;

	FeModel3D::MaterialOverride *entry = m_model->find_or_create_override( m_material_name );
	entry->filter_offset = fo;
	if ( entry->container )
		entry->container->set_filter_offset( fo, true );
}

void FeModel3DMaterialArtwork::rawset_index_offset( int io )
{
	if ( !m_model )
		return;

	FeModel3D::MaterialOverride *entry = m_model->find_or_create_override( m_material_name );
	entry->index_offset = io;
	if ( entry->container )
		entry->container->set_index_offset( io, false );
}

void FeModel3DMaterialArtwork::rawset_filter_offset( int fo )
{
	if ( !m_model )
		return;

	FeModel3D::MaterialOverride *entry = m_model->find_or_create_override( m_material_name );
	entry->filter_offset = fo;
	if ( entry->container )
		entry->container->set_filter_offset( fo, false );
}

int FeModel3DMaterialArtwork::get_video_flags() const
{
	const FeModel3D::MaterialOverride *entry =
		m_model ? m_model->find_override( m_material_name ) : nullptr;
	return entry ? entry->video_flags : VF_Normal;
}

void FeModel3DMaterialArtwork::set_video_flags( int flags )
{
	if ( !m_model )
		return;

	FeModel3D::MaterialOverride *entry = m_model->find_or_create_override( m_material_name );
	entry->video_flags = flags;
	if ( entry->container )
		entry->container->set_video_flags( static_cast<FeVideoFlags>( flags ) );
}

bool FeModel3DMaterialArtwork::get_movie_enabled() const
{
	return ( get_video_flags() & VF_DisableVideo ) == 0;
}

void FeModel3DMaterialArtwork::set_movie_enabled( bool enabled )
{
	int flags = get_video_flags();
	if ( enabled )
		flags &= ~VF_DisableVideo;
	else
		flags |= VF_DisableVideo;

	set_video_flags( flags );
}

bool FeModel3DMaterialArtwork::get_video_playing() const
{
	const FeModel3D::MaterialOverride *entry =
		m_model ? m_model->find_override( m_material_name ) : nullptr;
	if ( !entry )
		return false;

	if ( entry->container )
		return entry->container->get_play_state();

	return entry->use_play_state && entry->play_state;
}

void FeModel3DMaterialArtwork::set_video_playing( bool playing )
{
	if ( !m_model )
		return;

	FeModel3D::MaterialOverride *entry = m_model->find_or_create_override( m_material_name );
	entry->play_state = playing;
	entry->use_play_state = true;
	if ( entry->container )
		entry->container->set_play_state( playing );
}

int FeModel3DMaterialArtwork::get_video_duration() const
{
	const FeModel3D::MaterialOverride *entry =
		m_model ? m_model->find_override( m_material_name ) : nullptr;
	return ( entry && entry->container ) ? entry->container->get_video_duration() : 0;
}

int FeModel3DMaterialArtwork::get_video_time() const
{
	const FeModel3D::MaterialOverride *entry =
		m_model ? m_model->find_override( m_material_name ) : nullptr;
	return ( entry && entry->container ) ? entry->container->get_video_time() : 0;
}

bool FeModel3DMaterialArtwork::get_preserve_aspect_ratio() const
{
	const FeModel3D::MaterialOverride *entry =
		m_model ? m_model->find_override( m_material_name ) : nullptr;
	return entry ? entry->preserve_aspect_ratio : false;
}

void FeModel3DMaterialArtwork::set_preserve_aspect_ratio( bool preserve )
{
	if ( !m_model )
		return;

	FeModel3D::MaterialOverride *entry = m_model->find_or_create_override( m_material_name );
	if ( entry->preserve_aspect_ratio == preserve )
		return;

	entry->preserve_aspect_ratio = preserve;
	m_model->invalidate_geometry_cache();
	FePresent::script_flag_redraw();
}

const char *FeModel3DMaterialArtwork::get_file_name() const
{
	const FeModel3D::MaterialOverride *entry =
		m_model ? m_model->find_override( m_material_name ) : nullptr;
	return ( entry && entry->container ) ? entry->container->get_file_name() : "";
}

void FeModel3DMaterialArtwork::set_file_name( const char *filename )
{
	set_file( filename );
}

int FeModel3DMaterialArtwork::get_trigger() const
{
	const FeModel3D::MaterialOverride *entry =
		m_model ? m_model->find_override( m_material_name ) : nullptr;
	return entry ? entry->trigger : ToNewSelection;
}

void FeModel3DMaterialArtwork::set_trigger( int trigger )
{
	if ( !m_model )
		return;

	FeModel3D::MaterialOverride *entry = m_model->find_or_create_override( m_material_name );
	entry->trigger = trigger;
	if ( entry->container )
		entry->container->set_trigger( trigger );
}

float FeModel3DMaterialArtwork::get_volume() const
{
	const FeModel3D::MaterialOverride *entry =
		m_model ? m_model->find_override( m_material_name ) : nullptr;
	return entry ? entry->volume : 100.0f;
}

void FeModel3DMaterialArtwork::set_volume( float volume )
{
	if ( !m_model )
		return;

	FeModel3D::MaterialOverride *entry = m_model->find_or_create_override( m_material_name );
	if ( volume < 0.0f )
		volume = 0.0f;
	if ( volume > 100.0f )
		volume = 100.0f;
	entry->volume = volume;
	if ( entry->container )
		entry->container->set_volume( volume );
}

float FeModel3DMaterialArtwork::get_pan() const
{
	const FeModel3D::MaterialOverride *entry =
		m_model ? m_model->find_override( m_material_name ) : nullptr;
	return entry ? entry->pan : 0.0f;
}

void FeModel3DMaterialArtwork::set_pan( float pan )
{
	if ( !m_model )
		return;

	FeModel3D::MaterialOverride *entry = m_model->find_or_create_override( m_material_name );
	entry->pan = std::clamp( pan, -1.0f, 1.0f );
	if ( entry->container )
		entry->container->set_pan( entry->pan );
}

bool FeModel3DMaterialArtwork::get_smooth() const
{
	const FeModel3D::MaterialOverride *entry =
		m_model ? m_model->find_override( m_material_name ) : nullptr;
	return entry ? entry->smooth : false;
}

void FeModel3DMaterialArtwork::set_smooth( bool smooth )
{
	if ( !m_model )
		return;

	FeModel3D::MaterialOverride *entry = m_model->find_or_create_override( m_material_name );
	if ( entry->smooth == smooth )
		return;

	entry->smooth = smooth;
	if ( entry->container )
		entry->container->set_smooth( smooth );
	m_model->invalidate_geometry_cache();
	FePresent::script_flag_redraw();
}

bool FeModel3DMaterialArtwork::get_mipmap() const
{
	const FeModel3D::MaterialOverride *entry =
		m_model ? m_model->find_override( m_material_name ) : nullptr;
	return entry ? entry->mipmap : true;
}

void FeModel3DMaterialArtwork::set_mipmap( bool mipmap )
{
	if ( !m_model )
		return;

	FeModel3D::MaterialOverride *entry = m_model->find_or_create_override( m_material_name );
	if ( entry->mipmap == mipmap )
		return;

	entry->mipmap = mipmap;
	if ( entry->container )
		entry->container->set_mipmap( mipmap );
	m_model->invalidate_geometry_cache();
	FePresent::script_flag_redraw();
}

bool FeModel3DMaterialArtwork::get_repeat() const
{
	const FeModel3D::MaterialOverride *entry =
		m_model ? m_model->find_override( m_material_name ) : nullptr;
	return entry ? entry->repeat : false;
}

void FeModel3DMaterialArtwork::set_repeat( bool repeat )
{
	if ( !m_model )
		return;

	FeModel3D::MaterialOverride *entry = m_model->find_or_create_override( m_material_name );
	if ( entry->repeat == repeat )
		return;

	entry->repeat = repeat;
	if ( entry->container )
		entry->container->set_repeat( repeat );
	m_model->invalidate_geometry_cache();
	FePresent::script_flag_redraw();
}

float FeModel3DMaterialArtwork::get_sample_aspect_ratio() const
{
	const FeModel3D::MaterialOverride *entry =
		m_model ? m_model->find_override( m_material_name ) : nullptr;
	return ( entry && entry->container ) ? entry->container->get_sample_aspect_ratio() : 1.0f;
}

FeShader *FeModel3DMaterialArtwork::get_shader() const
{
	const FeModel3D::MaterialOverride *entry =
		m_model ? m_model->find_override( m_material_name ) : nullptr;
	return entry ? entry->shader : nullptr;
}

void FeModel3DMaterialArtwork::set_shader( FeShader *shader )
{
	if ( !m_model )
		return;

	FeModel3D::MaterialOverride *entry = m_model->find_or_create_override( m_material_name );
	if ( entry->shader == shader )
		return;

	entry->shader = shader;
	m_model->invalidate_geometry_cache();
	FePresent::script_flag_redraw();
}

void FeModel3DMaterialArtwork::set_artwork( const char *artwork_label )
{
	if ( !m_model )
		return;

	m_model->add_material_artwork( m_material_name.c_str(), artwork_label );
}

void FeModel3DMaterialArtwork::set_file( const char *filename )
{
	if ( !m_model )
		return;

	m_model->add_material_image( m_material_name.c_str(), filename );
}

void FeModel3DMaterialArtwork::clear()
{
	if ( !m_model )
		return;

	m_model->clear_material_texture( m_material_name.c_str() );
}

FeModel3D::FeModel3D( FePresentableParent &p, const std::string &filename )
	: FeBasePresentable( p ),
	  m_file_name( filename ),
	  m_pos( 0.0f, 0.0f ),
	  m_size( 1.0f, 1.0f ),
	  m_anchor( 0.0f, 0.0f, 0.0f ),
	  m_depth( 1.0f ),
	  m_rotation( 0.0f ),
	  m_color( Color::White ),
	  m_geometry_cache_valid( false ),
	  m_geometry_cache_model( nullptr ),
	  m_geometry_cache_pos( 0.0f, 0.0f ),
	  m_geometry_cache_size( 0.0f, 0.0f ),
	  m_geometry_cache_anchor( 0.0f, 0.0f, 0.0f ),
	  m_geometry_cache_depth( 0.0f ),
	  m_geometry_cache_rotation( 0.0f ),
	  m_geometry_cache_z( 0.0f ),
	  m_geometry_cache_rotation_x( 0.0f ),
	  m_geometry_cache_rotation_y( 0.0f ),
	  m_geometry_cache_rotation_order( 0 ),
	  m_geometry_cache_color( Color::White ),
	  m_geometry_cache_zbuffer( false ),
	  m_geometry_cache_camera_light( 0.0f )
{
	set_zbuffer( true );
	load_model( filename );
}

FeModel3D::FeModel3D( FeModel3D *o, FePresentableParent &p )
	: FeBasePresentable( p ),
	  m_file_name( o ? o->m_file_name : "" ),
	  m_pos( o ? o->m_pos : Vec2f( 0.0f, 0.0f ) ),
	  m_size( o ? o->m_size : Vec2f( 1.0f, 1.0f ) ),
	  m_anchor( o ? o->m_anchor : Vec3f( 0.0f, 0.0f, 0.0f ) ),
	  m_depth( o ? o->m_depth : 1.0f ),
	  m_rotation( o ? o->m_rotation : 0.0f ),
	  m_color( o ? o->m_color : Color::White ),
	  m_model( o ? o->m_model : nullptr ),
	  m_geometry_cache_valid( false ),
	  m_geometry_cache_model( nullptr ),
	  m_geometry_cache_pos( 0.0f, 0.0f ),
	  m_geometry_cache_size( 0.0f, 0.0f ),
	  m_geometry_cache_anchor( 0.0f, 0.0f, 0.0f ),
	  m_geometry_cache_depth( 0.0f ),
	  m_geometry_cache_rotation( 0.0f ),
	  m_geometry_cache_z( 0.0f ),
	  m_geometry_cache_rotation_x( 0.0f ),
	  m_geometry_cache_rotation_y( 0.0f ),
	  m_geometry_cache_rotation_order( 0 ),
	  m_geometry_cache_color( Color::White ),
	  m_geometry_cache_zbuffer( false ),
	  m_geometry_cache_camera_light( 0.0f )
{
	if ( !o )
		return;

	set_visible( o->get_visible() );
	set_z( o->get_z() );
	script_set_shader( o->get_shader() );
	set_rotation_x( o->get_rotation_x() );
	set_rotation_y( o->get_rotation_y() );
	set_rotation_order( o->get_rotation_order() );
	set_zorder( o->get_zorder() );
	set_zbuffer( o->get_zbuffer() );

	for ( const std::unique_ptr<MaterialOverride> &source_override : o->m_overrides )
	{
		if ( !source_override )
			continue;

		MaterialOverride *target_override =
			find_or_create_override( source_override->material_name );
		target_override->source_type = source_override->source_type;
		target_override->source_value = source_override->source_value;
		target_override->shader = source_override->shader;
		target_override->index_offset = source_override->index_offset;
		target_override->filter_offset = source_override->filter_offset;
		target_override->video_flags = source_override->video_flags;
		target_override->trigger = source_override->trigger;
		target_override->preserve_aspect_ratio = source_override->preserve_aspect_ratio;
		target_override->smooth = source_override->smooth;
		target_override->mipmap = source_override->mipmap;
		target_override->repeat = source_override->repeat;
		target_override->volume = source_override->volume;
		target_override->pan = source_override->pan;
		target_override->play_state = source_override->play_state;
		target_override->use_play_state = source_override->use_play_state;

		if ( !source_override->container )
			continue;

		FeTextureContainer *container = nullptr;
		if ( source_override->source_type == MaterialOverride::SourceArtwork )
			container = create_override_artwork_container( source_override->source_value.c_str() );
		else if ( source_override->source_type == MaterialOverride::SourceFile )
			container = create_override_file_container( source_override->source_value.c_str() );

		if ( container )
			set_override_container( source_override->material_name, container );
	}
}

FeModel3D::~FeModel3D()
{
	release_overrides();
}

Vec2f FeModel3D::getPosition() const
{
	return m_pos;
}

void FeModel3D::setPosition( const Vec2f &pos )
{
	if ( pos == m_pos )
		return;

	m_pos = pos;
	FePresent::script_flag_redraw();
}

Vec2f FeModel3D::getSize() const
{
	return m_size;
}

void FeModel3D::setSize( const Vec2f &size )
{
	if ( size == m_size )
		return;

	m_size = size;
	FePresent::script_flag_redraw();
}

float FeModel3D::getRotation() const
{
	return m_rotation;
}

void FeModel3D::setRotation( float rotation )
{
	if ( rotation == m_rotation )
		return;

	m_rotation = rotation;
	FePresent::script_flag_redraw();
}

Color FeModel3D::getColor() const
{
	return m_color;
}

void FeModel3D::setColor( Color color )
{
	if ( color == m_color )
		return;

	m_color = color;
	FePresent::script_flag_redraw();
}

void FeModel3D::set_anchor( float x, float y )
{
	set_anchor( x, y, m_anchor.z );
}

void FeModel3D::set_anchor( float x, float y, float z )
{
	if ( x == m_anchor.x && y == m_anchor.y && z == m_anchor.z )
		return;

	m_anchor = Vec3f( x, y, z );
	FePresent::script_flag_redraw();
}

void FeModel3D::set_anchor_x( float x )
{
	set_anchor( x, get_anchor_y(), get_anchor_z() );
}

float FeModel3D::get_anchor_x() const
{
	return m_anchor.x;
}

void FeModel3D::set_anchor_y( float y )
{
	set_anchor( get_anchor_x(), y, get_anchor_z() );
}

float FeModel3D::get_anchor_y() const
{
	return m_anchor.y;
}

void FeModel3D::set_anchor_z( float z )
{
	set_anchor( get_anchor_x(), get_anchor_y(), z );
}

float FeModel3D::get_anchor_z() const
{
	return m_anchor.z;
}

float FeModel3D::get_depth() const
{
	return m_depth;
}

void FeModel3D::set_depth( float depth )
{
	if ( depth == m_depth )
		return;

	m_depth = depth;
	FePresent::script_flag_redraw();
}

const char *FeModel3D::get_file_name() const
{
	return m_file_name.c_str();
}

void FeModel3D::initialize_override_defaults( MaterialOverride &entry )
{
	FePresent *fep = FePresent::script_get_fep();
	if ( fep && fep->get_fes() )
		entry.smooth = fep->get_fes()->get_info_bool( FeSettings::SmoothImages );
	else
		entry.smooth = false;

	entry.mipmap = true;
	entry.repeat = false;
}

FeModel3D::MaterialOverride *FeModel3D::find_override( const std::string &material_name )
{
	for ( const std::unique_ptr<MaterialOverride> &entry : m_overrides )
		if ( entry && entry->material_name == material_name )
			return entry.get();

	return nullptr;
}

const FeModel3D::MaterialOverride *FeModel3D::find_override( const std::string &material_name ) const
{
	for ( const std::unique_ptr<MaterialOverride> &entry : m_overrides )
		if ( entry && entry->material_name == material_name )
			return entry.get();

	return nullptr;
}

FeModel3D::MaterialOverride *FeModel3D::find_or_create_override( const std::string &material_name )
{
	if ( MaterialOverride *entry = find_override( material_name ) )
		return entry;

	std::unique_ptr<MaterialOverride> entry( new MaterialOverride() );
	entry->material_name = material_name;
	initialize_override_defaults( *entry );
	entry->handle.reset( new FeModel3DMaterialArtwork( *this, material_name ) );
	MaterialOverride *result = entry.get();
	m_overrides.push_back( std::move( entry ) );
	return result;
}

void FeModel3D::release_overrides()
{
	for ( const std::unique_ptr<MaterialOverride> &entry : m_overrides )
	{
		if ( !entry || !entry->container )
			continue;

		FePresent::script_unregister_texture_container( entry->container );
		delete entry->container;
	}

	m_overrides.clear();
}

void FeModel3D::invalidate_geometry_cache() const
{
	m_geometry_cache_valid = false;
}

void FeModel3D::clear_override( const std::string &material_name )
{
	MaterialOverride *entry = find_override( material_name );
	if ( !entry )
		return;

	entry->source_type = MaterialOverride::SourceNone;
	entry->source_value.clear();
	if ( !entry->container )
		return;

	FePresent::script_unregister_texture_container( entry->container );
	delete entry->container;
	entry->container = nullptr;
	invalidate_geometry_cache();
	FePresent::script_flag_redraw();
}

void FeModel3D::apply_override_settings( MaterialOverride &entry, bool do_update )
{
	if ( !entry.container )
		return;

	entry.container->set_smooth( entry.smooth );
	entry.container->set_mipmap( entry.mipmap );
	entry.container->set_repeat( entry.repeat );
	entry.container->set_index_offset( entry.index_offset, false );
	entry.container->set_filter_offset( entry.filter_offset, false );
	entry.container->set_video_flags( static_cast<FeVideoFlags>( entry.video_flags ) );
	entry.container->set_trigger( entry.trigger );
	entry.container->set_volume( entry.volume );
	entry.container->set_pan( entry.pan );

	if ( do_update )
	{
		FePresent::script_do_update( entry.container, true );
		if ( entry.use_play_state )
			entry.container->set_play_state( entry.play_state );
	}
}

FeTextureContainer *FeModel3D::create_override_artwork_container( const char *artwork_label ) const
{
	if ( !artwork_label )
		return nullptr;

	return new FeTextureContainer( true, artwork_label );
}

FeTextureContainer *FeModel3D::create_override_file_container( const char *filename ) const
{
	if ( !filename )
		return nullptr;

	FeTextureContainer *container = new FeTextureContainer( false );
	container->load_file( filename );
	return container;
}

void FeModel3D::set_override_container( const std::string &material_name, FeTextureContainer *container )
{
	if ( !container )
		return;

	MaterialOverride *entry = find_or_create_override( material_name );
	if ( entry->container )
	{
		FePresent::script_unregister_texture_container( entry->container );
		delete entry->container;
	}
	entry->container = container;

	FePresent::script_register_texture_container( container );
	apply_override_settings( *entry, true );
	invalidate_geometry_cache();
}

FeModel3DMaterialArtwork *FeModel3D::get_material_artwork( const char *material_name )
{
	if ( !material_name )
		return nullptr;

	MaterialOverride *entry = find_or_create_override( material_name );
	return entry ? entry->handle.get() : nullptr;
}

FeModel3DMaterialArtwork *FeModel3D::add_material_artwork( const char *material_name, const char *artwork_label )
{
	if ( !material_name || !artwork_label )
		return nullptr;

	MaterialOverride *entry = find_or_create_override( material_name );
	entry->source_type = MaterialOverride::SourceArtwork;
	entry->source_value = artwork_label;
	set_override_container( material_name, create_override_artwork_container( artwork_label ) );
	return entry->handle.get();
}

FeModel3DMaterialArtwork *FeModel3D::add_material_image( const char *material_name, const char *filename )
{
	if ( !material_name || !filename )
		return nullptr;

	MaterialOverride *entry = find_or_create_override( material_name );
	entry->source_type = MaterialOverride::SourceFile;
	entry->source_value = filename;
	set_override_container( material_name, create_override_file_container( filename ) );
	return entry->handle.get();
}

void FeModel3D::clear_material_texture( const char *material_name )
{
	if ( !material_name )
		return;

	clear_override( material_name );
}

void FeModel3D::load_model( const std::string &filename )
{
	m_model.reset();

	std::string resolved_path = clean_path( filename );
	if ( resolved_path.empty() )
		return;

	if ( is_relative_path( resolved_path ) )
		resolved_path = FePresent::script_get_base_path() + resolved_path;
	resolved_path = absolute_path( clean_path( resolved_path ) );

	static std::unordered_map<std::string, std::weak_ptr<ModelData>> s_model_cache;
	auto cached_model_it = s_model_cache.find( resolved_path );
	if ( cached_model_it != s_model_cache.end() )
	{
		if ( std::shared_ptr<ModelData> cached_model = cached_model_it->second.lock() )
		{
			const Vec3f bounds_size = cached_model->bounds_max - cached_model->bounds_min;
			m_size = Vec2f(
				( ( std::fabs( bounds_size.x ) > FE_EPSILON ) ? bounds_size.x : 1.0f ) * FE_DEFAULT_MODEL_3D_SCALE,
				( ( std::fabs( bounds_size.y ) > FE_EPSILON ) ? bounds_size.y : 1.0f ) * FE_DEFAULT_MODEL_3D_SCALE );
			m_depth = ( ( std::fabs( bounds_size.z ) > FE_EPSILON ) ? bounds_size.z : 1.0f ) * FE_DEFAULT_MODEL_3D_SCALE;
			m_model = std::move( cached_model );
			return;
		}

		s_model_cache.erase( cached_model_it );
	}

	std::vector<unsigned char> file_bytes;
	if ( !read_binary_file_content( resolved_path, file_bytes ) )
	{
		FeLog() << "FeModel3D: failed to read glTF file " << resolved_path << std::endl;
		return;
	}

	cgltf_options options = {};
	cgltf_data *data = nullptr;
	cgltf_result result = cgltf_parse( &options, file_bytes.data(), file_bytes.size(), &data );
	if ( result != cgltf_result_success || !data )
	{
		FeLog() << "FeModel3D: failed to parse glTF file " << resolved_path << std::endl;
		return;
	}

	const auto free_data = [&]()
	{
		cgltf_free( data );
	};

	if ( cgltf_load_buffers( &options, data, resolved_path.c_str() ) != cgltf_result_success )
	{
		FeLog() << "FeModel3D: failed to load glTF buffers for " << resolved_path << std::endl;
		free_data();
		return;
	}

	if ( cgltf_validate( data ) != cgltf_result_success )
	{
		FeLog() << "FeModel3D: validation failed for " << resolved_path << std::endl;
		free_data();
		return;
	}

	for ( cgltf_size i = 0; i < data->extensions_required_count; ++i )
	{
		const std::string extension = data->extensions_required[i] ? data->extensions_required[i] : "";
		if ( extension == "KHR_lights_punctual"
			|| extension == "KHR_materials_unlit"
			|| extension == "KHR_texture_transform" )
		{
			continue;
		}

		FeLog() << "FeModel3D: unsupported required glTF extension " << extension << " in " << resolved_path << std::endl;
		free_data();
		return;
	}

	std::shared_ptr<ModelData> model( new ModelData() );
	std::unordered_map<const cgltf_image *, FeBaseTextureContainer *> image_map;
	image_map.reserve( data->images_count );

	const std::string model_dir = get_parent_path( resolved_path );

	for ( cgltf_size image_index = 0; image_index < data->images_count; ++image_index )
	{
		cgltf_image &image = data->images[image_index];
		std::vector<unsigned char> image_bytes;

		if ( image.buffer_view && image.buffer_view->buffer && image.buffer_view->buffer->data )
		{
			const unsigned char *source =
				static_cast<const unsigned char *>( image.buffer_view->buffer->data ) + image.buffer_view->offset;
			const std::size_t size = static_cast<std::size_t>( image.buffer_view->size );
			image_bytes.assign( source, source + size );
		}
		else if ( image.uri && image.uri[0] )
		{
			const std::string uri = image.uri;
			if ( is_data_uri( uri ) )
				image_bytes = decode_data_uri( uri );
			else
			{
				std::string image_path = decode_uri( uri );
				if ( is_relative_path( image_path ) )
					image_path = clean_path( model_dir + image_path );
				if ( !read_binary_file_content( image_path, image_bytes ) )
				{
					FeLog() << "FeModel3D: failed to read glTF image " << image_path << std::endl;
					continue;
				}
			}
		}

		std::vector<unsigned char> rgba_pixels;
		unsigned int width = 0;
		unsigned int height = 0;
		if ( !decode_image_rgba( image_bytes, rgba_pixels, width, height ) )
		{
			FeLog() << "FeModel3D: failed to decode glTF image in " << resolved_path << std::endl;
			continue;
		}

		std::unique_ptr<FeModel3DTextureContainer> container(
			new FeModel3DTextureContainer( std::move( rgba_pixels ), width, height ) );
		image_map[ &image ] = container.get();
		model->image_containers.push_back( std::move( container ) );
	}

	bool logged_spec_gloss = false;
	bool logged_clearcoat = false;
	bool logged_transmission = false;
	bool logged_volume = false;
	bool logged_sheen = false;
	bool logged_specular = false;
	bool logged_ior = false;
	bool logged_iridescence = false;
	bool logged_diffuse_transmission = false;
	bool logged_anisotropy = false;
	bool logged_dispersion = false;
	bool logged_skipped_primitives = false;
	bool logged_tangent_failure = false;
	bool logged_extra_lights = false;
	bool logged_glass_fallback = false;

	auto make_texture_ref = [&]( const cgltf_texture_view &view ) -> ModelTextureRef
	{
		ModelTextureRef binding;
		if ( !view.texture || !view.texture->image )
			return binding;

		auto it = image_map.find( view.texture->image );
		if ( it == image_map.end() )
			return binding;

		binding.container = it->second;
		binding.smooth = sampler_uses_smooth_filter( view.texture->sampler );
		binding.mipmap = sampler_uses_mipmap( view.texture->sampler );
		binding.repeated = sampler_uses_repeat( view.texture->sampler );
		binding.texcoord_set =
			( view.has_transform && view.transform.has_texcoord )
				? view.transform.texcoord
				: view.texcoord;
		if ( view.has_transform )
		{
			binding.offset_u = view.transform.offset[0];
			binding.offset_v = view.transform.offset[1];
			binding.scale_u = view.transform.scale[0];
			binding.scale_v = view.transform.scale[1];
		}
		return binding;
	};

	auto collect_scene_roots = [&]( std::vector<const cgltf_node *> &roots )
	{
		if ( data->scene && data->scene->nodes_count > 0 )
		{
			for ( cgltf_size i = 0; i < data->scene->nodes_count; ++i )
				roots.push_back( data->scene->nodes[i] );
			return;
		}

		if ( data->scenes_count > 0 && data->scenes[0].nodes_count > 0 )
		{
			for ( cgltf_size i = 0; i < data->scenes[0].nodes_count; ++i )
				roots.push_back( data->scenes[0].nodes[i] );
			return;
		}

		for ( cgltf_size i = 0; i < data->nodes_count; ++i )
			if ( !data->nodes[i].parent )
				roots.push_back( &data->nodes[i] );
	};

	std::vector<const cgltf_node *> roots;
	collect_scene_roots( roots );
	if ( roots.empty() )
	{
		for ( cgltf_size i = 0; i < data->nodes_count; ++i )
			roots.push_back( &data->nodes[i] );
	}

	std::unordered_map<std::uint64_t, std::vector<std::size_t>> primitive_merge_lookup;
	std::vector<const cgltf_node *> stack = roots;
	while ( !stack.empty() )
	{
		const cgltf_node *node = stack.back();
		stack.pop_back();
		if ( !node )
			continue;

		for ( cgltf_size child_index = 0; child_index < node->children_count; ++child_index )
			stack.push_back( node->children[child_index] );

		float world_matrix[16] = {};
		cgltf_node_transform_world( node, world_matrix );

		if ( node->light )
		{
			if ( model->lights.size() >= 4 )
			{
				if ( !logged_extra_lights )
				{
					logged_extra_lights = true;
					FeDebug() << "FeModel3D: limiting glTF lights to 4 in " << resolved_path << std::endl;
				}
			}
			else
			{
				ModelLight light = {};
				light.type =
					( node->light->type == cgltf_light_type_directional ) ? FeRenderPbrLightDirectional :
					( node->light->type == cgltf_light_type_point ) ? FeRenderPbrLightPoint :
					FeRenderPbrLightSpot;
				light.color = Vec3f( node->light->color[0], node->light->color[1], node->light->color[2] );
				light.intensity = node->light->intensity;
				light.position = flip_layout_y( transform_point( world_matrix, Vec3f( 0.0f, 0.0f, 0.0f ) ) );
				light.direction = normalize( flip_layout_y( transform_direction( world_matrix, Vec3f( 0.0f, 0.0f, -1.0f ) ) ) );
				light.range = node->light->range;
				light.inner_cone_cos = std::cos( node->light->spot_inner_cone_angle );
				light.outer_cone_cos = std::cos( node->light->spot_outer_cone_angle );
				model->lights.push_back( light );
			}
		}

		if ( !node->mesh )
			continue;

		for ( cgltf_size primitive_index = 0; primitive_index < node->mesh->primitives_count; ++primitive_index )
		{
			const cgltf_primitive &primitive = node->mesh->primitives[primitive_index];
			if ( primitive.type != cgltf_primitive_type_triangles
				&& primitive.type != cgltf_primitive_type_triangle_strip
				&& primitive.type != cgltf_primitive_type_triangle_fan )
			{
				if ( !logged_skipped_primitives )
				{
					logged_skipped_primitives = true;
					FeDebug() << "FeModel3D: skipping unsupported non-triangle primitive in " << resolved_path << std::endl;
				}
				continue;
			}

			const cgltf_accessor *position_accessor = find_attribute_accessor( primitive, cgltf_attribute_type_position, 0 );
			if ( !position_accessor || position_accessor->count == 0 )
				continue;

			const std::size_t vertex_count = static_cast<std::size_t>( position_accessor->count );
			std::vector<Vec3f> positions( vertex_count );
			for ( std::size_t i = 0; i < vertex_count; ++i )
			{
				float values[3] = {};
				cgltf_accessor_read_float( position_accessor, i, values, 3 );
				positions[i] = flip_layout_y( transform_point( world_matrix, Vec3f( values[0], values[1], values[2] ) ) );
			}

			std::vector<Vec2f> uv0( vertex_count, Vec2f( 0.0f, 0.0f ) );
			std::vector<Vec2f> uv1( vertex_count, Vec2f( 0.0f, 0.0f ) );
			if ( const cgltf_accessor *uv0_accessor = find_attribute_accessor( primitive, cgltf_attribute_type_texcoord, 0 ) )
			{
				for ( std::size_t i = 0; i < vertex_count; ++i )
				{
					float values[2] = {};
					cgltf_accessor_read_float( uv0_accessor, i, values, 2 );
					uv0[i] = Vec2f( values[0], values[1] );
				}
			}
			if ( const cgltf_accessor *uv1_accessor = find_attribute_accessor( primitive, cgltf_attribute_type_texcoord, 1 ) )
			{
				for ( std::size_t i = 0; i < vertex_count; ++i )
				{
					float values[2] = {};
					cgltf_accessor_read_float( uv1_accessor, i, values, 2 );
					uv1[i] = Vec2f( values[0], values[1] );
				}
			}

			std::vector<cgltf_uint> triangle_indices;
			build_triangle_index_list( primitive, position_accessor->count, triangle_indices );
			if ( triangle_indices.empty() )
				continue;
			reverse_triangle_winding( triangle_indices );

			std::vector<Vec3f> normals( vertex_count, Vec3f( 0.0f, 0.0f, 1.0f ) );
			const cgltf_accessor *normal_accessor = find_attribute_accessor( primitive, cgltf_attribute_type_normal, 0 );
			if ( normal_accessor )
			{
				for ( std::size_t i = 0; i < vertex_count; ++i )
				{
					float values[3] = {};
					cgltf_accessor_read_float( normal_accessor, i, values, 3 );
					normals[i] = normalize( flip_layout_y( transform_normal( world_matrix, Vec3f( values[0], values[1], values[2] ) ) ) );
				}
			}
			else
				compute_normals( positions, triangle_indices, normals );

			ModelMaterial material;
			if ( primitive.material )
			{
				material.name = primitive.material->name ? primitive.material->name : "";
				material.unlit = primitive.material->unlit;
				material.double_sided = primitive.material->double_sided;
				material.alpha_cutoff = primitive.material->alpha_cutoff;
				material.alpha_mode =
					( primitive.material->alpha_mode == cgltf_alpha_mode_mask ) ? FeRenderPbrAlphaMask :
					( primitive.material->alpha_mode == cgltf_alpha_mode_blend ) ? FeRenderPbrAlphaBlend :
					FeRenderPbrAlphaOpaque;

				if ( primitive.material->has_pbr_metallic_roughness )
				{
					const cgltf_pbr_metallic_roughness &pbr = primitive.material->pbr_metallic_roughness;
					material.base_color_factor[0] = pbr.base_color_factor[0];
					material.base_color_factor[1] = pbr.base_color_factor[1];
					material.base_color_factor[2] = pbr.base_color_factor[2];
					material.base_color_factor[3] = pbr.base_color_factor[3];
					material.metallic_factor = pbr.metallic_factor;
					material.roughness_factor = pbr.roughness_factor;
					material.base_color_texture = make_texture_ref( pbr.base_color_texture );
					material.metallic_roughness_texture = make_texture_ref( pbr.metallic_roughness_texture );
				}

				material.normal_texture = make_texture_ref( primitive.material->normal_texture );
				material.normal_scale = primitive.material->normal_texture.scale;
				material.occlusion_texture = make_texture_ref( primitive.material->occlusion_texture );
				material.occlusion_strength = primitive.material->occlusion_texture.scale;
				material.emissive_texture = make_texture_ref( primitive.material->emissive_texture );
				material.emissive_factor[0] = primitive.material->emissive_factor[0];
				material.emissive_factor[1] = primitive.material->emissive_factor[1];
				material.emissive_factor[2] = primitive.material->emissive_factor[2];
				if ( primitive.material->has_emissive_strength )
				{
					const float emissive_strength = primitive.material->emissive_strength.emissive_strength;
					material.emissive_factor[0] *= emissive_strength;
					material.emissive_factor[1] *= emissive_strength;
					material.emissive_factor[2] *= emissive_strength;
				}
				if ( material.alpha_mode == FeRenderPbrAlphaOpaque
					&& material.base_color_factor[3] >= ( 1.0f - FE_EPSILON )
					&& material.double_sided
					&& material_name_suggests_glass( material.name ) )
				{
					material.alpha_mode = FeRenderPbrAlphaBlend;
					material.base_color_factor[3] = 0.2f;
					if ( !logged_glass_fallback )
					{
						logged_glass_fallback = true;
						FeDebug() << "FeModel3D: applying glass alpha fallback for opaque glTF material " << material.name << std::endl;
					}
				}

				if ( primitive.material->has_pbr_specular_glossiness )
					log_material_extension_ignored( "KHR_materials_pbrSpecularGlossiness", logged_spec_gloss );
				if ( primitive.material->has_clearcoat )
					log_material_extension_ignored( "KHR_materials_clearcoat", logged_clearcoat );
				if ( primitive.material->has_transmission )
					log_material_extension_ignored( "KHR_materials_transmission", logged_transmission );
				if ( primitive.material->has_volume )
					log_material_extension_ignored( "KHR_materials_volume", logged_volume );
				if ( primitive.material->has_sheen )
					log_material_extension_ignored( "KHR_materials_sheen", logged_sheen );
				if ( primitive.material->has_specular )
					log_material_extension_ignored( "KHR_materials_specular", logged_specular );
				if ( primitive.material->has_ior )
					log_material_extension_ignored( "KHR_materials_ior", logged_ior );
				if ( primitive.material->has_iridescence )
					log_material_extension_ignored( "KHR_materials_iridescence", logged_iridescence );
				if ( primitive.material->has_diffuse_transmission )
					log_material_extension_ignored( "KHR_materials_diffuse_transmission", logged_diffuse_transmission );
				if ( primitive.material->has_anisotropy )
					log_material_extension_ignored( "KHR_materials_anisotropy", logged_anisotropy );
				if ( primitive.material->has_dispersion )
					log_material_extension_ignored( "KHR_materials_dispersion", logged_dispersion );
			}

			std::vector<ModelTangent> tangents( vertex_count );
			const cgltf_accessor *tangent_accessor = find_attribute_accessor( primitive, cgltf_attribute_type_tangent, 0 );
			if ( tangent_accessor )
			{
				for ( std::size_t i = 0; i < vertex_count; ++i )
				{
					float values[4] = {};
					cgltf_accessor_read_float( tangent_accessor, i, values, 4 );
					const Vec3f tangent_xyz = normalize( flip_layout_y( transform_direction( world_matrix, Vec3f( values[0], values[1], values[2] ) ) ) );
					tangents[i].x = tangent_xyz.x;
					tangents[i].y = tangent_xyz.y;
					tangents[i].z = tangent_xyz.z;
					tangents[i].w = -values[3];
				}
			}
			else if ( material.normal_texture.container )
			{
				const std::vector<Vec2f> &tangent_uvs =
					( material.normal_texture.texcoord_set == 1 ) ? uv1 : uv0;
				if ( !compute_tangents( positions, normals, tangent_uvs, triangle_indices, tangents ) )
				{
					if ( !logged_tangent_failure )
					{
						logged_tangent_failure = true;
						FeDebug() << "FeModel3D: normal map tangent generation failed, rendering without that normal map in " << resolved_path << std::endl;
					}
					material.normal_texture = ModelTextureRef();
				}
			}

			ModelPrimitive model_primitive;
			model_primitive.material = material;
			model_primitive.vertices.reserve( triangle_indices.size() );
			for ( cgltf_uint source_index : triangle_indices )
			{
				if ( source_index >= positions.size() )
					continue;

				FeRenderVertex render_vertex = {};
				render_vertex.x = positions[source_index].x;
				render_vertex.y = positions[source_index].y;
				render_vertex.z = positions[source_index].z;
				render_vertex.u = uv0[source_index].x;
				render_vertex.v = uv0[source_index].y;
				render_vertex.u1 = uv1[source_index].x;
				render_vertex.v1 = uv1[source_index].y;
				render_vertex.nx = normals[source_index].x;
				render_vertex.ny = normals[source_index].y;
				render_vertex.nz = normals[source_index].z;
				render_vertex.tx = tangents[source_index].x;
				render_vertex.ty = tangents[source_index].y;
				render_vertex.tz = tangents[source_index].z;
				render_vertex.tw = tangents[source_index].w;
				render_vertex.r = 255;
				render_vertex.g = 255;
				render_vertex.b = 255;
				render_vertex.a = 255;
				model_primitive.vertices.push_back( render_vertex );

				model->bounds_min.x = std::min( model->bounds_min.x, render_vertex.x );
				model->bounds_min.y = std::min( model->bounds_min.y, render_vertex.y );
				model->bounds_min.z = std::min( model->bounds_min.z, render_vertex.z );
				model->bounds_max.x = std::max( model->bounds_max.x, render_vertex.x );
				model->bounds_max.y = std::max( model->bounds_max.y, render_vertex.y );
				model->bounds_max.z = std::max( model->bounds_max.z, render_vertex.z );
			}

			if ( !model_primitive.vertices.empty() )
			{
				model_primitive.bounds_aspect = estimate_primitive_bounds_aspect( model_primitive );
				model_primitive.target_aspect[0] = estimate_primitive_target_aspect( model_primitive, 0 );
				model_primitive.target_aspect[1] = estimate_primitive_target_aspect( model_primitive, 1 );
				const std::uint64_t material_hash =
					compute_model_material_hash( model_primitive.material );
				bool merged = false;
				auto merge_it = primitive_merge_lookup.find( material_hash );
				if ( merge_it != primitive_merge_lookup.end() )
				{
					for ( std::size_t primitive_slot : merge_it->second )
					{
						if ( primitive_slot >= model->primitives.size() )
							continue;

						ModelPrimitive &existing_primitive = model->primitives[ primitive_slot ];
						if ( !model_material_matches( existing_primitive.material, model_primitive.material ) )
							continue;

						existing_primitive.vertices.insert(
							existing_primitive.vertices.end(),
							model_primitive.vertices.begin(),
							model_primitive.vertices.end() );
						merged = true;
						break;
					}
				}

				if ( !merged )
				{
					model->primitives.push_back( std::move( model_primitive ) );
					primitive_merge_lookup[ material_hash ].push_back( model->primitives.size() - 1 );
				}
			}
		}
	}

	free_data();

	if ( model->primitives.empty() )
	{
		FeLog() << "FeModel3D: no renderable primitives found in " << resolved_path << std::endl;
		return;
	}

	if ( !model->has_bounds() )
	{
		model->bounds_min = Vec3f( 0.0f, 0.0f, 0.0f );
		model->bounds_max = Vec3f( 1.0f, 1.0f, 1.0f );
	}

	const Vec3f bounds_size = model->bounds_max - model->bounds_min;
	m_size = Vec2f(
		( ( std::fabs( bounds_size.x ) > FE_EPSILON ) ? bounds_size.x : 1.0f ) * FE_DEFAULT_MODEL_3D_SCALE,
		( ( std::fabs( bounds_size.y ) > FE_EPSILON ) ? bounds_size.y : 1.0f ) * FE_DEFAULT_MODEL_3D_SCALE );
	m_depth = ( ( std::fabs( bounds_size.z ) > FE_EPSILON ) ? bounds_size.z : 1.0f ) * FE_DEFAULT_MODEL_3D_SCALE;
	s_model_cache[ resolved_path ] = model;
	m_model = std::move( model );
}

bool FeModel3D::geometry_cache_matches( float camera_light ) const
{
	if ( !m_geometry_cache_valid
		|| ( m_geometry_cache_model != m_model.get() )
		|| ( m_geometry_cache_pos != m_pos )
		|| ( m_geometry_cache_size != m_size )
		|| ( m_geometry_cache_anchor != m_anchor )
		|| ( m_geometry_cache_depth != m_depth )
		|| ( m_geometry_cache_rotation != m_rotation )
		|| ( m_geometry_cache_z != get_z() )
		|| ( m_geometry_cache_rotation_x != get_rotation_x() )
		|| ( m_geometry_cache_rotation_y != get_rotation_y() )
		|| ( m_geometry_cache_rotation_order != get_rotation_order() )
		|| ( m_geometry_cache_color != m_color )
		|| ( m_geometry_cache_zbuffer != get_zbuffer() )
		|| ( m_geometry_cache_camera_light != camera_light )
		|| ( m_geometry_cache_primitives.size() != m_geometry_cache.size() ) )
	{
		return false;
	}

	for ( std::size_t cache_index = 0; cache_index < m_geometry_cache.size(); ++cache_index )
	{
		const std::size_t primitive_index = m_geometry_cache_primitives[cache_index];
		if ( primitive_index >= m_model->primitives.size() )
			return false;

		const ModelPrimitive &primitive = m_model->primitives[ primitive_index ];
		const MaterialOverride *override_entry = find_override( primitive.material.name );
		if ( !override_entry || !override_entry->container )
			continue;

		const FeBaseTextureContainer *container = override_entry->container;
		const Vec2u texture_size = container->get_texture_size();
		const FeRenderTextureBinding &base_binding =
			m_geometry_cache[cache_index].pbr_material.base_color_texture;
		if ( base_binding.texture_id != container->get_texture_source_id()
			|| base_binding.texture_source_type != container->get_texture_source_type()
			|| base_binding.repeated != container->get_repeat()
			|| base_binding.smooth != container->get_smooth()
			|| base_binding.mipmap != container->get_mipmap()
			|| base_binding.dynamic != container->is_volatile_texture()
			|| base_binding.width != static_cast<float>( texture_size.x )
			|| base_binding.height != static_cast<float>( texture_size.y )
			|| base_binding.content_version != container->get_texture_content_version() )
		{
			return false;
		}

		const bool use_emissive_override =
			primitive.material.emissive_texture.container
			|| has_nonzero_emissive_factor( primitive.material );
		if ( use_emissive_override )
		{
			const FeRenderTextureBinding &emissive_binding =
				m_geometry_cache[cache_index].pbr_material.emissive_texture;
			if ( emissive_binding.texture_id != container->get_texture_source_id()
				|| emissive_binding.texture_source_type != container->get_texture_source_type()
				|| emissive_binding.repeated != container->get_repeat()
				|| emissive_binding.smooth != container->get_smooth()
				|| emissive_binding.mipmap != container->get_mipmap()
				|| emissive_binding.dynamic != container->is_volatile_texture()
				|| emissive_binding.width != static_cast<float>( texture_size.x )
				|| emissive_binding.height != static_cast<float>( texture_size.y )
				|| emissive_binding.content_version != container->get_texture_content_version() )
			{
				return false;
			}
		}
	}

	return true;
}

void FeModel3D::update_geometry_cache_state( float camera_light ) const
{
	m_geometry_cache_valid = true;
	m_geometry_cache_model = m_model.get();
	m_geometry_cache_pos = m_pos;
	m_geometry_cache_size = m_size;
	m_geometry_cache_anchor = m_anchor;
	m_geometry_cache_depth = m_depth;
	m_geometry_cache_rotation = m_rotation;
	m_geometry_cache_z = get_z();
	m_geometry_cache_rotation_x = get_rotation_x();
	m_geometry_cache_rotation_y = get_rotation_y();
	m_geometry_cache_rotation_order = get_rotation_order();
	m_geometry_cache_color = m_color;
	m_geometry_cache_zbuffer = get_zbuffer();
	m_geometry_cache_camera_light = camera_light;
}

void FeModel3D::update_cached_material_state( FeRenderGeometry &entry, std::size_t primitive_index ) const
{
	if ( !m_model || primitive_index >= m_model->primitives.size() )
		return;

	const ModelPrimitive &primitive = m_model->primitives[ primitive_index ];
	const MaterialOverride *override_entry = find_override( primitive.material.name );
	const FeBaseTextureContainer *override_container =
		override_entry ? override_entry->container : nullptr;
	const bool use_emissive_override =
		override_container &&
		( primitive.material.emissive_texture.container
			|| has_nonzero_emissive_factor( primitive.material ) );

	entry.pbr_material.emissive_factor[0] = primitive.material.emissive_factor[0];
	entry.pbr_material.emissive_factor[1] = primitive.material.emissive_factor[1];
	entry.pbr_material.emissive_factor[2] = primitive.material.emissive_factor[2];
	entry.pbr_material.artwork_shader =
		( override_container && override_entry ) ? override_entry->shader : nullptr;
	entry.pbr_material.artwork_shader_emissive =
		( entry.pbr_material.artwork_shader != nullptr ) && use_emissive_override;
	if ( use_emissive_override && !primitive.material.emissive_texture.container )
		normalize_emissive_override_factor( entry.pbr_material.emissive_factor );

	fill_render_texture_binding(
		entry.pbr_material.base_color_texture,
		primitive.material.base_color_texture,
		override_container );
	fill_render_texture_binding(
		entry.pbr_material.metallic_roughness_texture,
		primitive.material.metallic_roughness_texture );
	fill_render_texture_binding(
		entry.pbr_material.normal_texture,
		primitive.material.normal_texture );
	fill_render_texture_binding(
		entry.pbr_material.occlusion_texture,
		primitive.material.occlusion_texture );
	fill_render_texture_binding(
		entry.pbr_material.emissive_texture,
		primitive.material.emissive_texture,
		use_emissive_override ? override_container : nullptr );

	if ( override_container && override_entry && override_entry->preserve_aspect_ratio )
	{
		const float sample_aspect_ratio = override_container->get_sample_aspect_ratio();
		apply_preserve_aspect_ratio_fit(
			entry.pbr_material.base_color_texture,
			primitive,
			sample_aspect_ratio );
		if ( use_emissive_override )
		{
			apply_preserve_aspect_ratio_fit(
				entry.pbr_material.emissive_texture,
				primitive,
				sample_aspect_ratio );
		}
	}

	entry.texture_id = entry.pbr_material.base_color_texture.texture_id;
	entry.texture_source_type = entry.pbr_material.base_color_texture.texture_source_type;
	entry.texture_repeated = entry.pbr_material.base_color_texture.repeated;
	entry.texture_smooth = entry.pbr_material.base_color_texture.smooth;
	entry.texture_mipmap = entry.pbr_material.base_color_texture.mipmap;
	entry.texture_width = entry.pbr_material.base_color_texture.width;
	entry.texture_height = entry.pbr_material.base_color_texture.height;
	entry.texture_content_version = entry.pbr_material.base_color_texture.content_version;
	entry.texture_dynamic =
		binding_is_dynamic( entry.pbr_material.base_color_texture )
		|| binding_is_dynamic( entry.pbr_material.metallic_roughness_texture )
		|| binding_is_dynamic( entry.pbr_material.normal_texture )
		|| binding_is_dynamic( entry.pbr_material.occlusion_texture )
		|| binding_is_dynamic( entry.pbr_material.emissive_texture );
}

void FeModel3D::refresh_geometry_cache() const
{
	if ( !m_model || m_geometry_cache.empty() )
		return;

	const Vec3f bounds_size = m_model->bounds_max - m_model->bounds_min;
	const Vec3f source_center = ( m_model->bounds_min + m_model->bounds_max ) * 0.5f;
	const Vec3f target_min(
		m_pos.x - ( m_anchor.x * m_size.x ),
		m_pos.y - ( m_anchor.y * m_size.y ),
		get_z() - ( m_anchor.z * m_depth ) );
	const Vec3f target_center(
		target_min.x + ( m_size.x * 0.5f ),
		target_min.y + ( m_size.y * 0.5f ),
		target_min.z + ( m_depth * 0.5f ) );
	const Vec3f scale(
		m_size.x / safe_extent( bounds_size.x ),
		m_size.y / safe_extent( bounds_size.y ),
		m_depth / safe_extent( bounds_size.z ) );
	float model_matrix[16] = {};
	build_model_model_matrix(
		model_matrix,
		source_center,
		target_center,
		scale,
		get_rotation_x(),
		get_rotation_y(),
		m_rotation,
		get_rotation_order() );
	float normal_matrix[9] = {};
	if ( !build_normal_matrix3( model_matrix, normal_matrix ) )
	{
		normal_matrix[0] = 1.0f;
		normal_matrix[1] = 0.0f;
		normal_matrix[2] = 0.0f;
		normal_matrix[3] = 0.0f;
		normal_matrix[4] = 1.0f;
		normal_matrix[5] = 0.0f;
		normal_matrix[6] = 0.0f;
		normal_matrix[7] = 0.0f;
		normal_matrix[8] = 1.0f;
	}

	const float color_scale_r = static_cast<float>( m_color.r ) / 255.0f;
	const float color_scale_g = static_cast<float>( m_color.g ) / 255.0f;
	const float color_scale_b = static_cast<float>( m_color.b ) / 255.0f;
	const float color_scale_a = static_cast<float>( m_color.a ) / 255.0f;
	float camera_light = 0.0f;
	if ( FePresent *fep = FePresent::script_get_fep() )
		camera_light = fep->get_camera_light();

	FeRenderPbrLight transformed_lights[4];
	int light_count = 0;
	if ( m_model->lights.empty() )
	{
		light_count = 1;
		transformed_lights[0].clear();
		transformed_lights[0].type = FeRenderPbrLightDirectional;
		transformed_lights[0].color[0] = 1.0f;
		transformed_lights[0].color[1] = 1.0f;
		transformed_lights[0].color[2] = 1.0f;
		transformed_lights[0].intensity = 3.0f;
		transformed_lights[0].position[0] = target_center.x;
		transformed_lights[0].position[1] = target_center.y;
		transformed_lights[0].position[2] = target_center.z + std::max( m_depth, 1.0f );
		const Vec3f direction = normalize( Vec3f( -0.35f, -0.55f, -1.0f ) );
		transformed_lights[0].direction[0] = direction.x;
		transformed_lights[0].direction[1] = direction.y;
		transformed_lights[0].direction[2] = direction.z;
	}
	else
	{
		light_count = static_cast<int>( m_model->lights.size() );
		for ( int light_index = 0; light_index < light_count; ++light_index )
		{
			const ModelLight &source_light = m_model->lights[ light_index ];
			FeRenderPbrLight &target_light = transformed_lights[ light_index ];
			target_light.clear();
			target_light.type = source_light.type;
			target_light.color[0] = source_light.color.x;
			target_light.color[1] = source_light.color.y;
			target_light.color[2] = source_light.color.z;
			target_light.intensity = source_light.intensity;
			const Vec3f transformed_light_position = transform_instance_position(
				source_light.position,
				source_center,
				target_center,
				scale,
				get_rotation_x(),
				get_rotation_y(),
				m_rotation,
				get_rotation_order() );
			const Vec3f transformed_light_direction = transform_instance_normal(
				source_light.direction,
				scale,
				get_rotation_x(),
				get_rotation_y(),
				m_rotation,
				get_rotation_order() );
			target_light.position[0] = transformed_light_position.x;
			target_light.position[1] = transformed_light_position.y;
			target_light.position[2] = transformed_light_position.z;
			target_light.direction[0] = transformed_light_direction.x;
			target_light.direction[1] = transformed_light_direction.y;
			target_light.direction[2] = transformed_light_direction.z;
			target_light.range = source_light.range;
			target_light.inner_cone_cos = source_light.inner_cone_cos;
			target_light.outer_cone_cos = source_light.outer_cone_cos;
		}
	}

	if ( m_overrides.empty() )
	{
		for ( std::size_t cache_index = 0; cache_index < m_geometry_cache.size(); ++cache_index )
		{
			const std::size_t primitive_index =
				( cache_index < m_geometry_cache_primitives.size() )
					? m_geometry_cache_primitives[ cache_index ]
					: cache_index;
			if ( primitive_index >= m_model->primitives.size() )
				continue;

			const ModelPrimitive &primitive = m_model->primitives[ primitive_index ];
			FeRenderGeometry &entry = m_geometry_cache[ cache_index ];
			entry.zbuffer = get_zbuffer();
			entry.pbr_material.base_color_factor[0] = primitive.material.base_color_factor[0] * color_scale_r;
			entry.pbr_material.base_color_factor[1] = primitive.material.base_color_factor[1] * color_scale_g;
			entry.pbr_material.base_color_factor[2] = primitive.material.base_color_factor[2] * color_scale_b;
			entry.pbr_material.base_color_factor[3] = primitive.material.base_color_factor[3] * color_scale_a;
			entry.ambient_color[0] = m_model->lights.empty() ? 0.08f : 0.03f;
			entry.ambient_color[1] = m_model->lights.empty() ? 0.08f : 0.03f;
			entry.ambient_color[2] = m_model->lights.empty() ? 0.08f : 0.03f;
			entry.camera_light = camera_light;
			entry.light_count = light_count;
			std::memcpy( entry.model_matrix, model_matrix, sizeof( model_matrix ) );
			std::memcpy( entry.normal_matrix, normal_matrix, sizeof( normal_matrix ) );
			for ( int light_index = 0; light_index < light_count; ++light_index )
				entry.lights[ light_index ] = transformed_lights[ light_index ];
		}
		update_geometry_cache_state( camera_light );
		return;
	}

	for ( std::size_t cache_index = 0; cache_index < m_geometry_cache.size(); ++cache_index )
	{
		const std::size_t primitive_index =
			( cache_index < m_geometry_cache_primitives.size() )
				? m_geometry_cache_primitives[ cache_index ]
				: cache_index;
		if ( primitive_index >= m_model->primitives.size() )
			continue;

		const MaterialOverride *override_entry =
			find_override( m_model->primitives[ primitive_index ].material.name );
		FeRenderGeometry &entry = m_geometry_cache[ cache_index ];
		const ModelPrimitive &primitive = m_model->primitives[ primitive_index ];
		entry.zbuffer = get_zbuffer();
		entry.pbr_material.base_color_factor[0] = primitive.material.base_color_factor[0] * color_scale_r;
		entry.pbr_material.base_color_factor[1] = primitive.material.base_color_factor[1] * color_scale_g;
		entry.pbr_material.base_color_factor[2] = primitive.material.base_color_factor[2] * color_scale_b;
		entry.pbr_material.base_color_factor[3] = primitive.material.base_color_factor[3] * color_scale_a;
		entry.ambient_color[0] = m_model->lights.empty() ? 0.08f : 0.03f;
		entry.ambient_color[1] = m_model->lights.empty() ? 0.08f : 0.03f;
		entry.ambient_color[2] = m_model->lights.empty() ? 0.08f : 0.03f;
		entry.camera_light = camera_light;
		entry.light_count = light_count;
		std::memcpy( entry.model_matrix, model_matrix, sizeof( model_matrix ) );
		std::memcpy( entry.normal_matrix, normal_matrix, sizeof( normal_matrix ) );
		for ( int light_index = 0; light_index < light_count; ++light_index )
			entry.lights[ light_index ] = transformed_lights[ light_index ];
		if ( override_entry )
			update_cached_material_state( entry, primitive_index );
	}

	update_geometry_cache_state( camera_light );
}

void FeModel3D::rebuild_geometry_cache( float camera_light ) const
{
	m_geometry_cache.clear();
	m_geometry_cache_primitives.clear();
	if ( !m_model || m_model->primitives.empty() )
	{
		m_geometry_cache_valid = false;
		return;
	}

	const Vec3f bounds_size = m_model->bounds_max - m_model->bounds_min;
	const Vec3f source_center = ( m_model->bounds_min + m_model->bounds_max ) * 0.5f;
	const Vec3f target_min(
		m_pos.x - ( m_anchor.x * m_size.x ),
		m_pos.y - ( m_anchor.y * m_size.y ),
		get_z() - ( m_anchor.z * m_depth ) );
	const Vec3f target_center(
		target_min.x + ( m_size.x * 0.5f ),
		target_min.y + ( m_size.y * 0.5f ),
		target_min.z + ( m_depth * 0.5f ) );
	const Vec3f scale(
		m_size.x / safe_extent( bounds_size.x ),
		m_size.y / safe_extent( bounds_size.y ),
		m_depth / safe_extent( bounds_size.z ) );
	float model_matrix[16] = {};
	build_model_model_matrix(
		model_matrix,
		source_center,
		target_center,
		scale,
		get_rotation_x(),
		get_rotation_y(),
		m_rotation,
		get_rotation_order() );
	float normal_matrix[9] = {};
	if ( !build_normal_matrix3( model_matrix, normal_matrix ) )
	{
		normal_matrix[0] = 1.0f;
		normal_matrix[1] = 0.0f;
		normal_matrix[2] = 0.0f;
		normal_matrix[3] = 0.0f;
		normal_matrix[4] = 1.0f;
		normal_matrix[5] = 0.0f;
		normal_matrix[6] = 0.0f;
		normal_matrix[7] = 0.0f;
		normal_matrix[8] = 1.0f;
	}
	const float color_scale_r = static_cast<float>( m_color.r ) / 255.0f;
	const float color_scale_g = static_cast<float>( m_color.g ) / 255.0f;
	const float color_scale_b = static_cast<float>( m_color.b ) / 255.0f;
	const float color_scale_a = static_cast<float>( m_color.a ) / 255.0f;

	m_geometry_cache.reserve( m_model->primitives.size() );
	m_geometry_cache_primitives.reserve( m_model->primitives.size() );
	for ( std::size_t primitive_index = 0; primitive_index < m_model->primitives.size(); ++primitive_index )
	{
		const ModelPrimitive &primitive = m_model->primitives[ primitive_index ];
		FeRenderGeometry entry;
		entry.clear();
		entry.geometry_kind = FeRenderGeometryObjectPbr;
		entry.zbuffer = get_zbuffer();
		entry.shader = nullptr;
		entry.custom_shader = false;
		entry.textured = true;
		entry.blend_mode =
			( primitive.material.alpha_mode == FeRenderPbrAlphaBlend )
				? FeBlend::Alpha
				: FeBlend::None;
		entry.pbr_material.base_color_factor[0] = primitive.material.base_color_factor[0] * color_scale_r;
		entry.pbr_material.base_color_factor[1] = primitive.material.base_color_factor[1] * color_scale_g;
		entry.pbr_material.base_color_factor[2] = primitive.material.base_color_factor[2] * color_scale_b;
		entry.pbr_material.base_color_factor[3] = primitive.material.base_color_factor[3] * color_scale_a;
		entry.pbr_material.metallic_factor = primitive.material.metallic_factor;
		entry.pbr_material.roughness_factor = primitive.material.roughness_factor;
		entry.pbr_material.normal_scale = primitive.material.normal_scale;
		entry.pbr_material.occlusion_strength = primitive.material.occlusion_strength;
		entry.pbr_material.alpha_cutoff = primitive.material.alpha_cutoff;
		entry.pbr_material.alpha_mode = primitive.material.alpha_mode;
		entry.pbr_material.unlit = primitive.material.unlit;
		entry.pbr_material.double_sided = primitive.material.double_sided;
		std::memcpy( entry.model_matrix, model_matrix, sizeof( model_matrix ) );
		std::memcpy( entry.normal_matrix, normal_matrix, sizeof( normal_matrix ) );
		entry.external_vertices = primitive.vertices.empty() ? nullptr : primitive.vertices.data();
		entry.external_vertex_count = primitive.vertices.size();
		entry.external_vertex_id = entry.external_vertices;
		entry.ambient_color[0] = m_model->lights.empty() ? 0.08f : 0.03f;
		entry.ambient_color[1] = m_model->lights.empty() ? 0.08f : 0.03f;
		entry.ambient_color[2] = m_model->lights.empty() ? 0.08f : 0.03f;
		entry.camera_light = camera_light;

		if ( m_model->lights.empty() )
		{
			entry.light_count = 1;
			entry.lights[0].clear();
			entry.lights[0].type = FeRenderPbrLightDirectional;
			entry.lights[0].color[0] = 1.0f;
			entry.lights[0].color[1] = 1.0f;
			entry.lights[0].color[2] = 1.0f;
			entry.lights[0].intensity = 3.0f;
			entry.lights[0].position[0] = target_center.x;
			entry.lights[0].position[1] = target_center.y;
			entry.lights[0].position[2] = target_center.z + std::max( m_depth, 1.0f );
			const Vec3f direction = normalize( Vec3f( -0.35f, -0.55f, -1.0f ) );
			entry.lights[0].direction[0] = direction.x;
			entry.lights[0].direction[1] = direction.y;
			entry.lights[0].direction[2] = direction.z;
		}
		else
		{
			entry.light_count = static_cast<int>( m_model->lights.size() );
			for ( int light_index = 0; light_index < entry.light_count; ++light_index )
			{
				const ModelLight &source_light = m_model->lights[ light_index ];
				FeRenderPbrLight &target_light = entry.lights[ light_index ];
				target_light.type = source_light.type;
				target_light.color[0] = source_light.color.x;
				target_light.color[1] = source_light.color.y;
				target_light.color[2] = source_light.color.z;
				target_light.intensity = source_light.intensity;
				const Vec3f transformed_light_position = transform_instance_position(
					source_light.position,
					source_center,
					target_center,
					scale,
					get_rotation_x(),
					get_rotation_y(),
					m_rotation,
					get_rotation_order() );
				const Vec3f transformed_light_direction = transform_instance_normal(
					source_light.direction,
					scale,
					get_rotation_x(),
					get_rotation_y(),
					m_rotation,
					get_rotation_order() );
				target_light.position[0] = transformed_light_position.x;
				target_light.position[1] = transformed_light_position.y;
				target_light.position[2] = transformed_light_position.z;
				target_light.direction[0] = transformed_light_direction.x;
				target_light.direction[1] = transformed_light_direction.y;
				target_light.direction[2] = transformed_light_direction.z;
				target_light.range = source_light.range;
				target_light.inner_cone_cos = source_light.inner_cone_cos;
				target_light.outer_cone_cos = source_light.outer_cone_cos;
			}
		}

		update_cached_material_state( entry, primitive_index );
		if ( entry.get_vertex_count() > 0 )
		{
			m_geometry_cache.push_back( entry );
			m_geometry_cache_primitives.push_back( primitive_index );
		}
	}

	update_geometry_cache_state( camera_light );
}

bool FeModel3D::build_render_geometry( std::vector<FeRenderGeometry> &geometry ) const
{
	if ( !m_model || m_model->primitives.empty() )
		return false;

	float camera_light = 0.0f;
	if ( FePresent *fep = FePresent::script_get_fep() )
		camera_light = fep->get_camera_light();

	if ( !m_geometry_cache_valid || ( m_geometry_cache_model != m_model.get() ) )
		rebuild_geometry_cache( camera_light );
	else if ( !geometry_cache_matches( camera_light ) )
		refresh_geometry_cache();

	if ( m_geometry_cache.empty() )
		return false;

	geometry.insert( geometry.end(), m_geometry_cache.begin(), m_geometry_cache.end() );
	return true;
}
