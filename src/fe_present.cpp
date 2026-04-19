/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-15 Andrew Mickelson
 *
 *  This file is part of Attract-Mode.
 *
 *  Attract-Mode is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Attract-Mode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Attract-Mode.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "fe_present.hpp"
#include "fe_util.hpp"
#include "fe_image.hpp"
#include "fe_model_3d.hpp"
#include "fe_text.hpp"
#include "fe_listbox.hpp"
#include "fe_rectangle.hpp"
#include "fe_input.hpp"
#include "fe_file.hpp"
#include "fe_blend.hpp"
#include "fe_shader.hpp"
#include "zip.hpp"
#include "base64.hpp"
#include "image_loader.hpp"

#include "BarlowCJK.ttf.h"
#include "Logo.png.h"
#include "Logo_Full_White.png.h"

#include <iostream>
#include <cmath>
#include <limits>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <utility>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#ifdef SDL_PLATFORM_MACOS
#include <CoreVideo/CoreVideo.h>
#endif

#ifdef SDL_PLATFORM_WINDOWS

#include <windows.h>

BOOL CALLBACK my_mon_enum_proc( HMONITOR, HDC, LPRECT mon_rect, LPARAM data )
{
	std::vector < FeMonitor > *monitors = (std::vector < FeMonitor > *)data;

	FeMonitor mon(
		monitors->size(),
		mon_rect->right - mon_rect->left,
		mon_rect->bottom - mon_rect->top );

	mon.transform = FeTransform().translate( { static_cast<float>( mon_rect->left ), static_cast<float>( mon_rect->top ) } );

	FeDebug() << "Multimon: monitor #" << monitors->size()
		<< ": " << mon.size.x << "x" << mon.size.y << " @ "
		<< mon_rect->left << "," << mon_rect->top << std::endl;

	// make sure primary monitor is first in m_mon vector
	//
	if (( mon_rect->left == 0 ) && ( mon_rect->top == 0 ))
		monitors->insert( monitors->begin(), mon );
	else
		monitors->push_back( mon );

	return TRUE;
}
#endif

namespace
{
	SDL_Surface *load_embedded_png_surface( const char *data_uri )
	{
		const std::vector<unsigned char> png_data = base64_decode( data_uri );
		if ( png_data.empty() )
			return nullptr;

		SDL_IOStream *stream = SDL_IOFromConstMem( png_data.data(), png_data.size() );
		if ( !stream )
		{
			FeLog() << "Failed to create SDL IO stream for embedded PNG: " << SDL_GetError() << std::endl;
			return nullptr;
		}

		SDL_Surface *surface = IMG_Load_IO( stream, true );
		if ( !surface )
		{
			FeLog() << "Failed to decode embedded PNG with SDL3_image: " << SDL_GetError() << std::endl;
			return nullptr;
		}

		SDL_Surface *rgba_surface = SDL_ConvertSurface( surface, SDL_PIXELFORMAT_RGBA32 );
		SDL_DestroySurface( surface );
		if ( !rgba_surface )
		{
			FeLog() << "Failed to convert embedded PNG surface to RGBA32: " << SDL_GetError() << std::endl;
			return nullptr;
		}

		return rgba_surface;
	}
}

FeFontContainer::FeFontContainer()
	: m_needs_reload( false )
{
}

FeFontContainer::~FeFontContainer()
{
}

void FeFontContainer::set_font( const std::string &n )
{
	m_name = n;

	if ( !m_font.openFromFile( n ) )
		FeLog() << "Error loading font from file: " << n << std::endl;
}

void FeFontContainer::load_default_font()
{
	m_font_binary_data = base64_decode( _binary_resources_fonts_BarlowCJK_ttf );
	std::ignore = m_font.openFromMemory( m_font_binary_data.data(), m_font_binary_data.size() );
}

const FeFont &FeFontContainer::get_font() const
{
	return m_font;
}

void FeFontContainer::clear_font()
{
	m_font.clear();
	m_needs_reload = true;
}

FeMonitor::FeMonitor( int n, int w, int h )
	: num( n )
{
	size.x = w;
	size.y = h;
}

FeMonitor::FeMonitor( const FeMonitor &o )
	: FePresentableParent( o ),
	transform( o.transform ),
	size( o.size ),
	num( o.num )
{
}

FeMonitor &FeMonitor::operator=( const FeMonitor &o )
{
	elements = o.elements;
	transform = o.transform;
	size = o.size;
	num = o.num;

	return *this;
}

int FeMonitor::get_width()
{
	return size.x;
}

int FeMonitor::get_height()
{
	return size.y;
}

int FeMonitor::get_num()
{
	return num;
}

FePresent::FePresent( FeSettings *fesettings, FeWindow &wnd )
	: m_feSettings( fesettings ),
	m_window( wnd ),
	m_layoutFont( NULL ),
	m_defaultFont( NULL ),
	m_logo_image( nullptr ),
	m_logo_full_image( nullptr ),
	m_layout_time_old(),
	m_frame_time( 0.0f ),
	m_baseRotation( FeSettings::RotateNone ),
	m_toggleRotation( FeSettings::RotateNone ),
	m_camera_light( 0.0f ),
	m_refresh_rate( 0 ),
	m_playMovies( true ),
	m_user_page_size( -1 ),
	m_preserve_aspect( false ),
	m_custom_overlay( false ),
	m_mouse_pointer_visible( false ),
	m_listBox( NULL ),
	m_emptyShader( NULL ),
	m_overlay_caption( NULL ),
	m_overlay_lb( NULL ),
	m_layout_loaded( false ),
	m_layout_has_content( false ),
	m_render_frame_serial( 0 )
{
	m_baseRotation = m_feSettings->get_screen_rotation();
	m_layoutFontName = "";
	init_monitors();
}

void FePresent::init_monitors()
{
	m_mon.clear();

	//
	// Handle multi-monitors
	//
	// We support multi-monitor setups on MS-Windows when in fullscreen or "fillscreen" mode
	// We also determine display's refresh rate here
	//

#if defined(SDL_PLATFORM_MACOS)
	CGDirectDisplayID display_id = CGMainDisplayID();
	CGDisplayModeRef display_mode = CGDisplayCopyDisplayMode( display_id );
	size_t refresh_rate = CGDisplayModeGetRefreshRate( display_mode );

	if ( refresh_rate == 0 )
	{
		CVDisplayLinkRef display_link;
		CVDisplayLinkCreateWithCGDisplay( display_id, &display_link );
		const CVTime time = CVDisplayLinkGetNominalOutputVideoRefreshPeriod( display_link );
		if ( !( time.flags & kCVTimeIsIndefinite ))
			m_refresh_rate = (int)( (float)time.timeScale / (float)time.timeValue + 0.5f );
	}
	else
		m_refresh_rate = refresh_rate;
#endif

#if defined(SDL_PLATFORM_LINUX)
	SDL_DisplayID current_display = 0;
	if ( m_window.owns_sdl_window() )
	{
		if ( SDL_Window *window = m_window.get_gpu_context().get_window() )
			current_display = SDL_GetDisplayForWindow( window );
	}

	if ( !current_display )
		current_display = SDL_GetPrimaryDisplay();

	if ( current_display )
	{
		if ( const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode( current_display ) )
			m_refresh_rate = static_cast<int>( mode->refresh_rate + 0.5f );
		else if ( const SDL_DisplayMode *desktop_mode = SDL_GetDesktopDisplayMode( current_display ) )
			m_refresh_rate = static_cast<int>( desktop_mode->refresh_rate + 0.5f );
	}
#endif

#if defined(SDL_PLATFORM_WINDOWS)
	DEVMODE devMode;
	memset( &devMode, 0, sizeof(DEVMODE) );
	devMode.dmSize = sizeof(DEVMODE);
	devMode.dmDriverExtra = 0;

	if ( EnumDisplaySettings( NULL, ENUM_CURRENT_SETTINGS, &devMode ) != 0 )
		m_refresh_rate = devMode.dmDisplayFrequency;

	if ( m_feSettings->get_multimon() && !is_windowed_mode( m_window.get_window_mode() ) )
	{
		EnumDisplayMonitors( NULL, NULL, my_mon_enum_proc, (LPARAM)&m_mon );

		//
		// The Windows virtual screen can have a negative valued top left corner. Normalize the
		// monitor transforms into our local 0,0-based desktop coordinate space now.
		//
		int translate_x = -GetSystemMetrics( SM_XVIRTUALSCREEN );
		int translate_y = -GetSystemMetrics( SM_YVIRTUALSCREEN );

		//
		// On Windows 'Fill screen' mode our window is offscreen 1 pixel in each direction, so correct
		// for that here to align draw area with screen
		//
		if ( m_window.get_window_mode() == FeSettings::Fillscreen )
		{
			translate_x += 1;
			translate_y += 1;
		}

		FeTransform correction = FeTransform().translate( { static_cast<float>( translate_x ), static_cast<float>( translate_y ) } );

		for ( std::vector<FeMonitor>::iterator itr=m_mon.begin(); itr!=m_mon.end(); ++itr )
			(*itr).transform *= correction;
	}
	else
#elif defined(SDL_PLATFORM_LINUX)
	if ( m_feSettings->get_multimon() && !is_windowed_mode( m_window.get_window_mode() ) )
	{
		int display_count = 0;
		SDL_DisplayID *displays = SDL_GetDisplays( &display_count );
		if ( displays && display_count > 0 )
		{
			std::vector<SDL_DisplayID> ordered_displays( displays, displays + display_count );
			SDL_free( displays );

			if ( current_display )
			{
				const auto preferred = std::find( ordered_displays.begin(), ordered_displays.end(), current_display );
				if ( preferred != ordered_displays.end() )
					std::rotate( ordered_displays.begin(), preferred, ordered_displays.end() );
			}

			bool have_bounds = false;
			int left = 0;
			int top = 0;
			std::vector<SDL_Rect> rects;
			rects.reserve( ordered_displays.size() );

			for ( SDL_DisplayID display : ordered_displays )
			{
				SDL_Rect rect = {};
				if ( !SDL_GetDisplayBounds( display, &rect ) )
					continue;

				rects.push_back( rect );

				if ( !have_bounds )
				{
					left = rect.x;
					top = rect.y;
					have_bounds = true;
				}
				else
				{
					left = std::min( left, rect.x );
					top = std::min( top, rect.y );
				}
			}

			for ( std::size_t i = 0; i < rects.size(); ++i )
			{
				const SDL_Rect &rect = rects[i];
				FeMonitor mon(
					static_cast<int>( i ),
					rect.w,
					rect.h );

				mon.transform = FeTransform().translate(
					{ static_cast<float>( rect.x - left ), static_cast<float>( rect.y - top ) } );

				FeDebug() << "Multimon: monitor #" << i
					<< ": " << mon.size.x << "x" << mon.size.y << " @ "
					<< rect.x << "," << rect.y << std::endl;

				m_mon.push_back( mon );
			}
		}
		else if ( displays )
			SDL_free( displays );
	}
#endif
	{
		FeMonitor mc( 0, m_window.get_size().x, m_window.get_size().y );

#ifdef SDL_PLATFORM_WINDOWS
		//
		// On Windows 'Fill screen' mode our window is offscreen 1 pixel in each direction, so correct
		// for that here to align draw area with screen.
		//
		if ( m_window.get_window_mode() == FeSettings::Fillscreen )
		{
			mc.size.x -= 2;
			mc.size.y -= 2;
			mc.transform = FeTransform().translate( { 1.0f, 1.0f } );
		}
#endif

		m_mon.push_back( mc );
	}

	ASSERT( m_mon.size() > 0 );

	m_layoutSize = m_mon[0].size;

	if ( m_refresh_rate == 0 )
	{
		FeLog() << "Failed to detect the refresh rate. Defaulting to 60 Hz" << std::endl;
		m_refresh_rate = 60;
	}
	else
		FeDebug() << "Monitor's Refresh Rate: " << m_refresh_rate << " Hz" << std::endl;
}

