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

    // Hide any previous message shown on the statusbar
    hedit_statusbar_show_message(hedit->statusbar, false, NULL);

    return true;
}

static bool mode_command_on_exit(HEdit* hedit, Mode* next) {
    buffer_free(hedit->command_buffer);
    hedit->command_buffer = NULL;
    return true;
}

static void mode_command_on_input(HEdit* hedit, const char* key) {
    
    // Add the key to the buffer, but ignore any combo key,
    bool islt = strncmp("<lt>", key, 4) == 0;
    if (islt || key[0] != '<') {
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
        .name = "normal",
        .display_name = "NORMAL",
        .bindings = NULL
    },

    [HEDIT_MODE_INSERT] = {
        .id = HEDIT_MODE_INSERT,
        .name = "insert",
        .display_name = "INSERT",
        .bindings = NULL,
        .on_input = mode_insert_on_input,
        .on_exit = mode_insert_on_exit
    },

    [HEDIT_MODE_REPLACE] = {
        .id = HEDIT_MODE_REPLACE,
        .name = "replace",
        .display_name = "REPLACE",
        .bindings = NULL,
        .on_input = mode_replace_on_input,
        .on_exit = mode_replace_on_exit
    },

    [HEDIT_MODE_COMMAND] = {
        .id = HEDIT_MODE_COMMAND,
        .name = "command",
        .display_name = "NORMAL",
        .bindings = NULL,
        .on_enter = mode_command_on_enter,
        .on_exit = mode_command_on_exit,
        .on_input = mode_command_on_input
    }

};

Mode* hedit_mode_from_name(const char* name) {
    for (int i = HEDIT_MODE_NORMAL; i < HEDIT_MODE_MAX; i++) {
        if (strcasecmp(hedit_modes[i].name, name) == 0) {
            return &hedit_modes[i];
        }
    }
    return NULL;
}

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
    event_fire(&hedit->ev_mode_switch, hedit, new, old);
}



// -----------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------



// This array will be populated by the single functions in the files views/*.c
View hedit_views[HEDIT_VIEW_MAX];

View* hedit_view_from_name(const char* name) {
    for (int i = HEDIT_VIEW_SPLASH; i < HEDIT_VIEW_MAX; i++) {
        if (strcasecmp(hedit_views[i].name, name) == 0) {
            return &hedit_views[i];
        }
    }
    return NULL;
}

void hedit_switch_view(HEdit* hedit, enum Views v) {
    assert(v >= HEDIT_VIEW_SPLASH && v <= HEDIT_VIEW_MAX);

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



static Theme* default_theme() {

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

    // Gray line numbers
    theme->linenos = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 8,
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

    // Full cell for block cursor
    theme->block_cursor = tickit_pen_clone(theme->text);
    tickit_pen_set_bool_attr(theme->block_cursor, TICKIT_PEN_REVERSE, true);

    // Soft cursor is just bold and underline, no background
    theme->soft_cursor = tickit_pen_clone(theme->text);
    tickit_pen_set_bool_attr(theme->soft_cursor, TICKIT_PEN_BOLD, true);
    tickit_pen_set_bool_attr(theme->soft_cursor, TICKIT_PEN_UNDER, true);

    // Statusbar with light background and dark text
    theme->statusbar = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 234,
        TICKIT_PEN_BG, 247,
        -1
    );

    // Command bar exactly like text
    theme->commandbar = tickit_pen_clone(theme->text);

    // Log highlights
    theme->log_debug = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 8,
        TICKIT_PEN_BG, 16,
        -1
    );
    theme->log_info = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 6,
        TICKIT_PEN_BG, 16,
        -1
    );
    theme->log_warn = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 3,
        TICKIT_PEN_BG, 16,
        TICKIT_PEN_BOLD, true,
        -1
    );
    theme->log_error = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 1,
        TICKIT_PEN_BG, 16,
        TICKIT_PEN_BOLD, true,
        -1
    );
    theme->log_fatal = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 5,
        TICKIT_PEN_BG, 16,
        TICKIT_PEN_BOLD, true,
        -1
    );

    // Text colors for the file formats
    theme->white = tickit_pen_clone(theme->text);
    theme->gray = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 8,
        TICKIT_PEN_BG, 16,
        -1
    );
    theme->blue = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 4,
        TICKIT_PEN_BG, 16,
        -1
    );
    theme->red = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 1,
        TICKIT_PEN_BG, 16,
        -1
    );
    theme->pink = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 13,
        TICKIT_PEN_BG, 16,
        -1
    );
    theme->green = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 2,
        TICKIT_PEN_BG, 16,
        -1
    );
    theme->purple = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 5,
        TICKIT_PEN_BG, 16,
        -1
    );
    theme->orange = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 208,
        TICKIT_PEN_BG, 16,
        -1
    );

    return theme;

}

