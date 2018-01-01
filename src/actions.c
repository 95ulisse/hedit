#include <stdbool.h>

#include "core.h"
#include "actions.h"
#include "commands.h"
#include "statusbar.h"
#include "util/log.h"
#include "util/map.h"
#include "util/buffer.h"

static void switch_mode(HEdit* hedit, const Arg* arg) {
    hedit_switch_mode(hedit, (enum Modes) arg->i);
}

static void command_move(HEdit* hedit, const Arg* arg) {
    if (arg->b) {
        buffer_set_cursor(hedit->command_buffer, arg->i < 0 ? 0 : buffer_get_len(hedit->command_buffer));
    } else {
        buffer_move_cursor(hedit->command_buffer, arg->i);
    }
    hedit_statusbar_redraw(hedit->statusbar);
}

static void command_del(HEdit* hedit, const Arg* arg) {
    buffer_del(hedit->command_buffer, arg->i);
    hedit_statusbar_redraw(hedit->statusbar);
}

static void command_exec(HEdit* hedit, const Arg* arg) {

    if (buffer_get_len(hedit->command_buffer) > 0) {

        // Copy the buffer to a string and execute it
        char command[buffer_get_len(hedit->command_buffer) + 1];
        buffer_copy_to(hedit->command_buffer, command);
        switch_mode(hedit, &(const Arg){ .i = HEDIT_MODE_NORMAL });
        hedit_command_exec(hedit, command);

    } else {
        switch_mode(hedit, &(const Arg){ .i = HEDIT_MODE_NORMAL });        
    }

}

const Action hedit_actions[] = {

    // Mode switch
    [HEDIT_ACTION_MODE_NORMAL] = {
        switch_mode,
        { .i = HEDIT_MODE_NORMAL }
    },
    [HEDIT_ACTION_MODE_OVERWRITE] = {
        switch_mode,
        { .i = HEDIT_MODE_OVERWRITE }
    },
    [HEDIT_ACTION_MODE_COMMAND] = {
        switch_mode,
        { .i = HEDIT_MODE_COMMAND }
    },

    // Command line editing
    [HEDIT_ACTION_COMMAND_MOVE_LEFT] = {
        command_move,
        { .i = -1 }
    },
    [HEDIT_ACTION_COMMAND_MOVE_RIGHT] = {
        command_move,
        { .i = 1 }
    },
    [HEDIT_ACTION_COMMAND_MOVE_HOME] = {
        command_move,
        { .i = -1, .b = true }
    },
    [HEDIT_ACTION_COMMAND_MOVE_END] = {
        command_move,
        { .i = 1, .b = true }
    },
    [HEDIT_ACTION_COMMAND_DEL_LEFT] = {
        command_del,
        { .i = 1 }
    },
    [HEDIT_ACTION_COMMAND_DEL_RIGHT] = {
        command_del,
        { .i = -1 }
    },
    [HEDIT_ACTION_COMMAND_EXEC] = {
        command_exec
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
        { ":",          ACTION(MODE_COMMAND)   },
        { NULL }
    },

    [HEDIT_MODE_OVERWRITE] = (const KeyBinding[]){
        { "<Escape>",   ACTION(MODE_NORMAL) },
        { NULL }
    },

    [HEDIT_MODE_COMMAND] = (const KeyBinding[]){
        { "<Escape>",        ACTION(MODE_NORMAL)        },
        { "<Left>",          ACTION(COMMAND_MOVE_LEFT)  },
        { "<Right>",         ACTION(COMMAND_MOVE_RIGHT) },
        { "<Home>",          ACTION(COMMAND_MOVE_HOME)  },
        { "<End>",           ACTION(COMMAND_MOVE_END)   },
        { "<Backspace>",     ACTION(COMMAND_DEL_LEFT)   },
        { "<Delete>",        ACTION(COMMAND_DEL_RIGHT)  },
        { "<Enter>",         ACTION(COMMAND_EXEC)       },
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