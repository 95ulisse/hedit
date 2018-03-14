#include <stdbool.h>

#include "core.h"
#include "actions.h"
#include "commands.h"
#include "statusbar.h"
#include "util/log.h"
#include "util/map.h"
#include "util/buffer.h"

static void switch_mode(HEdit* hedit, const Value* arg) {
    hedit_switch_mode(hedit, (enum Modes) arg->i);
}

static void movement(HEdit* hedit, const Value* arg) {
    if (hedit->view->on_movement != NULL) {
        hedit->view->on_movement(hedit, (enum Movement) arg->i, 0);
    }
}

static void undo(HEdit* hedit, const Value* arg) {
    if (hedit->file != NULL) {
        size_t pos;
        if (hedit_file_undo(hedit->file, &pos)) {
            if (hedit->view->on_movement != NULL) {
                hedit->view->on_movement(hedit, HEDIT_MOVEMENT_ABSOLUTE, pos);
            }
            hedit_redraw_view(hedit);
        }
    }
}

static void redo(HEdit* hedit, const Value* arg) {
    if (hedit->file != NULL) {
        size_t pos;
        if (hedit_file_redo(hedit->file, &pos)) {
            if (hedit->view->on_movement != NULL) {
                hedit->view->on_movement(hedit, HEDIT_MOVEMENT_ABSOLUTE, pos);
            }
            hedit_redraw_view(hedit);
        }
    }
}

static void delete(HEdit* hedit, const Value* arg) {
    if (hedit->view->on_delete != NULL) {
        hedit->view->on_delete(hedit, (ssize_t) arg->i);
    }
}

static void command_move(HEdit* hedit, const Value* arg) {
    if (arg->b) {
        buffer_set_cursor(hedit->command_buffer, arg->i < 0 ? 0 : buffer_get_len(hedit->command_buffer));
    } else {
        buffer_move_cursor(hedit->command_buffer, arg->i);
    }
    hedit_statusbar_redraw(hedit->statusbar);
}

static void command_del(HEdit* hedit, const Value* arg) {
    buffer_del(hedit->command_buffer, arg->i);
    hedit_statusbar_redraw(hedit->statusbar);
}

static void command_exec(HEdit* hedit, const Value* arg) {

    if (buffer_get_len(hedit->command_buffer) > 0) {

        // Copy the buffer to a string and execute it
        char command[buffer_get_len(hedit->command_buffer) + 1];
        buffer_copy_to(hedit->command_buffer, command);
        switch_mode(hedit, &(const Value){ .i = HEDIT_MODE_NORMAL });
        hedit_command_exec(hedit, command);

    } else {
        switch_mode(hedit, &(const Value){ .i = HEDIT_MODE_NORMAL });        
    }

}

static void clear_error(HEdit* hedit, const Value* arg) {
    hedit_statusbar_show_message(hedit->statusbar, false, NULL);
}

const Action hedit_actions[] = {

    // Mode switch
    [HEDIT_ACTION_MODE_NORMAL] = {
        switch_mode,
        { .i = HEDIT_MODE_NORMAL }
    },
    [HEDIT_ACTION_MODE_INSERT] = {
        switch_mode,
        { .i = HEDIT_MODE_INSERT }
    },
    [HEDIT_ACTION_MODE_REPLACE] = {
        switch_mode,
        { .i = HEDIT_MODE_REPLACE }
    },
    [HEDIT_ACTION_MODE_COMMAND] = {
        switch_mode,
        { .i = HEDIT_MODE_COMMAND }
    },

    // Cursor movement
    [HEDIT_ACTION_MOVEMENT_LEFT] = {
        movement,
        { .i = HEDIT_MOVEMENT_LEFT }
    },
    [HEDIT_ACTION_MOVEMENT_RIGHT] = {
        movement,
        { .i = HEDIT_MOVEMENT_RIGHT }
    },
    [HEDIT_ACTION_MOVEMENT_UP] = {
        movement,
        { .i = HEDIT_MOVEMENT_UP }
    },
    [HEDIT_ACTION_MOVEMENT_DOWN] = {
        movement,
        { .i = HEDIT_MOVEMENT_DOWN }
    },
    [HEDIT_ACTION_MOVEMENT_LINE_START] = {
        movement,
        { .i = HEDIT_MOVEMENT_LINE_START }
    },
    [HEDIT_ACTION_MOVEMENT_LINE_END] = {
        movement,
        { .i = HEDIT_MOVEMENT_LINE_END }
    },
    [HEDIT_ACTION_MOVEMENT_PAGE_UP] = {
        movement,
        { .i = HEDIT_MOVEMENT_PAGE_UP }
    },
    [HEDIT_ACTION_MOVEMENT_PAGE_DOWN] = {
        movement,
        { .i = HEDIT_MOVEMENT_PAGE_DOWN }
    },

    // Editing commands
    [HEDIT_ACTION_UNDO] = {
        undo
    },
    [HEDIT_ACTION_REDO] = {
        redo
    },
    [HEDIT_ACTION_DELETE_LEFT] = {
        delete,
        { .i = 1 }
    },
    [HEDIT_ACTION_DELETE_RIGHT] = {
        delete,
        { .i = -1 }
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
    },

    // Misc
    [HEDIT_ACTION_CLEAR_ERROR] = {
        clear_error
    }

};



