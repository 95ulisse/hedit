// This file is the only one directly evaluated at startup.
// The other files are modules that are executed only when required.

import hedit from 'hedit';
import file from 'hedit/file';
import log from 'hedit/log';
import format from 'hedit/private/format';

__hedit.registerFormatGuessFunction(format.guessFormat);

format.registerBuiltinFormat({
    'none':    null,
    'mifare':  { extension: 'mfd' },
    'luks':    { magic: new Uint8Array([ 0x4c, 0x55, 0x4b, 0x53, 0xba, 0xbe ]) },
    'string':  { magic: new Uint8Array([ 0x0a ]) }
});

hedit.registerOption('format', '', name => {
    if (!file.isOpen) {
        log.error('No file open.');
        return false;
    }
    format.setFormat(name);
    return true;
});