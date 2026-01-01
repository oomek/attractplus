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

#include "fe_image.hpp"
#include "fe_util.hpp"
#include "fe_settings.hpp"
#include "fe_shader.hpp"
#include "fe_present.hpp"
#include "fe_blend.hpp"
#include "fe_vm.hpp"
#include "fe_audio_fx.hpp"
#include "zip.hpp"
#include "image_loader.hpp"

#include <algorithm>
#include <cmath>

#ifndef NO_MOVIE
#include "media.hpp"
#endif

FeBaseTextureContainer::FeBaseTextureContainer()
{
}

FeBaseTextureContainer::~FeBaseTextureContainer()
{
}

bool FeBaseTextureContainer::get_visible() const
{
	return true;
}

void FeBaseTextureContainer::set_play_state( bool play )
{
}

bool FeBaseTextureContainer::get_play_state() const
{
	return false;
}

void FeBaseTextureContainer::set_index_offset( int io, bool do_update )
{
}

int FeBaseTextureContainer::get_index_offset() const
{
	return 0;
}

void FeBaseTextureContainer::set_filter_offset( int fo, bool do_update )
{
}

int FeBaseTextureContainer::get_filter_offset() const
{
	return 0;
}

void FeBaseTextureContainer::set_video_flags( FeVideoFlags f )
{
}

FeVideoFlags FeBaseTextureContainer::get_video_flags() const
{
	return VF_Normal;
}

int FeBaseTextureContainer::get_video_duration() const
{
	return 0;
}

int FeBaseTextureContainer::get_video_time() const
{
	return 0;
}

void FeBaseTextureContainer::load_file( const char *n )
{
}

const char *FeBaseTextureContainer::get_file_name() const
{
	return NULL;
}

void FeBaseTextureContainer::set_trigger( int t )
{
}

int FeBaseTextureContainer::get_trigger() const
{
	return ToNewSelection;
}

void FeBaseTextureContainer::set_mipmap( bool m )
{
}

bool FeBaseTextureContainer::get_mipmap() const
{
	return false;
}

void FeBaseTextureContainer::set_clear( bool c )
{
}

bool FeBaseTextureContainer::get_clear() const
{
	return false;
}

void FeBaseTextureContainer::set_repeat( bool r )
{
}

bool FeBaseTextureContainer::get_repeat() const
{
	return false;
}

void FeBaseTextureContainer::set_redraw( bool r )
{
}

bool FeBaseTextureContainer::get_redraw() const
{
	return false;
}

void FeBaseTextureContainer::set_volume( float v )
{
}

float FeBaseTextureContainer::get_volume() const
{
	return 0.0;
}

void FeBaseTextureContainer::set_pan( float p )
{
}

float FeBaseTextureContainer::get_pan() const
{
	return 0.0;
}

float FeBaseTextureContainer::get_sample_aspect_ratio() const
{
	return 1.0;
}

void FeBaseTextureContainer::transition_swap( FeBaseTextureContainer *o )
{
	//
	// Swap image lists
	//
	m_images.swap( o->m_images );

	//
	// Now update the images to point at their new parent textures.
	// texture_changed() will also cause them to update their sf::Sprite
	// accordingly
	//
	std::vector< FeImage * >::iterator itr;

	for ( itr = m_images.begin(); itr != m_images.end(); ++itr )
		(*itr)->texture_changed( this );

	for ( itr = o->m_images.begin(); itr != o->m_images.end(); ++itr )
		(*itr)->texture_changed( o );
}

bool FeBaseTextureContainer::fix_masked_image()
{
	return false;
}

bool FeBaseTextureContainer::tick( FeSettings *feSettings, bool play_movies )
{
	return false;
}

FeTextureContainer *FeBaseTextureContainer::get_derived_texture_container()
{
	return NULL;
}

FePresentableParent *FeBaseTextureContainer::get_presentable_parent()
{
	return NULL;
}

void FeBaseTextureContainer::register_image( FeImage *img )
{
	m_images.push_back( img );
}

void FeBaseTextureContainer::notify_texture_change()
{
	for ( std::vector<FeImage *>::iterator itr=m_images.begin();
			itr != m_images.end(); ++itr )
		(*itr)->texture_changed();
}

void FeBaseTextureContainer::release_audio( bool )
{
}

void FeBaseTextureContainer::on_redraw_surfaces()
{
}

namespace
{
	//
	// The number of "ticks" after a video is first loaded before
	// playing starts.  This should be 3 or higher (freezing has
	// been experienced at 2 when returning from games).
	//
	const int PLAY_COUNT=5;
};

FeTextureContainer::FeTextureContainer(
	bool is_artwork,
	const std::string &art_name )
	: m_index_offset( 0 ),
	m_filter_offset( 0 ),
	m_current_rom_index( -1 ),
	m_current_filter_index( -1 ),
	m_art_update_trigger( ToNewSelection ),
	m_movie( NULL ),
	m_movie_status( -1 ),
	m_video_flags( VF_Normal ),
	m_mipmap( false ),
	m_smooth( false ),
	m_volume( 100.0 ),
	m_pan( 0.0 ),
	m_fft_bands( 32 ),
	m_entry( NULL )
{
#ifndef NO_MOVIE
	m_audio_effects.add_effect( std::make_unique<FeAudioDCFilter>() );
	m_audio_effects.add_effect( std::make_unique<FeAudioNormaliser>() );
	m_audio_effects.add_effect( std::make_unique<FeAudioVisualiser>() );

	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
	{
		auto* normaliser = m_audio_effects.get_effect<FeAudioNormaliser>();
		if ( normaliser )
			normaliser->set_media_volume( fep->get_fes()->get_play_volume( FeSoundInfo::Movie ) / 100.0f );
	}

	m_audio_effects.set_ready_for_processing();
#endif

	if ( is_artwork )
	{
		m_type = IsArtwork;
		m_art_name = art_name.empty() ? FE_DEFAULT_ARTWORK : art_name;
	}
	else if ( art_name.find_first_of( "[" ) != std::string::npos )
	{
		m_type = IsDynamic;
		m_art_name = clean_path( art_name );
	}
	else
		m_type = IsStatic;
}

FeTextureContainer::~FeTextureContainer()
{
#ifndef NO_MOVIE
	if ( m_movie )
	{
		delete m_movie;
		m_movie=NULL;
	}
#endif

	if ( m_entry )
	{
		FeImageLoader &il = FeImageLoader::get_ref();
		il.release_entry( &m_entry );
	}
}

bool FeTextureContainer::get_visible() const
{
	if ( m_entry )
		return false;

	return true;
}

bool FeTextureContainer::fix_masked_image()
{
	bool retval=false;

	sf::Image tmp_img = m_texture.copyToImage();
	sf::Vector2u tmp_s = tmp_img.getSize();

	if (( tmp_s.x > 0 ) && ( tmp_s.y > 0 ))
	{
		sf::Color p = tmp_img.getPixel({ 0, 0 });
		tmp_img.createMaskFromColor( p );

		if ( m_texture.loadFromImage( tmp_img ) )
			retval=true;

		notify_texture_change();
	}

	return retval;
}

struct RGBAPixel { std::uint8_t r,g,b,a; };
static std::vector<RGBAPixel> s_black_pixels;

