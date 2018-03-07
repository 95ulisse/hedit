import file from 'hedit/file';
import Format from 'hedit/format';
import log from 'hedit/log';

/** A reverse lookup to provide fast automatic guesses of file formats. */
const guessLookup = {
    extension: {},
    magic: [],
    maxMagicLength: 0
};

function storeGuess(name, guess) {
    if (guess) {
        if (guess.extension) {
            guessLookup.extension[guess.extension] = name;
        }
        if (guess.magic) {
            const m = guess.magic.buffer ? new Uint8Array(guess.magic.buffer) : guess.magic;
            guessLookup.magic.push([ m, name ]);
            guessLookup.maxMagicLength = Math.max(guessLookup.maxMagicLength, m.byteLength);
        }
    }
}

/**
 * Map of all the registered formats.
 * The values are thunks, so that the builtin formats are evaluated only if required.
 */
const allFormats = {};

/** Proxy class that records and aggregates the access to the underlying file data. */
class FileProxy {
    read(offset, len) {
        const val = file.read(offset, len);
        log.info(`Read caught: [${offset}, ${offset + len - 1}] => ${new Uint8Array(val)}`);
        return val;
    }
}

/** Wrapper class that caches the values of a linearized format. */
class FormatCache {
    constructor(format) {
        this._format = format;
    }

    [Symbol.iterator]() {
        return this._format.__linearize(new FileProxy(), 0, '', Object.create(null));
    }
}

export default {

    registerBuiltinFormat(formats) {
        for (let k in formats) {
            allFormats[k] = () => __hedit.require('hedit/format/' + k).default;
            storeGuess(k, formats[k]);
        }
    },

    registerFormat(name, guess, desc) {
        // No duplicate names allowed
        if (allFormats[name]) {
            throw new Error('Duplicate format name: ' + name);
        }

        allFormats[name] = () => desc;
        storeGuess(name, guess);
    },

    // This function will be called by the native code when we need to make
    // a first guess on a freshly opened file.
    guessFormat() {

        // First try with the magic
        if (guessLookup.maxMagicLength > 0) {
            const magic = new Uint8Array(file.read(0, guessLookup.maxMagicLength));
            for (let [ k, name ] of guessLookup.magic) {
                if (k.byteLength <= magic.byteLength) {
                    let eq = true;
                    for (let i = 0; i < k.byteLength; i++) {
                        if (k[i] != magic[i]) {
                            eq = false;
                            break;
                        }
                    }
                    if (eq) {
                        log.debug('Guessing format ' + name + ' for matching magic.');
                        return name;
                    }
                }
            }
        }

        // Then with the extension
        const name = file.name;
        if (name) {
            const ext = name.match(/\.\w+$/);
            if (ext && ext[0]) {
                const formatName = guessLookup.extension[ext[0].substring(1)];
                if (formatName) {
                    log.debug('Guessing format ' + formatName + ' for matching extension.');
                    return formatName;
                }
            }
        }

        return 'none';

    },

    // This function is called evey time the `:set` option `format` changes.
    setFormat(name) {
        const format = allFormats[name];
        if (!format) {
            log.error(`Unknown format ${name}.`);
            return;
        }

        let f = format();
        if (!(f instanceof Format)) {
            log.error('Formats must be an instance of the Format class.');
            return;
        }

        __hedit.file_setFormat(new FormatCache(f));
    }

};