/**
 * ScorePlus Plugin
 *
 * @summary A five-star post-game rating system.
 * @version 1.0.3 2025-12-22
 * @author Chadnaut
 * @url https://github.com/Chadnaut
 */

fe.do_nut("input.nut")

// #region Config ------------------------------------------------------------------------------------------------------

class UserConfig </ help="ScorePlus v1.0.3 by Chadnaut\nA five-star post-game rating system" /> {
    </ label="—————— Show ——————", help="Show Settings", is_info=true, order=0 /> show_section = "————————————————"
    </ label="When Votes Below", help="Display the voting screen when the total number of votes is below this value", options="0,1,3,5,10,100,1000,1000000", order=1 /> show_votes = "10"
    </ label="When PlayedCount Below", help="Display the voting screen when the total number of plays is below this value", options="0,1,3,5,10,100,1000", order=2 /> show_plays = "0"
    </ label="When PlayedTime Below", help="Display the voting screen when the total play time is below this number of minutes", options="0,1,3,5,10,30,60,600", order=3 /> show_time = "0"
    </ label="When PlayedLast Above", help="Display the voting screen when the time since last played exceeds this many days", options="0,1,7,14,30,60,120,240,360", order=4 /> show_last = "30"
    </ label="Minimum Session", help="The play session must be longer than this many minutes to allow voting", options="0,1,3,5,10,30,60", order=5 /> show_session = "3"

    </ label="—————— Vote ——————", help="Vote Settings", is_info=true, order=6 /> vote_section = "————————————————"
    </ label="Vote Default", help="Set the default score for new votes", options="0,1,2,3,4,5", order=7 /> vote_default = "3"
    </ label="Vote Timeout", help="Set the time in seconds to dismiss the voting screen", options="3,5,10,15", order=8 /> vote_timeout = "5"
    </ label="Vote Weight", help="Set the weight percentage for new votes - larger values have a greater affect on the score\nHelps keep the score fresh, as votes have diminishing returns the more there are", options="Normal,10,20,30,40,50,60,70,80,90,100", order=9 /> vote_weight = "Normal"
    </ label="Allow Zero", help="Allow zero star votes", options="Yes,No", order=10 /> vote_zero = "Yes"
    </ label="Stop Timer", help="Stop the voting timer on user input", options="Yes,No", order=11 /> vote_wait = "No"
    </ label="Show Timer", help="Show the voting timer", options="Yes,No", order=12 /> vote_timer = "Yes"
    </ label="Show Results", help="Show the total score and votes after voting", options="Yes,No", order=13 /> vote_result = "On"
    </ label="Score Labels", help="Set the score labels, separated by a semicolon\n0: Broken, 1: Poor, 2: Fair, 3: Good, 4: Excellent, 5: Outstanding", order=14 /> vote_labels = "Broken;Bad;Poor;Good;Excellent;Outstanding"

    </ label="—————— Animation ——————", help="Animation Settings", is_info=true, order=15 /> animation_section = "————————————————"
    </ label="Motion", help="Enable star jumping animation", options="Yes,No", order=16 /> anim_motion = "On"
    </ label="Transition", help="Enable transition fading animation", options="Yes,No", order=17 /> anim_transition = "On"

    </ label="—————— Debug ——————", help="Debug Settings", is_info=true, order=18 /> debug_section = "————————————————"
    // NOTE: Some layouts expect a Score/Vote change prior to FromGame, which these hotkeys do not provide
    </ label="Vote Hotkey", help="Set the control that toggles the voting screen", options=",Custom1,Custom2,Custom3,Custom4,Custom5,Custom6,Custom7,Custom8,Custom9,Custom10", order=19 /> debug_control = ""
    </ label="Reset Hotkey", help="Set the control that resets the current game's score", options=",Custom1,Custom2,Custom3,Custom4,Custom5,Custom6,Custom7,Custom8,Custom9,Custom10", order=20 /> reset_control = ""
    </ label="Reload", help="Set the control that resets the current game's score", options=",Custom1,Custom2,Custom3,Custom4,Custom5,Custom6,Custom7,Custom8,Custom9,Custom10", order=21 /> reset_control = ""
    </ label="Debug Log", help="Print console logs when votes are changed", options="Yes,No", order=22 /> debug_log = "No"
}

if (FeVersionNum < 320) fe.log("Warning: ScorePlus requires Attract-Mode Plus v3.2.0 or higher")

// #endregion
// #region Helpers -----------------------------------------------------------------------------------------------------

