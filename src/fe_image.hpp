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

#ifndef FE_IMAGE_HPP
#define FE_IMAGE_HPP

#include <SFML/Graphics.hpp>
#include <sqrat.h>
#include "sprite.hpp"
#include "fe_presentable.hpp"
#include "fe_blend.hpp"
#include "sqrat_array_wrapper.hpp"
#include "fe_audio_fx.hpp"

class FeSettings;
class FeMedia;
class FeImage;
class FeText;
class FeListBox;
class FeRectangle;
class FeTextureContainer;
class FeImageLoaderEntry;

enum FeVideoFlags
{
	VF_Normal			= 0,
	VF_DisableVideo	= 1 << 0,
	VF_NoLoop			= 1 << 1,
	VF_NoAutoStart		= 1 << 2,
	VF_NoAudio			= 1 << 3
};

class FeBaseTextureContainer
{
public:
	virtual ~FeBaseTextureContainer();

	virtual const sf::Texture &get_texture()=0;

	virtual void on_new_selection( FeSettings *feSettings )=0;
	virtual void on_end_navigation( FeSettings *feSettings )=0;

	virtual void on_new_list( FeSettings *, bool new_display )=0;

	virtual bool get_visible() const;
	virtual bool tick( FeSettings *feSettings, bool play_movies ); // returns true if redraw required

	virtual void set_play_state( bool play );
	virtual bool get_play_state() const;

	virtual void set_index_offset( int io, bool do_update=true );
	virtual int get_index_offset() const;
	virtual void set_filter_offset( int fo, bool do_update=true );
	virtual int get_filter_offset() const;

	virtual void set_video_flags( FeVideoFlags );
	virtual FeVideoFlags get_video_flags() const;
	virtual int get_video_duration() const;
	virtual int get_video_time() const;

	virtual void load_file( const char *n );
	virtual const char *get_file_name() const;
	virtual void set_trigger( int );
	virtual int get_trigger() const;

	virtual void transition_swap( FeBaseTextureContainer *o );
	virtual bool fix_masked_image();

	virtual void set_smooth( bool )=0;
	virtual bool get_smooth() const=0;

	virtual void set_mipmap( bool )=0;
	virtual bool get_mipmap() const=0;

	virtual void set_clear( bool );
	virtual bool get_clear() const;

	virtual void set_repeat( bool );
	virtual bool get_repeat() const;

	virtual void set_redraw( bool );
	virtual bool get_redraw() const;

	virtual void set_volume( float );
	virtual float get_volume() const;

	virtual void set_pan( float );
	virtual float get_pan() const;

	virtual float get_sample_aspect_ratio() const;

	// function for use with surface objects
	//
	virtual FePresentableParent *get_presentable_parent();

	void register_image( FeImage * );

	virtual void release_audio( bool );
	virtual void on_redraw_surfaces();

protected:
	FeBaseTextureContainer();
	FeBaseTextureContainer( const FeBaseTextureContainer & );
	FeBaseTextureContainer &operator=( const FeBaseTextureContainer & );

	//
	// Return a pointer to the FeTextureContainer if this is that type of container
	//
	virtual FeTextureContainer *get_derived_texture_container();

	// call this to notify registered images that the texture has changed
	void notify_texture_change();

private:
	std::vector< FeImage * > m_images;

	friend class FeTextureContainer;
};

class FeTextureContainer : public FeBaseTextureContainer
{
public:
	FeTextureContainer( bool is_artwork, const std::string &name="" );

	~FeTextureContainer();

	const sf::Texture &get_texture();
	bool get_visible() const;

	void on_new_selection( FeSettings *feSettings );
	void on_end_navigation( FeSettings *feSettings );
	void on_new_list( FeSettings *, bool );

	bool tick( FeSettings *feSettings, bool play_movies ); // returns true if redraw required
	void set_play_state( bool play );
	bool get_play_state() const;

