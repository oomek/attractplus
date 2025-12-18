/**
 * SearchPlus Plugin
 *
 * @summary Search the Romlist using an On-Screen-Keyboard.
 * @version 1.3.4 2025-12-10
 * @author Chadnaut
 * @url https://github.com/Chadnaut
 *
 * Tips
 * - Use the `Plugin Options` control to show the last viewed Plugin
 * - Use the `Debug Show` Plugin option while tweaking the settings
 * - Regular keyboard layouts are optimised for hardware - use the Alpha layouts instead
 *
 * Keyboard layouts
 * @see http://xahlee.info/kbd/dvorak_and_all_keyboard_layouts.html
 */

fe.do_nut("input.nut")

// #region Config ----------------------------------------------------------------------------------

class UserConfig
</ help="SearchPlus v1.3.4 by Chadnaut\nSearch the Romlist using an On-Screen-Keyboard" />
{
    </ label="—————— Controls ——————", help="Control Settings", is_info=true, order=0 /> control_section = "————————————————"
    </ label="Show", help="Set the control that toggles the keyboard visibility", options="Custom1,Custom2,Custom3,Custom4,Custom5,Custom6,Custom7,Custom8,Custom9,Custom10", order=1 /> control_toggle = "Custom1"
    </ label="Space", help="Set the control that inserts a space (optional)", options=",Custom1,Custom2,Custom3,Custom4,Custom5,Custom6,Custom7,Custom8,Custom9,Custom10", order=2 /> control_space = ""
    </ label="Backspace", help="Set the control that triggers a backspace (optional)", options=",Custom1,Custom2,Custom3,Custom4,Custom5,Custom6,Custom7,Custom8,Custom9,Custom10", order=3 /> control_backspace = ""
    </ label="Hold Clear", help="Enable holding the 'Show' control to clear the search", options="Yes,No", order=4 /> control_clear = "Yes"
    </ label="Keyboard", help="Enable hardware keyboard input\nSome controls will be unavailable while the Keyboard is showing", options="Yes,No", order=5 /> control_keyboard = "No"
    </ label="Timeout", help="Set the idle time in seconds until the keyboard is automatically hidden", options="Never,15,30,60", order=6 /> control_timeout = "30"

    </ label="—————— Colour ——————", help="Colour Settings", is_info=true, order=7 /> style_colour = "————————————————"
    </ label="Panel Colour", help="Set the panel colour", options="Dark,Light,AM-Dark,AM-Light", order=8 /> col_panel = "Light"
    </ label="Text Colour", help="Set the text colour", options="Dark,Light,AM-Dark,AM-Light", order=9 /> col_text = "Dark"
    </ label="Selection Colour", help="Set the selection colour", options="Dark,Light,AM-Dark,AM-Light", order=10 /> col_selection = "Dark"
    </ label="Glow Colour", help="Set the glow colour", options="Dark,Light,AM-Dark,AM-Light", order=11 /> col_glow = "Dark"
    </ label="Glow Opacity", help="Set the glow opacity", options="Auto,0,10,20,30,40,50,60,70,80,90,100", order=12 /> col_glow_alpha = "Auto"
    </ label="Background Colour", help="Set the background colour", options="Dark,Light,AM-Dark,AM-Light", order=13 /> col_background = "Dark"
    </ label="Background Opacity", help="Set the background opacity", options="Auto,0,10,20,30,40,50,60,70,80,90,100", order=14 /> col_background_alpha = "Auto"

    </ label="—————— Style ——————", help="Style Settings", is_info=true, order=15 /> style_section = "————————————————"
    </ label="Mode", help="Set the panel display mode", options="Box,Bar", order=16 /> style_mode = "Box"
    </ label="Align", help="Set the panel alignment", options="Left,Centre,Right,Top,Middle,Bottom", order=17 /> style_align = "Centre"
    </ label="Size", help="Set the keyboard size", options="Auto,Smallest,Smaller,Small,Medium,Large,Larger,Largest", order=18 /> style_size = "Auto"
    </ label="Ratio", help="Set the key ratio", options="Auto,Narrowest,Narrower,Narrow,Square,Wide,Wider,Widest", order=19 /> style_ratio = "Wide"
    </ label="Padding", help="Set the keyboard padding", options="None,Small,Medium,Large", order=20 /> style_padding = "Medium"
    </ label="Text", help="Set the text size", options="Small,Medium,Large,Larger", order=21 /> style_text = "Medium"
    </ label="Corner Style", help="Set the corner style", options="Square,Bevel,Round", order=22 /> style_corner = "Round"
    </ label="Corner Size", help="Set the corner size", options="Small,Medium,Large,Larger", order=23 /> style_radius = "Medium"
    </ label="Icons", help="Enable icons for Delete, Space, and Backspace", options="Yes,No", order=24 /> style_icons = "Yes"

    </ label="—————— Search ——————", help="Search Settings", is_info=true, order=25 /> search_section = "————————————————"
    </ label="Layout", help="Set the keyboard layout", options="Auto,AlphaBox,AlphaCol,AlphaRow,Alphabetical,Asset,Azerty,Beakl,Canary,Colemak,ColemakDH,Dvorak,Engrammer,Halmak,Maltron,Minimak,Norman,Qfmlwy,Qwerty,Qwertz,Qwpr,Workman", order=26 /> search_layout = "Auto"
    </ label="Results", help="Set the search results presentation\nSelect: Jump to the result, Filter: Show all matching results", options="Select,Filter", order=27 /> search_results = "Filter"
    </ label="Fields", help="Set the available search fields\nAccepts Info types separated by a semicolon", order=28 /> search_fields = "Title;Manufacturer;Category;Year"

    </ label="—————— Animation ——————", help="Animation Settings", is_info=true, order=29 /> anim_section = "————————————————"
    </ label="Selection", help="Enable the key selection animation", options="Yes,No", order=30 /> anim_sel = "Yes"
    </ label="Show", help="Enable the keyboard toggle animation", options="Yes,No", order=31 /> anim_show = "Yes"
    </ label="Motion", help="Enable motion during the show animation", options="Yes,No", order=32 /> anim_motion = "Yes"

    </ label="—————— Debug ——————", help="Debug Settings", is_info=true, order=33 /> debug_section = "————————————————"
    </ label="Debug Show", help="Show the keyboard to make configuration easier", options="Yes,No", order=34 /> debug_show = "No"
}

if (FeVersionNum < 320) fe.log("Warning: SearchPlus requires Attract-Mode Plus v3.2.0 or higher")

// #endregion
// #region Helpers ---------------------------------------------------------------------------------

local flw = fe.layout.width
local flh = fe.layout.height
local par = fe.layout.preserve_aspect_ratio
local ease_in = @(t, b, c, d) ease.in_cubic(clamp(t, 0.0, d), b, c, d)
local ease_out = @(t, b, c, d) ease.out_cubic(clamp(t, 0.0, d), b, c, d)
local p240 = @(x) round((x * min(flw, flh)) / 240.0)
local to_float = @(v, d = 0.0) regexp("^-?[\\d\\.]+$").match(v.tostring()) ? v.tofloat() : d

