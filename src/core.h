#ifndef __CORE_H__
#define __CORE_H__

#include <stdbool.h>
#include <tickit.h>

#include "options.h"
#include "statusbar.h"



// Some constants
#define HEDIT_VERSION "0.1.0"



/**
 * Global state of the editor.
 * Contains eveything needed to describe the precise state of HEdit:
 * from the opened file, to the mode we are in, to the current statusbar text.
 */
typedef struct {

    // Components
    Options* options;
    Statusbar* statusbar;

    // UI
    TickitTerm* term;
    TickitWindow* rootwin;

    // Exit flag and exit code
    bool exit;
    int exitcode;

    // Event registration ids
    int on_resize_ev_id;

} HEdit;



/**
 * Initializes a new global state.
 * This function should be called only once at the beginning of the program.
 * 
 * @return The newly created global state, or NULL in case of error.
 */
HEdit* hedit_core_init(Options* options, TickitTerm* term);

/**
 * Releases all the resources associated with the given state.
 * This function should be called right before program exit.
 */
void hedit_core_teardown(HEdit* hedit);

#endif