#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include <stdbool.h>
#include "util/map.h"

/** Initializes the map with all the builtin commands. */
bool hedit_init_commands();

/**
 * Executes the command specified in the given string.
 * The string might contain additional parameters for the command.
 * 
 * Note: this function will alter the contents of str!
 */
bool hedit_command_exec(HEdit*, char* str);

#endif