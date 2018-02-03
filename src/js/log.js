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

export default log;