	void set_index_offset( int io, bool do_update );
	int get_index_offset() const;
	void set_filter_offset( int fo, bool do_update );
	int get_filter_offset() const;

	void set_video_flags( FeVideoFlags );
	FeVideoFlags get_video_flags() const;
	int get_video_duration() const;
	int get_video_time() const;

	void load_file( const char *n );
	const char *get_file_name() const;
	void set_trigger( int );
	int get_trigger() const;

	void transition_swap( FeBaseTextureContainer *o );

	bool fix_masked_image();

	void set_smooth( bool );
	bool get_smooth() const;

	void release_audio( bool );

	void set_mipmap( bool );
	bool get_mipmap() const;

	void set_repeat( bool );
	bool get_repeat() const;

	void set_volume( float );
	float get_volume() const;

	void set_pan( float );
	float get_pan() const;

	void set_fft_bands( int );
	int get_fft_bands() const;

	float get_vu_mono() const;
	float get_vu_left() const;
	float get_vu_right() const;
	const std::vector<float> *get_fft_mono_ptr() const;
	const std::vector<float> *get_fft_left_ptr() const;
	const std::vector<float> *get_fft_right_ptr() const;

	float get_sample_aspect_ratio() const;

	FeAudioEffectsManager& get_audio_effects() { return m_audio_effects; };
	FeMedia *get_media() const;

protected:
	FeTextureContainer *get_derived_texture_container();

private:

#ifndef NO_MOVIE
	bool load_with_ffmpeg(
		const std::string &filename,
		bool is_image );
#endif

	bool try_to_load(
		const std::string &filename,
		bool is_image=false );

	void internal_update_selection( FeSettings *feSettings );
	void clear();

	sf::Texture m_texture;

	std::string m_art_name; // artwork label/template name (dynamic images)
	std::string m_file_name; // the name of the loaded file
	int m_index_offset;
	int m_filter_offset;
	int m_current_rom_index;
	int m_current_filter_index;

	enum Type { IsArtwork, IsDynamic, IsStatic };
	Type m_type;
	int m_art_update_trigger;
	FeMedia *m_movie;
	int m_movie_status; // 0=no play, 1=ready to play, >=PLAY_COUNT=playing
	FeVideoFlags m_video_flags;
	bool m_mipmap;
	bool m_smooth;
	float m_volume;
	float m_pan;
	int m_fft_bands;
	FeImageLoaderEntry *m_entry;
	FeAudioEffectsManager m_audio_effects;
};

class FeSurfaceTextureContainer : public FeBaseTextureContainer, public FePresentableParent
{
public:

	FeSurfaceTextureContainer( int width, int height );
	~FeSurfaceTextureContainer();

	const sf::Texture &get_texture();

	void on_new_selection( FeSettings *feSettings );
	void on_end_navigation( FeSettings *feSettings );
	void on_new_list( FeSettings *, bool );

	void on_redraw_surfaces();

	void set_smooth( bool );
	bool get_smooth() const;

	void set_mipmap( bool );
	bool get_mipmap() const;

	void set_clear( bool );
	bool get_clear() const;

	void set_repeat( bool );
	bool get_repeat() const;

	void set_redraw( bool );
	bool get_redraw() const;

	FePresentableParent *get_presentable_parent();

private:
	sf::RenderTexture m_texture;
	bool m_clear;
	bool m_redraw;
	bool m_mipmap;
};

class FeImage : public sf::Drawable, public FeBasePresentable
{
public:
	enum Alignment
	{
		Left,
		Centre,
		Right,
		Top,
		Bottom,
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight
	};

	enum Fit
	{
		Fill,
		Contain,
		Cover,
		None
	};

	FeImage(
		FePresentableParent &p,
		FeBaseTextureContainer *,
		float x,
		float y,
		float w,
		float h
	);

	FeImage( FeImage * ); // clone the given image (texture is not copied)
	~FeImage();

