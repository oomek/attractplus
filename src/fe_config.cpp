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

#include "fe_config.hpp"
#include "fe_info.hpp"
#include "fe_settings.hpp"
#include "fe_util.hpp"
#include "fe_vm.hpp"
#include "fe_cache.hpp"

#include <SFML/Graphics/Shader.hpp>
#ifndef NO_MOVIE
#include "media.hpp"
#endif

#include <iostream>
#include <cmath>
#include <algorithm>
#include "nowide/fstream.hpp"

FeMenuOpt::FeMenuOpt( int t, const std::string &set, const std::string &val )
	: m_list_index( -1 ),
	type( t ),
	setting( set ),
	opaque( 0 )
{
	if ( !val.empty() )
		set_value( val );
}

FeMenuOpt::FeMenuOpt(int t,
		const std::string &set,
		const std::string &val,
		const std::string &help,
		int opq,
		const std::string &opq_str )
	: m_list_index( -1 ),
	type( t ),
	setting( set ),
	help_msg( help ),
	opaque( opq ),
	opaque_str( opq_str )
{
	if ( !val.empty() )
		set_value( val );
}

const std::string &FeMenuOpt::get_value() const
{
	if ( m_list_index == -1 )
		return m_edit_value;
	else
		return values_list[ m_list_index ];
}

int FeMenuOpt::get_vindex() const
{
	return m_list_index;
}

// Return standardized "yes" or "no" string depending on list index
const char* FeMenuOpt::get_bool()
{
	return ( m_list_index == 0 ) ? FE_CFG_YES_STR : FE_CFG_NO_STR;
}

void FeMenuOpt::set_value( const std::string &s )
{
	if ( !values_list.empty() )
	{
		for ( unsigned int i=0; i < values_list.size(); i++ )
		{
			if ( values_list[i].compare( s ) == 0 )
			{
				set_value( i );
				return;
			}
		}
	}

	ASSERT( m_list_index == -1 );
	m_edit_value = s;
}

void FeMenuOpt::set_value( int i )
{
	m_list_index = i;
	m_edit_value.clear(); //may be set for lists if no match previously
}

void FeMenuOpt::append_vlist( const char **clist )
{
	int i=0;
	while ( clist[i] != NULL )
	{
		values_list.push_back( clist[i++] );

		if (( m_list_index == -1 ) && ( m_edit_value.compare( values_list.back() ) == 0 ))
			set_value( values_list.size() - 1 );
	}
}

void FeMenuOpt::append_vlist( const std::vector< std::string > &list )
{
	for ( std::vector<std::string>::const_iterator it=list.begin(); it!=list.end(); ++it )
	{
		values_list.push_back( *it );

		if (( m_list_index == -1 ) && ( m_edit_value.compare( *it ) == 0 ))
			set_value( values_list.size() - 1 );
	}
}

FeConfigContext::FeConfigContext( FeSettings &f )
	: fe_settings( f ),
	style( SelectionList ),
	curr_sel( -1 ),
	default_sel( 0 ),
	save_req( false )
{
}

// Add menu option
// - Returns the added option to allow chaining
FeMenuOpt *FeConfigContext::add_opt(
	const int type,
	const std::string &label,
	const std::string &value,
	const std::string &help,
	const int &opaque,
	const int &flags
)
{
	// label is translated by default
	// - use TRANSLATE_NONE to prevent this, commonly used with user-provided labels
	std::string lbl = ( flags & OptFlag::TRANSLATE_LABEL )
		? fe_settings.get_translation( label )
		: label;

	// special case for TOGGLE options, convert the value and add the yes/no list options
	if ( type == Opt::TOGGLE )
	{
		std::vector<std::string> bool_opts = {
			fe_settings.get_translation( "Yes" ),
			fe_settings.get_translation( "No" )
		};
		opt_list.push_back( FeMenuOpt( type, lbl, bool_opts[value.empty() ? 1 : 0] ) );
		back_opt().append_vlist( bool_opts );
	}
	else
	{
		opt_list.push_back( FeMenuOpt( type, lbl, value ) );
	}

	// Help text always get translated
	fe_settings.get_translation( help, back_opt().help_msg );
	back_opt().opaque = opaque;

	return &back_opt();
}

// Add TOGGLE menu option, allows boolean value to be passed
FeMenuOpt *FeConfigContext::add_opt(
	const int type,
	const std::string &label,
	const bool &value,
	const std::string &help,
	const int &opaque,
	const int &flags
)
{
	// This method catches empty-strings, so check for TOGGLE type too
	return add_opt( type, label, std::string( ( type == Opt::TOGGLE && value ) ? "1" : ""), help, opaque, flags );
}

// Set the menu page style
void FeConfigContext::set_style( Style s, const std::string &t )
{
	style = s;
	fe_settings.get_translation( t, title );
}

FeBaseConfigMenu::FeBaseConfigMenu()
{
}

void FeBaseConfigMenu::get_options( FeConfigContext &ctx )
{
	ctx.add_opt( Opt::DEFAULTEXIT, "Back", "", "_help_back", -1 );
}

bool FeBaseConfigMenu::on_option_select(
		FeConfigContext &ctx, FeBaseConfigMenu *& submenu )
{
	return true;
}

bool FeBaseConfigMenu::save( FeConfigContext &ctx )
{
	return true;
}

FeEmuArtEditMenu::FeEmuArtEditMenu()
	: m_emulator( NULL )
{
}

void FeEmuArtEditMenu::get_options( FeConfigContext &ctx )
{
	std::string path;
	if ( m_emulator ) m_emulator->get_artwork( m_art_name, path );

	ctx.set_style( FeConfigContext::EditList, "Edit Artwork" );
	ctx.add_opt( Opt::EDIT, "Artwork Label", m_art_name, "_help_art_label" );
	ctx.add_opt( Opt::EDIT, "Artwork Path", path, "_help_art_path" );
	FeBaseConfigMenu::get_options( ctx );
}

bool FeEmuArtEditMenu::save( FeConfigContext &ctx )
{
	if (!m_emulator)
		return false;

	if ( !m_art_name.empty() )
		m_emulator->delete_artwork( m_art_name );

	std::string label = ctx.opt_list[0].get_value();
	if ( !label.empty() )
	{
		m_emulator->delete_artwork( label );
		m_emulator->add_artwork( label, ctx.opt_list[1].get_value() );
	}

	return true;
}

void FeEmuArtEditMenu::set_art( FeEmulatorInfo *emu,
	const std::string &art_name )
{
	m_emulator = emu;
	m_art_name = art_name;
}

const char *GENERATOR_LABEL = "Generate Collection/Rom List";

FeEmulatorEditMenu::FeEmulatorEditMenu()
	: m_emulator( NULL ),
	m_is_new( false ),
	m_romlist_exists( false ),
	m_parent_save( false )
{
}

void FeEmulatorEditMenu::get_options( FeConfigContext &ctx )
{
	ctx.set_style( FeConfigContext::EditList, "Edit Emulator" );

	if ( m_emulator )
	{
		// Don't allow editing of the name. User can set it when adding new
		//
		ctx.add_opt( Opt::INFO, "Emulator Name", m_emulator->get_info( FeEmulatorInfo::Name ) );

		for ( int i=1; i < FeEmulatorInfo::LAST_INDEX; i++ )
		{
#if defined(NO_NBM_WAIT)
			if ( i == (int)FeEmulatorInfo::NBM_wait )
				continue;
#endif
#if defined(NO_PAUSE_HOTKEY)
			if ( i == (int)FeEmulatorInfo::Pause_hotkey )
				continue;
#endif
			std::string help( "_help_emu_" );
			help += FeEmulatorInfo::indexStrings[i];
			std::string label = FeEmulatorInfo::indexDispStrings[i];
			std::string value = m_emulator->get_info( (FeEmulatorInfo::Index)i );

			if ( i == FeEmulatorInfo::Info_source )
				ctx.add_opt( Opt::LIST, label, value, help )->append_vlist( FeEmulatorInfo::infoSourceStrings );
			else if (( i == FeEmulatorInfo::Exit_hotkey ) || ( i == FeEmulatorInfo::Pause_hotkey ))
				ctx.add_opt( Opt::RELOAD, label, value, help, 100 );
			else
				ctx.add_opt( Opt::EDIT, label, value, help );
		}

		std::vector<std::pair<std::string, std::string>> alist;
		m_emulator->get_artwork_list( alist );

		for ( std::vector<std::pair<std::string, std::string>>::iterator itr=alist.begin(); itr!=alist.end(); ++itr )
			ctx.add_opt( Opt::MENU, (*itr).first, (*itr).second, "_help_art", 1, OptFlag::TRANSLATE_NONE );

		ctx.add_opt( Opt::MENU, "Add Artwork", "", "_help_art_add", 2 );
		ctx.add_opt( Opt::MENU, GENERATOR_LABEL, "", "_help_emu_gen_romlist", 3 );
		ctx.add_opt( Opt::MENU, "Scrape Artwork", "", "_help_emu_scrape_artwork", 4 );
		ctx.add_opt( Opt::EXIT, "Delete this Emulator", "", "_help_emu_delete", 5 );
	}

	// We need to call save if this is a new emulator
	//
	if ( m_is_new )
		ctx.save_req = true;

	FeBaseConfigMenu::get_options( ctx );
}

namespace
{
	// Return the value of a list index, or empty string if out-of-range
	const std::string value_at( const std::vector<std::string> list, const int index )
	{
		if ( index < 0 || index >= (int)list.size() ) return "";
		return list.at( index );
	}

	bool gen_ui_update( void *d, int i, const std::string &aux )
	{
		FeConfigContext *od = (FeConfigContext *)d;

		od->splash_message( "Generating Rom List: $1%", as_str( i ), aux );

		if ( od->check_for_cancel() )
		{
			od->splash_message( "Please Wait", "", "" );
			return false;
		}

		return true;
	}

	bool scrape_ui_update( void *d, int i, const std::string &aux )
	{
		FeConfigContext *od = (FeConfigContext *)d;

		od->splash_message( "Scraping Artwork: $1%", as_str( i ), aux );

		if ( od->check_for_cancel() )
		{
			od->splash_message( "Please Wait", "", "" );
			return false;
		}

		return true;
	}
};