#ifndef NO_MOVIE
bool FeTextureContainer::load_with_ffmpeg(
	const std::string &filename,
	bool is_image )
{
	std::string loaded_name;
	bool res=false;

	loaded_name = filename;
	if ( loaded_name.compare( m_file_name ) == 0 )
		return true;

	clear();

	if ( !file_exists( loaded_name ) )
	{
		m_texture = sf::Texture();
		return false;
	}

	m_movie = new FeMedia( FeMedia::AudioVideo, m_audio_effects );
	res = m_movie->open( "", loaded_name, &m_texture );

	if ( !res )
	{
		FeLog() << "ERROR loading video: "
			<< loaded_name << std::endl;

		m_texture = sf::Texture();
		delete m_movie;
		m_movie = NULL;
		return false;
	}

	if ( m_movie )
		m_movie->set_fft_bands( m_fft_bands );

	if ( is_image && (!m_movie->is_multiframe()) )
	{
		m_movie_status = -1; // don't play if there is only one frame

		// if there is only one frame, then we can update the texture immediately
		// (the frame will have been preloaded) and delete our movie object now
		delete m_movie;
		m_movie = NULL;
	}
	else if (m_video_flags & VF_NoAutoStart)
		m_movie_status = 0; // 0=loaded but not on track to play
	else
		m_movie_status = 1; // 1=on track to be played

	if ( res && !is_image )
	{
		// Fill the first video frame with an opaque black colour
		size_t required_pixels = m_texture.getSize().x * m_texture.getSize().y;
		if ( s_black_pixels.size() < required_pixels )
    		s_black_pixels.resize( required_pixels, { 0, 0, 0, 255 });
		m_texture.update( reinterpret_cast<std::uint8_t*>( s_black_pixels.data() ));
	}

	m_texture.setSmooth( m_smooth );
	m_file_name = loaded_name;

	return true;
}
#endif

bool FeTextureContainer::try_to_load(
	const std::string &filename,
	bool is_image )
{
	std::string loaded_name;

#ifndef NO_MOVIE
	if ( !is_image && FeMedia::is_supported_media_file( filename ) )
		return load_with_ffmpeg( filename, false );
#endif

	FeImageLoader &il = FeImageLoader::get_ref();
	unsigned char *data = NULL;

	loaded_name = filename;
	if ( loaded_name.compare( m_file_name ) == 0 )
		return true;

	clear();

	if ( !file_exists( loaded_name ) )
	{
		m_texture = sf::Texture();
		return false;
	}

	if ( il.load_image_from_file( loaded_name, &m_entry ) )
		data = m_entry->get_data();

	m_file_name = loaded_name;

	// resize our texture accordingly
	if ( m_texture.getSize() != sf::Vector2u( m_entry->get_width(), m_entry->get_height() ))
		std::ignore = m_texture.resize({ static_cast<unsigned int>( m_entry->get_width() ), static_cast<unsigned int>( m_entry->get_height() )});

	if ( data )
	{
		m_texture.update( data );
		il.release_entry( &m_entry ); // don't need entry any more
		if ( m_mipmap ) std::ignore = m_texture.generateMipmap();
		m_texture.setSmooth( m_smooth );
	}

	return true;
}

const sf::Texture &FeTextureContainer::get_texture()
{
	return m_texture;
}

void FeTextureContainer::on_new_selection( FeSettings *feSettings )
{
	if (( m_type != IsStatic ) && ( m_art_update_trigger == ToNewSelection ))
		internal_update_selection( feSettings );
}

void FeTextureContainer::on_new_list( FeSettings *feSettings, bool new_display )
{
	if ( new_display )
		m_current_filter_index=-1;

	if (( m_type != IsStatic ) && ( m_art_update_trigger == EndNavigation ))
		internal_update_selection( feSettings );
}

void FeTextureContainer::on_end_navigation( FeSettings *feSettings )
{
	if (( m_type != IsStatic ) && ( m_art_update_trigger == EndNavigation ))
		internal_update_selection( feSettings );
}

void FeTextureContainer::internal_update_selection( FeSettings *feSettings )
{
	int filter_index = feSettings->get_filter_index_from_offset( m_filter_offset );
	int rom_index = feSettings->get_rom_index( filter_index, m_index_offset );

	//
	// Optimization opportunity: We could already be showing the artwork for rom_index if the
	// layout uses the image swap() function... if we are, then there is no need to do anything...
	//
	if (( m_current_rom_index == rom_index )
				&& ( m_current_filter_index == filter_index ))
	{
#ifdef FE_DEBUG
		FeDebug() << "Texture internal update optimization: " << m_file_name
			<< " not reloaded." << std::endl;
#endif
		return;
	}

	m_current_rom_index = rom_index;
	m_current_filter_index = filter_index;

	std::vector<std::string> vid_list;
	std::vector<std::string> image_list;

	if ( m_type == IsArtwork )
	{
		FeRomInfo *rom	= feSettings->get_rom_absolute( filter_index, rom_index );
		if ( !rom )
			return;

		feSettings->get_best_artwork_file( *rom,
			m_art_name,
			vid_list,
			image_list,
			(m_video_flags & VF_DisableVideo) );
	}
	else if ( m_type == IsDynamic )
	{
		std::string work = m_art_name;
		FePresent::script_process_magic_strings( work,
				m_filter_offset,
				m_index_offset );

		feSettings->get_best_dynamic_image_file( filter_index,
			rom_index,
			work,
			vid_list,
			image_list );
	}

	// Load any found videos/images now
	//
	bool loaded=false;
	std::vector<std::string>::iterator itr;

#ifndef NO_MOVIE
	if ( m_video_flags & VF_DisableVideo )
		vid_list.clear();

	for ( itr=vid_list.begin(); itr != vid_list.end(); ++itr )
	{
		std::string filename = *itr;

		if ( try_to_load( filename ) )
		{
			loaded = true;
			break;
		}
	}
#endif

	if ( !loaded )
	{
		if ( image_list.empty() )
		{
			clear();
			m_texture = sf::Texture();
		}
		else
		{
			for ( itr=image_list.begin();
				itr != image_list.end(); ++itr )
			{
				std::string filename = *itr;

				if ( try_to_load( filename, true ) )
				{
					loaded = true;
					break;
				}
			}
		}
	}
	//
	// Texture was replaced, so notify the attached images
	//
	notify_texture_change();
}

bool FeTextureContainer::tick( FeSettings *feSettings, bool play_movies )
{
	//
	// We have an m_entry if the image is being loaded in the background
	//
	if ( m_entry )
	{
		FeImageLoader &il = FeImageLoader::get_ref();
		if ( il.check_loaded( m_entry ) )
		{
			m_texture.update( m_entry->get_data() );
			if ( m_mipmap ) std::ignore = m_texture.generateMipmap();
			m_texture.setSmooth( m_smooth );

			il.release_entry( &m_entry );
			return true;
		}
	}

	m_audio_effects.update_all();

	if ( !play_movies || (m_video_flags & VF_DisableVideo) )
		return false;

#ifndef NO_MOVIE
	if ( m_movie )
	{
		// VF_NoAudio flag overrides volume setting
		float vol = m_video_flags & VF_NoAudio
			? 0.f
			: m_volume * feSettings->get_play_volume( FeSoundInfo::Movie ) / 100.0;

		if ( vol != m_movie->getVolume() )
			m_movie->setVolume( vol );

		if ( m_pan != m_movie->getPan() )
			m_movie->setPan( m_pan );

		if ( m_movie_status > 0 )
		{
			if ( m_movie_status < PLAY_COUNT )
			{
				//
				// We skip the first few "ticks" after the movie
				// is first loaded because the user may just be
				// scrolling rapidly through the game list (there
				// are ticks between each selection scrolling by)
				//
				m_movie_status++;
				return false;
			}
			else if ( m_movie_status == PLAY_COUNT )
			{
				m_movie_status++;
				m_movie->play();
			}

			// restart looped video
			if ( !(m_video_flags & VF_NoLoop) && !m_movie->is_playing() )
			{
				m_movie->stop();
				m_movie->play();

				FeDebug() << "Restarted looped video" << std::endl;
			}
		}

		if ( m_movie->tick() )
		{
			if ( m_mipmap ) std::ignore = m_texture.generateMipmap();
			return true;
		}
	}
#endif

	return false;
}

