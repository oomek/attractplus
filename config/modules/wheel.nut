/*
################################################################################

Attract-Mode Frontend - Wheel module v1.4.0
Provides an animated artwork strip

by Oomek - Radek Dutkiewicz 2026
https://github.com/oomek/attract-extra

################################################################################


INITIALIZATION:
--------------------------------------------------------------------------------
local wheel = fe.add_wheel( preset )

preset can be one of the following:

string - for example "arch-vertical"
  Loads a preset from modules/wheel-presets folder (no extension)
  EXAMPLE:local wheel = fe.add_wheel( "arch-vertical" )


file path - for example "my_wheel.nut"
  Loads a wheel preset from the layout folder
  EXAMPLE: local wheel = fe.add_wheel( "my_wheel.nut" )


table - for example my_preset defined inside the layout code as local table
  EXAMPLE: local wheel = fe.add_wheel( my_preset )
  See EXAMPLES section to learn preset table structure


WHEEL PROPERTIES:
--------------------------------------------------------------------------------
x/y
  The position of the wheel anchor point on the screen.

alpha
  The overall alpha transparency of the wheel (0-255).
  This affects all slots proportionally.
  Default: 255

speed
  The animation speed in milliseconds for wheel transitions.
  Lower values = faster animation.
  Default: 500

slots
  Total number of slots in the wheel ( including 2 slots on edges that fade to alpha 0 )
  Default: 9 (set in preset or config)

artwork_label
  The artwork identifier to display in the wheel slots.
  Examples: "snap", "wheel", "marquee", "flyer", etc.
  Default: "snap"

video_flags
  Video playback flags for artwork. Can be combined with | operator.
  Default: Vid.Default

blend_mode
  The blending mode for slots.
  Default: BlendMode.Alpha

zorder
  The base z-order for all wheel slots.
  Default: 0

zorder_offset
  Offset applied to the z-order calculation of slots.
  Affects which slot appears on top.
  Default: 0

index_offset
  Default: 0 ( current game is in the middle slot )

preserve_aspect_ratio
  If true, slot images maintain their aspect ratio.
  If false, images stretch to fill slot dimensions.
  Default: false

trigger
  Default: Transition.ToNewSelection

acceleration
  Enable/disable wheel scrolling acceleration when holding navigation keys.
  When true, allows faster scrolling when AM+ accelerates navigation.
  When false, enforces the speed limit even during fast navigation.
  Default: true

anchor
  The anchor point for wheel positioning
  Options: Wheel.Anchor.Centre
           Wheel.Anchor.Left
           Wheel.Anchor.Right
           Wheel.Anchor.Top
           Wheel.Anchor.Bottom
           Wheel.Anchor.TopLeft
           Wheel.Anchor.TopRight
           Wheel.Anchor.BottomLeft
           Wheel.Anchor.BottomRight
  Default: Wheel.Anchor.Centre


READ-ONLY PROPERTIES:
--------------------------------------------------------------------------------
spinning
  Returns true if the wheel is currently animating, false otherwise.
  Useful for hiding UI elements during wheel animation.

sel_slot
  Reference to the currently selected slot's image object.
  Allows direct manipulation of the selected slot's properties.

slots
  Array of all slot image objects in the wheel.



PRESET-SPECIFIC PROPERTIES:
--------------------------------------------------------------------------------
Presets may define additional properties. Access them directly on the wheel object.
Example for arch-vertical preset:

arch
  Curvature of the arch in degrees (-180 to 180).
  Negative values reverse the arch direction.
  Default: 90

slot_aspect_ratio
  Width to height ratio.
  Default: 2.0

slot_scale
  Global scale multiplier for all slots.
  Default: 1.0

slot_width
  Fixed width for slots. Set to > 0 to override dynamic sizing.
  Default: 0

slot_height
  Fixed height for slots. Set to > 0 to override dynamic sizing.
  Default: 0

sel_slot_scale
  Scale multiplier for the selected slot.
  Default: 1.0

sel_slot_scale_spread
  How many neighboring slots are affected by sel_slot_scale.
  Default: 1.0

scale_fix
  false - slots remain same size
  true - slots scale with arch to maintain constant wheel height
  Default: false


LAYOUT ARRAYS:
--------------------------------------------------------------------------------
Presets define layout arrays that control slot properties per position.
Common layout arrays include:

  layout.x[] - X position for each slot
  layout.y[] - Y position for each slot
  layout.width[] - Width for each slot
  layout.height[] - Height for each slot
  layout.alpha[] - Alpha transparency for each slot (0-255)
  layout.rotation[] - Rotation angle for each slot
  layout.origin_x[] - X origin point for rotation/scaling
  layout.origin_y[] - Y origin point for rotation/scaling

These arrays are automatically interpolated during wheel animation.


EXAMPLES:
--------------------------------------------------------------------------------

LEVEL 1 - Loading a predefined preset:
--------------------------------------
fe.load_module("wheel")

local wheel = fe.add_wheel( "arch-vertical" )

// Set wheel properties
wheel.x = fe.layout.width * 0.8
wheel.y = fe.layout.height / 2
wheel.speed = 800
wheel.artwork_label = "snap"
wheel.preserve_aspect_ratio = false

// Set preset-specific properties
wheel.arch = 100
wheel.sel_slot_scale = 1.5


LEVEL 2 - Modifying a preset:
-----------------------------
fe.load_module("wheel")

local preset =
{
	function init()
	{
		// Load base preset from modules/wheel-presets folder
		preset <- "arch-vertical"

		// Override wheel properties
		slots <- 15
		speed <- 600

		// Override preset properties
		arch <- 80
		sel_slot_scale <- 2.0
		sel_slot_scale_spread <- 3
	}
}

local wheel = fe.add_wheel( preset )


LEVEL 3 - Animating preset properties with Inertia
---------------------------------------------------
fe.load_module("wheel")
fe.load_module("inertia")

local wheel = fe.add_wheel( "arch-vertical" )
wheel.speed = 500

// Bind Inertia to wheel preset properties
wheel = Inertia( wheel, 1000, "sel_slot_scale", "alpha" )
wheel.to_sel_slot_scale = 2.0
wheel.to_alpha = 50
wheel.delay_alpha = 1500

fe.add_transition_callback( "on_transition" )
function on_transition( ttype, var, ttime )
{
	switch ( ttype )
	{
		case Transition.FromOldSelection:
			wheel.to_sel_slot_scale = 1.0
			wheel.delay_sel_slot_scale = 400
			wheel.to_alpha = 255
			wheel.delay_alpha = 0
			break

		case Transition.EndNavigation:
			wheel.to_sel_slot_scale = 2.0
			wheel.delay_sel_slot_scale = 0
			wheel.to_alpha = 50
			wheel.delay_alpha = 1500
			break
	}
}


LEVEL 4 - Creating a custom preset
----------------------------------
fe.load_module("wheel")

local my_preset =
{
	function init()
	{
		// Wheel properties
		x <- parent.width / 2
		y <- parent.height / 2
		width <- parent.width
		haight <- parent.height
		slots <- 7
		speed <- 400
		artwork_label <- "snap"
		preserve_aspect_ratio <- false
		video_flags <- Vid.ImagesOnly
		anchor <- Wheel.Anchor.Centre

		// Custom preset properties
		slot_scale <- 1.0

		// Declare layout arrays
		layout.x <- []
		layout.width <- []
		layout.height <- []
		layout.alpha <- [0,150,150,255,150,150,0]
	}

	function update()
	{
		local slot_size = parent.width / (slots - 2)

		// Populate layout arrays based on properties
		for ( local i = 0; i < slots; i++ )
		{
			layout.x[i] = slot_size * ( i - slots / 2 ) * slot_scale
			layout.width[i] = slot_size * slot_scale
			layout.height[i] = slot_size * slot_scale
			layout.alpha[i] = 255
		}
	}
}

local wheel = fe.add_wheel( my_preset )

fe.add_ticks_callback( "on_tick" )
function on_tick ( ttime )
{
	wheel.slot_scale = sin( ttime * 0.002 ) * 0.1 + 1.0
}


NOTES:
--------------------------------------------------------------------------------
- First and last slots always fade to alpha 0
- The selected slot's video_flags removes Vid.NoAudio flag when navigation ends
- Changing preset properties triggers automatic update() if the function exists
- Page size is automatically adjusted based on slots count

################################################################################
*/

