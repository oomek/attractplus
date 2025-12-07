/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-16 Andrew Mickelson
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

#ifndef FE_SETTINGS_HPP
#define FE_SETTINGS_HPP

#include "fe_base.hpp"
#include "fe_info.hpp"
#include "fe_romlist.hpp"
#include "fe_input.hpp"
#include "fe_util.hpp"
#include "scraper_base.hpp"
#include "path_cache.hpp"
#include <deque>

#if defined(USE_DRM)
 #define FORCE_FULLSCREEN
#endif
#if defined(USE_DRM) || defined(SFML_SYSTEM_MACOS) || ( defined(SFML_SYSTEM_LINUX) && !defined(USE_XINERAMA) )
 #define NO_MULTIMON
#endif

extern const char *FE_ART_EXTENSIONS[];

extern const char *FE_CFG_FILE;
extern const char *FE_WINDOW_FILE;
extern const char *FE_CFG_SUBDIR;
extern const char *FE_SCRIPT_NV_FILE;
extern const char *FE_LAYOUT_NV_FILE;

extern const char *FE_SCRAPER_SUBDIR;
extern const char *FE_LAYOUT_FILE_BASE;
extern const char *FE_LAYOUT_FILE_EXTENSION;

extern const char *FE_CFG_YES_STR;
extern const char *FE_CFG_NO_STR;

extern const int FE_DEFAULT_UI_COLOR_TOKEN;

class FeOverlay;

// A container for each task when importing/building romlists from the command line
class FeImportTask
{
public:

	enum TaskType
	{
		BuildRomlist,
		ImportRomlist,
		ScrapeArtwork
	};

	FeImportTask( TaskType t, const std::string &en, const std::string &fn="" )
		: task_type( t ), emulator_name( en ), file_name( fn )
	{
	};

	TaskType task_type;
	std::string emulator_name;
	std::string file_name;
};

class FeLanguage
{
public:
	FeLanguage(
		const std::string &language,
		const std::string &label
	);

	std::string language;
	std::string label;
};

class FeSettings : public FeBaseConfigurable
{
public:
	enum RotationState { RotateNone=0, RotateRight, RotateFlip, RotateLeft };
	static const char *screenRotationTokens[];
	static const char *screenRotationDispTokens[];
	static const char *antialiasingTokens[];
	static const char *antialiasingDispTokens[];
	static const char *anisotropicTokens[];
	static const char *anisotropicDispTokens[];

	enum FePresentState
	{
		Intro_Showing,
		Layout_Showing,
		ScreenSaver_Showing
	};

	enum WindowType { Fillscreen=0, Fullscreen, Window, WindowNoBorder };
	static const char *windowModeTokens[];
	static const char *windowModeDispTokens[];

	enum FilterWrapModeType { WrapWithinDisplay=0, JumpToNextDisplay, NoWrap };
	static const char *filterWrapTokens[];
	static const char *filterWrapDispTokens[];

	enum StartupModeType { ShowLastSelection=0, ShowRandomSelection, ShowDisplaysMenu, LaunchLastGame };
	static const char *startupTokens[];
	static const char *startupDispTokens[];

	enum PrefixModeType { NoPrefix=0, SortPrefix, SortAndShowPrefix };
	static const char *prefixTokens[];
	static const char *prefixDispTokens[];

	static std::vector<std::string> uiColorTokens;
	static std::vector<std::string> uiColorDispTokens;
	void load_layout_params();

	// These values must align with `FeSettings::configSettingStrings`
	enum ConfigSettingIndex
	{
		Language=0,
		CustomLanguages,
		ExitCommand,
		ExitMessage,
		UIFontSize,
		UIColor,
		ScreenSaverTimeout,
		DisplaysMenuExit,
		HideBrackets,
		GroupClones,
		StartupMode,
		PrefixMode,
		QuickMenu,
		ConfirmFavourites,
		ConfirmExit,
		MouseThreshold,
		JoystickThreshold,
		WindowMode,
		ScreenRotation,
		AntiAliasing,
		Anisotropic,
		FilterWrapMode,
		LayoutPreview,
		CustomOverlay,
		TrackUsage,
		MultiMon,
		SmoothImages,
		SelectionMaxStep,
		SelectionDelay,
		SelectionSpeed,
		MoveMouseOnLaunch,
		ScrapeSnaps,
		ScrapeMarquees,
		ScrapeFlyers,
		ScrapeWheels,
		ScrapeFanArt,
		ScrapeVids,
		ScrapeOverview,
		ThegamesdbKey,
		PowerSaving,
		CheckForUpdates,
#ifdef SFML_SYSTEM_WINDOWS
		HideConsole,
#endif
		VideoDecoder,
		MenuPrompt, // 'Displays Menu' prompt
		MenuLayout, // 'Displays Menu' layout
		ImageCacheMBytes,
		LAST_INDEX
	};

