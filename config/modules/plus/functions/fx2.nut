::print( "Function FX2 Loaded\n" )

PLUS.FUNCTIONS.fx2 <-
{
	function onAdd( object, ... )
	{
		::print( "Function FX2 Add\n" )
		object.blue = 255
	}

	function onTick ( object, ttime )
	{
		// object.blue += 10
		// if ( object.blue > 255 ) object.blue = 0
	}
}