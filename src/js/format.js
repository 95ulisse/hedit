// Color names to integers map.
const COLORS = {
    white: 0,
    gray: 1,
    blue: 2,
    red: 3,
    pink: 4,
    green: 5,
    purple: 6,
    orange: 7
};

// Helper function to join two optional strings
function join(a, b) {
    if (a && b) {
        return a + ' > ' + b;
    } else {
        return a ? a : b;
    }
}

/**
 * A `Format` represents the binary structure of a file.
 *
 * A format is logically divided in **spans**, each of which holds some information
 * like an user friendly name and its length. The name is displayed in the statusbar
 * at the bottom of the screen to help the user navigate the file.
 *
 * Some file formats have spans that depend on the actual value of some bytes before them
 * (think of a Pascal-style string), so it is needed to establish some kind of dependency
 * on those bytes. To accomplish this, you can assign a variable name to a span and read
 * its value later when needed. See below for an example of the Pascal string format.
 *
 * The `Format` class exposes a fluent API to build formats in a single expression,
 * and can be combined to create complex trees out of simple smaller formats.
 * When you nest formats (using [array()]{@link Format#array}, or [child()]{@link Format#child},
 * as well as when using [group()]{@link Format#group}), you can always specify a name:
 * this name will be prepended to the name of the inner spans, resulting in names with the following structure:
 * ```
 * 'Group 1 > Group 2 > A byte'
 * ```
 *
 * You can access this class importing `hedit/format`:
 * ```
 * import Format from 'hedit/format';
 * ```
 * 
 * @example
 * // This is a format describing a sequence of two integers of diffent lengths
 * const exampleFormat =
 *     new Format()
 *         .uint16('A 16 bit integer', 'orange')
 *         .uint32('A longer 32 bit integer', 'blue');
 *
 * @example
 * // This is a format for a Pascal-like string, i.e. a string with its length prefixed.
 * const pascalString =
 *     new Format()
 *         .uint8('String length', 'orange', 'len')
 *         .array('String contents', 'len', 'blue');
 *         // The line above is a shortcut for:
 *         // .array('String contents', vars => vars.len, 'blue');
 *
 * @see For a list of the builtin formats, check [the source](https://github.com/95ulisse/hedit/tree/master/src/js/format).
 */
class Format {

    constructor() {
        this._segments = [];
    }

    /**
     * Repeates a child format a fixed number of times.
     *
     * @param {string?} name - Name of this segment.
     * @param {integer|function|string} length - How many times to repeat the child format.
     *                                  If it is an integer, it represents a fixed number of repetitions.
     *                                  If it is a function, it is called with a dictionary of all the available variables
     *                                  and is expected to return an integer. If it is a string, it is treated as a
     *                                  shortcut for `vars => vars[length]`.
     * @param {Format} child - Child format to repeat.
     * @return {Format}
     *
     *//**
     *
     * Adds a span of raw bytes to the format.
     *
     * @param {string?} name - Name of this segment.
     * @param {integer|function|string} length - How many times to repeat the child format.
     *                                  If it is an integer, it represents a fixed number of repetitions.
     *                                  If it is a function, it is called with a dictionary of all the available variables
     *                                  and is expected to return an integer. If it is a string, it is treated as a
     *                                  shortcut for `vars => vars[length]`.
     * @param {string} [color=white] - Color of the span.
     * @return {Format}
     */
    array(name, length, child = 'white') {
        const repeat = typeof length === 'string' ? data => 0 + data[length] : length;

        if (child instanceof Format) {

            // A composite child
            this._segments.push({
                child: {
                    *__linearize(proxy, absoffset, basename, variables) {
                        const n = typeof repeat === 'function' ? repeat(variables) : repeat;
                        let offset = absoffset;
                        for (let i = 0; i < n; i++) {
                            for (let childseg of child.__linearize(proxy, offset, join(basename, name), Object.create(variables))) {
                                yield childseg;
                                offset = childseg.to + 1;
                            }
                        }
                    }
                }
            });

        } else if (typeof child === 'string') {

            // Shortcut for a simple array of bytes
            this._segments.push({
                child: {
                    *__linearize(proxy, absoffset, basename, variables) {
                        const n = typeof repeat === 'function' ? repeat(variables) : repeat;
                        if (n > 0) {
                            yield {
                                name: join(basename, name),
                                color: COLORS[child],
                                from: absoffset,
                                to: absoffset + n - 1
                            };
                        }
                    }
                }
            });

        }

        return this;
    }

