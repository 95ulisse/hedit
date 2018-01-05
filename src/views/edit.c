#include <ctype.h>
#include <math.h>
#include <assert.h>

#include "core.h"
#include "file.h"
#include "util/common.h"
#include "util/log.h"

// Some defines useful to shrind the drawing code

#define WITH_CURSOR_PEN(block) \
    do { \
        tickit_renderbuffer_savepen(s->rb); \
        tickit_renderbuffer_setpen(s->rb, s->hedit->theme->cursor); \
        block; \
        tickit_renderbuffer_restore(s->rb); \
    } while (false);

/**
 * Private state of the edit view.
 * Contains info about cursor and scrolling.
 */
typedef struct {
    size_t cursor_pos;
    size_t scroll_lines;
} ViewState;

static bool on_enter(HEdit* hedit, View* old) {
    assert(hedit->file != NULL);

    hedit->viewdata = calloc(1, sizeof(ViewState));
    if (hedit->viewdata == NULL) {
        log_fatal("Out of memory");
        return false;
    }

    return true;
}

static bool on_exit(HEdit* hedit, View* new) {
    free(hedit->viewdata);
    return true;
}

struct file_visitor_state {
    HEdit* hedit;
    ViewState* view_state;
    TickitRenderBuffer* rb;
    const char* line_offset_format;
    size_t line_offset_len;
    size_t colwidth;
    bool lineoffset;
    size_t nextbyte; // Next byte to draw
};

static inline size_t byte_to_line(struct file_visitor_state* s, size_t offset) {
    return offset / s->colwidth - s->view_state->scroll_lines;
}

static void file_visitor(File* f, size_t offset, const char* data, size_t len, void* user) {
    struct file_visitor_state* s = user;
    const int ascii_spacing = 2;

    assert(offset == s->nextbyte);
    size_t nextbyte = offset;

    /**
     * We want to draw each line like this:
     * 
     * line off.                 data                                  ascii
     * |-------|-----------------------------------------------|  |--------------|
     * 00000000: aa aa aa aa aa aa aa aa aa aa aa aa aa aa aa aa  ................
     */

    // Position the cursor to where the next byte will be drawn
    tickit_renderbuffer_goto(s->rb, byte_to_line(s, offset), 3 * (offset % s->colwidth) + (s->lineoffset ? s->line_offset_len : 0));

    int i;
    for (i = 0; i < len; i++) {

        // If we are at the beginning of a line, print the line offset
        if (nextbyte % s->colwidth == 0) {

            // Before beginning a new line, draw the ASCII chars for this line
            if (i > 0) {
                // Skip the chars printed in a previous segment, but only if we are drawing the first line
                bool isfirstline = (offset % s->colwidth) + i - 1 < s->colwidth;
                int skip = isfirstline ? (offset % s->colwidth) : 0;
                tickit_renderbuffer_goto(s->rb, byte_to_line(s, nextbyte) - 1,
                    3 * s->colwidth +                                // All the binary data
                    (s->lineoffset ? s->line_offset_len : 0) +       // Line offset
                    ascii_spacing +                                  // A bit of spacing
                    skip);
                for (int j = i - (isfirstline ? s->colwidth - (offset % s->colwidth) : s->colwidth); j < i; j++) {
                    if (s->view_state->cursor_pos == offset + j) {
                        WITH_CURSOR_PEN(tickit_renderbuffer_textf(s->rb, "%c", isprint(data[j]) ? data[j] : '.'));
                    } else {
                        tickit_renderbuffer_textf(s->rb, "%c", isprint(data[j]) ? data[j] : '.');
                    }
                }
            }

            tickit_renderbuffer_goto(s->rb, byte_to_line(s, nextbyte), 0);
            if (s->lineoffset) {
                tickit_renderbuffer_textf(s->rb, s->line_offset_format, nextbyte);
            }
        }

        tickit_renderbuffer_text(s->rb, " ");
        if (nextbyte == s->view_state->cursor_pos) {
            WITH_CURSOR_PEN(tickit_renderbuffer_textf(s->rb, "%02x", data[i]));
        } else {
            tickit_renderbuffer_textf(s->rb, "%02x", data[i]);
        }

        nextbyte++;
    }

    // Flush any non-printed ascii chars because of an interrupred line
    bool isfirstline = (offset % s->colwidth) + i - 1 < s->colwidth;
    int skip = isfirstline ? (offset % s->colwidth) : 0;
    int lastlinechars = MIN(((offset % s->colwidth) + len) % s->colwidth, len);
    tickit_renderbuffer_goto(s->rb, byte_to_line(s, nextbyte) - (lastlinechars == 0 ? 1 : 0),
        3 * s->colwidth +                                // All the binary data
        (s->lineoffset ? s->line_offset_len : 0) +       // Line offset
        ascii_spacing +                                  // A bit of spacing
        skip);
    for (int j = len - (lastlinechars == 0 ? MIN(len, s->colwidth) : lastlinechars); j < len; j++) {
        if (s->view_state->cursor_pos == offset + j) {
            WITH_CURSOR_PEN(tickit_renderbuffer_textf(s->rb, "%c", isprint(data[j]) ? data[j] : '.'));
        } else {
            tickit_renderbuffer_textf(s->rb, "%c", isprint(data[j]) ? data[j] : '.');
        }
    }

    s->nextbyte = nextbyte;
}

