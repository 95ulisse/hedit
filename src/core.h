#ifndef __CORE_H__
#define __CORE_H__

#include <stdbool.h>
#include <tickit.h>

typedef struct HEdit HEdit;

#include "options.h"
#include "statusbar.h"
#include "file.h"
#include "util/common.h"
#include "util/map.h"
#include "util/event.h"
#include "util/buffer.h"



enum Modes {
    HEDIT_MODE_NORMAL = 1,
    HEDIT_MODE_INSERT,
    HEDIT_MODE_REPLACE,
    HEDIT_MODE_COMMAND, // Text editing of the command line after pressing ':'
    HEDIT_MODE_MAX
};

/**
 * A mode represents a structured way to group keybindings.
 *
 * It has an user-friendly name and some event hooks to perform special cleaning
 * during mode change.
 * 
 * If a key is not found in the bindings, the `on_input` function is called.
 * This is usually used to alter the document contents.
 */
typedef struct Mode Mode;
struct Mode {
    enum Modes id;
    const char* name;
    const char* display_name;
    Map* bindings; // Map of Action*
    bool (*on_enter)(HEdit* hedit, Mode* prev);
    bool (*on_exit)(HEdit* hedit, Mode* next);
    void (*on_input)(HEdit* hedit, const char* key);
};

/** Global definition of all the available modes. */
extern Mode hedit_modes[];


/** A theme is a collection of pens used to draw the various parts of the UI. */
typedef struct Theme Theme;
struct Theme {
    TickitPen* text;
    TickitPen* error;
    TickitPen* highlight1;
    TickitPen* highlight2;

    // When adding a new field to this structure, remember to update the default theme
    // and the `free_theme` function in core.c.
};



enum Views {
    HEDIT_VIEW_SPLASH = 1,
    HEDIT_VIEW_EDIT,
    HEDIT_VIEW_MAX
};

enum Movement {
    HEDIT_MOVEMENT_LEFT = 1,
    HEDIT_MOVEMENT_RIGHT,
    HEDIT_MOVEMENT_UP,
    HEDIT_MOVEMENT_DOWN,
    HEDIT_MOVEMENT_LINE_START,
    HEDIT_MOVEMENT_LINE_END,
    HEDIT_MOVEMENT_PAGE_UP,
    HEDIT_MOVEMENT_PAGE_DOWN,
    HEDIT_MOVEMENT_ABSOLUTE
};

/**
 * A view decides what should be drawn on the main screen and handles all the keyboard input.
 */
typedef struct View View;
struct View {
    enum Views id;
    const char* name;
    bool (*on_enter)(HEdit* hedit, View* prev);
    bool (*on_exit)(HEdit* hedit, View* next);
    void (*on_draw)(HEdit* hedit, TickitWindow* win, TickitExposeEventInfo* e);
    void (*on_input)(HEdit* hedit, const char* key, bool replace);
    void (*on_movement)(HEdit* hedit, enum Movement m, size_t arg);
};

/** Global definition of all the available views. */
extern View hedit_views[];

#define INIT_VIEW(v) __init_view_##v()
#define REGISTER_VIEW(id, definition) \
    void __init_view_##id() { \
        hedit_views[id] = definition; \
    }



enum OptionType {
    HEDIT_OPTION_TYPE_INT = 1,
    HEDIT_OPTION_TYPE_BOOL,
    HEDIT_OPTION_TYPE_MAX
};

/** A single `:set` option of the editor. */
typedef struct Option Option;
struct Option {
    const char* name;
    enum OptionType type;
    Value default_value;
    Value value;
    bool (*on_change)(HEdit*, Option*, const Value*);
};



/**
 * Global state of the editor.
 * Contains eveything needed to describe the precise state of HEdit:
 * from the opened file, to the mode we are in, to the current statusbar text.
 */
struct HEdit {

    Options* cli_options;

    // Components
    Mode* mode;
    Map* options; // Map of Option*
    File* file;
    View* view;
    void* viewdata; // Private state of the current view
    Statusbar* statusbar;
    Buffer* command_buffer;

    // Events                    // Handler signature
    Event ev_load;               // void (*)(HEdit*);
    Event ev_quit;               // void (*)(HEdit*);
    Event ev_mode_switch;        // void (*)(HEdit*, Mode* new, Mode* old)
    Event ev_view_switch;        // void (*)(HEdit*, View* new, View* old)
    Event ev_file_open;          // void (*)(HEdit*, File*)
    Event ev_file_beforewrite;   // void (*)(HEdit*, File*)
    Event ev_file_write;         // void (*)(HEdit*, File*)
    Event ev_file_close;         // void (*)(HEdit*, File*)

    // UI
    Tickit* tickit;
    TickitWindow* rootwin;
    TickitWindow* viewwin;
    Map* themes;
    Theme* theme;
    int on_keypress_bind_id;
    int on_resize_bind_id;
    int on_viewwin_expose_bind_id;

    // Exit flag and exit code
    bool exit;
    int exitcode;

};



/**
 * Initializes a new global state.
 * This function should be called only once at the beginning of the program.
 * 
 * @return The newly created global state, or NULL in case of error.
 */
HEdit* hedit_core_init(Options* options, Tickit* tickit);

/**
 * Releases all the resources associated with the given state.
 * This function should be called right before program exit.
 */
void hedit_core_teardown(HEdit* hedit);

/** Forces a full redraw of the UI. */
void hedit_redraw(HEdit* hedit);

/** Forces a full redraw of the current view. */
void hedit_redraw_view(HEdit* hedit);


/** Switches the editor to a new mode. */
void hedit_switch_mode(HEdit* hedit, enum Modes m);


/** Registers a new theme. */
bool hedit_register_theme(HEdit* hedit, const char* name, Theme* theme);

/** Unregisters a theme and releases all the resources held by it. */
void hedit_unregister_theme(HEdit* hedit, const char* name);

/** Selectes a new theme and redraws the whole UI. */
bool hedit_switch_theme(HEdit* hedit, const char* name);


/** Registers a new option. */
bool hedit_option_register(HEdit* hedit, const char* name, enum OptionType type, const Value default_value,
                           bool (*on_change)(HEdit*, Option*, const Value*));

/** Changes the value of an option. */
bool hedit_option_set(HEdit* hedit, const char* name, const char* newvalue);


/** Switches to the given view. */
void hedit_switch_view(HEdit* hedit, enum Views v);

#endif