#include <stdlib.h>
#include <stdbool.h>
#include <tickit.h>

#include "log.h"
#include "core.h"
#include "statusbar.h"

struct Statusbar {
    HEdit* hedit;
};

Statusbar* hedit_statusbar_init(HEdit* hedit) {

    // Allocate space
    Statusbar* statusbar = calloc(1, sizeof(Statusbar));
    if (statusbar == NULL) {
        log_fatal("Cannot allocate memory for statusbar.");
        return NULL;
    }
    statusbar->hedit = hedit;

    tickit_term_goto(hedit->term, 0, 0);
    tickit_term_print(hedit->term, "Status line");
    tickit_term_goto(hedit->term, 1, 0);
    tickit_term_print(hedit->term, ":Command line");

    return statusbar;
}

void hedit_statusbar_teardown(Statusbar* statusbar) {
    
    if (statusbar == NULL) {
        return;
    }

    free(statusbar);

}

void hedit_statusbar_on_resize(Statusbar* statusbar, int lines, int cols) {

    // The statusbar has height 2, and must always be located at the bottom of the screen

}