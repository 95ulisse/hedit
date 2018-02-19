import log from 'hedit/log';

/** Color names to integers map. */
const colors = {
    white: 0,
    gray: 1,
    blue: 2,
    red: 3,
    pink: 4,
    green: 5,
    purple: 6,
    orange: 7
};

/** A reverse lookup to provide fast automatic guesses of file formats. */
const guessLookup = {
    extension: {},
    magic: {}
};

function storeGuess(name, guess) {
    if (guess) {
        if (guess.extension) {
            guessLookup.extension[guess.extension] = name;
        }
        if (guess.magic) {
            guessLookup.magic[guess.extension] = name;
        }
    }
}

/**
 * Map of all the registered formats.
 * The values are thunks, so that the builtin formats are evaluated only if required.
 */
const allFormats = {};

function linearize(tree) {
    const sep = " >> ";
    const list = [];

    let pos = 0;
    function rec(prefix, children) {
        for (let c of children) {
            if (c.children) {
                // Inner node
                rec(prefix + (c.description ? c.description + sep : ''), c.children);
            } else {
                // Leaf
                if (!c.length) {
                    throw new Error('Missing len on format segment.');
                }
                list.push({
                    name: prefix + (c.description || ''),
                    from: pos,
                    to: pos + c.length - 1,
                    color: colors[c.color] || 0
                });
                pos += c.length;
            }
        }
    }

    rec('', Array.isArray(tree) ? tree : [tree]);
    return list;
}

export default {

    registerBuiltinFormat(formats) {
        for (let k in formats) {
            allFormats[k] = () => __hedit.require('hedit/format/' + k).default;
            storeGuess(formats[k]);
        }
    },

    registerFormat(name, guess, desc) {
        // No duplicate names allowed
        if (allFormats[name]) {
            throw new Error('Duplicate format name: ' + name);
        }

        allFormats[name] = typeof desc === 'function' ? desc : () => desc;
        storeGuess(name, guess);
    },

    // This function will be called by the native code when we need to make
    // a first guess on a freshly opened file.
    guessFormat() {
        return 'none';
    },

    // This function is called evey time the `:set` option `format` changes.
    setFormat(name) {
        const format = allFormats[name];
        if (!format) {
            log.error(`Unknown format ${name}.`);
            return;
        }
        __hedit.file_setFormat(linearize(format()));
    }

};