static void free_theme(Theme* t) {
    if (t == NULL) {
        return;
    }
    tickit_pen_unref(t->text);
    tickit_pen_unref(t->linenos);
    tickit_pen_unref(t->error);
    tickit_pen_unref(t->block_cursor);
    tickit_pen_unref(t->soft_cursor);
    tickit_pen_unref(t->statusbar);
    tickit_pen_unref(t->commandbar);
    tickit_pen_unref(t->log_debug);
    tickit_pen_unref(t->log_info);
    tickit_pen_unref(t->log_warn);
    tickit_pen_unref(t->log_error);
    tickit_pen_unref(t->log_fatal);
    tickit_pen_unref(t->white);
    tickit_pen_unref(t->gray);
    tickit_pen_unref(t->blue);
    tickit_pen_unref(t->red);
    tickit_pen_unref(t->pink);
    tickit_pen_unref(t->green);
    tickit_pen_unref(t->purple);
    tickit_pen_unref(t->orange);
    free(t);
}

void hedit_switch_theme(HEdit* hedit, Theme* newtheme) {

    if (hedit->theme != NULL) {
        free_theme(hedit->theme);
    }
   
    // Update the pointers and perform a full redraw
    hedit->theme = newtheme;
    hedit_redraw(hedit);

}



// -----------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------



