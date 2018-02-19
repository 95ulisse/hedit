/**
 * General set of functions to programmatically interact with HEdit.
 * @module hedit
 */

// All the builtin modules are evaluated in a context where there's a global `__hedit`
// which acts as a bridge between the JS and the C worlds.

import EventEmitter from 'hedit/private/eventemitter';
import format from 'hedit/private/format';

function parseHex(str) {
    const m = str.match(/^([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/i);
    if (!m) {
        throw new Error(`Invalid color: ${str}.`);
    }

    let r = parseInt(m[1], 16);
    let g = parseInt(m[2], 16);
    let b = parseInt(m[3], 16);

    // Grays live in another space
    if (r == g && g == b) {
        return 232 + Math.floor(r / 255 * 23);
    } else {
        r = Math.floor(r / 255 * 5);
        g = Math.floor(g / 255 * 5);
        b = Math.floor(b / 255 * 5);
        return 16 + 36 * r + 6 * g + b;
    }
}

function parseColor(c) {
    if (typeof c === 'string') {
        return parseHex(c);
    } else if (typeof c === 'number') {
        const n = Math.floor(c);
        if (n < 0 || n > 255) {
            throw new Error(`Invalid integer color value ${n}.`);
        }
        return n;
    } else {
        throw new Error(`Invalid color: ${c}.`);
    }
}

function expandPen(pen, defaults) {
    
    // Missing property
    if (typeof pen === 'undefined' || pen === null) {
        return defaults;

    // Hex or integer color
    } else if (typeof pen === 'string' || typeof pen === 'number') {
        return Object.assign({}, defaults, { fg: parseColor(pen) });

    // Complex object
    } else if (typeof pen === 'object') {
        let clone = Object.assign({}, defaults, pen);
        clone.fg = parseColor(clone.fg);
        clone.bg = parseColor(clone.bg);
        clone.bold = !!clone.bold;
        clone.under = !!clone.under;
        return clone;

    // Invalid
    } else {
        throw new Error(`Invalid pen descriptor: ${pen}`);
    }

}

/*
 * The user can represent themes with a wide variety of shortcuts,
 * but the final form of the descriptor that we have to pass to the native method is as follows:
 *
 * {
 *     text: { fg: [0 - 255], bg: [0 - 255], bold: true|false, under: true|false },
 *     linenos: ...,
 *     error: ...,
 *     ...
 * }
 *
 * All the properties are required, and must have the same exact format.
 *
 * The user can also specify colors in hex, which means that we have to convert them
 * to terminal colors.
 */

function expandTheme(t) {

    // Default theme
    const defaultTheme = {
        text:         { fg: 7,   bg: 16,  bool: false, under: false },
        linenos:      { fg: 8,   bg: 16,  bold: false, under: false },
        error:        { fg: 1,   bg: 16,  bold: true,  under: false },
        block_cursor: { fg: 16,  bg: 7,   bool: false, under: false },
        soft_cursor:  { fg: 7,   bg: 16,  bold: true,  under: true  },
        statusbar:    { fg: 234, bg: 247, bold: false, under: false },
        commandbar:   { fg: 7,   bg: 16,  bool: false, under: false },
        log_debug:    { fg: 8,   bg: 16,  bold: false, under: false },
        log_info:     { fg: 6,   bg: 16,  bold: false, under: false },
        log_warn:     { fg: 3,   bg: 16,  bold: true,  under: false },
        log_error:    { fg: 1,   bg: 16,  bold: true,  under: false },
        log_fatal:    { fg: 5,   bg: 16,  bold: true,  under: false },
        white:        { fg: 7,   bg: 16,  bold: false, under: false },
        gray:         { fg: 8,   bg: 16,  bold: false, under: false },
        blue:         { fg: 4,   bg: 16,  bold: false, under: false },
        red:          { fg: 1,   bg: 16,  bold: false, under: false },
        pink:         { fg: 13,  bg: 16,  bold: false, under: false },
        green:        { fg: 2,   bg: 16,  bold: false, under: false },
        purple:       { fg: 5,   bg: 16,  bold: false, under: false },
        orange:       { fg: 208, bg: 16,  bold: false, under: false }
    };

    let penDescriptor = {};
    const textprops = [ 'text', 'linenos', 'error', 'block_cursor', 'soft_cursor', 'commandbar', 'statusbar', 'log_debug', 'log_info', 'log_warn', 'log_error', 'log_fatal', 'white', 'gray', 'blue', 'red', 'pink', 'green', 'purple', 'orange' ];
    for (let k of textprops) {
        penDescriptor[k] = expandPen(t[k], defaultTheme[k]);
    }

    return penDescriptor;
}

class HEdit extends EventEmitter {

    /**
     * The name of the mode the editor is currently in.
     * @type {string}
     */
    get mode() {
        return __hedit.mode();
    }

    /**
     * The name of the currently active view.
     * @type {string}
     */
    get view() {
        return __hedit.view();
    }

    /**
     * Sets the current theme.
     *
     * A theme determines the colors used by the editor to render itself.
     * Different parts of the UI can be drawn with different pens, which contains the actual
     * style information. For example, to draw the line offsets bold green and text blue, we can use:
     *
     * ```
     * {
     *     linenos: { fg: '00ff00', bold: true },
     *     text: '0000ff' // Shortcut for `{ fg: '0000ff' }`
     * }
     * ```
     *
     * The following parts of the UI can be themed:
     * - `text`
     * - `linenos`
     * - `error`
     * - `block_cursor`
     * - `soft_cursor`
     * - `statusbar`
     * - `commandbar`
     * - `log_debug`
     * - `log_info`
     * - `log_warn`
     * - `log_error`
     * - `log_fatal`
     *
     * Each of these parts can be themed with a pen, which has 4 properties:
     * - `fg`
     * - `bg`
     * - `bold`
     * - `under`
     *
     * Colors can be specified either using standard ANSI number or hex RGB values,
     * which will be rounded to the closest color supported by the terminal.
     *
     * If any of the previous properties is missing, the default value will be used.
     *
     * For an example of the usage of `setTheme`, see {@tutorial colorful-statusbar}.
     *
     * @param {object} t - Theme description.
     * @throws Throws if an invalid theme description is passed.
     * @see Usage example: {@tutorial colorful-statusbar}.
     */
    setTheme(t) {
        __hedit.setTheme(expandTheme(t));
    }

    /**
     * Emits the given keys as if the user actually typed them.
     *
     * @alias module:hedit.emitKeys
     * @param {string} keys - Keys to emit.
     *
     * @example
     * hedit.emitKeys('<Escape>:w<Enter>i');
     */
    emitKeys(keys) {
        __hedit.emitKeys(keys);
    }

    /**
     * Executes a command.
     * @param {string} cmd - Command to execute.
     * @return {boolean} Returns `true` if the command executed successfully, `false` otherwise.
     *
     * @example
     * hedit.command('q!'); // Exits the editor
     */
    command(cmd) {
        return __hedit.command(cmd);
    }

    /**
     * Handler of a custom command.
     * @callback CommandCallback
     * @param {...string} args - All the commands supplied by the user at the moment
              of the invocation of the command.
     */

    /**
     * Registers a new command, whose implementation is up to the user.
     * @param {string} name - Command name.
     * @param {CommandCallback} handler - Function implementing the command.
     * @throws Throws if the command registration fails.
     *
     * @example
     * hedit.registerCommand('special', n => {
     *     log.info('The argument is', parseInt(n, 10) % 2 == 0 ? 'even' : 'odd');
     * });
     */
    registerCommand(name, handler) {
        if (typeof handler !== 'function') {
            throw new Error('Handler must be a function.');
        }
        if (!__hedit.registerCommand(name, handler)) {
            throw new Error('Command registration failed.');
        }
    }

    /**
     * Handler of a custom option.
     * @callback OptionCallback
     * @param {string} newValue - New value of the option.
     * @return {boolean} Returns `true` if the change is accepted, `false` otherwise.
     */

    /**
     * Registers a new option, whose implementation is up to the user.
     * @param {string} name - Name of the option.
     * @param {string} defaultValue - Default option value.
     * @param {OptionCallback} handler - Function to be called when the value of the option changes.
     * @thorws Throws if the option registration fails.
     *
     * @example
     * hedit.registerOption('cool', false, newValue => {
     *     if (newValue === 'true') {
     *         log.info('The coolness is now on!');
     *         return true;
     *     } else if (newValue === 'false') {
     *         log.info('So sad :(');
     *         return true;
     *     } else {
     *         // Invalid value
     *         return false;
     *     }
     * });
     */
    registerOption(name, defaultValue, handler) {
        if (typeof handler !== 'function') {
            throw new Error('Handler must be a function.');
        }
        if (!__hedit.registerOption(name, defaultValue, handler)) {
            throw new Error('Option registration failed.');
        }
    }

    /**
     * Registers a new key mapping.
     * @param {string} mode - Mode the new mapping is valid in.
     * @param {string} from - Key to map.
     * @param {string} to - The key sequence that the `from` key will be expanded to.
     * @param {boolean} [force = false] - Skip the check for an existing mapping for the same key.
     * @throws Throws if the mapping registration fails.
     * @see [emitKeys]{@link module:hedit.emitKeys} for more information about the format of the keys.
     *
     * @example
     * hedit.map('insert', '<C-j>', 'cafebabe');
     */
    map(mode, from, to, force = false) {
        if (!__hedit.map(mode, from, to, !!force)) {
            throw new Error('Key mapping registration failed.');
        }
    }

    /**
     * Sets the value of an option.
     * @param {string} name - Option name.
     * @param {string|number} value - Value of the option.
     * @throws Throws if there's an error setting the option.
     *
     * @example
     * hedit.set('colwidth', 8);
     */
    set(name, value) {
        if (!__hedit.set(name, value)) {
            throw new Error(`Failed to set option ${name}.`);
        }
    }

    /** Retrives the value of an option.
     * @param {string} name - Option name.
     * @return {*} The current value of the option.
     * @throws Throws if the option name is invalid.
     *
     * @example
     * log.info('Current colwidth:', hedit.get('colwidth'));
     */
    get(name) {
        return __hedit.get(name);
    }

    /**
     * Switches the editor to the given mode.
     * @param {string} name - Name of the mode to switch to.
     *
     * @example
     * hedit.switchMode('insert');
     */
    switchMode(name) {
        __hedit.switchMode(name);
    }

    /**
     * Registers a new file format.
     * @param {string} name - Name of the format. Must be unique.
     * @param {object} [guess] - Hint for automatic selection of format on file open.
     * @param {string} guess.extension - Use this format for the files matching this extension.
     * @param {string} guess.magic - Use this format for the files starting with the given magic.
     * @param {object} desc - Description of the file format.
     * @throws Throws if the name of the format is not unique or if the format description
     *         is invalid.
     */
    registerFormat(name, guess, desc) {
        if (typeof desc === 'undefined' && typeof guess === 'object') {
            desc = guess;
            guess = null;
        }
        format.registerFormat(name, guess, desc);
    }

};

const hedit = new HEdit();

const events = {
    'load': 'load',
    'quit': 'quit',
    'mode_switch': 'modeSwitch',
    'view_switch': 'viewSwitch'
};

__hedit.registerEventBroker((name, ...args) => {
    if (events[name]) {
        hedit.emit(events[name], ...args);
    }
});

/**
 * Event raised only once when the editor is fully loaded and ready.
 * @event load
 */

/**
 * Event raised only once when the editor is going to close.
 * @event quit
 */

/**
 * Event raised when the user switches mode.
 * @param {string} mode - New mode set.
 * @event modeSwitch
 */

/**
 * Event raised when the user switches view.
 * @param {string} view - New active view.
 * @event viewSwitch
 */

export default hedit;