	static const char *configSettingStrings[];
	static const char *otherSettingStrings[];

	enum GameExtra
	{
		Executable =0, // custom executable to override the configured emulator executable
		Arguments      // custom arguments to override the configured emulator arguments
	};

private:
	std::string m_config_path;
	std::string m_exit_command;
	std::string m_exit_message;
	std::string m_exit_question;
	std::string m_language;
	std::string m_current_search_str;

	std::string m_menu_prompt;		// 'Displays Menu" prompt
	std::string m_menu_layout;		// 'Displays Menu' layout.  if blank, use built-in menu
	std::string m_menu_layout_file;		// 'Displays Menu" toggled layout file

	std::string m_last_game_overview_path;  // cache the last loaded game overview path
	std::string m_last_game_overview_text;  // cache the last loaded game overview text

	// configured API key to use for for thegamesdb.net scraping.  If blank, AM's standard public key is used
	std::string m_tgdb_key;

	std::string m_default_layout;
	std::string m_default_romlist;
	FeDisplayInfo m_default_display;
	std::vector<FeDisplayInfo> m_displays;
	std::vector<FePlugInfo> m_plugins;
	std::vector<FeLayoutInfo> m_layout_params;
	std::vector<FeRomInfo *> m_current_search;
	std::vector<int> m_display_cycle; // display indices to show in cycle
	std::vector<int> m_display_menu; // display indices to show in menu
	std::map<GameExtra,std::string> m_game_extras; // "extra" rom settings for the current rom
	std::deque<int> m_display_stack; // stack for displays to navigate to when "back" button pressed (and
					// display shortcuts are used)
	FeRomList m_rl;
	FePathCache m_path_cache;

	FeInputMap m_inputmap;
	FeSoundInfo m_sounds;
	FeTranslationMap m_translation_map;
	FeLayoutInfo m_saver_params;
	FeLayoutInfo m_intro_params;
	FeLayoutInfo m_current_layout_params; // copy of current layout params (w/ per display params as well)
	FeLayoutInfo m_display_menu_per_display_params; // stores only the 'per_display' params for the display menu
	sf::IntRect m_mousecap_rect;
	sf::RenderWindow *m_rwnd;

	int m_current_display; // The index of the current display, -1 if showing a custom displays_menu
	int m_selected_display; // The index of the displays_menu selected display, -1 if EXIT is selected. Equals m_current_display if displays_menu not shown.
	int m_actual_display_index; // The most recent actual display index that was selected (for saving)
	FeBaseConfigurable *m_current_config_object;
	int m_ssaver_time;
	std::string m_last_plugin_name;
	int m_last_launch_display;
	int m_last_launch_filter;
	int m_last_launch_rom;
	int m_last_launch_clone; // only used if group_clones is on.  tracks the index of the clone that is launched
	int m_clone_index; // >= 0 if a clone group is being shown, otherwise -1
	int m_joy_thresh;		// [1..100], 100=least sensitive
	int m_mouse_thresh;	// [1..100], 100=least sensitive
	int m_current_search_index; // used when custom searching
	bool m_custom_languages;
	bool m_displays_menu_exit;
	bool m_hide_brackets;
	bool m_group_clones;
	StartupModeType m_startup_mode;
	PrefixModeType m_prefix_mode;
	bool m_confirm_favs;
	bool m_confirm_exit;
	bool m_layout_preview;
	bool m_custom_overlay;
	bool m_quick_menu;
	bool m_track_usage;
	bool m_multimon;
	WindowType m_window_mode;
	bool m_smooth_images;
	FilterWrapModeType m_filter_wrap_mode;
	int m_selection_max_step; // max selection acceleration step.  0 to disable accel
	int m_selection_delay; // delay before key-repeat
	int m_selection_speed; // key-repeat interval
	int m_image_cache_mbytes; // image cache size (in Megabytes)
	bool m_move_mouse_on_launch; // configure whether mouse gets moved to bottom right corner on launch
	bool m_scrape_snaps;
	bool m_scrape_marquees;
	bool m_scrape_flyers;
	bool m_scrape_wheels;
	bool m_scrape_fanart;
	bool m_scrape_vids;
	bool m_scrape_overview;
#ifdef SFML_SYSTEM_WINDOWS
	bool m_hide_console;
#endif
	bool m_power_saving;
	bool m_check_for_updates;
	RotationState m_screen_rotation;
	int m_antialiasing;
	int m_anisotropic;
	bool m_loaded_game_extras;
	enum FePresentState m_present_state;
	int m_ui_font_size;
	std::string m_ui_color;
	bool m_window_topmost;
	bool m_split_config_format;

