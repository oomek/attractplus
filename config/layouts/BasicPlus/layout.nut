/**
 * BasicPlus Layout
 *
 * @summary A minimalistic homage to the original Basic layout.
 * @version 1.7.6 2025-12-22
 * @author Chadnaut
 * @url https://github.com/Chadnaut
 *
 * @requires
 * @artwork snap
 *
 * Showcases tricks and effects using native AM+ 3.2.0 features
 * - No modules, plugins, shaders, or additional files required
 * - All code is contained within this file for portability & discoverability
 * - Simply provide `snap` artwork and you're ready to go!
 *
 * Tips
 * - There are 16 @trick sections to be found within this file
 * - Enable the Minimap (right-click the scrollbar in VSCode) to see region headings
 * - Enable Layout Preview in AM+ to easily see the effect of each layout option
 *
 * Get the AM+ Squirrel extension for VS Code
 * - @see https://marketplace.visualstudio.com/items?itemName=chadnaut.am-squirrel
 * - A suite of support tools to enhance your AM+ development experience
 *
 * Overview
 * - @see Config - The user config class
 * - @see Helpers - Generic helper functions
 * - @see Settings - Loads config settings into a helper class
 * - @see Layout - The top-level container that handles the entire layout
 * - @see Background - Draws background artwork and animation
 * - @see Scrollbar - Draws a vertical scrollbar for list position
 * - @see Columns - Contains the List, Heading, and Card elements
 * - @see List - The game list with custom selector
 * - @see Heading - The display and filter headings
 * - @see Card - The frame that contains the game artwork and information
 * - @see Image - The game artwork, or placeholder content if none is available
 * - @see Letter - A single letter displayed on prolonged scrolling
 * - @see Screensaver - Optional screensaver hides the list to help prevent CRT burn-in
 * - @see Main - Bootstraps the Layout
 */

// region Config -----------------------------------------------------------------------

local col_options = "Dark,Light,AM-Dark,AM-Light"
local pinch_options = "-100,-80,-60,-40,-20,0,20,40,60,80,100"
local bright_options = "0,10,20,30,40,50,60,70,80,90,100"
local info_options = "Category,Manufacturer,Score,Year,Year+Category,Year+Manufacturer,Year+Score"

class UserConfig
</ help="BasicPlus v1.7.6 by Chadnaut\nA minimalistic homage to the original Basic layout" />
{
    </ label="Preset", help="Set the layout theme preset - overrides some settings while in use\nSet to None to create your own theme", options="None,Basic,Inversion,Glassic,Cyber,Toon", order=0 /> theme_preset = "None"
    </ label="—————— List ——————", help="List Settings", is_info=true, order=1 /> list_section = "————————————————"
    </ label="List Mode", help="Set the list scrolling mode", options="Static,Moving,Bounded,Paged", order=2 /> list_mode = "Bounded"
    </ label="List Size", help="Set the list font size", options="Small,Medium,Large", order=3 /> list_size = "Medium"
    </ label="List Density", help="Set the list density mode", options="Low,Medium,High", order=4 /> list_density = "Low"
    </ label="List Bold", help="Enable the list bold style", options="Yes,No", order=5 /> list_bold = "Yes"
    </ label="List Align", help="Set the list text alignment", options="Left,Centre,Right", order=6 /> list_align = "Centre"
    </ label="List Margin", help="Set the number of list margin rows", options="Auto,0,1,2,3", order=7 /> list_margin = "Auto"
    </ label="List Edge", help="Set the display mode for list options within the margin and overflow edges\nUnavailable in Moving mode", options="Auto,Show,Dimmed,Hide", order=8 /> list_edge = "Auto"
    </ label="List Overflow", help="Show list options beyond the safe area\nUnavailable in Moving mode", options="Yes,No", order=9 /> list_overflow = "Yes"
    </ label="List Width", help="Set the width of the list", options="Smaller,Small,Medium,Large,Larger", order=10 /> list_width = "Medium"

    </ label="—————— Scrollbar ——————", help="Scrollbar Settings", is_info=true, order=11 /> scr_section = "————————————————"
    </ label="Scrollbar Enable", help="Enable the scrollbar", options="Yes,No", order=12 /> scr_enable = "Yes"
    </ label="Scrollbar Position", help="Set the scrollbar position relative to the window", options="Left,Right", order=13 /> scr_side = "Right"
    </ label="Scrollbar Size", help="Set the scrollbar thickness", options="Small,Medium,Large", order=14 /> scr_border = "Medium"
    </ label="Scrollbar Duration", help="Set the scrollbar display time", options="Short,Medium,Long,Always", order=15 /> scr_delay = "Short"
    </ label="Scrollbar NewList", help="Show scrollbar when changing lists", options="Yes,No", order=16 /> scr_newlist = "No"
    </ label="Scrollbar Redundant", help="Show the scrollbar even when the list fits the page", options="Yes,No", order=17 /> scr_redundant = "No"
    </ label="Scrollbar Letter", help="Enable the letter popup during prolonged list scrolling", options="Yes,No", order=18 /> scr_letter = "Yes"

    </ label="—————— Info ——————", help="Info Settings", is_info=true, order=19 /> info_section = "————————————————"
    </ label="Info Enable", help="Enable the card information", options="Yes,No", order=20 /> info_enable = "Yes"
    </ label="Info Size", help="Set the card information font size", options="Small,Medium,Large", order=21 /> info_size = "Small"
    </ label="Info Bold", help="Enable the card information bold style", options="Yes,No", order=22 /> info_bold = "Yes"
    </ label="Info Top", help="Set the top information content", options=info_options, order=23 /> info_top = "Year+Manufacturer"
    </ label="Info Bottom", help="Set the bottom information content", options=info_options, order=24 /> info_bottom = "Category"
    </ label="Info Score", help="Show the score in this position if available", options="None,Top,Bottom", order=25 /> info_score = "Bottom"
    </ label="Info Favourite", help="Set the favourite icon position", options="None,Top,Bottom", order=26 /> info_fav = "Bottom"

    </ label="—————— Card ——————", help="Card Settings", is_info=true, order=27 /> card_section = "————————————————"
    </ label="Card Enable", help="Enable the card", options="Yes,No", order=28 /> card_enable = "Yes"
    </ label="Card Position", help="Set the card position relative to the list", options="Left,Right", order=29 /> card_side = "Right"
    </ label="Card Padding", help="Set the card padding size", options="None,Small,Medium,Large", order=30 /> card_pad = "Medium"
    </ label="Card Artwork", help="Set the card artwork type", options="Title,Image,Video,Video+Audio", order=31 /> card_art = "Image"
    </ label="Card Aspect-Ratio", help="Force 4:3 ratio for raster artwork", options="Yes,No", order=32 /> card_ratio = "Yes"
    </ label="Card Gloss", help="Set the card gloss opacity", options="0,20,40,60,80,100", order=33 /> card_gloss = "20"

    </ label="—————— Background ——————", help="Background Settings", is_info=true, order=34 /> bg_section = "————————————————"
    </ label="Background Artwork", help="Set the background artwork type", options="Title,Image,Video", order=35 /> bg_art = "Image"
    </ label="Background Filter", help="Set the background filter effect", options="Mosaic,Blur,Reeded,Tile", order=36 /> bg_filter = "Mosaic"
    </ label="Background Size", help="Set the background filter size", options="Smaller,Small,Medium,Large", order=37 /> bg_size = "Large"
    </ label="Background Chromatic", help="Set the background chromatic effect", options="None,Small,Medium,Large,Diverging", order=38 /> bg_chromatic = "Small"
    </ label="Background Cross-Fade", help="Enable the background cross-fade persistence effect", options="Yes,No", order=39 /> bg_persist = "Yes"
    </ label="Background Rotate", help="Set the background rotation", options="-90,-85,-80,-75,-70,-65,-60,-55,-50,-45,-40,-35,-30,-25,-20,-15,-10,-5,0,5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90", order=40 /> bg_rotate = "-45"
    </ label="Background Vignette", help="Set the background vignette size", options="None,Smaller,Small,Medium,Large,Larger", order=41 /> bg_vignette = "Medium"

    </ label="—————— Style ——————", help="Style Settings", is_info=true, order=42 /> style_section = "————————————————"
    </ label="Outline Size", help="Set the outline thickness", options="None,Hairline,Small,Medium,Large", order=43 /> style_ol_size = "None"
    </ label="Corner Style", help="Set the corner style", options="Square,Bevel,Round", order=44 /> style_corner = "Round"
    </ label="Corner Radius X", help="Set the radius width", options="Smaller,Small,Medium,Large,Larger", order=45 /> style_radius_x = "Smaller"
    </ label="Corner Radius Y", help="Set the radius heigh", options="Smaller,Small,Medium,Large,Larger", order=46 /> style_radius_y = "Smaller"
    </ label="Corner Contour", help="Adjust the artwork corner size to preserve card padding", options="Yes,No", order=47 /> style_contour = "No"
    </ label="Left Skew X", help="Set the left column horizontal skew effect", options=pinch_options, order=48 /> style_l_skew_x = "0"
    </ label="Left Skew Y", help="Set the left column vertical skew effect", options=pinch_options, order=49 /> style_l_skew_y = "0"
    </ label="Left Pinch X", help="Set the left column horizontal pinch effect", options=pinch_options, order=50 /> style_l_pinch_x = "0"
    </ label="Left Pinch Y", help="Set the left column vertical pinch effect", options=pinch_options, order=51 /> style_l_pinch_y = "40"
    </ label="Right Skew X", help="Set the right column horizontal skew effect", options=pinch_options, order=52 /> style_r_skew_x = "0"
    </ label="Right Skew Y", help="Set the right column vertical skew effect", options=pinch_options, order=53 /> style_r_skew_y = "0"
    </ label="Right Pinch X", help="Set the right column horizontal pinch effect", options=pinch_options, order=54 /> style_r_pinch_x = "0"
    </ label="Right Pinch Y", help="Set the right column vertical pinch effect", options=pinch_options, order=55 /> style_r_pinch_y = "-40"
    </ label="Safe-Margin", help="Set the safe area margin", options="Small,Medium,Large", order=56 /> style_safe_margin = "Small"

    </ label="—————— Colour ——————", help="Colour Settings", is_info=true, order=57 /> col_section = "————————————————"
    </ label="Heading Colour", help="Set the heading colour", options=col_options, order=58 /> col_heading = "Light"
    </ label="List Colour", help="Set the list colour", options=col_options, order=59 /> col_list = "Light"
    </ label="List Dim Opacity", help="Set the list dimming opacity", options=bright_options, order=60 /> col_dim_alpha = "50"
    </ label="Info Color", help="Set the card information colour", options=col_options, order=61 /> col_info = "Dark"
    </ label="Info Opacity", help="Set the card information opacity", options=bright_options, order=62 /> col_info_alpha = "70"
    </ label="Card Color", help="Set the card colour", options=col_options, order=63 /> col_card = "Light"
    </ label="Card Opacity", help="Set the card colour opacity", options=bright_options, order=64 /> col_card_alpha = "100"
    </ label="Card Text Colour", help="Set the card foreground colour", options=col_options, order=65 /> col_fore = "Dark"
    </ label="Outline Colour", help="Set the outline colour", options=col_options, order=66 /> col_ol = "Dark"
    </ label="Outline Opacity", help="Set the outline opacity", options=bright_options, order=67 /> col_ol_alpha = "100"
    </ label="BG Artwork Opacity", help="Set the background artwork opacity, revealing the background colour beneath it", options=bright_options, order=68 /> col_bg_alpha = "50"
    </ label="BG Artwork Colourize", help="Set the background artwork colourize strength", options=bright_options, order=69 /> col_bg_colourize = "0"
    </ label="BG Colour Brightness", help="Set the background colour brightness", options=bright_options, order=70 /> col_bg_bright = "0"
    </ label="BG Colour Tint", help="Set the background colour tint strength", options=bright_options, order=71 /> col_bg_tint = "0"
    </ label="Vignette Color", help="Set the vignette colour", options=col_options, order=72 /> col_vig = "Dark"
    </ label="Vignette Opacity", help="Set the vignette opacity", options=bright_options, order=73 /> col_vig_alpha = "50"
    </ label="Letterbox Colour", help="Set the letterbox colour, shown behind headings when card disabled", options=col_options, order=74 /> col_box = "Dark"
    </ label="Letterbox Opacity", help="Set the letterbox opacity", options=bright_options, order=75 /> col_box_alpha = "80"

    </ label="—————— Animation ——————", help="Animation Settings", is_info=true, order=76 /> anim_section = "————————————————"
    </ label="List Pulsing", help="Enable the pulsing list selector animation", options="Yes,No", order=77 /> anim_list_pulse = "Yes"
    </ label="List Scrolling", help="Enable the scrolling list selector animation during game select", options="Yes,No", order=78 /> anim_list_scroll = "Yes"
    </ label="List Paging", help="Enable the scrolling list selector animation during Paged mode pagination", options="Yes,No", order=79 /> anim_list_page = "No"
    </ label="List Bumping", help="Enable the list bump animation during pagination", options="Yes,No", order=80 /> anim_list_bump = "Yes"
    </ label="List Wobbling", help="Enable the list wobble animation during random game select", options="Yes,No", order=81 /> anim_list_random = "Yes"
    </ label="List Sliding", help="Enable the list slide animation during filter select", options="Yes,No", order=82 /> anim_list_slide = "Yes"
    </ label="Card Floating", help="Enable the floating card animation", options="Yes,No", order=83 /> anim_card_float = "Yes"
    </ label="Card Parallax", help="Enable the card artwork parallax animation when floating enabled", options="Yes,No", order=84 /> anim_card_parallax = "Yes"
    </ label="Card Resizing", help="Enable the card resize animation during game select", options="Yes,No", order=85 /> anim_card_resize = "Yes"
    </ label="Background Panning", help="Enable the panning background animation", options="Yes,No", order=86 /> anim_bg_pan = "Yes"
    </ label="Display Wiping", help="Enable the wipe animation on display select", options="Yes,No", order=87 /> anim_display_wipe = "Yes"
    </ label="Launch Irising", help="Enable the iris animation on game launch", options="Yes,No", order=88 /> anim_launch_iris = "Yes"

    </ label="—————— Standby ——————", help="Standby Settings", is_info=true, order=89 /> sav_section = "————————————————"
    </ label="Standby Delay", help="Set the time in seconds before entering standby mode, which hides the list to reduce burn-in. 0 to disable", order=90 /> sav_delay = "0"
    </ label="Standby Change", help="Set the time in seconds between changing game in standby mode. 0 to disable", order=91 /> sav_change = "30"

    </ label="—————— Debug ——————", help="Debug Settings", is_info=true, order=92 /> debug_section = "————————————————"
    </ label="Debug Background", help="Hide all foreground layers", options="Yes,No", order=93 /> debug_bg = "No"
    </ label="Debug Popups", help="Force the scrollbar and letter to appear", options="Yes,No", order=94 /> debug_popup = "No"
    </ label="Debug Elements", help="Enable the debug element outlines", options="Yes,No", order=95 /> debug_element = "No"
    </ label="Debug Safe-Margin", help="Enable the debug safe area margin", options="Yes,No", order=96 /> debug_safe = "No"
    </ label="Debug Time", help="Set the debug time dilation. 1.0 is default", options="0.01,0.1,0.25,0.5,1.0,1.5,2.0", order=97 /> debug_time = "1.0"
    </ label="Debug Resolution", help="Set the debug internal resolution", options="320x240,240x320,640x480,480x640,4:3,3:4,Full", order=98 /> debug_res = "Full"
}

