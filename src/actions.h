#ifndef __ACTIONS_H__
#define __ACTIONS_H__

#include <stdbool.h>
#include "core.h"

enum Actions {
    HEDIT_ACTION_MODE_NORMAL,
    HEDIT_ACTION_MODE_OVERWRITE
};

extern const Action hedit_actions[];

/** Registers the default key bindings for all the modes in the given instance. */
bool hedit_init_actions();

#endif