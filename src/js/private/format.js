import hedit from 'hedit';
import file from 'hedit/file';
import log from 'hedit/log';
import IntervalTree from 'hedit/private/intervaltree';

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
    constructor() {
        this._accessed = new IntervalTree();
    }

    read(offset, len) {
        this._accessed.insert(offset, offset + len - 1);
        return file.read(offset, len);
    }

    hasRead(offset, len) {
        if (len < 0) {
            return false;
        }
        return this._accessed.search(offset, offset + len - 1).length > 0;
    }
}

/** Wrapper class that caches the values of a linearized format. */
class FormatCache {
    constructor(format) {
        this._format = format;
        this.invalidate();
    }

    /** Invalidates all the cached data. */
    invalidate() {
        this._fileProxy = new FileProxy();
        this._generator = this._format.__linearize(this._fileProxy, 0, '', Object.create(null));
        this._cachedSegments = [];
        this._cachedTree = new IntervalTree();
    }

    /**
     * This method is called from the native code to get an iterator every time the screen needs to be repainted.
     * The iterator returned must also expose a `seek` method to position the iterator at a given byte offset.
     * 
     * The returned iterator iterates over the cached segments and advances the underlying format iterator
     * only when needed.
     */
    [Symbol.iterator]() {
        let i = 0;
        let ended = false;

        return {
            next: function () {

                // Do nothing if the iterator ended already
                if (ended) {
                    return { done: true };
                }

                // Return a cached segment if available
                if (i < this._cachedSegments.length) {
                    i++;
                    return { value: this._cachedSegments[i], done: false };
                }
                
                // Otherwise advance the original generator and cache the new segment
                const { done, value } = this._generator.next();
                if (done) {
                    ended = true;
                    return { done: true };
                } else {
                    this._cachedSegments.push(value);
                    this._cachedTree.insert(value.from, value.to, [ i, value ]);
                    i++;
                    return { value, done: false };
                }
                
            }.bind(this),
            
            seek: function (pos) {
                
                // Do nothing if the iterator ended already
                if (ended) {
                    return null;
                }

                // Search for a cached segment
                const [ res ] = this._cachedTree.search(pos, pos);
                if (res) {
                    const [ newIndex, seg ] = res;
                    i = newIndex + 1;
                    return seg;
                }

                // Advance the iterator until we reach the position `pos`
                while (true) {
                    const { done, value } = this._generator.next();
                    if (done) {
                        ended = true;
                        return null;
                    } else {

                        // Cache the segment
                        this._cachedSegments.push(value);
                        this._cachedTree.insert(value.from, value.to, [ i, value ]);
                        i++;

                        // Stop if we reached the target position
                        if (pos <= value.to) {
                            return value;
                        }
                        
                    }
                }
                
            }.bind(this)            
        };
    }
}

// When the contents of the file change, check if we need to invalidate the current format cache
let currentFormatCache = null;
hedit.on('file/change', (offset, len) => {
    if (currentFormatCache && currentFormatCache._fileProxy.hasRead(offset, len)) {
        currentFormatCache.invalidate();
        log.debug('Format cache invalidated.');
    }
});

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

        currentFormatCache = new FormatCache(format());
        __hedit.file_setFormat(currentFormatCache);
    }

};