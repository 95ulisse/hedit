// All the builtin modules are evaluated in a context where there's a global `__hedit`
// which acts as a bridge between the JS and the C worlds.

export default {
    emit(keys) {
        __hedit.emit(keys);
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
};