#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <tickit.h>

#include "core.h"
#include "log.h"
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
    TickitTerm* term = tickit_term_new();
    if (term == NULL) {
        log_fatal("Cannot initialize libtickit: %s.", strerror(errno));
        return 1;
    }

    // Configure the tickit terminal
    tickit_term_set_input_fd(term, STDIN_FILENO);
    tickit_term_set_output_fd(term, STDOUT_FILENO);
    tickit_term_await_started(term, &(const struct timeval){ .tv_sec = 0, .tv_usec = 50000 });
    tickit_term_setctl_int(term, TICKIT_TERMCTL_ALTSCREEN, 1);
    tickit_term_setctl_int(term, TICKIT_TERMCTL_KEYPAD_APP, 0);
    tickit_term_setctl_int(term, TICKIT_TERMCTL_MOUSE, TICKIT_TERM_MOUSEMODE_OFF);
    tickit_term_clear(term);

    // Initialize a new global state
    HEdit* hedit = hedit_core_init(&options, term);
    if (hedit == NULL) {
        return 1;
    }

    // Main input loop
    while (!hedit->exit) {
        tickit_term_input_wait(term, NULL);
    }

    // Tear down everything
    int exitcode = hedit->exitcode;
    hedit_core_teardown(hedit);
    tickit_term_destroy(term);
    log_teardown();
    return exitcode;

}