FePresent::~FePresent()
{
	clear_resources();
}

//
// Clears common resources as well as layout-specific ones
// - The KMSDRM backend closes the window before launching the emulator
// - This requires the release of additional resources
//
void FePresent::clear_resources()
{
	clear_layout();
	delete m_defaultFont;
	m_defaultFont = NULL;
	SDL_DestroySurface( m_logo_image );
	m_logo_image = NULL;
	SDL_DestroySurface( m_logo_full_image );
	m_logo_full_image = NULL;
}

//
// Clear layout-specific resources
//
void FePresent::clear_layout()
{
	m_window.get_gpu_context().clear_layout_resources();

	//
	// keep toggle rotation, base rotation and mute state through clear
	//
	// Activating the screen saver keeps the previous base rotation, while
	// mute and toggle rotation are kept whenever the layout is changed
	//
	m_listBox=NULL; // listbox gets deleted with the m_mon.elements below
	m_layout_transform = FeTransform();
	m_ui_transform = FeTransform();
	m_layoutFont = NULL;
	m_layoutFontName = "";
	m_user_page_size = -1;
	m_preserve_aspect = false;
	m_custom_overlay = false;
	m_overlay_caption = NULL;
	m_overlay_lb = NULL;

	FeImageLoader &il = FeImageLoader::get_ref();
	il.set_background_loading( false );

	for ( std::vector<FeMonitor>::iterator itr=m_mon.begin(); itr!=m_mon.end(); ++itr )
	{
		while ( !(*itr).elements.empty() )
		{
			FeBasePresentable *p = (*itr).elements.back();
			(*itr).elements.pop_back();
			delete p;
		}
	}

	while ( !m_texturePool.empty() )
	{
		FeBaseTextureContainer *t = m_texturePool.back();
		m_texturePool.pop_back();
		delete t;
	}

	while ( !m_sounds.empty() )
	{
		FeSound *s = m_sounds.back();
		m_sounds.pop_back();
		delete s;
	}

	while ( !m_musics.empty() )
	{
		FeMusic *a = m_musics.back();
		m_musics.pop_back();
		delete a;
	}

	while ( !m_scriptShaders.empty() )
	{
		FeShader *sh = m_scriptShaders.back();
		m_scriptShaders.pop_back();
		delete sh;
	}

	if ( m_emptyShader )
	{
		delete m_emptyShader;
		m_emptyShader = NULL;
	}

	while ( !m_fontPool.empty() )
	{
		FeFontContainer *f = m_fontPool.back();
		m_fontPool.pop_back();
		delete f;
	}

	m_baseRotation = m_feSettings->get_screen_rotation();

	FeSettings::RotationState actualRotation = get_actual_rotation();

	if (( actualRotation == FeSettings::RotateLeft ) || ( actualRotation == FeSettings::RotateRight ))
	{
		m_layoutSize.x = m_mon[0].size.y;
		m_layoutSize.y = m_mon[0].size.x;
	}
	else
	{
		m_layoutSize.x = m_mon[0].size.x;
		m_layoutSize.y = m_mon[0].size.y;
	}

	m_layoutScale.x = 1.0f;
	m_layoutScale.y = 1.0f;

	FeBlend::clear_default_shaders();

	set_mouse_pointer( false );

}

namespace
{
	bool zcompare( FeBasePresentable *one, FeBasePresentable *two )
	{
		if ( one->get_zorder() < two->get_zorder() )
			return true;

		if ( one->get_zorder() > two->get_zorder() )
			return false;

		if ( one->get_zbuffer() || two->get_zbuffer() )
			return false;

		return ( one->get_z() < two->get_z() );
	}