bool FeEmulatorEditMenu::on_option_select(
		FeConfigContext &ctx, FeBaseConfigMenu *& submenu )
{
	FeMenuOpt &o = ctx.curr_opt();

	if ( !m_emulator )
		return true;

	switch ( o.opaque )
	{
	case 1: //	 Edit artwork
		m_art_menu.set_art( m_emulator, o.setting );
		submenu = &m_art_menu;
		break;

	case 2: //	 Add new artwork
		m_art_menu.set_art( m_emulator, "" );
		submenu = &m_art_menu;
		break;

	case 3: // Generate Romlist
		{
			// Make sure m_emulator is set with all the configured info
			//
			int j=0;
			for ( int i=0; i < FeEmulatorInfo::LAST_INDEX; i++ )
			{
#if defined(NO_NBM_WAIT)
				if ( i == (int)FeEmulatorInfo::NBM_wait )
					continue;
#endif
#if defined(NO_PAUSE_HOTKEY)
				if ( i == (int)FeEmulatorInfo::Pause_hotkey )
					continue;
#endif
				m_emulator->set_info( (FeEmulatorInfo::Index)i,
						ctx.opt_list[j++].get_value() );
			}
			// Do some checks and confirmation before launching the Generator
			//
			std::vector<std::string> paths = m_emulator->get_paths();

			for ( std::vector<std::string>::const_iterator itr = paths.begin(); itr != paths.end(); ++itr )
			{
				std::string rom_path = m_emulator->clean_path_with_wd( *itr );
				if ( !directory_exists( rom_path ) )
				{
					if ( ctx.confirm_dialog( "Rom path '$1' not found, proceed anyways?", rom_path ) == false )
						return false;
					else
						break; // only bug the user once if there are multiple paths configured
				}
			}

			std::string emu_name = m_emulator->get_info( FeEmulatorInfo::Name );

			if ( m_romlist_exists )
			{
				if ( ctx.confirm_dialog( "Overwrite existing '$1' list?", emu_name ) == false )
					return false;
			}

			FePresent *fep = FePresent::script_get_fep();
			if ( fep )
				fep->set_video_play_state( false );

			std::vector<std::string> emu_list;
			emu_list.push_back( emu_name );

			ctx.fe_settings.build_romlist(
				emu_list, emu_name, gen_ui_update, &ctx, ctx.help_msg );

			if ( fep )
				fep->set_video_play_state(
					fep->get_video_toggle() );

			//
			// If we don't have a display configured for this romlist,
			// configure one now
			//
			if ( !ctx.fe_settings.check_romlist_configured( emu_name ) )
			{
				FeDisplayInfo *new_disp = ctx.fe_settings.create_display( emu_name );
				new_disp->set_info( FeDisplayInfo::Romlist, emu_name );
			}

			ctx.save_req = true;
			m_parent_save = true;
		}
		break;
	case 4: // Scrape Artwork
		{
			FePresent *fep = FePresent::script_get_fep();
			if ( fep )
				fep->set_video_play_state( false );

			std::string emu_name = m_emulator->get_info( FeEmulatorInfo::Name );
			ctx.fe_settings.scrape_artwork( emu_name, scrape_ui_update, &ctx, ctx.help_msg );

			if ( fep )
				fep->set_video_play_state(
					fep->get_video_toggle() );
		}
		break;
	case 5: // Delete this Emulator
		{
			std::string name = m_emulator->get_info(FeEmulatorInfo::Name);

			if ( ctx.confirm_dialog( "Delete emulator '$1'?", name ) == false )
				return false;

			ctx.fe_settings.delete_emulator(
					m_emulator->get_info(FeEmulatorInfo::Name) );

			m_emulator = NULL;
		}
		break;

	case 100: // Hotkey input
		{
			FeInputMapEntry ent;
			FeInputMap::Command conflict( FeInputMap::LAST_COMMAND );
			ctx.input_map_dialog( "Press Hotkey", ent, conflict );
			std::string res = ent.as_string();

			bool save=false;
			if ( o.get_value().compare( res ) != 0 )
				save = true;
			else
			{
				if ( ctx.confirm_dialog( "Clear Hotkey?" ))
				{
					res.clear();
					save = true;
				}
			}

			if ( save )
			{
				o.set_value( res );
				ctx.save_req = true;
			}
		}
		break;

	default:
		break;
	}

	return true;
}

bool FeEmulatorEditMenu::save( FeConfigContext &ctx )
{
	if ( !m_emulator )
		return m_parent_save;

	int j=0;
	for ( int i=0; i < FeEmulatorInfo::LAST_INDEX; i++ )
	{
#if defined(NO_NBM_WAIT)
		if ( i == (int)FeEmulatorInfo::NBM_wait )
			continue;
#endif
#if defined(NO_PAUSE_HOTKEY)
		if ( i == (int)FeEmulatorInfo::Pause_hotkey )
			continue;
#endif
		m_emulator->set_info( (FeEmulatorInfo::Index)i,
				ctx.opt_list[j++].get_value() );
	}

	std::string filename = ctx.fe_settings.get_config_dir();
	confirm_directory( filename, FE_EMULATOR_SUBDIR );

	const std::string name = m_emulator->get_info( FeEmulatorInfo::Name );
	filename += FE_EMULATOR_SUBDIR;
	filename += name;
	filename += FE_EMULATOR_FILE_EXTENSION;
	m_emulator->save( filename );

	return m_parent_save;
}

void FeEmulatorEditMenu::set_emulator(
	FeEmulatorInfo *emu, bool is_new, const std::string &romlist_dir )
{
	m_emulator=emu;
	m_is_new=is_new;
	m_parent_save=false;

	if ( emu )
	{
		std::string filename = m_emulator->get_info( FeEmulatorInfo::Name );
		filename += FE_ROMLIST_FILE_EXTENSION;

		m_romlist_exists = file_exists( romlist_dir + filename );
	}
	else
		m_romlist_exists = false;
}

//
// Class for storing the romlist generator default selections in generator.am
//
class FeRLGenDefaults : public FeBaseConfigurable
{
private:
	std::string m_name;
	std::set<std::string> m_sel;

public:
	FeRLGenDefaults() {};

	FeRLGenDefaults( const std::string &n, const std::vector<std::string> &s )
		: m_name( n )
	{
		for ( std::vector<std::string>::const_iterator itr=s.begin(); itr!=s.end(); ++itr )
			m_sel.insert( *itr );
	}

	int process_setting( const std::string &setting,
		const std::string &value,
		const std::string &filename )
	{
		size_t pos=0;
		std::string token;

		if ( setting.compare( "selected" ) == 0 )
		{
			while ( token_helper( value, pos, token, ";" ) )
				m_sel.insert( token );
		}
		else if ( setting.compare( "name" ) == 0 )
			token_helper( value, pos, m_name );

		return 0;
	}

	void save( const std::string &filename )
	{
		nowide::ofstream outfile( filename.c_str() );
		if ( outfile.is_open() )
		{
			outfile << "selected ";
			for ( std::set<std::string>::iterator itr=m_sel.begin(); itr!=m_sel.end(); ++itr )
				outfile << *itr << ";";

			outfile << std::endl;
			outfile << "name " << m_name << std::endl;
		}
		outfile.close();
	}

	bool contains( const std::string &emu_name )
	{
		return ( m_sel.find( emu_name ) != m_sel.end() );
	}

	const std::string &get_default_name()
	{
		return m_name;
	}
};

const char *GENERATOR_FN = "generator.am";

void FeEmulatorGenMenu::get_options( FeConfigContext &ctx )
{
	ctx.set_style( FeConfigContext::EditList, GENERATOR_LABEL );

	std::vector<std::string> emu_file_list;
	ctx.fe_settings.get_list_of_emulators( emu_file_list );

	FeRLGenDefaults defaults;
	defaults.load_from_file( ctx.fe_settings.get_config_dir() + GENERATOR_FN );
	m_default_name = defaults.get_default_name();

	for ( std::vector<std::string>::iterator itr=emu_file_list.begin(); itr < emu_file_list.end(); ++itr )
		ctx.add_opt( Opt::TOGGLE, *itr, defaults.contains( *itr ), "_help_generator_opt", 0, OptFlag::TRANSLATE_NONE );

	ctx.add_opt( Opt::MENU, GENERATOR_LABEL, "", "_help_generator_build", 1 );

	FeBaseConfigMenu::get_options( ctx );
}

bool FeEmulatorGenMenu::on_option_select(
	FeConfigContext &ctx, FeBaseConfigMenu *& submenu )
{
	FeMenuOpt &o = ctx.curr_opt();
	if ( o.opaque == 1 )
	{
		FePresent *fep = FePresent::script_get_fep();
		if ( fep )
			fep->set_video_play_state( false );

		std::vector < std::string > emu_list;
		int i=0;

		while ( ctx.opt_list[i].opaque != 1 )
		{
			if ( ctx.opt_list[i].get_vindex() == 0 )
				emu_list.push_back( ctx.opt_list[i].setting );

			i++;
		}

		std::string res = m_default_name;
		if ( res.empty() && !emu_list.empty() )
		{
			// Create reasonable default name if we don't have one yet
			res = ( emu_list.size() == 1 )
				? emu_list[0]
				: ctx.fe_settings.get_translation( "multi" );
		}

		if ( !ctx.edit_dialog( "Enter Romlist name", res ) || res.empty() )
			return false;

		std::string path = ctx.fe_settings.get_config_dir();
		path += FE_ROMLIST_SUBDIR;
		path += res;
		path += FE_ROMLIST_FILE_EXTENSION;

		if ( file_exists( path ) )
		{
			if ( ctx.confirm_dialog( "Overwrite existing '$1' list?", res ) == false )
				return false;
		}

		ctx.fe_settings.build_romlist( emu_list, res, gen_ui_update, &ctx,
			ctx.help_msg );

		// Save these settings as defaults for next time
		FeRLGenDefaults defaults( res, emu_list );
		defaults.save( ctx.fe_settings.get_config_dir() + GENERATOR_FN );

		if ( fep )
			fep->set_video_play_state(
				fep->get_video_toggle() );

		//
		// If we don't have a display configured for this romlist,
		// configure one now
		//
		if ( !ctx.fe_settings.check_romlist_configured( res ) )
		{
			FeDisplayInfo *new_disp = ctx.fe_settings.create_display( res );
			new_disp->set_info( FeDisplayInfo::Romlist, res );
			ctx.save_req = true;
		}
	}

	return true;
}

void FeEmulatorSelMenu::get_options( FeConfigContext &ctx )
{
	ctx.set_style( FeConfigContext::SelectionList, "Configure > Emulators" );

	std::vector<std::string> emu_file_list;
	ctx.fe_settings.get_list_of_emulators( emu_file_list );

	for ( std::vector<std::string>::iterator itr=emu_file_list.begin(); itr < emu_file_list.end(); ++itr )
		ctx.add_opt( Opt::MENU, *itr, "", "_help_emu_sel", 0, OptFlag::TRANSLATE_NONE );

	ctx.add_opt( Opt::MENU, "Add Emulator", "", "_help_emu_add", 1 );
	ctx.add_opt( Opt::MENU, GENERATOR_LABEL, "", "_help_emu_sel_gen_romlist", 2 );
	ctx.add_opt( Opt::MENU, "Auto-detect Emulators", "", "_help_emu_sel_auto_detect", 3 );
	FeBaseConfigMenu::get_options( ctx );
}

bool FeEmulatorSelMenu::on_option_select(
	FeConfigContext &ctx, FeBaseConfigMenu *& submenu )
{
	FeMenuOpt &o = ctx.curr_opt();

	FeEmulatorInfo *e( NULL );
	bool flag=false;

	if ( o.opaque < 0 )
		return true;

	if ( o.opaque == 1 )
	{
		std::string res;
		if ( !ctx.edit_dialog( "Enter Emulator Name", res ) || res.empty() )
			return false;

		std::vector<std::string> t_list;
		ctx.fe_settings.get_list_of_emulators( t_list, true );

		std::string et;
		if ( t_list.size() > 0 )
		{
			t_list.insert( t_list.begin(), ctx.fe_settings.get_translation( "Default" ) );
			int sel = ctx.option_dialog( "Select template to use for emulator settings", t_list, 0 );

			if ( sel > 0 )
				et = t_list[ sel ];
		}

		e = ctx.fe_settings.create_emulator( res, et );
		flag = true;
	}
	else if ( o.opaque == 2 )
	{
		submenu = &m_gen_menu;
	}
	else if ( o.opaque == 3 )
	{
		if ( ctx.confirm_dialog( "Auto-detect emulators?" ))
		{
			FePresent *fep = FePresent::script_get_fep();
			if ( fep )
				fep->setup_wizard();
		}
	}
	else
		e = ctx.fe_settings.get_emulator( o.setting );

	if ( e )
	{
		std::string romlist_dir = ctx.fe_settings.get_config_dir();
		romlist_dir += FE_ROMLIST_SUBDIR;
		m_edit_menu.set_emulator( e, flag, romlist_dir );
		submenu = &m_edit_menu;
	}

	return true;
}

