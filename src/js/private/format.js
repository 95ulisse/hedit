import file from 'hedit/file';
import Format from 'hedit/format';
import log from 'hedit/log';

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

/** Proxy class that records and aggregates the access to the underlying file data. */
class FileProxy {
    read(offset, len) {
        const val = file.read(offset, len);
        log.info(`Read caught: [${offset}, ${offset + len - 1}] => ${new Uint8Array(val)}`);
        return val;
    }
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

        allFormats[name] = () => desc;
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

        let f = format();
        if (typeof f === 'function') {
            f = f(new FileProxy());
        }

        if (!(f instanceof Format)) {
            log.error('Formats must be an instance of the Format class, or functions returning a Format.');
            return;
        }

        __hedit.file_setFormat(f);
    }

};