	FeSettings( const FeSettings & );
	FeSettings &operator=( const FeSettings & );

	int process_setting( const std::string &,
		const std::string &,
		const std::string & );

	void load_state();
	void clear();
	void load_displays_configs();
	void save_displays_configs() const;
	void load_plugins_configs();
	void save_plugins_configs() const;
	void load_layouts_configs();
	void save_layouts_configs() const;

	void construct_display_maps();

	void internal_gather_config_files(
		std::vector<std::string> &ll,
		const std::string &extension,
		const char *subdir ) const;

	void internal_load_language( const std::string &lang );

	bool internal_get_best_artwork_file(
		const FeRomInfo &rom,
		const std::string &art_name,
		std::vector<std::string> &vid_list,
		std::vector<std::string> &image_list,
		bool image_only,
		bool ignore_emu );

	bool simple_scraper( FeImporterContext &, const char *, const char *, const char *, const char *, bool = false );
	bool general_mame_scraper( FeImporterContext & );
	bool thegamesdb_scraper( FeImporterContext & );
	bool apply_xml_import( FeImporterContext & );

	bool load_game_extras(
		const std::string &romlist_name,
		const std::string &romname,
		std::map<GameExtra,std::string> &extras );

	void save_game_extras(
		const std::string &romlist_name,
		const std::string &romname,
		const std::map<GameExtra,std::string> &extras );

	// sets path to filename for specified emu and romname.  Returns true if file exists, false otherwise
	bool get_game_overview_filepath( const std::string &emu, const std::string &romname, std::string &path );

public:
	FeSettings( const std::string &config_dir );

	void load();
	void save_state();
	void migration_cleanup_dialog( FeOverlay *overlay );

	FeInputMap::Command map_input( const std::optional<sf::Event> &e );
	void reset_input();

	void get_input_config_metrics( sf::IntRect &mousecap_rect, int &joy_thresh );
	FeInputMap::Command input_conflict_check( const FeInputMapEntry &e );

	// for use with Up, Down, Left, Right, Back commands to get what they are actually mapped to
	FeInputMap::Command get_default_command( FeInputMap::Command );
	void set_default_command( FeInputMap::Command c, FeInputMap::Command v );

	bool get_current_state( FeInputMap::Command c );
	bool get_key_state( std::string key );
	void get_input_mappings( std::vector < FeMapping > &l ) const { m_inputmap.get_mappings( l ); };
	void set_input_mapping( FeMapping &m ) { m_inputmap.set_mapping( m ); };

	void on_joystick_connect() { m_inputmap.on_joystick_connect(); };
	std::vector < std::pair< int, std::string > > &get_joy_config() { return m_inputmap.get_joy_config(); };

	void set_volume( FeSoundInfo::SoundType, const std::string & );
	int get_set_volume( FeSoundInfo::SoundType ) const;
	int get_play_volume( FeSoundInfo::SoundType ) const;
	bool get_mute() const;
	void set_mute( bool );
	bool get_loudness() const;
	void set_loudness( bool );
	bool get_sound_file( FeInputMap::Command, std::string &s, bool full_path=true ) const;
	void set_sound_file( FeInputMap::Command, const std::string &s );
	void get_sounds_list( std::vector < std::string > &ll ) const;

