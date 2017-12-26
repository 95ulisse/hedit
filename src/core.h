#ifndef __CORE_H__
#define __CORE_H__

#include <stdbool.h>
#include <tickit.h>

typedef struct HEdit HEdit;

#include "options.h"
#include "statusbar.h"



// Some constants
#define HEDIT_VERSION "0.1.0"



/**
 * Global state of the editor.
 * Contains eveything needed to describe the precise state of HEdit:
 * from the opened file, to the mode we are in, to the current statusbar text.
 */
struct HEdit {

    // Components
    Options* options;
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

#endif