FeRuleEditMenu::FeRuleEditMenu()
	: m_filter( NULL ), m_index( -1 )
{
}

void FeRuleEditMenu::get_options( FeConfigContext &ctx )
{
	FeRomInfo::Index target( FeRomInfo::LAST_INDEX );
	FeRule::FilterComp comp( FeRule::LAST_COMPARISON );
	std::string what;
	bool is_exception=false;

	if ( m_filter )
	{
		std::vector<FeRule> &r = m_filter->get_rules();
		if (( m_index >= 0 ) && ( m_index < (int)r.size() ))
		{
			target = r[m_index].get_target();
			comp = r[m_index].get_comp();
			what = r[m_index].get_what();
			is_exception = r[m_index].is_exception();
		}
	}

	std::string title = is_exception ? "Exception Edit" : "Rule Edit";
	std::vector<std::string> targets = ctx.fe_settings.get_translation( FeRomInfo::indexStrings );
	std::vector<std::string> comps = ctx.fe_settings.get_translation( FeRule::filterCompDisplayStrings );
	std::string target_str = value_at( targets, target );
	std::string comp_str = value_at( comps, comp );

	ctx.set_style( FeConfigContext::EditList, title );
	ctx.add_opt( Opt::LIST, "Target", target_str, "_help_rule_target" )->append_vlist( targets );
	ctx.add_opt( Opt::LIST, "Comparison", comp_str, "_help_rule_comp" )->append_vlist( comps );
	ctx.add_opt( Opt::EDIT, "Filter Value", what, "_help_rule_value" );
	ctx.add_opt( Opt::EXIT, "Delete this Rule", "", "_help_rule_delete", 1 );
	FeBaseConfigMenu::get_options( ctx );
}

bool FeRuleEditMenu::on_option_select(
	FeConfigContext &ctx, FeBaseConfigMenu *& submenu )
{
	FeMenuOpt &o = ctx.curr_opt();
	if ( o.opaque == 1 ) // the "delete" option
	{
		std::vector<FeRule> &r = m_filter->get_rules();

		if (( m_index >= 0 ) && ( m_index < (int)r.size() ))
			r.erase( r.begin() + m_index );

		m_filter = NULL;
		m_index = -1;
		ctx.save_req = true;
	}

	return true;
}

bool FeRuleEditMenu::save( FeConfigContext &ctx )
{
	int i = ctx.opt_list[0].get_vindex();
	if ( i == -1 ) i = FeRomInfo::LAST_INDEX;

	int c = ctx.opt_list[1].get_vindex();
	if ( c == -1 ) c = FeRule::LAST_COMPARISON;

	std::string what = ctx.opt_list[2].get_value();

	if ( m_filter )
	{
		std::vector<FeRule> &r = m_filter->get_rules();

		if (( m_index >= 0 ) && ( m_index < (int)r.size() ))
		{
			r[m_index].set_values( (FeRomInfo::Index)i, (FeRule::FilterComp)c, what );
		}
	}

	return true;
}

void FeRuleEditMenu::set_rule_index( FeFilter *filter, int index )
{
	m_filter=filter;
	m_index=index;
}

FeFilterEditMenu::FeFilterEditMenu()
	: m_display( NULL ), m_index( 0 )
{
}

void FeFilterEditMenu::get_options( FeConfigContext &ctx )
{
	ctx.set_style( FeConfigContext::EditList, "Filter Edit" );

	if ( m_display )
	{
		FeFilter *f = m_display->get_filter( m_index );

		if ( m_index < 0 )
			ctx.add_opt( Opt::INFO, "Filter Name", ctx.fe_settings.get_translation( "Global Filter" ), "_help_filter_name" );
		else
			ctx.add_opt( Opt::EDIT, "Filter Name", f->get_name(), "_help_filter_name" );

		int i=0;
		std::vector<FeRule> &rules = f->get_rules();

		for ( std::vector<FeRule>::const_iterator itr=rules.begin(); itr != rules.end(); ++itr )
		{
			std::string rule_str;
			FeRomInfo::Index t = (*itr).get_target();

			if ( t != FeRomInfo::LAST_INDEX )
			{
				rule_str += ctx.fe_settings.get_translation( FeRomInfo::indexStrings[t] );

				FeRule::FilterComp c = (*itr).get_comp();
				if ( c != FeRule::LAST_COMPARISON )
				{
					rule_str += " ";
					rule_str += ctx.fe_settings.get_translation( FeRule::filterCompDisplayStrings[c] );
					rule_str += " ";
					rule_str += (*itr).get_what();
				}
			}

			if ( (*itr).is_exception() )
				ctx.add_opt( Opt::MENU, "Exception", rule_str, "_help_filter_exception", 100 + i );
			else
				ctx.add_opt( Opt::MENU, "Rule", rule_str, "_help_filter_rule", 100 + i );

			i++;
		}

		ctx.add_opt( Opt::MENU, "Add Rule", "", "_help_filter_add_rule", 1);
		ctx.add_opt( Opt::MENU, "Add Exception", "", "_help_filter_add_exception", 2);

		if ( m_index >= 0 ) // don't add the following options for the global filter
		{
            FeRomInfo::Index sort_by = f->get_sort_by();
			bool reverse_order = f->get_reverse_order();
			if ( FeRomInfo::isNumeric( sort_by ) ) reverse_order = !reverse_order;
            std::vector<std::string> sort_opts = ctx.fe_settings.get_translation( FeRomInfo::indexStrings );
			sort_opts.push_back( ctx.fe_settings.get_translation( "No Sort" ) );
			std::string sort_str = value_at( sort_opts, sort_by );

			ctx.add_opt( Opt::LIST, "Sort By", sort_str, "_help_filter_sort_by" )->append_vlist( sort_opts );
			ctx.add_opt( Opt::TOGGLE, "Reverse Order", reverse_order, "_help_filter_reverse_order" );
			ctx.add_opt( Opt::EDIT, "List Limit", as_str( f->get_list_limit() ), "_help_filter_list_limit" );
			ctx.add_opt( Opt::EXIT, "Delete this Filter", "", "_help_filter_delete", 3);
		}
	}

	FeBaseConfigMenu::get_options( ctx );
}

bool FeFilterEditMenu::on_option_select(
		FeConfigContext &ctx, FeBaseConfigMenu *& submenu )
{
	if ( !m_display )
		return true;

	FeMenuOpt &o = ctx.curr_opt();
	FeFilter *f = m_display->get_filter( m_index );

	if (( o.opaque >= 100 ) || ( o.opaque == 1 ) || ( o.opaque == 2 ))
	{
		int r_index=0;

		if (( o.opaque == 1 ) || ( o.opaque == 2 ))
		{
			bool is_exception = ( o.opaque == 2 );

			std::vector<FeRule> &rules = f->get_rules();

			rules.push_back( FeRule() );
			rules.back().set_is_exception( is_exception );

			r_index = rules.size() - 1;
			ctx.save_req=true;
		}
		else
			r_index = o.opaque - 100;

		m_rule_menu.set_rule_index( f, r_index );
		submenu=&m_rule_menu;
	}
	else if ( o.opaque == 3 )
	{
		// "Delete this Filter"
		if ( ctx.confirm_dialog( "Delete filter '$1'?", f->get_name() ) == false )
			return false;

		m_display->delete_filter( m_index );
		m_display=NULL;
		m_index=-1;
		ctx.save_req=true;
	}

	return true;
}

bool FeFilterEditMenu::save( FeConfigContext &ctx )
{
	if (( m_display ) && ( m_index >= 0 ))
	{
		FeFilter *f = m_display->get_filter( m_index );

		std::string name = ctx.opt_list[0].get_value();
		f->set_name( name );

		int sort_pos = ctx.opt_list.size() - 5;
		FeRomInfo::Index sort_by = (FeRomInfo::Index)ctx.opt_list[ sort_pos ].get_vindex();
		f->set_sort_by( sort_by );

		// NOTE: reverse_order is *displayed* opposite for numerically sorted fields (stats)
		// Users expect numeric fields to display descending by default
		// - False (Default) = A-Z, Large-Small
		// - True (Reversed) = Z-A, Small-Large
		bool reverse_order = ctx.opt_list[ sort_pos + 1 ].get_vindex() == 0;
		if ( FeRomInfo::isNumeric( sort_by ) ) reverse_order = !reverse_order;
		f->set_reverse_order( reverse_order );

		std::string limit_str = ctx.opt_list[ sort_pos + 2 ].get_value();
		int list_limit = as_int( limit_str );
		f->set_list_limit( list_limit );
	}

	return true;
}

void FeFilterEditMenu::set_filter_index( FeDisplayInfo *d, int i )
{
	m_display=d;
	m_index=i;
}

FeDisplayEditMenu::FeDisplayEditMenu()
	: m_index( -1 )
{
}

void FeDisplayEditMenu::get_options( FeConfigContext &ctx )
{
	ctx.set_style( FeConfigContext::EditList, "Display Edit" );

	FeDisplayInfo *display = ctx.fe_settings.get_display( m_index );
	if ( display )
	{
		std::vector<std::string> layouts;
		std::vector<std::string> romlists;
		ctx.fe_settings.get_layouts_list( layouts );
		ctx.fe_settings.get_romlists_list( romlists );

		ctx.add_opt( Opt::EDIT, "Name", display->get_info( FeDisplayInfo::Name ), "_help_display_name" );
		ctx.add_opt( Opt::LIST, "Layout", display->get_info( FeDisplayInfo::Layout ), "_help_display_layout" )->append_vlist( layouts );
		ctx.add_opt( Opt::LIST, "Collection/Rom List", display->get_info( FeDisplayInfo::Romlist ), "_help_display_romlist" )->append_vlist( romlists );
		ctx.add_opt( Opt::TOGGLE, "Show in Cycle", display->show_in_cycle(), "_help_display_in_cycle" );
		ctx.add_opt( Opt::TOGGLE, "Show in Menu", display->show_in_menu(), "_help_display_in_menu" );

		FeFilter *f = display->get_filter( -1 );

		std::string filter_desc = ctx.fe_settings.get_translation( (f->get_rule_count() < 1) ? "Empty" : "$1 Rule(s)" );
		perform_substitution( filter_desc, { as_str(f->get_rule_count()) });
		ctx.add_opt( Opt::MENU, "Global Filter", filter_desc, "_help_display_global_filter", 9 );

		int i=0;
		std::vector<std::string> filters;
		display->get_filters_list( filters );
		for ( std::vector<std::string>::iterator itr=filters.begin(); itr != filters.end(); ++itr )
		{
			ctx.add_opt( Opt::MENU, "Filter", (*itr), "_help_display_filter", 100 + i );
			i++;
		}

		ctx.add_opt( Opt::MENU, "Add Filter", "", "_help_display_add_filter", 1 );
		ctx.add_opt( Opt::MENU, "Layout Options", "", "_help_display_layout_options", 2 );
		ctx.add_opt( Opt::EXIT, "Delete this Display", "", "_help_display_delete", 3 );
	}

	FeBaseConfigMenu::get_options( ctx );
}