local flw = fe.layout.width
local flh = fe.layout.height
local par = fe.layout.preserve_aspect_ratio
local ease_in = @(t, b, c, d) ease.in_cubic(clamp(t, 0.0, d), b, c, d)
local ease_out = @(t, b, c, d) ease.out_cubic(clamp(t, 0.0, d), b, c, d)
local to_float = @(v, d = 0.0) regexp("^-?[\\d\\.]+$").match(v.tostring()) ? v.tofloat() : d

// #endregion

class ScorePlus {
    // #region Properties ----------------------------------------------------------------------------------------------

    static ICON_STAR_HALF = "⯪"
    static ICON_STAR_EMPTY = "☆"
    static ICON_STAR_FULL = "★"
    static ZORDER = 2147483010 // 2147483647 is max

    flw = flw
    flh = flh
    par = par

    /** @type {UserConfig} */
    cfg = null
    /** @type {Input} */
    input = null

    /** @type {feSurface} */
    master_surf = null
    /** @type {feText} */
    top_txt = null
    /** @type {feText} */
    bot_txt = null
    /** @type {feSurface} */
    timer_surf = null
    /** @type {feRectangle} */
    timer_bg = null
    /** @type {feRectangle} */
    timer_slice = null

    /** @type {array(feText)} */
    star_array = null
    star_pos = null

    // Config
    star_alpha = 0.5 // alpha of un-voted stars
    seq_delay = 50 // delay between sequential star animations
    user_dur = 800 // time to show user score after selection
    jump_dur = 800 // duration of star jump animation
    fade_dur = 400 // duration of star fade animation
    total_dur = 1200 // time to show the total score before losing focus
    timer_rotation = 0
    messages = null

    // State
    score_updated = false
    score = -1
    score_min = 1
    score_max = 5
    focus = true

    // Anim
    launch_last = 0
    launch_time = 0
    focus_time = 0
    toggle_time = 0
    show_from = 0
    show_to = 0
    show_dur = 0
    select_time = 0
    change_time = 0

    // #endregion
    // #region Constructor ---------------------------------------------------------------------------------------------

    constructor() {
        // Config
        input = Input()
        cfg = fe.get_config()
        foreach (k, v in cfg) cfg[k] = v == "Yes" ? true : v == "No" ? false : v

        // Massage
        cfg.show_votes = to_float(cfg.show_votes)
        cfg.show_plays = to_float(cfg.show_plays)
        cfg.show_last = to_float(cfg.show_last) * 24.0 * 60.0 * 60.0 // days to seconds
        cfg.show_time = to_float(cfg.show_time) * 60 * 1000.0 // minutes to milliseconds
        cfg.show_session = to_float(cfg.show_session) * 60.0 * 1000.0 // minutes to milliseconds
        cfg.vote_default = to_float(cfg.vote_default, 3.0)
        cfg.vote_timeout = to_float(cfg.vote_timeout, 3.0) * 1000.0 // seconds to milliseconds
        cfg.vote_weight = to_float(cfg.vote_weight, 0.0) / 100.0
        cfg.debug_control = cfg.debug_control.tolower()
        cfg.reset_control = cfg.reset_control.tolower()
        messages = split(cfg.vote_labels, ";")
        if (messages.len() != score_max + 1) messages = split(UserConfig.vote_labels, ";")
        show_dur = cfg.anim_transition ? 200 : 1
        score_min = cfg.vote_zero ? 0 : 1

        // Inputs
        input.add({ signal = "left", on_press = action_left.bindenv(this) })
        input.add({ signal = "right", on_press = action_right.bindenv(this) })
        input.add({ signal = "select", on_press = action_select.bindenv(this) })
        input.add({ signal = "back", on_release = action_back.bindenv(this) })
        if (cfg.debug_control != "")
            input.add({
                signal = cfg.debug_control,
                block = false,
                on_release = action_toggle.bindenv(this)
            })
        if (cfg.reset_control != "")
            input.add({
                signal = cfg.reset_control,
                block = false,
                on_release = action_reset.bindenv(this)
            })

        // Handlers
        fe.add_ticks_callback(this, "on_tick")
        fe.add_signal_handler(this, "on_signal")
        fe.add_transition_callback(this, "on_transition")

        // Create
        create_overlay()
        set_score(get_score())
        set_focus(false)

        // Special
        override_transition_callback()
    }

    function print_log(value) {
        if (!cfg.debug_log) return
        fe.log("ScorePlus: " + value)
    }

    // #endregion
    // #region Actions -------------------------------------------------------------------------------------------------

    /** Choose a lower score */
    function action_left(option) {
        if (select_time) return
        cancel_timer()
        set_score(ceil(get_score() - 1))
    }

