import Format from 'hedit/format';

const keySlot = new Format()
    .uint32be('State', 'green')
    .uint32be('Iterations', 'orange')
    .array('Salt', 32, 'purple')
    .uint32be('Key material offset', 'blue')
    .uint32be('Number of anti-forensic stripes', 'gray');

const luksHeader = new Format()
    .array('Magic', 6, 'red')
    .uint16be('Version', 'blue')
    .array('Cipher name', 32, 'green')
    .array('Cipher mode', 32, 'pink')
    .array('Hash', 32, 'orange')
    .uint32be('Payload offset', 'red')
    .uint32be('Key bytes', 'blue')
    .array('Master key checksum', 20, 'green')
    .array('Master key salt', 32, 'pink')
    .uint32be('Master key checksum iterations', 'orange')
    .array('UUID', 40, 'red');

for (let i = 0; i < 8; i++) {
    luksHeader.child('Key slot #' + i, keySlot);
}

export default luksHeader;