function get_clipboard_value() {
    local value = get_clipboard()
    if (FeLogLevel == "debug") fe.log("SearchPlus get clipboard: " + value)
    if (value == null || value == "") return ""
    return split(value, "\r\n")[0]
}

function set_clipboard_value(value) {
    if (FeLogLevel == "debug") fe.log("SearchPlus set clipboard: " + value)
    set_clipboard(value)
}

// #endregion

class SearchPlus {
    // #region Properties --------------------------------------------------------------------------

    static CLEAR = "clear"
    static SPACE = "space"
    static BACKSPACE = "backspace"
    static FIELD = "field"
    static PREV = "prev"
    static NEXT = "next"

    static LEFT = "left"
    static RIGHT = "right"
    static TOP = "top"
    static BOTTOM = "bottom"
    static FADE = "fade"

    static MAX_HISTORY = 20
    static ZORDER = 2147483000 // 2147483647 is max
    static INFO = getconsttable()["Info"]

    keyboards = {
        AlphaBox =
            "{:field:6};ABCDEF;GHIJKL;MNOPQR;STUVWX;YZ1234;567890;{CLR:clear}{SPACE: :4}{DEL:backspace}",
        AlphaCol =
            "{:field:4};ABCD;EFGH;IJKL;MNOP;QRST;UVWX;YZ12;3456;7890;{CLR:clear}{SPACE: :2}{DEL:backspace}",
        AlphaRow =
            "{:field:13};ABCDEFGHIJKLM;NOPQRSTUVWXYZ;{CLR:clear}1234567890{SPACE: :1}{DEL:backspace}",
        Alphabetical =
            "{:field:10};1234567890;ABCDEFGHIJ;KLMNOPQRS{DEL:backspace};{CLR:clear}TUVWXYZ{SPACE: :2}",
        Asset = "{:field:10};1234567890;QWJFGYPUL{DEL:backspace};ASETDHNIOR;{CLR:clear}ZXCVBKM{SPACE: :2}",
        Azerty = "{:field:10};1234567890;AZERTYUIOP;QSDFGHJKLM;{CLR:clear}WXCVBN{SPACE: :2}{DEL:backspace}",
        Beakl = "{:field:10};1234567890;QHOUXGCRFZ;YIEADSTNB{DEL:backspace};{CLR:clear}JKWMLPV{SPACE: :2}",
        Canary = "{:field:10};1234567890;WLYPKZXOU{DEL:backspace};CRSTBFNEIA;{CLR:clear}JVDGQMH{SPACE: :2}",
        Colemak =
            "{:field:10};1234567890;QWFPGJLUY{DEL:backspace};ARSTDHNEIO;{CLR:clear}ZXCVBKM{SPACE: :2}",
        ColemakDH =
            "{:field:10};1234567890;QWFPBJLUY{DEL:backspace};ARSTGMNEIO;{CLR:clear}ZXCDVKH{SPACE: :2}",
        Dvorak = "{:field:10};1234567890;{SPACE: :2}PYFGCRL{DEL:backspace};AOEUIDHTNS;{CLR:clear:1}QJKXBMWVZ",
        Engrammer =
            "{:field:10};1234567890;BYOULDWVZ{DEL:backspace};CIEAHTSNQ{CLR:clear};GXJKRMFP{SPACE: :2}",
        Halmak = "{:field:10};1234567890;WLRBZQUDJ{DEL:backspace};SHNTAEOI{SPACE: :2};{CLR:clear}FMVCGPXKY",
        Maltron =
            "{:field:10};1234567890;QPYCBVMUZL;ANISFDTHOR;{CLR:clear}JGEWKX{SPACE: :2}{DEL:backspace}",
        Minimak =
            "{:field:10};1234567890;QWDRKYUIOP;ASTFGHJEL{DEL:backspace};{CLR:clear}ZXCVBNM{SPACE: :2}",
        Norman = "{:field:10};1234567890;QWDFKJURL{DEL:backspace};ASETGYNIOH;{CLR:clear}ZXCVBPM{SPACE: :2}",
        Qfmlwy = "{:field:10};1234567890;QFMLWYUOBJ;DSTNRIAEH{DEL:backspace};{CLR:clear}ZVGCXPK{SPACE: :2}",
        Qwerty = "{:field:10};1234567890;QWERTYUIOP;ASDFGHJKL{DEL:backspace};{CLR:clear}ZXCVBNM{SPACE: :2}",
        Qwertz = "{:field:10};1234567890;QWERTZUIOP;ASDFGHJKL{DEL:backspace};{CLR:clear}YXCVBNM{SPACE: :2}",
        Qwpr = "{:field:10};1234567890;QWPRFYUKL{DEL:backspace};ASDTGHNIOE;{CLR:clear}ZXCVBJM{SPACE: :2}",
        Workman =
            "{:field:10};1234567890;QDRWBJFUP{DEL:backspace};ASHTGYNEOI;{CLR:clear}ZXMCVKL{SPACE: :2}"
    }

    /** Keyboard must have alpha/numeric/space */
    sanity = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ "

    /** @type {UserConfig} */
    cfg = null

    /** @type {Input} */
    input = null

    /** @type {feSurface} */
    master_surf = null
    /** @type {feSurface} */
    panel_surf = null
    /** @type {feSurface} */
    panel_obj = null
    /** @type {feRectangle} */
    background_rect = null
    /** @type {feRectangle} */
    sel_rect = null
    /** @type {feText} */
    cursor = null
    /** @type {feText} */
    field_txt = null

    flw = flw
    flh = flh
    par = par

    kb_grid = []
    kb_rows = 0
    kb_cols = 0
    kb_x = 0
    kb_y = 0

    anim_wrap = true
    focus_transition = false
    cursor_pos = 0

    search_rule_rex = null
    search_fields = []
    search_field = ""
    search_term = ""
    search_history = []

    fe_input_mappings = {}
    fe_ui_colour = "255,255,255"
    fe_key_repeat = 40.0
    fe_key_delay = 400.0
    fe_hide_brackets = true
    fe_frame_time = 1000.0 / ScreenRefreshRate

    has_focus = false
    has_results = false
    is_filter = false
    is_wrap = false
    is_ignoring = false

    sel_last = { x = 0, y = 0, a = 0, bg_a = 0 }
    sel_from = null
    sel_to = null
    sel_time = 0
    sel_dur = 0.0
    click_time = 0
    click_dur = 500.0
    focus_time = 0
    focus_dur = 300.0
    focus_start = 0
    cursor_time = 0
    bg_time = 0
    bg_dur = 300.0
    pending = false
    pending_time = 0
    pending_dur = 1000.0 / ScreenRefreshRate
    pending_typing_dur = 350.0

    col_glow = [255, 255, 255]
    glow_alpha = 255
    panel_col = [0, 0, 0]
    txt_col = [255, 255, 255]
    sel_alpha = 128
    sel_outline = 5
    sel_col = [100, 100, 100]
    sel_outline_col = [255, 255, 255]
    background_col = [0, 0, 0]
    background_alpha = 128

    function get_option(name, values) {
        local keys = split(UserConfig.getattributes(name).options, ",")
        local i = keys.find(cfg[name])
        return i != null ? values[i] : values[keys.find(UserConfig[name])]
    }