    /** Choose a higher score */
    function action_right(option) {
        if (select_time) return
        cancel_timer()
        set_score(floor(get_score() + 1))
    }

    /** Submit the current score */
    function action_select(option) {
        if (select_time) return
        select_time = fe.layout.time
        score_updated = false
        add_score(get_score())
    }

    /** Close the voting screen */
    function action_back(option) {
        set_focus(false)
    }

    /** Toggle the voting screen */
    function action_toggle(option) {
        set_focus(!get_focus())
    }

    /** Prompt to reset the current games score */
    function action_reset(option) {
        if (fe.list.display_index < 0) return
        local msg = fe.get_text("Reset Score\n'[Title]'\n[Score] [ScoreStarAlt] ([Votes])")
        if (fe.overlay.list_dialog(["Reset", "Cancel"], msg, 1) == 0) clear_score()
    }

    // #endregion
    // #region Create --------------------------------------------------------------------------------------------------

    function create_overlay() {
        local flm = min(flw, flh)
        local title_sz = flm * 0.075
        local footer_sz = flm * 0.075
        local timeout_sz = flm * 0.075
        local star_sz = flm * 0.15
        local tw = flm * 0.9
        local m = flm * 0.025

        master_surf = fe.add_surface(flw, flh)
        master_surf.zorder = ZORDER
        master_surf.alpha = 0

        local bg = master_surf.add_rectangle(0, 0, flw, flh)
        bg.set_rgb(0, 0, 0)

        top_txt = master_surf.add_text("", (flw - tw) * 0.5, 0, tw, (flh - star_sz) * 0.5 - m)
        top_txt.msg = "[Title]"
        top_txt.char_size = title_sz
        top_txt.align = Align.BottomCentre
        top_txt.word_wrap = true
        top_txt.margin = 0

        create_stars(star_sz)

        bot_txt = master_surf.add_text("", 0, (flh + star_sz) * 0.5 + m, flw, footer_sz)
        bot_txt.char_size = footer_sz
        bot_txt.align = Align.TopCentre
        bot_txt.margin = 0

        if (cfg.vote_timer)
            create_timer(flw * 0.5, bot_txt.y + bot_txt.height + m + flm * 0.025, flm * 0.025)
    }

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

    // #endregion
    // #region Timer ---------------------------------------------------------------------------------------------------

    /** Create the timer elements */
    function create_timer(x, y, cr) {
        timer_surf = master_surf.add_surface(x, y, cr * 2, cr * 2)
        timer_surf.anchor = Anchor.Centre
        timer_surf.blend_mode = BlendMode.Add

        timer_bg = timer_surf.add_rectangle(0, 0, cr * 2, cr * 2)
        timer_bg.corner_radius = cr
        timer_bg.corner_points = 24

        timer_slice = timer_surf.add_rectangle(0, 0, 0, 0)
        timer_slice.set_rgb(0, 0, 0)

        local mr = cr * 0.666
        local timer_centre = timer_surf.add_rectangle(cr - mr, cr - mr, mr * 2, mr * 2)
        timer_centre.corner_radius = mr
        timer_centre.corner_points = 24
        timer_centre.set_rgb(0, 0, 0)

        timer_slice.width = cr * 2.0
        timer_slice.height = cr * 0.2
        timer_slice.set_pos(cr, cr)
        timer_slice.anchor = Anchor.Left
        timer_slice.rotation_origin = Origin.Left
        timer_slice.corner_ratio_x = 0.5
        timer_slice.corner_ratio_y = 0.5
        timer_slice.corner_points = 2

        timer_rotation = -90 + (atan(timer_slice.height / timer_slice.width) / PI) * 180
        timer_slice.rotation = timer_rotation
    }

    /** Stop the timer and allow user to vote without auto hide */
    function cancel_timer() {
        if (!cfg.vote_wait || change_time) return
        change_time = fe.layout.time
    }

    /** Set the timer value, where 1.0 == full and 0.0 == empty */
    function update_timer(val) {
        if (cfg.vote_timer) {
            timer_slice.rotation = timer_rotation - val * 360.0
            timer_bg.visible = false
            timer_surf.clear = false
            local t = change_time ? change_time : select_time
            if (t) timer_surf.alpha = ease_out(fe.layout.time - t, 255, -255, fade_dur)
        }
    }

    /** Hide and reset the timer */
    function clear_timer() {
        if (cfg.vote_timer && !timer_surf.clear) {
            timer_bg.visible = true
            timer_slice.rotation = timer_rotation
            timer_surf.clear = true
            timer_surf.alpha = 255
        }
    }

