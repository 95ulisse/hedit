#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <tickit.h>

#include "actions.h"
#include "core.h"
#include "util/log.h"
#include "options.h"

int main(int argc, char** argv) {

    // Init the logging framework as soon as possible
    log_init();

    // Parse the cli options
    Options options;
    if (!options_parse(&options, argc, argv)) {
        return 1;
    }

    // Exit immediately if help or version options have been used
    if (options.show_help || options.show_version) {
        return 0;
    }

    // Initialize libtickit
    log_debug("Initializing libtickit.");
    Tickit* tickit = tickit_new_stdio();
    if (tickit == NULL) {
        log_fatal("Cannot initialize libtickit: %s.", strerror(errno));
        return 1;
    }

    // Initialize default actions and keybindings
    if (!hedit_init_actions()) {
        log_fatal("Cannot initialize default actions and bindings.");
        return 1;
    }

    // Initialize a new global state
    HEdit* hedit = hedit_core_init(&options, tickit_get_rootwin(tickit));
    if (hedit == NULL) {
        return 1;
    }

    // Main input loop
    tickit_run(tickit);

    // Tear down everything
    int exitcode = hedit->exitcode;
    hedit_core_teardown(hedit);
    tickit_unref(tickit);
    log_teardown();
    return exitcode;

}