if ( FeVersionNum < 320 ) { fe.log( "ERROR: Wheel module v1.4.0 requires Attract-Mode Plus 3.2.0 or greater"); return }

fe.load_module( "inertia" )

class Wheel
{
	static VERSION = 140
	static PRESETS_DIR = ::fe.module_dir + "wheel-presets/"
	static SCRIPT_DIR = ::fe.script_dir
	static SELECTION_SPEED = ::fe.get_general_config().selection_speed_ms.tointeger()
	FRAME_TIME = null

	// properties
	x = null
	y = null
	alpha = null

	// locals
	cfg = null
	preset = null
	layout = null
	surface = null
	parent = null
	anchor = null
	anim = null
	anim_int = null
	slots = null
	queue = null
	queue_next = null
	queue_load = null
	wheel_idx = null
	max_idx_offset = null
	end_idx_old = null
	end_idx_offset = null
	end_navigation = null
	selected_slot_idx = null
	selection_time_old = null
	resync = null
	sel_slot = null


	constructor( ... )
	{
		// Variable arguments parser
		local config = ""

		if ( vargv.len() == 1 )
		{
			config = vargv[0]
			parent = ::fe.layout
			surface = ::fe
		}
		else if ( vargv.len() == 2 )
		{
			config = vargv[0]
			parent = vargv[1]
			surface = vargv[1]
		}
		else
			throw "add_wheel: Wrong number of parameters\n"

		// Config / Preset Loader
		if ( typeof config == "string" )
		{
			if ( ends_with( config, ".nut" ))
				cfg = ::dofile( SCRIPT_DIR + config, true )
			else
				cfg = ::dofile( PRESETS_DIR + config + ".nut", true )

			layout = {}
			cfg.layout <- {}
			cfg.parent <- parent
			cfg.init()
		}
		else
		{
			cfg = config
			layout = {}
			cfg.layout <- {}
			cfg.parent <- parent
			cfg.init()
		}

		if ( "preset" in cfg )
		{
			preset = ::dofile( PRESETS_DIR + cfg.preset + ".nut", true )
			delete cfg.preset
			preset.layout <- {}
			preset.parent <- parent
			preset.init()
		}

		// Copying preset parameters to config
		if ( preset )
			foreach ( k, v in preset )
				if ( !( k in cfg ) && k != "layout" && typeof v != "function" )
					cfg[k] <- preset[k]

		// Parsing Wheel's properties
		if ( "x" in cfg ) x = cfg.x; else x = 0
		if ( "y" in cfg ) y = cfg.y; else y = 0
		if ( "alpha" in cfg ) alpha = cfg.alpha; else alpha = 255

		// Parsing Wheel's config
		if ( !("slots" in cfg )) cfg.slots <- 9
		if ( !("speed" in cfg )) cfg.speed <- 500
		if ( !("anchor" in cfg )) cfg.anchor <- ::Wheel.Anchor.Centre
		if ( !("artwork_label" in cfg )) cfg.artwork_label <- "snap"
		if ( !("video_flags" in cfg )) cfg.video_flags <- Vid.Default
		if ( !("blend_mode" in cfg )) cfg.blend_mode <- BlendMode.Alpha
		if ( !("zorder" in cfg )) cfg.zorder <- 0
		if ( !("zorder_offset" in cfg )) cfg.zorder_offset <- 0
		if ( !("index_offset" in cfg )) cfg.index_offset <- 0
		if ( !("preserve_aspect_ratio" in cfg )) cfg.preserve_aspect_ratio <- false
		if ( !("trigger" in cfg )) cfg.trigger <- Transition.ToNewSelection
		if ( !("acceleration" in cfg )) cfg.acceleration <- true
		if ( !("preset" in cfg )) cfg.preset <- ""

		// Copying updated config parameters back to preset
		if ( preset )
			foreach ( k, v in cfg )
				if ( k != "layout" && typeof v != "function" )
					preset[k] <- cfg[k]

		// Copying array pointers from config and preset to master layout table
		if ( preset )
		{
			foreach ( k, v in preset.layout )
			{
				if ( v.len() == 0 ) preset.layout[k] = ::array( cfg.slots, 0.0 )
				if ( !( k in layout )) layout[k] <- []
				layout[k] = preset.layout[k]
			}
		}

		foreach ( k, v in cfg.layout )
		{
			if ( v.len() == 0 ) cfg.layout[k] = ::array( cfg.slots, 0.0 )
			if ( !( k in layout )) layout[k] <- []
			layout[k] = cfg.layout[k]
		}

		// First update
		if ( "update" in preset ) preset.update()
		if ( "update" in cfg ) cfg.update()

		// Initializing locals
		anim = ::Inertia( 0.0, cfg.speed, 0.0 )
		anim.mass = 1.0
		anim_int = 0
		slots = []
		queue = []
		queue_next = 0
		queue_load = 0
		wheel_idx = 0
		max_idx_offset = cfg.slots / 2
		end_idx_old = 0
		end_idx_offset = 0
		end_navigation = true
		selected_slot_idx = max_idx_offset - cfg.index_offset
		selection_time_old = 0
		resync = false
		FRAME_TIME = 1000 / ScreenRefreshRate

		// Create alpha array if not defined
		if ( !("alpha" in layout )) layout.alpha <- ::array( cfg.slots, 255 )

		// Setting alpha of offside slots to 0
		layout.alpha[0] = 0
		layout.alpha[cfg.slots - 1] = 0

		// Creating an array of images
		for ( local i = 0; i < cfg.slots; i++ )
		{
			local s = surface.add_image( "", 0, 0, 1, 1 )
			s.video_flags = cfg.video_flags
			s.preserve_aspect_ratio = cfg.preserve_aspect_ratio
			s.mipmap = true
			s.blend_mode = cfg.blend_mode
			s.zorder =  max_idx_offset - ::abs( i + cfg.zorder_offset - max_idx_offset ) + cfg.zorder + ::abs( cfg.zorder_offset )
			slots.push( s )
		}

		// To be accessed by the layout
		sel_slot = slots[selected_slot_idx]
		cfg.images <- []
		cfg.images = slots

		// Binding callbacks
		::fe.add_ticks_callback( this, "on_tick" )
		::fe.add_transition_callback( this, "on_transition" )
	}
}

