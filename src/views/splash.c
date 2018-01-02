#include <tickit.h>

#include "core.h"

static void on_draw(HEdit* hedit, TickitWindow* win, TickitExposeEventInfo* e) {

    tickit_renderbuffer_text_at(e->rb, 0, 0, "Welcome to HEdit!");

}

static View definition = {
    .id = HEDIT_VIEW_SPLASH,
    .name = "Splash",
    .on_draw = on_draw
};

REGISTER_VIEW(HEDIT_VIEW_SPLASH, definition)