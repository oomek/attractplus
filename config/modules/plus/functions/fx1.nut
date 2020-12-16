::print( "Function FX1 Loaded\n" )

PLUS.FUNCTIONS.fx1 <-
{
	function onAdd( object, ... )
	{
		::print( "Function FX1 Add\n" )
		object.red = 255
	}

	function onTick ( object, ttime )
	{
		// object.red += 10
		// if ( object.red > 255 ) object.red = 0
	}
}