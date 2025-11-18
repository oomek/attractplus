/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-2016 Andrew Mickelson
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

#ifndef FE_CONFIG_HPP
#define FE_CONFIG_HPP

#include <string>
#include <vector>
#include "fe_settings.hpp"
#include "fe_input.hpp"

class FeSettings;
class FeEmulatorInfo;
class FeDisplayInfo;
class FeFilter;
class FeRule;
class FePlugInfo;
class FeLayoutInfo;
class FeScriptConfigurable;

namespace Opt
{
	const int	EDIT        = 1; // option text can be editted by user
	const int	LIST        = 2; // option gets selected from values_list
	const int	INFO        = 3; // option is just for info (no changes)
	const int	MENU        = 4; // option leads to another menu
	const int	RELOAD      = 5; // option reloads menu
	const int	EXIT        = 6; // option results in exiting the menu
	const int	DEFAULTEXIT = 7; // option is default for exiting the menu
	const int	TOGGLE      = 8; // option is a toggle (yes/no)
};

//
// This class is used to set out a single menu option
//
class FeMenuOpt
{
private:
	std::string m_edit_value;	// setting value (edit or unmatched list)
	int m_list_index;	// selected index for list options (-1 if unmatched)

public:
	int type;		// see Opt namespace for values
	bool trigger_reload = false; // this option will trigger a ui reload on change
	bool trigger_colour = false; // special case for menu colour options

	std::string setting;	// the name of the setting
	std::string help_msg;	// the help message for this option
	std::vector<std::string> values_list; // list options

	int opaque;	// private variables available to the menu to track menu options
	std::string opaque_str;

	FeMenuOpt(int t,
		const std::string &set,
		const std::string &val="" );

	FeMenuOpt(int t,
		const std::string &set,
		const std::string &val,
		const std::string &help,
		int opq,
		const std::string &opq_str );

	void set_value( const std::string & );
	void set_value( int );

	const std::string &get_value() const;
	int get_vindex() const;
	const char* get_bool();

	void append_vlist( const char **clist );
	void append_vlist( const std::vector< std::string > &list );
};

//
// This class sets out the variables and functions that are shared between
// the config menus and FeOverlay (which draws them)
//
class FeConfigContext
{
private:
	FeConfigContext( const FeConfigContext & );
	FeConfigContext &operator=( const FeConfigContext & );

public:
	enum Style
	{
		SelectionList,	// A single list
		EditList		// A split option/value list
	};

	FeSettings &fe_settings;
	Style style;
	std::string title;
	std::string help_msg; // temporary help message,displayed until next input

	std::vector<FeMenuOpt> opt_list;	// menu options
	int curr_sel;		// index of currently selected menu option in opt_list
	int default_sel;
	bool save_req; 	// flag whether save() should be called on this menu
	bool update_req;	// flag whether update_to_menu should be called, triggered when is_function changes config

	FeConfigContext( FeSettings & );


	// Add menu option. Help always gets translated.
	FeMenuOpt *add_opt(
		const int type,
		const std::string &label,
		const std::string &value,
		const std::string &help="",
		const int &opaque=0
	);

	// Add menu toggle option.
	FeMenuOpt *add_opt(
		const int type,
		const std::string &label,
		const bool &value,
		const std::string &help="",
		const int &opaque=0
	);

	// set the Style and title for the menu.
	void set_style( Style s, const std::string &t );

	// Return curr_sel option
	FeMenuOpt &curr_opt() { return opt_list[curr_sel]; }

	// Return last option in opt_list; }
	FeMenuOpt &back_opt() { return opt_list.back(); }

	//
	// Dialog Toolbox.  "msg" strings get translated
	//
	virtual bool edit_dialog( const std::string &msg, std::string &text )=0;

	virtual bool confirm_dialog( const std::string &msg )=0;

	virtual int option_dialog( const std::string &title,
		const std::vector < std::string > &options,
		int default_sel=0 )=0;

	virtual void splash_message( const std::string &msg,
		const std::string &aux = "" )=0;

	virtual void input_map_dialog( const std::string &msg,
		FeInputMapEntry &res,
		FeInputMap::Command &conflict )=0;

	virtual void tags_dialog()=0;

	virtual bool check_for_cancel()=0;
};

class FeBaseConfigMenu
{
protected:
	FeBaseConfigMenu();
	FeBaseConfigMenu( const FeBaseConfigMenu & );
	FeBaseConfigMenu &operator=( const FeBaseConfigMenu & );

public:
	// Called to populate the list of options for this menu (ctx.opt_list)
	// and to set the style and title
	//
	virtual void get_options( FeConfigContext &ctx );

	// Called when a menu option is selected
	// Return true to proceed with option, false to cancel
	//
	virtual bool on_option_select( FeConfigContext &ctx,
		FeBaseConfigMenu *& submenu );