void FeTextureContainer::set_play_state( bool play )
{
#ifndef NO_MOVIE
	if (m_movie)
	{
		if ( play == get_play_state() )
			return;

		if ( m_movie_status > PLAY_COUNT )
		{
			if ( play )
			{
				m_movie->stop();
				m_movie->play();
			}
			else
			{
				m_movie->stop();
				m_movie_status = 0;
			}
		}
		else if ( m_movie_status >= 0 )
		{
			// m_movie_status is 0 if a movie is loaded but the VF_NoAutoStart flag is set.
			// If movie is in this state and user wants to play then put it on track to be played.
			//
			if (( m_movie_status == 0 ) && ( play ))
				m_movie_status = 1;
			else if ( !play )
				m_movie_status = 0;
		}
	}
#endif
}

bool FeTextureContainer::get_play_state() const
{
#ifndef NO_MOVIE
	if ( m_movie )
	{
		if ( m_movie_status > PLAY_COUNT )
			return m_movie->is_playing();
		else
			// if status > 0, we are in the process of starting to play
			return ( m_movie_status > 0 );
	}
#endif

	return false;
}

void FeTextureContainer::set_index_offset( int io, bool do_update )
{
	if ( m_index_offset != io || m_file_name.empty() )
	{
		m_index_offset = io;

		if ( do_update )
			FePresent::script_do_update( this, true );
	}
}

int FeTextureContainer::get_index_offset() const
{
	return m_index_offset;
}

void FeTextureContainer::set_filter_offset( int fo, bool do_update )
{
	if ( m_filter_offset != fo || m_file_name.empty() )
	{
		m_filter_offset = fo;

		if ( do_update )
			FePresent::script_do_update( this, true );
	}
}

int FeTextureContainer::get_filter_offset() const
{
	return m_filter_offset;
}

void FeTextureContainer::set_video_flags( FeVideoFlags f )
{
	m_video_flags = f;

#ifndef NO_MOVIE
	if ( m_movie )
	{
		if ( m_movie_status > 0 && m_video_flags & VF_NoAutoStart )
			m_movie_status = 0;
	}
#endif
}

FeVideoFlags FeTextureContainer::get_video_flags() const
{
	return m_video_flags;
}

int FeTextureContainer::get_video_duration() const
{
#ifndef NO_MOVIE
	if ( m_movie )
		return m_movie->get_duration().asMilliseconds();
#endif

	return 0;
}

int FeTextureContainer::get_video_time() const
{
#ifndef NO_MOVIE
	if ( m_movie )
		return m_movie->get_video_time().asMilliseconds();
#endif

	return 0;
}

void FeTextureContainer::load_file( const char *n )
{
	std::string filename = clean_path( n );

	if ( filename.empty() )
	{
		clear();
		m_texture = sf::Texture();
		notify_texture_change();
		return;
	}

	if ( is_relative_path( filename ) )
		filename = FePresent::script_get_base_path() + filename;

	bool is_image=tail_compare( filename, FE_ART_EXTENSIONS );
	try_to_load( filename, is_image );
	notify_texture_change();
}

const char *FeTextureContainer::get_file_name() const
{
	return m_file_name.c_str();
}

void FeTextureContainer::set_trigger( int t )
{
	m_art_update_trigger = t;
}

int FeTextureContainer::get_trigger() const
{
	return m_art_update_trigger;
}


void FeTextureContainer::transition_swap( FeBaseTextureContainer *o )
{
	FeTextureContainer *o_up = o->get_derived_texture_container();
	if ( o_up )
	{
		m_art_name.swap( o_up->m_art_name );
		std::swap( m_index_offset, o_up->m_index_offset );
		std::swap( m_filter_offset, o_up->m_filter_offset );
		std::swap( m_type, o_up->m_type );
		std::swap( m_art_update_trigger, o_up->m_art_update_trigger );
	}

	FeBaseTextureContainer::transition_swap( o );
}

FeTextureContainer *FeTextureContainer::get_derived_texture_container()
{
	return this;
}

void FeTextureContainer::clear()
{
	m_movie_status = -1;
	m_file_name.clear();

#ifndef NO_MOVIE
	// If a movie is running, close it...
	if ( m_movie )
	{
		FeImageLoader &il = FeImageLoader::get_ref();
		il.reap_video( m_movie );
		m_movie=NULL;
	}
#endif

	if ( m_entry )
	{
		FeImageLoader &il = FeImageLoader::get_ref();
		il.release_entry( &m_entry );
	}
}

void FeTextureContainer::set_smooth( bool s )
{
	m_smooth = s;
	m_texture.setSmooth( s );
}

bool FeTextureContainer::get_smooth() const
{
	return m_smooth;
}

void FeTextureContainer::set_mipmap( bool m )
{
	m_mipmap = m;
	if ( m_mipmap && !m_movie ) std::ignore = m_texture.generateMipmap();
}

bool FeTextureContainer::get_mipmap() const
{
	return m_mipmap;
}

void FeTextureContainer::set_repeat( bool r )
{
	m_texture.setRepeated( r );
}

bool FeTextureContainer::get_repeat() const
{
	return m_texture.isRepeated();
}

void FeTextureContainer::set_volume( float v )
{
	if ( v == m_volume ) return;

	if ( v < 0.0 ) v = 0.0;
	if ( v > 100.0 ) v = 100.0;
	m_volume = v;
}

float FeTextureContainer::get_volume() const
{
	return m_volume;
}

void FeTextureContainer::set_pan( float p )
{
	if ( p == m_pan ) return;
	m_pan = std::clamp( p, -1.0f, 1.0f );
}

float FeTextureContainer::get_pan() const
{
	return m_pan;
}

void FeTextureContainer::set_fft_bands( int count )
{
	m_fft_bands = count;

	if ( m_movie )
		m_movie->set_fft_bands( count );
}

int FeTextureContainer::get_fft_bands() const
{
	if ( m_movie )
		return m_movie->get_fft_bands();
	else
		return m_fft_bands;
}

float FeTextureContainer::get_sample_aspect_ratio() const
{
#ifndef NO_MOVIE
	if ( m_movie )
		return m_movie->get_aspect_ratio();
#endif
		return 1.0;
}

FeMedia *FeTextureContainer::get_media() const
{
#ifndef NO_MOVIE
	return m_movie;
#else
	return NULL;
#endif
}

void FeTextureContainer::release_audio( bool state )
{
}

float FeTextureContainer::get_vu_mono() const
{
	auto* visualiser = m_audio_effects.get_effect<FeAudioVisualiser>();
	return visualiser ? visualiser->get_vu_mono() : 0.0f;
}

float FeTextureContainer::get_vu_left() const
{
	auto* visualiser = m_audio_effects.get_effect<FeAudioVisualiser>();
	return visualiser ? visualiser->get_vu_left() : 0.0f;
}