    /**
     * Adds a child format to this format.
     * This is equivalent to `array(name, 1, c)`.
     *
     * @param {string} name - Name of this span.
     * @param {Format} c - Child format to insert.
     * @return {Format}
     *
     *//**
     *
     * Adds a child format to this format.
     * This is equivalent to `array(null, 1, c)`.
     *
     * @param {Format} c - Child format to insert.
     * @return {Format}
     */
    child(name, c) {
        if (typeof c === 'undefined') {
            c = name;
            name = null;
        }
        return this.array(name, 1, c);
    }

    /**
     * Adds an infinite sequence of the given child format to this format.
     * This is equivalent to `array(name, Infinity, c)`.
     *
     * @param {string} name - Name of this span.
     * @param {Format} c - Child format to repeat indefinitely.
     * @return {Format}
     *
     *//**
     *
     * Adds an infinite sequence of the given child format to this format.
     * This is equivalent to `array(null, Infinity, c)`.
     *
     * @param {Format} c - Child format to repeat indefinitely.
     * @return {Format}
     */
    sequence(name, c) {
        if (typeof c === 'undefined') {
            c = name;
            name = null;
        }
        return this.array(name, Infinity, c);
    }

    /**
     * Groups multiple spans under a single name.
     * There **must** be a matching call to [endgroup()]{@link Format#endgroup}.
     *
     * @param {string} name - Group name.
     * @return {Format}
     *
     * @example
     * // This generates the following two spans with the following names:
     * // Group > A byte
     * // Group > Another byte
     * new Format()
     *     .group('Group')
     *         .uint8('A byte')
     *         .uint8('Another byte')
     *     .endgroup();
     */
    group(name) {
        const childFormat = new Format();
        childFormat._parent = this;
        this.child(name, childFormat);
        return childFormat;
    }

    /**
     * Ends a group started with [group()]{@link Format#group}.
     * There **must** be a matching call to [group()]{@link Format#group}.
     *
     * @return {Format}
     *
     * @example
     * // This generates the following two spans with the following names:
     * // Group > A byte
     * // Group > Another byte
     * new Format()
     *     .group('Group')
     *         .uint8('A byte')
     *         .uint8('Another byte')
     *     .endgroup();
     */
    endgroup() {
        const parent = this._parent;
        if (!parent) {
            throw new Error('Unbalanced group()/endgroup() calls.');
        }
        delete this._parent;
        return parent;
    }

    *__linearize(proxy, absoffset, basename, variables) {

        if (this._parent) {
            throw new Error('Unbalanced group()/endgroup() calls.');
        }

        let offset = absoffset;

        // Iterate over all the segments computing the actual absolute offsets
        for (let seg of this._segments) {
            if (seg.child) {
                for (let childseg of seg.child.__linearize(proxy, offset, join(basename, seg.name), Object.create(variables))) {
                    yield childseg;
                    offset = childseg.to + 1;
                }
            } else if (seg.length > 0) {
                if (seg.id) {
                    variables[seg.id] = seg.read(proxy, offset);
                }
                yield {
                    name: join(basename, seg.name),
                    color: COLORS[seg.color],
                    from: offset,
                    to: offset + seg.length - 1
                };
                offset += seg.length;
            }
        }

    }

};


