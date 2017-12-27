#ifndef __CORE_H__
#define __CORE_H__

#include <stdbool.h>
#include <tickit.h>

typedef struct HEdit HEdit;

#include "options.h"
#include "statusbar.h"
#include "util/map.h"



// Some constants
#define HEDIT_VERSION "0.1.0"



enum Modes {
    HEDIT_MODE_NORMAL,
    HEDIT_MODE_OVERWRITE,
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
    Map* bindings;
    void (*on_enter)(HEdit* hedit, Mode* prev);
    void (*on_exit)(HEdit* hedit, Mode* next);
    void (*on_input)(HEdit* hedit, const char* key);
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

    // UI
    TickitWindow* rootwin;
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

#endif