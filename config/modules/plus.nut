function error_handler( message )
{
    local stack = getstackinfos( 2 ) // 1 as default, 2 for debugging the wrapper
    print( "\n" + "[" + stack.src + ":" + stack.line + "]:\n" + "(ERROR) " + message + "\n\n" )
}
seterrorhandler( error_handler )

fe.load_module( "plus/wrapper.nut" )
PLUS.init()