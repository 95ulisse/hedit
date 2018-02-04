#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <tickit.h>

#include "core.h"
#include "statusbar.h"
#include "util/log.h"
#include "util/event.h"

#define LAST_ERROR_SIZE 256

struct Statusbar {
    HEdit* hedit;
    TickitWindow* win;
    char last_error[LAST_ERROR_SIZE];
    bool show_last_error;

    // Registrations
    int on_resize_bind_id;
    void* on_mode_switch_registration;
    void* on_file_open_registration;
    void* on_file_write_registration;
    void* on_file_close_registration;
    void* log_sink_registration;
};

struct command_visitor_params {
    TickitRenderBuffer* rb;
    Theme* theme;
};

void command_visitor(Buffer* buf, size_t pos, const char* str, size_t len, void* user) {

    struct command_visitor_params* params = user;
    TickitRenderBuffer* rb = params->rb;
    Theme* theme = params->theme;

    size_t cursorpos = buffer_get_cursor(buf) - pos;

    // The char on which the cursor is on, needs a different background
    if (cursorpos < 0 || cursorpos >= len) {
        tickit_renderbuffer_textn(rb, str, len);
    } else {

        if (cursorpos > 0) {
            tickit_renderbuffer_textn(rb, str, cursorpos - 1);
        }

        tickit_renderbuffer_savepen(rb);
        tickit_renderbuffer_setpen(rb, theme->block_cursor);
        tickit_renderbuffer_textn(rb, str + cursorpos, 1);
        tickit_renderbuffer_restore(rb);

        if (cursorpos < len - 1) {
            tickit_renderbuffer_textn(rb, str + cursorpos + 1, len - cursorpos - 1);
        }

    }

}

static int on_expose(TickitWindow* win, TickitEventFlags flags, void* info, void* user) {

    Statusbar* statusbar = user;
    TickitExposeEventInfo* e = info;

    // Clear first line
    tickit_renderbuffer_setpen(e->rb, statusbar->hedit->theme->statusbar);
    tickit_renderbuffer_eraserect(e->rb, &(TickitRect){ .top = 0, .left = 0, .lines = 1, .cols = tickit_window_cols(win) });

    // Open file info on the right
    if (statusbar->hedit->file != NULL) {
        File* f = statusbar->hedit->file;
        size_t len = strlen(hedit_file_name(f)) + (hedit_file_is_ro(f) ? 5 : 0);
        tickit_renderbuffer_goto(e->rb, 0, tickit_window_cols(win) - len);
        tickit_renderbuffer_text(e->rb, hedit_file_name(f));
        if (hedit_file_is_ro(f)) {
            tickit_renderbuffer_text(e->rb, " [ro]");
        }
    }

    // Current mode on the left
    tickit_renderbuffer_textf_at(e->rb, 0, 0, " -- %s --", statusbar->hedit->mode->display_name);

    // Clear second line
    tickit_renderbuffer_setpen(e->rb, statusbar->hedit->theme->commandbar);
    tickit_renderbuffer_eraserect(e->rb, &(TickitRect){ .top = 1, .left = 0, .lines = 1, .cols = tickit_window_cols(win) });

    tickit_renderbuffer_goto(e->rb, 1, 0);

    // Write the error if present.
    // It should neve happen that we have to show an error while the user is typing a command.
    if (statusbar->show_last_error) {
        tickit_renderbuffer_setpen(e->rb, statusbar->hedit->theme->error);
        tickit_renderbuffer_text(e->rb, statusbar->last_error);
        return 1;
    }

    // Draw the command line to reflect the current buffer contents
    Buffer* buf = statusbar->hedit->command_buffer;
    if (buf != NULL) {

        tickit_renderbuffer_text(e->rb, ":");

        struct command_visitor_params params = {
            .rb = e->rb,
            .theme = statusbar->hedit->theme
        };
        buffer_visit(buf, command_visitor, &params);

        // As a special case, if the cursor is right at the end of the string,
        // add a fake space just to show the cursor
        if (buffer_get_cursor(buf) == buffer_get_len(buf)) {
            tickit_renderbuffer_savepen(e->rb);
            tickit_renderbuffer_setpen(e->rb, statusbar->hedit->theme->block_cursor);
            tickit_renderbuffer_text(e->rb, " ");
            tickit_renderbuffer_restore(e->rb);
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

    // Hide the last error
    statusbar->show_last_error = false;

    // Force a redraw of the statusbar window
    tickit_window_expose(statusbar->win, NULL);
}

static void on_file_event(void* user, HEdit* hedit, File* file) {
    Statusbar* statusbar = user;
    tickit_window_expose(statusbar->win, NULL);
}

static void on_log_message(void* user, struct log_config* config, const char* file, int line, log_severity severity, const char* format, va_list args) {

    // We want to show messages of severity error and fatal on the statusbar
    if (severity < LOG_ERROR) {
        return;
    }

    Statusbar* statusbar = user;

    // Print the message to the internal buffer
    vsnprintf(statusbar->last_error, LAST_ERROR_SIZE, format, args);
    statusbar->last_error[LAST_ERROR_SIZE - 1] = '\0';
    statusbar->show_last_error = true;

    // Force a redraw
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
    statusbar->show_last_error = false;

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
    statusbar->on_file_open_registration = event_add(&hedit->ev_file_open, on_file_event, statusbar);
    statusbar->on_file_write_registration = event_add(&hedit->ev_file_write, on_file_event, statusbar);
    statusbar->on_file_close_registration = event_add(&hedit->ev_file_close, on_file_event, statusbar);
    statusbar->log_sink_registration = log_register_sink(on_log_message, statusbar);

    return statusbar;
}

void hedit_statusbar_teardown(Statusbar* statusbar) {
    
    if (statusbar == NULL) {
        return;
    }

    // Detach handlers
    tickit_window_unbind_event_id(statusbar->hedit->rootwin, statusbar->on_resize_bind_id);
    event_del(statusbar->on_mode_switch_registration);
    event_del(statusbar->on_file_open_registration);
    event_del(statusbar->on_file_write_registration);
    event_del(statusbar->on_file_close_registration);
    log_unregister_sink(statusbar->log_sink_registration);

    // Destroy the window and free the memory
    tickit_window_close(statusbar->win);
    tickit_window_destroy(statusbar->win);
    free(statusbar);

}

void hedit_statusbar_redraw(Statusbar* statusbar) {
    tickit_window_expose(statusbar->win, NULL);
}