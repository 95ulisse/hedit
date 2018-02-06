#include <string.h>
#include <time.h>
#include <assert.h>
#include <tickit.h>

#include "core.h"
#include "actions.h"
#include "util/list.h"
#include "util/log.h"

#define MAX_LOG_ENTRIES 100
#define MAX_TIMESTAMP_LEN 24
#define MAX_FILE_LEN 64
#define MAX_MESSAGE_LEN 512

static const char* severity_names[] = {
    "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL"
};

typedef struct {
    char timestamp[MAX_TIMESTAMP_LEN];
    log_severity severity;
    char file[MAX_FILE_LEN];
    int line;
    char message[MAX_MESSAGE_LEN];

    struct list_head list;
} LogMessage;

typedef struct {
    View* oldview;
    size_t scroll;
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
    if (strftime((char*) &msg->timestamp, MAX_TIMESTAMP_LEN, "%Y-%m-%d %H:%M:%S", tm_info) == 0) {
        // Error formatting the time: do not output the time
        msg->timestamp[0] = '\0'; 
    }

    // Copy the file name
    strncpy(msg->file, file, MAX_FILE_LEN);
    msg->file[MAX_FILE_LEN - 1] = '\0';

    // Preformat the message
    if (vsnprintf(msg->message, MAX_MESSAGE_LEN, format, args) < 0) {
        goto error;
    }
    msg->message[MAX_MESSAGE_LEN - 1] = '\0';

    // Add the message to the global queue
    list_add_tail(&messages, &msg->list);
    if (messages_count < MAX_LOG_ENTRIES) {
        messages_count++;
    } else {
        LogMessage* first = list_first(&messages, LogMessage, list);
        list_del(&first->list);
        free(first);
    }

    return;

error:
    if (msg != NULL) {
        free(msg);
    }
}

static void on_program_exit() {
    // Free all the stored messages
    list_for_each_member(m, &messages, LogMessage, list) {
        free(m);
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
    size_t skip = 0;

    list_for_each_member(m, &messages, LogMessage, list) {
        if (skip < state->scroll) {
            skip++;
            continue;
        }

        tickit_renderbuffer_setpen(e->rb, severity_pen[m->severity]);
        tickit_renderbuffer_textf_at(e->rb, line, 0, "%s %s %s:%d %s",
            m->timestamp, severity_names[m->severity], m->file, m->line, m->message);
        line++;

        if (line == win_lines) {
            break;
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

    switch (m) {
        case HEDIT_MOVEMENT_UP:
            if (state->scroll > 0) {
                state->scroll--;
            }
            break;
        case HEDIT_MOVEMENT_DOWN:
            if (state->can_scroll_down) {
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
    
    // Register an handler for the program exit to clean up all the stored messages
    atexit(on_program_exit);

})