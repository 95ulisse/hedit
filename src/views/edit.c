#include <assert.h>

#include "core.h"
#include "file.h"

static bool on_enter(HEdit* hedit, View* old) {
    assert(hedit->file != NULL);
    return true;
}

static void print_file(File* f, size_t offset, const char* data, size_t len, void* user) {
    TickitRenderBuffer* rb = user;
    
    // This is just to see something on the screen
    int line = 0;
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '\n') {
            tickit_renderbuffer_goto(rb, line++, 0);
        } else {
            tickit_renderbuffer_char(rb, data[i]);
        }
    }

}

static void on_draw(HEdit* hedit, TickitWindow* win, TickitExposeEventInfo* e) {

    tickit_renderbuffer_goto(e->rb, 0, 0);
    hedit_file_visit(hedit->file, print_file, e->rb);

}

static View definition = {
    .id = HEDIT_VIEW_EDIT,
    .name = "Edit",
    .on_enter = on_enter,
    .on_draw = on_draw
};

REGISTER_VIEW(HEDIT_VIEW_EDIT, definition)