float FeTextureContainer::get_vu_right() const
{
	auto* visualiser = m_audio_effects.get_effect<FeAudioVisualiser>();
	return visualiser ? visualiser->get_vu_right() : 0.0f;
}

const std::vector<float> *FeTextureContainer::get_fft_mono_ptr() const
{
	auto* visualiser = m_audio_effects.get_effect<FeAudioVisualiser>();
	return visualiser ? visualiser->get_fft_mono_ptr() : nullptr;
}

const std::vector<float> *FeTextureContainer::get_fft_left_ptr() const
{
	auto* visualiser = m_audio_effects.get_effect<FeAudioVisualiser>();
	return visualiser ? visualiser->get_fft_left_ptr() : nullptr;
}

const std::vector<float> *FeTextureContainer::get_fft_right_ptr() const
{
	auto* visualiser = m_audio_effects.get_effect<FeAudioVisualiser>();
	return visualiser ? visualiser->get_fft_right_ptr() : nullptr;
}

FeSurfaceTextureContainer::FeSurfaceTextureContainer( int width, int height )
	: m_clear( true ),
	m_redraw( true ),
	m_mipmap( false )
{
	sf::ContextSettings ctx;
	FePresent *fep = FePresent::script_get_fep();
	if ( fep )
	{
		FeSettings *fes = fep->get_fes();
		if ( fes ) ctx.antiAliasingLevel = fes->get_antialiasing();
	}
	if ( m_texture.resize({ static_cast<unsigned int>(width), static_cast<unsigned int>(height) }, ctx ) )
		m_texture.clear( sf::Color::Transparent );
}

FeSurfaceTextureContainer::~FeSurfaceTextureContainer()
{
	while ( !elements.empty() )
	{
		FeBasePresentable *p = elements.back();
		elements.pop_back();
		delete p;
	}
}

const sf::Texture &FeSurfaceTextureContainer::get_texture()
{
	return m_texture.getTexture();
}

void FeSurfaceTextureContainer::on_new_selection( FeSettings *s )
{
	for ( std::vector<FeBasePresentable *>::iterator itr = elements.begin();
				itr != elements.end(); ++itr )
		(*itr)->on_new_selection( s );
}

void FeSurfaceTextureContainer::on_end_navigation( FeSettings *feSettings )
{
}

void FeSurfaceTextureContainer::on_new_list( FeSettings *s, bool )
{
	//
	// We don't do any scaling of the objects when they are being drawn
	// to the surface.
	//
	for ( std::vector<FeBasePresentable *>::iterator itr = elements.begin();
				itr != elements.end(); ++itr )
		(*itr)->on_new_list( s );
}

void FeSurfaceTextureContainer::on_redraw_surfaces()
{
	//
	// Draw the surface's draw list to the render texture
	//
	if ( m_clear ) m_texture.clear( sf::Color::Transparent );
	if ( m_redraw )
	{
		for ( std::vector<FeBasePresentable *>::const_iterator itr = elements.begin();
					itr != elements.end(); ++itr )
		{
			if ( (*itr)->get_visible() )
				m_texture.draw( (*itr)->drawable() );
		}

		m_texture.display();
		if ( m_mipmap ) std::ignore = m_texture.generateMipmap();
	}
}

void FeSurfaceTextureContainer::set_smooth( bool s )
{
	m_texture.setSmooth( s );
}

bool FeSurfaceTextureContainer::get_smooth() const
{
	return m_texture.isSmooth();
}

void FeSurfaceTextureContainer::set_mipmap( bool m )
{
	m_mipmap = m;
}

bool FeSurfaceTextureContainer::get_mipmap() const
{
	return m_mipmap;
}

void FeSurfaceTextureContainer::set_clear( bool c )
{
	m_clear = c;
}

bool FeSurfaceTextureContainer::get_clear() const
{
	return m_clear;
}

void FeSurfaceTextureContainer::set_repeat( bool r )
{
	m_texture.setRepeated( r );
}

bool FeSurfaceTextureContainer::get_repeat() const
{
	return m_texture.isRepeated();
}

void FeSurfaceTextureContainer::set_redraw( bool r )
{
	m_redraw = r;
}

bool FeSurfaceTextureContainer::get_redraw() const
{
	return m_redraw;
}

FePresentableParent *FeSurfaceTextureContainer::get_presentable_parent()
{
	return this;
}

FeImage::FeImage(
	FePresentableParent &p,
	FeBaseTextureContainer *tc,
	float x,
	float y,
	float w,
	float h
):
	FeBasePresentable( p ),
	m_tex( tc ),
	m_pos( x, y ),
	m_size( w, h ),
	m_auto_size( w == 0, h == 0 ),
	m_origin( 0.f, 0.f ),
	m_transform_origin( 0.f, 0.f ),
	m_transform_origin_type( TopLeft ),
	m_anchor( 0.f, 0.f ),
	m_anchor_type( TopLeft ),
	m_rotation( 0.0 ),
	m_rotation_origin( 0.f, 0.f ),
	m_rotation_origin_type( TopLeft ),
	m_crop( true ),
	m_fit( Fill ),
	m_fit_anchor( 0.5f, 0.5f ),
	m_fit_anchor_type( Centre ),
	m_blend_mode( FeBlend::Alpha ),
	m_preserve_aspect_ratio( false ),
	m_force_aspect_ratio( 0.0 ),
	m_fft_data_zero( FeAudioVisualiser::FFT_BANDS_MAX, 0.0f ),
	m_fft_zero_wrapper( &m_fft_data_zero ),
	m_fft_array_wrapper( &m_fft_data_zero )
{
	ASSERT( m_tex );
	m_tex->register_image( this );
	scale();
}

FeImage::FeImage( FeImage *o ):
	FeBasePresentable( *o ),
	m_tex( o->m_tex ),
	m_sprite( o->m_sprite ),
	m_pos( o->m_pos ),
	m_size( o->m_size ),
	m_auto_size( o->m_auto_size ),
	m_origin( o->m_origin ),
	m_transform_origin( o->m_anchor ),
	m_transform_origin_type( o->m_transform_origin_type ),
	m_anchor( o->m_anchor ),
	m_anchor_type( o->m_anchor_type ),
	m_rotation( o->m_rotation ),
	m_rotation_origin( o->m_rotation_origin ),
	m_rotation_origin_type( o->m_rotation_origin_type ),
	m_crop( o->m_crop ),
	m_fit( o->m_fit ),
	m_fit_anchor( o->m_fit_anchor ),
	m_fit_anchor_type( o->m_fit_anchor_type ),
	m_blend_mode( o->m_blend_mode ),
	m_preserve_aspect_ratio( o->m_preserve_aspect_ratio ),
	m_force_aspect_ratio( o->m_force_aspect_ratio ),
	m_fft_data_zero( FeAudioVisualiser::FFT_BANDS_MAX, 0.0f ),
	m_fft_zero_wrapper( &m_fft_data_zero ),
	m_fft_array_wrapper( &m_fft_data_zero )
{
	set_smooth( o->get_smooth() );
	m_tex->register_image( this );
}

FeImage::~FeImage() {}

const sf::Texture *FeImage::get_texture()
{
	if ( m_tex )
		return &(m_tex->get_texture());
	else
		return NULL;
}

bool FeImage::get_visible() const
{
	if ( !FeBasePresentable::get_visible() || !m_tex )
		return false;

	return m_tex->get_visible();

}

