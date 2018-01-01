#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <tickit.h>

#include "core.h"
#include "actions.h"
#include "commands.h"
#include "options.h"
#include "statusbar.h"
#include "util/log.h"
#include "util/map.h"
#include "util/buffer.h"


static bool mode_normal_on_enter(HEdit* hedit, Mode* prev) {
    return true;
}

static bool mode_normal_on_exit(HEdit* hedit, Mode* next) {
    return true;
}

static void mode_normal_on_input(HEdit* hedit, const char* key) {
    log_debug("NORMAL input: %s", key);
}

static bool mode_overwrite_on_enter(HEdit* hedit, Mode* prev) {
    return true;
}

static bool mode_overwrite_on_exit(HEdit* hedit, Mode* next) {
    return true;
}

static void mode_overwrite_on_input(HEdit* hedit, const char* key) {
    log_debug("OVERWRITE input: %s", key);
}

static bool mode_command_on_enter(HEdit* hedit, Mode* prev) {

    // Initializes a new buffer for the command line
    Buffer* b = buffer_new();
    if (b == NULL) {
        log_fatal("Cannot allocate new buffer for command line.");
        return false;
    }
    hedit->command_buffer = b;

    return true;
}

static bool mode_command_on_exit(HEdit* hedit, Mode* next) {
    buffer_free(hedit->command_buffer);
    hedit->command_buffer = NULL;
    return true;
}

static void mode_command_on_input(HEdit* hedit, const char* key) {

    // Add the key to the buffer, but ignore any combo key
    if (key[0] != '<') {
        if (!buffer_put_char(hedit->command_buffer, key[0])) {
            log_fatal("Cannot insert char into command line buffer.");
        }
        hedit_statusbar_redraw(hedit->statusbar);
    }

}

Mode hedit_modes[] = {
    
    [HEDIT_MODE_NORMAL] = {
        .id = HEDIT_MODE_NORMAL,
        .name = "NORMAL",
        .display_name = "NORMAL",
        .bindings = NULL,
        .on_enter = mode_normal_on_enter,
        .on_exit = mode_normal_on_exit,
        .on_input = mode_normal_on_input
    },

    [HEDIT_MODE_OVERWRITE] = {
        .id = HEDIT_MODE_OVERWRITE,
        .name = "OVERWRITE",
        .display_name = "OVERWRITE",
        .bindings = NULL,
        .on_enter = mode_overwrite_on_enter,
        .on_exit = mode_overwrite_on_exit,
        .on_input = mode_overwrite_on_input
    },

    [HEDIT_MODE_COMMAND] = {
        .id = HEDIT_MODE_COMMAND,
        .name = "COMMAND",
        .display_name = "NORMAL",
        .bindings = NULL,
        .on_enter = mode_command_on_enter,
        .on_exit = mode_command_on_exit,
        .on_input = mode_command_on_input
    }

};

void hedit_switch_mode(HEdit* hedit, enum Modes m) {
    assert(m >= HEDIT_MODE_NORMAL && m <= HEDIT_MODE_MAX);

    Mode* old = hedit->mode;
    Mode* new = &hedit_modes[m];

    if (old == new) {
        return;
    }

    // Perform the switch and invoke the enter/exit events
    if (old != NULL && old->on_exit != NULL) {
        if (!old->on_exit(hedit, new)) {
            return; // Switch vetoed
        }
    }
    hedit->mode = new;
    if (new->on_enter != NULL) {
        if (!new->on_enter(hedit, old == NULL ? new : old)) {
            hedit->mode = old; // Switch vetoed
            return;
        }
    }

    // Fire the event
    log_debug("Mode switch: %s -> %s", old == NULL ? NULL : old->name, new->name);
    event_fire(&hedit->ev_mode_switch, hedit, new, old);
}


// -----------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------

static Theme* init_default_theme() {

    Theme* theme = malloc(sizeof(Theme));
    if (theme == NULL) {
        log_fatal("Cannot allocate memory for default theme.");
        return NULL;
    }

    // Text is white on black
    theme->text = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 7,
        TICKIT_PEN_BG, 16,
        -1
    );
    
    // Cursor inverts the colors
    theme->cursor = tickit_pen_clone(theme->text);
    tickit_pen_set_bool_attr(theme->cursor, TICKIT_PEN_REVERSE, true);

    // Bold red for errors
    theme->error = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 1,
        TICKIT_PEN_BG, 16,
        TICKIT_PEN_BOLD, true,
        -1
    );

    return theme;

}