function Wheel::on_transition( ttype, var, ttime )
{
	if ( ttype == Transition.ToNewList )
	{
		reload_slots()
	}

	else if ( ttype == Transition.ToNewSelection )
	{
		if ( ::fe.list.size > 0 )
		{
			if ( cfg.trigger == ttype )
			{
				foreach ( s in slots ) s.video_flags = cfg.video_flags | Vid.NoAudio

				if ( ::abs( var ) == 1 )
					queue.push( var )
				else
					queue.push( idx2off( ::fe.layout.index + var, ::fe.layout.index ))
				end_navigation = false
			}
			else end_idx_offset += var
		}
		// Image preload
		if ( var > 0 ) ::fe.image_cache.add_image( ::fe.get_art( cfg.artwork_label, var + max_idx_offset + cfg.index_offset, 0, cfg.video_flags ))
		else ::fe.image_cache.add_image( ::fe.get_art( cfg.artwork_label, var - max_idx_offset + cfg.index_offset, 0, cfg.video_flags ))
	}

	else if ( ttype == Transition.FromOldSelection )
	{
	}

	else if ( ttype == Transition.EndNavigation )
	{
		if ( ::fe.list.size > 0 )
		{
			if ( cfg.trigger == ttype )
			{
				foreach ( s in slots ) s.video_flags = cfg.video_flags | Vid.NoAudio

				if ( ::abs( end_idx_offset ) == 1 )
					queue.push( end_idx_offset )
				else
					queue.push( idx2off( end_idx_old + end_idx_offset, end_idx_old ) )
			}

			end_idx_old = ::fe.layout.index
			end_idx_offset = 0
			end_navigation = true
		}
	}

	return false
}

