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

const LINEARIZE = Symbol('linearize');

// Helper function to join two optional strings
function join(a, b) {
    if (a && b) {
        return a + ' > ' + b;
    } else {
        return a ? a : b;
    }
}

export default class Format {
    constructor(proxy) {
        this._proxy = proxy;
        this._segments = [];
    }

    array(name, length, child = 'white') {
        const repeat = typeof length === 'string' ? data => 0 + data[length] : length;

        if (child instanceof Format) {

            // A composite child
            this._segments.push({
                child: {
                    *[LINEARIZE](absoffset, basename, variables) {
                        const n = typeof repeat === 'function' ? repeat(variables) : repeat;
                        let offset = absoffset;
                        for (let i = 0; i < n; i++) {
                            for (let childseg of child[LINEARIZE](offset, join(basename, name), Object.create(variables))) {
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
                    *[LINEARIZE](absoffset, basename, variables) {
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

    child(name, c) {
        if (typeof c === 'undefined') {
            c = name;
            name = null;
        }
        return this.array(name, 1, c);
    }

    sequence(name, c) {
        if (typeof c === 'undefined') {
            c = name;
            name = null;
        }
        return this.array(name, Infinity, c);
    }

    group(name) {
        const childFormat = new Format();
        childFormat._parent = this;
        this.child(name, childFormat);
        return childFormat;
    }

    endgroup() {
        const parent = this._parent;
        if (!parent) {
            throw new Error('Unbalanced group()/endgroup() calls.');
        }
        delete this._parent;
        return parent;
    }

    *[LINEARIZE](absoffset, basename, variables) {

        if (this._parent) {
            throw new Error('Unbalanced group()/endgroup() calls.');
        }

        let offset = absoffset;

        // Iterate over all the segments computing the actual absolute offsets
        for (let seg of this._segments) {
            if (seg.child) {
                for (let childseg of seg.child[LINEARIZE](offset, join(basename, seg.name), Object.create(variables))) {
                    yield childseg;
                    offset = childseg.to + 1;
                }
            } else if (seg.length > 0) {
                if (seg.id) {
                    variables[seg.id] = seg.read(this._proxy, offset);
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

    [Symbol.iterator]() {
        return this[LINEARIZE](0, '', Object.create(null));
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

addCommonMethod('int8',     1,  DataView.prototype.getInt8,     false);
addCommonMethod('uint8',    1,  DataView.prototype.getUint8,    false);
addCommonMethod('int16',    2,  DataView.prototype.getInt16,    true );
addCommonMethod('uint16',   2,  DataView.prototype.getUint16,   true );
addCommonMethod('int32',    4,  DataView.prototype.getInt32,    true );
addCommonMethod('uint32',   4,  DataView.prototype.getUint32,   true );
addCommonMethod('float32',  4,  DataView.prototype.getFloat32,  true );
addCommonMethod('float64',  8,  DataView.prototype.getFloat64,  true );
