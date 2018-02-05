#include <string.h>
#include <time.h>
#include <assert.h>
#include <tickit.h>

#include "core.h"
#include "actions.h"
#include "util/list.h"
#include "util/log.h"

static const char* severity_names[] = {
    "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL"
};

typedef struct {
    char timestamp[20];
    log_severity severity;
    const char* file;
    int line;
    const char* message;

    struct list_head list;
} LogMessage;

typedef struct {
    View* oldview;
    size_t scroll;
    struct list_head* start_message;
    bool can_scroll_down;
} ViewState;



static struct list_head messages = LIST_HEAD_INIT(messages);
static size_t messages_count = 0;



static void on_log(void* user, struct log_config* cfg, const char* file, int line, log_severity severity, const char* format, va_list args) {
    
    LogMessage* msg = calloc(1, sizeof(LogMessage));
    if (msg == NULL) {
        return;
    }
    msg->severity = severity;
    msg->line = line;
    list_init(&msg->list);

    // Store the formatted timestamp
    time_t t;
    struct tm* tm_info;
    time(&t);
    tm_info = localtime(&t);
    if (strftime((char*) &msg->timestamp, 20, "%Y-%m-%d %H:%M:%S", tm_info) == 0) {
        // Error formatting the time: do not output the time
        msg->timestamp[0] = '\0'; 
    }

    // Clone the file name
    const char* filedup = strdup(file);
    if (filedup == NULL) {
        goto error;
    }
    msg->file = filedup;

    // Preformat the message

    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    if (size < 0) {
        goto error;
    }

    char* message = malloc(1 + size * sizeof(char));
    if (message == NULL) {
        goto error;
    }
    msg->message = message;

    size = vsnprintf(message, size + 1, format, args);
    if (size < 0) {
        goto error;
    }

    list_add_tail(&messages, &msg->list);
    messages_count++;

    return;

error:
    if (msg != NULL) {
        if (msg->file != NULL) {
            free((void*) msg->file);
        }
        if (msg->message != NULL) {
            free((void*) msg->message);
        }
        free(msg);
    }
}

static bool on_enter(HEdit* hedit, View* prev) {
    // Store the previous view so that we can return to it when we exit
    ViewState* state = calloc(1, sizeof(ViewState));
    if (state == NULL) {
        log_fatal("Out of memory.");
        return false;
    }
    state->oldview = prev;
    hedit->viewdata = state;
    return true;
}

static bool on_exit(HEdit* hedit, View* next) {
    free(hedit->viewdata);
    return true;
}

static void on_draw(HEdit* hedit, TickitWindow* win, TickitExposeEventInfo* e) {
    ViewState* state = hedit->viewdata;

    // Clear the window
    tickit_renderbuffer_eraserect(e->rb, &e->rect);

    TickitPen* severity_pen[] = {
        hedit->theme->log_debug,
        hedit->theme->log_info,
        hedit->theme->log_warn,
        hedit->theme->log_error,
        hedit->theme->log_fatal
    };

    int win_lines = tickit_window_lines(win);
    size_t line = 0;

    if (messages_count > 0) {
        state->start_message = state->start_message == NULL ? messages.next : state->start_message;

        for (struct list_head* pos = state->start_message; pos != &messages; pos = pos->next) {
            LogMessage* m = container_of(pos, LogMessage, list);
    
            tickit_renderbuffer_setpen(e->rb, severity_pen[m->severity]);
            tickit_renderbuffer_textf_at(e->rb, line, 0, "%s %s %s:%d %s",
                m->timestamp, severity_names[m->severity], m->file, m->line, m->message);
            line++;

            if (line == win_lines) {
                break;
            }
        }
    }
    
    state->can_scroll_down = messages_count > state->scroll + line;

    // Fill the remaining lines with `~`
    if (win_lines > line) {
        tickit_renderbuffer_setpen(e->rb, hedit->theme->linenos);
        while (line < win_lines) {
            tickit_renderbuffer_text_at(e->rb, line, 0, "~");
            line++;
        }
    }

}

static void on_movement(HEdit* hedit, enum Movement m, size_t arg) {
    ViewState* state = hedit->viewdata;

    if (messages_count == 0 || state->start_message == NULL) {
        return;
    }
    
    struct list_head* top = state->start_message;

    switch (m) {
        case HEDIT_MOVEMENT_UP:
            if (messages.next != top) {
                assert(state->scroll > 0);
                state->start_message = top->prev;
                state->scroll--;
            }
            break;
        case HEDIT_MOVEMENT_DOWN:
            if (state->can_scroll_down) {
                state->start_message = top->next;
                state->scroll++;
            }
            break;
        default:
            return;
    }

    hedit_redraw_view(hedit);
}

static void do_quit(HEdit* hedit, const Value* arg) {
    ViewState* state = hedit->viewdata;
    hedit_switch_view(hedit, state->oldview->id);
}

static Action action_quit = {
    .cb = do_quit
};

static View definition = {
    .id = HEDIT_VIEW_LOG,
    .name = "log",
    .on_enter = on_enter,
    .on_exit = on_exit,
    .on_draw = on_draw,
    .on_movement = on_movement
};

REGISTER_VIEW2(HEDIT_VIEW_LOG, definition, {

    // Prepare a map for the binding overrides
    Map* map = map_new();
    if (map == NULL) {
        log_fatal("Out of memory.");
        return;
    }
    if (!map_put(map, "q", &action_quit)) {
        log_fatal("Out of memory.");
        map_free(map);
        return;
    }
    definition.binding_overrides[HEDIT_MODE_NORMAL] = map;

    // Register a log sink at initialization time,
    // since we have to collect log messages even if the view is not active
    void* token = log_register_sink(on_log, NULL);
    if (token == NULL) {
        log_fatal("Out of memory.");
        return;
    }

})