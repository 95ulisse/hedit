#include <stdlib.h>
#include <tickit.h>

#include "core.h"
#include "log.h"
#include "options.h"
#include "statusbar.h"

HEdit* hedit_core_init(Options* options, TickitTerm* term) {

    // Allocate space for the global state
    HEdit* hedit = calloc(1, sizeof(HEdit));
    if (hedit == NULL) {
        log_fatal("Cannot allocate memory for global state.");
        goto error;
    }
    hedit->options = options;
    hedit->term = term;
    hedit->rootwin;
    hedit->exit = false;

    // Attach the handlers for the terminal events
    hedit->on_resize_ev_id = tickit_term_bind_event(term, TICKIT_EV_RESIZE, hedit_core_on_resize, hedit);

    // Initialize statusbar
    if ((hedit->statusbar = hedit_statusbar_init(hedit)) == NULL) {
        goto error;
    }

    // Exit with success
    return hedit;

error:

    if (hedit != NULL) {
        hedit_statusbar_teardown(hedit->statusbar);
        free(hedit);
    }

    return NULL;

}

static void hedit_core_on_resize(TickitTerm* tt, TickitEventType ev, TickitEvent* args, void* data) {

    if (ev != TICKIT_EV_RESIZE) {
        return;
    }

    log_debug("Terminal resize. New dimensions: %d rows, %d cols.", args->lines, args->cols);

    // Forward the event to the single components
    HEdit* hedit = data;
    hedit_statusbar_on_resize(hedit->statusbar, args->lines, args->cols);
}

static bool hedit_core_process_key(HEdit* hedit, int key, int* exitcode) {

    // The Big Key Switch
    switch (key) {

        case KEY_RESIZE:
            hedit_core_on_resize(state, LINES, COLS);
            break;
        
        case 'q':
            *exitcode = 0;
            return false;

        default:
            log_info("Unknown key %d.", key);
            break;

    }

    return true;
}

void hedit_core_teardown(HEdit* hedit) {
    
    if (hedit == NULL) {
        return;
    }

    log_debug("Core teardown begun.");

    // Terminate the single components
    hedit_statusbar_teardown(hedit->statusbar);

    // Free the global state
    free(hedit);

}