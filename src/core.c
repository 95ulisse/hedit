#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <tickit.h>

#include "core.h"
#include "actions.h"
#include "options.h"
#include "statusbar.h"
#include "util/log.h"
#include "util/map.h"


static void mode_normal_on_enter(HEdit* hedit, Mode* prev) {
    log_debug("Entering NORMAL mode.");
}

static void mode_normal_on_exit(HEdit* hedit, Mode* next) {
    log_debug("Leaving NORMAL mode.");
}

static void mode_normal_on_input(HEdit* hedit, const char* key) {
    log_debug("NORMAL input: %s", key);
}

static void mode_overwrite_on_enter(HEdit* hedit, Mode* prev) {
    log_debug("Entering OVERWRITE mode.");
}

static void mode_overwrite_on_exit(HEdit* hedit, Mode* next) {
    log_debug("Leaving OVERWRITE mode.");
}

static void mode_overwrite_on_input(HEdit* hedit, const char* key) {
    log_debug("OVERWRITE input: %s", key);
}

Mode hedit_modes[] = {
    
    [HEDIT_MODE_NORMAL] = {
        .id = HEDIT_MODE_NORMAL,
        .name = "Normal",
        .bindings = NULL,
        .on_enter = mode_normal_on_enter,
        .on_exit = mode_normal_on_exit,
        .on_input = mode_normal_on_input
    },

    [HEDIT_MODE_OVERWRITE] = {
        .id = HEDIT_MODE_OVERWRITE,
        .name = "Overwrite",
        .bindings = NULL,
        .on_enter = mode_overwrite_on_enter,
        .on_exit = mode_overwrite_on_exit,
        .on_input = mode_overwrite_on_input
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
        old->on_exit(hedit, new);
    }
    hedit->mode = new;
    if (new->on_enter != NULL) {
        new->on_enter(hedit, old == NULL ? new : old);
    }

    // Fire the event
    event_fire(&hedit->ev_mode_switch, hedit, new, old);
}


// -----------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------


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

HEdit* hedit_core_init(Options* options, TickitWindow* rootwin) {

    // Allocate space for the global state
    HEdit* hedit = calloc(1, sizeof(HEdit));
    if (hedit == NULL) {
        log_fatal("Cannot allocate memory for global state.");
        goto error;
    }
    hedit->options = options;
    hedit->rootwin = rootwin;
    hedit->exit = false;
    event_init(&hedit->ev_mode_switch);

    // Register the handler for the keys
    hedit->on_keypress_bind_id = tickit_window_bind_event(rootwin, TICKIT_WINDOW_ON_KEY, 0, on_keypress, hedit);

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

    // Free the global state
    free(hedit);

}