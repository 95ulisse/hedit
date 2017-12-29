#ifndef __CORE_H__
#define __CORE_H__

#include <stdbool.h>
#include <tickit.h>

typedef struct HEdit HEdit;

#include "options.h"
#include "statusbar.h"
#include "util/map.h"
#include "util/event.h"
#include "util/buffer.h"



/** Generic struct to pass a single argument to a callback function. */
typedef struct {
    int i;
    bool b;
} Arg;

/** Type of the functions that will be called when an action needs to be performed. */
typedef void (*ActionCallback)(HEdit* hedit, const Arg* arg);

typedef struct {
    ActionCallback cb;
    const Arg arg;
} Action;


enum Modes {
    HEDIT_MODE_NORMAL,
    HEDIT_MODE_OVERWRITE,
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
    TickitPen* cursor;

    // When adding a new field to this structure, remember to update the default theme
    // and the `free_theme` function in core.c.
};



/**
 * Global state of the editor.
 * Contains eveything needed to describe the precise state of HEdit:
 * from the opened file, to the mode we are in, to the current statusbar text.
 */
struct HEdit {

    Options* options;

    // Components
    Mode* mode;
    Statusbar* statusbar;
    Buffer* command_buffer;

    // Events
    Event ev_mode_switch;

    // UI
    TickitWindow* rootwin;
    Map* themes;
    Theme* theme;
    int on_keypress_bind_id;

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
HEdit* hedit_core_init(Options* options, TickitWindow* rootwin);

/**
 * Releases all the resources associated with the given state.
 * This function should be called right before program exit.
 */
void hedit_core_teardown(HEdit* hedit);

/** Switches the editor to a new mode. */
void hedit_switch_mode(HEdit* hedit, enum Modes m);

/** Registers a new theme. */
bool hedit_register_theme(HEdit* hedit, const char* name, Theme* theme);

/** Unregisters a theme and releases all the resources held by it. */
void hedit_unregister_theme(HEdit* hedit, const char* name);

/** Selectes a new theme and redraws the whole UI. */
bool hedit_switch_theme(HEdit* hedit, const char* name);

#endif