	void step_current_selection( int step );
	void set_current_selection( int filter_index, int rom_index ); // use rom_index<0 to only change the filter

	//////////////////////
	//
	// Set the "display" that will be shown to the one at the specified index
	//
	//////////////////////
	//
	// index=-1, stack_previous=false is a special case, used to show the "displays menu"
	// when a custom layout is being used
	//
	// If "stack_previous" is true, the currently shown display is added to the display
	// stack (so that if the user presses the back button later the fe will navigate back
	// to the earlier display).  If index is -1 when stack_previous is true, then the
	// display will be moved to the display at the top of the display stack (if there is one)
	//
	// Returns true if the display change results in a new layout, false otherwise
	//
	bool set_display( int index, bool stack_previous=false );
	void init_display();

	// Return true if there are displays available to navigate back to on a "back" button press
	//
	bool back_displays_available() { return !m_display_stack.empty(); };

	int get_current_display_index() const;
	int get_actual_display_index() const;
	int get_selected_display_index() const;
	int get_display_index_from_name( const std::string &name ) const;
	int displays_count() const;

	bool navigate_display( int step, bool wrap_mode=false );
	bool navigate_filter( int step );

	int get_filter_index_from_name( const std::string &name ) const;
	int get_current_filter_index() const;
	const std::string &get_filter_name( int filter_index );
	void get_current_display_filter_names( std::vector<std::string> &list ) const;
	int get_filter_index_from_offset( int offset ) const;
	int get_filter_size( int filter_index ) const;
	int get_filter_count() const;

	// apply a search rule (see FeRule) to the currently selected
	// filter
	//
	// set to an empty string to clear the current search
	//
	void set_search_rule( const std::string &rule );
	const std::string &get_search_rule() const;

	bool switch_to_clone_group( int index = -1 ); // set index to the index of the clone to select.  -1=default
	bool switch_from_clone_group();
	int get_clone_index();

	bool select_last_launch( bool initial_load = false );
	bool is_last_launch( int filter_offset, int index_offset );
	int get_joy_thresh() const { return m_joy_thresh; }
	void init_mouse_capture( sf::RenderWindow *window );
	bool has_mouse_moves() const { return m_inputmap.has_mouse_moves(); }
	bool test_mouse_wrap() const;
	void wrap_mouse();

	// prepares for emulator launch by setting various tracking variables (last launch, etc)
	// and determining the correct executable, command line parameters and working directory to use
	void prep_for_launch(
		std::string &command,
		std::string &args,
		std::string &work_dir,
		FeEmulatorInfo *&emu );

	int exit_command() const; // run configured exit command (if any)
	void get_exit_message( std::string &exit_message ) const;
	void get_exit_question( std::string &exit_question ) const;

	void toggle_layout();
	void set_current_layout_file( const std::string &layout_file );

	int get_rom_index( int filter_index, int offset ) const;

	void do_text_substitutions( std::string &str, int filter_offset, int index_offset );
	void do_text_substitutions_absolute( std::string &str, int filter_index, int rom_index );
	bool get_token_value( std::string &token, int filter_index, int rom_index, std::string &value );
	bool get_special_token_value( std::string &token, int filter_index, int rom_index, std::string &value );

	void get_current_sort( FeRomInfo::Index &idx, bool &rev, int &limit );

	const std::string &get_current_display_title() const;
	const std::string &get_rom_info( int filter_offset, int rom_offset, FeRomInfo::Index index );
	const std::string &get_rom_info_absolute( int filter_index, int rom_index, FeRomInfo::Index index );
	FeRomInfo *get_rom_offset( int filter_offset, int rom_offset );
	FeRomInfo *get_rom_absolute( int filter_index, int rom_index );

	int selection_delay() const { return m_selection_delay; }
	int selection_speed() const { return m_selection_speed; }
	int selection_max_step() const { return m_selection_max_step; }

	// get a list of available plugins
	void get_available_plugins( std::vector < std::string > &list ) const;
	std::vector<FePlugInfo> &get_plugins() { return m_plugins; }
	void get_plugin( const std::string &label, FePlugInfo *&plug, int &index );
	bool get_plugin_enabled( const std::string &label ) const;
	void get_plugin_full_path( const std::string &label, std::string &path, std::string &filename ) const;
	void get_plugin_full_path( int id, std::string &path ) const;
	bool get_last_plugin( FePlugInfo *&plug, int &index );
	void set_last_plugin( FePlugInfo *plug );