if (FeVersionNum < 320) fe.log("Warning: BasicPlus requires Attract-Mode Plus v3.2.0 or higher")

// endregion
// region Presets ----------------------------------------------------------------------

cfg_presets <- {
    Basic = {
        bg_filter = "Mosaic",
        bg_chromatic = "Small",
        bg_size = "Large",
        style_ol_size = "None",
        col_heading = "Light",
        col_list = "Light",
        col_info = "Dark",
        col_info_alpha = "70",
        col_card = "Light",
        col_card_alpha = "100",
        col_fore = "Dark",
        col_bg_alpha = "50",
        col_bg_colourize = "0",
        col_bg_bright = "0",
        col_bg_tint = "0",
        col_vig = "Dark",
        col_vig_alpha = "50",
        col_box = "Dark",
        col_box_alpha = "80"
    },
    Inversion = {
        bg_filter = "Reeded",
        bg_chromatic = "Small",
        bg_size = "Medium",
        style_ol_size = "None",
        col_heading = "Dark",
        col_list = "Dark",
        col_info = "Light",
        col_info_alpha = "70",
        col_card = "Dark",
        col_card_alpha = "100",
        col_fore = "Light",
        col_bg_alpha = "80",
        col_bg_colourize = "0",
        col_bg_bright = "50",
        col_bg_tint = "0",
        col_vig = "Light",
        col_vig_alpha = "50",
        col_box = "Light",
        col_box_alpha = "80"
    },
    Glassic = {
        bg_filter = "Blur",
        bg_chromatic = "Medium",
        bg_size = "Large",
        style_ol_size = "Hairline",
        col_heading = "Light",
        col_list = "Light",
        col_info = "Light",
        col_info_alpha = "90",
        col_card = "Light",
        col_card_alpha = "20",
        col_fore = "Light",
        col_ol = "Light",
        col_ol_alpha = "50",
        col_bg_alpha = "80",
        col_bg_colourize = "0",
        col_bg_bright = "0",
        col_bg_tint = "0",
        col_vig = "Dark",
        col_vig_alpha = "50",
        col_box = "Dark",
        col_box_alpha = "80"
    },
    Cyber = {
        bg_filter = "Mosaic",
        bg_chromatic = "Diverging",
        bg_size = "Smaller",
        style_ol_size = "Small",
        col_heading = "Light",
        col_list = "AM-Light",
        col_info = "AM-Light",
        col_info_alpha = "70",
        col_card = "Dark",
        col_card_alpha = "50",
        col_fore = "Light",
        col_ol = "AM-Light",
        col_ol_alpha = "100",
        col_bg_alpha = "30",
        col_bg_colourize = "50",
        col_bg_bright = "0",
        col_bg_tint = "50",
        col_vig = "AM-Dark",
        col_vig_alpha = "30",
        col_box = "Dark",
        col_box_alpha = "80"
    },
    Toon = {
        bg_filter = "Tile",
        bg_chromatic = "Large",
        bg_size = "Large",
        style_ol_size = "Medium",
        col_heading = "Dark",
        col_list = "Dark",
        col_info = "Light",
        col_info_alpha = "100",
        col_card = "AM-Light",
        col_card_alpha = "100",
        col_fore = "Light",
        col_ol = "Dark",
        col_ol_alpha = "80",
        col_bg_alpha = "100",
        col_bg_colourize = "30",
        col_bg_bright = "80",
        col_bg_tint = "30",
        col_vig = "AM-Light",
        col_vig_alpha = "40",
        col_box = "Light",
        col_box_alpha = "80"
    }
}

// endregion
// region Helpers ----------------------------------------------------------------------

/** Ease in helper, clamps time */
ease_in <- @(t, b, c, d) ease.in_cubic(clamp(t, 0.0, d), b, c, d)

/** Ease out helper, clamps time */
ease_out <- @(t, b, c, d) ease.out_cubic(clamp(t, 0.0, d), b, c, d)

/** Return the current layout time */
tt <- @() fe.layout.time

/** Returns `x` scaled from `240p` to the given current layout size */
p240 <- @(x) round((x * min(fe.layout.width, fe.layout.height)) / 240.0)

/** Return string to float conversion, or default if not numeric */
to_float <- @(v, d = 0.0) regexp("^-?[\\d\\.]+$").match(v.tostring()) ? v.tofloat() : d

/**
 * @trick Aspect-Ratio - Corrected ratio for CRT raster images
 * - The returned ratio is used to resize snaps to their intended CRT size
 * - Used instead of preserve_active_ratio, which only works for videos
 * - All native Mame snaps require this, notable examples: ddp3 oscar qbert spyhunt willow
 */
/** Returns the aspect ratio of the current game */
get_ratio <- function (art, force) {
    if (
        force &&
        fe.game_info(Info.DisplayType).tolower() == "raster" &&
        fe.game_info(Info.DisplayCount) == "1"
    ) {
        local r = fe.game_info(Info.Rotation)
        return r == "90" || r == "270" ? 3.0 / 4.0 : 4.0 / 3.0
    }
    local w = art.texture_width.tofloat()
    local h = art.texture_height.tofloat()
    return h ? (w / h) * art.sample_aspect_ratio : 1.0
}

/** Rect helper class stores dimensions */
Rect <- class {
    x = 0.0
    y = 0.0
    width = 0.0
    height = 0.0
}

// endregion
// region Settings ---------------------------------------------------------------------

class Settings extends UserConfig {
    bg_flags = Vid.NoAudio
    bg_mosaic = 0.0
    card_flags = Vid.Default
    card_left = false
    char_size = 0.0
    clm_pad = 0.0
    frame_time = 0.0
    head_char = 0.0
    head_h = 0.0
    info_style = Style.Regular
    card_h = 0.0
    safe_h = 0.0
    inner_radius_x = 0.0
    inner_radius_y = 0.0
    key_repeat = 40
    label_char = 0.0
    label_h = 0.0
    radius_x = 0.0
    radius_y = 0.0
    safe_margin = 0.0
    scr_width = 0.0
    show_duration = 650
    card_pad_size = 0.0
    tint_col = null
    ol_size = 0
    letter_char = 0.0
    letter_size = 0.0
    rad_sizes = null
    col_dark = [0, 0, 0]
    col_light = [255, 255, 255]
    info_top_msg = ""
    info_bottom_msg = ""
    score_msg = ""

    function read_preset(cfg, v) {
        local pairs = split(v, ",")
        foreach (pair in pairs) {
            local keyval = split(pair, "=")
            cfg[keyval[0]] = keyval[1]
        }
    }