bool hedit_option_register(HEdit* hedit, const char* name, enum OptionType type, const Value default_value,
                           bool (*on_change)(HEdit*, Option*, const Value*, void* user), void* user)
{
    assert(type >= HEDIT_OPTION_TYPE_INT && type <= HEDIT_OPTION_TYPE_MAX);

    Option* opt = malloc(sizeof(Option));
    if (opt == NULL) {
        log_fatal("Out of memory.");
        return false;
    }

    if (!map_put(hedit->options, name, opt)) {
        if (errno == ENOMEM) {
            log_fatal("Out of memory.");
        }
        free(opt);
        return false;
    }

    opt->name = name;
    opt->type = type;
    opt->default_value = default_value;
    opt->value = default_value;
    opt->on_change = on_change;
    opt->user = user;

    // Duplicate the default value if the type of the option is a string
    if (type == HEDIT_OPTION_TYPE_STRING) {
        char* dup = strdup(default_value.str);
        char* dup2 = strdup(default_value.str);
        if (dup == NULL || dup2 == NULL) {
            log_fatal("Out of memory.");
            map_delete(hedit->options, name);
            free(opt);
            return false;
        }
        opt->default_value = (Value){ .str = dup };
        opt->value = (Value){ .str = dup2 };
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

            // Stop here if the value did not actually change
            if (newvalue.i == opt->value.i) {
                return true;
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
            
            // Stop here if the value did not actually change
            if (newvalue.b == opt->value.b) {
                return true;
            }

            break;

        case HEDIT_OPTION_TYPE_STRING:
        {    
            // Argument is required
            if (newstr == NULL) {
                log_error("Value required.");
                return false;
            }

            // Stop here if the value did not actually change
            if (strcmp(newstr, opt->value.str) == 0) {
                return true;
            }

            // Duplicate the string for storage
            char* dup = strdup(newstr);
            if (dup == NULL) {
                log_fatal("Out of memory.");
                return false;
            }
            newvalue.str = dup;

            break;
        }

        default:
            abort();

    }

    // Invoke the callback and change the option
    if (opt->on_change != NULL && !opt->on_change(hedit, opt, &newvalue, opt->user)) {
        log_error("Invalid value %s for option %s.", newstr, name);
        
        // Free the allocated duplicate string
        if (opt->type == HEDIT_OPTION_TYPE_STRING) {
            free(newvalue.str);
        }

        return false;
    }

    // Before updating the option value, free the old string
    if (opt->type == HEDIT_OPTION_TYPE_STRING) {
        free(opt->value.str);
    }

    opt->value = newvalue;
    log_debug("New value for option %s: %s", name, newstr);

    return true;

}

static bool free_option(const char* key, void* value, void* data) {
    Option* opt = value;
    if (opt->type == HEDIT_OPTION_TYPE_STRING) {
        free(opt->default_value.str);
        free(opt->value.str);
    }
    free(opt);
    return true;
}

static bool option_colwidth(HEdit* hedit, Option* opt, const Value* v, void* user) {
    if (v->i <= 0) {
        return false;
    } else {
        hedit_redraw(hedit);
        return true;
    }
}

static bool option_lineoffset(HEdit* hedit, Option* opt, const Value* v, void* user) {
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
    if (!hedit_option_register(hedit, name, HEDIT_OPTION_TYPE_##type, (const Value) def, on_change, NULL)) { \
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

    // Wrap the key name in <> if it is not a single char.
    // Escape the literal `<` as `<lt>`.
    char key[30];
    if (strlen(e->str) == 1) {
        if (e->str[0] == '<') {
            strcpy(key, "<lt>");
        } else {
            strncpy(key, e->str, 30);
            key[29] = '\0';
        }
    } else {
        snprintf(key, 30, "<%s>", e->str);
    }

    hedit_emit_keys(hedit, key);
    
    return 1;

}

static void mapped_key_action(HEdit* hedit, const Value* arg) {

    // Commit a new revision if a file is opened
    if (hedit->file != NULL) {
        hedit_file_commit_revision(hedit->file);
    }
    
    // Simulate the various keys
    hedit_emit_keys(hedit, arg->str);

}

bool hedit_map_keys(HEdit* hedit, enum Modes m, const char* from, const char* to, bool force) {
    assert(m >= HEDIT_MODE_NORMAL && m <= HEDIT_MODE_MAX);

    // Duplicate the strings representing the keys
    char* fromdup = strdup(from);
    char* todup = strdup(to);
    if (fromdup == NULL || todup == NULL) {
        log_fatal("Out of memory.");
        return false;
    }

    Action* a = malloc(sizeof(Action));
    if (a == NULL) {
        log_fatal("Out of memory.");
        return false;
    }
    a->cb = mapped_key_action;
    a->arg.str = todup;

    if (!map_put(hedit_modes[m].bindings, fromdup, a)) {
        if (errno == EEXIST && !force) {
            log_error("A mapping for the same key already exists. Use map! to disable this warning.");
            free(a);
            return false;
        } else if (errno == EEXIST && force) {
            map_delete(hedit_modes[m].bindings, fromdup);
            if (!map_put(hedit_modes[m].bindings, fromdup, a)) {
                log_fatal("Out of memory.");
                free(a);
                return false;
            }
        } else {
            log_fatal("Out of memory.");
            free(a);
            return false;
        }
    }

    log_debug("Mapping registered: %s %s => %s", hedit_modes[m].name, from, to);

    return true;
}

void hedit_emit_keys(HEdit* hedit, const char* keys) {
    
    // Split each single key
    char buf[20];
    size_t keys_len = strlen(keys);
    for (int i = 0; i < keys_len; i++) {
        if (keys[i] == '<') {

            // Find matching closing angular parenthesis
            int j = i + 1;
            while (keys[j] != '\0' && keys[j] != '>') {
                j++;
            }
            if (keys[j] != '>') {
                break;
            }
            int keylen = j - i - 1;
            i = j;
            if (keylen == 0) {
                continue;
            }
            if (keylen + 2 /* For the <> */ > sizeof(buf) / sizeof(buf[0]) - 1) {
                log_error("Max key length exceeded. Keys to send: %s", keys);
                continue;
            }

            memmove(buf, keys + i - keylen - 1, keylen + 2);
            buf[keylen + 2] = '\0';

        } else {
            buf[0] = keys[i];
            buf[1] = '\0';
        }

        // If the current view exposes an override for this key, use that,
        // otherwise checks if it is mapped in the current mode
        Action* a = NULL;
        if (hedit->view->binding_overrides[hedit->mode->id] != NULL) {
            a = map_get(hedit->view->binding_overrides[hedit->mode->id], buf);
        }
        if (a == NULL) {
            a = map_get(hedit->mode->bindings, buf);
        }

        // Invoke the action, or pass the key as raw input
        if (a != NULL) {
            a->cb(hedit, &a->arg);
        } else if (hedit->mode->on_input != NULL) {
            hedit->mode->on_input(hedit, buf);
        }

    }

}

HEdit* hedit_core_init(Options* cli_options, Tickit* tickit) {
    
    // If the views have not been initialized yet, do it now
    if (hedit_views[0].id == 0) {
#pragma GCC diagnostic ignored "-Wimplicit-function-declaration"
        INIT_VIEW(HEDIT_VIEW_SPLASH);
        INIT_VIEW(HEDIT_VIEW_LOG);
        INIT_VIEW(HEDIT_VIEW_EDIT);
#pragma GCC diagnostic warning "-Wimplicit-function-declaration"
    }

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
    event_init(&hedit->ev_file_before_write);
    event_init(&hedit->ev_file_write);
    event_init(&hedit->ev_file_close);

    // Initialize default builtin options
    if (!init_builtin_options(hedit)) {
        goto error;
    }

    // Initialize default builtin commands
    if (!hedit_init_commands(hedit)) {
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
    Theme* deftheme = default_theme();
    if (deftheme == NULL) {
        goto error;
    }
    hedit->theme = deftheme;

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
        if (hedit->commands != NULL) {
            map_free_full(hedit->commands);
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
    
    // File format
    if (hedit->format != NULL) {
        hedit_format_free(hedit->format);
    }

    // Free the theme
    if (hedit->theme != NULL) {
        free_theme(hedit->theme);
    }

    // Options
    map_iterate(hedit->options, free_option, NULL);
    map_free(hedit->options);

    // Commands
    map_free_full(hedit->commands);

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