	bool build_normal_matrix3_from_model( const float *model_matrix, float *out_normal_matrix )
	{
		const float a00 = model_matrix[0];
		const float a01 = model_matrix[4];
		const float a02 = model_matrix[8];
		const float a10 = model_matrix[1];
		const float a11 = model_matrix[5];
		const float a12 = model_matrix[9];
		const float a20 = model_matrix[2];
		const float a21 = model_matrix[6];
		const float a22 = model_matrix[10];

		const float det =
			a00 * ( a11 * a22 - a12 * a21 ) -
			a01 * ( a10 * a22 - a12 * a20 ) +
			a02 * ( a10 * a21 - a11 * a20 );
		if ( std::fabs( det ) <= 1.0e-6f )
			return false;

		const float inv_det = 1.0f / det;
		const float inverse[9] = {
			( a11 * a22 - a12 * a21 ) * inv_det,
			( a02 * a21 - a01 * a22 ) * inv_det,
			( a01 * a12 - a02 * a11 ) * inv_det,
			( a12 * a20 - a10 * a22 ) * inv_det,
			( a00 * a22 - a02 * a20 ) * inv_det,
			( a02 * a10 - a00 * a12 ) * inv_det,
			( a10 * a21 - a11 * a20 ) * inv_det,
			( a01 * a20 - a00 * a21 ) * inv_det,
			( a00 * a11 - a01 * a10 ) * inv_det
		};

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

	Vec2f transform_direction_xy( const FeTransform &transform, const Vec2f &direction )
	{
		const Vec2f origin = transform.transformPoint( { 0.0f, 0.0f } );
		const Vec2f endpoint = transform.transformPoint( direction );
		return endpoint - origin;
	}

	FeRenderPbrInstance make_pbr_instance( const FeRenderGeometry &entry )
	{
		FeRenderPbrInstance instance;
		std::memcpy( instance.model_matrix, entry.model_matrix, sizeof( instance.model_matrix ) );
		std::memcpy( instance.normal_matrix, entry.normal_matrix, sizeof( instance.normal_matrix ) );
		return instance;
	}

	bool pbr_texture_binding_matches( const FeRenderTextureBinding &lhs, const FeRenderTextureBinding &rhs )
	{
		return lhs.texture_id == rhs.texture_id
			&& lhs.texture_source_type == rhs.texture_source_type
			&& lhs.repeated == rhs.repeated
			&& lhs.smooth == rhs.smooth
			&& lhs.mipmap == rhs.mipmap
			&& lhs.dynamic == rhs.dynamic
			&& lhs.width == rhs.width
			&& lhs.height == rhs.height
			&& lhs.content_version == rhs.content_version
			&& lhs.offset_u == rhs.offset_u
			&& lhs.offset_v == rhs.offset_v
			&& lhs.scale_u == rhs.scale_u
			&& lhs.scale_v == rhs.scale_v
			&& lhs.rotation == rhs.rotation
			&& lhs.fit_scale_u == rhs.fit_scale_u
			&& lhs.fit_scale_v == rhs.fit_scale_v
			&& lhs.texcoord_set == rhs.texcoord_set;
	}

	bool is_batchable_pbr_geometry( const FeRenderGeometry &entry )
	{
		if ( entry.geometry_kind != FeRenderGeometryObjectPbr
			|| !entry.has_external_vertices()
			|| !entry.zbuffer
			|| entry.pbr_material.alpha_mode == FeRenderPbrAlphaBlend )
		{
			return false;
		}

		for ( int light_index = 0; light_index < entry.light_count; ++light_index )
			if ( entry.lights[light_index].type != FeRenderPbrLightDirectional )
				return false;

		return true;
	}

	bool can_batch_pbr_geometry( const FeRenderGeometry &lhs, const FeRenderGeometry &rhs )
	{
		if ( !is_batchable_pbr_geometry( lhs )
			|| !is_batchable_pbr_geometry( rhs )
			|| lhs.get_vertex_count() != rhs.get_vertex_count()
			|| lhs.external_vertex_id != rhs.external_vertex_id
			|| lhs.texture_id != rhs.texture_id
			|| lhs.texture_source_type != rhs.texture_source_type
			|| lhs.texture_repeated != rhs.texture_repeated
			|| lhs.texture_smooth != rhs.texture_smooth
			|| lhs.texture_mipmap != rhs.texture_mipmap
			|| lhs.texture_dynamic != rhs.texture_dynamic
			|| lhs.texture_content_version != rhs.texture_content_version
			|| lhs.blend_mode != rhs.blend_mode
			|| lhs.camera_light != rhs.camera_light
			|| lhs.light_count != rhs.light_count )
		{
			return false;
		}

		const FeRenderPbrMaterial &lhs_material = lhs.pbr_material;
		const FeRenderPbrMaterial &rhs_material = rhs.pbr_material;
		if ( lhs_material.alpha_mode != rhs_material.alpha_mode
			|| lhs_material.unlit != rhs_material.unlit
			|| lhs_material.double_sided != rhs_material.double_sided
			|| lhs_material.metallic_factor != rhs_material.metallic_factor
			|| lhs_material.roughness_factor != rhs_material.roughness_factor
			|| lhs_material.normal_scale != rhs_material.normal_scale
			|| lhs_material.occlusion_strength != rhs_material.occlusion_strength
			|| lhs_material.alpha_cutoff != rhs_material.alpha_cutoff )
		{
			return false;
		}

		for ( int i = 0; i < 4; ++i )
			if ( lhs_material.base_color_factor[i] != rhs_material.base_color_factor[i] )
				return false;

		for ( int i = 0; i < 3; ++i )
		{
			if ( lhs_material.emissive_factor[i] != rhs_material.emissive_factor[i]
				|| lhs.ambient_color[i] != rhs.ambient_color[i] )
			{
				return false;
			}
		}

		if ( !pbr_texture_binding_matches( lhs_material.base_color_texture, rhs_material.base_color_texture )
			|| !pbr_texture_binding_matches( lhs_material.metallic_roughness_texture, rhs_material.metallic_roughness_texture )
			|| !pbr_texture_binding_matches( lhs_material.normal_texture, rhs_material.normal_texture )
			|| !pbr_texture_binding_matches( lhs_material.occlusion_texture, rhs_material.occlusion_texture )
			|| !pbr_texture_binding_matches( lhs_material.emissive_texture, rhs_material.emissive_texture ) )
		{
			return false;
		}

		for ( int light_index = 0; light_index < lhs.light_count; ++light_index )
		{
			const FeRenderPbrLight &lhs_light = lhs.lights[light_index];
			const FeRenderPbrLight &rhs_light = rhs.lights[light_index];
			if ( lhs_light.type != rhs_light.type
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

	std::uint64_t hash_combine_pbr_batch( std::uint64_t seed, std::uint64_t value )
	{
		return seed ^ ( value + 0x9e3779b97f4a7c15ULL + ( seed << 6 ) + ( seed >> 2 ) );
	}

	std::uint64_t hash_float_pbr_batch( std::uint64_t seed, float value )
	{
		std::uint32_t bits = 0;
		std::memcpy( &bits, &value, sizeof( bits ) );
		return hash_combine_pbr_batch( seed, static_cast<std::uint64_t>( bits ) );
	}

	void hash_pbr_texture_binding( const FeRenderTextureBinding &binding, std::uint64_t &hash )
	{
		hash = hash_combine_pbr_batch( hash, reinterpret_cast<std::uint64_t>( binding.texture_id ) );
		hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( binding.texture_source_type ) );
		hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( binding.repeated ? 1 : 0 ) );
		hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( binding.smooth ? 1 : 0 ) );
		hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( binding.mipmap ? 1 : 0 ) );
		hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( binding.dynamic ? 1 : 0 ) );
		hash = hash_float_pbr_batch( hash, binding.width );
		hash = hash_float_pbr_batch( hash, binding.height );
		hash = hash_combine_pbr_batch( hash, binding.content_version );
		hash = hash_float_pbr_batch( hash, binding.offset_u );
		hash = hash_float_pbr_batch( hash, binding.offset_v );
		hash = hash_float_pbr_batch( hash, binding.scale_u );
		hash = hash_float_pbr_batch( hash, binding.scale_v );
		hash = hash_float_pbr_batch( hash, binding.rotation );
		hash = hash_float_pbr_batch( hash, binding.fit_scale_u );
		hash = hash_float_pbr_batch( hash, binding.fit_scale_v );
		hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( binding.texcoord_set ) );
	}

	std::uint64_t compute_pbr_batch_hash( const FeRenderGeometry &entry )
	{
		std::uint64_t hash = 1469598103934665603ULL;
		hash = hash_combine_pbr_batch( hash, reinterpret_cast<std::uint64_t>( entry.external_vertex_id ) );
		hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( entry.get_vertex_count() ) );
		hash = hash_combine_pbr_batch( hash, reinterpret_cast<std::uint64_t>( entry.texture_id ) );
		hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( entry.texture_source_type ) );
		hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( entry.texture_repeated ? 1 : 0 ) );
		hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( entry.texture_smooth ? 1 : 0 ) );
		hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( entry.texture_mipmap ? 1 : 0 ) );
		hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( entry.texture_dynamic ? 1 : 0 ) );
		hash = hash_combine_pbr_batch( hash, entry.texture_content_version );
		hash = hash_float_pbr_batch( hash, entry.camera_light );
		hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( entry.light_count ) );

		const FeRenderPbrMaterial &material = entry.pbr_material;
		hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( material.alpha_mode ) );
		hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( material.unlit ? 1 : 0 ) );
		hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( material.double_sided ? 1 : 0 ) );
		hash = hash_float_pbr_batch( hash, material.metallic_factor );
		hash = hash_float_pbr_batch( hash, material.roughness_factor );
		hash = hash_float_pbr_batch( hash, material.normal_scale );
		hash = hash_float_pbr_batch( hash, material.occlusion_strength );
		hash = hash_float_pbr_batch( hash, material.alpha_cutoff );
		for ( int i = 0; i < 4; ++i )
			hash = hash_float_pbr_batch( hash, material.base_color_factor[i] );
		for ( int i = 0; i < 3; ++i )
		{
			hash = hash_float_pbr_batch( hash, material.emissive_factor[i] );
			hash = hash_float_pbr_batch( hash, entry.ambient_color[i] );
		}

		hash_pbr_texture_binding( material.base_color_texture, hash );
		hash_pbr_texture_binding( material.metallic_roughness_texture, hash );
		hash_pbr_texture_binding( material.normal_texture, hash );
		hash_pbr_texture_binding( material.occlusion_texture, hash );
		hash_pbr_texture_binding( material.emissive_texture, hash );

		for ( int light_index = 0; light_index < entry.light_count; ++light_index )
		{
			const FeRenderPbrLight &light = entry.lights[light_index];
			hash = hash_combine_pbr_batch( hash, static_cast<std::uint64_t>( light.type ) );
			hash = hash_float_pbr_batch( hash, light.intensity );
			hash = hash_float_pbr_batch( hash, light.range );
			hash = hash_float_pbr_batch( hash, light.inner_cone_cos );
			hash = hash_float_pbr_batch( hash, light.outer_cone_cos );
			for ( int i = 0; i < 3; ++i )
			{
				hash = hash_float_pbr_batch( hash, light.color[i] );
				hash = hash_float_pbr_batch( hash, light.direction[i] );
			}
		}

		return hash;
	}

	void collapse_pbr_geometry_instances( std::vector<FeRenderGeometry> &geometry )
	{
		if ( geometry.empty() )
			return;

		std::vector<FeRenderGeometry> collapsed;
		collapsed.reserve( geometry.size() );

		for ( std::size_t index = 0; index < geometry.size(); )
		{
			if ( !is_batchable_pbr_geometry( geometry[index] ) )
			{
				collapsed.push_back( std::move( geometry[index] ) );
				++index;
				continue;
			}

			std::size_t run_end = index;
			while ( run_end < geometry.size() && is_batchable_pbr_geometry( geometry[run_end] ) )
				++run_end;

			std::unordered_map<std::uint64_t, std::vector<std::size_t>> run_lookup;
			run_lookup.reserve( run_end - index );
			for ( std::size_t run_index = index; run_index < run_end; ++run_index )
			{
				FeRenderGeometry &entry = geometry[run_index];
				const std::uint64_t hash = compute_pbr_batch_hash( entry );
				bool merged = false;
				auto lookup_it = run_lookup.find( hash );
				if ( lookup_it != run_lookup.end() )
				{
					for ( std::size_t collapsed_index : lookup_it->second )
					{
						FeRenderGeometry &head = collapsed[collapsed_index];
						if ( !can_batch_pbr_geometry( head, entry ) )
							continue;

						if ( !head.has_pbr_instances() )
							head.pbr_instances.push_back( make_pbr_instance( head ) );
						head.pbr_instances.push_back( make_pbr_instance( entry ) );
						merged = true;
						break;
					}
				}

				if ( merged )
					continue;

				collapsed.push_back( std::move( entry ) );
				run_lookup[hash].push_back( collapsed.size() - 1 );
			}

			index = run_end;
		}

		geometry.swap( collapsed );
	}

	void apply_object_geometry_transform( FeRenderGeometry &entry, const FeTransform &transform )
	{
		const Vec2f transformed_origin =
			transform.transformPoint( { entry.model_matrix[12], entry.model_matrix[13] } );

		float transformed_model[16] = {};
		std::memcpy( transformed_model, entry.model_matrix, sizeof( transformed_model ) );
		transformed_model[12] = transformed_origin.x;
		transformed_model[13] = transformed_origin.y;

		for ( int column = 0; column < 3; ++column )
		{
			const int index = column * 4;
			const Vec2f basis_endpoint = transform.transformPoint(
				{
					entry.model_matrix[12] + entry.model_matrix[index + 0],
					entry.model_matrix[13] + entry.model_matrix[index + 1]
				} );
			transformed_model[index + 0] = basis_endpoint.x - transformed_origin.x;
			transformed_model[index + 1] = basis_endpoint.y - transformed_origin.y;
		}

		std::memcpy( entry.model_matrix, transformed_model, sizeof( transformed_model ) );
		build_normal_matrix3_from_model( entry.model_matrix, entry.normal_matrix );

		for ( int light_index = 0; light_index < entry.light_count; ++light_index )
		{
			FeRenderPbrLight &light = entry.lights[light_index];
			const Vec2f transformed_position =
				transform.transformPoint( { light.position[0], light.position[1] } );
			const Vec2f transformed_direction =
				transform_direction_xy( transform, { light.direction[0], light.direction[1] } );
			light.position[0] = transformed_position.x;
			light.position[1] = transformed_position.y;

			const float direction_length = std::sqrt(
				( transformed_direction.x * transformed_direction.x ) +
				( transformed_direction.y * transformed_direction.y ) +
				( light.direction[2] * light.direction[2] ) );
			if ( direction_length > 1.0e-6f )
			{
				light.direction[0] = transformed_direction.x / direction_length;
				light.direction[1] = transformed_direction.y / direction_length;
				light.direction[2] /= direction_length;
			}
		}
	}

	void apply_geometry_transform( std::vector<FeRenderGeometry> &geometry, const FeTransform &transform )
	{
		if ( transform.isIdentity() )
			return;

		for ( FeRenderGeometry &entry : geometry )
		{
			if ( entry.geometry_kind == FeRenderGeometryObjectPbr )
			{
				apply_object_geometry_transform( entry, transform );
				continue;
			}

			for ( FeRenderVertex &vertex : entry.vertices )
			{
				const Vec2f p = transform.transformPoint( { vertex.x, vertex.y } );
				vertex.x = p.x;
				vertex.y = p.y;
			}
		}
	}
};

void FePresent::sort_zorder()
{
	std::vector<FeBaseTextureContainer *>::iterator itc;
	std::vector<FeMonitor>::iterator itm;

	for ( itc=m_texturePool.begin(); itc != m_texturePool.end(); ++itc )
		if ( (*itc)->get_presentable_parent() )
			std::stable_sort( (*itc)->get_presentable_parent()->elements.begin(), (*itc)->get_presentable_parent()->elements.end(), zcompare );

	for ( itm=m_mon.begin(); itm != m_mon.end(); ++itm )
		std::stable_sort( (*itm).elements.begin(), (*itm).elements.end(), zcompare );
}

void FePresent::build_render_geometry( std::vector<FeRenderGeometry> &geometry ) const
{
	geometry.clear();

	for ( std::size_t monitor_index = 0; monitor_index < m_mon.size(); ++monitor_index )
	{
		const FeMonitor &monitor = m_mon[ monitor_index ];
		std::vector<FeRenderGeometry> monitor_geometry;

		for ( const FeBasePresentable *presentable : monitor.elements )
		{
			if ( !presentable || !presentable->get_visible() )
				continue;

			const FeImage *image = dynamic_cast<const FeImage *>( presentable );
			if ( image )
			{
				FeRenderGeometry image_geometry;
				if ( image->build_render_geometry( image_geometry ) )
					monitor_geometry.push_back( image_geometry );
				continue;
			}

			const FeRectangle *rectangle = dynamic_cast<const FeRectangle *>( presentable );
			if ( rectangle )
			{
				FeRenderGeometry rectangle_geometry;
				if ( rectangle->build_render_geometry( rectangle_geometry ) )
					monitor_geometry.push_back( rectangle_geometry );
				continue;
			}

			const FeModel3D *object = dynamic_cast<const FeModel3D *>( presentable );
			if ( object )
			{
				object->build_render_geometry( monitor_geometry );
				continue;
			}

			const FeText *text = dynamic_cast<const FeText *>( presentable );
			if ( text )
			{
				text->build_render_geometry( monitor_geometry );
				continue;
			}

			const FeListBox *listbox = dynamic_cast<const FeListBox *>( presentable );
			if ( listbox )
				listbox->build_render_geometry( monitor_geometry );
		}

		const FeTransform &transform = ( monitor_index == 0 ) ? m_layout_transform : monitor.transform;
		apply_geometry_transform( monitor_geometry, transform );
		collapse_pbr_geometry_instances( monitor_geometry );
		geometry.insert( geometry.end(), monitor_geometry.begin(), monitor_geometry.end() );
	}
}

