#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <tickit.h>

#include "util/log.h"
#include "core.h"
#include "statusbar.h"
#include "util/event.h"

struct Statusbar {
    HEdit* hedit;
    TickitWindow* win;
    int on_resize_bind_id;
    void* on_mode_switch_registration;
};

void command_visitor(Buffer* buf, size_t pos, const char* str, size_t len, void* user) {

    TickitRenderBuffer* rb = user;
    size_t totallen = buffer_get_len(buf);
    size_t cursorpos = buffer_get_cursor(buf) - pos;

    // The char on which the cursor is on, needs a different background
    if (cursorpos < 0 || cursorpos >= len) {
        tickit_renderbuffer_textn(rb, str, len);
    } else {

        if (cursorpos > 0) {
            tickit_renderbuffer_textn(rb, str, cursorpos - 1);
        }

        TickitPen* pen = tickit_pen_new_attrs(
            TICKIT_PEN_FG, 0,
            TICKIT_PEN_BG, 7,
            -1
        );
        tickit_renderbuffer_savepen(rb);
        tickit_renderbuffer_setpen(rb, pen);
        tickit_renderbuffer_textn(rb, str + cursorpos, 1);
        tickit_renderbuffer_restore(rb);
        tickit_pen_unref(pen);

        if (cursorpos < len - 1) {
            tickit_renderbuffer_textn(rb, str + cursorpos + 1, len - cursorpos - 1);
        }

    }

}

static int on_expose(TickitWindow* win, TickitEventFlags flags, void* info, void* user) {

    Statusbar* statusbar = user;
    TickitExposeEventInfo* e = info;

    // Clear
    tickit_renderbuffer_eraserect(e->rb, &e->rect);

    // Draw the status line
    tickit_renderbuffer_textf_at(e->rb, 0, 0, "-- %s --", statusbar->hedit->mode->display_name);

    // Draw the command line to reflect the current buffer contents
    Buffer* buf = statusbar->hedit->command_buffer;
    if (buf != NULL) {
        
        tickit_renderbuffer_goto(e->rb, 1, 0);
        tickit_renderbuffer_text(e->rb, ":");
        buffer_visit(buf, command_visitor, e->rb);

        // As a special case, if the cursor is right at the end of the string,
        // add a fake space just to show the cursor
        if (buffer_get_cursor(buf) == buffer_get_len(buf)) {
            TickitPen* pen = tickit_pen_new_attrs(
                TICKIT_PEN_FG, 0,
                TICKIT_PEN_BG, 7,
                -1
            );
            tickit_renderbuffer_savepen(e->rb);
            tickit_renderbuffer_setpen(e->rb, pen);
            tickit_renderbuffer_text(e->rb, " ");
            tickit_renderbuffer_restore(e->rb);
            tickit_pen_unref(pen);
        }
    }

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

static void on_mode_switch(void* user, HEdit* hedit, Mode* new, Mode* old) {
    Statusbar* statusbar = user;

    // Force a redraw of the statusbar window
    tickit_window_expose(statusbar->win, NULL);
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
    statusbar->on_mode_switch_registration = event_add(&hedit->ev_mode_switch, on_mode_switch, statusbar);

    return statusbar;
}

void hedit_statusbar_teardown(Statusbar* statusbar) {
    
    if (statusbar == NULL) {
        return;
    }

    // Detach handlers
    tickit_window_unbind_event_id(statusbar->hedit->rootwin, statusbar->on_resize_bind_id);
    event_del(&statusbar->hedit->ev_mode_switch, statusbar->on_mode_switch_registration);

    // Destroy the window and free the memory
    tickit_window_close(statusbar->win);
    tickit_window_destroy(statusbar->win);
    free(statusbar);

}

void hedit_statusbar_redraw(Statusbar* statusbar) {
    tickit_window_expose(statusbar->win, NULL);
}