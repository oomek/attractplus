///////////////////////////////////////////////////
//
// Attract-Mode Frontend - TOS GRS Restrictor Plate Switcher plugin
//
// For use with the command-line utilities provided by ThunderStickStudios.com 
// by @concentrateddon
//
// NOTES
// This will attempt to use the tos428cl.exe tool to figure out
// which "COM" port the controller board is connected to via USB
// so generally you can ignore the "Port" configuration
// This assumes your ROMlist has a field ('Control') that contains
// 4, 8, or X. 4 or 8 will make the joystick switch to that way
// before launching MAME; X won't try to switch at all.
//
///////////////////////////////////////////////////

//
// The UserConfig class identifies plugin settings that can be configured
// from Attract-Mode's configuration menu
//
class UserConfig </ help="Integration plug-in for use with the command line utility provided by ThunderStickStudios.com" /> {

	</ label="Command", help="Path to the command line utility", order=1 />
	command="C:\\Program Files (x86)\\tos428cl\\tos428cl.exe";

	</ label="Port", help="Run tos428cl showports to find this; leave at COM22 to attempt to auto-discover each time this runs", order=2 />
	port="COM22";

	</ label="Joystick", help="Which joystick to switch", options="1,2,3,4,all" order=3 />
	joystick="all";

	</ label="ROMlist Field", help="The game info field that contains 4 or 8 (or X to not switch)", options="Control", order=4 />
	romlist_field="Control";
}

local config=fe.get_config(); // get user config settings corresponding to the UserConfig class above

//
// Copy the configured values from uconfig so we can use them
// whenever the transition callback function gets called
//

local game_field=Info.Control;
if ( config["romlist_field"] == "Control" )
	game_field=Info.Control;

fe.add_transition_callback( "tosgrs_plugin_transition" );

// try to get the port we're plugged into
function getport(op)
{
	// op will have the form tos428 found at COM3
	// we just want the COM3 part
	print(op + "\n");
	local a = split(op," ");
	print(a[3] + "\n");
	config["port"]=a[3];
	return false;
}

print(" Getting COM port for joystick servo \n");
fe.plugin_command (config["command"], "getport", getport);

function tosgrs_plugin_transition( ttype, var, ttime ) {

	if ( ScreenSaverActive )
		return false;

	switch ( ttype )
	{
		case Transition.ToGame:
		{
			if ( fe.game_info( game_field) != "X" )
			{
				fe.plugin_command( config["command"], 
					config["port"] + " setway," + config["joystick"] + "," + fe.game_info( game_field ));
				break;
			}
		}
	}

	return false; // must return false
}
