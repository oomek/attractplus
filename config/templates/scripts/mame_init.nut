////////////////////////////////////////////////////////////////////////
//
// Attract-Mode Frontend - Emulator detection script (MAME)
//
// Copyright (c) 2017 Andrew Mickelson
//
// This program comes with ABSOLUTELY NO WARRANTY.  It is licensed under
// the terms of the GNU General Public License, version 3 or later.
//
////////////////////////////////////////////////////////////////////////
fe.load_module( "file" );

local DETECT_SYSTEMS = false;

// Our base default values for mame
//
mame_emu <-
{
	"name"   : "mame",
	"exe"    : "mame",
	"args"   : "[name]",
	"rompath": "$HOME/mame/roms/",
	"exts"   : ".zip;.7z",
	"system" : "Arcade",
	"source" : "listxml",
	"import_extras" : ""
};

// We only keep this at the nes settings if no non-arcade systems are
// detected and we are writing a "mess-style" template
//
console_emu <-
{
	"name"   : "mame-nes",
	"exe"    : "mame",
	"args"   : "[system] -cart \"[romfilename]\"",
	"rompath": "$HOME/mame/roms/nes/",
	"exts"   : ".zip;.7z",
	"system" : "nes;Nintendo Entertainment System (NES)",
	"source" : "listsoftware"
};

////////////////////////////////////////////////////////////////////////
//
// Paths to check for the MAME executable
//
////////////////////////////////////////////////////////////////////////
search_paths <- {

	"Linux" : [
		"/usr/games",
		"/usr/bin",
		"/usr/local/games",
		"/usr/local/bin",
		"$HOME/mame",
		"$HOME/sdlmame",
		"./mame",
		"../mame",
		],

	"Windows" : [
		"$HOME/mame/",
		"./mame/",
		"../mame/",
		],

	"OSX" : [
		"$HOME/mame",
		"$HOME/sdlmame",
		"./mame",
		"../mame",
		],
};

////////////////////////////////////////////////////////////////////////
//
// Potential names for the mame executable
//
////////////////////////////////////////////////////////////////////////
search_names <- {
	"Linux" : [
		"mame",
		"mame64",
		"mame32",
		"sdlmame",
		"groovymame",
		],

	"Windows" : [
		"mame.exe",
		"mame64.exe",
		"mame32.exe",
		"groovymame.exe",
		],

	"OSX" : [
		"mame",
		"mame64",
		"mame32",
		"sdlmame",
		"groovymame",
		]
};

local my_OS = OS;
if ( OS == "FreeBSD" )
	my_OS = "Linux";

////////////////////////////////////////////////////////////////////////
//
// Classes and functions used in script
//
////////////////////////////////////////////////////////////////////////

// add trailing slash if path not empty
function add_slash( path )
{
	if ( !path.len() ) return path;
	if ( path[ path.len()-1 ] == '/' ) return path;
	if ( path[ path.len()-1 ] == '\\' ) return path;
	return path + "/";
}

// remove quotes
function unquote( path )
{
	if ( path.len() < 2 ) return path;
	if ( path[0] != '"' ) return path;
	if ( path[path.len() - 1] != '"' ) return path;
	return path.slice( 1, path.len() - 1 );
}

////////////////////////////////////////////////////////////////////////
class RompathParser
{
	exepath = "";
	homepath = "";
	rompaths = [];

	constructor( p, e )
	{
		exepath = p;
		homepath = p; // Default to the exepath, updated from mame config if different
		rompaths = [];

		console_message( "Running", p + e + " -showconfig" );
		fe.plugin_command( p + e, "-showconfig", this, "parse_cb" );
	}

	function _tailing1( t )
	{
		local retval = "";
		for ( local i=1; i<t.len(); i++ )
		{
			retval += "/";
			retval += t[i];
		}
		return retval;
	}