    // #endregion
    // #region Constructor -------------------------------------------------------------------------

    constructor() {
        input = Input()

        // Get general config
        local general = fe.get_general_config()
        fe_key_repeat = general.selection_speed_ms.tointeger()
        fe_key_delay = general.selection_delay_ms.tointeger()
        fe_hide_brackets = general.hide_brackets.tolower() != "no"
        fe_ui_colour = general.ui_color
        fe_input_mappings = fe.get_input_mappings()

        // Plugin config
        cfg = fe.get_config()
        foreach (k, v in cfg) cfg[k] = v == "Yes" ? true : v == "No" ? false : v

        // Massage config
        cfg.control_toggle = cfg.control_toggle.tolower()
        cfg.control_space = cfg.control_space.tolower()
        cfg.control_backspace = cfg.control_backspace.tolower()
        sel_outline = p240(2)
        local trns = [LEFT, RIGHT, RIGHT, TOP, BOTTOM, BOTTOM]
        cfg.style_text = get_option("style_text", [0.75, 1.0, 1.25, 1.5])
        local is_sidebar = cfg.style_mode != "Box"
        focus_transition = cfg.anim_motion && is_sidebar ? get_option("style_align", trns) : FADE
        cfg.control_timeout = cfg.debug_show ? 0 : to_float(cfg.control_timeout, 0) * 1000
        refresh_colours()

        // Setup keyboard
        is_filter = cfg.search_results == "Filter"
        local lx = "Qwerty"
        local ly = "AlphaBox"
        local layout = cfg.search_layout
        if (layout == "Auto")
            layout = is_sidebar ? get_option("style_align", [ly, ly, ly, lx, lx, lx]) : ly

        kb_grid = parse_grid_config(layout)
        kb_rows = kb_grid.len()
        kb_cols = kb_grid.reduce(@(r, v) v.len() > r.len() ? v : r).len()

        // Vars
        local key_ratio = get_option("style_ratio", [0, 0.5, 0.7, 0.85, 1.0, 1.15, 1.3, 1.5])
        local padding = p240(get_option("style_padding", [0, 2, 3, 5]))
        local points = get_option("style_corner", [1, 2, 12])
        local size = get_option("style_size", [0.0, 0.5, 0.6, 0.7, 0.75, 0.8, 0.9, 1.0])

        // Auto size the keyboard depending on the kb/panel ratio
        if (size == 0.0) {
            local kb_ratio = kb_cols.tofloat() / kb_rows
            local rx = flw / (flh * 0.5)
            local ry = (flw * 0.5) / flh
            local p_ratio = get_option("style_align", [ry, ry, ry, rx, rx, rx])

            size = is_sidebar
                ? fabs(kb_ratio - p_ratio) > 0.25
                    ? get_option("style_align", [0.9, 0.9, 0.9, 0.9, 0.9, 0.9])
                    : get_option("style_align", [0.7, 0.7, 0.7, 0.7, 0.7, 0.7])
                : 0.7
        }

        local glow_size = p240(12)
        local panel_w = is_sidebar ? get_option("style_align", [0.5, 0.5, 0.5, 1.0, 1.0, 1.0]) : 1.0
        local panel_h = is_sidebar ? get_option("style_align", [1.0, 1.0, 1.0, 0.5, 0.5, 0.5]) : 1.0
        local is_row = is_sidebar && get_option("style_align", [0, 0, 0, 1, 1, 1])
        local is_col = is_sidebar && get_option("style_align", [1, 1, 1, 0, 0, 0])
        local is_box = !is_sidebar
        local gw = panel_w * (flw * (is_box ? size : 1.0))
        local gh = panel_h * (flh * (is_box ? size : 1.0))
        local gx = (flw - gw) * get_option("style_align", [0.0, 0.5, 1.0, 0.5, 0.5, 0.5])
        local gy = (flh - gh) * get_option("style_align", [0.5, 0.5, 0.5, 0.0, 0.5, 1.0])
        local key_x = 0.5
        local key_y = 0.5

        // Rule setup
        local info = INFO
        search_rule_rex = regexp2("^([^ ]+) contains (?:\\^\\[\\^\\(\\/\\[\\]\\*\\?)?(.*)$", "i")
        search_fields = split(cfg.search_fields, ";,").filter(@(i, v) v in info)
        if (!search_fields.len()) search_fields.push("Title")
        search_field = search_fields[0]
        parse_rule(fe.list.search_rule)

        // Base surfaces
        // master_surf = fe.add_surface(flw, flh)
        master_surf = fe.monitors[0].add_surface(flw, flh)
        background_rect = master_surf.add_rectangle(0, 0, flw, flh)
        panel_surf = master_surf.add_surface(flw, flh)
        master_surf.zorder = ZORDER
        master_surf.smooth = false

        // Calculate sizes
        local max_w = gw * (is_box ? 1.0 : size) - padding * 2.0
        local max_h = gh * (is_box ? 1.0 : size) - padding * 2.0

        if (key_ratio) {
            local key_r = key_ratio ? kb_cols.tofloat() / (kb_rows.tofloat() / key_ratio) : max_r
            local max_r = max_w / max_h
            max_w = key_r < max_r ? max_h * key_r : max_w
            max_h = key_r > max_r ? max_w / key_r : max_h
        }

        local cw = floor(max_w / kb_cols)
        local ch = floor(max_h / kb_rows)
        local kw = cw * kb_cols
        local kh = ch * kb_rows
        local radius = points == 1 ? 0.0 : get_option("style_radius", [0.1, 0.2, 0.3, 0.5])
        radius *= min(cw, ch)

        if (is_box) {
            local kpw = kw + padding * 2.0
            local kph = kh + padding * 2.0
            gx = gx + (gw - kpw) * key_x
            gy = gy + (gh - kph) * key_y
            gw = kpw
            gh = kph
        }

        local px = round(is_row ? 0 : gx)
        local py = round(is_col ? 0 : gy)
        local pw = round(is_row ? flw : gw)
        local ph = round(is_col ? flh : gh)

        if (!is_sidebar) {
            local safe = p240(20)
            local pcx = get_option("style_align", [0.0, 0.5, 1.0, 0.5, 0.5, 0.5])
            local pcy = get_option("style_align", [0.5, 0.5, 0.5, 0.0, 0.5, 1.0])
            px = round(clamp(flw * 0.5 - pw * (1.0 - pcx), safe, flw - pw - safe))
            py = round(clamp(flh * 0.5 - ph * (1.0 - pcy), safe, flh - ph - safe))
        }

        local kx = round((pw - kw) * key_x)
        local ky = round((ph - kh) * key_y)

        // Create the elements
        create_glow(px, py, pw, ph, glow_size, padding, is_box ? radius : 0, points)
        create_panel(px, py, pw, ph, padding, is_box ? radius : 0, points)
        create_sel(radius, points)
        create_keys(kx, ky, kw, kh)

        // Create inputs
        create_inputs()

        // Add handlers
        fe.add_signal_handler(this, "on_signal")
        fe.add_ticks_callback(this, "on_tick")
        fe.add_transition_callback(this, "on_transition")

        // Setup initial state
        set_focus(cfg.debug_show)
        refresh_keys()
        kb_x = is_filter ? 0 : 1
        sel_to = sel_from = get_pos(get_grid_item(kb_x, kb_y).txt)
        focus_time = -10000
        click_time = -10000

        // Draw to show initial state
        on_tick(0)
    }

