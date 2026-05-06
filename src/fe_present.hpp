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

#ifndef FE_PRESENT_HPP
#define FE_PRESENT_HPP

#include "fe_font.hpp"
#include "fe_presentable.hpp"
#include "fe_renderer.hpp"
#include "fe_settings.hpp"
#include "fe_music.hpp"
#include "fe_sound.hpp"
#include "fe_shader.hpp"
#include "fe_transform.hpp"
#include "fe_time.hpp"
#include "fe_types.hpp"
#include "fe_window.hpp"

class FeImage;
class FeModel3D;
class FeBaseTextureContainer;
class FeTextureContainer;
class FeText;
class FeListBox;
class FeRectangle;
class FeFontContainer;
class FeSurfaceTextureContainer;
class FePresentableParent;

namespace sf
{
	class InputStream;
};

struct SDL_Surface;

enum FeTransitionType
{
	StartLayout      = 1<<0,  // var: FromToScreenSaver, FromToFrontend or FromToNoValue
	EndLayout        = 1<<1,  // var: FromToScreenSaver, FromToFrontend or FromToNoValue
	ToNewSelection   = 1<<2,  // var = index_offset of new selection
	FromOldSelection = 1<<3,  // var = index_offset of old selection
	ToGame           = 1<<4,  // var = 0
	FromGame         = 1<<5,  // var = 0
	ToNewList        = 1<<6,  // var = filter offset of new filter (if available), otherwise 0
	EndNavigation    = 1<<7,  // var = 0
	ShowOverlay      = 1<<8,  // var = Custom, Exit, Displays, Filters, Tags
	HideOverlay      = 1<<9,  // var = 0
	NewSelOverlay    = 1<<10, // var = index of new selection
	ChangedTag       = 1<<11  // var = FeRomInfo::Favourite, FeRomInfo::Tags
};

//
// Container class for use in our font pool
//
class FeFontContainer
{
public:
	FeFontContainer();
	~FeFontContainer();

	void set_font( const std::string &n );
	void load_default_font();

	const FeFont &get_font() const;
	const std::string &get_name() const { return m_name; };

	void clear_font();

private:
	FeFontContainer( const FeFontContainer & );
	const FeFontContainer &operator=( const FeFontContainer & );

	mutable FeFont m_font;
	std::string m_name;
	mutable bool m_needs_reload;
	std::vector<unsigned char> m_font_binary_data;
};

//
// Container class for our per-monitor settings
//
class FeMonitor : public FePresentableParent
{
public:
	FeMonitor( int num, int w, int h );
	FeMonitor( const FeMonitor & );
	FeMonitor &operator=( const FeMonitor & );

	int get_width();
	int get_height();
	int get_num();

	FeTransform transform;
	Vec2i size;
	int num;
};

class FeStableClock
{
public:
	FeStableClock();
	void start();
	void reset();
	void tick();
	FeTime getElapsedTime();

private:
	FeClock m_real_timer;
	FeTime m_time;
};

class FePresent
{
	friend class FePresentableParent;
	friend class FeVM;

protected:
	enum FromToType
	{
		FromToNoValue=0,
		FromToScreenSaver=1,
		FromToFrontend=2
	};

	FeSettings *m_feSettings;
	FeWindow &m_window;

	const FeFontContainer *m_layoutFont;
	FeFontContainer *m_defaultFont;
	std::string m_layoutFontName;
	SDL_Surface *m_logo_image;
	SDL_Surface *m_logo_full_image;

	FeStableClock m_layout_time;
	FeTime m_layout_time_old;
	float m_frame_time;
	FeTime m_lastInput;

	FeSettings::RotationState m_baseRotation;
	FeSettings::RotationState m_toggleRotation;
	FeTransform m_layout_transform;
	FeTransform m_ui_transform;
	FePerspectiveCamera m_layout_camera;
	float m_3d_ambient_light;
	float m_3d_light;
	float m_3d_light_radius;
	std::string m_3d_cubemap_filename;
	FeTextureContainer *m_3d_cubemap_texture;

