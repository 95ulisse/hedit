#ifndef __ACTIONS_H__
#define __ACTIONS_H__

#include <stdbool.h>
#include "core.h"
#include "util/common.h"

/** Type of the functions that will be called when an action needs to be performed. */
typedef void (*ActionCallback)(HEdit* hedit, const Value* arg);

/** An action is bound to a key stroke and has no parameters. */
typedef struct {
    ActionCallback cb;
    const Value arg;
} Action;

enum Actions {

    // Mode switch
    HEDIT_ACTION_MODE_NORMAL,
    HEDIT_ACTION_MODE_OVERWRITE,
    HEDIT_ACTION_MODE_COMMAND,

    // Command line editing
    HEDIT_ACTION_COMMAND_MOVE_LEFT,
    HEDIT_ACTION_COMMAND_MOVE_RIGHT,
    HEDIT_ACTION_COMMAND_MOVE_HOME,
    HEDIT_ACTION_COMMAND_MOVE_END,
    HEDIT_ACTION_COMMAND_DEL_LEFT,
    HEDIT_ACTION_COMMAND_DEL_RIGHT,
    HEDIT_ACTION_COMMAND_EXEC

};

extern const Action hedit_actions[];

/** Registers the default key bindings for all the modes in the given instance. */
bool hedit_init_actions();

#endif