    // #endregion
    // #region Input -------------------------------------------------------------------------------

    /**
     * Return the part of a signal map that has direction/joystick controls
     * - These mappings wont interfere with keyboard input mappings
     */
    function get_joy_map(signal) {
        return input.get_signal_mapping(signal).filter(function (i, map) {
            local keys = split(map, "+")
            foreach (key in keys) {
                if (key.find("Up") != null) return true
                if (key.find("Down") != null) return true
                if (key.find("Left") != null) return true
                if (key.find("Right") != null) return true
                if (key.find("Joy") != null) return true
            }
            return false
        })
    }

    function create_inputs() {
        // Add inputs
        if (cfg.control_keyboard) {
            // Keyboard style controls
            local key_strings = {
                // Numbers
                ["Num1-Shift,Numpad1"] = "1",
                ["Num2-Shift,Numpad2"] = "2",
                ["Num3-Shift,Numpad3"] = "3",
                ["Num4-Shift,Numpad4"] = "4",
                ["Num5-Shift,Numpad5"] = "5",
                ["Num6-Shift,Numpad6"] = "6",
                ["Num7-Shift,Numpad7"] = "7",
                ["Num8-Shift,Numpad8"] = "8",
                ["Num9-Shift,Numpad9"] = "9",
                ["Num0-Shift,Numpad0"] = "0",
                // Shifted Numbers
                ["Num1+Shift"] = "!",
                ["Num2+Shift"] = "@",
                ["Num3+Shift"] = "#",
                ["Num4+Shift"] = "$",
                ["Num5+Shift"] = "%",
                ["Num6+Shift"] = "^",
                ["Num7+Shift"] = "&",
                ["Num8+Shift"] = "*",
                ["Num9+Shift"] = "(",
                ["Num0+Shift"] = ")",
                // Extra
                ["LBracket-Shift"] = "[",
                ["RBracket-Shift"] = "]",
                ["Semicolon-Shift"] = ";",
                ["Comma-Shift"] = ",",
                ["Period-Shift"] = ".",
                ["Quote-Shift"] = "'",
                ["Slash-Shift"] = "/",
                ["Backslash-Shift"] = "\\",
                ["Tilde-Shift"] = "`",
                ["Equal-Shift"] = "=",
                ["Dash-Shift"] = "-",
                // Shifted Extra
                ["LBracket+Shift"] = "{",
                ["RBracket+Shift"] = "}",
                ["Semicolon+Shift"] = ":",
                ["Comma+Shift"] = "<",
                ["Period+Shift"] = ">",
                ["Quote+Shift"] = "\"",
                ["Slash+Shift"] = "?",
                ["Backslash+Shift"] = "|",
                ["Tilde+Shift"] = "~",
                ["Equal+Shift"] = "+",
                ["Dash+Shift"] = "_",
                // Other
                ["Add"] = "+",
                ["Subtract"] = "-",
                ["Multiply"] = "*",
                ["Divide"] = "/",
                ["Space"] = " "
            }

            // Alpha keys, with shifted and capslock states
            for (local k = 'A'; k <= 'Z'; k++) {
                local upper = k.tochar()
                local lower = upper.tolower()
                key_strings[
                    upper +
                        "-Shift-CapsLock-Control-Alt-System," +
                        upper +
                        "+Shift+CapsLock-Control-Alt-System"
                ] <- lower
                key_strings[
                    upper +
                        "+Shift-CapsLock-Control-Alt-System," +
                        upper +
                        "-Shift+CapsLock-Control-Alt-System"
                ] <- upper
            }

            // Add mappings for all keys
            foreach (k, v in key_strings) {
                input.add({
                    combo = k,
                    opaque = v,
                    on_press = action_insert.bindenv(this),
                    repeat = true
                })
            }

            // Special actions for function keys
            input.add({
                mapping = [["Backspace"]],
                repeat = true,
                on_press = action_backspace.bindenv(this)
            })
            input.add({
                mapping = [["Delete"]],
                repeat = true,
                on_press = action_delete.bindenv(this)
            })
            input.add({ mapping = [["Home"]], on_press = action_home.bindenv(this) })
            input.add({ mapping = [["End"]], on_press = action_end.bindenv(this) })
            input.add({ mapping = [["Tab"]], on_press = action_field.bindenv(this) })

            // Allow multiple modifier keys for copy/paste
            // - Control (Windows/Linux), System (Mac), Alt (Mac + non-mac-kb)
            input.add({
                mapping = [["Control", "C"], ["Alt", "C"], ["System", "C"]],
                on_press = action_copy.bindenv(this)
            })
            input.add({
                mapping = [["Control", "V"], ["Alt", "V"], ["System", "V"]],
                on_press = action_paste.bindenv(this)
            })
            input.add({
                mapping = [["Control", "Z"], ["Alt", "Z"], ["System", "Z"]],
                repeat = true,
                on_press = action_undo.bindenv(this)
            })

            local map_select = get_joy_map("select")
            map_select.push(["Return"])

            input.add({
                mapping = map_select,
                repeat = true,
                on_press = action_select.bindenv(this)
            })
            input.add({
                mapping = get_joy_map("left"),
                opaque = -1,
                repeat = true,
                on_press = action_cursor.bindenv(this)
            })
            input.add({
                mapping = get_joy_map("right"),
                opaque = 1,
                repeat = true,
                on_press = action_cursor.bindenv(this)
            })
            input.add({
                mapping = get_joy_map("up"),
                on_press = action_up.bindenv(this),
                repeat = true
            })
            input.add({
                mapping = get_joy_map("down"),
                on_press = action_down.bindenv(this),
                repeat = true
            })
            input.add({
                signal = cfg.control_toggle,
                on_release = action_show.bindenv(this),
                on_held = cfg.control_clear ? action_clear.bindenv(this) : null,
                opaque = false,
                block = false
            })
            input.add({
                mapping = get_joy_map(cfg.control_toggle),
                on_release = action_hide.bindenv(this),
                on_held = cfg.control_clear ? action_clear.bindenv(this) : null
            })
            if (cfg.control_space.len())
                input.add({
                    mapping = get_joy_map(cfg.control_space),
                    on_press = action_insert.bindenv(this),
                    opaque = " ",
                    repeat = true
                })
            if (cfg.control_backspace.len())
                input.add({
                    mapping = get_joy_map(cfg.control_backspace),
                    on_press = action_backspace.bindenv(this),
                    repeat = true
                })
        } else {
            // Cabinet style controls use the signals
            input.add({ signal = "up", on_press = action_up.bindenv(this), repeat = true })
            input.add({ signal = "down", on_press = action_down.bindenv(this), repeat = true })
            input.add({ signal = "left", on_press = action_left.bindenv(this), repeat = true })
            input.add({ signal = "right", on_press = action_right.bindenv(this), repeat = true })
            input.add({ signal = "select", on_press = action_select.bindenv(this), repeat = true })
            input.add({
                signal = cfg.control_toggle,
                on_release = action_toggle.bindenv(this),
                on_held = cfg.control_clear ? action_clear.bindenv(this) : null,
                block = false
            })
            if (cfg.control_space.len())
                input.add({
                    signal = cfg.control_space,
                    on_press = action_insert.bindenv(this),
                    opaque = " ",
                    repeat = true
                })
            if (cfg.control_backspace.len())
                input.add({
                    signal = cfg.control_backspace,
                    on_press = action_backspace.bindenv(this),
                    repeat = true
                })
        }
    }