    constructor() {
        init()

        // Get AM settings
        local general = fe.get_general_config()

        // Get the UI colours
        local a = 160.0 / 255.0 // adjust to match menu-focus colour
        local ui_light = split(general.ui_color, ",").map(@(c) c.tointeger())
        while (ui_light.len() < 3) ui_light.push(0)
        local ui_dark = [mix(0, ui_light[0], a), mix(0, ui_light[1], a), mix(0, ui_light[2], a)]

        // These radius sizes allow different combinations to match the available bg rotation
        rad_sizes = [
            tan((10 / 180.0) * PI) * 21,
            tan((15 / 180.0) * PI) * 21,
            tan((25 / 180.0) * PI) * 21,
            tan((35 / 180.0) * PI) * 21,
            21
        ]

        local alignments = [Align.MiddleLeft, Align.MiddleCentre, Align.MiddleRight]
        local art_flags = [-1, Vid.ImagesOnly, Vid.NoAudio, Vid.Default]
        local modes = [Selection.Static, Selection.Moving, Selection.Bounded, Selection.Paged]
        local cols = [col_dark, col_light, ui_dark, ui_light]
        local w = fe.layout.width
        local h = fe.layout.height
        local resolutions = [
            [320, 240],
            [240, 320],
            [640, 480],
            [480, 640],
            w / h > 3.0 / 4.0 ? [(h * 4) / 3, h] : [w, (w * 3) / 4],
            w / h > 3.0 / 4.0 ? [(h * 3) / 4, h] : [w, (w * 4) / 3],
            [w, h]
        ]

        score_msg = "[Score] [ScoreStarAlt] ([Votes])"

        local info_tokens = [
            "[Category]",
            "[Manufacturer]",
            score_msg,
            "[Year]",
            "[Year] / [Category]",
            "[Year] / [Manufacturer]",
            "[Year] / " + score_msg
        ]

        key_repeat = general.selection_speed_ms.tointeger()
        tint_col = ui_light
        debug_res = get_option("debug_res", resolutions)
        list_mode = get_option("list_mode", modes)
        list_align = get_option("list_align", alignments)
        list_size = get_option("list_size", [8, 10, 12])
        list_density = get_option("list_density", [0.0, 0.5, 1.0])
        list_bold = list_bold ? Style.Bold : Style.Regular
        list_margin = get_option("list_margin", [-1, 0, 1, 2, 3])
        list_width = get_option("list_width", [4, 5, 6, 7, 8]) / 12.0
        scr_delay = get_option("scr_delay", [1, 2, 4, 0])
        scr_border = get_option("scr_border", [2, 3, 5])
        card_flags = get_option("card_art", art_flags)
        card_gloss = (to_float(card_gloss, 20) * 255) / 100
        card_pad = get_option("card_pad", [0, 2, 3, 5])
        card_left = card_enable && card_side == "Left"
        style_radius_x = get_option("style_radius_x", rad_sizes)
        style_radius_y = get_option("style_radius_y", rad_sizes)
        style_l_skew_x = to_float(style_l_skew_x) / 100.0
        style_l_skew_y = to_float(style_l_skew_y) / 100.0
        style_l_pinch_x = to_float(style_l_pinch_x) / 100.0
        style_l_pinch_y = to_float(style_l_pinch_y) / 100.0
        style_r_skew_x = to_float(style_r_skew_x) / 100.0
        style_r_skew_y = to_float(style_r_skew_y) / 100.0
        style_r_pinch_x = to_float(style_r_pinch_x) / 100.0
        style_r_pinch_y = to_float(style_r_pinch_y) / 100.0
        style_corner = get_option("style_corner", [1, 2, 12])
        info_size = get_option("info_size", [5, 6, 7])
        info_style = info_bold ? Style.Bold : Style.Regular
        info_top_msg = get_option("info_top", info_tokens)
        info_bottom_msg = get_option("info_bottom", info_tokens)
        bg_flags = get_option("bg_art", art_flags)
        col_bg_alpha = to_float(col_bg_alpha, 50)
        col_bg_colourize = to_float(col_bg_colourize, 0)
        col_bg_bright = to_float(col_bg_bright, 50)
        col_bg_tint = to_float(col_bg_tint, 0)
        col_card_alpha = (to_float(col_card_alpha, 100) * 255) / 100
        bg_size = get_option("bg_size", [80.0, 40.0, 30.0, 20.0])
        bg_rotate = to_float(bg_rotate, 45.0)
        bg_chromatic = get_option("bg_chromatic", [0, 1, 2, 3, -1])
        bg_vignette = get_option("bg_vignette", [0, 12, 10, 8, 6, 4])
        style_safe_margin = get_option("style_safe_margin", [10, 15, 20])
        style_ol_size = get_option("style_ol_size", [0, -1.0, 1.0, 2.0, 3.0])
        col_dim_alpha = (to_float(col_dim_alpha, 100) * 255) / 100
        col_ol_alpha = (to_float(col_ol_alpha, 100) * 255) / 100
        col_vig_alpha = (to_float(col_vig_alpha, 100) * 255) / 100
        col_box_alpha = (to_float(col_box_alpha, 100) * 255) / 100
        col_info_alpha = (to_float(col_info_alpha, 70) * 255) / 100
        col_ol = get_option("col_ol", cols)
        col_card = get_option("col_card", cols)
        col_vig = get_option("col_vig", cols)
        col_list = get_option("col_list", cols)
        col_info = get_option("col_info", cols)
        col_heading = get_option("col_heading", cols)
        col_box = get_option("col_box", cols)
        col_fore = get_option("col_fore", cols)
        sav_delay = to_float(sav_delay, 0)
        sav_change = to_float(sav_change, 0)
        debug_time = to_float(debug_time, 1.0)
        frame_time = (1000.0 / ScreenRefreshRate) * debug_time

        /**
         * @trick Time Dilation - Speed-up or slow-down time
         * - Every animation uses this method to fetch layout time
         * - Altering the returned value will change the speed of all animations
         */
        local _debug_time = debug_time
        ::tt <- @() fe.layout.time * _debug_time

        cleanup()
    }

    /** Set all user config values onto this instance */
    function init() {
        local cfg = fe.get_config()

        // Presets overrides user config
        if (cfg.theme_preset in cfg_presets) {
            foreach (key, val in cfg_presets[cfg.theme_preset]) {
                if (key in cfg) cfg[key] = val
            }
        }

        // Update all boolean-type values
        foreach (k, v in cfg) this[k] = v == "Yes" ? true : v == "No" ? false : v
    }

    /** Update conflicting settings */
    function cleanup() {
        if (debug_bg) debug_popup = false

        if (list_mode == Selection.Moving) {
            list_overflow = false
            list_edge = "None"
        }

        if (list_margin == -1) {
            if (list_mode == Selection.Static) list_margin = 0
            else if (list_mode == Selection.Moving) list_margin = 2
            else if (list_mode == Selection.Bounded) list_margin = 1
            else if (list_mode == Selection.Paged) list_margin = 0
        }

        if (list_edge == "Auto") {
            if (list_mode == Selection.Static) list_edge = "Show"
            else if (list_mode == Selection.Moving) list_edge = "Show"
            else if (list_mode == Selection.Bounded) list_edge = "Show"
            else if (list_mode == Selection.Paged) list_edge = "Hide"
        }

        if (style_corner == 1) {
            style_radius_x = 0
            style_radius_y = 0
        }
    }

    /**
     * Return config value corresponding to the named option
     * @param {UserConfig} cfg A user config table
     * @param {string} name Name of the config entry
     * @param {array} values Array of values corresponding to the config options
     * @returns {*}
     */
    function get_option(name, values) {
        local keys = split(Settings.getattributes(name).options, ",")
        local val = this[name]
        local i = keys.find(val)
        if (i == null) {
            i = keys.find(Settings[name])
            if (i == null) {
                fe.log("Error: Invalid default '" + name + " = " + val + "'")
                i = 0
            }
        }
        return values[i]
    }

    /**
     * Update derived config values based on the given surface dimensions
     * @param {fe.Surface} parent Surface to resize settings for
     */
    function update(parent) {
        local w = parent.texture_width
        local h = parent.texture_height

        // Override p240 to match the current surface
        ::p240 <- @(x) round((x * min(w, h)) / 240.0)

        ol_size = style_ol_size < 0 ? 1 : p240(style_ol_size)
        char_size = p240(list_size)
        card_pad_size = p240(card_pad)
        scr_width = p240(scr_border * 2.0)
        radius_x = p240(style_radius_x)
        radius_y = p240(style_radius_y)
        local rm = style_corner == 2 ? 0.5 : 1.0
        inner_radius_x = max(0, radius_x - card_pad_size * rm * min(1.0, radius_x / radius_y))
        inner_radius_y = max(0, radius_y - card_pad_size * rm * min(1.0, radius_y / radius_x))
        bg_mosaic = bg_filter == "Mosaic" && bg_size == 80 ? hypot(w, h) : bg_size
        safe_margin = p240(style_safe_margin)
        clm_pad = p240(10)
        local col_scale = min(1.0, (1.0 - list_width) * 2.0)
        label_char = p240(info_size * col_scale)
        label_h = info_enable ? label_char + floor(clm_pad * 0.5) : card_pad_size
        head_char = round(char_size * 1.5 * col_scale)
        head_h = head_char + clm_pad
        safe_h = h - safe_margin * 2.0
        letter_char = p240(24)
        letter_size = p240(rad_sizes[rad_sizes.len() - 1] * 2)
    }
}

// endregion
// region Layout -----------------------------------------------------------------------

class Layout {
    /** @type {Settings} */
    cfg = null
    /** @type {fe.Rectangle} */
    mask = null
    /** @type {fe.Surface} */
    cont = null
    /** @type {fe.Surface} */
    body = null
    /** @type {fe.Surface} */
    cont = null
    /** @type {fe.Surface} */
    surf = null
    /** @type {fe.Rectangle} */
    bg = null

    /** @type {Background} */
    background = null
    /** @type {Scrollbar} */
    scrollbar = null
    /** @type {Letter} */
    letter = null
    /** @type {Screensaver} */
    screensaver = null
    /** @type {Columns} */
    columns = null

    mask_time = 0
    mask_size = 0.0
    mask_dir = 0
    mask_dur = 400.0
    mask_transition = false
    last_index = 0
    display_var = 0
    next_random = false
    next_page = false
    sav_tick = 0
    sav_rand = false

    /**
     * @param {fe.Surface} parent Surface to attach to
     * @param {Settings} cfg Layout settings
     */
    constructor(parent, cfg) {
        cfg.update(parent)
        this.cfg = cfg
        local w = parent.texture_width
        local h = parent.texture_height
        mask_size = w + h

        /**
         * @trick Iris-Wipe - The game-launch transition effect
         * - The mask rectangle combines with the body BlendMode.Multiply to reveal it
         * - Animating the mask size changes the portion of the body that is shown
         */
        mask = parent.add_rectangle(w * 0.5, h * 0.5, mask_size, mask_size)
        mask.anchor = Anchor.Centre
        mask.corner_ratio_x = min(1.0, cfg.radius_x / cfg.radius_y) * 0.5
        mask.corner_ratio_y = min(1.0, cfg.radius_y / cfg.radius_x) * 0.5
        mask.corner_points = cfg.style_corner

        // Body holds the entire layout
        body = parent.add_surface(w, h)
        body.blend_mode = BlendMode.Multiply
        body.rotation_origin = Origin.Centre
        body.anchor = Anchor.Centre
        body.set_pos(w * 0.5, h * 0.5)

        // Background rectangle prevents flash-of-white on first load
        bg = body.add_rectangle(0, 0, w, h)
        bg.set_rgb(0, 0, 0)
        background = Background(body, cfg)

        // Extra surfaces used for screensaver effect
        cont = body.add_surface(w, h)
        surf = cont.add_surface(w, h)

        if (cfg.debug_safe) {
            local m = cfg.safe_margin
            local safe = parent.add_rectangle(m, m, w - m * 2.0, h - m * 2.0)
            safe.alpha = 0
            safe.outline = p240(1)
            safe.set_outline_rgb(255, 0, 0)
        }

        // Create the layout elements - each one will handle its own child elements
        scrollbar = Scrollbar(surf, cfg)
        columns = Columns(surf, cfg)
        letter = Letter(surf, cfg, background)
        if (cfg.sav_delay) screensaver = Screensaver(this, cfg)

        fe.add_ticks_callback(this, "on_tick")
        fe.add_transition_callback(this, "on_transition")
        fe.add_signal_handler(this, "on_signal")
    }

    function mask_draw(ttime) {
        if (mask_dir) {
            local show = mask_dir == 1
            local scale = show
                ? ease_in(ttime - mask_time, 0.0, 1.0, mask_dur)
                : ease_out(ttime - mask_time, 1.0, -1.0, mask_dur)
            mask.width = mask.height = mask_size * scale
        }
    }

    function saver_draw(ttime) {
        if (screensaver) {
            if (cfg.sav_change) {
                local ch = floor(ttime / (cfg.sav_change * 1000))
                sav_rand = sav_tick != ch
                sav_tick = ch
                if (sav_rand && screensaver.get_active()) fe.signal("random_game")
            }
            screensaver.draw(ttime)
        }
    }

    /** Redraw all child elements on each tick */
    function on_tick(ttime) {
        ttime = tt()
        mask_draw(ttime)
        background.draw(ttime)
        scrollbar.draw(ttime)
        columns.draw(ttime)
        letter.draw(ttime)
        saver_draw(ttime)
    }