bool FeDisplayEditMenu::on_option_select(
		FeConfigContext &ctx, FeBaseConfigMenu *& submenu )
{
	FeMenuOpt &o = ctx.curr_opt();

	FeDisplayInfo *display = ctx.fe_settings.get_display( m_index );
	if ( !display )
		return true;

	if (( o.opaque >= 100 ) || ( o.opaque == 1 ) | ( o.opaque == 9 ))
	{
		// a filter or "Add Filter" is selected
		int f_index=0;

		if ( o.opaque == 1 )
		{
			std::string res;
			if ( !ctx.edit_dialog( "Enter Filter Name", res ) || res.empty() )
				return false;		// if they don't enter a name then cancel

			ctx.fe_settings.create_filter( *display, res );

			f_index = display->get_filter_count() - 1;
			ctx.save_req=true;
		}
		else if ( o.opaque == 9 )
			f_index = -1; // this will get us the global filter
		else
			f_index = o.opaque - 100;

		m_filter_menu.set_filter_index( display, f_index );
		submenu=&m_filter_menu;
	}
	else if ( o.opaque == 2 )
	{
		// Layout Options
		FeLayoutInfo &cfg = ctx.fe_settings.get_layout_config( ctx.opt_list[1].get_value() );
		m_layout_menu.set_layout( &cfg, &display->get_layout_per_display_params(), display );

		submenu=&m_layout_menu;
	}
	else if ( o.opaque == 3 )
	{
		// "Delete this Display"
		if ( ctx.confirm_dialog( "Delete display '$1'?", display->get_info( FeDisplayInfo::Name ) ) == false )
			return false;

		ctx.fe_settings.delete_display( m_index );
		ctx.save_req=true;
	}

	return true;
}

bool FeDisplayEditMenu::save( FeConfigContext &ctx )
{
	FeDisplayInfo *display = ctx.fe_settings.get_display( m_index );
	if ( display )
	{
		for ( int i=0; i< FeDisplayInfo::LAST_INDEX; i++ )
		{
			if (( i == FeDisplayInfo::InCycle ) || ( i == FeDisplayInfo::InMenu ))
				display->set_info( i, ctx.opt_list[i].get_bool() );
			else
				display->set_info( i, ctx.opt_list[i].get_value() );
		}
	}

	return true;
}

void FeDisplayEditMenu::set_display_index( int index )
{
	m_index=index;
}

void FeDisplayMenuEditMenu::get_options( FeConfigContext &ctx )
{
	std::string prompt_str = ctx.fe_settings.get_info( FeSettings::MenuPrompt );
	std::string default_str = ctx.fe_settings.get_translation( "Default" );

	std::string layout = ctx.fe_settings.has_custom_displays_menu()
		? ctx.fe_settings.get_info( FeSettings::MenuLayout )
		: default_str;

	std::vector<std::string> layouts;
	ctx.fe_settings.get_layouts_list( layouts ); // this sorts the list
	layouts.insert( layouts.begin(), default_str ); // force first enty to the 'default' string

	ctx.set_style( FeConfigContext::EditList, "Configure > Displays Menu" );
	ctx.add_opt( Opt::EDIT, "Menu Prompt", prompt_str, "_help_displays_menu_prompt" );
	ctx.add_opt( Opt::LIST, "Menu Style > Layout", layout, "_help_displays_menu_layout" )->append_vlist( layouts );
	ctx.add_opt( Opt::TOGGLE, "Allow Exit from 'Displays Menu'", ctx.fe_settings.get_info_bool( FeSettings::DisplaysMenuExit ), "_help_displays_menu_exit" );
	ctx.add_opt( Opt::MENU, "Layout Options", "", "_help_display_layout_options", 1 );
	FeBaseConfigMenu::get_options( ctx );
}

bool FeDisplayMenuEditMenu::on_option_select(
		FeConfigContext &ctx, FeBaseConfigMenu *& submenu )
{
	FeMenuOpt &o = ctx.curr_opt();

	if (( o.opaque == 1 ) && ( ctx.opt_list[1].get_vindex() != 0 ))
	{
		FeLayoutInfo &cfg = ctx.fe_settings.get_layout_config( ctx.opt_list[1].get_value() );
		m_layout_menu.set_layout( &cfg, &ctx.fe_settings.get_display_menu_per_display_params(), NULL );
		submenu=&m_layout_menu;
	}

	return true;
}

bool FeDisplayMenuEditMenu::save( FeConfigContext &ctx )
{
	ctx.fe_settings.set_info( FeSettings::MenuPrompt, ctx.opt_list[0].get_value() );
	ctx.fe_settings.set_info( FeSettings::MenuLayout, ( ctx.opt_list[1].get_vindex() == 0 ) ? "" : ctx.opt_list[1].get_value() );
	ctx.fe_settings.set_info( FeSettings::DisplaysMenuExit, ctx.opt_list[2].get_bool() );
	return true;
}

void FeDisplaySelMenu::get_options( FeConfigContext &ctx )
{
	ctx.default_sel = ctx.fe_settings.get_current_display_index();
	if ( ctx.default_sel < 0 ) ctx.default_sel = 0; // We are in the displays menu.

	ctx.set_style( FeConfigContext::SelectionList, "Configure > Displays" );

	int display_count = ctx.fe_settings.displays_count();
	for ( int i=0; i< display_count; i++ )
		ctx.add_opt( Opt::MENU, ctx.fe_settings.get_display( i )->get_info( FeDisplayInfo::Name ), "", "_help_display_sel", i, OptFlag::TRANSLATE_NONE );

	ctx.add_opt( Opt::MENU, "'Displays Menu' Options", "", "_help_displays_menu", 99999 );
	ctx.add_opt( Opt::MENU, "Add New Display", "", "_help_display_add", 100000 );
	FeBaseConfigMenu::get_options( ctx );
}

bool FeDisplaySelMenu::on_option_select(
		FeConfigContext &ctx, FeBaseConfigMenu *& submenu )
{
	FeMenuOpt &o = ctx.curr_opt();

	if ( o.opaque < 0 )
		return true;

	if ( o.opaque == 99999 )
	{
		submenu = &m_menu_menu;
		return true;
	}

	if ( o.opaque == 100000 )
	{
		std::string res;
		if ( !ctx.edit_dialog( "Enter Display Name", res ) || res.empty() )
			return false;		// if they don't enter a name then cancel

		ctx.save_req=true;

		ctx.fe_settings.create_display( res );
		m_edit_menu.set_display_index(
			ctx.fe_settings.displays_count() - 1 );
		submenu = &m_edit_menu;
	}
	else
	{
		m_edit_menu.set_display_index( o.opaque );
		submenu = &m_edit_menu;
	}

	return true;
}

FeInputEditMenu::FeInputEditMenu()
	: m_mapping( NULL )
{
}

void FeInputEditMenu::get_options( FeConfigContext &ctx )
{
	ctx.set_style( FeConfigContext::EditList, "Edit Control" );

	if (m_mapping)
	{
		std::string act = ctx.fe_settings.get_translation( FeInputMap::commandDispStrings[m_mapping->command] );
		ctx.add_opt( Opt::INFO, "Action", act, "_help_input_action" );

		for ( std::vector<std::string>::iterator it=m_mapping->input_list.begin(); it<m_mapping->input_list.end(); ++it )
			ctx.add_opt( Opt::RELOAD, "Remove Input", (*it), "_help_input_delete" );

		ctx.add_opt( Opt::RELOAD, "Add Input", "", "_help_input_add", 1 );

		if ( m_mapping->command < FeInputMap::Select )
		{
			std::vector < std::string > ol;
			int i=FeInputMap::Select+1;
			while ( FeInputMap::commandDispStrings[i] != NULL )
				ol.push_back( FeInputMap::commandDispStrings[i++] );

			ol.push_back( "" );

			FeInputMap::Command cc = ctx.fe_settings.get_default_command( m_mapping->command );
			std::string cc_str = ( cc == FeInputMap::LAST_COMMAND )
				? "" : FeInputMap::commandDispStrings[cc];

			ctx.add_opt( Opt::LIST, "Default Action", cc_str, "_help_input_default_action", 2 )->append_vlist( ol );
		}
	}

	FeBaseConfigMenu::get_options( ctx );
}

bool FeInputEditMenu::on_option_select(
		FeConfigContext &ctx, FeBaseConfigMenu *& submenu )
{
	FeMenuOpt &o = ctx.curr_opt();
	if (!m_mapping)
		return true;

	switch ( o.opaque )
	{
	case 0:
		{
			if (( m_mapping->input_list.size() <= 1 ) && (
					( m_mapping->command == FeInputMap::Select )
					|| ( m_mapping->command == FeInputMap::Back )
					|| ( m_mapping->command == FeInputMap::Up )
					|| ( m_mapping->command == FeInputMap::Down )
					|| ( m_mapping->command == FeInputMap::Left )
					|| ( m_mapping->command == FeInputMap::Right ) ) )
			{
				// We don't let the user unmap all UI menu controls.
				// Doing so would prevent them from further navigating configure mode.
				break;
			}

			for ( std::vector<std::string>::iterator it=m_mapping->input_list.begin(); it<m_mapping->input_list.end(); ++it )
			{
				if ( (*it).compare( o.get_value() ) == 0 )
				{
					// User selected to remove this mapping
					//
					m_mapping->input_list.erase( it );
					ctx.save_req = true;
					break;
				}
			}
		}
		break;

	case 1:
		{
			FeInputMapEntry ent;
			FeInputMap::Command conflict( FeInputMap::LAST_COMMAND );
			ctx.input_map_dialog( "Press Input", ent, conflict );
			std::string res = ent.as_string();

			if ( res.empty() )
				return true;

			bool save=true;
			if ( conflict == m_mapping->command )
				save = false; // don't add duplicate entries
			else if ( conflict != FeInputMap::LAST_COMMAND )
			{
				std::string command_str = ctx.fe_settings.get_translation( FeInputMap::commandDispStrings[ conflict ] );
				save = ctx.confirm_dialog( "This will overwrite an existing mapping ($1).  Proceed?", command_str );
			}

			if ( save )
			{
				m_mapping->input_list.push_back( res );
				ctx.save_req = true;
			}
		}
		break;

	default:
		break;
	}

	return true;
}

bool FeInputEditMenu::save( FeConfigContext &ctx )
{
	if ( m_mapping )
	{
		int idx = ctx.opt_list.size() - 2;
		if ( ctx.opt_list[idx].opaque == 2 )
		{
			// Set default mapping
			int val = ctx.opt_list[idx].get_vindex();
			if ( val == (int)ctx.opt_list[idx].values_list.size()-1 ) // the empty entry
				val = FeInputMap::LAST_COMMAND;
			else
				val += FeInputMap::Select+1;

			ctx.fe_settings.set_default_command(
				m_mapping->command,
				(FeInputMap::Command)val );
		}

		ctx.fe_settings.set_input_mapping( *m_mapping );
	}

	return true;
}

void FeInputEditMenu::set_mapping( FeMapping *mapping )
{
	m_mapping = mapping;
}

void FeInputJoysticksMenu::get_options( FeConfigContext &ctx )
{
	ctx.set_style( FeConfigContext::EditList, "Configure > Joystick Mapping" );


	std::vector < std::string > values;
	std::set < std::string > values_set;

	std::string default_str = ctx.fe_settings.get_translation( "Default" );
	values.push_back( default_str ); // we assume this is the first entry in the vector

	for ( size_t i=0; i < sf::Joystick::Count; i++ )
	{
		if ( sf::Joystick::isConnected( i ) )
		{
			std::string temp = sf::Joystick::getIdentification( i ).name.toAnsiString();

			if ( values_set.find( temp ) == values_set.end() )
			{
				values_set.insert( temp );
				values.push_back( temp );
			}
		}
	}

	std::vector<std::pair<int, std::string>> &joy_config = ctx.fe_settings.get_joy_config();

	for ( std::vector<std::pair<int, std::string>>::iterator itr=joy_config.begin(); itr!=joy_config.end(); ++itr )
	{
		if ( values_set.find( (*itr).second ) == values_set.end() )
		{
			values_set.insert( (*itr).second );
			values.push_back( (*itr).second );
		}
	}

	for ( int i=0; i < (int)sf::Joystick::Count; i++ )
	{
		std::string value = default_str;
		std::string name = ctx.fe_settings.get_translation( "Joystick $1" );
		perform_substitution( name, { as_str( i ) });

		for ( std::vector<std::pair<int, std::string>>::iterator itr=joy_config.begin(); itr!=joy_config.end(); ++itr )
		{
			if ( (*itr).first == i ) value = (*itr).second;
		}

		ctx.add_opt( Opt::LIST, name, value, "_help_joystick_map" )->append_vlist( values );
	}

	FeBaseConfigMenu::get_options( ctx );
}