    // #endregion
    // #region Theme -------------------------------------------------------------------------------

    function refresh_colours() {
        local ui_light = split(fe_ui_colour, ",").map(@(c) c.tointeger())
        local ui_dark = ui_light.map(@(c) mix(0, c, 160.0 / 255.0))
        local dark_col = [0, 0, 0]
        local light_col = [255, 255, 255]
        local mid_col = [100, 100, 100]
        local cols = [dark_col, light_col, ui_dark, ui_light]

        local glow_alpha_auto = get_option("col_glow", [25, 20, 40, 40])
        col_glow = get_option("col_glow", cols)
        glow_alpha = (to_float(cfg.col_glow_alpha, glow_alpha_auto) * 255) / 100

        local background_alpha_auto = get_option("col_background", [50, 25, 40, 40])
        background_col = get_option("col_background", cols)
        background_alpha = (to_float(cfg.col_background_alpha, background_alpha_auto) * 255) / 100

        panel_col = get_option("col_panel", cols)
        sel_col = get_option("col_selection", cols)

        if (cfg.col_text != cfg.col_panel) {
            txt_col = get_option("col_text", cols)
        } else {
            // If the panel == text color, tint the text a little so its visible
            local cols2 = [
                dark_col.map(@(c) mix(c, 255, 0.2)),
                light_col.map(@(c) mix(c, 0, 0.2)),
                ui_dark.map(@(c) mix(c, 0, 0.2)),
                ui_light.map(@(c) mix(c, 255, 0.2))
            ]
            txt_col = get_option("col_text", cols2)
        }

        sel_alpha = get_option("col_selection", [64, 64, 160, 160])
        sel_outline_col = txt_col
    }

    // #endregion
    // #region Drawing -----------------------------------------------------------------------------

    /** Resize the master surface to match the layout */
    function resize_surf() {
        local flw = fe.layout.width
        local flh = fe.layout.height
        local par = fe.layout.preserve_aspect_ratio
        if (flw == this.flw && flh == this.flh && par == this.par) return

        this.flw = flw
        this.flh = flh
        this.par = par
        local fr = flw.tofloat() / flh
        local sr = master_surf.width.tofloat() / master_surf.height
        local nw = par ? (sr < fr ? flw : flh * sr) : flw
        local nh = par ? (sr < fr ? flw / sr : flh) : flh
        master_surf.set_pos((flw - nw) * 0.5, (flh - nh) * 0.5, nw, nh)
    }

    /** Draw the panel glow */
    function create_glow(x, y, width, height, size, pad = 0, rad = 0, points = 1) {
        background_rect.set_rgb(background_col[0], background_col[1], background_col[2])

        if (glow_alpha == 0) return
        local bev = rad && points == 2

        // https://www.shadertoy.com/view/4llXD7
        local sdRoundBox =
            @"float sdBox(vec2 p, vec2 b, vec4 r) {
                r.xy = (p.x>0.0) ? r.xy : r.zw;
                r.x  = (p.y>0.0) ? r.x  : r.y;
                vec2 q = abs(p)-b+r.x;
                return min(max(q.x,q.y),0.0) + length(max(q,0.0)) - r.x;
            }"

