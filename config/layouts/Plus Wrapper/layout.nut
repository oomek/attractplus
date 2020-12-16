// Test layout for Plus Wrapper

fe.load_module("plus")

local flw = fe.layout.width
local flh = fe.layout.height

local img1 = fe.add_artwork( "snap", 0, 0, flw/2, flh/2 )
local img2 = fe.add_artwork( "snap", 100, 100 )
local img3 = fe.add_artwork( "snap" )
img1.video_flags = Vid.ImagesOnly
img2.video_flags = Vid.ImagesOnly
img3.video_flags = Vid.ImagesOnly

// img1.set_rgb(200,0,0)
// img2.set_rgb(0,200,0)
img3.set_rgb(0,0,200)
// img2.swap(img3)
img2.set_pos(0,flh/2)
img3.set_pos(40,40,200,100)
// img1.alpha = 200
// img2.alpha = 200
// img3.alpha = 200

local su = fe.add_surface(flw/2,flh)
su.set_pos(flw/2,0)

local simg1 = su.add_artwork( "snap", 0, 0, flw/2, flh/2 )
local simg2 = su.add_artwork( "snap", 100, 100 )
local simg3 = su.add_artwork( "snap" )
simg1.video_flags = Vid.ImagesOnly
simg2.video_flags = Vid.ImagesOnly
simg3.video_flags = Vid.ImagesOnly

// simg1.set_rgb(200,0,0)
// simg2.set_rgb(0,200,0)
simg3.set_rgb(0,0,200)
// simg2.swap(simg3)
simg2.set_pos(0,flh/2)
simg3.set_pos(40,40,200,100)
// simg1.alpha = 200
// simg2.alpha = 200
// simg3.alpha = 200


img1.set_rgb(0,0,0)

// Troubles start here the img1 should have purple tint
img1.add_fx1()
img1.add_fx2()


function on_transition( ttype, var, ttime )
{
	if ( ttype == Transition.FromOldSelection)
	{
		img1.y -= var * 10
	}
}
fe.add_transition_callback( "on_transition" )
