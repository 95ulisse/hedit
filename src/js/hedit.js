// All the builtin modules are evaluated in a context where there's a global `__hedit`
// which acts as a bridge between the JS and the C worlds.

import EventEmitter from 'hedit/private/eventemitter';

let hedit = new EventEmitter();
Object.assign(hedit, {
    get mode() {
        return __hedit.mode();
    },
    get view() {
        return __hedit.view();
    },
    setTheme(t) {
        __hedit.setTheme(t);
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