bool FeInputJoysticksMenu::save( FeConfigContext &ctx )
{
	std::vector < std::pair < int, std::string > > &joy_config = ctx.fe_settings.get_joy_config();

	joy_config.clear();

	for ( size_t i=0; i < sf::Joystick::Count; i++ )
	{
		if ( ctx.opt_list[ i].get_vindex() != 0 ) // we don't record anything if "Default" is selected for a slot
			joy_config.push_back( std::pair < int, std::string >( i, ctx.opt_list[i].get_value() ) );
	}

	ctx.save_req = true; // because FeInputSelMenu saves before calling us, we need to flag that a save is now
				// required again
	return true;
}

void FeInputSelMenu::get_options( FeConfigContext &ctx )
{
	ctx.set_style( FeConfigContext::EditList, "Configure > Controls" );
	ctx.fe_settings.get_input_mappings( m_mappings );

	std::string orstr = ctx.fe_settings.get_translation( "OR" );
	for ( std::vector<FeMapping>::iterator it=m_mappings.begin(); it != m_mappings.end(); ++it )
	{
		std::string value;
		for ( std::vector<std::string>::iterator iti=(*it).input_list.begin(); iti != (*it).input_list.end(); ++iti )
		{
			if ( iti > (*it).input_list.begin() )
			{
				value += " ";
				value += orstr;
				value += " ";
			}

			value += (*iti);
		}

		//
		// Show the default action in brackets beside the UI controls (up/down/left...etc)
		//
		std::string name;
		ctx.fe_settings.get_translation( FeInputMap::commandDispStrings[(*it).command], name );
		if ( (*it).command < FeInputMap::Select )
		{
			FeInputMap::Command c = ctx.fe_settings.get_default_command( (*it).command );
			if ( c != FeInputMap::LAST_COMMAND )
			{
				name += " (";
				name += ctx.fe_settings.get_translation( FeInputMap::commandDispStrings[c] );
				name += ")";
			}
		}

		std::string help_msg( "_help_control_" );
		help_msg += FeInputMap::commandStrings[(*it).command];

		ctx.add_opt( Opt::MENU, name, value, help_msg, 0, OptFlag::TRANSLATE_NONE );
	}

	// Create a list of evenly spaced thresholds from 100...0 (clamped at 99...1)
	int n = 20;
	std::vector<std::string> thresh( n + 1 );
	for ( int i=0; i<(n+1); i++)
		thresh[i] = as_str( std::clamp( 100 - ( i * 100 / n ), 1, 99 ) );

	// Add the joystick and mouse threshold settings to this menu as well
	std::string joy_str = ctx.fe_settings.get_info( FeSettings::JoystickThreshold );
	std::string mouse_str = ctx.fe_settings.get_info( FeSettings::MouseThreshold );

	ctx.add_opt( Opt::LIST, "Joystick Threshold", joy_str, "_help_joystick_threshold", 1 )->append_vlist( thresh );
	ctx.add_opt( Opt::LIST, "Mouse Threshold", mouse_str, "_help_mouse_threshold", 1 )->append_vlist( thresh );
	ctx.add_opt( Opt::MENU, "Joystick Mappings", "", "_help_joystick_map", 2 );

	FeBaseConfigMenu::get_options( ctx );
}

bool FeInputSelMenu::save( FeConfigContext &ctx )
{
	ctx.fe_settings.set_info( FeSettings::JoystickThreshold, ctx.opt_list[ ctx.opt_list.size() - 4 ].get_value() );
	ctx.fe_settings.set_info( FeSettings::MouseThreshold, ctx.opt_list[ ctx.opt_list.size() - 3 ].get_value() );
	return true;
}

bool FeInputSelMenu::on_option_select(
		FeConfigContext &ctx, FeBaseConfigMenu *& submenu )
{
	FeMenuOpt &o = ctx.curr_opt();

	if ( o.opaque == 0 )
	{
		// save now if needed so that the updated mouse and joystick
		// threshold values are used for any further mapping
		if ( ctx.save_req )
		{
			save( ctx );
			ctx.save_req = false;
		}

		m_edit_menu.set_mapping( &(m_mappings[ ctx.curr_sel ]) );
		submenu = &m_edit_menu;
	}
	else if ( o.opaque == 2 )
	{
		// save now if needed so that the updated mouse and joystick
		// threshold values are used for any further mapping
		if ( ctx.save_req )
		{
			save( ctx );
			ctx.save_req = false;
		}

		submenu = &m_joysticks_menu;
	}

	return true;
}

void FeSoundMenu::get_options( FeConfigContext &ctx )
{
	ctx.set_style( FeConfigContext::EditList, "Configure > Sound" );

	int n = 10;
	std::vector<std::string> volumes(n+1);
	for ( int i=0; i<(n+1); i++ )
		volumes[i] = as_str( 100 - ( i * 100 / n ) );

	//
	// Sound, Ambient and Movie Volumes
	//
	for ( int i=0; i<3; i++ )
	{
		int v = ctx.fe_settings.get_set_volume( (FeSoundInfo::SoundType)i );
		ctx.add_opt( Opt::LIST, FeSoundInfo::settingDispStrings[i], as_str(v), "_help_volume" )->append_vlist( volumes );
	}
	// Loudness Normalisation toggle
	//
	ctx.add_opt( Opt::TOGGLE, FeSoundInfo::settingDispStrings[3], ctx.fe_settings.get_loudness(), "_help_loudness_normalisation" );

	//
	// Get the list of available sound files
	// Note the sound_list vector gets copied to each option!
	//
	std::vector<std::string> sound_list;
	ctx.fe_settings.get_sounds_list( sound_list );

#ifndef NO_MOVIE
	for ( std::vector<std::string>::iterator itr=sound_list.begin(); itr != sound_list.end(); )
	{
		if ( !FeMedia::is_supported_media_file( *itr ) )
			itr = sound_list.erase( itr );
		else
			itr++;
	}
#endif

	sound_list.push_back( "" );

	//
	// Sounds that can be mapped to input commands.
	//
	for ( int i=0; i < FeInputMap::LAST_EVENT; i++ )
	{
		// there is a NULL in the middle of the commandStrings list
		if ( FeInputMap::commandDispStrings[i] != NULL )
		{
			std::string v;
			ctx.fe_settings.get_sound_file( (FeInputMap::Command)i, v, false );
			ctx.add_opt( Opt::LIST, FeInputMap::commandDispStrings[i], v, "_help_sound_sel", i )->append_vlist( sound_list );
		}
	}

	FeBaseConfigMenu::get_options( ctx );
}

bool FeSoundMenu::save( FeConfigContext &ctx )
{
	//
	// Sound, Ambient and Movie Volumes
	//
	int i;
	for ( i=0; i<3; i++ )
		ctx.fe_settings.set_volume(
			(FeSoundInfo::SoundType)i,
			ctx.opt_list[i].get_value()
		);

	//
	// Loudness Normalisation
	//
	ctx.fe_settings.set_loudness( ctx.opt_list[3].get_vindex() == 0 );

	//
	// Save the sound settings
	//
	for ( i=4; i< (int)ctx.opt_list.size(); i++ )
	{
		if ( ctx.opt_list[i].opaque >= 0 )
			ctx.fe_settings.set_sound_file(
				(FeInputMap::Command)ctx.opt_list[i].opaque,
				ctx.opt_list[i].get_value()
			);
	}

	return true;
}

void FeScraperMenu::get_options( FeConfigContext &ctx )
{
	ctx.set_style( FeConfigContext::EditList, "Configure > Scraper" );
	ctx.add_opt( Opt::TOGGLE, "Scrape Snaps", ctx.fe_settings.get_info_bool( FeSettings::ScrapeSnaps ), "_help_scrape_snaps" );
	ctx.add_opt( Opt::TOGGLE, "Scrape Marquees", ctx.fe_settings.get_info_bool( FeSettings::ScrapeMarquees ), "_help_scrape_marquees" );
	ctx.add_opt( Opt::TOGGLE, "Scrape Flyers/Boxart", ctx.fe_settings.get_info_bool( FeSettings::ScrapeFlyers ), "_help_scrape_flyers" );
	ctx.add_opt( Opt::TOGGLE, "Scrape Game Logos (Wheel Art)", ctx.fe_settings.get_info_bool( FeSettings::ScrapeWheels ), "_help_scrape_wheels" );
	ctx.add_opt( Opt::TOGGLE, "Scrape Fanart", ctx.fe_settings.get_info_bool( FeSettings::ScrapeFanArt ), "_help_scrape_fanart" );
	ctx.add_opt( Opt::TOGGLE, "Scrape Videos (MAME only)", ctx.fe_settings.get_info_bool( FeSettings::ScrapeVids ), "_help_scrape_vids" );
	FeBaseConfigMenu::get_options( ctx );
}

bool FeScraperMenu::save( FeConfigContext &ctx )
{
	ctx.fe_settings.set_info( FeSettings::ScrapeSnaps, ctx.opt_list[0].get_bool() );
	ctx.fe_settings.set_info( FeSettings::ScrapeMarquees, ctx.opt_list[1].get_bool() );
	ctx.fe_settings.set_info( FeSettings::ScrapeFlyers, ctx.opt_list[2].get_bool() );
	ctx.fe_settings.set_info( FeSettings::ScrapeWheels, ctx.opt_list[3].get_bool() );
	ctx.fe_settings.set_info( FeSettings::ScrapeFanArt, ctx.opt_list[4].get_bool() );
	ctx.fe_settings.set_info( FeSettings::ScrapeVids, ctx.opt_list[5].get_bool() );
	return true;
}