	//
	FePresentState get_present_state() const { return m_present_state; };
	void set_present_state( FePresentState s ) { m_present_state=s; };

	int ui_font_size() const { return m_ui_font_size; }
	void set_window_topmost( bool window_topmost ) { m_window_topmost = window_topmost; }
	bool get_window_topmost() const { return m_window_topmost; }

#ifdef SFML_SYSTEM_WINDOWS
	bool get_hide_console() const { return m_hide_console; };
#endif

	enum FePathType
	{
		Current, // could be Layout, ScreenSaver or Intro depending on
			// m_present_state
		Layout,
		ScreenSaver,
		Intro,
		Loader
	};

	bool get_path( FePathType t,
		std::string &path,
		std::string &filename ) const;

	bool get_path( FePathType t,
		std::string &path ) const;

	FeLayoutInfo &get_current_config( FePathType t );

	bool get_layout_dir( const std::string &layout_name, std::string &layout_dir ) const;
	void get_layouts_list( std::vector<std::string> &layouts ) const;
	FeLayoutInfo &get_layout_config( const std::string &layout_name );
	FeScriptConfigurable &get_display_menu_per_display_params() { return m_display_menu_per_display_params; };

	void get_best_artwork_file(
		const FeRomInfo &rom,
		const std::string &art_name,
		std::vector<std::string> &vid_list,
		std::vector<std::string> &image_list,
		bool image_only );

	bool has_artwork( const FeRomInfo &rom, const std::string &art_name );
	bool has_video_artwork( const FeRomInfo &rom, const std::string &art_name );
	bool has_image_artwork( const FeRomInfo &rom, const std::string &art_name );

	bool get_best_dynamic_image_file(
		int filter_index,
		int rom_index,
		const std::string &logo_str,
		std::vector<std::string> &vid_list,
		std::vector<std::string> &image_list );

	bool get_module_path( const std::string &module,
		std::string &module_dir,
		std::string &module_file ) const;

	const std::string &get_config_dir() const;
	bool config_file_exists() const;

	// [out] fpath = if font is in a zip file, this is the path to the zip
	// [out] ffile = font file to open
	// [in] fontname = name of font to find.  If empty return default font
	bool get_font_file(
		std::string &ffile,
		const std::string &fontname="" ) const;

	bool get_multimon() const;
	WindowType get_window_mode() const;
	RotationState get_screen_rotation() const;
	int get_antialiasing() const;
	int get_anisotropic() const;
	FilterWrapModeType get_filter_wrap_mode() const;
	StartupModeType get_startup_mode() const;
	PrefixModeType get_prefix_mode() const;
	std::string get_ui_color() const;
	int get_screen_saver_timeout() const;

	bool get_current_fav();

	// returns true if the current list changed as a result of setting the tag
	bool set_fav_offset( bool state, int filter_offset, int rom_offset );
	bool set_fav_absolute( bool state, int filter_index, int rom_index );
	bool set_fav_current( bool state );
	int get_prev_fav_offset();
	int get_next_fav_offset();

	int get_next_letter_offset( int step );

	std::vector<std::string> get_tags_available();
	void get_current_tags_list( std::vector< std::pair<std::string, bool> > &tags_list );

	// returns true if the current list changed as a result of setting the tag
	bool set_tag_offset( const std::string &tag, bool add_tag, int filter_offset, int rom_offset );
	bool set_tag_absolute( const std::string &tag, bool add_tag, int filter_index, int rom_index );
	bool set_tag_current( const std::string &tag, bool add_tag );

	bool replace_tags_offset( const std::string &tags, int filter_offset, int rom_offset );
	bool replace_tags_absolute( const std::string &tags, int filter_index, int rom_index );

	//
	// This function implements command-line romlist generation/imports
	// If output_name is empty, then a non-existing filename is chosen for
	// the resulting romlist file
	//
	bool build_romlist( const std::vector< FeImportTask > &task_list,
		const std::string &output_name,
		FeFilter &filter,
		bool full );