    /** Update elements depending on the current transition */
    function on_transition(ttype, var, ttime) {
        ttime = tt()
        switch (ttype) {
            case Transition.FromGame:
                if (cfg.anim_launch_iris) {
                    mask_time = ttime - cfg.frame_time
                    mask_dir = 1.0
                }
                columns.reset(ttime)
                break
            case Transition.ToGame:
                if (cfg.anim_launch_iris) {
                    if (mask_dir != -1.0) {
                        mask_time = ttime - cfg.frame_time
                        mask_dir = -1.0
                    }
                    on_tick(ttime) // Draw elements during transition
                    mask_transition = ttime < mask_time + mask_dur
                    return mask_transition
                }
                break
            case Transition.FromOldSelection:
                if (next_random) columns.next_random(ttime, fe.list.index - last_index)
                if (next_page || abs(var) > 1) columns.next_page(ttime, var)
                background.reset(ttime)
                columns.next_selection(ttime, var, fe.list.index, fe.list.size)
                letter.request(ttime)
                scrollbar.update(ttime, fe.list.index, fe.list.size)
                next_random = false
                next_page = false
                break
            case Transition.ToNewList:
                if (display_var) columns.next_display(ttime, display_var)
                display_var = 0
                background.reset(ttime)
                columns.next_list(ttime, var, fe.list.index, fe.list.size)
                scrollbar.update(ttime, fe.list.index, fe.list.size, true)
                break
        }
    }

    /** Update elements depending on the signal received */
    function on_signal(signal) {
        local ttime = tt()
        last_index = fe.list.index
        if (signal == "prev_display") display_var = -1
        if (signal == "next_display") display_var = 1
        if (signal == "random_game") next_random = true
        if (signal == "prev_favourite" || signal == "next_favourite") next_page = true
        if (signal == "prev_letter" || signal == "next_letter") next_page = true
        if (signal == "prev_letter" || signal == "next_letter") letter.show(ttime)
        return screensaver ? (sav_rand ? false : screensaver.reset(ttime)) : false
    }
}

// endregion
// region Background -------------------------------------------------------------------

class Background {
    /** @type {Settings} */
    cfg = null
    /** @type {fe.Surface} */
    surf = null
    /** @type {fe.Surface} */
    rgb_surf = null
    /** @type {fe.Surface} */
    rgb = null
    /** @type {fe.Surface} */
    rgb_cont = null
    /** @type {fe.Surface} */
    content = null
    /** @type {fe.Surface} */
    red = null
    /** @type {fe.Surface} */
    green = null
    /** @type {fe.Surface} */
    blue = null
    /** @type {fe.Text} */
    placeholder = null
    /** @type {fe.Image} */
    art = null

    bg_angle = 0.0
    bg_angle_offset = 0.0
    art_size = 0
    ratio = false

    /**
     * @param {fe.Surface} parent Surface to attach to
     * @param {Settings} cfg Layout settings
     */
    constructor(parent, cfg) {
        this.cfg = cfg
        local w = parent.texture_width
        local h = parent.texture_height
        local s = hypot(w, h)

        local reeded = cfg.bg_filter == "Reeded"
        local tile = cfg.bg_filter == "Tile"
        local blur = cfg.bg_filter == "Blur"
        local scale = blur || reeded || tile ? (cfg.bg_mosaic / s) * 0.5 : 1.0
        local s2 = s * scale

        surf = parent.add_surface(w, h)

        // Underlay colour shows beneath transparent artwork
        local underlay = surf.add_rectangle(0, 0, w, h)
        underlay.set_rgb(
            (mix(255, cfg.tint_col[0], cfg.col_bg_tint / 100.0) * cfg.col_bg_bright) / 100.0,
            (mix(255, cfg.tint_col[1], cfg.col_bg_tint / 100.0) * cfg.col_bg_bright) / 100.0,
            (mix(255, cfg.tint_col[2], cfg.col_bg_tint / 100.0) * cfg.col_bg_bright) / 100.0
        )

        rgb_surf = surf.add_surface(w, h)
        rgb_surf.blend_mode = BlendMode.Add
        rgb_surf.alpha = (cfg.col_bg_alpha / 100.0) * 255.0
        rgb_surf.set_rgb(
            mix(255, cfg.tint_col[0], cfg.col_bg_colourize / 100.0),
            mix(255, cfg.tint_col[1], cfg.col_bg_colourize / 100.0),
            mix(255, cfg.tint_col[2], cfg.col_bg_colourize / 100.0)
        )

        if (blur || reeded || tile) {
            /**
             * @trick Blur-blur-blur - Soften mipmap blur artifacts
             * - Mipmaps are a cheap way to achieve a simple blur effect, however they're very low quality
             * - Using multiple nested mipmap surfaces helps smooth the results
             */
            local s3 = s2 * 2
            local s4 = s2 * 4
            local rw = reeded ? s : tile ? s3 : s4
            local rh = reeded ? s3 : tile ? s3 : s4

            rgb = rgb_surf.add_surface(w, h)
            local rgb1 = rgb.add_surface(rw, rh)
            local rgb2 = rgb1.add_surface(s3, s3)
            local rgb3 = rgb2.add_surface(s2, s2)
            rgb1.set_pos(0, 0, rgb.texture_width, rgb.texture_height)
            rgb2.set_pos(0, 0, rgb1.texture_width, rgb1.texture_height)
            rgb3.set_pos(0, 0, rgb2.texture_width, rgb2.texture_height)
            rgb1.smooth = blur
            rgb_cont = rgb3
        } else {
            rgb = rgb_surf.add_surface(s2, s2)
            rgb_cont = rgb
        }

        rgb.set_pos(w * 0.5, h * 0.5, s, s)
        rgb.anchor = Anchor.Centre
        rgb.rotation_origin = Origin.Centre
        rgb.rotation = cfg.bg_rotate

        /**
         * @trick Cross-Fade - The background image-persistence effect
         * - The background surface is set to NOT clear, causing attached elements to "paint" onto it
         * - Transparent elements build up gradually to create a soft, blurry, fading effect
         * - NOTE: Nausea inducing when used on videos (modern games use a similar effect for "drunk-mode")
         */
        rgb_surf.clear = !cfg.bg_persist
        rgb.alpha = cfg.bg_persist ? 32 : 255
        rgb.blend_mode = cfg.bg_persist ? BlendMode.Alpha : BlendMode.Premultiplied

        /**
         * @trick Colour Channels - The background chromatic effect
         * - Three separate surfaces are created (one master and two clones), each a different size and colour
         * - The colours are re-combined using BlendMode.Add, displaying "aberration" where they mis-align
         * - While a controversial effect in modern games, it adds depth and colour to otherwise monotone backgrounds
         */
        local chroma = !!cfg.bg_chromatic
        red = rgb_cont.add_surface(s2 * 0.5, s2 * 0.5, s2, s2)
        red.anchor = Anchor.Centre
        red.repeat = true
        if (chroma) red.set_rgb(255, 0, 0)

        green = rgb_cont.add_clone(red)
        green.blend_mode = BlendMode.Add
        green.set_rgb(0, 255, 0)
        green.visible = chroma

        blue = rgb_cont.add_clone(green)
        blue.set_rgb(0, 0, 255)

        if (chroma) {
            bg_angle_offset = cfg.bg_chromatic < 0 ? (PI * 2.0) / 3.0 : 0

            local channel_offset = 0.04 * abs(cfg.bg_chromatic)
            blue.width = blue.height = s2 * (1.0 + channel_offset * 0.0)
            red.width = red.height = s2 * (1.0 + channel_offset * 1.0)
            green.width = green.height = s2 * (1.0 + channel_offset * 2.0)
        }

        local msg = "[Title] [Title] [Title] [Title] [Title] [Title]"
        local m_size = cfg.bg_mosaic
        local art_pos = m_size * 0.5
        art_size = m_size * 2.0 // overlap to crop "score" area

        /**
         * @trick Pixelate - The background pixelation effect
         * - A surface's texture size is locked upon creation, and does not change thereafter
         * - When created small then enlarged without smoothing, it results in big blocky pixels
         */
        content = red.add_surface(m_size, m_size)
        content.width = content.height = s2
        content.smooth = false

        // Placeholder text is seen when there's no background artwork
        local p_size = 240 // force low-res to match game art
        local place_cont = content.add_surface(p_size, p_size)
        place_cont.set_pos(-art_pos, -art_pos, art_size, art_size)
        place_cont.smooth = false
        placeholder = place_cont.add_text(msg, 0, 0, p_size, p_size)
        placeholder.char_size = p_size * 0.15
        placeholder.line_spacing = 0.66
        placeholder.align = Align.MiddleCentre
        placeholder.word_wrap = true
        placeholder.bg_alpha = 255

        // Artwork gets displayed in each channel surface clone
        local resource = cfg.bg_flags == -1 ? "-" : "snap"
        art = content.add_artwork(resource, art_pos, art_pos, art_size, art_size)
        art.anchor = Anchor.Centre
        art.video_flags = cfg.bg_flags
        art.smooth = false

        if (tile) add_tile_overlay(w, h, s)
        if (reeded) add_reeded_overlay(w, h, s)
        if (cfg.bg_vignette) add_vignette_overlay(w, h)
    }

    /** Add the tile overlay **/
    function add_tile_overlay(w, h, s) {
        local ss = ceil2(s / cfg.bg_mosaic)
        local se = max(2.0, p240(2) * 0.65)
        local sm1 = p240(1) * 0.25
        local sz1 = ss - sm1 * 2.0
        local sm2 = sm1 + se
        local sz2 = sz1 - se

        /**
         * @trick Tiled pattern - Used by the Tiled Background Filter
         * - Create a pattern on a small surface texture
         * - The surface is enlarged, subimg adjusted, and "repeat" enabled to create a tiled pattern
         */
        local slot = surf.add_surface(ss, ss)
        local slot_rect = slot.add_rectangle(0, 0, ss, ss)
        local slot_high = slot.add_rectangle(sm1, sm1, sz1, sz1)
        local slot_mask = slot.add_rectangle(sm2, sm2, sz2, sz2)
        slot_rect.set_rgb(0, 0, 0)
        slot_high.corner_ratio = slot_mask.corner_ratio = 0.1 // 0.05
        slot_high.set_rgb(255, 255, 255)
        slot_high.alpha = 200
        slot_mask.blend_mode = BlendMode.Subtract
        slot.set_pos(w * 0.5, h * 0.5, s, s)
        slot.anchor = Anchor.Centre
        slot.rotation_origin = Origin.Centre
        slot.rotation = cfg.bg_rotate
        slot.subimg_width = ss * cfg.bg_mosaic
        slot.subimg_height = ss * cfg.bg_mosaic
        slot.repeat = true
        slot.blend_mode = BlendMode.Overlay
        slot.alpha = 30
    }

    /** Add the reeded overlay **/
    function add_reeded_overlay(w, h, s) {
        local ss = ceil2(s / cfg.bg_mosaic)
        local slot = surf.add_surface(ss, ss)
        local slot_rect = slot.add_rectangle(0, 0, ss, ss * 0.1)
        slot.repeat = true
        slot.set_pos(w * 0.5, h * 0.5, s, s)
        slot.anchor = Anchor.Centre
        slot.rotation_origin = Origin.Centre
        slot.rotation = cfg.bg_rotate
        slot.subimg_width = ss * cfg.bg_mosaic
        slot.subimg_height = ss * cfg.bg_mosaic
        slot.blend_mode = BlendMode.Screen
        slot.alpha = 10
    }

    /** Add the vignette effect */
    function add_vignette_overlay(w, h) {
        // Uses the Blur-blur-blur trick to smoothly upscale a small black outline
        local v_surf1 = surf.add_surface(cfg.bg_vignette * 4, cfg.bg_vignette * 4)
        local v_surf2 = v_surf1.add_surface(cfg.bg_vignette * 2, cfg.bg_vignette * 2)
        local v_surf3 = v_surf2.add_surface(cfg.bg_vignette, cfg.bg_vignette)
        local v_rect = v_surf3.add_rectangle(0, 0, cfg.bg_vignette, cfg.bg_vignette)

        local v_over = 1.0 + 1.0 / cfg.bg_vignette
        v_surf1.anchor = Anchor.Centre
        v_surf1.alpha = cfg.col_vig_alpha
        v_surf1.set_pos(w * 0.5, h * 0.5, w * v_over, h * v_over)
        v_surf2.set_pos(0, 0, v_surf1.texture_width, v_surf1.texture_height)
        v_surf3.set_pos(0, 0, v_surf2.texture_width, v_surf2.texture_height)

        v_rect.set_outline_rgb(cfg.col_vig[0], cfg.col_vig[1], cfg.col_vig[2])
        v_rect.outline = -1
        v_rect.alpha = 0
    }