#define ACTION(name) &hedit_actions[HEDIT_ACTION_##name]

typedef struct {
    const char* key;
    const Action* action;
} KeyBinding;

static const KeyBinding* bindings[] = {

    [HEDIT_MODE_NORMAL] = (const KeyBinding[]){
        { "<Escape>",        ACTION(CLEAR_ERROR)         },
        { "i",               ACTION(MODE_INSERT)         },
        { "R",               ACTION(MODE_REPLACE)        },
        { ":",               ACTION(MODE_COMMAND)        },
        { "u",               ACTION(UNDO)                },
        { "<C-r>",           ACTION(REDO)                },
        { "h",               ACTION(MOVEMENT_LEFT)       },
        { "j",               ACTION(MOVEMENT_DOWN)       },
        { "k",               ACTION(MOVEMENT_UP)         },
        { "l",               ACTION(MOVEMENT_RIGHT)      },
        { "<Left>",          ACTION(MOVEMENT_LEFT)       },
        { "<Right>",         ACTION(MOVEMENT_RIGHT)      },
        { "<Up>",            ACTION(MOVEMENT_UP)         },
        { "<Down>",          ACTION(MOVEMENT_DOWN)       },
        { "<Home>",          ACTION(MOVEMENT_LINE_START) },
        { "<End>",           ACTION(MOVEMENT_LINE_END)   },
        { "<PageUp>",        ACTION(MOVEMENT_PAGE_UP)    },
        { "<PageDown>",      ACTION(MOVEMENT_PAGE_DOWN)  },
        { NULL }
    },

    [HEDIT_MODE_INSERT] = (const KeyBinding[]){
        { "<Escape>",        ACTION(MODE_NORMAL)         },
        { "<Backspace>",     ACTION(DELETE_LEFT)         },
        { "<Delete>",        ACTION(DELETE_RIGHT)        },
        { "<Left>",          ACTION(MOVEMENT_LEFT)       },
        { "<Right>",         ACTION(MOVEMENT_RIGHT)      },
        { "<Up>",            ACTION(MOVEMENT_UP)         },
        { "<Down>",          ACTION(MOVEMENT_DOWN)       },
        { "<Home>",          ACTION(MOVEMENT_LINE_START) },
        { "<End>",           ACTION(MOVEMENT_LINE_END)   },
        { "<PageUp>",        ACTION(MOVEMENT_PAGE_UP)    },
        { "<PageDown>",      ACTION(MOVEMENT_PAGE_DOWN)  },
        { NULL }
    },

    [HEDIT_MODE_REPLACE] = (const KeyBinding[]){
        { "<Escape>",        ACTION(MODE_NORMAL)         },
        { "<Left>",          ACTION(MOVEMENT_LEFT)       },
        { "<Right>",         ACTION(MOVEMENT_RIGHT)      },
        { "<Up>",            ACTION(MOVEMENT_UP)         },
        { "<Down>",          ACTION(MOVEMENT_DOWN)       },
        { "<Home>",          ACTION(MOVEMENT_LINE_START) },
        { "<End>",           ACTION(MOVEMENT_LINE_END)   },
        { "<PageUp>",        ACTION(MOVEMENT_PAGE_UP)    },
        { "<PageDown>",      ACTION(MOVEMENT_PAGE_DOWN)  },
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