/** Color names to integers map. */
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

export default class Format {
    constructor(proxy) {
        this._proxy = proxy;
        this._segments = [];
    }

    uint8(name, color, id) {
        this._segments.push({
            name,
            color,
            length: 1,
            id,
            read(proxy, off) {
                return new Uint8Array(proxy.read(off, 1))[0];
            }
        });
        return this;
    }

    int8(name, color, id) {
        this._segments.push({
            name,
            color,
            length: 1,
            id,
            read(proxy, off) {
                return new Int8Array(proxy.read(off, 1))[0];
            }
        });
        return this;
    }

    array(name, length, child) {
        const repeat = typeof length === 'string' ? data => 0 + data[length] : length;

        if (child instanceof Format) {

            // A composite child
            this._segments.push({
                child: {
                    *iterator(absoffset, variables) {
                        const n = typeof repeat === 'function' ? repeat(variables) : repeat;
                        let offset = absoffset;
                        for (let i = 0; i < n; i++) {
                            for (let childseg of child.iterator(offset, Object.create(variables))) {
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
                    *iterator(absoffset, variables) {
                        const n = typeof repeat === 'function' ? repeat(variables) : repeat;
                        if (n > 0) {
                            yield {
                                name: name,
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

    sequence(child) {
        return this.array(null, Infinity, child);
    }

    *iterator(absoffset, variables) {
        let offset = absoffset;

        // Iterate over all the segments computing the actual absolute offsets
        for (let seg of this._segments) {
            if (seg.child) {
                for (let childseg of seg.child.iterator(offset, Object.create(variables))) {
                    yield childseg;
                    offset = childseg.to + 1;
                }
            } else if (seg.length > 0) {
                if (seg.id) {
                    variables[seg.id] = seg.read(this._proxy, offset);
                }
                yield {
                    name: seg.name,
                    color: COLORS[seg.color],
                    from: offset,
                    to: offset + seg.length - 1
                };
                offset += seg.length;
            }
        }

    }

    [Symbol.iterator]() {
        return this.iterator(0,  Object.create(null));
    }
};