	function parse_cb( op )
	{
		op = strip( op );
		local temp = split( op, " \t\n" );

		if ( temp.len() < 2 )
			return;

		temp[1] = unquote( strip( op.slice( temp[0].len() ) ) ); // capture paths with spaces, and remove quotes

		if (( temp[0] == "homepath" ))
		{
			local t = split( temp[1], "/\\" );
			if ( t.len() )
			{
				if (( t[0] == "$HOME" ) || ( t[0] == "~" ))
					homepath = fe.path_expand( t[0] ) + _tailing1( t );
				else if ( t[0] == "." )
					homepath = exepath + _tailing1( t );
				else if ( t[0] == ".." )
					homepath = exepath; // (unsupported)
				else
					homepath = fe.path_expand( temp[1] );

				console_message( "Found homepath", homepath );
			}
		}
		else if ( temp[0] == "rompath" )
		{
			local t = split( temp[1], ";" );
			if ( t.len() )
			{
				foreach ( p in t )
				{
					// make sure there is a trailing slash
					local p2 = add_slash(strip( p ));
					if ( p2.len() )
					{
						local rompath = "";
						if ( fe.path_test( p2, PathTest.IsRelativePath ) )
						{
							if ( homepath.len() && fe.path_test( homepath + p2, PathTest.IsDirectory ) )
								rompath = homepath + p2;
							else if ( fe.path_test( exepath + p2, PathTest.IsDirectory ) )
								rompath = exepath + p2;
						}
						else
						{
							rompath = fe.path_expand( p2 );
						}

						if ( rompath.len() )
						{
							rompaths.push( rompath );
							console_message( "Found rompath", rompath );
						}
					}
				}
			}
		}
	}
};

////////////////////////////////////////////////////////////////////////
class VersionParser
{
	version=0;

	constructor( p, e )
	{
		version = 0;

		// This is the first time we are running an executable we suspect is mame
		// we use the timeout command where available in case the executable isn't
		// what we expect (in which case it will be killed soon after running
		//
		console_message( "Running", p + e + " -help" );
		if ( my_OS == "Windows" )
			fe.plugin_command( p + e, "-help", this, "parse_cb" );
		else
			fe.plugin_command( "timeout", "8s " + p + e + " -help", this, "parse_cb" );
	}

	function parse_cb( op )
	{
		if ( version == 0 )
		{
			local temp = split( op, " \t\n" );
			if ( temp.len() < 2 )
			{
				version = -1;
				return;
			}

			local temp2 = split( temp[1], "." );
			if ( temp2.len() < 2 )
			{
				version = -1;
				return;
			}

			version = temp2[1].tointeger();
		}
	}
}

class FullNameParser
{
	list = null;

	constructor( p, e )
	{
		list = {}
		console_message( "Running", p + e + " -listfull" );
		fe.plugin_command( p + e, "-listfull", this, "parse_cb" );
	}

	function parse_cb( op )
	{
		local parts = split( op, " \t\n" );
		local name = parts[0];
		local full = unquote( strip( op.slice( name.len() ) ) );
		list[name] <- strip( split( full, "(/" )[0] );
	}

	function get_fullname( name )
	{
		return ( name in list ) ? list[name] : null;
	}
}

class MediaParser
{
	list = null;
	count = 0;

	constructor( p, e )
	{
		list = {}
		console_message( "Running", p + e + " -listmedia" );
		fe.plugin_command( p + e, "-listmedia", this, "parse_cb" );
	}

	function parse_cb( op )
	{
		count++;
		if (!(count % 5000)) console_message("Reading...");

		local parts = split( op, " \t\n" );
		if (parts.len() < 4)
			return;

		local name = parts[0]
		if (!(name in list)) list[name] <- { media = parts[1], extensions = [] };
		list[name].extensions.extend(parts.slice(3).map(@(e) strip(e)).filter(@(i,e) e.len()));
	}

	function get_content( name )
	{
		return ( name in list ) ? list[name] : null;
	}
};

////////////////////////////////////////////////////////////////////////
//
// Ok, start doing stuff now...
//
////////////////////////////////////////////////////////////////////////

// search for file
//
console_message( "Searching for exe..." );
local res = search_for_file( search_paths[ my_OS ], search_names[ my_OS ] );

// run 'mame -help' to get the version of the MAME we found
//
local ver=0;
if ( res )
{
	console_message( "Found exe", res[0] + res[1] );
	local vp = VersionParser( res[0], res[1] );
	ver = vp.version;
}

// If we didn't get a version, then we don't really know what we are
// dealing with...
//
if ( ver <= 0 )
{
	console_message( "Cannot find exe" );
	// No MAME found, write templates and get out of here...
	//
	console_report( mame_emu["name"], "" );
	console_report( console_emu["name"], "" );
	local wm = write_config( mame_emu, FeConfigDirectory + "templates/emulators/" + mame_emu["name"] + ".cfg" );
	local wc = write_config( console_emu, FeConfigDirectory + "templates/emulators/" + console_emu["name"] + ".cfg", true );
	return;
}
else
{
	console_message( "Found version", ver );
}