	std::vector<FeBaseTextureContainer *> m_texturePool;
	std::vector<FeSound *> m_sounds;
	std::vector<FeMusic *> m_musics;
	std::vector<FeShader *> m_scriptShaders;
	std::vector<FeFontContainer *> m_fontPool;
	std::vector<FeMonitor> m_mon;
	int m_refresh_rate;
	bool m_playMovies;
	int m_user_page_size;
	bool m_preserve_aspect;
	bool m_custom_overlay;
	bool m_mouse_pointer_visible;

	FeListBox *m_listBox; // we only keep this ptr so we can get page sizes
	Vec2i m_layoutSize;
	Vec2f m_layoutScale;

	FeShader *m_emptyShader;

	FeText *m_overlay_caption;
	FeListBox *m_overlay_lb;
	bool m_layout_loaded;
	bool m_layout_has_content;
	unsigned long long m_render_frame_serial;

	FePresent( const FePresent & );
	FePresent &operator=( const FePresent & );

	void toggle_movie();

	void toggle_rotate( FeSettings::RotationState ); // toggle between none and provided state
	FeSettings::RotationState get_actual_rotation();

	// Overrides from base classes:
	//
	void sort_zorder();

	FeImage *add_image(bool a, const std::string &n, float x, float y, float w, float h, FePresentableParent &p);
	FeModel3D *add_model_3d(const std::string &n, FePresentableParent &p);
	FeImage *add_clone(FeImage *, FePresentableParent &p);
	FeModel3D *add_clone(FeModel3D *, FePresentableParent &p);
	FeText *add_text(const std::string &n, int x, int y, int w, int h, FePresentableParent &p);
	FeListBox *add_listbox(int x, int y, int w, int h, FePresentableParent &p);
	FeRectangle *add_rectangle(float x, float y, float w, float h, FePresentableParent &p);
	FeImage *add_surface(float x, float y, int w, int h, FePresentableParent &p);
	FeSound *add_sound(const char *n);
	FeMusic *add_music(const char *n);
	FeShader *add_shader(FeShader::Type type, const char *shader1, const char *shader2);
	FeShader *compile_shader(FeShader::Type type, const char *shader1, const char *shader2);
	float get_layout_width() const;
	float get_layout_height() const;
	int get_base_rotation() const;
	int get_toggle_rotation() const;
	const char *get_display_name() const;
	int get_display_index() const;
	const char *get_filter_name() const;
	int get_filter_index() const;
	void set_filter_index( int );
	int get_current_filter_size() const;
	bool get_clones_list_showing() const;
	Sqrat::Array get_tags_available() const;
	int get_selection_index() const;
	int get_sort_by() const;
	bool get_reverse_order() const;
	int get_list_limit() const;
	void set_search_rule( const char * );
	const char *get_search_rule();
	const char *get_layout_font_name() const;
	bool get_preserve_aspect_ratio();

	void set_selection_index( int );
	void set_layout_width( float );
	void set_layout_height( float );
	float get_perspective_fov() const;
	void set_perspective_fov( float fov );
	float get_perspective_near() const;
	void set_perspective_near( float near_plane );
	float get_perspective_far() const;
	void set_perspective_far( float far_plane );
	float get_perspective_default_z() const;
	void set_perspective_default_z( float z );
	void set_base_rotation( int );
	void set_toggle_rotation( int );
	void set_layout_font_name( const char * );
	void set_preserve_aspect_ratio( bool );
	void reset_scene3d_globals();
	void clear_3d_cubemap_texture();

public:
	static constexpr float SCENE3D_DEFAULT_AMBIENT_LIGHT = 0.0f;
	static constexpr float SCENE3D_DEFAULT_LIGHT = 100.0f;
	static constexpr float SCENE3D_DEFAULT_LIGHT_RADIUS = 1.0f;
	static constexpr float SCENE3D_LIGHT_POWER_SCALE = 2000.0f;
	static constexpr float SCENE3D_LIGHT_RADIUS_SCALE = 0.1f;

	FePresent( FeSettings *fesettings, FeWindow &wnd );
	virtual ~FePresent( void );

	virtual void clear_layout();
	virtual void clear_resources();