        // https://www.shadertoy.com/view/3fc3zs
        local sdChamferBox =
            @"float sdBox(vec2 p, vec2 b, vec4 r) {
                p = abs(p) - b;
                p = (p.y>p.x) ? p.yx : p.xy;
                p.y += r.x;
                const float k = 1.0-sqrt(2.0);
                if( p.y<0.0 && p.y+p.x*k<0.0 ) return p.x;
                if( p.x<p.y ) return (p.x+p.y)*sqrt(0.5);
                return length(p);
            }"

        // 3.2.0 has a new `compile_shader` function
        // - Here we use it to insert the correct sdBox to match our panel corners
        // - This avoids shader branching, but care must be taken to align the args
        local shader = fe.compile_shader(
            Shader.Fragment,
            format(
                @"#version 120
                uniform vec2 size = vec2(0.0);
                uniform vec4 rect = vec4(0.0);
                uniform float radius = 0.0;
                uniform float glow = 0.0;
                uniform float offset = 0.0;

                %s

                void main() {
                    vec2 uv = gl_TexCoord[0].xy;
                    uv.y = (1.0 - uv.y);
                    vec2 sc = size / size.y;
                    vec4 rz = rect * sc.xyxy;

                    vec2 si = rz.zw / size * 0.5;
                    vec2 p = (uv * sc) - (rz.xy / size) - si;
                    vec4 ra = vec4(radius / size.y);
                    float d = (sdBox( p, si, ra ) * size.y - offset) / glow;
                    float a = pow(clamp(1.0 - d, 0.0, 1.0), 2.0);

                    gl_FragColor = gl_Color;
                    gl_FragColor.a *= a;
                }",
                bev ? sdChamferBox : sdRoundBox
            )
        )

        // The glow shader inset prevents bevel corner artifacts
        local ins = bev ? size * 0.5 : 0.0
        shader.set_param("size", flw, flh)
        shader.set_param("rect", x + ins, y + ins, width - ins * 2.0, height - ins * 2.0)
        shader.set_param("radius", rad ? rad + (pad - ins) * (bev ? sqrt(2.0) * 0.5 : 1.0) : 0)
        shader.set_param("glow", size)
        shader.set_param("offset", ins)

        local bg_glow = panel_surf.add_surface(flw, flh)
        bg_glow.set_pos(0, 0, flw, flh)
        bg_glow.set_rgb(col_glow[0], col_glow[1], col_glow[2])
        bg_glow.blend_mode = BlendMode.Alpha
        bg_glow.alpha = glow_alpha
        bg_glow.shader = shader
    }

    /** Draw the keyboard panel */
    function create_panel(x, y, width, height, pad = 0, radius = 0, points = 1) {
        panel_obj = panel_surf.add_surface(x, y, width, height)

        // background is upscaled to create gradient
        local s = 8
        local panel_bg = panel_obj.add_surface(s, s)
        panel_bg.set_pos(0, 0, width, height)

        // rect creates a gradient between colour and outline when upscaled
        local panel_alpha = 252
        local panel_rect = panel_bg.add_rectangle(0, 1, s, s - 2)
        panel_rect.set_rgb(panel_col[0], panel_col[1], panel_col[2])
        panel_rect.alpha = panel_alpha
        panel_rect.outline = 1
        panel_rect.set_outline_rgb(panel_col[0], panel_col[1], panel_col[2])
        panel_rect.outline_alpha = panel_alpha - 18

        // mask clips the panel corners
        local panel_mask = panel_obj.add_rectangle(0, 0, width, height)
        panel_mask.blend_mode = BlendMode.Subtract
        panel_mask.set_rgb(0, 0, 0)
        panel_mask.alpha = 0
        panel_mask.outline = pad + radius

        if (radius) {
            panel_mask.corner_points = points
            panel_mask.corner_radius = radius + pad * (points == 2 ? 0.5 : 1.0)
        }
    }

    function create_sel(radius, points) {
        sel_rect = panel_obj.add_rectangle(0, 0, 0, 0)
        sel_rect.set_outline_rgb(sel_outline_col[0], sel_outline_col[1], sel_outline_col[2])
        sel_rect.set_rgb(sel_col[0], sel_col[1], sel_col[2])
        sel_rect.alpha = sel_alpha
        sel_rect.corner_radius = radius
        sel_rect.corner_points = points

        cursor = panel_obj.add_text("|", 0, 0, 0, 0)
        cursor.set_rgb(txt_col[0], txt_col[1], txt_col[2])
        cursor.visible = cfg.control_keyboard
    }

    function get_icon(item) {
        switch (item.opaque) {
            case CLEAR:
                return "🗑"
            case " ":
                return "␠"
            case BACKSPACE:
                return "␈"
            default:
                return item.msg
        }
    }

    /** Create all keyboard key elements */
    function create_keys(x, y, width, height) {
        local ky = y
        local kh = height / kb_rows
        local ic = cfg.style_icons ? 0.5 : 0.3

        foreach (row, grid_row in kb_grid) {
            local kx = x
            local kw = width / grid_row.len()
            local char_size = min(kw, kh)
            foreach (col, item in grid_row) {
                local item_w = item.size * kw

                if (item.size) {
                    local fi = item.opaque == FIELD
                    local msg = cfg.style_icons ? get_icon(item) : item.msg
                    local ks = (fi ? 0.4 : item.msg.len() == 1 ? 0.5 : ic) * cfg.style_text
                    item.txt <- panel_obj.add_text(msg, kx, ky + row * kh, item_w, kh)
                    item.txt.set_outline_rgb(txt_col[0], txt_col[1], txt_col[2])
                    item.txt.outline = 0.1 // improves gamma on tiny fonts
                    item.txt.set_rgb(txt_col[0], txt_col[1], txt_col[2])
                    item.txt.align = Align.MiddleCentre
                    item.txt.char_size = ceil2(char_size * ks)

                    if (fi) {
                        field_txt = item.txt
                        cursor.set_pos(0, field_txt.y, 0, kh)
                        cursor.align = Align.MiddleCentre
                        cursor.char_size = field_txt.char_size
                    } else {
                        item.txt.margin = 0
                    }
                }

                kx += item_w
            }
        }
    }

    // #endregion
    // #region Actions -----------------------------------------------------------------------------

    /** Call the appropriate input action */
    function call_item_action(item) {
        switch (item.opaque) {
            case CLEAR:
                return action_clear(item)
            case BACKSPACE:
                return action_backspace(item)
            case FIELD:
                return action_field(item)
            case PREV:
                return select_next_game(-1)
            case NEXT:
                return select_next_game(1)
            default:
                return action_insert(item)
        }
    }

    /** Move the selector up */
    function action_up(input) {
        local next = (kb_y - 1 + kb_rows) % kb_rows
        if (input.is_repeating && next > kb_y) next = 0
        is_wrap = next > kb_y
        kb_y = next
        focus_restart()
        return true
    }

    /** Move the selector down */
    function action_down(input) {
        local next = (kb_y + 1 + kb_rows) % kb_rows
        if (input.is_repeating && next < kb_y) next = kb_rows - 1
        is_wrap = next < kb_y
        kb_y = next
        focus_restart()
        return true
    }

    /** Move the selector left */
    function action_left(input) {
        local next = resolve_next_master_col(kb_x, kb_y, -1)
        if (input.is_repeating && next > kb_x) next = 0
        is_wrap = next > kb_x
        kb_x = next
        focus_restart()
        return true
    }

    /** Move the selector right */
    function action_right(input) {
        local next = resolve_next_master_col(kb_x, kb_y, 1)
        if (input.is_repeating && next < kb_x) next = kb_cols - 1
        is_wrap = next < kb_x
        kb_x = next
        focus_restart()
        return true
    }

    /** Fire the assigned input action, and start a click animation */
    function action_select(input = null) {
        click_time = fe.layout.time - fe_frame_time
        call_item_action(get_grid_item(kb_x, kb_y))
        focus_restart()
        return true
    }

    /** Toggle the keyboard focus */
    function action_toggle(input = null) {
        set_focus(!get_focus())
    }

    /** Show keyboard */
    function action_show(input = null) {
        if (!get_focus()) set_focus(true)
    }

    function action_hide(input = null) {
        if (get_focus()) set_focus(false)
    }

    /** Clear the search_term, ensuring the current game remains selected */
    function action_clear(item = null) {
        if (cfg.control_keyboard && get_focus() && item.opaque == false) return
        local name = fe.game_info(Info.Name)
        local emu = fe.game_info(Info.Emulator)
        set_search_term("")
        cursor_end()
        apply_search()
        select_game(name, emu)
        focus_restart()
    }

    /** Select the next search_field */
    function action_field(item = null) {
        local i = search_fields.find(search_field)
        if (i == null) i = 0
        i = (i + 1) % search_fields.len()
        if (search_field == search_fields[i]) return
        search_field = search_fields[i]
        flag_search(false)
        focus_restart()
    }

    /** Delete the character before the cursor */
    function action_backspace(item = null) {
        if (!cfg.control_keyboard) cursor_end()
        if (cursor_pos == 0) return
        set_search_term(search_term.slice(0, cursor_pos - 1) + search_term.slice(cursor_pos))
        cursor_offset(-1)
        flag_search()
        focus_restart()
    }

    /** Delete the character after the cursor */
    function action_delete(item = null) {
        if (!cfg.control_keyboard) cursor_end()
        if (cursor_pos == search_term.len()) return
        set_search_term(search_term.slice(0, cursor_pos) + search_term.slice(cursor_pos + 1))
        flag_search()
        focus_restart()
    }

    /** Inserts a letter at the cursor pos, and moves the cursor behind it */
    function action_insert(item) {
        if (!cfg.control_keyboard) cursor_end()
        set_search_term(
            search_term.slice(0, cursor_pos) + item.opaque + search_term.slice(cursor_pos)
        )
        cursor_offset(item.opaque.len())
        flag_search()
        focus_restart()
    }

    function action_copy(item) {
        set_clipboard_value(search_term)
    }

    function action_paste(item) {
        item.opaque = get_clipboard_value()
        action_insert(item)
        focus_restart()
    }

    function action_undo(item) {
        pop_search_term()
        flag_search()
        focus_restart()
    }

    function action_home(item) {
        cursor_start()
        focus_restart()
    }

    function action_end(item) {
        cursor_end()
        focus_restart()
    }

    function action_cursor(item) {
        focus_restart()
        local field_move = false
        if (get_grid_item(kb_x, kb_y).opaque == FIELD) field_move = cursor_offset(item.opaque)
        if (!field_move) {
            // only move selector if at bounds of field
            if (item.opaque < 0) action_left(item)
            if (item.opaque > 0) action_right(item)
        }
    }

    // #endregion
    // #region Rule --------------------------------------------------------------------------------

    function clear_search_term() {
        search_term = ""
    }

    /** Set the search term */
    function set_search_term(value) {
        search_history.push(search_term)
        if (search_history.len() > MAX_HISTORY) search_history = search_history.slice(-MAX_HISTORY)
        search_term = value
    }

    function pop_search_term() {
        if (search_history.len()) {
            search_term = search_history.pop()
            cursor_end()
            focus_restart()
        }
    }

    /** Format a search_term for regexp use */
    function get_rule_pattern(value) {
        return fe_hide_brackets && search_field == "Title" ? format("^[^(/[]*?%s", value) : value
    }

    /**
     * Set the fe.list.search_rule value
     * - Sets has_results true if matches found
     */
    function set_search_rule(value = "") {
        local rule = value.len()
            ? format("%s contains %s", search_field, get_rule_pattern(value))
            : ""
        if (fe.list.search_rule != rule) {
            is_ignoring = true
            fe.list.search_rule = rule
        }
        has_results = fe.list.search_rule == rule
    }

    /**
     * Trigger a late apply_search call
     * - Allows the key click animation to draw a few frames first
     * - Noticeable on huge lists
     */
    function flag_search(delay = null) {
        if (delay == null) delay = cfg.control_keyboard
        pending = true
        pending_time = fe.layout.time + (delay ? pending_typing_dur : pending_dur)
        if (cfg.control_keyboard) has_results = true
    }

    /** Fire apply_search is a flag_search has been called */
    function refresh_search() {
        if (!pending) return
        if (fe.layout.time < pending_time) return
        apply_search()
    }

    /** Apply the search to filter/select the target game(s) */
    function apply_search() {
        pending = false
        if (is_filter) {
            set_search_rule(search_term)
        } else {
            select_next_game()
        }
    }

    /** Read the give rule into our search_field/search_term properties */
    function parse_rule(rule) {
        local matches = search_rule_rex.capture(rule)
        if (!matches) {
            search_field = search_fields[0]
            clear_search_term()
            cursor_end()
            has_results = false
            return false
        }
        is_ignoring = true
        search_field = rule.slice(matches[1].begin, matches[1].end)
        set_search_term(rule.slice(matches[2].begin, matches[2].end))
        cursor_end()
        has_results = rule.len() != 0
        return true
    }

    /**
     * Move the list index to the game matching the current search_term
     * - Set has_results true if match found
     */
    function select_next_game(dir = 0) {
        local f = INFO[search_field]
        local reg = regexp2(get_rule_pattern(search_term), "i")
        local i = 0
        local n = fe.list.size
        local first = true
        set_search_rule()
        while (true) {
            i = modulo(i + dir, n)
            if (!first && i == 0) break
            if (first) {
                first = false
                if (!dir) dir = 1
            }
            if (reg.search(fe.game_info(f, i)) == null) continue
            fe.list.index = modulo(fe.list.index + i, n)
            return
        }
        has_results = reg.search(fe.game_info(f, i)) != null
    }

    /** Move the list index to the given game */
    function select_game(name, emu) {
        for (local i = 0, n = fe.list.size; i < n; i++) {
            if (fe.game_info(Info.Name, i) != name) continue
            if (fe.game_info(Info.Emulator, i) != emu) continue
            fe.list.index += i
            return
        }
    }

    // #endregion
    // #region Keyboard ----------------------------------------------------------------------------

    /** Return the selected keyboard layout */
    function get_keyboard(keyboard) {
        return keyboard in keyboards ? keyboards[keyboard] : keyboards.Qwerty
    }

    /** Return the closest column of a valid item, looks left until column-zero */
    function resolve_master_col(col, row) {
        local grid_row = kb_grid[row]
        col = min(col, grid_row.len() - 1)
        local item = grid_row[col]
        return item.size ? col : col ? resolve_master_col(col - 1, row) : col
    }

    /** Return the next column of a valid item, wrapping around if necessary */
    function resolve_next_master_col(col, row, col_inc) {
        local i = 0
        local last_col = resolve_master_col(col, row)
        do {
            col = (col + col_inc + kb_cols) % kb_cols
        } while (resolve_master_col(col, row) == last_col && i++ < kb_cols)
        return col
    }

    /** Return the position of the given element */
    function get_pos(e) {
        return { x = e.x, y = e.y, width = e.width, height = e.height }
    }

    /** Return the master grid item at the given position */
    function get_grid_item(col, row) {
        return kb_grid[row][resolve_master_col(col, row)]
    }

    /** Queue up key selection animations */
    function refresh_keys() {
        local sel_col = resolve_master_col(kb_x, kb_y)
        local sel_row = kb_y
        local no_anim = !cfg.anim_sel || (!anim_wrap && is_wrap)

        foreach (row, grid_row in kb_grid) {
            foreach (col, item in grid_row) {
                col = resolve_master_col(col, row)
                item = grid_row[col] // may resolve different than foreach value

                if (col == sel_col && row == sel_row) {
                    sel_from = get_pos(sel_rect)
                    sel_to = get_pos(item.txt)
                    sel_time = fe.layout.time - fe_frame_time
                    sel_dur = no_anim ? 0.1 : fe_key_repeat * 2.5
                    return
                }
            }
        }
    }

    /** Parse a custom grid item, and return the next index of the config string */
    function parse_item_config(item_conf, i, item) {
        local n = item_conf.len()
        local parts = []
        local part = ""
        i++
        while (i < n && item_conf[i] != '}') {
            local c = item_conf[i]
            i++
            if (c == ':') {
                parts.push(part)
                part = ""
            } else {
                part += c.tochar()
            }
        }
        parts.push(part)
        item.msg <- parts[0]
        item.opaque <- parts.len() > 1 ? parts[1] : ""
        item.size <- parts.len() > 2 && parts[2].len() ? parts[2].tointeger() : 1
        return i
    }

    /** Read a keyboard config string and create an object defining the keyboard */
    function parse_grid_config(layout) {
        local i = 0
        local c
        local grid_conf = get_keyboard(layout)
        local n = grid_conf.len()
        local grid_row = []
        local grid = [grid_row]

        local sanity = sanity.slice()
        local duplicate = ""

        while (i < n) {
            c = grid_conf[i]
            local letter = ""
            switch (c) {
                case '{':
                    // parse a custom item {msg:opaque:size}
                    local item = {}
                    i = parse_item_config(grid_conf, i, item)
                    letter = item.opaque

                    // for select-result mode add PREV key
                    local is_select = !is_filter && item.opaque == FIELD
                    if (is_select) item.size -= 2
                    if (is_select) grid_row.push({ msg = "<", opaque = PREV, size = 1 })

                    // add the item, and dummy slots to pad it out
                    grid_row.push(item)
                    for (local s = 1; s < item.size; s++) grid_row.push({ opaque = null, size = 0 })

                    // for select-result mode add NEXT key
                    if (is_select) grid_row.push({ msg = ">", opaque = NEXT, size = 1 })
                    break
                case ';':
                    // create a new row
                    grid_row = []
                    grid.push(grid_row)
                    break
                default:
                    // add a letter
                    letter = c.tochar()
                    grid_row.push({
                        msg = letter,
                        opaque = letter,
                        size = 1
                    })
                    break
            }
            if (letter.len() == 1) {
                local s = sanity.find(letter)
                if (s == null) {
                    duplicate += letter + ","
                } else {
                    sanity = sanity.slice(0, s) + sanity.slice(s + 1)
                }
            }
            i++
        }

        if (sanity.len()) fe.log("Error: SearchPlus '" + layout + "' missing '" + sanity + "'")

        if (duplicate.len())
            fe.log("Error: SearchPlus '" + layout + "' duplicates '" + duplicate + "'")

        local row_len = 0
        local even = true
        foreach (row in grid) {
            if (row_len == 0) row_len = row.len()
            if (row_len != row.len()) even = false
        }

        if (!even) fe.log("Error: SearchPlus '" + layout + "' uneven grid")

        return grid
    }

    // #endregion
    // #region Focus -------------------------------------------------------------------------------

    /** Return true if the keyboard has focus */
    function get_focus() {
        return has_focus
    }

    /** Set focus to the keyboard and queue up show/hide animation */
    function set_focus(f) {
        if (f) focus_restart()
        has_focus = f
        input.focus = f
        focus_time = fe.layout.time - fe_frame_time
        focus_dur = cfg.anim_show ? 300.0 : 0.1
        sel_last = {
            x = panel_surf.x,
            y = panel_surf.y,
            a = panel_surf.alpha,
            bg_a = background_rect.alpha
        }
    }

    function focus_restart() {
        focus_start = fe.layout.time
        refresh_keys()
    }

    function refresh_focus() {
        if (!cfg.control_timeout || !get_focus()) return
        if (fe.layout.time > focus_start + cfg.control_timeout) set_focus(false)
    }

    // #endregion
    // #region Animation ---------------------------------------------------------------------------

    /** Animate the panel/backgroun show/hide */
    function animate_panel() {
        local t = fe.layout.time
        local focus = get_focus()
        if (!focus && !panel_surf.alpha && !background_rect.alpha) return

        local r = focus_transition
        local x = focus ? 0 : master_surf.texture_width
        local y = focus ? 0 : master_surf.texture_height
        local a = focus ? 255 : 0

        local change_x = r == RIGHT ? x - sel_last.x : r == LEFT ? -x - sel_last.x : 0
        local change_y = r == BOTTOM ? y - sel_last.y : r == TOP ? -y - sel_last.y : 0
        local change_a = r == FADE ? a - sel_last.a : 0
        local ease_func = change_a ? ease_out : focus ? ease_out : ease_in

        // subtle motion during alpha fade
        if (cfg.anim_motion) change_y = r == FADE ? y * 0.025 - sel_last.y : change_y

        if (change_x) panel_surf.x = ease_func(t - focus_time, sel_last.x, change_x, focus_dur)
        if (change_y) panel_surf.y = ease_func(t - focus_time, sel_last.y, change_y, focus_dur)
        if (change_a) panel_surf.alpha = ease_func(t - focus_time, sel_last.a, change_a, focus_dur)

        local has_search = search_term.len() > 0
        local field_msg =
            has_search || cfg.control_keyboard
                ? format("%s \"%s\"", search_field, search_term)
                : search_field

        local changed = field_txt.msg != field_msg
        field_txt.msg = field_msg
        field_txt.alpha = !has_search || has_results ? 255 : 128
        if (changed) refresh_cursor()

        local b = focus ? background_alpha : 0
        local bg_dur = focus_dur * 2.0
        background_rect.alpha = ease_out(t - focus_time, sel_last.bg_a, b - sel_last.bg_a, bg_dur)
    }

    /** Animate the selector moving between keys, and the click animation */
    function animate_sel() {
        local t = fe.layout.time
        local p = ease_out(t - sel_time, 0.0, 1.0, sel_dur)
        local c = ease_out(t - click_time, 1.0, -1.0, click_dur)

        sel_rect.outline = c * -sel_outline
        sel_rect.outline_alpha = c * 255
        sel_rect.set_pos(
            mix(sel_from.x, sel_to.x + c * sel_outline, p),
            mix(sel_from.y, sel_to.y + c * sel_outline, p),
            mix(sel_from.width, sel_to.width - c * sel_outline * 2.0, p),
            mix(sel_from.height, sel_to.height - c * sel_outline * 2.0, p)
        )

        // If selection colour same as panel colour make sure an outline is visible
        if (cfg.col_selection == cfg.col_panel) {
            sel_rect.outline = min(sel_rect.outline, -p240(1))
            sel_rect.outline_alpha = max(sel_rect.outline_alpha, sel_alpha)
        }
    }

    // #endregion
    // #region Cursor ------------------------------------------------------------------------------

    function animate_cursor() {
        local t = fe.layout.time

        local ms = t - cursor_time
        local cursor_fade = clamp(sin((ms / 500.0) * PI) * 2.0 + 1.0, 0.0, 1.0) * 255
        cursor.alpha = cursor_fade
    }

    function refresh_cursor() {
        if (!cursor) return
        cursor_time = fe.layout.time
        local x = field_txt.get_cursor_pos(search_field.len() + 2 + cursor_pos)
        cursor.x = floor(field_txt.x + x)
    }

    function set_cursor_pos(pos) {
        local last_pos = cursor_pos
        cursor_pos = pos
        refresh_cursor()
        return cursor_pos != last_pos
    }

    function cursor_offset(inc) {
        return set_cursor_pos(clamp(cursor_pos + inc, 0, search_term.len()))
    }

    function cursor_end() {
        return set_cursor_pos(search_term.len())
    }

    function cursor_start() {
        return set_cursor_pos(0)
    }

    // #endregion
    // #region Handlers ----------------------------------------------------------------------------

    /** Block signals used by the keyboard when it has focus */
    function on_signal(signal) {
        local focus = get_focus()

        if (focus) {
            // Back closes an open keyboard
            if (signal == "back") {
                set_focus(false)
                return true
            }

            // Block all signals when using hardware keyboard
            if (cfg.control_keyboard) return true

            // Prevent game launch while keyboard open
            if (signal == "replay_last_game") return true
        }

        // Clear the current filter
        if (signal == "back" && fe.list.search_rule.len()) {
            action_clear()
            return true
        }
    }

    /** Update inputs and animations */
    function on_tick(ttime) {
        if (get_focus()) {
            resize_surf()
            refresh_search()
            animate_sel()
            animate_cursor()
            refresh_focus()
        }
        animate_panel()
    }

    /** Clear the search_term and hide on list change */
    function on_transition(ttype, var, ttime) {
        switch (ttype) {
            case Transition.ToNewList:
                // Ignore the transition that occurs when setting the search_rule
                if (!is_ignoring) {
                    if (search_term.len()) {
                        clear_search_term()
                        cursor_end()
                        apply_search()
                    }
                    if (!cfg.debug_show) set_focus(false)
                }
                is_ignoring = false
                break
        }
    }

    // #endregion
}

fe.plugin["SearchPlus"] <- SearchPlus()
