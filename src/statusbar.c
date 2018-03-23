#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <tickit.h>

#include "core.h"
#include "statusbar.h"
#include "util/log.h"
#include "util/event.h"

#define MAX_MESSAGE_LEN 512

struct Statusbar {
    HEdit* hedit;
    TickitWindow* win;
    char last_message[MAX_MESSAGE_LEN];
    bool last_message_is_error;
    bool last_message_is_sticky;
    bool show_last_message;

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

        tickit_renderbuffer_setpen(rb, theme->block_cursor);
        tickit_renderbuffer_textn(rb, str + cursorpos, 1);
        tickit_renderbuffer_setpen(rb, theme->commandbar);

        if (cursorpos < len - 1) {
            tickit_renderbuffer_textn(rb, str + cursorpos + 1, len - cursorpos - 1);
        }

    }

}

static int on_expose(TickitWindow* win, TickitEventFlags flags, void* info, void* user) {

    Statusbar* statusbar = user;
    TickitExposeEventInfo* e = info;

    bool redraw_first = e->rect.top == 0 && e->rect.lines >= 1;
    bool redraw_second = e->rect.top <= 1 && e->rect.top + e->rect.lines >= 2;

    if (redraw_first) {

        // Clear first line
        tickit_renderbuffer_setpen(e->rb, statusbar->hedit->theme->statusbar);
        tickit_renderbuffer_eraserect(e->rb, &(TickitRect){ .top = 0, .left = 0, .lines = 1, .cols = tickit_window_cols(win) });
    
        // Open file info on the right
        if (statusbar->hedit->file != NULL) {
            File* f = statusbar->hedit->file;
            char* format_name = ((Option*) map_get(statusbar->hedit->options, "format"))->value.str;
            bool format_is_none = strcmp("none", format_name) == 0;
    
            const char* fname = hedit_file_name(f);
            int buf_size = tickit_window_cols(win) + 1;
            char buf[buf_size];
            int printed =
                snprintf(buf, buf_size, "%s %s%s%s%s",
                    fname == NULL ? "<no name>" : fname,
                    hedit_file_is_ro(f) ? "[ro] " : "",
                    format_is_none ? "" : "[",
                    format_is_none ? "" : format_name,
                    format_is_none ? "" : "] "
                );
            buf[buf_size - 1] = '\0';
            tickit_renderbuffer_text_at(e->rb, 0, tickit_window_cols(win) - MIN(printed, buf_size - 1), buf);
        }
    
        // Current mode on the left
        tickit_renderbuffer_textf_at(e->rb, 0, 0, " -- %s --", statusbar->hedit->mode->display_name);

    }

    if (redraw_second) {

        // Clear second line
        tickit_renderbuffer_setpen(e->rb, statusbar->hedit->theme->commandbar);
        tickit_renderbuffer_eraserect(e->rb, &(TickitRect){ .top = 1, .left = 0, .lines = 1, .cols = tickit_window_cols(win) });
    
        tickit_renderbuffer_goto(e->rb, 1, 0);
    
        // Draw the command line to reflect the current buffer contents.
        // It should never happen that we have to show an error while the user is typing a command.
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
                tickit_renderbuffer_setpen(e->rb, statusbar->hedit->theme->block_cursor);
                tickit_renderbuffer_text(e->rb, " ");
            }
    
        } else if (statusbar->show_last_message) {
    
            // Draw the message
            tickit_renderbuffer_setpen(e->rb, statusbar->last_message_is_error ? statusbar->hedit->theme->error : statusbar->hedit->theme->text);
            tickit_renderbuffer_text(e->rb, statusbar->last_message);
    
        }

    }

    return 1;

}

static inline void redraw(Statusbar* statusbar) {
    tickit_window_expose(statusbar->win, NULL);
}

static inline void redraw_first_line(Statusbar* statusbar) {
    tickit_window_expose(
        statusbar->win,
        &(TickitRect) { .top = 0, .left = 0, .cols = tickit_window_cols(statusbar->win), .lines = 1 }
    );
}

static inline void redraw_second_line(Statusbar* statusbar) {
    tickit_window_expose(
        statusbar->win,
        &(TickitRect) { .top = 1, .left = 0, .cols = tickit_window_cols(statusbar->win), .lines = 1 }
    );
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

    // Force a redraw
    redraw(statusbar);

    return 1;
    
}

static void on_mode_switch(void* user, HEdit* hedit, Mode* new, Mode* old) {
    Statusbar* statusbar = user;

    // Hide the last error
    if (!statusbar->last_message_is_sticky) {
        statusbar->show_last_message = false;
    }

    // Force a redraw of the first line
    redraw_first_line(statusbar);
}

static void on_file_event(void* user, HEdit* hedit, File* file) {
    Statusbar* statusbar = user;
    redraw_first_line(statusbar);
}

static void on_log_message(void* user, struct log_config* config, const char* file, int line, log_severity severity, const char* format, va_list args) {

    // We want to show messages of severity error and fatal on the statusbar
    if (severity < LOG_ERROR) {
        return;
    }

    Statusbar* statusbar = user;

    // Print the message to the internal buffer
    vsnprintf(statusbar->last_message, MAX_MESSAGE_LEN, format, args);
    statusbar->last_message[MAX_MESSAGE_LEN - 1] = '\0';
    statusbar->last_message_is_error = true;
    statusbar->last_message_is_sticky = false;
    statusbar->show_last_message = true;

    // Force a redraw
    redraw_second_line(statusbar);
}

Statusbar* hedit_statusbar_init(HEdit* hedit) {

    // Allocate space
    Statusbar* statusbar = calloc(1, sizeof(Statusbar));
    if (statusbar == NULL) {
        log_fatal("Cannot allocate memory for statusbar.");
        return NULL;
    }
    statusbar->hedit = hedit;
    statusbar->show_last_message = false;
    statusbar->last_message_is_error = false;

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
    redraw(statusbar);
}

void hedit_statusbar_show_message(Statusbar* statusbar, bool sticky, const char* msg) {
    
    // Do not show a normal message if an error is visible and has not been explicitely cleared
    if (msg != NULL && statusbar->show_last_message && statusbar->last_message_is_error) {
        return;
    }

    if (msg == NULL) {
        statusbar->show_last_message = false;
    } else {
        strncpy(statusbar->last_message, msg, MAX_MESSAGE_LEN);
        statusbar->last_message[MAX_MESSAGE_LEN - 1] = '\0';
        statusbar->last_message_is_error = false;
        statusbar->last_message_is_sticky = sticky;
        statusbar->show_last_message = true;
    }

    redraw_second_line(statusbar);
}