	//
	// Save an updated rom in the current romlist file (used with "Game Options" command)
	// original is assumed to be the currently selected rom
	//
	enum UpdateType { UpdateEntry, EraseEntry, InsertEntry };

	bool update_romlist_after_edit(
		const FeRomInfo &original,		// original rom value
		const FeRomInfo &replacement,	// new rom value
		UpdateType u_type=UpdateEntry	// update type
	);

	bool update_romlist_entry(
		const FeRomInfo &original,		// original rom value
		const FeRomInfo &replacement,	// new rom value
		UpdateType u_type=UpdateEntry	// update type
	);

	bool update_romlist_file(
		const std::string &romlist_name,// name of romlist to update
		const FeRomInfo &original,		// original rom value
		const FeRomInfo &replacement,	// new rom value
		UpdateType u_type=UpdateEntry	// update type
	);

	bool write_romlist(
		const std::string &filename,
		const FeRomInfoListType &romlist
	);

	// Returns true if the stats update may have altered the current filters
	bool update_stats_offset( int count_incr, int time_incr, int filter_offset, int rom_offset );
	bool update_stats_absolute( int count_incr, int time_incr, int filter_index, int rom_index );
	bool update_stats_current( int count_incr, int time_incr );

	//
	// The frontend maintains extra per game settings/extra info
	//
	// This info is only ever loaded for the currently selected game, and is not intended
	// to be used in a filter
	//
	const std::string &get_game_extra( GameExtra id );
	void set_game_extra( GameExtra id, const std::string &value );
	void save_game_extras();

	bool get_game_overview_absolute( int filter_index, int rom_index, std::string &overview );

	// only overwrites an existing file if overwrite = true
	void set_game_overview( const std::string &emu, const std::string &romname, const std::string &overview, bool overwrite );

	// This function implements the config-mode romlist generation
	// A romlist named "<emu_name>.txt" is created in the romlist dir,
	// overwriting any previous list of this name.
	//
	// Returns false if cancelled by the user
	//
	bool build_romlist( const std::vector < std::string > &emu_name, const std::string &out_filename,
		UiUpdate, void *, std::string &, bool use_net=true );
	bool scrape_artwork( const std::string &emu_name, UiUpdate uiu, void *uid, std::string &msg );

	FeEmulatorInfo *get_emulator( const std::string & );
	FeEmulatorInfo *create_emulator( const std::string &, const std::string & );
	void delete_emulator( const std::string & );

	void get_list_of_emulators( std::vector<std::string> &emu_list, bool get_templates=false );

	//
	// Functions used for configuration
	//
	const std::string get_info( int index ) const; // see "ConfigSettingIndex"
	bool get_info_bool( int index ) const;
	bool set_info( int index, const std::string & );

	bool has_custom_displays_menu();
	void get_displays_menu( std::string &title,
		std::vector<std::string> &names,
		std::vector<int> &indices,
		int &current_idx ) const;

	bool load_default_display();
	bool save_default_display();
	bool apply_default_display( FeDisplayInfo &display );
	FeDisplayInfo *get_default_display();

	std::vector<FeDisplayInfo> *get_displays();
	FeDisplayInfo *get_display( int index );
	FeDisplayInfo *create_display( const std::string &n );
	void delete_display( int index );
	bool check_duplicate_display_name( const std::string &name, int exclude_index = -1 ) const;

	void create_filter( FeDisplayInfo &l, const std::string &name ) const;

	// return true if specified romlist name is configured for use as a display
	// list
	bool check_romlist_configured( const std::string &n ) const;
	void get_romlists_list( std::vector < std::string > &ll ) const;

	void save() const;

	void set_language( const std::string &l );
	void get_languages_list( std::vector < FeLanguage > &ll ) const;

	bool get_emulator_setup_script( std::string &path, std::string &file );

	// Utility function to get a list of layout*.nut files from the specified path...
	static void get_layout_file_basenames_from_path(
		const std::string &path,
		std::vector<std::string> &names_list );
};

inline bool is_windowed_mode( int m )
{
	return (( m == FeSettings::Window ) || ( m == FeSettings::WindowNoBorder ));
}

bool art_exists( const std::string &path, const std::string &base );

#endif
