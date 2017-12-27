#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <tickit.h>

#include "log.h"
#include "core.h"
#include "statusbar.h"

struct Statusbar {
    HEdit* hedit;
    TickitWindow* win;
    int on_resize_bind_id;
};

static int on_expose(TickitWindow* win, TickitEventFlags flags, void* info, void* user) {

    Statusbar* statusbar = user;
    TickitExposeEventInfo* e = info;

    // Clear
    tickit_renderbuffer_eraserect(e->rb, &e->rect);

    // Redraw the two lines of text
    tickit_renderbuffer_text_at(e->rb, 0, 0, statusbar->hedit->mode->name);
    tickit_renderbuffer_text_at(e->rb, 1, 0, "");

    return 1;

}

static int on_resize(TickitWindow* win, TickitEventFlags flags, void* info, void* user) {

    Statusbar* statusbar = user;

    // Statusbar should always be at the bottom of the window
    TickitWindow* parent = tickit_window_parent(statusbar->win);
    TickitRect parentgeom = tickit_window_get_geometry(parent);
    tickit_window_set_geometry(statusbar->win, (TickitRect) {
        .top = parentgeom.lines - 2,
        .left = 0,
        .lines = 2,
        .cols = parentgeom.cols
    });

    // Force a repaint
    tickit_window_expose(statusbar->win, NULL);

    return 1;
    
}

Statusbar* hedit_statusbar_init(HEdit* hedit) {

    // Allocate space
    Statusbar* statusbar = calloc(1, sizeof(Statusbar));
    if (statusbar == NULL) {
        log_fatal("Cannot allocate memory for statusbar.");
        return NULL;
    }
    statusbar->hedit = hedit;

    // Create a new window for the statusbar
    TickitRect rootgeom = tickit_window_get_geometry(hedit->rootwin);
    statusbar->win = tickit_window_new(hedit->rootwin, (TickitRect){
        .top = rootgeom.lines - 2,
        .left = 0,
        .lines = 2,
        .cols = rootgeom.cols
    }, 0);
    if (statusbar->win == NULL) {
        log_fatal("Cannot create new window for statusbar: %s.", strerror(errno));
        free(statusbar);
        return NULL;
    }

    // Register the event handlers
    tickit_window_bind_event(statusbar->win, TICKIT_WINDOW_ON_EXPOSE, 0, on_expose, statusbar);
    statusbar->on_resize_bind_id = tickit_window_bind_event(hedit->rootwin, TICKIT_WINDOW_ON_GEOMCHANGE, 0, on_resize, statusbar);

    return statusbar;
}

void hedit_statusbar_teardown(Statusbar* statusbar) {
    
    if (statusbar == NULL) {
        return;
    }

    // Detach handlers
    tickit_window_unbind_event_id(statusbar->hedit->rootwin, statusbar->on_resize_bind_id);

    // Destroy the window and free the memory
    tickit_window_close(statusbar->win);
    tickit_window_destroy(statusbar->win);
    free(statusbar);

}