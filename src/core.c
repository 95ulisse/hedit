#include <stdlib.h>
#include <tickit.h>

#include "core.h"
#include "log.h"
#include "options.h"
#include "statusbar.h"

static int on_keypress(TickitWindow* win, TickitEventFlags flags, void* info, void* user) {

    TickitKeyEventInfo* e = info;

    log_info(e->str);

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

    // Register the handler for the keys
    hedit->on_keypress_bind_id = tickit_window_bind_event(rootwin, TICKIT_WINDOW_ON_KEY, 0, on_keypress, hedit);

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