function addCommonMethod(name, length, m, endianess) {
    if (endianess) {
        
        // Generate two methods for the little and big endian version
        Format.prototype[name + 'le'] = function (name, color = 'white', id) {
            this._segments.push({
                name,
                color,
                length,
                id,
                read(proxy, off) {
                    return m.call(new DataView(proxy.read(off, length)), 0, true /* Little endian */);
                }
            });
            return this;
        };
        Format.prototype[name + 'be'] = function (name, color = 'white', id) {
            this._segments.push({
                name,
                color,
                length,
                id,
                read(proxy, off) {
                    return m.call(new DataView(proxy.read(off, length)), 0, false /* Big endian */);
                }
            });
            return this;
        };
 
        // Name without endianess specification defaults to big endian
        Format.prototype[name] = Format.prototype[name + 'be'];

    } else {

        // Generate a single method regardless of the endianess
        Format.prototype[name] = function (name, color = 'white', id) {
            this._segments.push({
                name,
                color,
                length,
                id,
                read(proxy, off) {
                    return m.call(new DataView(proxy.read(off, length)), 0);
                }
            });
            return this;
        };

    }

}

/**
 * Adds a span of 1 byte to the current format.
 * The difference between the signed and unsigned versions matters only if reading
 * the actual value of the byte.
 *
 * @name Format#int8
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
addCommonMethod('int8', 1, DataView.prototype.getInt8, false);

/**
 * Adds a span of 1 byte to the current format.
 * The difference between the signed and unsigned versions matters only if reading
 * the actual value of the byte.
 *
 * @name Format#uint8
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
addCommonMethod('uint8', 1, DataView.prototype.getUint8, false);

/**
 * Adds a span of 2 bytes to the current format.
 * The difference between the signed, unsigned, little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#int16
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
/**
 * Adds a span of 2 bytes to the current format.
 * The difference between the signed, unsigned, little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#int16le
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
/**
 * Adds a span of 2 bytes to the current format.
 * The difference between the signed, unsigned, little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#int16be
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
/**
 * Adds a span of 2 bytes to the current format.
 * The difference between the signed, unsigned, little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#uint16
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
/**
 * Adds a span of 2 bytes to the current format.
 * The difference between the signed, unsigned, little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#uint16le
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
/**
 * Adds a span of 2 bytes to the current format.
 * The difference between the signed, unsigned, little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#uint16be
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
addCommonMethod('int16', 2, DataView.prototype.getInt16, true );
addCommonMethod('uint16', 2, DataView.prototype.getUint16, true );

/**
 * Adds a span of 4 bytes to the current format.
 * The difference between the signed, unsigned, little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#int32
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
/**
 * Adds a span of 4 bytes to the current format.
 * The difference between the signed, unsigned, little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#int32le
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
/**
 * Adds a span of 4 bytes to the current format.
 * The difference between the signed, unsigned, little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#int32be
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
/**
 * Adds a span of 4 bytes to the current format.
 * The difference between the signed, unsigned, little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#uint32
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
/**
 * Adds a span of 4 bytes to the current format.
 * The difference between the signed, unsigned, little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#uint32le
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
/**
 * Adds a span of 4 bytes to the current format.
 * The difference between the signed, unsigned, little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#uint32be
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
addCommonMethod('int32', 4, DataView.prototype.getInt32, true );
addCommonMethod('uint32', 4, DataView.prototype.getUint32, true );

/**
 * Adds a span of 4 bytes to the current format.
 * The difference between the little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#float32
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
/**
 * Adds a span of 4 bytes to the current format.
 * The difference between the little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#float32le
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
/**
 * Adds a span of 4 bytes to the current format.
 * The difference between the little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#float32be
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
addCommonMethod('float32', 4, DataView.prototype.getFloat32, true );

/**
 * Adds a span of 8 bytes to the current format.
 * The difference between the little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#float64
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
/**
 * Adds a span of 8 bytes to the current format.
 * The difference between the little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#float64le
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
/**
 * Adds a span of 8 bytes to the current format.
 * The difference between the little and big endian
 * versions matters only if reading the actual value of the byte.
 * If no endianess is specified, the default is big endian.
 *
 * @name Format#float64be
 * @function
 * @param {string?} name - Name of the span.
 * @param {string} [color=white] - Color of the span.
 * @param {string} [id] - Name of the variable in which to store the actual value of this byte.
 * @return {Format}
 */
addCommonMethod('float64', 8, DataView.prototype.getFloat64, true );


export { Format as default };
