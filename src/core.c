#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
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
    
    // Add the key to the buffer, but ignore any combo key,
    if (key[0] != '<') {
        if (!buffer_put_char(hedit->command_buffer, key[0])) {
            log_fatal("Cannot insert char into command line buffer.");
        }
        hedit_statusbar_redraw(hedit->statusbar);
    }

}

static void mode_insert_on_input(HEdit* hedit, const char* key) {

    // Delegate to the current active view
    if (hedit->view->on_input != NULL) {
        hedit->view->on_input(hedit, key, false);
    }

}

static bool mode_insert_on_exit(HEdit* hedit, Mode* next) {

    // Add a new revision to the file being edited
    if (hedit->file != NULL) {
        hedit_file_commit_revision(hedit->file);
    }

    return true;
}

static void mode_replace_on_input(HEdit* hedit, const char* key) {
    
    // Delegate to the current active view
    if (hedit->view->on_input != NULL) {
        hedit->view->on_input(hedit, key, true);
    }

}

static bool mode_replace_on_exit(HEdit* hedit, Mode* next) {

    // Add a new revision to the file being edited
    if (hedit->file != NULL) {
        hedit_file_commit_revision(hedit->file);
    }

    return true;
}

Mode hedit_modes[] = {
    
    [HEDIT_MODE_NORMAL] = {
        .id = HEDIT_MODE_NORMAL,
        .name = "NORMAL",
        .display_name = "NORMAL",
        .bindings = NULL
    },

    [HEDIT_MODE_INSERT] = {
        .id = HEDIT_MODE_INSERT,
        .name = "INSERT",
        .display_name = "INSERT",
        .bindings = NULL,
        .on_input = mode_insert_on_input,
        .on_exit = mode_insert_on_exit
    },

    [HEDIT_MODE_REPLACE] = {
        .id = HEDIT_MODE_REPLACE,
        .name = "REPLACE",
        .display_name = "REPLACE",
        .bindings = NULL,
        .on_input = mode_replace_on_input,
        .on_exit = mode_replace_on_exit
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



// This array will be populated by the single functions in the files views/*.c
View hedit_views[HEDIT_VIEW_MAX];

void hedit_switch_view(HEdit* hedit, enum Views v) {
    assert(v >= HEDIT_VIEW_SPLASH && v <= HEDIT_VIEW_MAX);

    // If the views have not been initialized yet, do it now
    if (hedit_views[0].id == 0) {
        INIT_VIEW(HEDIT_VIEW_SPLASH);
        INIT_VIEW(HEDIT_VIEW_EDIT);
    }

    View* old = hedit->view;
    View* new = &hedit_views[v];

    if (old == new) {
        return;
    }

    // Perform the switch and invoke the enter/exit events
    if (old != NULL && old->on_exit != NULL) {
        if (!old->on_exit(hedit, new)) {
            return; // Switch vetoed
        }
    }
    hedit->view = new;
    if (new->on_enter != NULL) {
        if (!new->on_enter(hedit, old == NULL ? new : old)) {
            hedit->view = old; // Switch vetoed
            return;
        }
    }

    // Fire the event
    log_debug("View switch: %s -> %s", old == NULL ? NULL : old->name, new->name);
    event_fire(&hedit->ev_view_switch, hedit, new, old);

    tickit_window_expose(hedit->viewwin, NULL);
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

    // Bold red for errors
    theme->error = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 1,
        TICKIT_PEN_BG, 16,
        TICKIT_PEN_BOLD, true,
        -1
    );

    // Primary highlight for cursor
    theme->highlight1 = tickit_pen_clone(theme->text);
    tickit_pen_set_bool_attr(theme->highlight1, TICKIT_PEN_REVERSE, true);

    // Secondary highlight
    theme->highlight2 = tickit_pen_clone(theme->text);
    tickit_pen_set_bool_attr(theme->highlight2, TICKIT_PEN_BOLD, true);
    tickit_pen_set_bool_attr(theme->highlight2, TICKIT_PEN_UNDER, true);

    return theme;

}

static bool free_theme(const char* unused, void* theme, void* unused2) {
    Theme* t = theme;

    tickit_pen_unref(t->text);
    tickit_pen_unref(t->error);
    tickit_pen_unref(t->highlight1);
    tickit_pen_unref(t->highlight2);
    free(t);

    return true;
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
    hedit_redraw(hedit);
    log_debug("Theme switched to %s.", name);
    return true;

}



// -----------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------



bool hedit_option_register(HEdit* hedit, const char* name, enum OptionType type, const Value default_value,
                           bool (*on_change)(HEdit*, Option*, const Value*))
{
    assert(type >= HEDIT_OPTION_TYPE_INT && type <= HEDIT_OPTION_TYPE_MAX);

    Option* opt = malloc(sizeof(Option));
    if (opt == NULL) {
        log_fatal("Out of memory.");
        return false;
    }
    opt->name = name;
    opt->type = type;
    opt->default_value = default_value;
    opt->value = default_value;
    opt->on_change = on_change;

    if (!map_put(hedit->options, name, opt)) {
        log_fatal("Out of memory.");
        free(opt);
        return false;
    }

    return true;
}

bool hedit_option_set(HEdit* hedit, const char* name, const char* newstr) {

    // Look for the option in the map
    Option* opt = map_get(hedit->options, name);
    if (opt == NULL) {
        log_error("Unknown option %s.", name);
        return false;
    }

    Value newvalue = { 0 };

    switch (opt->type) {

        case HEDIT_OPTION_TYPE_INT:

            // Argument is required
            if (newstr == NULL) {
                log_error("Value required.");
                return false;
            }

            if (!str2int(newstr, 10, &newvalue.i)) {
                log_error("Invalid value %s for option %s.", newstr, name);
                return false;
            }

            break;

        case HEDIT_OPTION_TYPE_BOOL:

            // Bool options might not have an argument, which defaults to true
            if (newstr == NULL) {
                newvalue.b = true;
            } else if (strcasecmp("true", newstr) == 0 || strcasecmp("yes", newstr) == 0) {
                newvalue.b = true;
            } else if (strcasecmp("false", newstr) == 0 || strcasecmp("no", newstr) == 0) {
                newvalue.b = false;
            } else {
                log_error("Invalid value %s for option %s.", newstr, name);
                return false;
            }
            break;

        default:
            assert(false);

    }

    // Invoke the callback and change the option
    if (opt->on_change != NULL && !opt->on_change(hedit, opt, &newvalue)) {
        log_error("Invalid value %s for option %s.", newstr, name);
        return false;
    }
    opt->value = newvalue;

    return true;

}

static bool option_colwidth(HEdit* hedit, Option* opt, const Value* v) {
    if (v->i <= 0) {
        return false;
    } else {
        hedit_redraw(hedit);
        return true;
    }
}

static bool option_lineoffset(HEdit* hedit, Option* opt, const Value* v) {
    hedit_redraw(hedit);
    return true;
}

static bool init_builtin_options(HEdit* hedit) {
    
    if ((hedit->options = map_new()) == NULL) {
        log_fatal("Cannot create options map: out of memory.");
        return false;
    }

    // Register all the options!

#define REG(name, type, def, on_change) \
    if (!hedit_option_register(hedit, name, HEDIT_OPTION_TYPE_##type, (const Value) def, on_change)) { \
        return false; \
    }

    REG("colwidth",    INT,   { .i = 16   },  option_colwidth);
    REG("lineoffset",  BOOL,  { .b = true },  option_lineoffset);

#undef REG

    return true;

}


// -----------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------



static int on_viewwin_expose(TickitWindow* win, TickitEventFlags flags, void* info, void* user) {

    HEdit* hedit = user;
    TickitExposeEventInfo* e = info;

    // Delegate the drawing of the main window to the current view
    assert(hedit->view != NULL);
    tickit_renderbuffer_eraserect(e->rb, &e->rect);
    hedit->view->on_draw(hedit, win, e);

    return 1;
}

static int on_resize(TickitWindow* win, TickitEventFlags flags, void* info, void* user) {
    HEdit* hedit = user;

    // The view window should always be the full screen, except the last two lines for the statusbar
    TickitRect parentgeom = tickit_window_get_geometry(hedit->rootwin);
    tickit_window_set_geometry(hedit->viewwin, (TickitRect) {
        .top = 0,
        .left = 0,
        .lines = parentgeom.lines - 2,
        .cols = parentgeom.cols
    });

    // Force a repaint
    tickit_window_expose(hedit->viewwin, NULL);

    return 1;
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

HEdit* hedit_core_init(Options* cli_options, Tickit* tickit) {

    // Allocate space for the global state
    HEdit* hedit = calloc(1, sizeof(HEdit));
    if (hedit == NULL) {
        log_fatal("Cannot allocate memory for global state.");
        goto error;
    }
    hedit->cli_options = cli_options;
    hedit->tickit = tickit;
    hedit->rootwin = tickit_get_rootwin(tickit);
    hedit->exit = false;
    event_init(&hedit->ev_load);
    event_init(&hedit->ev_quit);
    event_init(&hedit->ev_mode_switch);
    event_init(&hedit->ev_view_switch);
    event_init(&hedit->ev_file_open);
    event_init(&hedit->ev_file_beforewrite);
    event_init(&hedit->ev_file_write);
    event_init(&hedit->ev_file_close);

    // Initialize default builtin options
    if (!init_builtin_options(hedit)) {
        goto error;
    }

    // Create the window for the view
    hedit->viewwin = tickit_window_new(hedit->rootwin, (TickitRect) {
        .top = 0,
        .left = 0,
        .lines = tickit_window_lines(hedit->rootwin) - 2,
        .cols = tickit_window_cols(hedit->rootwin)
    }, 0);
    if (hedit->viewwin == NULL) {
        log_fatal("Canont create tickit window.");
        goto error;
    }

    // Initialize the default theme
    Theme* default_theme = init_default_theme();
    if (default_theme == NULL) {
        goto error;
    }
    if (!hedit_register_theme(hedit, "default", default_theme)) {
        goto error;
    }
    hedit->theme = default_theme;

    // Register the handler for the events windows
    hedit->on_keypress_bind_id = tickit_window_bind_event(hedit->rootwin, TICKIT_WINDOW_ON_KEY, 0, on_keypress, hedit);
    hedit->on_resize_bind_id = tickit_window_bind_event(hedit->rootwin, TICKIT_WINDOW_ON_GEOMCHANGE, 0, on_resize, hedit);
    hedit->on_viewwin_expose_bind_id = tickit_window_bind_event(hedit->viewwin, TICKIT_WINDOW_ON_EXPOSE, 0, on_viewwin_expose, hedit);

    // Initialize statusbar
    if ((hedit->statusbar = hedit_statusbar_init(hedit)) == NULL) {
        goto error;
    }

    // Switch to normal mode and splash view
    hedit_switch_mode(hedit, HEDIT_MODE_NORMAL);
    hedit_switch_view(hedit, HEDIT_VIEW_SPLASH);

    // Exit with success
    return hedit;

error:

    if (hedit != NULL) {
        if (hedit->options != NULL) {
            map_free_full(hedit->options);
        }
        if (hedit->viewwin != NULL) {
            tickit_window_close(hedit->viewwin);
            tickit_window_destroy(hedit->viewwin);
        }
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

    // Invoke the view's on_exit handler
    if (hedit->view->on_exit != NULL) {
        hedit->view->on_exit(hedit, NULL);
    }

    // Terminate the single components
    hedit_statusbar_teardown(hedit->statusbar);

    // Remove event handlers
    tickit_window_unbind_event_id(hedit->rootwin, hedit->on_keypress_bind_id);
    tickit_window_unbind_event_id(hedit->rootwin, hedit->on_resize_bind_id);
    tickit_window_unbind_event_id(hedit->viewwin, hedit->on_viewwin_expose_bind_id);

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

    // Options
    map_free_full(hedit->options);

    // Destroy the view window
    tickit_window_close(hedit->viewwin);
    tickit_window_destroy(hedit->viewwin);

    // Free the global state
    free(hedit);

}

void hedit_redraw(HEdit* hedit) {
    tickit_window_expose(hedit->rootwin, NULL);
}

void hedit_redraw_view(HEdit* hedit) {
    tickit_window_expose(hedit->viewwin, NULL);
}