    /** Draw the elements */
    function draw(ttime) {
        local t = cfg.anim_bg_pan ? ttime / 80000.0 : 0
        // subimg must be on a smooth = true surface to enable sub-pixel positioning
        local r = bg_angle + t
        local g = r + bg_angle_offset
        local b = g + bg_angle_offset
        red.subimg_x = cos(r) * red.texture_width
        red.subimg_y = sin(r) * red.texture_height
        green.subimg_x = cos(g) * green.texture_width
        green.subimg_y = sin(g) * green.texture_height
        blue.subimg_x = cos(b) * blue.texture_width
        blue.subimg_y = sin(b) * blue.texture_height
    }

    /** Reset the background when the game changes */
    function reset(ttime) {
        bg_angle = randomf(0.0, PI * 2.0)

        local tw = art.texture_width.tofloat()
        local th = art.texture_height.tofloat()
        if (tw && th) {
            local art_r = get_ratio(art, cfg.card_ratio)
            local w = art_r > 1.0 ? tw : th * art_r
            local h = art_r < 1.0 ? th : tw / art_r
            local scale = art_size / (art_r > 1.0 ? h : w)
            art.width = scale * w
            art.height = scale * h
        }
    }
}

// endregion
// region Scrollbar --------------------------------------------------------------------

class Scrollbar {
    /** @type {Settings} */
    cfg = null
    /** @type {fe.Surface} */
    surf = null
    /** @type {fe.Rectangle} */
    rect = null

    y = 0.0
    height_max = 0.0
    height = 0.0
    dur = 0.0
    hidden = true
    show_dur = 0
    scroll_y = -1.0
    scroll_h = 0.0
    scroll_time = 0
    f_time = 0
    f_alpha = 0
    t_alpha = 0
    f_dur = 300.0
    min_h = 0.0
    max_h = 0.0

    /**
     * @param {fe.Surface} parent Surface to attach to
     * @param {Settings} cfg Layout settings
     */
    constructor(parent, cfg) {
        this.cfg = cfg
        if (!cfg.debug_popup && !cfg.scr_enable) return
        show_dur = cfg.show_duration * cfg.scr_delay
        dur = cfg.anim_list_scroll ? 150.0 : 1.0
        height_max = parent.texture_height.tofloat()

        local x = cfg.scr_side == "Left" ? 0 : parent.texture_width
        surf = parent.add_surface(x, 0, cfg.scr_width + cfg.ol_size * 2.0, height_max)
        surf.anchor = Anchor.Top
        surf.alpha = show_dur ? 0 : 255

        rect = surf.add_rectangle(cfg.ol_size, 0, cfg.scr_width, 0)
        rect.corner_radius_x = min(cfg.radius_x, cfg.scr_width * 0.5)
        rect.corner_radius_y = (rect.corner_radius_x / cfg.radius_x) * cfg.radius_y
        rect.corner_points = cfg.style_corner
        rect.set_rgb(cfg.col_card[0], cfg.col_card[1], cfg.col_card[2])
        rect.outline = cfg.ol_size
        rect.outline_alpha = cfg.col_ol_alpha
        rect.set_outline_rgb(cfg.col_ol[0], cfg.col_ol[1], cfg.col_ol[2])
        rect.alpha = cfg.col_card_alpha

        min_h = max(rect.corner_radius_y * 2.0, height_max * 0.1)
        max_h = height_max * 0.5
    }

    /** Draw the elements */
    function draw(ttime) {
        if (!surf) return
        rect.y = ease_out(ttime - scroll_time, scroll_y, y - scroll_y, dur)
        rect.height = ease_out(ttime - scroll_time, scroll_h, height - scroll_h, dur)

        local hide = (show_dur || hidden) && ttime >= f_time
        surf.alpha = hide
            ? ease_out(ttime - f_time, t_alpha, -t_alpha, f_dur)
            : ease_out(ttime - scroll_time, f_alpha, t_alpha - f_alpha, f_dur)

        if (cfg.debug_popup) surf.alpha = 255
    }

    /** Size and position the scrollbar to fit a new list */
    function resize(index, size) {
        height = height_max * 0.1
        y = 0
        if (!size) return

        height = (height_max / size) * fe.layout.page_size
        height = min(max(height, min_h), max_h)
        y = size == 1 ? (height_max - height) * 0.5 : (index * (height_max - height)) / (size - 1)
    }

    /** Update the scrollbar dimensions when the list changes */
    function update(ttime, index, size, newlist = false) {
        local hide = !cfg.debug_popup && show_dur && newlist && !cfg.scr_newlist

        if (!cfg.scr_redundant && fe.list.size <= fe.layout.page_size) hide = true

        if (hide) {
            hidden = true
            f_time = ttime - cfg.frame_time
            f_alpha = surf && surf.alpha ? surf.alpha : 0
            t_alpha = f_alpha
            return
        }

        resize(index, size)

        if (hidden) {
            rect.y = y
            rect.height = height
            hidden = false
        }

        scroll_y = surf ? rect.y : 0.0
        scroll_h = surf ? rect.height : 0.0
        scroll_time = ttime - cfg.frame_time
        f_time = ttime + show_dur - cfg.frame_time
        f_alpha = surf ? surf.alpha : 0
        t_alpha = 255
    }
}

// endregion
// region Columns ----------------------------------------------------------------------

class Columns {
    /** @type {Settings} */
    cfg = null
    /** @type {fe.Surface} */
    surf = null
    /** @type {fe.Surface} */
    mask = null
    /** @type {fe.Surface} */
    list_clm = null
    /** @type {fe.Surface} */
    card_clm = null

    /** @type {List} */
    list = null
    /** @type {Heading} */
    heading = null
    /** @type {Card} */
    card = null

    x = 0.0
    y = 0.0
    width = 0.0
    height = 0.0
    mask_time = 0
    mask_dur = 0.0
    mask_dir = 0.0
    rand_time = 0
    rand_dir = 0
    rand_dur = 0.0
    page_start = 0
    page_time = 0
    page_dir = 0
    page_dur = 0.0

    /**
     * @param {fe.Surface} parent Surface to attach to
     * @param {Settings} cfg Layout settings
     */
    constructor(parent, cfg) {
        this.cfg = cfg
        local w = parent.texture_width
        local h = parent.texture_height
        width = ceil2(min(h * 1.5, w) - cfg.safe_margin * 2.0)

        mask_dur = cfg.anim_display_wipe ? 800.0 : 1.0
        rand_dur = cfg.anim_list_random ? 640.0 : 0.0
        page_dur = cfg.anim_list_bump ? 320.0 : 0.0
        rand_time = -rand_dur
        page_time = -page_dur

        local cols = cfg.card_enable ? 2.0 : 1.0
        local clm_pad = cfg.card_enable ? cfg.clm_pad : 0.0
        local list_width = cfg.card_left ? 1.0 - cfg.list_width : cfg.list_width
        local left_w = ceil2((width - clm_pad) * list_width)
        local right_w = ceil2(width - clm_pad - left_w)
        if (!cfg.card_enable) {
            left_w = width
            right_w = w
        }

        local left_x = w * 0.5 - width * 0.5 + left_w * 0.5
        local right_x = w * 0.5 + width * 0.5 - right_w * 0.5
        if (!cfg.card_enable) right_x = w * 0.5

        height = ceil2(h + (left_w * 0.2 + cfg.char_size) * 2.0)
        local skx = height * 0.1
        local sky = left_w * 0.1
        local pin = min(w, h) * 0.1

        // Holds the columns
        x = w * 0.5
        y = h * 0.5
        surf = parent.add_surface(x, y, w, height)
        surf.anchor = Anchor.Centre
        surf.rotation_origin = Origin.Centre
        if (cfg.debug_bg) surf.visible = false

        // Left column, holds list/card
        local l_clm = surf.add_surface(left_x, height * 0.5, left_w, height)
        l_clm.anchor = Anchor.Centre

        l_clm.pinch_x = ceil2(pin * cfg.style_l_pinch_x)
        l_clm.pinch_y = ceil2(pin * cfg.style_l_pinch_y)
        l_clm.width += -2.0 * max(0, -l_clm.pinch_x)
        l_clm.height += -2.0 * max(0, -l_clm.pinch_y)

        l_clm.skew_x = ceil2(skx * cfg.style_l_skew_x)
        l_clm.skew_y = ceil2(sky * cfg.style_l_skew_y)
        l_clm.x += -0.5 * l_clm.skew_x
        l_clm.y += -0.5 * l_clm.skew_y

        // Right column, holds card/list
        local r_clm = surf.add_surface(right_x, height * 0.5, right_w, height)
        r_clm.anchor = Anchor.Centre

        if (cfg.card_enable) {
            r_clm.pinch_x = ceil2(pin * cfg.style_r_pinch_x)
            r_clm.pinch_y = ceil2(pin * cfg.style_r_pinch_y)
            r_clm.width += -2.0 * max(0, -r_clm.pinch_x)
            r_clm.height += -2.0 * max(0, -r_clm.pinch_y)

            r_clm.skew_x = ceil2(skx * cfg.style_r_skew_x)
            r_clm.skew_y = ceil2(sky * cfg.style_r_skew_y)
            r_clm.x += -0.5 * r_clm.skew_x
            r_clm.y += -0.5 * r_clm.skew_y
        }

        if (cfg.debug_element) {
            local cr = parent.add_rectangle(x, y, width, h - cfg.safe_margin * 2.0)
            local lr = l_clm.add_rectangle(0, 0, l_clm.texture_width, l_clm.texture_height)
            local rr = r_clm.add_rectangle(0, 0, r_clm.texture_width, r_clm.texture_height)
            cr.anchor = Anchor.Centre
            cr.alpha = 0
            cr.outline = p240(1)
            cr.set_outline_rgb(255, 128, 0)
            lr.alpha = rr.alpha = 0
            lr.outline = rr.outline = -p240(1)
            lr.outline_alpha = rr.outline_alpha = 100
            lr.set_outline_rgb(255, 255, 0)
            rr.set_outline_rgb(255, 255, 0)
        }

        /**
         * @trick Horizontal-Wipe - The display transition effect
         * - The mask uses an up-scaled surface to create a soft gradient
         * - BlendMode.Subtract hides the surfaces below, and subimg positioning controls movement
         */
        mask = surf.add_surface(4, 1)
        mask.add_rectangle(1, 0, 2, 1)
        mask.set_pos(w * 0.5, height * 0.5, w * 4.0, height)
        mask.blend_mode = BlendMode.Subtract
        mask.anchor = Anchor.Centre
        mask.repeat = true

        list_clm = cfg.card_left ? r_clm : l_clm
        card_clm = cfg.card_left ? l_clm : r_clm

        local card_w = card_clm.width - cfg.ol_size * 2.0
        local card_h1 = cfg.safe_h - cfg.head_h * 2.0
        local card_h2 = ceil2(((card_w - cfg.card_pad_size * 2.0) * 4.0) / 3.0) + cfg.label_h * 2.0
        cfg.card_h = cfg.card_enable ? min(card_h1, card_h2) : 0.0

        if (cfg.debug_element && cfg.card_enable) {
            local rx = card_clm.texture_width * 0.5
            local ry = card_clm.texture_height * 0.5
            local r = card_clm.add_rectangle(rx, ry, card_clm.width, cfg.card_h)
            r.anchor = Anchor.Centre
            r.set_outline_rgb(0, 255, 255)
            r.outline_alpha = 150
            r.outline = -p240(1)
            r.alpha = 0
            r.zorder = 1
        }

        // Create column elements
        list = List(list_clm, cfg)
        heading = Heading(card_clm, cfg)
        if (cfg.card_enable) card = Card(card_clm, cfg)
    }

