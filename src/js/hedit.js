// All the builtin modules are evaluated in a context where there's a global `__hedit`
// which acts as a bridge between the JS and the C worlds.

import EventEmitter from 'hedit/private/eventemitter';

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

/**
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
        log_fatal:    { fg: 5,   bg: 16,  bold: true,  under: false }
    };

    let penDescriptor = {};
    const textprops = [ 'text', 'linenos', 'error', 'block_cursor', 'soft_cursor', 'commandbar', 'statusbar', 'log_debug', 'log_info', 'log_warn', 'log_error', 'log_fatal' ];
    for (let k of textprops) {
        penDescriptor[k] = expandPen(t[k], defaultTheme[k]);
    }

    return penDescriptor;
}

let hedit = new EventEmitter();
Object.assign(hedit, {
    get mode() {
        return __hedit.mode();
    },
    get view() {
        return __hedit.view();
    },
    setTheme(t) {
        __hedit.setTheme(expandTheme(t));
    },
    emitKeys(keys) {
        __hedit.emitKeys(keys);
    },
    command(cmd) {
        return __hedit.command(cmd);
    },
    registerCommand(name, handler) {
        if (!__hedit.registerCommand(name, handler)) {
            throw new Error('Command registration failed.');
        }
    },
    map(mode, from, to, force = false) {
        if (!__hedit.map(mode, from, to, !!force)) {
            throw new Error('Key mapping registration failed.');
        }
    },
    set(name, value) {
        if (!__hedit.set(name, value)) {
            throw new Error(`Failed to set option ${name}.`);
        }
    },
    switchMode(name) {
        __hedit.switchMode(name);
    }
});

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

export default hedit;
