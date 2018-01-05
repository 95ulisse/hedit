#include <string.h>
#include <tickit.h>

#include "core.h"

static void on_draw(HEdit* hedit, TickitWindow* win, TickitExposeEventInfo* e) {

    // Draw a simple splash screen with the name and the instructions to get started

    const char* msg1 = "Welcome to HEdit!";
    const char* msg2 = "Type :o to open a file and get started.";
    const int msg1len = strlen(msg1);
    const int msg2len = strlen(msg2);
    const int paddingv = 1;
    const int paddingh = 3;

    TickitRect geom = tickit_window_get_geometry(win);
    int ccol = geom.cols / 2;
    int cline = geom.lines / 2;

    // Centered text
    int left = ccol - msg1len / 2;
    tickit_renderbuffer_text_at(e->rb, cline, left, msg1);

    // Bounding box
    tickit_renderbuffer_hline_at(e->rb, cline - paddingv - 1, left - paddingh - 1, left + msg1len + paddingh, TICKIT_LINE_SINGLE, 0);
    tickit_renderbuffer_hline_at(e->rb, cline + paddingv + 1, left - paddingh - 1, left + msg1len + paddingh, TICKIT_LINE_SINGLE, 0);
    tickit_renderbuffer_vline_at(e->rb, cline - paddingv - 1, cline + paddingv + 1, left - paddingh - 1, TICKIT_LINE_SINGLE, 0);
    tickit_renderbuffer_vline_at(e->rb, cline - paddingv - 1, cline + paddingv + 1, left + msg1len + paddingh, TICKIT_LINE_SINGLE, 0);

    // Text below
    left = ccol - msg2len / 2;
    tickit_renderbuffer_text_at(e->rb, cline + paddingv + 3, left, msg2);

}

static View definition = {
    .id = HEDIT_VIEW_SPLASH,
    .name = "Splash",
    .on_draw = on_draw
};

REGISTER_VIEW(HEDIT_VIEW_SPLASH, definition)