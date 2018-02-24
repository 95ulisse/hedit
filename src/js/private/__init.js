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
    'string':  null
});

hedit.registerOption('format', '', name => {
    if (!file.isOpen) {
        log.error('No file open.');
        return false;
    }
    format.setFormat(name);
    return true;
});