void FeMiscMenu::get_options( FeConfigContext &ctx )
{
	ctx.set_style( FeConfigContext::EditList, "Configure > General" );

	// ---------------------------------------------------------------------------------

	ctx.fe_settings.get_languages_list( m_languages );
	std::string curr_lang = ctx.fe_settings.get_info( FeSettings::Language );
	std::string disp_lang;
	std::vector<std::string> disp_lang_list;

	for ( std::vector<FeLanguage>::iterator itr=m_languages.begin(); itr!=m_languages.end(); ++itr )
	{
		disp_lang_list.push_back( (*itr).label );
		if ( curr_lang.compare( (*itr).language ) == 0 )
			disp_lang = (*itr).label;
	}
	ctx.add_opt( Opt::LIST, "Language", disp_lang, "_help_language" )->append_vlist( disp_lang_list );

	// ---------------------------------------------------------------------------------

	std::string uicol_hex = ctx.fe_settings.get_ui_color();
	std::vector<std::string>::iterator uicol_item = FeSettings::uiColorTokens.begin();
	if ( !uicol_hex.empty() )
	{
		// Find the selected ui colour
		uicol_item = std::find( FeSettings::uiColorTokens.begin(), FeSettings::uiColorTokens.end(), uicol_hex );

		// If not found add a custom colour option to the lists
		if ( uicol_item == FeSettings::uiColorTokens.end() )
		{
			FeSettings::uiColorTokens.push_back( uicol_hex );
			FeSettings::uiColorDispTokens.push_back( "(Custom)" );
			uicol_item = --FeSettings::uiColorTokens.end();
		}
	}
	std::vector<std::string> uicol_opts = ctx.fe_settings.get_translation( FeSettings::uiColorDispTokens );
	std::string uicol_sel = value_at( uicol_opts, uicol_item - FeSettings::uiColorTokens.begin() );
	ctx.add_opt( Opt::LIST, "Menu Colour", uicol_sel, "_help_ui_color" )->append_vlist( uicol_opts );
	ctx.back_opt().trigger_reload = true; // flag the ui to reload if this option changes
    ctx.back_opt().trigger_colour = true; // special case for menu colour options

	// ---------------------------------------------------------------------------------

#if !defined(FORCE_FULLSCREEN)
	std::vector<std::string> win_modes = ctx.fe_settings.get_translation( FeSettings::windowModeDispTokens );
	std::string win_mode = value_at( win_modes, ctx.fe_settings.get_window_mode() );
	ctx.add_opt( Opt::LIST, "Window Mode", win_mode, "_help_window_mode" )->append_vlist( win_modes );
#endif

	std::vector<std::string> rot_modes = ctx.fe_settings.get_translation( FeSettings::screenRotationDispTokens );
	std::string rot_mode = value_at( rot_modes, ctx.fe_settings.get_screen_rotation() );
	ctx.add_opt( Opt::LIST, "Screen Rotation", rot_mode, "_help_screen_rotation" )->append_vlist( rot_modes );

	// convert AA setting from multiplier[0,2,4,8] to index[0,1,2,3] respectively
	int aa_i = 0;
	std::vector<std::string> aa_avail;
	while ( FeSettings::antialiasingDispTokens[aa_i] != 0 && as_int( FeSettings::antialiasingTokens[aa_i] ) <= std::min( static_cast<int>( sf::RenderTexture::getMaximumAntiAliasingLevel() ), 8 ))
		aa_avail.push_back( FeSettings::antialiasingDispTokens[ aa_i++ ] );
	std::vector<std::string> aa_modes = ctx.fe_settings.get_translation( aa_avail );
	std::string aa_mode = value_at( aa_modes, (int)std::max( log2( ctx.fe_settings.get_antialiasing() ), 0.0 ) );
	ctx.add_opt( Opt::LIST, "Anti-Aliasing", aa_mode, "_help_antialiasing" )->append_vlist( aa_modes );

	// convert AF setting from multiplier[0,2,4,8,16] to index[0,1,2,3,4] respectively
	std::vector<std::string> af_modes = ctx.fe_settings.get_translation( FeSettings::anisotropicDispTokens );
	std::string af_mode = value_at( af_modes, (int)std::max( log2( ctx.fe_settings.get_anisotropic() ), 0.0 ) );
	ctx.add_opt( Opt::LIST, "Anisotropic Filtering", af_mode, "_help_anisotropic_filtering" )->append_vlist( af_modes );

	std::vector<std::string> startup_modes = ctx.fe_settings.get_translation( FeSettings::startupDispTokens );
	std::string startup_mode = value_at( startup_modes, ctx.fe_settings.get_startup_mode() );
	ctx.add_opt( Opt::LIST, "Startup Mode", startup_mode, "_help_startup_mode" )->append_vlist( startup_modes );

	ctx.add_opt( Opt::TOGGLE, "Quick Menu", ctx.fe_settings.get_info_bool( FeSettings::QuickMenu ), "_help_quick_menu" );
	ctx.add_opt( Opt::TOGGLE, "Layout Preview", ctx.fe_settings.get_info_bool( FeSettings::LayoutPreview ), "_help_layout_preview" );
	ctx.add_opt( Opt::TOGGLE, "Track Usage", ctx.fe_settings.get_info_bool( FeSettings::TrackUsage ), "_help_track_usage" );
#if !defined(NO_MULTIMON)
	ctx.add_opt( Opt::TOGGLE, "Enable Multiple Monitors", ctx.fe_settings.get_info_bool( FeSettings::MultiMon ), "_help_multiple_monitors" );
#endif
	ctx.add_opt( Opt::TOGGLE, "Hide Brackets in Game Title", ctx.fe_settings.get_info_bool( FeSettings::HideBrackets ), "_help_hide_brackets" );
	ctx.add_opt( Opt::TOGGLE, "Group Clones", ctx.fe_settings.get_info_bool( FeSettings::GroupClones ), "_help_group_clones" );
	ctx.add_opt( Opt::TOGGLE, "Confirm Favourites", ctx.fe_settings.get_info_bool( FeSettings::ConfirmFavourites ), "_help_confirm_favs" );

	std::vector<std::string> wrap_modes = ctx.fe_settings.get_translation( FeSettings::filterWrapDispTokens );
	std::string wrap_mode = value_at( wrap_modes, ctx.fe_settings.get_filter_wrap_mode() );
	ctx.add_opt( Opt::LIST, "Filter Wrap Mode", wrap_mode, "_help_filter_wrap_mode" )->append_vlist( wrap_modes );

	ctx.add_opt( Opt::TOGGLE, "Confirm Exit", ctx.fe_settings.get_info_bool( FeSettings::ConfirmExit ), "_help_confirm_exit" );
	ctx.add_opt( Opt::EDIT, "Exit Command", ctx.fe_settings.get_info( FeSettings::ExitCommand ), "_help_exit_command" );
	ctx.add_opt( Opt::EDIT, "Exit Message", ctx.fe_settings.get_info( FeSettings::ExitMessage ), "_help_exit_message" );

	// ---------------------------------------------------------------------------------

	std::vector<std::string> decoders;
	std::string vid_dec;
#ifdef NO_MOVIE
	vid_dec = "software";
	decoders.push_back( vid_dec );
#else
	vid_dec = FeMedia::get_current_decoder();
	FeMedia::get_decoder_list( decoders );
#endif
	ctx.add_opt( Opt::LIST, "Video Decoder", vid_dec, "_help_video_decoder" )->append_vlist( decoders );

	// ---------------------------------------------------------------------------------

	ctx.add_opt( Opt::EDIT, "Image Cache Size", ctx.fe_settings.get_info( FeSettings::ImageCacheMBytes ), "_help_image_cache_mbytes" );
	ctx.add_opt( Opt::TOGGLE, "Power Saving", ctx.fe_settings.get_info_bool( FeSettings::PowerSaving ), "_help_power_saving" );
	ctx.add_opt( Opt::TOGGLE, "Check for Updates", ctx.fe_settings.get_info_bool( FeSettings::CheckForUpdates ), "_help_check_for_updates" );

#ifdef SFML_SYSTEM_WINDOWS
	ctx.add_opt( Opt::TOGGLE, "Hide Console", ctx.fe_settings.get_hide_console(), "_help_hide_console" );
#endif

	FeBaseConfigMenu::get_options( ctx );
}

bool FeMiscMenu::save( FeConfigContext &ctx )
{
	int i=0;
	ctx.fe_settings.set_language( m_languages[ ctx.opt_list[i++].get_vindex() ].language );
	ctx.fe_settings.set_info( FeSettings::UIColor, FeSettings::uiColorTokens[ ctx.opt_list[i++].get_vindex() ] );
#if !defined(FORCE_FULLSCREEN)
	ctx.fe_settings.set_info( FeSettings::WindowMode, FeSettings::windowModeTokens[ ctx.opt_list[i++].get_vindex() ] );
#endif
	ctx.fe_settings.set_info( FeSettings::ScreenRotation, FeSettings::screenRotationTokens[ ctx.opt_list[i++].get_vindex() ] );
	ctx.fe_settings.set_info( FeSettings::AntiAliasing, FeSettings::antialiasingTokens[ ctx.opt_list[i++].get_vindex() ] );
	ctx.fe_settings.set_info( FeSettings::Anisotropic, FeSettings::anisotropicTokens[ ctx.opt_list[i++].get_vindex() ] );
	ctx.fe_settings.set_info( FeSettings::StartupMode, FeSettings::startupTokens[ ctx.opt_list[i++].get_vindex() ] );
	ctx.fe_settings.set_info( FeSettings::QuickMenu, ctx.opt_list[i++].get_bool() );
	ctx.fe_settings.set_info( FeSettings::LayoutPreview, ctx.opt_list[i++].get_bool() );
	ctx.fe_settings.set_info( FeSettings::TrackUsage, ctx.opt_list[i++].get_bool() );
#if !defined(NO_MULTIMON)
	ctx.fe_settings.set_info( FeSettings::MultiMon, ctx.opt_list[i++].get_bool() );
#endif
	ctx.fe_settings.set_info( FeSettings::HideBrackets, ctx.opt_list[i++].get_bool() );
	ctx.fe_settings.set_info( FeSettings::GroupClones, ctx.opt_list[i++].get_bool() );
	ctx.fe_settings.set_info( FeSettings::ConfirmFavourites, ctx.opt_list[i++].get_bool() );
	ctx.fe_settings.set_info( FeSettings::FilterWrapMode, FeSettings::filterWrapTokens[ ctx.opt_list[i++].get_vindex() ] );
	ctx.fe_settings.set_info( FeSettings::ConfirmExit, ctx.opt_list[i++].get_bool() );
	ctx.fe_settings.set_info( FeSettings::ExitCommand, ctx.opt_list[i++].get_value() );
	ctx.fe_settings.set_info( FeSettings::ExitMessage, ctx.opt_list[i++].get_value() );
	ctx.fe_settings.set_info( FeSettings::VideoDecoder, ctx.opt_list[i++].get_value() );
	ctx.fe_settings.set_info( FeSettings::ImageCacheMBytes, ctx.opt_list[i++].get_value() );
	ctx.fe_settings.set_info( FeSettings::PowerSaving, ctx.opt_list[i++].get_bool() );
	ctx.fe_settings.set_info( FeSettings::CheckForUpdates, ctx.opt_list[i++].get_bool() );
#ifdef SFML_SYSTEM_WINDOWS
	ctx.fe_settings.set_info( FeSettings::HideConsole, ctx.opt_list[i++].get_bool() );
#endif

	return true;
}

FeScriptConfigMenu::FeScriptConfigMenu()
	: m_state( FeSettings::Layout_Showing ),
	m_script_id( -1 ),
	m_configurable( NULL ),
	m_per_display( NULL )
{
}

bool FeScriptConfigMenu::on_option_select(
		FeConfigContext &ctx, FeBaseConfigMenu *& submenu )
{
	FeMenuOpt &o = ctx.curr_opt();

	if ( o.opaque == 1 )
	{
		FeInputMap::Command conflict( FeInputMap::LAST_COMMAND );
		FeInputMapEntry ent;
		ctx.input_map_dialog( "Press Input", ent, conflict );
		std::string res = ent.as_string();

		if ((conflict == FeInputMap::Back )
			|| ( conflict == FeInputMap::Exit )
			|| ( conflict == FeInputMap::ExitToDesktop ))
		{
			// Clear the mapping if the user pushed the back button
			res.clear();
		}

		o.set_value( res );
		ctx.save_req = true;
	}
	else if (( o.opaque == 2 ) && ( m_configurable ))
	{
		save( ctx );

		ctx.fe_settings.set_present_state( m_state );
		FePresent *fep = FePresent::script_get_fep();
		if ( fep )
			fep->set_script_id( m_script_id );

		FeVM::script_run_config_function(
				*m_configurable,
				m_file_path,
				m_file_name,
				o.opaque_str,
				ctx.help_msg );
	}
	return true;
}

