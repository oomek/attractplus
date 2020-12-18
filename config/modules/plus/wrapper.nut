class PLUS
{
	// We have to make a clone of fe table before overriding fe functions
	static FE = clone fe

	static OBJECTS = []
	static DRAWABLES = {}
	static FUNCTIONS = {}

	CALLBACKS = null

	constructor( caller )
	{
		CALLBACKS = {}
		OBJECTS.push( caller )
	}

	function init()
	{
		const DRW_DIR = "drawables/"
		const FNC_DIR = "functions/"

		local drw = zip_get_dir( FE.module_dir + DRW_DIR )
		foreach ( f in drw )
			FE.do_nut( FE.module_dir + DRW_DIR + f )

		local func = zip_get_dir( FE.module_dir + FNC_DIR )
		foreach ( f in func )
			FE.do_nut( FE.module_dir + FNC_DIR + f )

		::fe.add_ticks_callback( this, "tick" );
	}

	function tick( ttime ) { foreach ( o in OBJECTS ) foreach ( cb in o.CALLBACKS ) cb( o, ttime ) }
}

// Base class for Image Artwork and Surface
class PlusBaseImageWrapper extends PLUS
{
	object = null
	functions = null

	constructor( caller )
	{
		::print("PlusBaseImageWrapper\n")

		functions = {}

		// fe.Image functions wrappers
		caller.getclass().newmember( "set_rgb", function(...){ local a = vargv
			if ( a.len() == 3 ) object.set_rgb( a[0], a[1], a[2] )
			else throw "set_rgb() Wrong number of arguments." })

		caller.getclass().newmember( "set_pos", function(...){ local a = vargv
			if ( a.len() == 2 ) object.set_pos( a[0], a[1] )
			else if ( a.len() == 4 ) object.set_pos( a[0], a[1], a[2], a[3] )
			else throw "set_pos() Wrong number of arguments." })

		caller.getclass().newmember("swap", function(other){
			if ( typeof other == "instance" ) object.swap( other.object )
			else throw "Wrong argument in swap()" })

		caller.getclass().newmember( "fix_masked_image", function(...){
			throw "the index 'fix_masked_image' does not exist" })

		caller.getclass().newmember( "load_from_archive", function(...){
			throw "the index 'load_from_archive' does not exist" })

		base.constructor( caller )

		foreach ( name, func in FUNCTIONS )
		{
			caller.getclass().newmember( "add_" + name, function(...)
			{
				foreach ( name, func in this.getclass() )
					if ( typeof func == "function" )
						if ( ::callee() == func )
						{
							local func_name = name.slice(4)
							functions[func_name] <- FUNCTIONS[func_name].instance()
							if ( !functions[func_name].onAdd( object, vargv ) ) throw "Wrong number of arguments."
							if ( !(func_name in CALLBACKS )) CALLBACKS[func_name] <- ( functions[func_name].onTick )
						}
			})
		}
	}

	function _get( idx )
	{
		return object[idx]
	}

	function _set( idx, val )
	{
		object[idx] = val
	}
}

// fe.add_image() wrapper
class PlusImageWrapper extends PlusBaseImageWrapper
{
	constructor( parent, a )
	{
		::print("PlusImageWrapper "+a.len()+"\n")

		// variable arguments list parser
		if ( a.len() == 5 ) object = parent.add_image( a[0], a[1], a[2], a[3], a[4] )
		else if ( a.len() == 3 ) object = parent.add_image( a[0], a[1], a[2] )
		else if ( a.len() == 1 ) object = parent.add_image( a[0] )
		else throw "fe.add_image() Wrong number of arguments."

		base.constructor( this )

		this.getclass().newmember( "fix_masked_image", function(...){
			object.fix_masked_image() })

		this.getclass().newmember( "load_from_archive", function( archive, filename ){
			object.load_from_archive( archive, filename ) })
	}
}

// fe.add_artwork() wrapper
class PlusArtworkWrapper extends PlusBaseImageWrapper
{
	constructor( parent, a )
	{
		::print("PlusArtworkWrapper "+a.len()+"\n")

		// variable arguments list parser
		if ( a.len() == 5 ) object = parent.add_artwork(a[0], a[1], a[2], a[3], a[4] )
		else if ( a.len() == 3 ) object = parent.add_artwork( a[0], a[1], a[2] )
		else if ( a.len() == 1 ) object = parent.add_artwork( a[0] )
		else throw "fe.add_image() Wrong number of arguments."

		base.constructor( this )

		this.getclass().newmember( "fix_masked_image", function(...){
			object.fix_masked_image() })

		this.getclass().newmember( "load_from_archive", function( archive, filename ){
			object.load_from_archive( archive, filename ) })
	}
}

// fe.add_surface() wrapper
class PlusSurfaceWrapper extends PlusBaseImageWrapper
{
	constructor( parent, a )
	{
		::print("PlusSurfaceWrapper "+a.len()+"\n")

		// variable arguments list parser
		if ( a.len() == 2 ) object = parent.add_surface( a[0], a[1] )
		else throw "fe.add_surface() Wrong number of arguments."

		base.constructor( this )
	}

	// surface add_ functions wrappers
	function add_image(...) { return ::PlusImageWrapper( object, vargv ) }
	function add_artwork(...) { return ::PlusArtworkWrapper( object, vargv ) }
	function add_surface(...) { return ::PlusSurfaceWrapper( object, vargv ) }
}

// fe.add_ functions overrides
fe.add_image <- function(...){ return ::PlusImageWrapper( ::PLUS.FE, vargv ) }
fe.add_artwork <- function(...){ return ::PlusArtworkWrapper( ::PLUS.FE, vargv ) }
fe.add_surface <- function(...){ return ::PlusSurfaceWrapper( ::PLUS.FE, vargv ) }