local path = res[0];
local executable = res[1];
local systems = [];

function is_duplicate_system( name )
{
	foreach ( s in systems )
	{
		if ( s.name == name )
			return true;
	}

	return false;
}

console_message( "Parsing config..." );
local rp = RompathParser( path, executable );

console_message( "Searching for extras..." );
// Check for extras files
//
ext_files <-
[
	"catver.ini",
	"series.ini",
	"languages.ini",
	"nplayers.ini"
];

ext_paths <- [ path ];

if ( rp.homepath != path )
	ext_paths.push( rp.homepath );

// "folders" is the "official" path for ini files
ext_paths.push( add_slash( path ) + "folders/" )

if ( my_OS == "Linux" )
	ext_paths.push( fe.path_expand( "$HOME/.mame/" ) );

foreach ( ef in ext_files )
{
	foreach ( ep in ext_paths )
	{
		if ( fe.path_test( ep + ef, PathTest.IsFile ) )
		{
			mame_emu["import_extras"] += ep + ef + ";";
			console_message( "Found extra", ep + ef );
			break;
		}
	}
}

local emu_dir = FeConfigDirectory + "emulators/";

// Write emulator config for arcade
//
mame_emu["exe"] = path + executable;
mame_emu["workdir"] <- rp.homepath;

mame_emu["rompath"] <- "";
foreach ( r in rp.rompaths )
	mame_emu["rompath"] += r + ";";

console_message( "Writing templates..." );
console_report( mame_emu["name"], mame_emu["exe"] );
write_config( mame_emu, FeConfigDirectory + "templates/emulators/" + mame_emu["name"] + ".cfg", true );

emulators_to_generate.push( mame_emu["name"] );


// Search for console systems (we assume they are organized as subdirectories
// of the rom paths, with the sub name being the name of the system)
//
// i.e. <rompath>/nes/ for nintendo entertainment system, etc.
//
if ( DETECT_SYSTEMS && ver >= 162 ) // mame and mess merged as of v 162
{
	console_message( "Parsing fullname..." );
	local fp = FullNameParser( path, executable );

	// Parsing the full-media listing is faster than requesting an individual listing for every subdir
	console_message( "Parsing media..." );
	local mp = MediaParser( path, executable );

	console_message( "Parsing systems..." );
	foreach ( p in rp.rompaths )
	{
		local dl = DirectoryListing( p, false );

		foreach ( sd in dl.results )
		{
			sd = sd.tolower();
			local fullname = fp.get_fullname(sd);
			if ( fullname == null )
				continue;

			local content = mp.get_content(sd);
			if ( content == null )
				continue;

			if ( is_duplicate_system( sd ) )
				continue;

			if ( !fe.path_test( p + sd, PathTest.IsDirectory ) )
				continue;

			systems.push({
				name = sd,
				fullname = fullname,
				triggerpath = p + sd,
				extensions = content.extensions,
				media = content.media,
			});
		}
	}
}

// Write emulator configs for each console system
//
console_emu["exe"] <- path + executable;
console_emu["workdir"] <- rp.homepath;

// Write configs for every console system found
foreach ( s in systems )
{
	if ( s.media.len() )
		console_emu["args"] <- "[system] -" + s.media + " \"[romfilename]\"";

	console_emu["name"] <- "mame-" + s.name;
	console_emu["system"] = s.name;

	if ( s.fullname.len() )
		console_emu["system"] += ";" + s.fullname;

	console_emu["rompath"] <- "";
	foreach ( r in rp.rompaths )
		console_emu["rompath"] += r + s.name + ";";

	console_emu["exts"] <- ".zip;.7z;";
	foreach ( e in s.extensions )
		console_emu["exts"] += e + ";";

	local cfg_fn = emu_dir + console_emu["name"] + ".cfg";
	local tmp_fn = FeConfigDirectory + "templates/emulators/" + console_emu["name"] + ".cfg";

	console_report( console_emu["name"], s.triggerpath );
	write_config( console_emu, tmp_fn, true );

	emulators_to_generate.push( console_emu["name"] );
}

if ( systems.len() == 0 )
	write_config( console_emu, FeConfigDirectory + "templates/emulators/" + console_emu["name"] + ".cfg" );