    // #endregion
    // #region Animate -------------------------------------------------------------------------------------------------

    /** Update the vote screen */
    function refresh_dialog() {
        // Exit early if hidden
        if (!get_visible()) {
            select_time = 0
            clear_timer()
            return
        }

        local t = fe.layout.time

        // fade in/out the entire surface
        master_surf.alpha = ease_out(t - toggle_time, show_from, show_to - show_from, show_dur)

        // Count-down the timer
        local timeout = max(0, cfg.vote_timeout - (t - focus_time))

        // Draw the timer
        update_timer(timeout / cfg.vote_timeout)

        // If the timer has run out the close the voting screen
        if (timeout <= 0 && !select_time && !change_time) set_focus(false)

        // No selection has been made yet
        if (select_time == 0) {
            foreach (i, star in star_array) star.alpha = star_alpha * 255
            return
        }

        // the selection animation
        foreach (i, star in star_array) {
            local ts = t - select_time - i * seq_delay

            // bounce
            local str = ease_out(ts, 0.0, 1.0, jump_dur)
            local m = sin(str * 1.25 * PI) * (1.0 - str)

            // brighten
            local a = (str * star_alpha + (1.0 - star_alpha)) * 255

            // and fadeout
            if (cfg.vote_result) a *= ease_out(ts - user_dur, 1, -1, fade_dur)

            if (cfg.anim_motion) star.y = star_pos[i].y + -m * star.height * 0.2
            star.alpha = a
        }

        local th = t - select_time - user_dur - score_max * seq_delay

        if (!cfg.vote_result) {
            if (th > 0) set_focus(false)
            return
        }

        // fade out footer, then fade in again
        bot_txt.alpha =
            ease_out(t - select_time - user_dur, 255, -255, fade_dur) +
            ease_in(th, 0, 255, fade_dur)

        // change the total after user score has faded out
        if (th > 0 && !score_updated) {
            local score = to_float(fe.get_game_info(Info.Score))
            local votes = to_float(fe.get_game_info(Info.Votes))
            set_score(score)
            bot_txt.msg = format("%.1f (%d)", score, votes)
            score_updated = true
        }

        // fade in stars showing total score
        if (th >= 0) {
            foreach (i, star in star_array) {
                star.alpha = ease_in(th - i * seq_delay, 0, 255, fade_dur)
                if (cfg.anim_motion) {
                    local x = star.width * 0.5
                    star.x = star_pos[i].x + ease_out(th - i * seq_delay, x, -x, 500)
                }
            }
        }

        // close the score
        if (th - score_max * seq_delay > fade_dur + total_dur) {
            set_focus(false)
        }
    }

    // #endregion
    // #region Stars ---------------------------------------------------------------------------------------------------

    /** Create the star elements */
    function create_stars(star_sz) {
        star_pos = []
        star_array = []
        for (local i = 0; i < score_max; i++) {
            local star = master_surf.add_text(ICON_STAR_EMPTY, 0, 0, 0, 0)
            star.char_size = star_sz
            star.align = Align.MiddleCentre

            local w = star.msg_width * 1.075
            local h = star.glyph_size
            local x = flw * 0.5 - score_max * 0.5 * w + i * w
            local y = flh * 0.5 - h * 0.5

            star.set_pos(x, y, w, h)
            star_array.push(star)
            star_pos.push({ x = x, y = y, width = w, height = h })
        }
    }

    /** Return array containing stars representing the given value */
    function get_star_chars(val) {
        local val_round = round(val * 2.0) / 2.0
        local val_int = floor(val_round)
        local half = val_round - val_int > 0

        local arr = []
        for (local i = 0; i < val_int; i++) arr.push(ICON_STAR_FULL)
        if (half) arr.push(ICON_STAR_HALF)
        while (arr.len() < score_max) arr.push(ICON_STAR_EMPTY)
        return arr
    }

    // #endregion
    // #region Score ---------------------------------------------------------------------------------------------------

    /**
     * Set the current score, and update the star icons to match
     * @param {integer} val The score to set, must be 0...score_max
     */
    function set_score(val) {
        val = clamp(val, score_min, score_max)
        if (score == val) return
        score = val

        local arr = get_star_chars(score)
        foreach (i, star in arr) star_array[i].msg = star
        bot_txt.msg = messages[score]
    }

    /** Return the current score */
    function get_score() {
        return score
    }

