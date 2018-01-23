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
    HEDIT_ACTION_MODE_NORMAL = 1,
    HEDIT_ACTION_MODE_INSERT,
    HEDIT_ACTION_MODE_REPLACE,
    HEDIT_ACTION_MODE_COMMAND,

    // Cursor movement
    HEDIT_ACTION_MOVEMENT_LEFT,
    HEDIT_ACTION_MOVEMENT_RIGHT,
    HEDIT_ACTION_MOVEMENT_UP,
    HEDIT_ACTION_MOVEMENT_DOWN,
    HEDIT_ACTION_MOVEMENT_LINE_START,
    HEDIT_ACTION_MOVEMENT_LINE_END,
    HEDIT_ACTION_MOVEMENT_PAGE_UP,
    HEDIT_ACTION_MOVEMENT_PAGE_DOWN,

    // Editing commands
    HEDIT_ACTION_UNDO,
    HEDIT_ACTION_REDO,
    HEDIT_ACTION_DELETE_LEFT,
    HEDIT_ACTION_DELETE_RIGHT,

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