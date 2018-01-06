#ifndef __OPTIONS_H__
#define __OPTIONS_H__

#include <stdbool.h>

/** Structure containing all the HEdit command line options. */
typedef struct {
    bool show_help;
    bool show_version;
    const char* file;
} Options;

/**
 *    Parses the given raw arguments into a more usable `Options`.
 *
 *    @param  argc Number of arguments.
 *    @param  argv Array of `char*` containing the arguments.
 *    @return `true` if the parsing succeeded and the program should continue,
 *            `false` otherwise.
 */
bool options_parse(Options* options, int argc, char** argv);

#endif