function Wheel::on_tick( ttime )
{
	// ANIMATING THE WHEEL
	if ( queue_load == 0 && queue_next == 0 ) resync = false

	if ( queue.len() > 0 )
		if ( resync == false || (( ::sign( queue[0] ) == ::sign( -anim.velocity ) ) && ( ::sign( queue[0] ) == ::sign( queue_next ) || ( queue_next == 0 ))))
			queue_next += queue.remove(0)

	if ( queue_next != 0 )
	{
		if ( cfg.acceleration || ::fe.layout.time - selection_time_old > SELECTION_SPEED )
		{
			selection_time_old = ::fe.layout.time
			local dir = ::sign( queue_next )
			if ( ::abs( queue_next + queue_load ) > cfg.slots )
			{
				local jump = queue_next + queue_load - cfg.slots * dir
				wheel_idx = ::modulo( wheel_idx + jump, ::fe.list.size )
				queue_next -= jump
				resync = true
			}
			if ( queue_next != 0 )
			{
				anim.from += dir
				anim_int -= dir
				queue_load += dir
				queue_next -= dir
			}
		}
	}

	// Handling audio flags
	if ( end_navigation == true && wheel_idx == ::fe.layout.index && queue_load == 0 )
	{
		slots[selected_slot_idx].video_flags = cfg.video_flags
		end_navigation = false
	}

	// LOADING ARTWORK
	if ( anim.get + anim_int <= -0.5 && ( anim.velocity < 0.0 || cfg.speed < FRAME_TIME ))
	{
		swap_slots( 1 )
		anim_int++
		queue_load--
		wheel_idx++
		wheel_idx = ::modulo( wheel_idx, ::fe.list.size )
		slots[cfg.slots - 1].video_flags = cfg.video_flags | Vid.NoAudio
		slots[cfg.slots - 1].file_name = ::fe.get_art( cfg.artwork_label,
			idx2off( wheel_idx + max_idx_offset + cfg.index_offset, ::fe.list.index ),
			0,
			cfg.video_flags & Art.ImagesOnly )
	}
	if ( anim.get + anim_int >= 0.5 && ( anim.velocity > 0.0 || cfg.speed < FRAME_TIME ))
	{
		swap_slots( -1 )
		anim_int--
		queue_load++
		wheel_idx--
		wheel_idx = ::modulo( wheel_idx, ::fe.list.size )
		slots[0].video_flags = cfg.video_flags | Vid.NoAudio
		slots[0].file_name = ::fe.get_art( cfg.artwork_label,
			idx2off( wheel_idx - max_idx_offset + cfg.index_offset, ::fe.list.index ),
			0,
			cfg.video_flags & Art.ImagesOnly )
	}

	// SETTING PROPERTIES
	local mix_amount = anim.get % 1.0
	if ( anim.get <= 0 ) mix_amount += 1
	local mix_idx2 = ::floor( 0.5 - mix_amount )
	local mix_idx1 = mix_idx2 + 1

	// Applying interpolated layout properties to slots
	foreach ( name, prop in layout )
	{
		for ( local i = 0; i < cfg.slots; i++ )
		{
			local idx1 = ::modulo( mix_idx1 + i, cfg.slots )
			local idx2 = ::modulo( mix_idx2 + i, cfg.slots )
			slots[i][name] = ::mix( prop[idx2], prop[idx1], mix_amount )
		}
	}

	// Adjusting slots position properties based on the wheel's position and anchor
	foreach ( name, prop in layout )
	{
		for ( local i = 0; i < cfg.slots; i++ )
		{
			if ( name == "x" ) slots[i][name] += x
			else if ( name == "y" ) slots[i][name] += y
			else if ( name == "alpha" ) slots[i][name] = slots[i][name] / 255.0 * alpha
			else if ( name == "origin_x" ) slots[i][name] += slots[i].width * cfg.anchor[0]
			else if ( name == "origin_y" ) slots[i][name] += slots[i].height * cfg.anchor[1]
		}
	}

	// Center slots if origin not defined in layout
	foreach ( i, s in slots )
	{
		if ( !( "x" in layout )) s.x = x
		if ( !( "y" in layout )) s.y = y
		if ( !( "origin_x" in layout )) s.origin_x = s.width * cfg.anchor[0]
		if ( !( "origin_y" in layout )) s.origin_y = s.height * cfg.anchor[1]
	}

	for ( local i = 0; i < cfg.slots; i++ ) slots[i].visible = slots[i].alpha
}

