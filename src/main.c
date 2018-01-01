#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <tickit.h>

#include "core.h"
#include "actions.h"
#include "commands.h"
#include "options.h"
#include "util/log.h"
#include "util/event.h"

static int on_tickit_ready(Tickit *t, TickitEventFlags flags, void *user) {
    HEdit* hedit = user;
    event_fire(&hedit->ev_load, hedit);
    return 1;
}

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

    // Initialize default commands
    if (!hedit_init_commands()) {
        log_fatal("Cannot initialize default commands.");
        return 1;
    }

    // Initialize a new global state
    HEdit* hedit = hedit_core_init(&options, tickit);
    if (hedit == NULL) {
        return 1;
    }

    // Fire the load event as soon as everything is ready
    tickit_later(tickit, 0, on_tickit_ready, hedit);

    // Main input loop
    tickit_run(tickit);

    // Fire the quit event
    event_fire(&hedit->ev_quit, hedit);

    // Tear down everything
    int exitcode = hedit->exitcode;
    hedit_core_teardown(hedit);
    tickit_unref(tickit);
    log_teardown();
    return exitcode;

}