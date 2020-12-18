::print( "Function FX2 Loaded\n" )

class PLUS.FUNCTIONS["fx2"]
{
	function onAdd( object, args )
	{
		if ( args.len() > 0 ) return false
		object.blue = 255
		return true
	}

	function onTick ( object, ttime )
	{
		object.blue += 10
		if ( object.blue > 255 ) object.blue = 0
	}
}