#include <stdbool.h>

#include "actions.h"
#include "core.h"
#include "util/map.h"

static void switch_mode(HEdit* hedit, const Arg* arg) {
    hedit_switch_mode(hedit, (enum Modes) arg->i);
}

const Action hedit_actions[] = {
    [HEDIT_ACTION_MODE_NORMAL] = {
        switch_mode,
        { .i = HEDIT_MODE_NORMAL }
    },
    [HEDIT_ACTION_MODE_OVERWRITE] = {
        switch_mode,
        { .i = HEDIT_MODE_OVERWRITE }
    }
};



#define ACTION(name) &hedit_actions[HEDIT_ACTION_##name]

typedef struct {
    const char* key;
    const Action* action;
} KeyBinding;

static const KeyBinding* bindings[] = {

    [HEDIT_MODE_NORMAL] = (const KeyBinding[]){
        { "i",          ACTION(MODE_OVERWRITE) },
        { NULL }
    },

    [HEDIT_MODE_OVERWRITE] = (const KeyBinding[]){
        { "<Escape>",   ACTION(MODE_NORMAL) },
        { NULL }
    }

};

bool hedit_init_actions() {

    // For each mode, register all the default key bindings
    for (int m = HEDIT_MODE_NORMAL; m < HEDIT_MODE_MAX; m++) {
        for (const KeyBinding* binding = &bindings[m][0]; binding->key != NULL; binding++) {
            
            Mode* mode = &hedit_modes[m];

            if (mode->bindings == NULL && (mode->bindings = map_new()) == NULL) {
                return false;
            }

            if (!map_put(mode->bindings, binding->key, binding->action)) {
                return false;
            }

        }
    }

    return true;

}