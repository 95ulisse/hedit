// Test format to check data-dependent formats

import Format from 'hedit/format';

const string = file =>
    new Format(file)
        .uint8('Length', 'red', 'strlen')
        .array('String', 'strlen', 'green');

export default file =>
    new Format(file)
        .sequence(string(file));