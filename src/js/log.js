/**
 * Functions to write to the HEdit log.
 *
 * To view the log, you can use the `:log` command,
 * or alternatively you can start HEdit with the following arguments
 * to make it write the logs to the given external file descriptor:
 *
 * ```
 * hedit --debug-fd 3 --debug-colors
 * ```
 *
 * @module hedit/log
 */

function getStack() {
    let origPrepareStackTrace = Error.prepareStackTrace;
    Error.prepareStackTrace = (_, stack) => stack;
    let err = new Error();
    let stack = err.stack;
    Error.prepareStackTrace = origPrepareStackTrace;
    stack.shift();
    return stack;
}

const severities = [ 'debug', 'info', 'warn', 'error', 'fatal' ];
let log = {};

for (let i in severities) {
    log[severities[i]] = function logger() {

        // Capture the stack trace to retrive the location of the caller
        const frame = getStack()[1];

        // Forward to the native implementation
        let filename;
        let lineno;
        if (frame && frame.getFileName()) {
            filename = '' + frame.getFileName();
            filename = filename.substring(filename.lastIndexOf('/') + 1);
            lineno = 0 + frame.getLineNumber();
        } else {
            filename = "<unknown>";
            lineno = 0;
        }
        __hedit.log(filename, lineno, i, [].join.call(arguments, ' '));
        
    };
}

/**
 * @name debug
 * @function
 * @description Emits a message to the log with DEBUG severity.
 * @param {...*} args - Values to output to the log.
 */

/**
 * @name info
 * @function
 * @description Emits a message to the log with INFO severity.
 * @param {...*} args - Values to output to the log.
 */

/**
 * @name warn
 * @function
 * @description Emits a message to the log with WARN severity.
 * @param {...*} args - Values to output to the log.
 */

/**
 * @name error
 * @function
 * @description Emits a message to the log with ERROR severity.
 *              Messages written with this verity will be shown in the status bar.
 * @param {...*} args - Values to output to the log.
 */

/**
 * @name fatal
 * @function
 * @description Emits a message to the log with FATAL severity.
 *              Messages written with this verity will be shown in the status bar.
 * @param {...*} args - Values to output to the log.
 */

export default log;