// Test format to check data-dependent formats

import Format from 'hedit/format';

const string =
    new Format()
        .uint8('Length', 'red', 'strlen')
        .array('String', 'strlen', 'green');

export default
    new Format()
        .sequence(string);