	// Called when leaving the menu if ctx.save_req is true (it is either
	// set to true in on_option_select above or when an EDIT or LIST option
	// is changed
	// Return true if save required on parent menu
	//
	virtual bool save( FeConfigContext &ctx );

	// When true will cause display_config_dialog to save and exit on every change
	// Used to create "live" menus, such as Layout Options
	// main.cpp must then re-display the menu until user explicitly exits
	//
	bool exit_on_change = false;

	// Holds the last selection from layout_options_dialog (excluding exit)
	// Can be used to re-enter the menu at the same option
	int last_sel = 0;
};

// Utility class where script parameters are being configured
class FeScriptConfigMenu : public FeBaseConfigMenu
{
protected:
	FeScriptConfigMenu();

	virtual bool save( FeConfigContext &ctx )=0;

	bool on_option_select(
		FeConfigContext &ctx, FeBaseConfigMenu *& submenu );

	bool save_helper( FeConfigContext &ctx, int first_idx=0 );

	FeSettings::FePresentState m_state;
	int m_script_id;
	FeScriptConfigurable *m_configurable;
	FeScriptConfigurable *m_per_display;
	std::string m_file_path;
	std::string m_file_name;
};

class FeLayoutEditMenu : public FeScriptConfigMenu
{
private:
	FeLayoutInfo *m_layout;
	FeDisplayInfo *m_display;

public:
	FeLayoutEditMenu();
	void get_options( FeConfigContext &ctx );

	bool save( FeConfigContext &ctx );
	void set_layout( FeLayoutInfo *layout, FeScriptConfigurable *per_display, FeDisplayInfo *display );
};

class FeIntroEditMenu : public FeScriptConfigMenu
{
public:
	void get_options( FeConfigContext &ctx );

	bool save( FeConfigContext &ctx );
};

class FeSaverEditMenu : public FeScriptConfigMenu
{
public:
	void get_options( FeConfigContext &ctx );

	bool save( FeConfigContext &ctx );
};

class FeEmuArtEditMenu : public FeBaseConfigMenu
{
private:
	std::string m_art_name;
	FeEmulatorInfo *m_emulator;

public:
	FeEmuArtEditMenu();

	void get_options( FeConfigContext &ctx );

	bool save( FeConfigContext &ctx );
	void set_art( FeEmulatorInfo *emu, const std::string &art_name );
};

class FeEmulatorEditMenu : public FeBaseConfigMenu
{
private:
	FeEmuArtEditMenu m_art_menu;
	FeEmulatorInfo *m_emulator;
	bool m_is_new;
	bool m_romlist_exists;
	bool m_parent_save;

public:
	FeEmulatorEditMenu();

	void get_options( FeConfigContext &ctx );
	bool on_option_select( FeConfigContext &ctx,
		FeBaseConfigMenu *& submenu );
	bool save( FeConfigContext &ctx );

	void set_emulator( FeEmulatorInfo *emu, bool is_new,
		const std::string &romlist_dir );
};

class FeEmulatorGenMenu : public FeBaseConfigMenu
{
private:
	std::string m_default_name;

public:
	void get_options( FeConfigContext &ctx );
	bool on_option_select( FeConfigContext &ctx,
		FeBaseConfigMenu *& submenu );
};

class FeEmulatorSelMenu : public FeBaseConfigMenu
{
private:
	FeEmulatorEditMenu m_edit_menu;
	FeEmulatorGenMenu m_gen_menu;

public:
	void get_options( FeConfigContext &ctx );
	bool on_option_select( FeConfigContext &ctx,
		FeBaseConfigMenu *& submenu );
};

class FeRuleEditMenu : public FeBaseConfigMenu
{
private:
	FeFilter *m_filter;
	int m_index;

public:
	FeRuleEditMenu();

	void get_options( FeConfigContext &ctx );
	bool on_option_select( FeConfigContext &ctx,
		FeBaseConfigMenu *& submenu );
	bool save( FeConfigContext &ctx );
	void set_rule_index( FeFilter *f, int i );
};

class FeFilterEditMenu : public FeBaseConfigMenu
{
private:
	FeDisplayInfo *m_display;
	int m_index;
	FeRuleEditMenu m_rule_menu;

public:
	FeFilterEditMenu();

	void get_options( FeConfigContext &ctx );
	bool on_option_select( FeConfigContext &ctx,
		FeBaseConfigMenu *& submenu );
	bool save( FeConfigContext &ctx );
	void set_filter_index( FeDisplayInfo *d, int i );
};

class FeDisplayEditMenu : public FeBaseConfigMenu
{
private:
	FeFilterEditMenu m_filter_menu;
	FeLayoutEditMenu m_layout_menu;
	int m_index; // the index for m_display in FeSettings' master list

public:
	FeDisplayEditMenu();

	virtual FeDisplayInfo* get_display( FeConfigContext &ctx );
	virtual void get_options( FeConfigContext &ctx );
	virtual bool save( FeConfigContext &ctx );

