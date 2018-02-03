#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include <stdbool.h>

#include "core.h"
#include "util/map.h"

#ifdef __cplusplus
extern "C" {
#endif


/** Iterator over the arguments of a command. */
typedef struct ArgIterator ArgIterator;

/** Initializes the map with all the builtin commands. */
bool hedit_init_commands(HEdit*);

/** Registers a new command. */
bool hedit_command_register(HEdit*, const char* name, bool (*cb)(HEdit*, bool, ArgIterator*, void*), void* user);

/**
 * Executes the command specified in the given string.
 * The string might contain additional parameters for the command.
 * 
 * Note: this function will alter the contents of str!
 */
bool hedit_command_exec(HEdit*, char* str);

/**
 * Advances the iterator and returns a pointer to the current argument.
 * If no more arguments are available, NULL is returned.
 */
const char* it_next(ArgIterator*);


#ifdef __cplusplus
}
#endif

#endif