    /** Draw the elements */
    function draw(ttime) {
        if (mask_dur) {
            local mask_x = ease_out(ttime - mask_time, mask_dir, -mask_dir, mask_dur)
            mask.subimg_x = 2.0 + mask_x

            local move_x = ease_out(ttime - mask_time, mask_dir, -mask_dir, mask_dur * 0.5)
            surf.x = x + -move_x * min(width, height) * 0.025
        }

        local y = 0.5 * (height - list_clm.skew_y)
        if (rand_dur && ttime <= rand_time + rand_dur) {
            local t = ttime - rand_time
            local str = ease_out(t, 1.0, -1.0, rand_dur) * rand_dir * cfg.char_size
            list_clm.y = y + str * sin((ttime - rand_time) / 30.0)
        } else if (page_dur && ttime <= page_time + page_dur) {
            local t = page_time + page_dur - ttime
            local str = max(0, t / page_dur) * page_dir * cfg.char_size * 0.25
            list_clm.y = y + str * sin((ttime - page_start) / 120.0)
        } else {
            list_clm.y = y
        }

        list.draw(ttime)
        heading.draw(ttime)
        if (card) card.draw(ttime)
    }

    /** Update elements when the game changes */
    function next_selection(ttime, var, index, size) {
        list.next_selection(ttime, var, index, size)
        if (card) card.reset(ttime)
    }

    /** Trigger an animation when a random game is selected */
    function next_random(ttime, var) {
        var = list.next_random(var)
        rand_time = ttime - cfg.frame_time
        rand_dir = var >= 0 ? -1 : 1
    }

    /** Trigger an animation when an entire page is scrolled */
    function next_page(ttime, var) {
        if (ttime >= page_time + page_dur) page_start = ttime
        page_time = ttime - cfg.frame_time
        page_dir = var >= 0 ? -1 : 1
    }

    /** Trigger an animation when the display changes */
    function next_display(ttime, var) {
        mask_time = ttime - cfg.frame_time
        mask_dir = var > 0 ? -2.0 : 2.0
    }

    /** Update elements when a new list is selected */
    function next_list(ttime, var, index, size) {
        list.next_list(ttime, var, index, size)
        heading.next_list(ttime, var, index, size)
        if (card) card.reset(ttime)
    }

    function reset(ttime) {
        if (card) card.reset(ttime)
    }
}

// endregion
// region List -------------------------------------------------------------------------

class List {
    /** @type {Settings} */
    cfg = null
    /** @type {fe.Rectangle} */
    sel_mask = null
    /** @type {fe.Rectangle} */
    sel_over = null
    /** @type {fe.ListBox} */
    list1 = null
    /** @type {fe.ListBox} */
    list2 = null

    list_dir = 0.0
    list_dur = 0.0
    list_time = 0
    height = 0.0
    width = 0.0

    row_h = 0.0
    sel_w = 0.0
    sel_h = 0.0
    sel_e = 0.0
    sel_row_last = -1
    sel_from = 0.0
    sel_to = 0.0
    sel_dur = 0.0
    sel_time = 0
    sel_repeat = false
    sel_offset = 0

    /**
     * @param {fe.Surface} parent Surface to attach to
     * @param {Settings} cfg Layout settings
     */
    constructor(parent, cfg) {
        this.cfg = cfg
        local rows = round(cfg.safe_h / (cfg.char_size * 1.75)) + cfg.list_density * 4
        if (rows == ceil2(rows)) rows--
        row_h = floor(cfg.safe_h / rows)

        // Show extra rows outside the safe area
        local over = 0
        if (cfg.list_overflow) {
            over = ceil((parent.texture_height - cfg.safe_h) / row_h)
            rows += over * 2
        }

        width = parent.texture_width
        height = row_h * rows
        sel_w = width - cfg.ol_size * 2.0
        list_dur = cfg.anim_list_slide ? 400.0 : 0.0

        /**
         * @trick Sliding List - The filter-change transition effect
         * - A second list with a filter_offset is positioned outside the surface's visible area
         * - When the filter is changed the current list slides out as the next list slides in
         * - The same effect is also applied to the filter text
         */
        local y = floor((parent.texture_height - height) * 0.5)
        local list_cont = parent.add_surface(0, y, width, height)
        local list_surf = list_cont.add_surface(width, height)
        list1 = list_surf.add_listbox(0, 0, width, height)
        list2 = list_surf.add_listbox(0, 0, width, height)
        list1.set_rgb(255, 255, 255)
        list2.set_rgb(255, 255, 255)
        list1.set_sel_rgb(255, 255, 255)
        list2.set_sel_rgb(255, 255, 255)
        list1.set_outline_rgb(255, 255, 255)
        list2.set_outline_rgb(255, 255, 255)
        list1.set_sel_outline_rgb(255, 255, 255)
        list2.set_sel_outline_rgb(255, 255, 255)
        list1.outline = list1.outline = 0.1 // improves gamma on tiny fonts
        list1.selbg_alpha = list2.selbg_alpha = 0
        list1.rows = list2.rows = rows
        list1.align = list2.align = cfg.list_align
        list1.style = list2.style = cfg.list_bold
        list1.sel_style = list2.sel_style = cfg.list_bold
        list1.char_size = list2.char_size = cfg.char_size
        list1.format_string = list2.format_string = "[Title]"
        list2.filter_offset = 1
        list2.x = list1.x + width
        list_cont.set_rgb(cfg.col_list[0], cfg.col_list[1], cfg.col_list[2])

        list1.sel_mode = list2.sel_mode = cfg.list_mode
        list1.sel_margin = list2.sel_margin = cfg.list_margin + over
        list1.list_align = list2.list_align = ListAlign.Middle
        fe.layout.page_size = list1.rows - list1.sel_margin * 2

        if (cfg.debug_element) {
            local r = parent.add_rectangle(0, y, list_cont.width, list_cont.height)
            r.set_outline_rgb(0, 255, 255)
            r.outline_alpha = 150
            r.outline = -p240(1)
            r.alpha = 0
            r.zorder = 1
        }

        // Custom list selection box that is animated independent of the list
        // - sel_cont is taller to accommodate the sel_over's ol_size outline
        sel_offset = y
        local sel_cont = parent.add_surface(0, 0, width, parent.texture_height)
        local sel_text = parent.add_surface(0, y, width, height)
        local sel_surf = parent.add_surface(width, height)

        sel_e = (row_h - list1.glyph_size) % 2 != 0 ? 1 : 0
        sel_h = row_h + sel_e
        local sel_y = get_sel_y()

        sel_mask = sel_surf.add_rectangle(sel_w * 0.5 + cfg.ol_size, sel_y, sel_w, sel_h)
        sel_over = sel_cont.add_rectangle(sel_mask.x, 0, sel_mask.width, sel_mask.height)
        sel_mask.anchor = sel_over.anchor = Anchor.Centre
        sel_mask.corner_points = sel_over.corner_points = cfg.style_corner
        draw_sel(sel_w, sel_h)

        local margin = max(cfg.char_size * 0.5 + sel_mask.corner_radius_x, sel_h * 0.5)
        list1.margin = list2.margin = margin

        sel_over.set_rgb(cfg.col_card[0], cfg.col_card[1], cfg.col_card[2])
        sel_over.alpha = cfg.col_card_alpha
        sel_over.outline = cfg.ol_size
        sel_over.outline_alpha = cfg.col_ol_alpha
        sel_over.set_outline_rgb(cfg.col_ol[0], cfg.col_ol[1], cfg.col_ol[2])

        /**
         * @trick List Mask - Cutout effect for list text on the custom selector
         * - Use the selection surface hide the list portion it overlaps
         * - Then use an inverse of it to show the overlapped text in a different colour
         */
        local sel_cut = list_cont.add_clone(sel_surf)
        sel_cut.blend_mode = BlendMode.Subtract
        local sel_text_list = sel_text.add_clone(list_surf)
        local sel_text_mask = sel_text.add_surface(width, height)
        local sel_text_rect = sel_text_mask.add_rectangle(0, 0, width, height)
        local sel_text_sel = sel_text_mask.add_clone(sel_surf)
        sel_text_sel.blend_mode = BlendMode.Subtract
        sel_text_mask.blend_mode = BlendMode.Subtract
        sel_text_list.set_rgb(cfg.col_fore[0], cfg.col_fore[1], cfg.col_fore[2])
        sel_surf.visible = false // Hide *after* we've cloned it

        local dim = cfg.list_edge
        local dim_alpha = dim == "Dimmed" ? cfg.col_dim_alpha : dim == "Hide" ? 0 : 255
        local dim_h = list1.sel_margin * 2

        if (dim_alpha < 255 && dim_h) {
            local dim_surf = list_cont.add_surface(1, rows)
            dim_surf.set_pos(0, 0, width, height)
            dim_surf.add_rectangle(0, 0, 1, dim_h)
            dim_surf.repeat = true
            dim_surf.subimg_y = dim_h * 0.5
            dim_surf.blend_mode = cfg.debug_element ? BlendMode.Alpha : BlendMode.Subtract
            dim_surf.smooth = false
            dim_surf.alpha = cfg.debug_element ? 128 : 255 - dim_alpha
        }
    }

    /** Return the y position for the selection */
    function get_sel_y() {
        return list1.sel_row * row_h + sel_h * 0.5 - sel_e
    }

    /** Reposition list selection */
    function next_selection(ttime, var, index, size) {
        sel_repeat = ttime - sel_time <= cfg.key_repeat * 2.0
        sel_dur = cfg.anim_list_scroll ? cfg.key_repeat * (sel_repeat ? 2.5 : 3.75) : 1.0
        sel_row_last = list1.sel_row
        sel_from = sel_mask.y
        sel_to = get_sel_y()
        sel_time = ttime - cfg.frame_time

        // In paged mode we don't want to animate selector when paginating
        if (!cfg.anim_list_page && cfg.list_mode == Selection.Paged && abs(var) == 1 && size) {
            local last_page = ((index + var + size) % size) / fe.layout.page_size
            local next_page = index / fe.layout.page_size
            if (last_page != next_page) sel_dur = 1.0
        }
    }

    /** Trigger an animation when a random game is selected */
    function next_random(var) {
        local mode = list1.sel_mode
        if (mode == Selection.Bounded || mode == Selection.Moving) {
            // Pick a random sel_row, not sel_row_last, and not in margin
            local rows = []
            local m = list1.sel_margin
            local n = list1.rows - m
            for (local i = m; i < n; i++) if (i != sel_row_last) rows.push(i)
            list1.sel_row = rows[random(0, rows.len() - 1)]
        }
        local diff = list1.sel_row - sel_row_last
        return diff ? diff : var
    }

    /** Trigger list sliding animation when the list changes */
    function next_list(ttime, var, index, size) {
        list_time = ttime - cfg.frame_time
        list2.filter_offset = var ? -var : 1
        list2.sel_row = sel_row_last
        list_dir = var.tofloat()
        next_selection(ttime, var, index, size)
        sel_mask.visible = sel_over.visible = fe.list.size > 0
    }

    function draw_sel(w, h) {
        sel_mask.width = w
        sel_mask.height = h
        local rx = cfg.radius_x / w
        local ry = cfg.radius_y / h
        local rs = min(0.5, ry) / ry
        sel_mask.corner_radius_x = sel_over.corner_radius_x = cfg.radius_x * rs
        sel_mask.corner_radius_y = sel_over.corner_radius_y = cfg.radius_y * rs
    }