static void on_draw(HEdit* hedit, TickitWindow* win, TickitExposeEventInfo* e) {

    ViewState* state = hedit->viewdata;

    // Extract all the options needed for rendering
    size_t colwidth = ((Option*) map_get(hedit->options, "colwidth"))->value.i;
    bool lineoffset = ((Option*) map_get(hedit->options, "lineoffset"))->value.b;

    // Precompute the format for the line offset
    char line_offset_format[10];
    int line_offset_len = MAX(8, (int) floor(log(hedit_file_size(hedit->file)) / log(16)));
    sprintf(line_offset_format, "%%0%dx:", line_offset_len);

    // Set the normal pen for the text
    tickit_renderbuffer_setpen(e->rb, hedit->theme->text);

    int lines = tickit_window_lines(win);
    struct file_visitor_state visitor_state = {
        .hedit = hedit,
        .view_state = state,
        .rb = e->rb,
        .line_offset_format = line_offset_format,
        .line_offset_len = line_offset_len + 1, // +1 for the ':'
        .colwidth = colwidth,
        .lineoffset = lineoffset,
        .nextbyte = state->scroll_lines * colwidth
    };
    hedit_file_visit(hedit->file, visitor_state.nextbyte, colwidth * lines, file_visitor, &visitor_state);

    // Fill the remaining lines with `~`
    int emptylines = lines - (hedit_file_size(hedit->file) / colwidth + 1);
    if (emptylines > 0) {
        for (int i = 1; i <= emptylines; i++) {
            tickit_renderbuffer_text_at(e->rb, lines - i, 0, "~");
        }
    }

}

static void on_movement(HEdit* hedit, enum Movement m) {
    ViewState* state = hedit->viewdata;
    size_t colwidth = ((Option*) map_get(hedit->options, "colwidth"))->value.i;
    size_t pagesize = colwidth * tickit_window_lines(hedit->viewwin);

    switch (m) {
        case HEDIT_MOVEMENT_LEFT:
            if (state->cursor_pos > 0) {
                state->cursor_pos--;
            }
            break;
        case HEDIT_MOVEMENT_RIGHT:
            if (state->cursor_pos < hedit_file_size(hedit->file) - 1) {
                state->cursor_pos++;
            }
            break;
        case HEDIT_MOVEMENT_UP:
            if (state->cursor_pos > colwidth - 1) {
                state->cursor_pos = state->cursor_pos - colwidth;
            }
            break;
        case HEDIT_MOVEMENT_DOWN:
            if (state->cursor_pos + colwidth < hedit_file_size(hedit->file)) {
                state->cursor_pos = state->cursor_pos + colwidth;
            }
            break;
        case HEDIT_MOVEMENT_LINE_START:
            state->cursor_pos -= state->cursor_pos % colwidth;
            break;
        case HEDIT_MOVEMENT_LINE_END:
            state->cursor_pos = MIN(state->cursor_pos + colwidth - (state->cursor_pos % colwidth) - 1, hedit_file_size(hedit->file) - 1);
            break;
        case HEDIT_MOVEMENT_PAGE_UP:
            if (state->cursor_pos >= pagesize) {
                state->cursor_pos -= pagesize;
            } else {
                state->cursor_pos = 0;
            }
            break;
        case HEDIT_MOVEMENT_PAGE_DOWN:
            if (state->cursor_pos + pagesize < hedit_file_size(hedit->file) - 1) {
                state->cursor_pos += pagesize;
            } else {
                state->cursor_pos = hedit_file_size(hedit->file) - 1;
            }
            break;

        default:
            log_warn("Unknown movement: %d", m);
            return;
    }

    // Scroll so that the cursor is always visible
    int windowlines = tickit_window_lines(hedit->viewwin);
    int cursor_line = state->cursor_pos / colwidth;
    if (cursor_line < state->scroll_lines) {
        state->scroll_lines = cursor_line;
    } else if (cursor_line > state->scroll_lines + windowlines - 1) {
        state->scroll_lines = cursor_line - windowlines + 1;
    }

    hedit_redraw_view(hedit);
}

static View definition = {
    .id = HEDIT_VIEW_EDIT,
    .name = "Edit",
    .on_enter = on_enter,
    .on_exit = on_exit,
    .on_draw = on_draw,
    .on_movement = on_movement
};

REGISTER_VIEW(HEDIT_VIEW_EDIT, definition)