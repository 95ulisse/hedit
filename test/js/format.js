import Format from 'hedit/format';

// Additional assertion for formats
should.Assertion.add('linearizeTo', function (expected) {
    this.params = { operator: 'to linearize to' };

    this.obj.should.be.instanceOf(Format);

    const g = this.obj.__linearize(
        {
            read(pos, length) {
                return new Int8Array(Array.from({ length }, (_, i) => i)).buffer;
            }
        },
        0,
        '',
        Object.create(null)
    );
    let i = 0;
    for (let curr = g.next(); !curr.done; curr = g.next()) {
        curr.value.should.deepEqual(expected[i]);
        i++;
    }

    expected.should.have.length(i);
});

function span(name, from, to) {
    return { name, from, to, color: 0 };
}



export const EmptyFormatIsValid = () => {
    new Format().should.linearizeTo([]);
};

export const CommonIntegerLengths = () => {

    const lengths = [
        [],
        [ 'int8', 'uint8' ],
        [ 'int16', 'int16le', 'int16be', 'uint16', 'uint16le', 'uint16be' ],
        [],
        [ 'int32', 'int32le', 'int32be', 'uint32', 'uint32le', 'uint32be', 'float32', 'float32le', 'float32be' ],
        [],
        [],
        [],
        [ 'float64', 'float64le', 'float64be' ]
    ];

    lengths.forEach((methods, l) => {
        for (let m of methods) {
            new Format()[m]('Name')
                .should.linearizeTo([ span('Name', 0, l - 1) ]);
        }
    });

};

export const ArrayCanDescribeArbitrarySpanLengths = () => {
    new Format()
        .array('a', 17)

    .should.linearizeTo([
        span('a', 0, 16)
    ]);
};

export const ArrayLengthsCanBeEncapsulatedInFunctions = () => {
    new Format()
        .array('a', () => 17)

    .should.linearizeTo([
        span('a', 0, 16)
    ]);
};

export const ProducedSpansHaveConsecutiveAbsolutePositions = () => {
    new Format()
        .uint8('a')
        .uint16('b')
        .uint32('c')
        .uint8('d')
    
    .should.linearizeTo([
        span('a', 0, 0),
        span('b', 1, 2),
        span('c', 3, 6),
        span('d', 7, 7)
    ]);
};

export const GroupNamesArePrependedToSpanNames = () => {
    new Format()
        .group('Group 1')
            .uint8('A byte')
            .group('Group 2')
                .uint8('Another byte')
            .endgroup()
            .uint8('A final byte')
        .endgroup()
        .uint8('Free')

    .should.linearizeTo([
        span('Group 1 > A byte', 0, 0),
        span('Group 1 > Group 2 > Another byte', 1, 1),
        span('Group 1 > A final byte', 2, 2),
        span('Free', 3, 3)
    ]);
};

export const UnbalancedGroupCallsThrow = () => {
    should.throws(() => {
        new Format().group('1').endgroup().endgroup();
    });

    should.throws(() => {
        new Format().group('1').group('2').endgroup()
            .should.linerizeTo([]); // To force evaluation
    });
};

export const FormatsCanBeNested = () => {
    const c1 = new Format().uint8('C1.1').uint16('C1.2');
    const c2 = new Format().uint8('C2.1').uint16('C2.2');
    
    new Format()
        .uint8('R1')
        .child(c1)
        .uint8('R2')
        .child(c2)
        .uint8('R3')

    .should.linearizeTo([
        span('R1', 0, 0),
        span('C1.1', 1, 1),
        span('C1.2', 2, 3),
        span('R2', 4, 4),
        span('C2.1', 5, 5),
        span('C2.2', 6, 7),
        span('R3', 8, 8)
    ]);
};

export const CanPrependANameToNestedFormats = () => {
    const c = new Format().uint8('1').uint16('2');
    
    new Format()
        .uint8('R1')
        .child('C1', c)
        .uint8('R2')
        .child('C2', c)
        .uint8('R3')

    .should.linearizeTo([
        span('R1', 0, 0),
        span('C1 > 1', 1, 1),
        span('C1 > 2', 2, 3),
        span('R2', 4, 4),
        span('C2 > 1', 5, 5),
        span('C2 > 2', 6, 7),
        span('R3', 8, 8)
    ]);
};

export const NestedFormatsCanBeRepetedWithArray = () => {
    const c = new Format().uint8('1').uint16('2');
    
    // First with a name
    new Format()
        .array('C', 3, c)
    .should.linearizeTo([
        span('C > 1', 0, 0),
        span('C > 2', 1, 2),
        span('C > 1', 3, 3),
        span('C > 2', 4, 5),
        span('C > 1', 6, 6),
        span('C > 2', 7, 8)
    ]);

    // Then without
    new Format()
        .array(null, 3, c)
    .should.linearizeTo([
        span('1', 0, 0),
        span('2', 1, 2),
        span('1', 3, 3),
        span('2', 4, 5),
        span('1', 6, 6),
        span('2', 7, 8)
    ]);

};

export const SequenceRepeatsAChildIndefinitely = () => {
    const c = new Format().uint8('X');
    let f = new Format().sequence(c);

    // Expand manually the first 100 spans
    let g = f.__linearize(null, 0, '', Object.create(null));
    for (let i = 0; i < 100; i++) {
        const curr = g.next();
        curr.done.should.be.false();
        curr.value.should.deepEqual(span('X', i, i));
    }
};

export const ValuesCanBeReadInVariables = () => {
    new Format()
        .uint8('Length', 'white', 'b1')
        .array('Array', vars => {
            vars.b1.should.equal(0);
            return vars.b1;
        })

    .should.linearizeTo([
        span('Length', 0, 0)
    ]);
};

export const EndianessIsHandledWhenReadingVariableValues = () => {
    
    // For both reads the fake file reports the byte sequence 0x0001
    new Format()
        .uint16le('Length LE', 'white', 'b1')
        .array('Array 1', 'b1')
        .uint16be('Length BE', 'white', 'b2')
        .array('Array 2', 'b2')

    .should.linearizeTo([
        span('Length LE', 0, 1),
        span('Array 1', 2, 257 /* 2 + 0x0100 - 1 */),
        span('Length BE', 258, 259),
        span('Array 2', 260, 260)
    ]);

};

export const NestedFormatsHaveTheirOwnVariableScope = () => {
    
    const c1 = new Format()
        .uint32('Child length 1', 'white', 'x') // x = 0x00010203
        .uint32('Child length 2', 'white', 'y') // y = 0x00010203
        .array('Child array', vars => {
            vars.x.should.equal(0x00010203); // Shadows the x in the root format
            vars.y.should.equal(0x00010203);
            return vars.x;
        });

    const c2 = new Format()
        .child(c1)
        .array('Will have length 0', vars => {
            vars.x.should.equal(0) // Inherited from the root format
            should(vars).not.have.property('y'); // y from the child format must not be visible
            return 0;
        });

    new Format()
        .uint8('Parent length', 'white', 'x') // x = 0x00
        .child(c2)
        .array('Will have length 0', vars => {
            vars.x.should.equal(0);
            should(vars).not.have.property('y');
            return vars.x;
        })

    .should.linearizeTo([
        span('Parent length', 0, 0),
        span('Child length 1', 1, 4),
        span('Child length 2', 5, 8),
        span('Child array', 9, 9 + 0x00010203 - 1)
    ]);
};