	const sf::Texture *get_texture();
	int get_texture_width() const;
	int get_texture_height() const;
	void texture_changed( FeBaseTextureContainer *new_tex=NULL );

	void transition_swap( FeImage * );
	bool fix_masked_image();
	FePresentableParent *get_presentable_parent();
	const sf::Drawable &drawable() const { return (const sf::Drawable &)*this; };

	template <typename T>
	void setSize( T w, T h ) { setSize( sf::Vector2f( w, h ) ); };
	void setSize( const sf::Vector2f &s );
	sf::Vector2f getSize() const;

	template <typename T>
	void setPosition( T x, T y ) { setPosition( sf::Vector2f( x, y ) );};
	void setPosition( const sf::Vector2f & );
	sf::Vector2f getPosition() const;

	void setRotation( float );
	float getRotation() const;

	void setColor( sf::Color );
	sf::Color getColor() const;

	void setIndexOffset( int );
	void rawset_index_offset( int io );
	int getIndexOffset() const;

	void setFilterOffset( int );
	void rawset_filter_offset( int fo );
	int getFilterOffset() const;

	void setTextureRect( const sf::FloatRect &);
	sf::FloatRect getTextureRect() const;
	sf::Vector2u getTextureSize() const;

	// deprecated as of 1.3, use video_flags instead:
	void setMovieEnabled( bool );
	bool getMovieEnabled() const;

	void setVideoFlags( int f );
	int getVideoFlags() const;

	void setVideoPlaying( bool );
	bool getVideoPlaying() const;

	int getVideoDuration() const;
	int getVideoTime() const;

	void setFileName( const char * );
	const char *getFileName() const;

	void setTrigger( int );
	int getTrigger() const;

	void set_pos( float x, float y, float w, float h );
	void set_width( float w );
	float get_width() const;
	void set_height( float h );
	float get_height() const;

	void set_pan( float );
	float get_pan() const;

	float get_vu_mono() const;
	float get_vu_left() const;
	float get_vu_right() const;
	const SqratArrayWrapper& get_fft_array_mono() const;
	const SqratArrayWrapper& get_fft_array_left() const;
	const SqratArrayWrapper& get_fft_array_right() const;
	void set_fft_bands( int count );
	int get_fft_bands() const;

	void set_volume( float );
	float get_volume() const;

	void set_auto_width( bool w );
	bool get_auto_width() const;
	void set_auto_height( bool h );
	bool get_auto_height() const;

	void set_origin_x( float x );
	float get_origin_x() const;
	void set_origin_y( float y );
	float get_origin_y() const;

	void set_transform_origin( float x, float y );
	void set_transform_origin_type( int t );
	int get_transform_origin_type() const;
	void set_transform_origin_x( float x );
	float get_transform_origin_x() const;
	void set_transform_origin_y( float y );
	float get_transform_origin_y() const;

	void set_anchor( float x, float y );
	void set_anchor_type( int t );
	int get_anchor_type() const;
	void set_anchor_x( float x );
	float get_anchor_x() const;
	void set_anchor_y( float y );
	float get_anchor_y() const;

	void set_crop( bool c );
	bool get_crop() const;
	void set_fit( int f );
	int get_fit() const;
	void set_fit_anchor( float x, float y );
	void set_fit_anchor_type( int t );
	int get_fit_anchor_type() const;
	void set_fit_anchor_x( float x );
	float get_fit_anchor_x() const;
	void set_fit_anchor_y( float y );
	float get_fit_anchor_y() const;
	float get_fit_x() const;
	float get_fit_y() const;
	float get_fit_width() const;
	float get_fit_height() const;

	void set_rotation_origin( float x, float y );
	void set_rotation_origin_type( int t );
	int get_rotation_origin_type() const;
	void set_rotation_origin_x( float x );
	float get_rotation_origin_x() const;
	void set_rotation_origin_y( float y );
	float get_rotation_origin_y() const;