function Wheel::_get( idx )
{
	switch ( idx )
	{
		case "x":
		case "y":
		case "alpha":
			return idx

		case "spinning":
			return anim.get != 0

		default:
			if ( idx in this ) return idx
			else if ( idx in preset ) return preset[idx]
			else if ( idx in cfg ) return cfg[idx]
	}
}

function Wheel::_set( idx, val )
{
	switch ( idx )
	{
		case "x":
		case "y":
		case "alpha":
			idx = val
			break

		case "speed":
			cfg.speed = val
			anim.time = val
			break

		case "preserve_aspect_ratio":
			foreach( s in slots ) s[idx] = val
			break

		case "zorder":
			cfg.zorder = val
			foreach( i, s in slots )
				s.zorder =  max_idx_offset - ::abs( i + cfg.zorder_offset - max_idx_offset ) + cfg.zorder + ::abs( cfg.zorder_offset )
			break

		default:
			if ( idx in this ) idx = val
			else if ( idx in preset ) preset[idx] = val
			else if ( idx in cfg ) cfg[idx] = val
			if ( "update" in preset ) preset.update()
			if ( "update" in cfg ) cfg.update()
	}

	// Reset the alpha of edge slots in case the preset ovverrides it in update()
	layout.alpha[0] = 0
	layout.alpha[cfg.slots - 1] = 0
}

