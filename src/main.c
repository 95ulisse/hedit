#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <tickit.h>

#include "core.h"
#include "actions.h"
#include "commands.h"
#include "options.h"
#include "js.h"
#include "util/log.h"
#include "util/event.h"

static sigjmp_buf sigint_jmpbuf;

static void on_sigint(int signo) {
    siglongjmp(sigint_jmpbuf, 1);
}

static int do_register_sigint(Tickit* t, TickitEventFlags flags, void* user) {
    
    // Register an handler for sigint
    struct sigaction sa = { 0 };
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        log_fatal("Cannot register SIGINT handler.");
        return 0;
    }

    return 1;
}

static int do_open_file(Tickit *t, TickitEventFlags flags, void *user) {
    HEdit* hedit = user;
    
    const char* path = hedit->cli_options->file;
    tickit_term_input_push_bytes(tickit_get_term(t), ":edit ", 6);
    tickit_term_input_push_bytes(tickit_get_term(t), path, strlen(path));
    
    hedit_emit_keys(hedit, "<Enter>");

    return 1;
}

static int on_tickit_ready(Tickit *t, TickitEventFlags flags, void *user) {
    HEdit* hedit = user;
    hedit_js_load_user_config(hedit);
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

    // Initialize V8
    if (!hedit_js_init(argc, argv)) {
        log_fatal("Cannot initialize V8.");
        return 1;
    }

    // Initialize a new global state
    HEdit* hedit = hedit_core_init(&options, tickit);
    if (hedit == NULL) {
        return 1;
    }

    // Fire the load event as soon as everything is ready
    tickit_later(tickit, 0, do_register_sigint, hedit);
    tickit_later(tickit, 0, on_tickit_ready, hedit);

    // If a file name has been passed on the command line, issue an :edit command
    if (options.file != NULL) {
        tickit_later(tickit, 0, do_open_file, hedit);
    }

    if (sigsetjmp(sigint_jmpbuf, true) == 1) {
        // We ended up here because of a SIGINT, so translate it to a C-c
        TickitKeyEventInfo e = {
            .type = TICKIT_KEYEV_KEY,
            .mod = TICKIT_MOD_CTRL,
            .str = "C-c"
        };
        tickit_term_emit_key(tickit_get_term(tickit), &e);
        hedit_redraw(hedit);

        // Re-register the signal handler because the `tickit_run` will override it again
        tickit_later(tickit, 0, do_register_sigint, hedit);
    }

    // Main input loop
    tickit_run(tickit);

    // Fire the quit event
    event_fire(&hedit->ev_quit, hedit);

    // Tear down everything
    int exitcode = hedit->exitcode;
    hedit_core_teardown(hedit);
    hedit_js_teardown();
    tickit_unref(tickit);
    log_teardown();
    return exitcode;

}