	void init_monitors();

	bool load_intro(); // returns false if no intro is available
	void load_screensaver();
	void load_layout( bool initial_load=false );
	void set_layout_loaded( bool loaded ) { m_layout_loaded = loaded; };
	bool is_layout_loaded() { return m_layout_loaded; };
	void set_transforms();

	void update_to( FeTransitionType type, bool reset_display=false );
	void redraw_surfaces();

	bool tick(); // run vm on_tick and update videos.  return true if redraw required
	bool video_tick(); // update videos only. return true if redraw required
	void redraw(); // redraw the screen while doing computationally intensive loops
	void submit_render_frame();

	bool saver_activation_check();
	void on_stop_frontend();
	void pre_run();
	void post_run();
	void build_render_geometry( std::vector<FeRenderGeometry> &geometry ) const;
	void build_render_surface_frames( std::vector<FeRenderSurfaceFrame> &surfaces ) const;

	bool reset_screen_saver();
	bool handle_event( FeInputMap::Command );

	void change_selection( int step, bool end_navigation=true );

	FeSettings *get_fes() const { return m_feSettings; };

	int get_page_size() const;
	void set_page_size( int );

	const FeTransform &get_transform() const;
	const FeTransform &get_ui_transform() const;
	const FePerspectiveCamera &get_layout_camera() const { return m_layout_camera; }
	const FeFont *get_layout_font();
	const FeFont *get_default_font();
	const FeFontContainer *get_default_font_container();
	const SDL_Surface *get_logo_image();
	const SDL_Surface *get_logo_full_image();

	float get_layout_scale_x() const;
	float get_layout_scale_y() const;

	// Get a font from the font pool, loading it if necessary
	const FeFontContainer *get_pooled_font( const std::vector < std::string > &l );
	const FeFontContainer *get_pooled_font( const std::string &n );

	const Vec2i &get_layout_size() const { return m_layoutSize; }
	const Vec2i get_screen_size();
	FeShader *get_empty_shader();

	// Returns true if a script has set custom overlay controls.
	// parameters are set to those controls (which may be NULL!)
	//
	bool get_overlay_custom_controls( FeText *&, FeListBox *& );

	void set_video_play_state( bool state );
	void set_audio_loudness( bool enabled );
	bool get_video_toggle() { return m_playMovies; };

	int get_layout_ms();
	FeTime get_layout_time();
	float get_layout_frame_time();
	int get_refresh_rate();
	float get_3d_ambient_light() const;
	void set_3d_ambient_light( float light );
	float get_3d_light() const;
	void set_3d_light( float light );
	float get_3d_light_radius() const;
	void set_3d_light_radius( float radius );
	const char *get_3d_cubemap_filename() const;
	void set_3d_cubemap_filename( const char *filename );
	const FeBaseTextureContainer *get_3d_cubemap_texture() const;
	bool get_mouse_pointer();
	void set_mouse_pointer( bool );
	FeSdl3GpuContext &get_gpu_context() { return m_window.get_gpu_context(); }
	const FeSdl3GpuContext &get_gpu_context() const { return m_window.get_gpu_context(); }

	//
	// Script static functions
	//
	static FePresent *script_get_fep();
	static void script_do_update( FeBaseTextureContainer *, bool do_update = false );
	static void script_do_update( FeBasePresentable * );
	static void script_register_texture_container( FeBaseTextureContainer * );
	static void script_unregister_texture_container( FeBaseTextureContainer * );
	static void script_flag_redraw();
	static void script_flag_sort_zorder();
	static void script_process_magic_strings( std::string &str,
			int filter_offset,
			int index_offset );
	static std::string script_get_base_path();

	//
	//
	virtual bool on_new_layout()=0;
	virtual bool on_tick()=0;
	virtual void on_transition( FeTransitionType, int var )=0;
	virtual void flag_redraw()=0;
	virtual void flag_sort_zorder()=0;
	virtual void init_with_default_layout()=0;
	virtual int get_script_id()=0;
	virtual void set_script_id( int )=0;
	virtual bool setup_wizard()=0;
};


#endif
