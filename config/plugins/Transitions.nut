/*
################################################################################

Attract-Mode Plus Frontend - Transitions plugin v1.0
Adds configurable animated transitions

by Oomek - Radek Dutkiewicz 2025
https://github.com/oomek/attract-extra

################################################################################
*/

local order = 0
class UserConfig </ help="A plugin that creates animated transitions " />
{

	</ label="Animation Time",
		help="",
		options="125, 250, 500, 750, 1000",
		order=order++ />
	animation_time = "500"

	</ label="Next Display",
		help="",
		options="None, Fade Out, Fade In, Move Left, Move Right, Move Up, Move Down",
		order=order++ />
	next_display = "Move Left"

	</ label="Previous Display",
		help="",
		options="None, Fade Out, Fade In, Move Left, Move Right, Move Up, Move Down",
		order=order++ />
	prev_display = "Move Right"

    </ label="Displays Menu",
		help="",
		options="None, Fade Out, Fade In, Move Left, Move Right, Move Up, Move Down",
		order=order++ />
	displays_menu = "Move Down"

    </ label="Select",
		help="",
		options="None, Fade Out, Fade In, Move Left, Move Right, Move Up, Move Down",
		order=order++ />
	select = "Move Up"

    </ label="To Game",
		help="",
		options="None, Fade Out, Fade In, Move Left, Move Right, Move Up, Move Down",
		order=order++ />
	to_game = "Fade Out"

    </ label="From Game",
		help="",
		options="None, Fade Out, Fade In, Move Left, Move Right, Move Up, Move Down",
		order=order++ />
	from_game = "Fade In"
}


fe.load_module("inertia")

class FadeTransitions
{
    static NV_KEY = "AM+_Transitions"
    config = null
    layout = fe.layout.surface
    snapshot = fe.layout.snapshot
    counter = null
    animation_time = null
    animated = null
    pending_signal = null

    constructor()
    {
        config = fe.get_config()
        animation_time = config["animation_time"].tointeger()
        animated = false
        pending_signal = ""

        layout = Inertia( layout, animation_time, "alpha", "x", "y" )
        snapshot = Inertia( snapshot, animation_time, "alpha", "x", "y" )

        layout.zorder = 1
        snapshot.zorder = 0
        snapshot.visible = true

        fe.add_transition_callback( this, "on_transitions" )
        fe.add_ticks_callback( this, "on_tick" )
        fe.add_signal_handler( this, "on_signal" )

        counter = 0
    }

    function animate( animation_name )
    {
        if ( animated )
            return

        switch( animation_name )
        {
            case "Move Left":
                layout.x = layout.width
                layout.to_x = 0
                snapshot.x = 0
                snapshot.to_x = -snapshot.width
                break

            case "Move Right":
                layout.x = -layout.width
                layout.to_x = 0
                snapshot.x = 0
                snapshot.to_x = snapshot.width
                break

            case "Move Up":
                layout.y = layout.height
                layout.to_y = 0
                snapshot.y = 0
                snapshot.to_y = -snapshot.height
                break

            case "Move Down":
                layout.y = -layout.height
                layout.to_y = 0
                snapshot.y = 0
                snapshot.to_y = snapshot.height
                break

            case "Fade In":
                layout.alpha = 0
                layout.to_alpha = 255
                break

            case "Fade Out":
                layout.alpha = 255
                layout.to_alpha = 0
                break

            case "None":
                snapshot.visible = false
                break
        }

        animated = true
    }

    function on_transitions( ttype, var, ttime )
    {
        switch( ttype )
        {
            case Transition.StartLayout:
            case Transition.EndLayout:
                if ( var == FromTo.Frontend && fe.nv.rawin( NV_KEY ))
                    delete fe.nv[NV_KEY]
                break

            case Transition.ToNewList:
                if ( fe.nv.rawin( NV_KEY ))
                {
                    local signal = fe.nv[NV_KEY]
                    if ( config.rawin( signal ))
                    {
                        snapshot.visible = true
                        animate( config[signal] )
                    }

                    delete fe.nv[NV_KEY]
                }
                break

            case Transition.ToGame:
                snapshot.visible = false
                if ( ttime < animation_time )
                {
                    animate( config.to_game )
                    return true
                }
                break

            case Transition.FromGame:
                snapshot.visible = false
                animate( config.from_game )
                break
        }

        animated = false
        return false
    }

    function on_tick( ttime )
    {
        if ( !layout.running && !snapshot.running && pending_signal != "" )
        {
            fe.signal( pending_signal )
            pending_signal = ""
        }
    }

    function on_signal( sig )
    {
        if (( layout.running || snapshot.running ))
        {
            if ( pending_signal == "" )
                pending_signal = sig

            return true
        }

        switch( sig )
        {
            case "next_display":
            case "prev_display":
            case "displays_menu":
                if ( fe.list.display_index == -1 )
                    fe.nv[NV_KEY] <- "select"
                else
                    fe.nv[NV_KEY] <- sig
                break

            case "select":
                if ( fe.list.display_index == -1 )
                    fe.nv[NV_KEY] <- "select"
                break

            case "exit":
                if ( !fe.list.clones_list )
                    fe.nv[NV_KEY] <- "select"
                break

            default:
        }
    }

}

fe.plugin["AM+_Transitions"] <- FadeTransitions()