Wheel.Anchor <-
{
	Centre = [0.5,0.5]
	Left = [0.0,0.5]
	Right = [1.0,0.5]
	Top = [0.5,0.0]
	Bottom = [0.5,1.0]
	TopLeft = [0.0,0.0]
	TopRight = [1.0,0.0]
	BottomLeft = [0.0,1.0]
	BottomRight = [1.0,1.0]
}

function Wheel::reload_slots()
{
	::fe.list.page_size = ::min( cfg.slots, ::max( ::fe.list.size / 2, 1 ))
	wheel_idx = ::fe.list.index
	end_idx_old = ::fe.list.index
	end_idx_offset = 0
	anim_int = 0
	anim.set = 0.0
	queue_next = 0
	queue_load = 0

	for ( local i = 0; i < cfg.slots; i++ )
	{
		slots[i].file_name = ::fe.get_art( cfg.artwork_label,
			i - max_idx_offset + cfg.index_offset,
			0,
			cfg.video_flags & Art.ImagesOnly )

		slots[i].video_flags = cfg.video_flags | Vid.NoAudio
	}

	slots[selected_slot_idx].video_flags = cfg.video_flags
}

function Wheel::idx2off( new, old )
{
	local positive = ::modulo( new - old, ::fe.list.size )
	local negative = ::modulo( old - new, ::fe.list.size )
	if ( positive > negative )
		return -negative
	else
		return positive
}

function Wheel::swap_slots( dir )
{
	if ( dir == 1 )
		for ( local i = 1; i < cfg.slots; i++ ) slots[i].swap( slots[i - 1] )
	else
		for ( local i = cfg.slots - 1; i > 0; i-- ) slots[i].swap( slots[i - 1] )
}

function Wheel::ends_with( name, string )
{
	if ( name.slice( name.len() - string.len(), name.len()) == string )
		return true
	else
		return false
}

// Binding the wheel to fe
fe.add_wheel <- Wheel