    /** Add the passed score to the current game */
    function add_score(user_score) {
        local old_score = to_float(fe.get_game_info(Info.Score))
        local old_votes = to_float(fe.get_game_info(Info.Votes))
        local new_votes = old_votes + 1

        // minimum weight is `1/votes` (fair)
        // maximum weight is `1` (override)
        local weight = max(1.0, cfg.vote_weight * new_votes) / new_votes
        local new_score = mix(old_score, score, weight)
        print_log(
            format(
                "%.2f(%d) + %.2f(1)(weight:%.2f) = %.2f(%d)",
                old_score,
                old_votes,
                score,
                cfg.vote_weight,
                new_score,
                new_votes
            )
        )

        fe.set_game_info(Info.Score, new_score)
        fe.set_game_info(Info.Votes, new_votes)
    }

    /** Clear the score for the current game */
    function clear_score() {
        fe.set_game_info(Info.Score, 0.0)
        fe.set_game_info(Info.Votes, 0)
    }

    // #endregion
    // #region Focus ---------------------------------------------------------------------------------------------------

    function set_focus(next_focus) {
        if (fe.list.display_index < 0) next_focus = false
        if (focus == next_focus) return
        focus = next_focus
        input.focus = next_focus
        toggle_time = fe.layout.time
        show_from = master_surf.alpha
        show_to = next_focus ? 255 : 0
        if (next_focus) {
            focus_time = fe.layout.time
            change_time = 0
            select_time = 0
            score_updated = false
            set_score(cfg.vote_default)
        }
    }

    function get_focus() {
        return focus
    }

    /** Return true is visible (either focus, or not yet faded out) */
    function get_visible() {
        return get_focus() || master_surf.alpha > 0
    }

    // #endregion
    // #region Condition -----------------------------------------------------------------------------------------------

    function show_voting(session) {
        if (session < cfg.show_session) {
            print_log(format("Skip - Session %d below limit %d", session, cfg.show_session))
            return false
        }

        local votes = to_float(fe.get_game_info(Info.Votes))
        if (votes < cfg.show_votes) {
            print_log(format("Show - Votes %d below limit %d", votes, cfg.show_votes))
            return true
        }

        local playedCount = to_float(fe.get_game_info(Info.PlayedCount))
        if (playedCount < cfg.show_plays) {
            print_log(format("Show - Played Count %d below limit %d", playedCount, cfg.show_plays))
            return true
        }

        local playedTime = to_float(fe.get_game_info(Info.PlayedTime))
        if (playedTime < cfg.show_time) {
            print_log(format("Show - Played Time %d below limit %d", playedTime, cfg.show_time))
            return true
        }

        if (launch_last) {
            local lastOffset = time() - launch_last
            if (lastOffset > cfg.show_last) {
                print_log(
                    format(
                        "Show - Played Last offset %d exceeds limit %d",
                        lastOffset,
                        cfg.show_last
                    )
                )
                return true
            }
        }

        print_log("Skip - All voting quotas reached")
        return false
    }

    // #endregion
    // #region Handlers ------------------------------------------------------------------------------------------------

    function on_tick(ttime) {
        if (get_focus()) resize_surf()
        refresh_dialog()
    }

    function on_signal(signal) {
        if (get_focus()) return true
    }

    function on_transition(ttype, var, ttime) {
        switch (ttype) {
            case Transition.ToGame:
                launch_last = to_float(fe.game_info(Info.PlayedLast))
                launch_time = fe.layout.time
                break
            case Transition.FromGame:
                if (ttime == 0 && show_voting(fe.layout.time - launch_time)) set_focus(true)
                input.on_tick(ttime) // process inputs
                refresh_dialog() // redraw the screen
                if (get_visible()) return true // blocking until hidden
                break
        }

        return call_transition_callbacks(ttype, var, ttime)
    }

    // #endregion
    // #region Override ------------------------------------------------------------------------------------------------

    /**
     * Trigger a layout reload
     * - Used when manually voting or resetting
     */
    function trigger_reload() {
        fe.signal("reload_layout")
    }

    /**
     * Override the `fe.add_transition_callback` method with our own
     * - This allow us to fire the FromGame transition *after* the voting screen has completed
     */
    function override_transition_callback() {
        ::fe_transition_callbacks <- []
        fe.add_transition_callback = @(...) ::fe_transition_callbacks.push(vargv)
    }

    /**
     * Run all registered transition callbacks
     * - This MUST be done since we're handling the transitions now
     */
    function call_transition_callbacks(ttype, var, ttime) {
        foreach (args in ::fe_transition_callbacks) {
            local name = args.top()
            local env = args.len() == 2 ? args[0] : getroottable()
            local func = env[name].bindenv(env)
            if (func(ttype, var, ttime)) return true
        }
        return false
    }

    // #endregion
}

fe.plugin["ScorePlus"] <- ScorePlus()