    /** Draw the elements */
    function draw(ttime) {
        if (list_dur) {
            local d = ease_out(ttime - list_time, list_dir, -list_dir, list_dur)
            local n = cos(d * PI * 0.5)
            local x = d > 0 ? 1.0 - n : n - 1.0
            list1.x = width * x
            list2.x = list1.x + list2.filter_offset * width
        }

        if (cfg.anim_list_pulse) {
            local n = floor(sel_h * 0.1) * ((cos(ttime * 0.01) - 1.0) * 0.5)
            draw_sel(sel_w + n, sel_h + n)
        }

        if (sel_repeat) {
            // Use sine ease during prolonged scroll for smoother continuous motion
            local t = clamp(ttime - sel_time, 0.0, sel_dur)
            sel_mask.y = ease.out_sine(t, sel_from, sel_to - sel_from, sel_dur)
        } else {
            // The regular cubic easing is fast and snappy
            sel_mask.y = ease_out(ttime - sel_time, sel_from, sel_to - sel_from, sel_dur)
        }

        // Adjust y for sel_over as it's in a taller container to accommodate outline
        sel_over.set_pos(sel_mask.x, sel_mask.y + sel_offset, sel_mask.width, sel_mask.height)
    }
}

// endregion
// region Heading ----------------------------------------------------------------------

class Heading {
    /** @type {Settings} */
    cfg = null
    /** @type {fe.Text} */
    header1 = null
    /** @type {fe.Text} */
    footer1 = null
    /** @type {fe.Text} */
    footer2 = null

    head_h = 0.0
    width = 0.0
    height = 0.0
    y1 = 0.0
    y2 = 0.0

    r_time = 0
    r_dir = 0.0
    r_dur = 0.0

    h_dur = 0.0
    h_out_time = 0
    h_in_time = 0
    h_out_y = 0.0
    h_in_y = 0.0

    f_dur = 0.0
    f_out_time = 0
    f_in_time = 0
    f_out_y = 0.0
    f_in_y = 0.0

    /**
     * @param {fe.Surface} parent Surface to attach to
     * @param {Settings} cfg Layout settings
     */
    constructor(parent, cfg) {
        this.cfg = cfg
        width = parent.texture_width
        height = parent.texture_height
        r_dur = cfg.anim_list_slide ? 400.0 : 1.0
        h_dur = f_dur = cfg.anim_list_slide ? 400.0 : 1.0

        if (cfg.card_enable) {
            head_h = cfg.head_h
            y1 = height * 0.5 - cfg.card_h * 0.5 - head_h
            y2 = height * 0.5 + cfg.card_h * 0.5
        } else {
            head_h = ceil2(cfg.head_char * 0.7 + cfg.safe_margin + cfg.clm_pad * 2.0)
            y1 = height * 0.5 - cfg.safe_h * 0.5 - cfg.safe_margin
            y2 = height * 0.5 + cfg.safe_h * 0.5 + cfg.safe_margin - head_h
        }

        // Header shows display name
        header1 = parent.add_text("[DisplayName]", 0, y1, width, head_h)

        // Footer shows filter name, and animates between list transitions
        footer1 = parent.add_text("[FilterName]", 0, y2, width, head_h)
        footer2 = parent.add_text("[FilterName]", 0, y2, width, head_h)
        footer2.x = footer1.x + width

        header1.align = footer1.align = footer2.align = Align.MiddleCentre
        header1.style = footer1.style = footer2.style = cfg.list_bold
        header1.char_size = footer1.char_size = footer2.char_size = cfg.head_char
        header1.word_wrap = footer1.word_wrap = footer2.word_wrap = true
        header1.margin = footer1.margin = footer2.margin = 0
        header1.line_spacing = footer1.line_spacing = footer2.line_spacing = 0.85
        header1.set_rgb(cfg.col_heading[0], cfg.col_heading[1], cfg.col_heading[2])
        footer1.set_rgb(cfg.col_heading[0], cfg.col_heading[1], cfg.col_heading[2])
        footer2.set_rgb(cfg.col_heading[0], cfg.col_heading[1], cfg.col_heading[2])

        if (cfg.debug_element) {
            header1.bg_alpha = footer1.bg_alpha = 80
            header1.set_bg_rgb(0, 255, 0)
            footer1.set_bg_rgb(0, 255, 0)
        }

        if (!cfg.card_enable) {
            header1.set_bg_rgb(cfg.col_box[0], cfg.col_box[1], cfg.col_box[2])
            footer1.set_bg_rgb(cfg.col_box[0], cfg.col_box[1], cfg.col_box[2])
            footer2.set_bg_rgb(cfg.col_box[0], cfg.col_box[1], cfg.col_box[2])
            header1.bg_alpha = footer1.bg_alpha = footer2.bg_alpha = cfg.col_box_alpha
            header1.y = -head_h
            footer1.y = footer2.y = height
            header1.align = Align.BottomCentre
            footer1.align = footer2.align = Align.TopCentre
            header1.margin = footer1.margin = footer2.margin = cfg.clm_pad
        }
    }

    /** Draw the elements */
    function draw(ttime) {
        if (r_dur) {
            local d = ease_out(ttime - r_time, r_dir, -r_dir, r_dur)
            local n = cos(d * PI * 0.5)
            local x = d > 0 ? 1.0 - n : n - 1.0
            footer1.x = width * x
            footer2.x = footer1.x + footer2.filter_offset * width
        }

        if (!cfg.card_enable) {
            if (h_out_time && ttime >= h_out_time) {
                header1.y = ease_in(ttime - h_out_time, h_out_y, -head_h, h_dur)
            } else if (ttime >= h_in_time) {
                header1.y = ease_out(ttime - h_in_time, h_in_y, y1 - h_in_y, h_dur)
                h_out_y = header1.y
            }

            if (f_out_time && ttime >= f_out_time) {
                footer1.y = ease_in(ttime - f_out_time, f_out_y, head_h, f_dur)
                footer2.y = footer1.y
            } else if (ttime >= f_in_time) {
                footer1.y = ease_out(ttime - f_in_time, f_in_y, y2 - f_in_y, f_dur)
                f_out_y = footer2.y = footer1.y
            }
        }
    }

    /** Update the heading to match the new game list */
    function next_list(ttime, var, index, size) {
        if (!cfg.card_enable) {
            h_in_time = f_in_time = ttime - cfg.frame_time
            h_out_time = f_out_time = h_in_time + h_dur + cfg.show_duration
            h_in_y = h_out_y = header1.y
            f_in_y = f_out_y = footer1.y
        }
        r_time = ttime - cfg.frame_time
        r_dir = var.tofloat()
        footer2.filter_offset = var ? -var : 1
        local sublist = fe.list.clones_list || fe.list.search_rule.len()
        footer1.msg = sublist ? "[[FilterName]]" : "[FilterName]"
    }
}

// endregion
// region Card -------------------------------------------------------------------------

class Card {
    /** @type {Settings} */
    cfg = null
    /** @type {fe.Surface} */
    surf = null
    /** @type {fe.Rectangle} */
    bg = null
    /** @type {fe.Text} */
    label_top = null
    /** @type {fe.Text} */
    label_bottom = null
    /** @type {Image} */
    img = null

    width = 0
    height = 0
    fav_top = ""
    fav_bottom = ""

    /**
     * @param {fe.Surface} parent Surface to attach to
     * @param {Settings} cfg Layout settings
     */
    constructor(parent, cfg) {
        this.cfg = cfg
        width = parent.texture_width
        height = parent.texture_height

        // Hold card elements, also gets animated for the float effect
        surf = parent.add_surface(width * 0.5, height * 0.5, width, height)
        surf.anchor = Anchor.Centre

        // Solid colour base defining the card object
        bg = surf.add_rectangle(width * 0.5, height * 0.5, 0, 0)
        bg.anchor = Anchor.Centre
        bg.corner_radius_x = cfg.radius_x
        bg.corner_radius_y = cfg.radius_y
        bg.corner_points = cfg.style_corner
        bg.set_rgb(cfg.col_card[0], cfg.col_card[1], cfg.col_card[2])
        bg.outline = cfg.ol_size
        bg.outline_alpha = cfg.col_ol_alpha
        bg.set_outline_rgb(cfg.col_ol[0], cfg.col_ol[1], cfg.col_ol[2])
        bg.visible = cfg.ol_size || cfg.card_pad || cfg.info_enable
        bg.alpha = cfg.col_card_alpha

        fav_top = cfg.info_fav == "Top" ? "[FavouriteHeart] " : ""
        fav_bottom = cfg.info_fav == "Bottom" ? "[FavouriteHeart] " : ""

        // Labels for game information within the card
        label_top = surf.add_text(fav_top + cfg.info_top_msg, 0, 0, 0, cfg.label_h)
        label_bottom = surf.add_text(fav_bottom + cfg.info_bottom_msg, 0, 0, 0, cfg.label_h)
        label_top.set_rgb(cfg.col_info[0], cfg.col_info[1], cfg.col_info[2])
        label_bottom.set_rgb(cfg.col_info[0], cfg.col_info[1], cfg.col_info[2])
        label_top.align = label_bottom.align = Align.MiddleCentre
        label_top.margin = label_bottom.margin = p240(max(2, cfg.card_pad)) + cfg.radius_x
        label_top.char_size = label_bottom.char_size = cfg.label_char
        label_top.style = label_bottom.style = cfg.info_style
        label_top.visible = label_bottom.visible = cfg.info_enable
        label_top.alpha = label_bottom.alpha = cfg.col_info_alpha

        if (cfg.info_enable && (label_top.height - label_top.glyph_size) % 2 != 0) {
            label_top.height--
            label_bottom.height--
        }

        img = Image(surf, cfg)
    }

    /** Draw the elements */
    function draw(ttime) {
        // Draw the image first since we need its mask size to redraw the card
        img.draw(ttime)

        bg.width = img.mask.width + cfg.card_pad_size * 2.0
        bg.height = img.mask.height + label_top.height * 2.0

        label_top.y = bg.y - bg.height * 0.5
        label_bottom.y = bg.y + bg.height * 0.5 - label_bottom.height
        label_top.width = label_bottom.width = ceil2(bg.width)
        label_top.x = label_bottom.x = bg.x - label_top.width * 0.5

        if (cfg.anim_card_float) {
            local dir = cfg.card_left ? -1.0 : 1.0
            local gx = (cos(ttime * 0.0011) + 1.0) * 0.5
            local gy = cos(ttime * 0.001) * 0.5
            local gp = dir * gx
            surf.y = height * (0.5 + 0.01 * gy)
            surf.width = width * (1.0 - 0.02 * (2.0 * gx))
            surf.height = height * (1.0 - 0.02 * gp)
            surf.pinch_y = height * -0.02 * gp
            img.gloss.subimg_y = -dir * (2.0 * gx - 1.0) * 0.9 + 1.0
        }
    }

    function get_msg() {
        return to_float(fe.game_info(Info.Votes)) ? cfg.auto_score_msg : cfg.auto_msg
    }

    /** Reset the card when the game has changed */
    function reset(ttime) {
        img.reset(ttime)
        label_top.visible = label_bottom.visible = cfg.info_enable && fe.list.size > 0

        if (cfg.info_score != "None") {
            local v = to_float(fe.game_info(Info.Votes))
            if (cfg.info_score == "Top") {
                label_top.msg = fav_top + (v ? cfg.score_msg : cfg.info_top_msg)
            } else {
                label_bottom.msg = fav_bottom + (v ? cfg.score_msg : cfg.info_bottom_msg)
            }
        }
    }
}

// endregion
// region Image ------------------------------------------------------------------------

class Image {
    /** @type {Settings} */
    cfg = null
    /** @type {fe.Surface} */
    surf = null
    /** @type {fe.Surface} */
    mask = null
    /** @type {fe.Surface} */
    content = null
    /** @type {fe.Text} */
    placeholder = null
    /** @type {fe.Image} */
    art = null
    /** @type {fe.Surface} */
    gloss = null

    /** @type {Rect} */
    from_rect = null
    /** @type {Rect} */
    to_rect = null

    from_time = 0
    dur = 0
    width = 0.0
    height = 0.0