void FePresent::build_render_surface_frames( std::vector<FeRenderSurfaceFrame> &surfaces ) const
{
	surfaces.clear();

	struct SurfaceState
	{
		std::uint64_t signature;
		bool dynamic_content;
	};
	struct PendingSurfaceFrame
	{
		FeSurfaceTextureContainer *surface;
		FeRenderSurfaceFrame frame;
		std::vector<const void *> dependencies;
		std::size_t original_index;
	};

	auto hash_combine = []( std::uint64_t seed, std::uint64_t value ) -> std::uint64_t
	{
		return seed ^ ( value + 0x9e3779b97f4a7c15ULL + ( seed << 6 ) + ( seed >> 2 ) );
	};
	auto hash_float = [&]( std::uint64_t seed, float value ) -> std::uint64_t
	{
		return hash_combine( seed, static_cast<std::uint64_t>( std::lround( value * 1024.0f ) ) );
	};
	auto hash_texture_binding = [&]( const FeRenderTextureBinding &binding, std::uint64_t seed ) -> std::uint64_t
	{
		seed = hash_combine( seed, reinterpret_cast<std::uint64_t>( binding.texture_id ) );
		seed = hash_combine( seed, static_cast<std::uint64_t>( binding.texture_source_type ) );
		seed = hash_combine( seed, static_cast<std::uint64_t>( binding.repeated ? 1 : 0 ) );
		seed = hash_combine( seed, static_cast<std::uint64_t>( binding.smooth ? 1 : 0 ) );
		seed = hash_combine( seed, static_cast<std::uint64_t>( binding.mipmap ? 1 : 0 ) );
		seed = hash_combine( seed, static_cast<std::uint64_t>( binding.dynamic ? 1 : 0 ) );
		seed = hash_combine( seed, static_cast<std::uint64_t>( binding.texcoord_set ) );
		seed = hash_float( seed, binding.width );
		seed = hash_float( seed, binding.height );
		seed = hash_float( seed, binding.offset_u );
		seed = hash_float( seed, binding.offset_v );
		seed = hash_float( seed, binding.scale_u );
		seed = hash_float( seed, binding.scale_v );
		seed = hash_float( seed, binding.rotation );
		return seed;
	};
	auto hash_geometry = [&]( const FeRenderGeometry &geometry, std::uint64_t seed ) -> std::uint64_t
	{
		seed = hash_combine( seed, static_cast<std::uint64_t>( geometry.geometry_kind ) );
		seed = hash_combine( seed, reinterpret_cast<std::uint64_t>( geometry.texture_id ) );
		seed = hash_combine( seed, static_cast<std::uint64_t>( geometry.texture_source_type ) );
		seed = hash_combine( seed, static_cast<std::uint64_t>( geometry.texture_repeated ? 1 : 0 ) );
		seed = hash_combine( seed, static_cast<std::uint64_t>( geometry.texture_smooth ? 1 : 0 ) );
		seed = hash_combine( seed, static_cast<std::uint64_t>( geometry.texture_mipmap ? 1 : 0 ) );
		seed = hash_combine( seed, static_cast<std::uint64_t>( geometry.blend_mode ) );
		seed = hash_combine( seed, static_cast<std::uint64_t>( geometry.zbuffer ? 1 : 0 ) );
		seed = hash_combine( seed, static_cast<std::uint64_t>( geometry.custom_shader ? 1 : 0 ) );
		seed = hash_combine( seed, static_cast<std::uint64_t>( geometry.get_vertex_count() ) );
		if ( geometry.has_external_vertices() )
		{
			seed = hash_combine( seed, reinterpret_cast<std::uint64_t>( geometry.external_vertex_id ) );
		}
		else for ( const FeRenderVertex &vertex : geometry.vertices )
		{
			seed = hash_combine( seed, static_cast<std::uint64_t>( std::lround( vertex.x * 1024.0f ) ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( std::lround( vertex.y * 1024.0f ) ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( std::lround( vertex.z * 1024.0f ) ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( std::lround( vertex.u * 1024.0f ) ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( std::lround( vertex.v * 1024.0f ) ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( std::lround( vertex.u1 * 1024.0f ) ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( std::lround( vertex.v1 * 1024.0f ) ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( std::lround( vertex.nx * 1024.0f ) ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( std::lround( vertex.ny * 1024.0f ) ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( std::lround( vertex.nz * 1024.0f ) ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( std::lround( vertex.tx * 1024.0f ) ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( std::lround( vertex.ty * 1024.0f ) ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( std::lround( vertex.tz * 1024.0f ) ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( std::lround( vertex.tw * 1024.0f ) ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( vertex.r ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( vertex.g ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( vertex.b ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( vertex.a ) );
		}

		if ( geometry.geometry_kind == FeRenderGeometryObjectPbr )
		{
			for ( int i = 0; i < 16; ++i )
				seed = hash_float( seed, geometry.model_matrix[i] );
			for ( int i = 0; i < 9; ++i )
				seed = hash_float( seed, geometry.normal_matrix[i] );
			seed = hash_texture_binding( geometry.pbr_material.base_color_texture, seed );
			seed = hash_texture_binding( geometry.pbr_material.metallic_roughness_texture, seed );
			seed = hash_texture_binding( geometry.pbr_material.normal_texture, seed );
			seed = hash_texture_binding( geometry.pbr_material.occlusion_texture, seed );
			seed = hash_texture_binding( geometry.pbr_material.emissive_texture, seed );
			for ( int i = 0; i < 4; ++i )
				seed = hash_float( seed, geometry.pbr_material.base_color_factor[i] );
			for ( int i = 0; i < 3; ++i )
			{
				seed = hash_float( seed, geometry.pbr_material.emissive_factor[i] );
				seed = hash_float( seed, geometry.ambient_color[i] );
			}
			seed = hash_float( seed, geometry.camera_light );
			seed = hash_float( seed, geometry.pbr_material.metallic_factor );
			seed = hash_float( seed, geometry.pbr_material.roughness_factor );
			seed = hash_float( seed, geometry.pbr_material.normal_scale );
			seed = hash_float( seed, geometry.pbr_material.occlusion_strength );
			seed = hash_float( seed, geometry.pbr_material.alpha_cutoff );
			seed = hash_combine( seed, static_cast<std::uint64_t>( geometry.pbr_material.alpha_mode ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( geometry.pbr_material.unlit ? 1 : 0 ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( geometry.pbr_material.double_sided ? 1 : 0 ) );
			seed = hash_combine( seed, static_cast<std::uint64_t>( geometry.light_count ) );
			for ( int light_index = 0; light_index < geometry.light_count; ++light_index )
			{
				const FeRenderPbrLight &light = geometry.lights[light_index];
				seed = hash_combine( seed, static_cast<std::uint64_t>( light.type ) );
				for ( int i = 0; i < 3; ++i )
				{
					seed = hash_float( seed, light.color[i] );
					seed = hash_float( seed, light.position[i] );
					seed = hash_float( seed, light.direction[i] );
				}
				seed = hash_float( seed, light.intensity );
				seed = hash_float( seed, light.range );
				seed = hash_float( seed, light.inner_cone_cos );
				seed = hash_float( seed, light.outer_cone_cos );
			}
		}

		return seed;
	};
	auto append_surface_dependencies =
		[]( const FeRenderGeometry &geometry,
			const FeSurfaceTextureContainer *current_surface,
			std::vector<const void *> &dependencies )
		{
			auto add_binding_dependency =
				[]( const FeRenderTextureBinding &binding,
					const FeSurfaceTextureContainer *surface,
					std::vector<const void *> &dependency_list )
				{
					if ( binding.texture_source_type != FeRenderTextureSourceContainer || !binding.texture_id )
						return;

					const FeSurfaceTextureContainer *referenced_surface =
						dynamic_cast<const FeSurfaceTextureContainer *>(
							static_cast<const FeBaseTextureContainer *>( binding.texture_id ) );
					if ( referenced_surface && referenced_surface != surface )
						dependency_list.push_back( referenced_surface );
				};

			if ( geometry.texture_source_type == FeRenderTextureSourceContainer )
			{
				const FeSurfaceTextureContainer *referenced_surface =
					dynamic_cast<const FeSurfaceTextureContainer *>(
						static_cast<const FeBaseTextureContainer *>( geometry.texture_id ) );
				if ( referenced_surface && referenced_surface != current_surface )
					dependencies.push_back( referenced_surface );
			}

			if ( geometry.geometry_kind == FeRenderGeometryObjectPbr )
			{
				add_binding_dependency( geometry.pbr_material.base_color_texture, current_surface, dependencies );
				add_binding_dependency( geometry.pbr_material.metallic_roughness_texture, current_surface, dependencies );
				add_binding_dependency( geometry.pbr_material.normal_texture, current_surface, dependencies );
				add_binding_dependency( geometry.pbr_material.occlusion_texture, current_surface, dependencies );
				add_binding_dependency( geometry.pbr_material.emissive_texture, current_surface, dependencies );
			}

			if ( !geometry.shader )
				return;

			for ( const auto &entry : geometry.shader->get_texture_param_images() )
			{
				FeImage *shader_image = entry.second;
				const FeBaseTextureContainer *texture_container =
					shader_image ? shader_image->get_texture_container() : nullptr;
				const FeSurfaceTextureContainer *referenced_surface =
					dynamic_cast<const FeSurfaceTextureContainer *>( texture_container );
				if ( referenced_surface && referenced_surface != current_surface )
					dependencies.push_back( referenced_surface );
			}
		};

	std::vector<FeSurfaceTextureContainer *> surface_list;
	surface_list.reserve( m_texturePool.size() );
	for ( FeBaseTextureContainer *container : m_texturePool )
	{
		FeSurfaceTextureContainer *surface = dynamic_cast<FeSurfaceTextureContainer *>( container );
		if ( surface )
			surface_list.push_back( surface );
	}

	std::stable_sort(
		surface_list.begin(),
		surface_list.end(),
		[]( FeSurfaceTextureContainer *a, FeSurfaceTextureContainer *b )
		{
			return a->get_nesting_level() > b->get_nesting_level();
		} );

	std::vector<PendingSurfaceFrame> pending_frames;
	pending_frames.reserve( surface_list.size() );
	for ( std::size_t surface_index = 0; surface_index < surface_list.size(); ++surface_index )
	{
		FeSurfaceTextureContainer *surface = surface_list[ surface_index ];
		FeRenderSurfaceFrame frame;
		frame.surface_id = surface;
		frame.surface_texture_id = surface;
		frame.surface_texture_source_type = FeRenderTextureSourceContainer;
		frame.nesting_level = surface->get_nesting_level();
		frame.width = surface->get_width();
		frame.height = surface->get_height();
		frame.mipmapped = surface->get_mipmap();
		frame.clear = surface->get_clear();
		frame.redraw = surface->get_redraw();
		frame.camera = m_layout_camera;
		frame.camera.update_projection( static_cast<float>( frame.width ), static_cast<float>( frame.height ) );
		frame.geometry_signature = 1469598103934665603ULL;
		frame.content_signature = 1469598103934665603ULL;
		std::vector<const void *> dependencies;

		for ( const FeBasePresentable *presentable : surface->elements )
		{
			if ( !presentable || !presentable->get_visible() )
				continue;

			const FeImage *image = dynamic_cast<const FeImage *>( presentable );
			if ( image )
			{
				FeRenderGeometry image_geometry;
				if ( image->build_render_geometry( image_geometry ) )
				{
					append_surface_dependencies( image_geometry, surface, dependencies );
					frame.dynamic_content = frame.dynamic_content || image_geometry.texture_dynamic || image_geometry.custom_shader;
					frame.geometry_signature = hash_geometry( image_geometry, frame.geometry_signature );
					frame.content_signature = hash_geometry( image_geometry, frame.content_signature );
					frame.geometry.push_back( image_geometry );
				}
				continue;
			}

			const FeRectangle *rectangle = dynamic_cast<const FeRectangle *>( presentable );
			if ( rectangle )
			{
				FeRenderGeometry rectangle_geometry;
				if ( rectangle->build_render_geometry( rectangle_geometry ) )
				{
					append_surface_dependencies( rectangle_geometry, surface, dependencies );
					frame.dynamic_content = frame.dynamic_content || rectangle_geometry.texture_dynamic || rectangle_geometry.custom_shader;
					frame.geometry_signature = hash_geometry( rectangle_geometry, frame.geometry_signature );
					frame.content_signature = hash_geometry( rectangle_geometry, frame.content_signature );
					frame.geometry.push_back( rectangle_geometry );
				}
				continue;
			}

			const FeModel3D *object = dynamic_cast<const FeModel3D *>( presentable );
			if ( object )
			{
				const std::size_t old_size = frame.geometry.size();
				object->build_render_geometry( frame.geometry );
				for ( std::size_t i = old_size; i < frame.geometry.size(); ++i )
				{
					const FeRenderGeometry &object_geometry = frame.geometry[i];
					append_surface_dependencies( object_geometry, surface, dependencies );
					frame.dynamic_content = frame.dynamic_content || object_geometry.texture_dynamic;
					frame.geometry_signature = hash_geometry( object_geometry, frame.geometry_signature );
					frame.content_signature = hash_geometry( object_geometry, frame.content_signature );
				}
				continue;
			}

			const FeText *text = dynamic_cast<const FeText *>( presentable );
			if ( text )
			{
				const std::size_t old_size = frame.geometry.size();
				text->build_render_geometry( frame.geometry );
				for ( std::size_t i = old_size; i < frame.geometry.size(); ++i )
				{
					const FeRenderGeometry &text_geometry = frame.geometry[i];
					append_surface_dependencies( text_geometry, surface, dependencies );
					frame.dynamic_content = frame.dynamic_content || text_geometry.texture_dynamic || text_geometry.custom_shader;
					frame.geometry_signature = hash_geometry( text_geometry, frame.geometry_signature );
					frame.content_signature = hash_geometry( text_geometry, frame.content_signature );
				}
				continue;
			}

			const FeListBox *listbox = dynamic_cast<const FeListBox *>( presentable );
			if ( listbox )
			{
				const std::size_t old_size = frame.geometry.size();
				listbox->build_render_geometry( frame.geometry );
				for ( std::size_t i = old_size; i < frame.geometry.size(); ++i )
				{
					const FeRenderGeometry &list_geometry = frame.geometry[i];
					append_surface_dependencies( list_geometry, surface, dependencies );
					frame.dynamic_content = frame.dynamic_content || list_geometry.texture_dynamic || list_geometry.custom_shader;
					frame.geometry_signature = hash_geometry( list_geometry, frame.geometry_signature );
					frame.content_signature = hash_geometry( list_geometry, frame.content_signature );
				}
			}
		}

		if ( frame.redraw )
			frame.dynamic_content = true;

		std::sort( dependencies.begin(), dependencies.end() );
		dependencies.erase( std::unique( dependencies.begin(), dependencies.end() ), dependencies.end() );
		frame.dependencies = dependencies;
		pending_frames.push_back( { surface, frame, dependencies, surface_index } );
	}

	std::unordered_map<const FeSurfaceTextureContainer *, std::size_t> pending_indices;
	pending_indices.reserve( pending_frames.size() );
	for ( std::size_t i = 0; i < pending_frames.size(); ++i )
		pending_indices[ pending_frames[i].surface ] = i;

	std::vector<int> indegree( pending_frames.size(), 0 );
	std::vector<std::vector<std::size_t>> dependents( pending_frames.size() );
	for ( std::size_t i = 0; i < pending_frames.size(); ++i )
	{
		for ( const void *dependency : pending_frames[i].dependencies )
		{
			auto it = pending_indices.find( static_cast<const FeSurfaceTextureContainer *>( dependency ) );
			if ( it == pending_indices.end() )
				continue;

			++indegree[i];
			dependents[it->second].push_back( i );
		}
	}

	std::vector<std::size_t> ordered_indices;
	ordered_indices.reserve( pending_frames.size() );
	std::vector<bool> emitted( pending_frames.size(), false );
	while ( ordered_indices.size() < pending_frames.size() )
	{
		std::size_t chosen = pending_frames.size();
		for ( std::size_t i = 0; i < pending_frames.size(); ++i )
		{
			if ( emitted[i] || indegree[i] != 0 )
				continue;

			if ( chosen == pending_frames.size() ||
				pending_frames[i].frame.nesting_level > pending_frames[chosen].frame.nesting_level ||
				( pending_frames[i].frame.nesting_level == pending_frames[chosen].frame.nesting_level &&
					pending_frames[i].original_index < pending_frames[chosen].original_index ) )
			{
				chosen = i;
			}
		}

		if ( chosen == pending_frames.size() )
		{
			for ( std::size_t i = 0; i < pending_frames.size(); ++i )
			{
				if ( emitted[i] )
					continue;

				chosen = i;
				break;
			}
		}

		emitted[chosen] = true;
		ordered_indices.push_back( chosen );
		for ( std::size_t dependent : dependents[chosen] )
			--indegree[dependent];
	}

	std::unordered_map<const FeSurfaceTextureContainer *, SurfaceState> surface_states;
	surface_states.reserve( pending_frames.size() );
	for ( std::size_t ordered_index : ordered_indices )
	{
		PendingSurfaceFrame &pending = pending_frames[ordered_index];
		for ( const void *dependency : pending.dependencies )
		{
			auto it = surface_states.find( static_cast<const FeSurfaceTextureContainer *>( dependency ) );
			if ( it == surface_states.end() )
				continue;

			pending.frame.content_signature = hash_combine( pending.frame.content_signature, it->second.signature );
			pending.frame.dynamic_content = pending.frame.dynamic_content || it->second.dynamic_content;
		}

		surface_states[ pending.surface ] = { pending.frame.content_signature, pending.frame.dynamic_content };
		surfaces.push_back( pending.frame );
	}
}

FeImage *FePresent::add_image( bool is_artwork,
		const std::string &n,
		float x,
		float y,
		float w,
		float h,
		FePresentableParent &p )
{
	FeTextureContainer *new_tex = new FeTextureContainer( is_artwork, n );
	new_tex->set_smooth( m_feSettings->get_info_bool( FeSettings::SmoothImages ) );

	FeImage *new_image = new FeImage( p, new_tex, x, y, w, h );
	new_image->set_scale_factor( m_layoutScale.x, m_layoutScale.y );

	// if this is a static image/video then load it now
	//
	if (( !is_artwork ) && ( n.find_first_of( "[" ) == std::string::npos ))
		new_image->setFileName( n.c_str() );

	if ( get_script_id() < 0 )
		m_layout_has_content = true;

	flag_redraw();
	m_texturePool.push_back( new_tex );
	p.elements.push_back( new_image );
	flag_sort_zorder();

	return new_image;
}

FeModel3D *FePresent::add_model_3d( const std::string &n, FePresentableParent &p )
{
	FeModel3D *new_model_3d = new FeModel3D( p, n );

	if ( get_script_id() < 0 )
		m_layout_has_content = true;

	flag_redraw();
	p.elements.push_back( new_model_3d );
	flag_sort_zorder();
	return new_model_3d;
}

FeImage *FePresent::add_clone( FeImage *o,
			FePresentableParent &p )
{
	FeImage *new_image = new FeImage( o, p );
	flag_redraw();
	p.elements.push_back( new_image );
	flag_sort_zorder();

	if ( o->get_presentable_parent() != NULL )
		o->get_presentable_parent()->set_nesting_level( p.get_nesting_level() + 1 );

	return new_image;
}

FeModel3D *FePresent::add_clone( FeModel3D *o, FePresentableParent &p )
{
	FeModel3D *new_model_3d = new FeModel3D( o, p );
	flag_redraw();
	p.elements.push_back( new_model_3d );
	flag_sort_zorder();
	return new_model_3d;
}

FeText *FePresent::add_text( const std::string &n, int x, int y, int w, int h,
			FePresentableParent &p )
{
	FeText *new_text = new FeText( p, n, x, y, w, h );

	new_text->setFont( *get_layout_font() );
	new_text->set_scale_factor( m_layoutScale.x, m_layoutScale.y );

	if ( get_script_id() < 0 )
		m_layout_has_content = true;

	flag_redraw();
	p.elements.push_back( new_text );
	flag_sort_zorder();
	return new_text;
}

FeListBox *FePresent::add_listbox( int x, int y, int w, int h,
		FePresentableParent &p )
{
	FeListBox *new_lb = new FeListBox( p, x, y, w, h );

	new_lb->setFont( *get_layout_font() );
	new_lb->set_scale_factor( m_layoutScale.x, m_layoutScale.y );

	if ( get_script_id() < 0 )
		m_layout_has_content = true;

	flag_redraw();
	m_listBox = new_lb;
	p.elements.push_back( new_lb );
	flag_sort_zorder();
	return new_lb;
}

FeRectangle *FePresent::add_rectangle( float x, float y, float w, float h,
		FePresentableParent &p )
{
	FeRectangle *new_rc = new FeRectangle( p, x, y, w, h );
	new_rc->set_scale_factor( m_layoutScale.x, m_layoutScale.y );

	if ( get_script_id() < 0 )
		m_layout_has_content = true;

	flag_redraw();
	p.elements.push_back( new_rc );
	flag_sort_zorder();
	return new_rc;
}

FeImage *FePresent::add_surface( float x, float y, int w, int h, FePresentableParent &p )
{
	FeSurfaceTextureContainer *new_surface = new FeSurfaceTextureContainer( w, h );
	new_surface->set_smooth( m_feSettings->get_info_bool( FeSettings::SmoothImages ) );
	new_surface->set_nesting_level( p.get_nesting_level() + 1 );

	//
	// Set the default sprite size to the same as the texture itself
	//
	FeImage *new_image = new FeImage( p, new_surface, x, y, w, h );
	new_image->set_scale_factor( m_layoutScale.x, m_layoutScale.y );
	new_image->set_blend_mode( FeBlend::Premultiplied );

	new_image->texture_changed();

	if ( get_script_id() < 0 )
		m_layout_has_content = true;

	flag_redraw();
	p.elements.push_back( new_image );
	m_texturePool.push_back( new_surface );
	flag_sort_zorder();
	return new_image;
}

FeSound *FePresent::add_sound( const char *n )
{
	std::string path;
	std::string name=n;
	if ( !name.empty() )
	{
		if ( is_relative_path( name ) )
		{
			int script_id = get_script_id();
			if ( script_id < 0 )
				m_feSettings->get_path( FeSettings::Current, path );
			else
				m_feSettings->get_plugin_full_path( script_id, path );
		}
	}

	FeSound *new_sound = new FeSound();

	if ( !name.empty() )
		new_sound->load( path + name );

	m_sounds.push_back( new_sound );
	return new_sound;
}

FeMusic *FePresent::add_music( const char *n )
{
	std::string path;
	std::string name=n;
	if ( !name.empty() )
	{
		if ( is_relative_path( name ) )
		{
			int script_id = get_script_id();
			if ( script_id < 0 )
				m_feSettings->get_path( FeSettings::Current, path );
			else
				m_feSettings->get_plugin_full_path( script_id, path );
		}
	}

	FeMusic *new_music = new FeMusic();

	if ( !name.empty() )
		new_music->load( path + name );

	m_musics.push_back( new_music );
	return new_music;
}

namespace
{
	// Return shader path
	std::string get_shader_path( FeSettings *fes, const char *shader )
	{
		if ( !shader ) return "";
		std::string path = "";
		std::string filename = clean_path( shader );
		if ( is_relative_path( filename ) ) fes->get_path( FeSettings::Current, path );
		return path + filename;
	}
}

FeShader *FePresent::add_shader( FeShader::Type type, const char *shader1, const char *shader2 )
{
	m_scriptShaders.push_back( new FeShader() );
	FeShader *sh = m_scriptShaders.back();

	switch ( type )
	{
		case FeShader::VertexAndFragment:
			sh->load( get_shader_path( m_feSettings, shader1 ), get_shader_path( m_feSettings, shader2 ) );
			break;

		case FeShader::Vertex:
		case FeShader::Fragment:
		{
			sh->load( get_shader_path( m_feSettings, shader1 ), type );
			break;
		}

		case FeShader::Empty:
		default:
			// do nothing, sh shader remains empty
			break;
	}

	return sh;
}

FeShader *FePresent::compile_shader( FeShader::Type type, const char *shader1, const char *shader2 )
{
	m_scriptShaders.push_back( new FeShader() );
	FeShader *sh = m_scriptShaders.back();

	switch ( type )
	{
		case FeShader::VertexAndFragment:
			sh->loadFromMemory( shader1, shader2 );
			break;

		case FeShader::Vertex:
		case FeShader::Fragment:
			sh->loadFromMemory( shader1, type );
			break;

		case FeShader::Empty:
		default:
			// do nothing, sh shader remains empty
			break;
	}

	return sh;
}

float FePresent::get_layout_width() const
{
	return (float)m_layoutSize.x;
}

float FePresent::get_layout_height() const
{
	return (float)m_layoutSize.y;
}

float FePresent::get_layout_scale_x() const
{
	return m_layoutScale.x;
}

float FePresent::get_layout_scale_y() const
{
	return m_layoutScale.y;
}

void FePresent::set_layout_width( float w )
{
	if ( w != m_layoutSize.x )
	{
		m_layoutSize.x = w;
		set_transforms();
		flag_redraw();
	}
}

void FePresent::set_layout_height( float h )
{
	if ( h != m_layoutSize.y )
	{
		m_layoutSize.y = h;
		set_transforms();
		flag_redraw();
	}
}

float FePresent::get_perspective_fov() const
{
	return m_layout_camera.fov_y_degrees;
}

void FePresent::set_perspective_fov( float fov )
{
	if ( fov <= 1.0f || fov >= 179.0f || fov == m_layout_camera.fov_y_degrees )
		return;

	m_layout_camera.fov_y_degrees = fov;
	set_transforms();
	flag_redraw();
}

float FePresent::get_perspective_near() const
{
	return m_layout_camera.near_plane;
}

void FePresent::set_perspective_near( float near_plane )
{
	if ( near_plane <= 0.0f || near_plane >= m_layout_camera.far_plane || near_plane == m_layout_camera.near_plane )
		return;

	m_layout_camera.near_plane = near_plane;
	set_transforms();
	flag_redraw();
}

float FePresent::get_perspective_far() const
{
	return m_layout_camera.far_plane;
}

void FePresent::set_perspective_far( float far_plane )
{
	if ( far_plane <= m_layout_camera.near_plane || far_plane == m_layout_camera.far_plane )
		return;

	m_layout_camera.far_plane = far_plane;
	set_transforms();
	flag_redraw();
}

float FePresent::get_perspective_default_z() const
{
	return m_layout_camera.default_plane_z;
}

void FePresent::set_perspective_default_z( float z )
{
	if ( z == m_layout_camera.default_plane_z )
		return;

	m_layout_camera.default_plane_z = z;
	flag_redraw();
}

float FePresent::get_camera_light() const
{
	return m_camera_light;
}

void FePresent::set_camera_light( float light )
{
	if ( light < 0.0f )
		light = 0.0f;

	if ( light == m_camera_light )
		return;

	m_camera_light = light;
	flag_redraw();
}

const FeFontContainer *FePresent::get_pooled_font( const std::string &n )
{
	std::vector<std::string> my_list;
	my_list.push_back( n );

	return get_pooled_font( my_list );
}

const FeFontContainer *FePresent::get_pooled_font(
		const std::vector < std::string > &l )
{
	std::string ffile;

	// search list for a font
	for ( std::vector<std::string>::const_iterator itr=l.begin();
			itr != l.end(); ++itr )
	{
		if ( m_feSettings->get_font_file( ffile, *itr ) )
			break;
	}

	if ( ffile.empty() )
		return get_default_font_container();

	// Next check if this font is already loaded in our pool
	//
	for ( std::vector<FeFontContainer *>::iterator itr=m_fontPool.begin();
		itr != m_fontPool.end(); ++itr )
	{
		if ( ffile.compare( (*itr)->get_name() ) == 0 )
			return *itr;
	}

	// No match, so load this font and add it to the pool
	//
	m_fontPool.push_back( new FeFontContainer() );
	m_fontPool.back()->set_font( ffile );

	return m_fontPool.back();
}

void FePresent::set_layout_font_name( const char *n )
{
	const FeFontContainer *font = get_pooled_font( n );

	if ( font )
	{
		m_layoutFontName = n;
		m_layoutFont = font;
		flag_redraw();
	}
}

const char *FePresent::get_layout_font_name() const
{
	return m_layoutFontName.c_str();
}

void FePresent::set_base_rotation( int r )
{
	FeLog() << "! NOTE: Setting base_rotation is deprecated. Use toggle_rotation or Screen Rotation in General Settings." << std::endl;
}

int FePresent::get_base_rotation() const
{
	return m_baseRotation;
}

void FePresent::set_toggle_rotation( int r )
{
	if ( r != m_toggleRotation )
	{
		m_toggleRotation = (FeSettings::RotationState)r;
		set_transforms();
		flag_redraw();
	}
}

int FePresent::get_toggle_rotation() const
{
	return m_toggleRotation;
}

const char *FePresent::get_display_name() const
{
	return m_feSettings->get_current_display_title().c_str();
}

int FePresent::get_display_index() const
{
	return m_feSettings->get_current_display_index();
}

const char *FePresent::get_filter_name() const
{
	return m_feSettings->get_filter_name( m_feSettings->get_current_filter_index() ).c_str();
}

int FePresent::get_filter_index() const
{
	return m_feSettings->get_current_filter_index();
}

void FePresent::set_filter_index( int idx )
{
	int new_offset = idx - get_filter_index();
	if ( new_offset != 0 )
	{
		if ( m_feSettings->navigate_filter( new_offset ) )
			load_layout();
		else
		{
			update_to( ToNewList, false );
			on_transition( ToNewList, new_offset );
		}
	}
}

int FePresent::get_current_filter_size() const
{
	return m_feSettings->get_filter_size( m_feSettings->get_current_filter_index() );
}

bool FePresent::get_clones_list_showing() const
{
	if ( m_feSettings->get_clone_index() >= 0 )
		return true;
	else
		return false;
}

Sqrat::Array FePresent::get_tags_available() const
{
	HSQUIRRELVM vm = Sqrat::DefaultVM::Get();
	Sqrat::Array tags( vm );
	for ( auto tag : m_feSettings->get_tags_available() )
		tags.Append( tag );
	return tags;
}

int FePresent::get_selection_index() const
{
	return m_feSettings->get_rom_index( m_feSettings->get_current_filter_index(), 0 );
}

int FePresent::get_sort_by() const
{
	FeRomInfo::Index idx;
	bool rev;
	int limit;

	m_feSettings->get_current_sort( idx, rev, limit );
	return idx;
}

bool FePresent::get_reverse_order() const
{
	FeRomInfo::Index idx;
	bool rev;
	int limit;

	m_feSettings->get_current_sort( idx, rev, limit );
	return rev;
}

int FePresent::get_list_limit() const
{
	FeRomInfo::Index idx;
	bool rev;
	int limit;

	m_feSettings->get_current_sort( idx, rev, limit );
	return limit;
}

void FePresent::set_selection_index( int index )
{
	int new_offset = index - get_selection_index();
	if ( new_offset != 0 )
		change_selection( new_offset );
}

void FePresent::change_selection( int step, bool end_navigation )
{
		on_transition( ToNewSelection, step );

		m_feSettings->step_current_selection( step );
		update_to( ToNewSelection, false );

		on_transition( FromOldSelection, -step );

		if ( end_navigation )
		{
			update_to( EndNavigation, false );
			on_transition( EndNavigation, 0 );
		}
}

bool FePresent::reset_screen_saver()
{
	if ( m_feSettings->get_present_state() == FeSettings::ScreenSaver_Showing )
	{
		// Reset from screen saver
		//
		load_layout();
		return true;
	}

	m_lastInput=m_layout_time.getElapsedTime();
	return false;
}

// First press, repeat in main
bool FePresent::handle_event( FeInputMap::Command c )
{
	if ( reset_screen_saver() )
		return true;

	switch( c )
	{
	case FeInputMap::NextGame:
		change_selection( 1, false );
		break;

	case FeInputMap::PrevGame:
		change_selection( -1, false );
		break;

	case FeInputMap::NextPage:
		change_selection( get_page_size(), false );
		break;

	case FeInputMap::PrevPage:
		change_selection( -get_page_size(), false );
		break;

	case FeInputMap::RandomGame:
		{
			int ls = m_feSettings->get_filter_size( m_feSettings->get_current_filter_index() );
			if ( ls > 1 )
			{
				int step = 1 + ( rand() % ( ls / 2 ) );
				int dir = rand() % 2 ? 1 : -1;
				change_selection( step * dir );
			}
		}
		break;

	case FeInputMap::ToggleRotateRight:
		toggle_rotate( FeSettings::RotateRight );
		break;

	case FeInputMap::ToggleFlip:
		toggle_rotate( FeSettings::RotateFlip );
		break;

	case FeInputMap::ToggleRotateLeft:
		toggle_rotate( FeSettings::RotateLeft );
		break;

	case FeInputMap::ToggleMovie:
		toggle_movie();
		break;

	case FeInputMap::NextDisplay:
	case FeInputMap::PrevDisplay:
		if ( m_feSettings->navigate_display( ( c == FeInputMap::NextDisplay ) ? 1 : -1 ) )
			load_layout();
		else
		{
			update_to( ToNewList, true );
			on_transition( ToNewList, 0 );
		}

		break;

	case FeInputMap::NextFilter:
	case FeInputMap::PrevFilter:
		{
			int offset = ( c == FeInputMap::NextFilter ) ? 1 : -1;
			if ( m_feSettings->navigate_filter( offset ) )
				load_layout();
			else
			{
				update_to( ToNewList, false );
				on_transition( ToNewList, offset );
			}
		}
		break;

	case FeInputMap::ToggleLayout:
		m_feSettings->toggle_layout();
		load_layout();
		break;

	case FeInputMap::PrevFavourite:
	case FeInputMap::NextFavourite:
	case FeInputMap::PrevLetter:
	case FeInputMap::NextLetter:
		{
			int step( 0 );
			switch ( c )
			{
				case FeInputMap::PrevFavourite:
					step = m_feSettings->get_prev_fav_offset();
					break;

				case FeInputMap::NextFavourite:
					step = m_feSettings->get_next_fav_offset();
					break;

				case FeInputMap::PrevLetter:
					step = m_feSettings->get_next_letter_offset( -1 );
					break;

				case FeInputMap::NextLetter:
					step = m_feSettings->get_next_letter_offset( 1 );
					break;

				default:
					break;
			}

			if ( step != 0 )
				change_selection( step, false );
		}
		break;

	case FeInputMap::ScreenSaver:
		load_screensaver();
		break;

	case FeInputMap::Intro:
		if ( !load_intro() )
			load_layout();
		break;

	case FeInputMap::LAST_COMMAND:
	default:
		// Not handled by us, return false so calling function knows
		//
		return false;
	}

	return true;
}

void FePresent::update_to( FeTransitionType type, bool reset_display )
{
	std::vector<FeBaseTextureContainer *>::iterator itc;
	std::vector<FeBasePresentable *>::iterator itl;
	std::vector<FeMonitor>::iterator itm;

	switch ( type )
	{
		case ToNewList:
			for ( itc = m_texturePool.begin(); itc != m_texturePool.end(); ++itc )
				(*itc)->on_new_list( m_feSettings, reset_display );

			for ( itm = m_mon.begin(); itm != m_mon.end(); ++itm )
				for ( itl = (*itm).elements.begin(); itl != (*itm).elements.end(); ++itl )
					(*itl)->on_new_list( m_feSettings );
			// Fallthrough intended

		case ToNewSelection:
			for ( itc = m_texturePool.begin(); itc != m_texturePool.end(); ++itc )
				(*itc)->on_new_selection( m_feSettings );

			for ( itm = m_mon.begin(); itm != m_mon.end(); ++itm )
				for ( itl = (*itm).elements.begin(); itl != (*itm).elements.end(); ++itl )
					(*itl)->on_new_selection( m_feSettings );
			break;

		case EndNavigation:
			for ( itc = m_texturePool.begin(); itc != m_texturePool.end(); ++itc )
				(*itc)->on_end_navigation( m_feSettings );
			break;

		default:
			break;
	}
}

void FePresent::redraw_surfaces()
{
	std::vector<FeBaseTextureContainer *>::iterator itc;

	for ( itc=m_texturePool.begin(); itc != m_texturePool.end(); ++itc )
		(*itc)->on_redraw_surfaces();
}

// return false if the into should be cancelled
bool FePresent::load_intro()
{
	clear_layout();
	// m_baseRotation = FeSettings::RotateNone;
	set_transforms();
	m_feSettings->set_present_state( FeSettings::Intro_Showing );

	m_layout_time.reset();
	if ( !on_new_layout() )
		return false;

	update_to( ToNewList, true );
	m_layout_time.start();
	return ( !m_mon[0].elements.empty() );
}

void FePresent::load_screensaver()
{
	on_transition( EndLayout, FromToScreenSaver );
	clear_layout();
	set_transforms();
	m_feSettings->set_present_state( FeSettings::ScreenSaver_Showing );

	//
	// Run the script which actually sets up the screensaver
	//
	m_layout_time.reset();
	on_new_layout();

	//
	// if there is no screen saver script then do a blank screen
	//
	on_transition( StartLayout, FromToNoValue );
	update_to( ToNewList, true );
	m_layout_time.start();
}

void FePresent::load_layout( bool initial_load )
{
	m_layout_loaded = false;

	int var = ( m_feSettings->get_present_state() == FeSettings::ScreenSaver_Showing )
			? FromToScreenSaver : FromToNoValue;

	if ( !initial_load )
		on_transition( EndLayout, FromToNoValue );
	else
		var = FromToFrontend;

	clear_layout();

	set_transforms();
	m_feSettings->set_present_state( FeSettings::Layout_Showing );

	if ( m_feSettings->displays_count() < 1 )
		return;

	//
	// Run the script which actually sets up the layout
	//
	m_layout_time.reset();
	m_layout_time_old = FeTime();
	m_layout_has_content = false;

	on_new_layout();

	// make things usable if the layout is empty
	if ( !m_layout_has_content )
	{
		FeLog() << " - Layout is empty, initializing with the default layout settings" << std::endl;
		init_with_default_layout();
	}

	on_transition( StartLayout, var );
	update_to( ToNewList, true );
	on_transition( ToNewList, FromToNoValue );
	m_layout_time.start();
}

bool FePresent::tick()
{
	FeTime current_time = m_layout_time.getElapsedTime();
	FeTime delta_time = current_time - m_layout_time_old;
	m_layout_time_old = current_time;
	m_frame_time = delta_time.asSeconds() * 1000.0f;

	bool ret_val = false;
	if ( on_tick())
		ret_val = true;

	if ( video_tick() )
		ret_val = true;

	return ret_val;
}

bool FePresent::video_tick()
{
	bool ret_val=false;

	for ( std::vector<FeBaseTextureContainer *>::iterator itm=m_texturePool.begin();
			itm != m_texturePool.end(); ++itm )
	{
		if ( (*itm)->tick( m_feSettings, m_playMovies ) )
			ret_val=true;
	}

	// Check if we need to loop any script sounds that are set to loop
	for ( std::vector<FeSound *>::iterator its=m_sounds.begin();
			its != m_sounds.end(); ++its )
		(*its)->tick();

	for ( std::vector<FeMusic *>::iterator itm=m_musics.begin();
			itm != m_musics.end(); ++itm )
		(*itm)->tick();

	return ret_val;
}

// Used by fe.layout.redraw
void FePresent::redraw()
{
	// Process tick only when Layout is fully loaded
	if ( is_layout_loaded() )
		tick();

	redraw_surfaces();
	submit_render_frame();
	m_window.clear();
	m_window.display();

	m_layout_time.tick();
}

void FePresent::submit_render_frame()
{
	FeRenderFrame frame;
	frame.camera = m_layout_camera;
	frame.viewport_width = m_mon[0].size.x;
	frame.viewport_height = m_mon[0].size.y;
	frame.antialiasing = m_feSettings->get_antialiasing();
	frame.anisotropic = m_feSettings->get_anisotropic();
	frame.frame_number = ++m_render_frame_serial;
	build_render_geometry( frame.images );
	build_render_surface_frames( frame.surfaces );
	frame.image_count = static_cast<unsigned long long>( frame.images.size() );
	m_window.get_gpu_context().submit_frame( std::move( frame ) );
}

bool FePresent::saver_activation_check()
{
	int saver_timeout = m_feSettings->get_screen_saver_timeout();
	bool saver_active = ( m_feSettings->get_present_state() == FeSettings::ScreenSaver_Showing );

	if ( !saver_active && ( saver_timeout > 0 ))
	{
		if ( ( m_layout_time.getElapsedTime() - m_lastInput )
				> fe_seconds( saver_timeout ) )
		{
			load_screensaver();
			return true;
		}
	}

	// Protect against integer overflow of the layout time. Some downstream paths
	// still cast to 32-bit milliseconds, so force a layout reset before that wraps.
	//
	// THis means the layout is forced by AM to reset after about a month of running
	//
	if ( m_layout_time.getElapsedTime().asMilliseconds() > std::numeric_limits<std::int32_t>::max() - 10000 )
		load_layout();

	return false;
}

int FePresent::get_page_size() const
{
	if ( m_user_page_size != -1 )
		return m_user_page_size;
	else if ( m_listBox )
		return m_listBox->getRowCount();
	else
		return 5;
}

void FePresent::set_page_size( int ps )
{
	if ( ps > 0 )
		m_user_page_size = ps;
	else
		m_user_page_size = -1;
}

void FePresent::on_stop_frontend()
{
	set_video_play_state( false );
	on_transition( EndLayout, FromToFrontend );
}

void FePresent::pre_run()
{
	on_transition( ToGame, FromToNoValue );
	set_video_play_state( false );

	for ( std::vector<FeSound *>::iterator its=m_sounds.begin();
				its != m_sounds.end(); ++its )
	{
		(*its)->set_playing( false );
		(*its)->release_audio( true );
	}

	for ( std::vector<FeMusic *>::iterator its=m_musics.begin();
				its != m_musics.end(); ++its )
	{
		(*its)->set_playing( false );
		(*its)->release_audio( true );
	}
}

void FePresent::post_run()
{
	std::vector<FeSound *>::iterator its;

	for ( std::vector<FeBaseTextureContainer *>::iterator itm=m_texturePool.begin();
				itm != m_texturePool.end(); ++itm )
		(*itm)->release_audio( false );

	for ( std::vector<FeMusic *>::iterator itm=m_musics.begin();
				itm != m_musics.end(); ++itm )
		(*itm)->release_audio( false );

	for ( its=m_sounds.begin(); its != m_sounds.end(); ++its )
		(*its)->release_audio( false );

	set_video_play_state( m_playMovies );

	m_layout_time.tick();
	m_layout_time_old = m_layout_time.getElapsedTime();

	if ( !fe_is_sdl_backend_kmsdrm() )
		on_transition( FromGame, FromToNoValue );

	m_feSettings->reset_input();
	reset_screen_saver();
	update_to( ToNewList, false );
}

void FePresent::toggle_movie()
{
	m_playMovies = !m_playMovies;
	set_video_play_state( m_playMovies );
}

void FePresent::set_video_play_state( bool state )
{
	for ( std::vector<FeBaseTextureContainer *>::iterator itm=m_texturePool.begin();
				itm != m_texturePool.end(); ++itm )
		(*itm)->set_play_state( state );
}

void FePresent::set_audio_loudness( bool enabled )
{
	for ( FeMusic *music : m_musics )
		if ( music )
			if ( auto normaliser = music->get_audio_effects().get_normaliser() )
				normaliser->set_enabled( enabled );

	for ( FeBaseTextureContainer *container : m_texturePool )
		if ( auto tex = dynamic_cast<FeTextureContainer *>(container) )
			if ( auto normaliser = tex->get_audio_effects().get_normaliser() )
				normaliser->set_enabled( enabled );
}

const FeTransform &FePresent::get_transform() const
{
	return m_layout_transform;
}

const FeTransform &FePresent::get_ui_transform() const
{
	return m_ui_transform;
}

const FeFont *FePresent::get_layout_font()
{
	if ( !m_layoutFont )
		m_layoutFont = get_default_font_container();

	return &(m_layoutFont->get_font());
}

const FeFont *FePresent::get_default_font()
{
	return &(get_default_font_container()->get_font());
}

const FeFontContainer *FePresent::get_default_font_container()
{
	if ( !m_defaultFont )
	{
		m_defaultFont = new FeFontContainer();
		m_defaultFont->load_default_font();
	}
	return m_defaultFont;
}

const SDL_Surface *FePresent::get_logo_image()
{
	if ( !m_logo_image )
		m_logo_image = load_embedded_png_surface( _binary_resources_images_Logo_png );

	return m_logo_image;
}

const SDL_Surface *FePresent::get_logo_full_image()
{
	if ( !m_logo_full_image )
		m_logo_full_image = load_embedded_png_surface( _binary_resources_images_Logo_Full_White_png );

	return m_logo_full_image;
}

void FePresent::toggle_rotate( FeSettings::RotationState r )
{
	if ( m_toggleRotation != FeSettings::RotateNone )
		m_toggleRotation = FeSettings::RotateNone;
	else
		m_toggleRotation = r;

	load_layout();
}

FeSettings::RotationState FePresent::get_actual_rotation()
{
	return (FeSettings::RotationState)(( m_baseRotation + m_toggleRotation ) % 4 );
}

void FePresent::set_transforms()
{
	m_layout_transform = m_mon[0].transform;
	m_layout_camera.update_projection(
		static_cast<float>( m_mon[0].size.x ),
		static_cast<float>( m_mon[0].size.y ) );

	FeSettings::RotationState actualRotation = get_actual_rotation();

	if ( m_preserve_aspect )
	{
		switch ( actualRotation )
		{
			case FeSettings::RotateNone:
			{
				m_layoutScale.x = m_layoutScale.y = std::min( (float) m_mon[0].size.x / m_layoutSize.x, (float) m_mon[0].size.y / m_layoutSize.y );
				float adjust_x = std::floor(( m_layoutSize.x * m_layoutScale.x - m_mon[0].size.x ) / 2.0 + 0.5 );
				float adjust_y = std::floor(( m_layoutSize.y * m_layoutScale.y - m_mon[0].size.y ) / 2.0 + 0.5 );
				m_layout_transform.translate({ -adjust_x, -adjust_y });
				break;
			}

			case FeSettings::RotateRight:
			{
				m_layoutScale.x = m_layoutScale.y = std::min( (float) m_mon[0].size.y / m_layoutSize.x,	(float) m_mon[0].size.x / m_layoutSize.y );
				float adjust_x = std::floor( std::abs( m_layoutSize.y * m_layoutScale.x - m_mon[0].size.x ) / 2 + 0.5 );
				float adjust_y = std::floor( std::abs( m_layoutSize.x * m_layoutScale.y - m_mon[0].size.y ) / 2 + 0.5 );
				m_layout_transform.translate({ m_mon[0].size.x - adjust_x, adjust_y });
				m_layout_transform.rotate( 90.0f );
				break;
			}

			case FeSettings::RotateLeft:
			{
				m_layoutScale.x = m_layoutScale.y = std::min( (float) m_mon[0].size.y / m_layoutSize.x,	(float) m_mon[0].size.x / m_layoutSize.y );
				float adjust_x = std::floor( std::fabs( m_layoutSize.y * m_layoutScale.x - m_mon[0].size.x ) / 2 + 0.5 );
				float adjust_y = std::floor( std::fabs( m_layoutSize.x * m_layoutScale.y - m_mon[0].size.y ) / 2 + 0.5 );
				m_layout_transform.translate({ adjust_x, m_mon[0].size.y - adjust_y });
				m_layout_transform.rotate( 270.0f );
				break;
			}

			case FeSettings::RotateFlip:
			{
				m_layoutScale.x = m_layoutScale.y = std::min( (float) m_mon[0].size.x / m_layoutSize.x, (float) m_mon[0].size.y / m_layoutSize.y );
				float adjust_x = std::floor(( m_layoutSize.x * m_layoutScale.x - m_mon[0].size.x ) / 2.0 + 0.5 );
				float adjust_y = std::floor(( m_layoutSize.y * m_layoutScale.y - m_mon[0].size.y ) / 2.0 + 0.5 );
				m_layout_transform.translate({ m_mon[0].size.x + adjust_x, m_mon[0].size.y + adjust_y });
				m_layout_transform.rotate( 180.0f );
				break;
			}
		}
	}
	else // !m_preserve_aspect
	{
		switch ( actualRotation )
		{
			case FeSettings::RotateNone:
				m_layoutScale.x = (float) m_mon[0].size.x / m_layoutSize.x;
				m_layoutScale.y = (float) m_mon[0].size.y / m_layoutSize.y;
				break;

			case FeSettings::RotateRight:
				m_layout_transform.translate({ static_cast<float>( m_mon[0].size.x ), 0 });
				m_layoutScale.x = (float) m_mon[0].size.y / m_layoutSize.x;
				m_layoutScale.y = (float) m_mon[0].size.x / m_layoutSize.y;
				m_layout_transform.rotate( 90.0f );
				break;

			case FeSettings::RotateLeft:
				m_layout_transform.translate({ 0, static_cast<float>( m_mon[0].size.y )});
				m_layoutScale.x = (float) m_mon[0].size.y / m_layoutSize.x;
				m_layoutScale.y = (float) m_mon[0].size.x / m_layoutSize.y;
				m_layout_transform.rotate( 270.0f );
				break;

			case FeSettings::RotateFlip:
				m_layout_transform.translate({ static_cast<float>( m_mon[0].size.x ), static_cast<float>( m_mon[0].size.y )});
				m_layoutScale.x = (float) m_mon[0].size.x / m_layoutSize.x;
				m_layoutScale.y = (float) m_mon[0].size.y / m_layoutSize.y;
				m_layout_transform.rotate( 180.0f );
				break;
		}
	}

	m_layout_transform.scale( { m_layoutScale.x, m_layoutScale.y } );

	for ( std::vector<FeBasePresentable *>::iterator itr=m_mon[0].elements.begin();
			itr!=m_mon[0].elements.end(); ++itr )
		(*itr)->set_scale_factor( m_layoutScale.x, m_layoutScale.y );


	// UI transform
	m_ui_transform = m_mon[0].transform;

	switch ( actualRotation )
	{
		case FeSettings::RotateNone:
			break;

		case FeSettings::RotateRight:
			m_ui_transform.translate({ static_cast<float>( m_mon[0].size.x ), 0 });
			m_ui_transform.rotate( 90.0f );
			break;

		case FeSettings::RotateLeft:
			m_ui_transform.translate({ 0, static_cast<float>( m_mon[0].size.y )});
			m_ui_transform.rotate( 270.0f );
			break;

		case FeSettings::RotateFlip:
			m_ui_transform.translate({ static_cast<float>( m_mon[0].size.x ), static_cast<float>( m_mon[0].size.y )});
			m_ui_transform.rotate( 180.0f );
			break;
	}
}

FeShader *FePresent::get_empty_shader()
{
	if ( !m_emptyShader )
		m_emptyShader = new FeShader();

	return m_emptyShader;
}

void FePresent::set_search_rule( const char *s )
{
	m_feSettings->set_search_rule( s );
	update_to( ToNewList, true );
	on_transition( ToNewList, 0 );
}

const char *FePresent::get_search_rule()
{
	return m_feSettings->get_search_rule().c_str();
}

void FePresent::set_preserve_aspect_ratio( bool p )
{
	if ( p != m_preserve_aspect )
	{
		m_preserve_aspect = p;
		set_transforms();
		flag_redraw();
	}
}

bool FePresent::get_preserve_aspect_ratio()
{
	return m_preserve_aspect;
}

bool FePresent::get_overlay_custom_controls( FeText *&t, FeListBox *&lb )
{
	t = m_overlay_caption;
	lb = m_overlay_lb;

	return m_custom_overlay;
}

int FePresent::get_layout_ms()
{
	return m_layout_time.getElapsedTime().asMilliseconds();
}

FeTime FePresent::get_layout_time()
{
	return m_layout_time.getElapsedTime();
}

float FePresent::get_layout_frame_time()
{
	return m_frame_time;
}

int FePresent::get_refresh_rate()
{
	return m_refresh_rate;
}

void FePresent::set_mouse_pointer( bool b )
{
	m_mouse_pointer_visible = b;

	if ( is_windowed_mode( m_window.get_window_mode() ))
		m_window.set_mouse_cursor_visible( true );
	else
		m_window.set_mouse_cursor_visible( b );
}

bool FePresent::get_mouse_pointer()
{
	return m_mouse_pointer_visible;
}

void FePresent::script_do_update( FeBasePresentable *bp )
{
	FePresent *fep = script_get_fep();
	if ( fep )
	{
		bp->on_new_list( fep->m_feSettings );
		bp->on_new_selection( fep->m_feSettings );
		fep->flag_redraw();
	}
}

// Note: do_update forces artwork to reload
void FePresent::script_do_update( FeBaseTextureContainer *tc, bool do_update )
{
	FePresent *fep = script_get_fep();
	if ( fep )
	{
		tc->on_new_list( fep->m_feSettings, do_update );
		tc->on_new_selection( fep->m_feSettings );
		fep->flag_redraw();
	}
}

void FePresent::script_register_texture_container( FeBaseTextureContainer *tc )
{
	FePresent *fep = script_get_fep();
	if ( !fep || !tc )
		return;

	if ( std::find( fep->m_texturePool.begin(), fep->m_texturePool.end(), tc ) == fep->m_texturePool.end() )
		fep->m_texturePool.push_back( tc );
}

void FePresent::script_unregister_texture_container( FeBaseTextureContainer *tc )
{
	FePresent *fep = script_get_fep();
	if ( !fep || !tc )
		return;

	std::vector<FeBaseTextureContainer *>::iterator it =
		std::find( fep->m_texturePool.begin(), fep->m_texturePool.end(), tc );
	if ( it != fep->m_texturePool.end() )
		fep->m_texturePool.erase( it );
}

void FePresent::script_flag_redraw()
{
	FePresent *fep = script_get_fep();
	if ( fep )
		fep->flag_redraw();
}

void FePresent::script_flag_sort_zorder()
{
	FePresent *fep = script_get_fep();
	if ( fep )
		fep->flag_sort_zorder();
}

std::string FePresent::script_get_base_path()
{
	std::string path;

	FePresent *fep = script_get_fep();
	if ( fep )
	{
		FeSettings *fes = fep->get_fes();
		if ( fes )
		{
			int script_id = fep->get_script_id();
			if ( script_id < 0 )
				fes->get_path( FeSettings::Current, path );
			else
				fes->get_plugin_full_path(
					script_id, path );
		}
	}

	return path;
}

const Vec2i FePresent::get_screen_size()
{
	if ( get_actual_rotation() == FeSettings::RotateLeft || get_actual_rotation() == FeSettings::RotateRight )
		return Vec2i( m_mon[0].size.y, m_mon[0].size.x );
	else
		return m_mon[0].size;
}

FeStableClock::FeStableClock()
	: m_time()
{
	m_real_timer.stop();
	m_real_timer.reset();
}

void FeStableClock::start()
{
	m_real_timer.start();
}

void FeStableClock::reset()
{
	m_real_timer.reset();
	m_time = FeTime();
}

void FeStableClock::tick()
{
	if ( !m_real_timer.isRunning() )
		m_real_timer.start();

	FeTime real_elapsed = m_real_timer.getElapsedTime();
	FePresent *fep = FePresent::script_get_fep();
	FeTime stable_increment = fe_microseconds( 1000000 / fep->get_refresh_rate() );

	FeTime new_time = m_time + stable_increment;

	// If the new time is lagging behind the real time, catch up.
	if ( new_time < real_elapsed )
		new_time = real_elapsed;

	m_time = new_time;
}

FeTime FeStableClock::getElapsedTime()
{
	if ( !m_real_timer.isRunning() )
		m_real_timer.start();

	FeTime real_elapsed = m_real_timer.getElapsedTime();

	// If the new time is lagging behind the real time, catch up.
	if ( m_time < real_elapsed )
		m_time = real_elapsed;

	return m_time;
}