bool FeScriptConfigMenu::save_helper( FeConfigContext &ctx, int first_idx )
{
	m_configurable->clear_params();

	if ( m_per_display )
		m_per_display->clear_params();

	for ( unsigned int i=first_idx; i < ctx.opt_list.size(); i++ )
	{
		std::string &os = ctx.opt_list[i].opaque_str;

		//
		// grep for 'hacky' in src/fe_vm.cpp to find the other end of this...
		// FeVM::script_get_config_options() inserts a '%' character at the start
		// of opaque_str if this one is a "per_display" option.  So we deal with
		// that now by stripping the % character out and storing this to the per
		// display parameters instead of the general per layout parameters
		//
		if ( m_per_display && !os.empty() && ( os[0] == '%' ) )
		{
			m_per_display->set_param( os.substr( 1 ),
				ctx.opt_list[i].get_value() );
		}
		else
		{
			m_configurable->set_param( os,
				ctx.opt_list[i].get_value() );
		}
	}

	return true;
}

FePluginEditMenu::FePluginEditMenu()
	: m_plugin( NULL )
{
}

void FePluginEditMenu::get_options( FeConfigContext &ctx )
{
	if ( !m_plugin )
		return;

	ctx.set_style( FeConfigContext::EditList, "Configure Plug-in" );
	ctx.add_opt( Opt::INFO, "Name", m_plugin->get_name(), "_help_plugin_name" );
	ctx.add_opt( Opt::TOGGLE, "Enabled", m_plugin->get_enabled(), "_help_plugin_enabled" );

	//
	// We run the plug-in script to check if a "UserConfig" class is defined.
	// If it is, then its members and member attributes set out what it is
	// that the plug-in needs configured by the user.
	//
	ctx.fe_settings.get_plugin_full_path( m_plugin->get_name(), m_file_path, m_file_name );
	m_configurable = m_plugin;

	std::string gen_help;
	FeVM::script_get_config_options( ctx, gen_help, *m_plugin, m_file_path, m_file_name );

	if ( !gen_help.empty() )
		ctx.opt_list[0].help_msg = gen_help;

	FeBaseConfigMenu::get_options( ctx );
}

bool FePluginEditMenu::save( FeConfigContext &ctx )
{
	if ( m_plugin == NULL )
		return false;

	m_plugin->set_enabled(
		ctx.opt_list[1].get_vindex() == 0 ? true : false );

	return FeScriptConfigMenu::save_helper( ctx, 2 );
}

void FePluginEditMenu::set_plugin( FePlugInfo *plugin, int index )
{
	m_plugin = plugin;
	m_script_id = index;
}

void FePluginSelMenu::get_options( FeConfigContext &ctx )
{
	ctx.set_style( FeConfigContext::EditList, "Configure > Plug-ins" );

	std::vector<std::string> plugins;
	ctx.fe_settings.get_available_plugins( plugins );

	std::string enabled_str = ctx.fe_settings.get_translation( "Enabled" );
	std::string disabled_str = ctx.fe_settings.get_translation( "Disabled" );

	for ( std::vector<std::string>::iterator itr=plugins.begin(); itr != plugins.end(); ++itr )
	{
		std::string file_path, file_name, gen_help( "_help_plugin_sel" );
		ctx.fe_settings.get_plugin_full_path( *itr, file_path, file_name );
		FeVM::script_get_config_options( gen_help, file_path, file_name );

		std::string value = ctx.fe_settings.get_plugin_enabled( *itr ) ? enabled_str : disabled_str;
		ctx.add_opt( Opt::MENU, (*itr), value, gen_help, 0, OptFlag::TRANSLATE_NONE );
	}

	FeBaseConfigMenu::get_options( ctx );
}

bool FePluginSelMenu::on_option_select(
		FeConfigContext &ctx, FeBaseConfigMenu *& submenu )
{
	FeMenuOpt &o = ctx.curr_opt();

	if ( o.opaque == 0 )
	{
		FePlugInfo *plug;
		int plug_index;
		ctx.fe_settings.get_plugin( o.setting, plug, plug_index );
		m_edit_menu.set_plugin( plug, plug_index );
		submenu = &m_edit_menu;
	}

	return true;
}

FeLayoutEditMenu::FeLayoutEditMenu()
	: m_layout( NULL ),
	m_display( NULL )
{
}

void FeLayoutEditMenu::get_options( FeConfigContext &ctx )
{
	if ( m_layout )
	{
		const std::string &name = m_layout->get_name();
		ctx.set_style( FeConfigContext::EditList, "Configure Layout" );
		ctx.add_opt( Opt::INFO, "Name", name, "_help_layout_name" );

		ctx.fe_settings.get_layout_dir( name, m_file_path );

		std::vector< std::string > file_list;
		FeSettings::get_layout_file_basenames_from_path(
					m_file_path, file_list );

		if ( file_list.empty() )
		{
			// set an empty m_file_name if this is a layout that gets loaded
			// by the loader script...
			m_file_name.clear();
		}
		else
		{
			// Default to layout.nut for the config params - may be overridden below
			m_file_name = FE_LAYOUT_FILE_BASE;

			if (( m_display ) && ( file_list.size() > 1 ))
			{
				// Since there are multiple layout files available, add a config option allowing the user to select which one to use.
				m_file_name = m_display->get_current_layout_file();
				if ( m_file_name.empty() ) m_file_name = FE_LAYOUT_FILE_BASE;

				// Load user config params from the current layout
				ctx.add_opt( Opt::LIST, "Layout File", m_file_name, "_help_layout_file", 500 )->append_vlist( file_list );
				ctx.back_opt().trigger_reload = true;
			}

			m_file_name += FE_LAYOUT_FILE_EXTENSION;
		}

		m_configurable = m_layout;     // parent member
		//m_per_display = m_per_display; // parent member

		// create a copy of m_layout and merge in any per display settings
		// so that appropriate config options are available to the script
		// when run in FeVM::script_Get_config_options() below
		//
		FeLayoutInfo temp_layout( *m_layout );
		if ( m_per_display )
			temp_layout.merge_params( *m_per_display );

		std::string gen_help;
		FeVM::script_get_config_options( ctx, gen_help, temp_layout,
			m_file_path, m_file_name );

		if ( !gen_help.empty() )
			ctx.opt_list[0].help_msg = gen_help;
	}
	FeBaseConfigMenu::get_options( ctx );
}

bool FeLayoutEditMenu::save( FeConfigContext &ctx )
{
	if ( m_layout == NULL )
		return false;

	int first_idx = 1;
	if ( m_display && ( ctx.opt_list.size() > 2 ) && ( ctx.opt_list[1].opaque == 500 ))
	{
		first_idx = 2;
		m_display->set_current_layout_file( ctx.opt_list[1].get_value() );
	}

	// Make a list of saved params that are not in the current option list (ie: belong to variant layouts)
	std::vector<std::pair<std::string, std::string>> prev_params;
	std::vector<std::string> prev_labels;
	m_layout->get_param_labels(prev_labels);
	std::for_each(
		prev_labels.begin(), prev_labels.end(),
		[ &ctx, &prev_params, this ]( std::string label )
		{
			if (
				std::find_if(
					ctx.opt_list.begin(), ctx.opt_list.end(),
					[ &label ]( FeMenuOpt opt ) { return opt.opaque_str == label; }
				) == ctx.opt_list.end()
			)
			{
				std::string value;
				m_layout->get_param( label, value );
				prev_params.push_back({ label, value });
			}
		}
	);

	// Save the current params
	bool retval = FeScriptConfigMenu::save_helper( ctx, first_idx );

	// Save the other params
	std::for_each(
		prev_params.begin(), prev_params.end(),
		[ this ]( std::pair<std::string, std::string> entry )
		{
			m_configurable->set_param( entry.first, entry.second );
		}
	);

	return retval;
}

void FeLayoutEditMenu::set_layout( FeLayoutInfo *layout,
	FeScriptConfigurable *per_display_params,
	FeDisplayInfo *display )
{
	m_layout = layout;
	m_per_display = per_display_params;
	m_display = display;
}

void FeIntroEditMenu::get_options( FeConfigContext &ctx )
{
	ctx.set_style( FeConfigContext::EditList, "Configure > Intro" );

	std::string gen_help;
	ctx.fe_settings.get_path( FeSettings::Intro,
		m_file_path, m_file_name );

	m_configurable = &(ctx.fe_settings.get_current_config( FeSettings::Intro ) );

	FeVM::script_get_config_options( ctx, gen_help, *m_configurable, m_file_path, m_file_name );

	if ( !gen_help.empty() )
		ctx.opt_list[0].help_msg = gen_help;

	FeBaseConfigMenu::get_options( ctx );
}

bool FeIntroEditMenu::save( FeConfigContext &ctx )
{
	return FeScriptConfigMenu::save_helper( ctx );
}

void FeSaverEditMenu::get_options( FeConfigContext &ctx )
{
	ctx.set_style( FeConfigContext::EditList, "Configure > Screen Saver" );
	ctx.add_opt( Opt::EDIT, "Screen Saver Timeout", ctx.fe_settings.get_info( FeSettings::ScreenSaverTimeout ), "_help_screen_saver_timeout" );

	std::string gen_help;
	ctx.fe_settings.get_path( FeSettings::ScreenSaver, m_file_path, m_file_name );

	m_configurable = &(ctx.fe_settings.get_current_config( FeSettings::ScreenSaver ) );

	FeVM::script_get_config_options( ctx, gen_help, *m_configurable, m_file_path, m_file_name );

	if ( !gen_help.empty() )
		ctx.opt_list[0].help_msg = gen_help;

	FeBaseConfigMenu::get_options( ctx );
}

bool FeSaverEditMenu::save( FeConfigContext &ctx )
{
	ctx.fe_settings.set_info( FeSettings::ScreenSaverTimeout,
			ctx.opt_list[0].get_value() );

	return FeScriptConfigMenu::save_helper( ctx, 1 );
}

void FeConfigMenu::get_options( FeConfigContext &ctx )
{
	ctx.default_sel = 1;
	ctx.set_style( FeConfigContext::SelectionList, "Configure" );
	ctx.help_msg = FE_COPYRIGHT;

	ctx.add_opt( Opt::MENU, "Emulators", "", "_help_emulators" );
	ctx.add_opt( Opt::MENU, "Displays", "", "_help_displays" );
	ctx.add_opt( Opt::MENU, "Controls", "", "_help_controls" );
	ctx.add_opt( Opt::MENU, "Sound", "", "_help_sound" );
	ctx.add_opt( Opt::MENU, "Intro", "", "_help_intro" );
	ctx.add_opt( Opt::MENU, "Screen Saver", "", "_help_screen_saver" );
	ctx.add_opt( Opt::MENU, "Plug-ins", "", "_help_plugins" );
	ctx.add_opt( Opt::MENU, "Scraper", "", "_help_scraper" );
	ctx.add_opt( Opt::MENU, "General", "", "_help_misc" );

	//
	// Force save if there is no config file
	//
	if ( ctx.fe_settings.config_file_exists() == false )
		ctx.save_req = true;

	FeBaseConfigMenu::get_options( ctx );
}

bool FeConfigMenu::on_option_select( FeConfigContext &ctx, FeBaseConfigMenu *& submenu )
{
	switch (ctx.curr_sel)
	{
		case 0: submenu = &m_emu_menu; break;
		case 1: submenu = &m_list_menu; break;
		case 2: submenu = &m_input_menu; break;
		case 3: submenu = &m_sound_menu; break;
		case 4: submenu = &m_intro_menu; break;
		case 5: submenu = &m_saver_menu; break;
		case 6: submenu = &m_plugin_menu; break;
		case 7: submenu = &m_scraper_menu; break;
		case 8: submenu = &m_misc_menu; break;
		default: break;
	}
	return true;
}

bool FeConfigMenu::save( FeConfigContext &ctx )
{
	ctx.fe_settings.save();
	return true;
}