    /**
     * @param {fe.Surface} parent Surface to attach to
     * @param {Settings} cfg Layout settings
     */
    constructor(parent, cfg) {
        this.cfg = cfg
        width = parent.texture_width - cfg.card_pad_size * 2.0
        height = cfg.card_h - cfg.label_h * 2.0
        from_rect = Rect()
        to_rect = Rect()
        dur = cfg.anim_card_resize ? 150.0 : 1.0
        local x = cfg.card_pad_size
        local y = (parent.texture_height - height) * 0.5
        local z = hypot(width, height)

        // Holds the image and content
        surf = parent.add_surface(x, y, width, height)

        /**
         * @trick Content Crop - The artwork cropped-corner effect
         * - The mask rectangle combines with the content BlendMode.Multiply to reveal it
         * - This mask is resized to fit the dimensions of the content artwork
         * - The artwork must be 1 pixel larger to prevent the mask bleeding through edge-aligned subpixels
         */
        local rc = cfg.style_contour ? 1.0 : 0.0
        local ry = max(0, cfg.inner_radius_y - rc * (cfg.label_h - cfg.card_pad_size))
        local rx = ry ? max(0, (cfg.inner_radius_x * ry) / cfg.inner_radius_y) : 0
        mask = surf.add_rectangle(0, 0, 0, 0)
        mask.corner_radius_x = rx
        mask.corner_radius_y = ry
        mask.corner_points = cfg.style_corner

        // Holds all the content that gets masked by the cornered surface above
        content = surf.add_surface(width, height)
        content.blend_mode = BlendMode.Multiply

        // Fallback text is visible when there is no artwork covering it
        placeholder = content.add_text("[Title]", 0, 0, width, height)
        placeholder.align = Align.MiddleCentre
        placeholder.margin = (width - (height * 3.0) / 4.0) * 0.5 + p240(10)
        placeholder.char_size = p240(12)
        placeholder.style = cfg.list_bold
        placeholder.word_wrap = true
        placeholder.bg_alpha = 255

        // Artwork displays an image or video
        local resource = cfg.card_flags == -1 ? "-" : "snap"
        art = content.add_artwork(resource, 0, 0, width, height)
        art.video_flags = cfg.card_flags

        // Gloss overlay effect over the image content, up-scaled to create a smooth gradient
        gloss = content.add_surface(8, 8)
        gloss.add_rectangle(0, 0, 8, 2)
        gloss.set_pos(width * 0.5, height * 0.5, z, z)
        gloss.anchor = Anchor.Centre
        gloss.rotation_origin = Origin.Centre
        gloss.blend_mode = BlendMode.Add
        gloss.repeat = true
        gloss.rotation = -45
        gloss.alpha = cfg.card_gloss
    }

    /** Draw the elements */
    function draw(ttime) {
        local t = dur > 1 && from_rect.width ? ttime - from_time : dur
        local x = ease_out(t, from_rect.x, to_rect.x - from_rect.x, dur)
        local y = ease_out(t, from_rect.y, to_rect.y - from_rect.y, dur)
        local w = ease_out(t, from_rect.width, to_rect.width - from_rect.width, dur)
        local h = ease_out(t, from_rect.height, to_rect.height - from_rect.height, dur)

        mask.set_pos(x, y, w, h)
        art.set_pos(x - 0.5, y - 0.5, w + 1.0, h + 1.0)

        /**
         * @trick Parallax - Card artwork depth effect
         * - Enlarge the artwork subimg, then move it in time with the card float animation
         * - This gives the impression the image is "deeper" than the card, creating a parallax effect
         */
        if (cfg.anim_card_float && cfg.anim_card_parallax && art.texture_width) {
            local dir = cfg.card_left ? -1.0 : 1.0
            local gx = (cos(ttime * 0.0011) + 1.0) * 0.5
            local gy = cos(ttime * 0.001) * 0.5
            local over = round(max(art.texture_width, art.texture_height) * 0.025)
            art.subimg_width = art.texture_width - over
            art.subimg_height = art.texture_height - over
            art.subimg_x = over * (dir > 0 ? 1.0 - gx : gx)
            art.subimg_y = over * (gy + 1.0) * 0.5
        }
    }

    /** Reset the sizing rect to match the new image */
    function reset(ttime) {
        local img_r = width / height
        local art_r = get_ratio(art, cfg.card_ratio)

        from_time = ttime - cfg.frame_time
        from_rect.x = mask.x
        from_rect.y = mask.y
        from_rect.width = mask.width
        from_rect.height = mask.height

        to_rect.width = (art_r < img_r ? ceil2(height * art_r) : width) - cfg.ol_size * 2.0
        to_rect.height = (art_r > img_r ? ceil2(width / art_r) : height) - cfg.ol_size * 2.0
        to_rect.x = (width - to_rect.width) * 0.5
        to_rect.y = (height - to_rect.height) * 0.5

        art.visible = fe.list.size > 0
    }
}

// endregion
// region Letter -----------------------------------------------------------------------

class Letter {
    /** @type {Settings} */
    cfg = null
    /** @type {fe.Surface} */
    surf = null
    /** @type {fe.Surface} */
    shadow_surf = null
    /** @type {fe.Rectangle} */
    shadow = null
    /** @type {fe.Rectangle} */
    over = null
    /** @type {fe.Text} */
    text = null

    req_time = 0
    show_time = 0
    hide_time = 0
    from_alpha = 0
    dur = 300.0

    /**
     * @param {fe.Surface} parent Surface to attach to
     * @param {Settings} cfg Layout settings
     * @param {Background} bg The background to show through the letter
     */
    constructor(parent, cfg, bg) {
        this.cfg = cfg
        if (!cfg.debug_popup && !cfg.scr_letter) return

        local w = parent.texture_width
        local h = parent.texture_height
        local size = cfg.letter_size
        local x = (w - size) * 0.5
        local y = (h - size) * 0.5

        surf = parent.add_surface(w, h)
        surf.alpha = 0

        // Up-scale a tiny surface to create a soft shadow beneath the letter elements
        local s = 5
        shadow_surf = surf.add_surface(s, s)
        shadow_surf.set_pos(x - size * 0.5, y - size * 0.5, size * 2.0, size * 2.0)
        shadow = shadow_surf.add_rectangle(1, 1, s - 2, s - 2)
        shadow.set_rgb(cfg.col_fore[0], cfg.col_fore[1], cfg.col_fore[2])
        shadow.corner_radius = s / 5.0
        shadow.corner_points = cfg.style_corner
        shadow.alpha = 64

        /**
         * @trick Cutout Window - See the background through the foreground
         * - The background surface is passed to this class upon creation
         * - Mask it to match the letter overlay, so it appears to "see through" the foreground
         */
        local cut = surf.add_surface(w, h)
        local cut_bg = cut.add_clone(bg.surf)
        local cut_mask = cut.add_surface(w, h)
        local cut_rect = cut_mask.add_rectangle(0, 0, w, h)
        local cut_shape = cut_mask.add_rectangle(x, y, size, size)
        cut_shape.corner_radius_x = cfg.radius_x
        cut_shape.corner_radius_y = cfg.radius_y
        cut_shape.corner_points = cfg.style_corner
        cut_shape.blend_mode = BlendMode.Subtract
        cut_mask.blend_mode = BlendMode.Subtract
        cut_mask.set_rgb(0, 0, 0)
        cut.blend_mode = BlendMode.Alpha

        over = surf.add_rectangle(x, y, size, size)
        over.corner_radius_x = cfg.radius_x
        over.corner_radius_y = cfg.radius_y
        over.corner_points = cfg.style_corner
        over.set_rgb(cfg.col_card[0], cfg.col_card[1], cfg.col_card[2])
        over.outline = cfg.ol_size
        over.outline_alpha = cfg.col_ol_alpha
        over.set_outline_rgb(cfg.col_ol[0], cfg.col_ol[1], cfg.col_ol[2])
        over.alpha = cfg.col_card_alpha

        text = surf.add_text("[TitleLetter]", x, y, size, size)
        text.char_size = cfg.letter_char
        text.set_rgb(cfg.col_fore[0], cfg.col_fore[1], cfg.col_fore[2])
        text.align = Align.MiddleCentre
        text.style = cfg.list_bold
    }

    /** Draw the elements */
    function draw(ttime) {
        if (!surf) return
        if (ttime >= hide_time) {
            surf.alpha = ease_out(ttime - hide_time, from_alpha, -from_alpha, dur)
        } else if (ttime >= show_time) {
            surf.alpha = ease_out(ttime - show_time, from_alpha, 255 - from_alpha, dur)
            from_alpha = surf.alpha
        }
        if (cfg.debug_popup) surf.alpha = 255
    }

    /** Request to show the letter, when enough requests are made it will appear */
    function request(ttime) {
        if (!surf) return
        if (req_time < ttime) req_time = ttime
        req_time += cfg.key_repeat + cfg.frame_time
        if (req_time - cfg.key_repeat - ttime > 1600.0 / cfg.frame_time) show(ttime)
        if (hide_time > ttime) show(ttime)
    }

    /** Instantly show the letter */
    function show(ttime) {
        if (!surf || !fe.list.size) return
        show_time = ttime - cfg.frame_time
        hide_time = show_time + cfg.show_duration
        from_alpha = surf.alpha
    }
}

// endregion
// region Screensaver ------------------------------------------------------------------

class Screensaver {
    /** @type {Settings} */
    cfg = null
    /** @type {fe.Surface} */
    cont = null
    /** @type {fe.Surface} */
    surf = null

    width = 0
    height = 0
    size = 0.0
    hide_time = 0
    show_time = 0
    alpha_from = 0
    fade_delay = 0
    fade_dur = 500.0

    /**
     * @param {Layout} layout Layout element to attach screensaver to
     * @param {Settings} cfg Layout settings
     */
    constructor(layout, cfg) {
        this.cfg = cfg
        cont = layout.cont
        surf = layout.surf
        width = cont.width
        height = cont.height
        fade_delay = cfg.sav_delay * 1000
        size = min(width, height) * 0.015

        cont.smooth = false
        if (cont.anchor != Anchor.Centre) {
            cont.anchor = Anchor.Centre
            cont.set_pos(cont.width * 0.5, cont.height * 0.5)
        }

        reset(0)
    }

    /** Return true if saver is active */
    function get_active() {
        return cont.alpha == 0
    }

    /** Restart the screensaver delay */
    function reset(ttime) {
        if (!fade_delay) return
        hide_time = ttime + fade_delay - cfg.frame_time
        show_time = ttime - cfg.frame_time
        alpha_from = cont.alpha
        return get_active()
    }

    /** Draw the elements */
    function draw(ttime) {
        if (!fade_delay) return
        local hide = ttime >= hide_time
        local s = hide
            ? ease_out(ttime - hide_time, alpha_from, -alpha_from, fade_dur)
            : ease_out(ttime - show_time, alpha_from, 255.0 - alpha_from, fade_dur)
        if (!hide) alpha_from = s

        /**
         * @trick Mosaic - The SNES-like growing-pixel transition used during the Layout standby effect
         * - Decrease the subimg size while increasing its container size
         * - Results in the resolution lowering while remaining the same visual size
         */
        local m = (1.0 - s / 255.0) * size
        local n = 1.0 + 2.0 * m
        surf.subimg_x = surf.width * -m
        surf.subimg_y = surf.height * -m
        surf.subimg_width = surf.width * n
        surf.subimg_height = surf.height * n
        cont.width = width * n
        cont.height = height * n
        cont.alpha = s
    }
}

// endregion
// region Main -------------------------------------------------------------------------

// Get the user settings
local config = Settings()
local res = config.debug_res
local flw = fe.layout.width
local flh = fe.layout.height

/**
 * @trick Resolution - Debug the entire layout at a different screen size
 * - All elements base their dimensions on their parent containers
 * - Create a top-level surface at a low-res, then up-scale to fit the window
 * - Accurately handles small fonts (adjusting fe.layout dimensions does not)
 */
local scale = min(flw / res[0], flh / res[1])
if (floor(scale)) scale = floor(scale)
local surf = fe.add_surface(res[0], res[1])
surf.set_pos(floor(flw * 0.5), floor(flh * 0.5), res[0] * scale, res[1] * scale)
surf.anchor = Anchor.Centre
surf.smooth = false
Layout(surf, config)

// endregion