	void add_options( FeConfigContext &ctx, bool isDefault = false );
	bool on_option_select( FeConfigContext &ctx, FeBaseConfigMenu *& submenu );
	void set_display_index( int index );
};

class FeDisplayDefaultMenu : public FeDisplayEditMenu
{
public:
	FeDisplayInfo* get_display( FeConfigContext &ctx ) override;
	void get_options( FeConfigContext &ctx ) override;
	bool save( FeConfigContext &ctx ) override;
};

class FeDisplayMenuEditMenu : public FeBaseConfigMenu
{
private:
	FeLayoutEditMenu m_layout_menu;
public:
	void get_options( FeConfigContext &ctx );
	bool on_option_select( FeConfigContext &ctx,
		FeBaseConfigMenu *& submenu );
	bool save( FeConfigContext &ctx );
};

class FeDisplaySelMenu : public FeBaseConfigMenu
{
private:
	FeDisplayEditMenu m_edit_menu;
	FeDisplayDefaultMenu m_default_menu;
	FeDisplayMenuEditMenu m_menu_menu;

public:
	void get_options( FeConfigContext &ctx );
	bool on_option_select( FeConfigContext &ctx,
		FeBaseConfigMenu *& submenu );
};

class FeMapping;

class FeInputEditMenu : public FeBaseConfigMenu
{
private:
	FeMapping *m_mapping;

public:
	FeInputEditMenu();
	void get_options( FeConfigContext &ctx );
	bool on_option_select( FeConfigContext &ctx,
		FeBaseConfigMenu *& submenu );
	bool save( FeConfigContext &ctx );
	void set_mapping( FeMapping * );
};

class FeInputJoysticksMenu : public FeBaseConfigMenu
{
	void get_options( FeConfigContext &ctx );
	bool save( FeConfigContext &ctx );
};

class FeInputSelMenu : public FeBaseConfigMenu
{
private:
	std::vector<FeMapping> m_mappings;
	FeInputEditMenu m_edit_menu;
	FeInputJoysticksMenu m_joysticks_menu;

public:
	void get_options( FeConfigContext &ctx );
	bool on_option_select( FeConfigContext &ctx,
		FeBaseConfigMenu *& submenu );
	bool save( FeConfigContext &ctx );
};

class FeSoundMenu : public FeBaseConfigMenu
{
public:
	void get_options( FeConfigContext &ctx );
	bool save( FeConfigContext &ctx );
};

class FeMiscMenu : public FeBaseConfigMenu
{
	std::vector<FeLanguage> m_languages;

public:
	void get_options( FeConfigContext &ctx );
	bool save( FeConfigContext &ctx );
};

class FeScraperMenu : public FeBaseConfigMenu
{
public:
	void get_options( FeConfigContext &ctx );
	bool save( FeConfigContext &ctx );
};

class FePluginEditMenu : public FeScriptConfigMenu
{
private:
	FePlugInfo *m_plugin;

public:
	FePluginEditMenu();
	void get_options( FeConfigContext &ctx );

	bool save( FeConfigContext &ctx );
	void set_plugin( FePlugInfo *plugin, int index );
};

class FePluginSelMenu : public FeBaseConfigMenu
{
private:
	FePluginEditMenu m_edit_menu;

public:
	void get_options( FeConfigContext &ctx );
	bool on_option_select( FeConfigContext &ctx,
		FeBaseConfigMenu *& submenu );
};

class FeConfigMenu : public FeBaseConfigMenu
{
private:
	FeEmulatorSelMenu m_emu_menu;
	FeDisplaySelMenu m_list_menu;
	FeInputSelMenu m_input_menu;
	FeSoundMenu m_sound_menu;
	FeIntroEditMenu m_intro_menu;
	FeSaverEditMenu m_saver_menu;
	FePluginSelMenu m_plugin_menu;
	FeScraperMenu m_scraper_menu;
	FeMiscMenu m_misc_menu;

public:
	void get_options( FeConfigContext &ctx );
	bool on_option_select( FeConfigContext &ctx,
		FeBaseConfigMenu *& submenu );
	bool save( FeConfigContext &ctx );
};

class FeEditGameMenu : public FeBaseConfigMenu
{
private:
	FeRomInfo m_rom_original;
	bool m_update_rl;
	bool m_update_stats;
	bool m_update_extras;
	bool m_update_overview;

public:
	FeEditGameMenu();

	void get_options( FeConfigContext &ctx );
	bool on_option_select( FeConfigContext &ctx,
		FeBaseConfigMenu *& submenu );

	bool save( FeConfigContext &ctx );
};

class FeEditShortcutMenu : public FeBaseConfigMenu
{
private:
	FeRomInfo m_rom_original;
	bool m_update_rl;

public:
	FeEditShortcutMenu();

	void get_options( FeConfigContext &ctx );
	bool on_option_select( FeConfigContext &ctx,
		FeBaseConfigMenu *& submenu );

	bool save( FeConfigContext &ctx );
};


#endif
