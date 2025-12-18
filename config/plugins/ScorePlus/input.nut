/**
 * Input Module
 *
 * @summary Watches key combos and fire callbacks when they're pressed.
 * @version 1.0.2 2025-12-08
 * @author Chadnaut
 * @url https://github.com/Chadnaut
 *
 * local input = Input()
 * input.add(options)
 * input.focus = true // stops watching if false
 */

class Input {
    /** @private Modifier keys */
    kb_modifiers = [
        "Shift",
        "LShift",
        "RShift",
        "Control",
        "LControl",
        "RControl",
        "Alt",
        "LAlt",
        "RAlt",
        "System",
        "LSystem",
        "RSystem",
        "CapsLock"
    ]

    /**
     * @private Registered keys to watch
     * @type {array}
     */
    kb_inputs = null

    /**
     * @private The state of all added keys
     * @type {table}
     */
    kb_state = null

    /**
     * @private The state of all pressed keys
     * @type {table}
     */
    kb_press = null

    /**
     * @private The keys to watching when not in focus
     * @type {table}
     */
    kb_watch = null

    /** @private AM input mappings */
    fe_input_mappings = null

    /** @private AM key repeat */
    fe_key_repeat = 40.0

    /** @private AM key delay*/
    fe_key_delay = 400.0

    /** When true keys will be watched and the appropriate signals blocked */
    focus = true

    constructor() {
        kb_inputs = []
        kb_state = {}
        kb_press = {}
        kb_watch = {}

        local general = fe.get_general_config()
        fe_key_repeat = general.selection_speed_ms.tointeger()
        fe_key_delay = general.selection_delay_ms.tointeger()
        fe_input_mappings = fe.get_input_mappings()

        fe.add_ticks_callback(this, "on_tick")
        fe.add_signal_handler(this, "on_signal")
    }

    // #region Add ----------------------------------------------------------------------------

    /**
     * Returns the keys mapped to the given signal
     * @returns {array(string)}
     */
    function get_signal_mapping(signal) {
        local mapping = []
        if (signal in fe_input_mappings) mapping.extend(fe_input_mappings[signal])
        return mapping
    }

    /**
     * Add a key to watch and fire a callback when pressed
     * @param {table} options Options table contains watcher configuration
     * - `signal` = `"up"` - AM signal to watch, OR
     * - `mapping` = `[["Up"],["Down+Shift"]]` - list of key combos to watch
     * - `on_press` = `callback(options)` - called on press
     * - `on_release` = `callback(options)` - called on release
     * - `on_held` = `callback(options)` - called on held
     * - `repeat` = `true` - allow key repeat, fires `on_press`
     * - `block` = `true` - blocks signals, if `false` is watched even when `focus = false`
     * - `opaque` = any user data, will contain `signal` if none provided
     */
    function add(options) {
        // Ensure a signal exists for attempted signal blocking
        if (!("signal" in options)) options.signal <- ""

        // Shortcut for setting keys
        if ("combo" in options) options.mapping <- split(options.combo, ",")

        // If no mapping the acquire the keys mapped to the signal
        if (!("mapping" in options)) options.mapping <- get_signal_mapping(options.signal)

        // Split combo into individual keys
        foreach (i, map in options.mapping) {
            if (typeof map == "string") {
                options.mapping[i] = split(join(split(map, "-"), "+-"), "+")
            }
        }

        options = {
            signal = options.signal,
            mapping = options.mapping,
            on_press = "on_press" in options ? options.on_press : null,
            on_release = "on_release" in options ? options.on_release : null,
            on_held = "on_held" in options ? options.on_held : null,
            repeat = "repeat" in options ? options.repeat : false,
            block = "block" in options ? options.block : true,
            opaque = "opaque" in options ? options.opaque : options.signal,
            is_active = false,
            is_held = false,
            is_repeating = false,
            is_locked = false,
            press_time = 0
        }

        // Add mapped keys to the watch list
        foreach (map in options.mapping) {
            foreach (key in map) {
                kb_state[key] <- false
                kb_press[key] <- false
                // These keys are ALWAYS watched, even when focus is false
                if (!options.block) kb_watch[key] <- true
            }
        }

        kb_inputs.push(options)
    }

    // #endregion
    // #region State ----------------------------------------------------------------------------

    /**
     * Update table contain state of all keys
     * - Will perform limited scan if focus is false
     * @private
     */
    function refresh_key_state() {
        local watch = focus ? kb_state : kb_watch
        local state
        foreach (key, s in watch) {
            s = fe.get_input_state(get_key_name(key))
            state = get_key_off(key) ? !s : s
            kb_press[key] = state && !kb_state[key]
            kb_state[key] = state
        }
    }

    /**
     * Returns true if key name has "-"
     * @private
     */
    function get_key_off(key) {
        return key[0] == '-'
    }

    /**
     * Returns the key name (slices off "-")
     * @private
     */
    function get_key_name(key) {
        return get_key_off(key) ? key.slice(1) : key
    }

    /**
     * Returns true if any of the mapped keys (or combos) are active
     * @private
     */
    function get_input_state(input) {
        foreach (map in input.mapping) {
            if (map.len() == 0) continue
            local active = true
            foreach (key in map) active = active && kb_state[key]
            if (active) return true
        }
        return false
    }

    /**
     * Returns true if any of the mapped keys (or combos) are pressed
     * @private
     */
    function get_input_press(input) {
        foreach (map in input.mapping) {
            if (map.len() == 0) continue
            local active = true
            foreach (key in map) {
                active = active && (is_modifier(key) ? kb_state[key] : kb_press[key])
            }
            if (active) return true
        }
        return false
    }

    /**
     * Returns true if key is modifier
     * @private
     */
    function is_modifier(key) {
        return kb_modifiers.find(get_key_name(key)) != null
    }

    // #endregion
    // #region Refresh ----------------------------------------------------------------------------

    /**
     * Check inputs and fire the appropriate callbacks if pressed
     * - Handles key-repeat, and held button callbacks
     * @private
     */
    function refresh_inputs() {
        refresh_key_state()
        local t = fe.layout.time

        foreach (input in kb_inputs) {
            if (!focus && input.block) continue
            local state = get_input_state(input)

            if (input.is_active != state) {
                input.is_active = state
                input.press_time = t + fe_key_delay
                input.is_repeating = false
                if (!input.is_held) {
                    if (get_input_press(input) && input.on_press) {
                        input.on_press(input)
                    } else if (!state && input.on_release) {
                        input.on_release(input)
                    }
                }
                if (!state) input.is_held = false
            } else if (state && input.on_held) {
                if (input.press_time && t - input.press_time >= fe_key_repeat) {
                    input.press_time = 0
                    input.is_held = true
                    input.on_held(input)
                }
            } else if (state && input.repeat) {
                if (t - input.press_time >= fe_key_repeat) {
                    input.is_repeating = true
                    input.press_time += fe_key_repeat
                    input.on_press(input)
                }
            }
        }
    }

    // #endregion
    // #region Handlers ----------------------------------------------------------------------------

    /**
     * Poll the inputs on each tick
     * @private
     */
    function on_tick(ttime) {
        refresh_inputs()
    }

    /**
     * Block signal when focus is true
     * @private
     */
    function on_signal(signal) {
        if (focus) {
            foreach (input in kb_inputs) if (input.block && input.signal == signal) return true
        }
    }

    // #endregion
}
