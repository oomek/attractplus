::print( "Function FX1 Loaded\n" )

class PLUS.FUNCTIONS["fx1"]
{
	function onAdd( object, args )
	{
		if ( args.len() > 0 ) return false
		object.red = 255
		return true
	}

	function onTick ( object, ttime )
	{
		object.red += 10
		if ( object.red > 255 ) object.red = 0
	}
}