	void set_skew_x( float x );
	float get_skew_x() const;
	void set_skew_y( float y );
	float get_skew_y() const;
	void set_pinch_x( float x );
	float get_pinch_x() const;
	void set_pinch_y( float y );
	float get_pinch_y() const;

	void set_border( int l, int t, int r, int b );
	void set_border_left( int l );
	int get_border_left() const;
	void set_border_top( int t );
	int get_border_top() const;
	void set_border_right( int r );
	int get_border_right() const;
	void set_border_bottom( int b );
	int get_border_bottom() const;
	void set_border_scale( float s );
	float get_border_scale() const;

	void set_padding( int l, int t, int r, int b );
	void set_padding_left( int l );
	int get_padding_left() const;
	void set_padding_top( int t );
	int get_padding_top() const;
	void set_padding_right( int r );
	int get_padding_right() const;
	void set_padding_bottom( int b );
	int get_padding_bottom() const;

	float get_sample_aspect_ratio() const;
	void set_force_aspect_ratio( float r );
	float get_force_aspect_ratio() const;
	void set_preserve_aspect_ratio( bool p );
	bool get_preserve_aspect_ratio() const;

	void set_subimg_x( float x );
	float get_subimg_x() const;
	void set_subimg_y( float y );
	float get_subimg_y() const;
	void set_subimg_width( float w );
	float get_subimg_width() const;
	void set_subimg_height( float h );
	float get_subimg_height() const;

	void set_mipmap( bool m );
	bool get_mipmap() const;
	void set_smooth( bool );
	bool get_smooth() const;

	void set_clear( bool );
	bool get_clear() const;
	void set_repeat( bool );
	bool get_repeat() const;
	void set_redraw( bool );
	bool get_redraw() const;
	void set_blend_mode( int b );
	int get_blend_mode() const;

	bool get_visible() const;

	//
	// Callback functions for use with surface objects
	//
	FeImage *add_image( const char *,float, float, float, float );
	FeImage *add_image( const char *, float, float );
	FeImage *add_image( const char * );
	FeImage *add_artwork( const char *, float, float, float, float );
	FeImage *add_artwork( const char *, float, float );
	FeImage *add_artwork( const char * );
	FeImage *add_surface( float, float, int, int );
	FeImage *add_surface( int, int );
	FeImage *add_clone( FeImage * );
	FeText *add_text( const char *, int, int, int, int );
	FeListBox *add_listbox( int, int, int, int );
	FeRectangle *add_rectangle( float, float, float, float );

protected:
	FeBaseTextureContainer *m_tex;
	FeSprite m_sprite;
	sf::Vector2f m_pos;
	sf::Vector2f m_size;
	sf::Vector2u m_auto_size;

	sf::Vector2f m_origin;
	sf::Vector2f m_transform_origin;
	FeImage::Alignment m_transform_origin_type;

	sf::Vector2f m_anchor;
	FeImage::Alignment m_anchor_type;

	float m_rotation;
	sf::Vector2f m_rotation_origin;
	FeImage::Alignment m_rotation_origin_type;

	bool m_crop;
	FeImage::Fit m_fit;
	sf::Vector2f m_fit_anchor;
	FeImage::Alignment m_fit_anchor_type;

	FeBlend::Mode m_blend_mode;
	bool m_preserve_aspect_ratio;
	float m_force_aspect_ratio;

	sf::Vector2f m_scale;
	sf::FloatRect m_fit_rect;

	void scale();
	int resolveFit() const;
	float resolveAspectRatio() const;
	sf::Vector2f alignTypeToVector( int a );

	void draw( sf::RenderTarget& target, sf::RenderStates states ) const;

private:
	std::vector<float> m_fft_data_zero;
	mutable SqratArrayWrapper m_fft_zero_wrapper;
	mutable SqratArrayWrapper m_fft_array_wrapper;

};

#endif