void FeImage::texture_changed( FeBaseTextureContainer *new_tex )
{
	if ( new_tex )
		m_tex = new_tex;

	m_sprite.setTexture( m_tex->get_texture() );

	//  reset texture rect now to the one reported by the new texture object
	m_sprite.setTextureRect(
		sf::FloatRect({ 0, 0 }, { static_cast<float>( m_tex->get_texture().getSize().x ), static_cast<float>( m_tex->get_texture().getSize().y )}));

	scale();
}

sf::Vector2f FeImage::alignTypeToVector( int type )
{
	switch( type )
	{
		case Left:
			return sf::Vector2f( 0.0f, 0.5f );

		case Centre:
			return sf::Vector2f( 0.5f, 0.5f );

		case Right:
			return sf::Vector2f( 1.0f, 0.5f );

		case Top:
			return sf::Vector2f( 0.5f, 0.0f );

		case Bottom:
			return sf::Vector2f( 0.5f, 1.0f );

		case TopLeft:
			return sf::Vector2f( 0.0f, 0.0f );

		case TopRight:
			return sf::Vector2f( 1.0f, 0.0f );

		case BottomLeft:
			return sf::Vector2f( 0.0f, 1.0f );

		case BottomRight:
			return sf::Vector2f( 1.0f, 1.0f );

		default:
			return sf::Vector2f( 0.0f, 0.0f );
	}
}

int FeImage::getIndexOffset() const
{
	return m_tex->get_index_offset();
}

void FeImage::setIndexOffset( int io )
{
	m_tex->set_index_offset( io );
}

int FeImage::getFilterOffset() const
{
	return m_tex->get_filter_offset();
}

void FeImage::setFilterOffset( int fo )
{
	m_tex->set_filter_offset( fo );
}

void FeImage::rawset_index_offset( int io )
{
	m_tex->set_index_offset( io, false );
}

void FeImage::rawset_filter_offset( int fo )
{
	m_tex->set_filter_offset( fo, false );
}

bool FeImage::fix_masked_image()
{
	return m_tex->fix_masked_image();
}


void FeImage::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
	FeShader *s = get_shader();
	if ( s )
	{
		const sf::Shader *sh = s->get_shader();
		if ( sh )
			states.shader = sh;
	}
	else
		states.shader = FeBlend::get_default_shader( m_blend_mode );

	states.blendMode = FeBlend::get_blend_mode( m_blend_mode );
	target.draw( m_sprite, states );
}

void FeImage::scale()
{
	// The texture size is the actual pixel dimensions of the image
	sf::FloatRect texture_rect = m_sprite.getTextureRect();
	sf::Vector2f tex_size = sf::Vector2f(
		abs( texture_rect.size.x ),
		abs( texture_rect.size.y )
	);

	// Exit early if the texture size is invalid
	if (( tex_size.x == 0 ) || ( tex_size.y == 0 ))
		return;

	// Prepare some ratios and sizes
	sf::Vector2f size = sf::Vector2f( abs( m_size.x ), abs( m_size.y ) );
	float img_ratio = size.x / size.y;
	float tex_ratio = tex_size.x / tex_size.y;
	float par_ratio = resolveAspectRatio();

	sf::Vector2f par_size = par_ratio > 1.0
		? sf::Vector2f( tex_size.y * par_ratio, tex_size.y )
		: sf::Vector2f( tex_size.x, tex_size.x / par_ratio );

	sf::Vector2f goal_size = m_preserve_aspect_ratio ? par_size : tex_size;
	float goal_ratio = m_preserve_aspect_ratio ? par_ratio : tex_ratio;

	// Select the size according to the `fit` required
	switch ( resolveFit() )
	{
		default:
		case Fill:
			if ( m_auto_size.x && m_auto_size.y ) {
				size = goal_size;
			} else if ( m_auto_size.x ) {
				size.x = tex_size.x * size.y / tex_size.x * goal_ratio;
			} else if ( m_auto_size.y ) {
				size.y = tex_size.y * size.x / tex_size.y / goal_ratio;
			}
			break;
		case None:
			size = goal_size;
			break;
		case Contain:
			if ( goal_ratio > img_ratio ) {
				size.y = size.x / goal_ratio;
			} else {
				size.x = size.y * goal_ratio;
			}
			break;
		case Cover:
			if ( goal_ratio > img_ratio ) {
				size.x = size.y * goal_ratio;
			} else {
				size.y = size.x / goal_ratio;
			}
			break;
	}

	// Determine which way the image is flipped
	sf::Vector2f flip = sf::Vector2f( m_size.x < 0 ? -1 : 1, m_size.y < 0 ? -1 : 1 );
	if ( m_auto_size.x && flip.y == -1 ) flip.x = -1;
	if ( m_auto_size.y && flip.x == -1 ) flip.y = -1;

	// Flip the resulting size to match the image flip
	size.x *= flip.x;
	size.y *= flip.y;

	sf::Vector2f scale = sf::Vector2f(
		size.x / tex_size.x,
		size.y / tex_size.y
	);

	// If auto-sizing set the calculated values back into m_size
	if ( m_auto_size.x ) m_size.x = size.x;
	if ( m_auto_size.y ) m_size.y = size.y;

	// Anchor the texture, most common for Fit.Contain where the texture is smaller than the image
	sf::Vector2f offset = sf::Vector2f(
		( m_size.x - size.x ) * m_fit_anchor.x,
		( m_size.y - size.y ) * m_fit_anchor.y
	);

	// The crop property instructs the sprite to reduce size to hide the texture overlap
	FloatEdges crop = !m_crop
		? FloatEdges( 0, 0, 0, 0)
		: FloatEdges(
			std::max( 0.f, (-offset.x) * flip.x ),
			std::max( 0.f, (-offset.y) * flip.y ),
			std::max( 0.f, (( offset.x + size.x - m_size.x )) * flip.x ),
			std::max( 0.f, (( offset.y + size.y - m_size.y )) * flip.y )
		);

	// Translate the texture to match the desired position
	sf::Transform t;
	sf::Angle rotation = sf::degrees( m_rotation );
	sf::Vector2f pos_rotation = offset.length()
		? t.rotate( rotation ).transformPoint( offset )
		: sf::Vector2f( 0, 0 );

	sf::Vector2f pos = m_pos;
	pos += pos_rotation;
	pos += sf::Vector2f(
		( m_rotation_origin.x - m_anchor.x ) * m_size.x,
		( m_rotation_origin.y - m_anchor.y ) * m_size.y
	);

	sf::Vector2f origin = sf::Vector2f(
		( m_origin.x + m_rotation_origin.x * m_size.x ) / scale.x,
		( m_origin.y + m_rotation_origin.y * m_size.y ) / scale.y
	);

	// Populate the fit_rect so users can get the resulting image dimensions
	IntEdges padding = m_sprite.getPadding();
	m_fit_rect = sf::FloatRect(
		sf::Vector2f(
			pos.x - pos_rotation.x + offset.x - ( origin.x * scale.x ) + (( crop.left - padding.left ) * flip.x ) - m_pos.x,
			pos.y - pos_rotation.y + offset.y - ( origin.y * scale.y ) + (( crop.top - padding.top ) * flip.y ) - m_pos.y
		),
		sf::Vector2f(
			( tex_size.x * scale.x ) + (( padding.left + padding.right - crop.left - crop.right ) * flip.x ),
			( tex_size.y * scale.y ) + (( padding.top + padding.bottom - crop.top - crop.bottom ) * flip.y )
		)
	);

	// Apply the transformations
	m_sprite.setCrop( crop );
	m_sprite.setScale( scale );
	m_sprite.setPosition( pos );
	m_sprite.setRotation( rotation );
	m_sprite.setOrigin( origin );
}