FeEditGameMenu::FeEditGameMenu()
	: m_update_rl( false ),
	m_update_stats( false ),
	m_update_extras( false ),
	m_update_overview( false )
{
}

void FeEditGameMenu::get_options( FeConfigContext &ctx )
{
	ctx.set_style( FeConfigContext::EditList, "Edit Game" );

	for ( int i=0; i < (int)FeRomInfo::FileIsAvailable; i++ )
	{
		int type = Opt::EDIT;
		int opaque = 0;
		std::vector<std::string> ol;
		std::string value = ctx.fe_settings.get_rom_info( 0, 0, (FeRomInfo::Index)i );

		switch ( i )
		{
		case FeRomInfo::Emulator:
			ctx.fe_settings.get_list_of_emulators( ol );
			type = Opt::LIST;

			//
			// If we have no emulator set, then set one now if possible.  Use an emulator
			// name that matches the romlist name (if possible), otherwise default to first
			// emulator available
			//
			if ( value.empty() && !ol.empty() )
			{
				value = ol[0];

				int idx = ctx.fe_settings.get_current_display_index();
				if ( idx >= 0 )
				{
					FeDisplayInfo *d = ctx.fe_settings.get_display( idx );
					for ( std::vector<std::string>::iterator itr=ol.begin(); itr != ol.end(); ++itr )
					{
						if ( (*itr).compare( d->get_romlist_name() ) == 0 )
						{
							value = (*itr);
							break;
						}
					}
				}
			}
			break;

		case FeRomInfo::Rotation:
			ol.push_back( "0" );
			ol.push_back( "90" );
			ol.push_back( "180" );
			ol.push_back( "270" );
			type = Opt::LIST;
			break;

		case FeRomInfo::DisplayType:
			ol.push_back( "raster" );
			ol.push_back( "vector" );
			ol.push_back( "lcd" );
			ol.push_back( "unknown" );
			ol.push_back( "" );
			type = Opt::LIST;
			break;

		case FeRomInfo::Status:
			ol.push_back( "good" );
			ol.push_back( "imperfect" );
			ol.push_back( "preliminary" );
			ol.push_back( "" );
			type = Opt::LIST;
			break;

		case FeRomInfo::Favourite:
			type = Opt::RELOAD;
			opaque = 1;
			break;

		case FeRomInfo::Tags:
			type = Opt::RELOAD;
			opaque = 2;
			break;

		case FeRomInfo::PlayedCount:
		case FeRomInfo::PlayedTime:
		case FeRomInfo::PlayedLast:
			type = Opt::EDIT;
			opaque = 3;
			break;

		default:
			break;
		}

		ctx.add_opt( type, FeRomInfo::indexStrings[i], value, "_help_game_edit", opaque );

		if ( !ol.empty() )
			ctx.back_opt().append_vlist( ol );
	}

	int filter_idx = ctx.fe_settings.get_filter_index_from_offset( 0 );
	int rom_idx = ctx.fe_settings.get_rom_index( filter_idx, 0 );

	std::string ov;
	ctx.fe_settings.get_game_overview_absolute( filter_idx, rom_idx, ov );

	ctx.add_opt( Opt::EDIT, "Overview", newline_escape( ov ), "_help_game_overview", 5 );
	ctx.add_opt( Opt::EDIT, "Custom Executable", ctx.fe_settings.get_game_extra( FeSettings::Executable ), "_help_game_custom_executable", 4 );
	ctx.add_opt( Opt::EDIT, "Custom Arguments", ctx.fe_settings.get_game_extra( FeSettings::Arguments ), "_help_game_custom_args", 4 );
	ctx.add_opt( Opt::EXIT, "Delete this Game", "", "_help_game_delete", 100 );

	m_update_stats=false;
	m_update_rl=false;
	m_update_extras=false;
	m_update_overview=false;

	FeRomInfo *rom = ctx.fe_settings.get_rom_absolute( filter_idx, rom_idx );
	if ( rom )
		m_rom_original = *rom;

	FeBaseConfigMenu::get_options( ctx );
}

bool FeEditGameMenu::on_option_select( FeConfigContext &ctx, FeBaseConfigMenu *& submenu )
{
	switch ( ctx.curr_opt().opaque )
	{
	case -1:
		return true;

	case 1: // Favourite
		{
			bool new_state = !ctx.fe_settings.get_current_fav();

			std::string msg = ( new_state )
				? "Add '$1' to Favourites?"
				: "Remove '$1' from Favourites?";

			if ( ctx.confirm_dialog( msg, ctx.opt_list[1].get_value() ) )
			{
				ctx.fe_settings.set_current_fav( new_state );

				// ugh
				FePresent *fep = FePresent::script_get_fep();
				if ( fep )
					fep->on_transition( ChangedTag, FeRomInfo::Favourite );
			}
		}
		break;

	case 2: // Tags
		ctx.tags_dialog();
		break;

	case 3: // PlayedCount, PlayedTime, PlayedLast
		m_update_stats = true;
		break;

	case 4: // Custom Executable, Command Arguments
		m_update_extras = true;
		break;

	case 5: // Overview
		m_update_overview = true;
		break;

	case 100: // Delete Game
		if ( ctx.confirm_dialog( "Delete game '$1'?", ctx.opt_list[1].get_value() ) )
		{
			ctx.fe_settings.update_romlist_after_edit( m_rom_original,
				m_rom_original,
				FeSettings::EraseEntry );
			return true;
		}
		return false;
	default:
		m_update_rl = true;
		break;
	}

	return true;
}

bool FeEditGameMenu::save( FeConfigContext &ctx )
{
	int filter_idx = ctx.fe_settings.get_filter_index_from_offset( 0 );
	int rom_idx = ctx.fe_settings.get_rom_index( filter_idx, 0 );

	FeRomInfo *rom = ctx.fe_settings.get_rom_absolute( filter_idx, rom_idx );

	FeRomInfo replacement;
	if (rom)
		replacement = *rom;

	// Update working romlist with the info provided by the user
	//
	int border = (int)FeRomInfo::FileIsAvailable;
	for ( int i=0; i < border; i++ )
		replacement.set_info( (FeRomInfo::Index)i, ctx.opt_list[i].get_value() );

	// Resave the romlist file that our romlist was loaded from
	//
	if ( m_update_rl )
		ctx.fe_settings.update_romlist_after_edit( m_rom_original, replacement );

	// Resave the usage stats (if they were changed)
	//
	if ( m_update_stats )
		ctx.fe_settings.update_stats(0,0); // this will force a rewrite of the file

	if ( m_update_overview )
	{
		std::string ov = ctx.opt_list[border].get_value();
		perform_substitution( ov, "\\n", "\n" );

		ctx.fe_settings.set_game_overview(
			replacement.get_info( FeRomInfo::Emulator ),
			replacement.get_info( FeRomInfo::Romname ),
			ov, true ); // force overwrites
	}

	if ( m_update_extras )
	{
		ctx.fe_settings.set_game_extra( FeSettings::Executable, ctx.opt_list[border+1].get_value() );
		ctx.fe_settings.set_game_extra( FeSettings::Arguments, ctx.opt_list[border+2].get_value() );
		ctx.fe_settings.save_game_extras();
	}

	return true;
}

FeEditShortcutMenu::FeEditShortcutMenu()
	: m_update_rl( false )
{
}

void FeEditShortcutMenu::get_options( FeConfigContext &ctx )
{
	ctx.set_style( FeConfigContext::EditList, "Edit Shortcut" );

	/////////////////////////////////////////////////////////////////////
	//
	// Something is a shortcut if the romlist "Emulator" field starts with a "@" character
	//
	// Relevant fields:
	//
	// Title      = Shortcut description
	// Emulator   = "@"               (Display shortcut)
	//            = "@<signal_name>"  (Command shortcut)
	// Romname    = shortcut target   (Display shortcut only)
	// AltRomname = optional artwork label
	//
	/////////////////////////////////////////////////////////////////////

	int filter_idx = ctx.fe_settings.get_filter_index_from_offset( 0 );
	int rom_idx = ctx.fe_settings.get_rom_index( filter_idx, 0 );

	FeRomInfo *rom = ctx.fe_settings.get_rom_absolute( filter_idx, rom_idx );
	if ( rom )
		m_rom_original = *rom;

	std::string command;
	std::string temp = m_rom_original.get_info( FeRomInfo::Emulator );
	if ( temp.size() > 1 )
		command = temp.substr( 1 );

	ctx.add_opt( Opt::EDIT, "Title", m_rom_original.get_info( FeRomInfo::Title ), "_help_shortcut_label_edit" );

	std::string value;
	std::vector<std::string> option_list;
	if ( command.empty() )
	{
		value = m_rom_original.get_info( FeRomInfo::Romname );
		int display_count = ctx.fe_settings.displays_count();
		for ( int i=0; i< display_count; i++ )
			option_list.push_back( ctx.fe_settings.get_display( i )->get_info( FeDisplayInfo::Name ) );
	}
	else
	{
		value = command;
		int i=0;
		while ( FeInputMap::commandDispStrings[i] )
		{
			if ( command.compare( FeInputMap::commandStrings[i] ) == 0 )
				value = FeInputMap::commandDispStrings[i];

			option_list.push_back( FeInputMap::commandDispStrings[i] );
			i++;
		}
	}

	ctx.add_opt( Opt::LIST, "Target", value, "_help_shortcut_target_edit" )->append_vlist( option_list );
	ctx.add_opt( Opt::EDIT, "Artwork Name", m_rom_original.get_info( FeRomInfo::AltRomname ), "_help_shortcut_artwork_name_edit" );
	ctx.add_opt( Opt::EXIT, "Delete this Shortcut", "", "_help_shortcut_delete", 1 );

	FeBaseConfigMenu::get_options( ctx );
}

bool FeEditShortcutMenu::on_option_select( FeConfigContext &ctx, FeBaseConfigMenu *& submenu )
{
	if ( ctx.curr_opt().opaque == 1 )
	{
		if ( ctx.confirm_dialog( "Delete shortcut '$1'?", ctx.opt_list[0].get_value() ) )
		{
			ctx.fe_settings.update_romlist_after_edit( m_rom_original,
				m_rom_original,
				FeSettings::EraseEntry );

			m_update_rl = false;
			return true;
		}
		return false;
	}

	m_update_rl = true;
	return true;
}

bool FeEditShortcutMenu::save( FeConfigContext &ctx )
{
	if ( m_update_rl )
	{
		FeRomInfo replacement = m_rom_original;

		std::string command;
		std::string temp = m_rom_original.get_info( FeRomInfo::Emulator );
		if ( temp.size() > 1 )
			command = temp.substr( 1 );

		// Update working romlist with the info provided by the user
		//
		replacement.set_info( FeRomInfo::Title, ctx.opt_list[0].get_value() );

		if ( command.empty() )
		{
			// Display shortcut
			replacement.set_info( FeRomInfo::Romname, ctx.opt_list[1].get_value() );
			replacement.set_info( FeRomInfo::Emulator, "@" );
		}
		else
		{
			std::string new_command = FeInputMap::commandStrings[ ctx.opt_list[1].get_vindex() ];

			replacement.set_info( FeRomInfo::Romname, new_command );

			// Command/signal shortcut - set emulator field to "@<command>"
			std::string new_emu = "@";
			new_emu += new_command;

			replacement.set_info( FeRomInfo::Emulator, new_emu );
		}

		replacement.set_info( FeRomInfo::AltRomname, ctx.opt_list[2].get_value() );

		// Resave the romlist file that our romlist was loaded from
		//
		ctx.fe_settings.update_romlist_after_edit( m_rom_original, replacement );
	}

	return true;
}