static bool free_theme(const char* unused, void* theme, void* unused2) {
    Theme* t = theme;

    tickit_pen_unref(t->text);
    tickit_pen_unref(t->cursor);
    tickit_pen_unref(t->error);
    free(t);

    return true;
}

static int on_keypress(TickitWindow* win, TickitEventFlags flags, void* info, void* user) {

    HEdit* hedit = user;
    TickitKeyEventInfo* e = info;

    // Wrap the key name in <> if it is not a single char
    char key[30];
    if (strlen(e->str) == 1) {
        strncpy(key, e->str, 30);
        key[29] = '\0';
    } else {
        snprintf(key, 30, "<%s>", e->str);
    }

    Action* a = map_get(hedit->mode->bindings, key);
    if (a != NULL) {
        a->cb(hedit, &a->arg);
    } else if (hedit->mode->on_input != NULL) {
        hedit->mode->on_input(hedit, key);
    }

    return 1;

}

HEdit* hedit_core_init(Options* options, Tickit* tickit) {

    // Allocate space for the global state
    HEdit* hedit = calloc(1, sizeof(HEdit));
    if (hedit == NULL) {
        log_fatal("Cannot allocate memory for global state.");
        goto error;
    }
    hedit->options = options;
    hedit->tickit = tickit;
    hedit->rootwin = tickit_get_rootwin(tickit);
    hedit->exit = false;
    event_init(&hedit->ev_load);
    event_init(&hedit->ev_quit);
    event_init(&hedit->ev_mode_switch);
    event_init(&hedit->ev_file_open);
    event_init(&hedit->ev_file_beforewrite);
    event_init(&hedit->ev_file_write);
    event_init(&hedit->ev_file_close);

    // Initialize the default theme
    Theme* default_theme = init_default_theme();
    if (default_theme == NULL) {
        goto error;
    }
    if (!hedit_register_theme(hedit, "default", default_theme)) {
        goto error;
    }
    hedit->theme = default_theme;

    // Register the handler for the keys
    hedit->on_keypress_bind_id = tickit_window_bind_event(hedit->rootwin, TICKIT_WINDOW_ON_KEY, 0, on_keypress, hedit);

    // Initialize statusbar
    if ((hedit->statusbar = hedit_statusbar_init(hedit)) == NULL) {
        goto error;
    }

    // Switch to normal mode
    hedit_switch_mode(hedit, HEDIT_MODE_NORMAL);

    // Exit with success
    return hedit;

error:

    if (hedit != NULL) {
        hedit_statusbar_teardown(hedit->statusbar);
        free(hedit);
    }

    return NULL;

}

void hedit_core_teardown(HEdit* hedit) {
    
    if (hedit == NULL) {
        return;
    }

    log_debug("Core teardown begun.");

    // Terminate the single components
    hedit_statusbar_teardown(hedit->statusbar);

    // Remove event handlers
    tickit_window_unbind_event_id(hedit->rootwin, hedit->on_keypress_bind_id);

    // Clear the buffers
    buffer_free(hedit->command_buffer);
    if (hedit->file != NULL) {
        hedit_file_close(hedit->file);
    }

    // Free and unregister all the themes
    if (hedit->themes != NULL) {
        map_iterate(hedit->themes, free_theme, NULL);
        map_free(hedit->themes);
    }

    // Free the global state
    free(hedit);

}

bool hedit_register_theme(HEdit* hedit, const char* name, Theme* theme) {
    
    if (hedit->themes == NULL && (hedit->themes = map_new()) == NULL) {
        log_fatal("Cannot allocate memory for map: %s.", strerror(errno));
        return false;
    }

    if (!map_put(hedit->themes, name, theme)) {
        log_error("Cannot register theme: %s.", strerror(errno));
        return false;
    }

    log_debug("Theme %s registered.", name);
    return true;

}

void hedit_unregister_theme(HEdit* hedit, const char* name) {

    if (hedit->themes == NULL) {
        return;
    }

    Theme* theme = map_delete(hedit->themes, name);
    if (theme != NULL) {
        free_theme(NULL, theme, NULL);
    }

}

bool hedit_switch_theme(HEdit* hedit, const char* name) {

    if (hedit->themes == NULL) {
        log_error("Cannot find theme %s.", name);
        return false;
    }

    Theme* theme = map_get(hedit->themes, name);
    if (theme == NULL) {
        log_error("Cannot find theme %s.", name);
        return false;
    }
    
    // Update the pointers and perform a full redraw
    hedit->theme = theme;
    tickit_window_expose(hedit->rootwin, NULL);
    log_debug("Theme switched to %s.", name);
    return true;

}