sf::Vector2f FeImage::getPosition() const
{
	return m_pos;
}

sf::Vector2f FeImage::getSize() const
{
	return m_size;
}

bool FeImage::get_auto_width() const
{
	return m_auto_size.x;
}

bool FeImage::get_auto_height() const
{
	return m_auto_size.y;
}

float FeImage::get_width() const
{
	return FeBasePresentable::get_width();
}

float FeImage::get_height() const
{
	return FeBasePresentable::get_height();
}

void FeImage::set_auto_width( bool w )
{
	if ( m_auto_size.x != w )
	{
		m_auto_size.x = w;
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_auto_height( bool h )
{
	if ( m_auto_size.y != h )
	{
		m_auto_size.y = h;
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_width( float w )
{
	m_auto_size.x = false;
	FeBasePresentable::set_width( w );
}

void FeImage::set_height( float h )
{
	m_auto_size.y = false;
	FeBasePresentable::set_height( h );
}

void FeImage::set_pos( float x, float y, float w, float h )
{
	m_auto_size.x = false;
	m_auto_size.y = false;
	FeBasePresentable::set_pos( x, y, w, h );
}

void FeImage::setSize( const sf::Vector2f &s )
{
	if ( s != m_size )
	{
		m_size = s;
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeImage::setPosition( const sf::Vector2f &p )
{
	if ( p != m_pos )
	{
		m_pos = p;
		scale();
		FePresent::script_flag_redraw();
	}
}

float FeImage::getRotation() const
{
	return m_rotation;
}

void FeImage::setRotation( float r )
{
	if ( r != m_rotation )
	{
		m_rotation = r;
		scale();
		FePresent::script_flag_redraw();
	}
}

sf::Color FeImage::getColor() const
{
	return m_sprite.getColor();
}

void FeImage::setColor( sf::Color c )
{
	if ( c != m_sprite.getColor() )
	{
		m_sprite.setColor( c );
		FePresent::script_flag_redraw();
	}
}

sf::Vector2u FeImage::getTextureSize() const
{
	return m_tex->get_texture().getSize();
}

sf::FloatRect FeImage::getTextureRect() const
{
	return m_sprite.getTextureRect();
}

void FeImage::setTextureRect( const sf::FloatRect &r )
{
	if ( r != m_sprite.getTextureRect() )
	{
		m_sprite.setTextureRect( r );
		scale();
		FePresent::script_flag_redraw();
	}
}

int FeImage::getVideoFlags() const
{
	return (int)m_tex->get_video_flags();
}

void FeImage::setVideoFlags( int f )
{
	if ( f == getVideoFlags() )
		return;

	m_tex->set_video_flags( (FeVideoFlags)f );
}

bool FeImage::getVideoPlaying() const
{
	return m_tex->get_play_state();
}

void FeImage::setVideoPlaying( bool f )
{
	m_tex->set_play_state( f );
}

int FeImage::getVideoDuration() const
{
	return m_tex->get_video_duration();
}

int FeImage::getVideoTime() const
{
	return m_tex->get_video_time();
}

const char *FeImage::getFileName() const
{
	return m_tex->get_file_name();
}

void FeImage::setFileName( const char *n )
{
	std::string filename = n;

	m_tex->load_file( filename.c_str() );
}

int FeImage::getTrigger() const
{
	return m_tex->get_trigger();
}

void FeImage::setTrigger( int t )
{
	m_tex->set_trigger( t );
}

bool FeImage::getMovieEnabled() const
{
	return !(m_tex->get_video_flags() & VF_DisableVideo );
}

void FeImage::setMovieEnabled( bool f )
{
	int c = (int)m_tex->get_video_flags();

	if ( f )
		c |= VF_DisableVideo;
	else
		c &= ~VF_DisableVideo;

	m_tex->set_video_flags( (FeVideoFlags)c );
}

float FeImage::get_origin_x() const
{
	return m_origin.x;
}

float FeImage::get_origin_y() const
{
	return m_origin.y;
}

int FeImage::get_transform_origin_type() const
{
	return (FeImage::Alignment)m_transform_origin_type;
}

int FeImage::get_anchor_type() const
{
	return (FeImage::Alignment)m_anchor_type;
}

int FeImage::get_fit_anchor_type() const
{
	return (FeImage::Alignment)m_fit_anchor_type;
}

int FeImage::get_rotation_origin_type() const
{
	return (FeImage::Alignment)m_rotation_origin_type;
}

float FeImage::get_transform_origin_x() const
{
	return m_transform_origin.x;
}

float FeImage::get_transform_origin_y() const
{
	return m_transform_origin.y;
}

float FeImage::get_anchor_x() const
{
	return m_anchor.x;
}

float FeImage::get_anchor_y() const
{
	return m_anchor.y;
}

bool FeImage::get_crop() const
{
	return m_crop;
}

int FeImage::get_fit() const
{
	return m_fit;
}

// Special cases where the fit-type should be forced depending on other properties
int FeImage::resolveFit() const
{
	// Auto-size makes the image match the texture size
	if ( m_auto_size.x || m_auto_size.y )
		return Fill;

	// When using border Fill must be used to correctly position the texture
	IntEdges b = m_sprite.getBorder();
	if ( b.left || b.top || b.right || b.bottom )
		return Fill;

	// Replace legacy combination Fill + PAR with Contain
	if ((m_fit == Fill) && m_preserve_aspect_ratio && !m_auto_size.x && !m_auto_size.y)
		return Contain;

	return m_fit;
}

float FeImage::get_fit_anchor_x() const
{
	return m_fit_anchor.x;
}

float FeImage::get_fit_anchor_y() const
{
	return m_fit_anchor.y;
}

float FeImage::get_fit_x() const
{
	return m_fit_rect.position.x;
}

float FeImage::get_fit_y() const
{
	return m_fit_rect.position.y;
}

float FeImage::get_fit_width() const
{
	return m_fit_rect.size.x;
}

float FeImage::get_fit_height() const
{
	return m_fit_rect.size.y;
}

float FeImage::get_rotation_origin_x() const
{
	return m_rotation_origin.x;
}

float FeImage::get_rotation_origin_y() const
{
	return m_rotation_origin.y;
}

float FeImage::get_skew_x() const
{
	return m_sprite.getSkewX();
}

float FeImage::get_skew_y() const
{
	return m_sprite.getSkewY();
}

float FeImage::get_pinch_x() const
{
	return m_sprite.getPinchX();
}

float FeImage::get_pinch_y() const
{
	return m_sprite.getPinchY();
}

void FeImage::set_origin_x( float x )
{
	if ( x != m_origin.x )
	{
		m_origin.x = x;
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_origin_y( float y )
{
	if ( y != m_origin.y )
	{
		m_origin.y = y;
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_transform_origin( float x, float y )
{
	if (
		x != m_transform_origin.x || x != m_anchor.x || x != m_rotation_origin.x ||
		y != m_transform_origin.y || y != m_anchor.y || y != m_rotation_origin.y
	)
	{
		m_transform_origin = sf::Vector2f( x, y );
		m_anchor = sf::Vector2f( x, y );
		m_rotation_origin = sf::Vector2f( x, y );
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_transform_origin_type( int t )
{
	m_transform_origin_type = (FeImage::Alignment)t;
	sf::Vector2f a = alignTypeToVector( t );
	set_transform_origin( a.x, a.y );
}

void FeImage::set_anchor( float x, float y )
{
	if ( x != m_anchor.x || y != m_anchor.y )
	{
		m_anchor = sf::Vector2f( x, y );
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_anchor_type( int t )
{
	m_anchor_type = (FeImage::Alignment)t;
	sf::Vector2f a = alignTypeToVector( t );
	set_anchor( a.x, a.y );
}

void FeImage::set_fit_anchor( float x, float y )
{
	if ( x != m_fit_anchor.x || y != m_fit_anchor.y )
	{
		m_fit_anchor = sf::Vector2f( x, y );
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_fit_anchor_type( int t )
{
	m_fit_anchor_type = (FeImage::Alignment)t;
	sf::Vector2f a = alignTypeToVector( t );
	set_fit_anchor( a.x, a.y );
}

void FeImage::set_rotation_origin( float x, float y )
{
	if ( x != m_rotation_origin.x || y != m_rotation_origin.y )
	{
		m_rotation_origin = sf::Vector2f( x, y );
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_rotation_origin_type( int t )
{
	m_rotation_origin_type = (FeImage::Alignment)t;
	sf::Vector2f o = alignTypeToVector( t );
	set_rotation_origin( o.x, o.y );
}

void FeImage::set_transform_origin_x( float x )
{
	if ( x != m_transform_origin.x || x != m_anchor.x || x != m_rotation_origin.x )
	{
		m_transform_origin.x = x;
		m_anchor.x = x;
		m_rotation_origin.x = x;
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_transform_origin_y( float y )
{
	if ( y != m_transform_origin.y || y != m_anchor.y || y != m_rotation_origin.y )
	{
		m_transform_origin.y = y;
		m_anchor.y = y;
		m_rotation_origin.y = y;
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_anchor_x( float x )
{
	set_anchor( x, get_anchor_y() );
}

void FeImage::set_anchor_y( float y )
{
	set_anchor( get_anchor_x(), y );
}

void FeImage::set_crop( bool c )
{
	if ( c == m_crop ) return;
	m_crop = c;
	scale();
	FePresent::script_flag_redraw();
}

void FeImage::set_fit( int f )
{
	if ( f == m_fit ) return;
	m_fit = (FeImage::Fit)f;
	scale();
	FePresent::script_flag_redraw();
}

void FeImage::set_fit_anchor_x( float x )
{
	set_fit_anchor( x, get_fit_anchor_y() );
}

void FeImage::set_fit_anchor_y( float y )
{
	set_fit_anchor( get_fit_anchor_x(), y );
}

void FeImage::set_rotation_origin_x( float x )
{
	set_rotation_origin( x, get_rotation_origin_y() );
}

void FeImage::set_rotation_origin_y( float y )
{
	set_rotation_origin( get_rotation_origin_x(), y );
}

void FeImage::set_skew_x( float x )
{
	if ( x != m_sprite.getSkewX() )
	{
		m_sprite.setSkewX( x );
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_skew_y( float y )
{
	if ( y != m_sprite.getSkewY() )
	{
		m_sprite.setSkewY( y );
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_pinch_x( float x )
{
	if ( x != m_sprite.getPinchX() )
	{
		m_sprite.setPinchX( x );
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_pinch_y( float y )
{
	if ( y != m_sprite.getPinchY() )
	{
		m_sprite.setPinchY( y );
		FePresent::script_flag_redraw();
	}
}

int FeImage::get_texture_width() const
{
	return getTextureSize().x;
}

int FeImage::get_texture_height() const
{
	return getTextureSize().y;
}

float FeImage::get_subimg_x() const
{
	return getTextureRect().position.x;
}

float FeImage::get_subimg_y() const
{
	return getTextureRect().position.y;
}

float FeImage::get_subimg_width() const
{
	return getTextureRect().size.x;
}

float FeImage::get_subimg_height() const
{
	return getTextureRect().size.y;
}

float FeImage::resolveAspectRatio() const
{
	if ( resolveFit() == Fill && !m_auto_size.x && !m_auto_size.y )
		return abs( m_size.x / m_size.y );

	if ( m_force_aspect_ratio )
		return m_force_aspect_ratio;

	sf::FloatRect r = getTextureRect();
	float tex_ratio = r.size.x / r.size.y;

	if ( get_preserve_aspect_ratio() )
		return tex_ratio * get_sample_aspect_ratio();

	return tex_ratio;
}

float FeImage::get_force_aspect_ratio() const
{
	return m_force_aspect_ratio;
}

float FeImage::get_sample_aspect_ratio() const
{
	return m_tex->get_sample_aspect_ratio();
}

bool FeImage::get_preserve_aspect_ratio() const
{
	return m_preserve_aspect_ratio;
}

void FeImage::set_subimg_x( float x )
{
	sf::FloatRect r = getTextureRect();
	r.position.x=x;
	setTextureRect( r );
}

void FeImage::set_subimg_y( float y )
{
	sf::FloatRect r = getTextureRect();
	r.position.y=y;
	setTextureRect( r );
}

void FeImage::set_subimg_width( float w )
{
	sf::FloatRect r = getTextureRect();
	r.size.x=w;
	setTextureRect( r );
}

void FeImage::set_subimg_height( float h )
{
	sf::FloatRect r = getTextureRect();
	r.size.y=h;
	setTextureRect( r );
}

void FeImage::set_force_aspect_ratio( float r )
{
	r = abs( r );
	if ( r != m_force_aspect_ratio )
	{
		m_force_aspect_ratio = r;
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_preserve_aspect_ratio( bool p )
{
	if ( p != m_preserve_aspect_ratio )
	{
		m_preserve_aspect_ratio = p;
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_mipmap( bool m )
{
	m_tex->set_mipmap( m );
}

bool FeImage::get_mipmap() const
{
	return m_tex->get_mipmap();
}

void FeImage::set_clear( bool c )
{
	m_tex->set_clear( c );
}

bool FeImage::get_clear() const
{
	return m_tex->get_clear();
}

void FeImage::set_repeat( bool r )
{
	m_tex->set_repeat( r );
}

bool FeImage::get_repeat() const
{
	return m_tex->get_repeat();
}

void FeImage::set_redraw( bool r )
{
	m_tex->set_redraw( r );
}

bool FeImage::get_redraw() const
{
	return m_tex->get_redraw();
}

void FeImage::set_volume( float v )
{
	m_tex->set_volume( v );
}

float FeImage::get_volume() const
{
	return m_tex->get_volume();
}

void FeImage::set_pan( float p )
{
	m_tex->set_pan( p );
}

float FeImage::get_pan() const
{
	return m_tex->get_pan();
}

float FeImage::get_vu_mono() const
{
	FeTextureContainer *tc = dynamic_cast<FeTextureContainer*>(m_tex);
	return tc ? tc->get_vu_mono() : 0.0f;
}

float FeImage::get_vu_left() const
{
	FeTextureContainer *tc = dynamic_cast<FeTextureContainer*>(m_tex);
	return tc ? tc->get_vu_left() : 0.0f;
}

float FeImage::get_vu_right() const
{
	FeTextureContainer *tc = dynamic_cast<FeTextureContainer*>(m_tex);
	return tc ? tc->get_vu_right() : 0.0f;
}

const SqratArrayWrapper& FeImage::get_fft_array_mono() const
{
	FeTextureContainer *tc = dynamic_cast<FeTextureContainer*>( m_tex );
	if ( tc )
	{
		if ( const auto* ptr = tc->get_fft_mono_ptr() )
		{
			m_fft_array_wrapper.set_data( ptr );
			return m_fft_array_wrapper;
		}
	}

	return m_fft_zero_wrapper;
}

const SqratArrayWrapper& FeImage::get_fft_array_left() const
{
	FeTextureContainer *tc = dynamic_cast<FeTextureContainer*>( m_tex );
	if ( tc )
	{
		if ( const auto* ptr = tc->get_fft_left_ptr() )
		{
			m_fft_array_wrapper.set_data( ptr );
			return m_fft_array_wrapper;
		}
	}

	return m_fft_zero_wrapper;
}

const SqratArrayWrapper& FeImage::get_fft_array_right() const
{
	FeTextureContainer *tc = dynamic_cast<FeTextureContainer*>( m_tex );
	if ( tc )
	{
		if ( const auto* ptr = tc->get_fft_right_ptr() )
		{
			m_fft_array_wrapper.set_data( ptr );
			return m_fft_array_wrapper;
		}
	}

	return m_fft_zero_wrapper;
}

void FeImage::transition_swap( FeImage *o )
{
	// if we're pointing at the same texture, don't do anything
	//
	if ( m_tex == o->m_tex )
		return;

	// otherwise swap the textures
	//
	m_tex->transition_swap( o->m_tex );
}

void FeImage::set_smooth( bool s )
{
	m_tex->set_smooth( s );
}

bool FeImage::get_smooth() const
{
	return m_tex->get_smooth();
}

int FeImage::get_blend_mode() const
{
	return (FeBlend::Mode)m_blend_mode;
}

void FeImage::set_blend_mode( int b )
{
	m_blend_mode = (FeBlend::Mode)b;
}

FeImage *FeImage::add_image(const char *n, float x, float y, float w, float h)
{
	FePresentableParent *p = m_tex->get_presentable_parent();
	if ( p )
		return p->add_image( n, x, y, w, h );

	return NULL;
}

FeImage *FeImage::add_image(const char *n, float x, float y )
{
	return add_image( n, x, y, 0, 0 );
}

FeImage *FeImage::add_image(const char *n )
{
	return add_image( n, 0, 0, 0, 0 );
}

FeImage *FeImage::add_artwork(const char *l, float x, float y, float w, float h )
{
	FePresentableParent *p = m_tex->get_presentable_parent();
	if ( p )
		return p->add_artwork( l, x, y, w, h );

	return NULL;
}

FeImage *FeImage::add_artwork(const char *l, float x, float y)
{
	return add_artwork( l, x, y, 0, 0 );
}

FeImage *FeImage::add_artwork(const char *l )
{
	return add_artwork( l, 0, 0, 0, 0 );
}

FeImage *FeImage::add_clone(FeImage *i )
{
	FePresentableParent *p = m_tex->get_presentable_parent();
	if ( p )
		return p->add_clone( i );

	return NULL;
}

FeText *FeImage::add_text(const char *t, int x, int y, int w, int h)
{
	FePresentableParent *p = m_tex->get_presentable_parent();
	if ( p )
		return p->add_text( t, x, y, w, h );

	return NULL;
}

FeListBox *FeImage::add_listbox(int x, int y, int w, int h)
{
	FePresentableParent *p = m_tex->get_presentable_parent();
	if ( p )
		return p->add_listbox( x, y, w, h );

	return NULL;
}

FeRectangle *FeImage::add_rectangle(float x, float y, float w, float h)
{
	FePresentableParent *p = m_tex->get_presentable_parent();
	if ( p )
		return p->add_rectangle( x, y, w, h );

	return NULL;
}

FeImage *FeImage::add_surface(int w, int h)
{
	return add_surface( 0, 0, w, h );
}

FeImage *FeImage::add_surface(float x, float y, int w, int h)
{
	FePresentableParent *p = m_tex->get_presentable_parent();
	if ( p )
		return p->add_surface( x, y, w, h );

	return NULL;
}

FePresentableParent *FeImage::get_presentable_parent()
{
	return m_tex->get_presentable_parent();
}

void FeImage::set_border( int l, int t, int r, int b )
{
	IntEdges border( std::max(0, l), std::max(0, t), std::max(0, r), std::max(0, b) );
	if ( border != m_sprite.getBorder() )
	{
		m_sprite.setBorder( border );
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_padding( int l, int t, int r, int b )
{
	IntEdges padding( l, t, r, b );
	if ( padding != m_sprite.getPadding() )
	{
		m_sprite.setPadding( padding );
		scale();
		FePresent::script_flag_redraw();
	}
}

void FeImage::set_border_scale( float s )
{
	if ( s != m_sprite.getBorderScale() )
	{
		m_sprite.setBorderScale( s );
		scale();
		FePresent::script_flag_redraw();
	}
}

int FeImage::get_padding_left() const
{
	return m_sprite.getPadding().left;
}

int FeImage::get_padding_top() const
{
	return m_sprite.getPadding().top;
}

int FeImage::get_padding_right() const
{
	return m_sprite.getPadding().right;
}

int FeImage::get_padding_bottom() const
{
	return m_sprite.getPadding().bottom;
}

void FeImage::set_padding_left( int l )
{
	IntEdges padding = m_sprite.getPadding();
	set_padding( l, padding.top, padding.right, padding.bottom );
}

void FeImage::set_padding_top( int t )
{
	IntEdges padding = m_sprite.getPadding();
	set_padding( padding.left, t, padding.right, padding.bottom );
}

void FeImage::set_padding_right( int r )
{
	IntEdges padding = m_sprite.getPadding();
	set_padding( padding.left, padding.top, r, padding.bottom );
}

void FeImage::set_padding_bottom( int b )
{
	IntEdges padding = m_sprite.getPadding();
	set_padding( padding.left, padding.top, padding.right, b );
}

int FeImage::get_border_left() const
{
	return m_sprite.getBorder().left;
}

int FeImage::get_border_top() const
{
	return m_sprite.getBorder().top;
}

int FeImage::get_border_right() const
{
	return m_sprite.getBorder().right;
}

int FeImage::get_border_bottom() const
{
	return m_sprite.getBorder().bottom;
}

void FeImage::set_border_left( int l )
{
	IntEdges border = m_sprite.getBorder();
	set_border( l, border.top, border.right, border.bottom );
}

void FeImage::set_border_top( int t )
{
	IntEdges border = m_sprite.getBorder();
	set_border( border.left, t, border.right, border.bottom );
}

void FeImage::set_border_right( int r )
{
	IntEdges border = m_sprite.getBorder();
	set_border( border.left, border.top, r, border.bottom );
}

void FeImage::set_border_bottom( int b )
{
	IntEdges border = m_sprite.getBorder();
	set_border( border.left, border.top, border.right, b );
}

float FeImage::get_border_scale() const
{
	return m_sprite.getBorderScale();
}

void FeImage::set_fft_bands( int count )
{
	FeTextureContainer *tc = dynamic_cast<FeTextureContainer*>( m_tex );
	if ( tc )
		tc->set_fft_bands( count );
}

int FeImage::get_fft_bands() const
{
	FeTextureContainer *tc = dynamic_cast<FeTextureContainer*>( m_tex );
	if ( tc )
		return tc->get_fft_bands();

	return 32;
}
