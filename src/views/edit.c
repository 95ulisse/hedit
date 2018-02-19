#include <ctype.h>
#include <math.h>
#include <assert.h>

#include "core.h"
#include "file.h"
#include "format.h"
#include "util/common.h"
#include "util/log.h"
   

/**
 * Private state of the edit view.
 * Contains info about cursor and scrolling.
 */
typedef struct {
    size_t cursor_pos;
    bool left;
    size_t scroll_lines;
} ViewState;

static bool on_enter(HEdit* hedit, View* old) {
    assert(hedit->file != NULL);

    ViewState* s = calloc(1, sizeof(ViewState));
    if (s == NULL) {
        log_fatal("Out of memory");
        return false;
    }
    s->left = true;
    hedit->viewdata = s;

    return true;
}

static bool on_exit(HEdit* hedit, View* new) {
    free(hedit->viewdata);
    return true;
}

static void draw_bytes(HEdit* hedit, TickitRenderBuffer* rb, size_t padding, size_t colwidth,
                       size_t abs_offset, size_t window_offset, size_t cursor_pos /* absolute */, bool cursor_left,
                       const unsigned char* data, size_t len, FormatIterator* format_it)
{
    // Current format segment
    FormatSegment* seg = hedit_format_iter_current(format_it);

    // Array of all the colors for the format
    TickitPen* pens[] = {
        hedit->theme->white,
        hedit->theme->gray,
        hedit->theme->blue,
        hedit->theme->red,
        hedit->theme->pink,
        hedit->theme->green,
        hedit->theme->purple,
        hedit->theme->orange
    };

    // Draw for each byte both the hex and the ascii representation
    for (int i = 0; i < len; i++) {
        int line = (window_offset + i) / colwidth;
        int byte_col = padding + ((window_offset + i) % colwidth) * 3;
        int ascii_col = padding + colwidth * 3 + ((window_offset + i) % colwidth) + 2 /* Some breadth */;

        // Decide the color of the char depending on the highlighting data reported by the format
        TickitPen* pen = hedit->theme->text;
        if (seg != NULL) {
            size_t off = abs_offset + i;
            while (seg != NULL && off > seg->to) {
                seg = hedit_format_iter_next(format_it);
            }
            if (seg != NULL && off >= seg->from && off <= seg->to) {
                pen = pens[MIN(seg->color, sizeof(pens) / sizeof(pens[0]))];
            }
        }
        tickit_renderbuffer_setpen(rb, pen);

        if (abs_offset + i != cursor_pos) {
            tickit_renderbuffer_textf_at(rb, line, byte_col, "%02x", data[i]);
            tickit_renderbuffer_textf_at(rb, line, ascii_col, "%c", isprint(data[i]) ? data[i] : '.');
        } else {

            // The current byte is highlighted by the cursor
            
            char buf[3];
            snprintf(buf, 3, "%02x", data[i]);
            if (cursor_left) {
                tickit_renderbuffer_textf_at(rb, line, byte_col + 1, "%c", buf[1]);
                tickit_renderbuffer_setpen(rb, hedit->theme->block_cursor);
                tickit_renderbuffer_textf_at(rb, line, byte_col, "%c", buf[0]);
            } else {
                tickit_renderbuffer_textf_at(rb, line, byte_col, "%c", buf[0]);
                tickit_renderbuffer_setpen(rb, hedit->theme->block_cursor);
                tickit_renderbuffer_textf_at(rb, line, byte_col + 1, "%c", buf[1]);
            }

            tickit_renderbuffer_setpen(rb, hedit->theme->soft_cursor);
            tickit_renderbuffer_textf_at(rb, line, ascii_col, "%c", isprint(data[i]) ? data[i] : '.');
        }
    }

    // Since the cursor can be past the end, if we have just drawn the last portion of the file,
    // and the cursor is past the end, draw it explicitly
    if (hedit_file_size(hedit->file) == abs_offset + len && cursor_pos == abs_offset + len) {
        int line = (window_offset + len) / colwidth;
        int cursor_col = padding + ((window_offset + len) % colwidth) * 3;
        tickit_renderbuffer_setpen(rb, hedit->theme->block_cursor);
        tickit_renderbuffer_text_at(rb, line, cursor_col, " ");
    }

}

static void on_draw(HEdit* hedit, TickitWindow* win, TickitExposeEventInfo* e) {

    ViewState* state = hedit->viewdata;

    // Extract all the options needed for rendering
    size_t colwidth = ((Option*) map_get(hedit->options, "colwidth"))->value.i;
    bool lineoffset = ((Option*) map_get(hedit->options, "lineoffset"))->value.b;

    // Precompute the format for the line offset
    char lineoffset_format[10];
    int lineoffset_len = MAX(8, (int) floor(log(hedit_file_size(hedit->file)) / log(16)));
    snprintf(lineoffset_format, 10, "%%0%dx:", (unsigned char) lineoffset_len);

    // Set the normal pen for the text
    tickit_renderbuffer_setpen(e->rb, hedit->theme->text);
    tickit_renderbuffer_clear(e->rb);

    /**
     * We want to draw each line like this:
     * 
     * line off.                 data                                  ascii
     * |-------|-----------------------------------------------|  |--------------|
     * 00000000: aa aa aa aa aa aa aa aa aa aa aa aa aa aa aa aa  ................
     */

    int lines = tickit_window_lines(win);
    FileIterator* file_it = hedit_file_iter(hedit->file, state->scroll_lines * colwidth, colwidth * lines);
    FormatIterator* format_it = hedit_format_iter(hedit->format, state->scroll_lines * colwidth);

    // Iterate over the portion of the file we have to draw
    size_t off = 0; // Relative to the first byte to draw
    const unsigned char* data;
    size_t len;
    while (hedit_file_iter_next(file_it, &data, &len)) {
        draw_bytes(hedit, e->rb, lineoffset ? lineoffset_len + 2 : 0, colwidth,
                   off + state->scroll_lines * colwidth, off, state->cursor_pos, state->left,
                   data, len, format_it);
        off += len;
    }
    
    hedit_format_iter_free(format_it);
    hedit_file_iter_free(file_it);

    // Draw the line offsets
    if (lineoffset) {
        int used_lines = hedit_file_size(hedit->file) / colwidth + 1;
        tickit_renderbuffer_setpen(e->rb, hedit->theme->linenos);
        for (int i = 0; i < MIN(used_lines, lines); i++) {
            tickit_renderbuffer_textf_at(e->rb, i, 0, lineoffset_format, (i + state->scroll_lines) * colwidth);
        }
    }

    // Fill the remaining lines with `~`
    int emptylines = lines - (hedit_file_size(hedit->file) / colwidth + 1);
    if (emptylines > 0) {
        tickit_renderbuffer_setpen(e->rb, hedit->theme->linenos);
        for (int i = 1; i <= emptylines; i++) {
            tickit_renderbuffer_text_at(e->rb, lines - i, 0, "~");
        }
    }

}

static void on_movement(HEdit* hedit, enum Movement m, size_t arg) {
    ViewState* state = hedit->viewdata;
    size_t colwidth = ((Option*) map_get(hedit->options, "colwidth"))->value.i;
    size_t pagesize = colwidth * tickit_window_lines(hedit->viewwin);

    switch (m) {
        case HEDIT_MOVEMENT_LEFT:
            if (!state->left) {
                state->left = true;
            } else if (state->cursor_pos > 0) {
                state->cursor_pos--;
                state->left = false;
            }
            break;
        case HEDIT_MOVEMENT_RIGHT:
            if (state->left) {
                state->left = false;
            } else if (state->cursor_pos < hedit_file_size(hedit->file)) {
                state->cursor_pos++;
                state->left = true;
            }
            break;
        case HEDIT_MOVEMENT_UP:
            if (state->cursor_pos > colwidth - 1) {
                state->cursor_pos = state->cursor_pos - colwidth;
            }
            break;
        case HEDIT_MOVEMENT_DOWN:
            if (state->cursor_pos + colwidth <= hedit_file_size(hedit->file)) {
                state->cursor_pos = state->cursor_pos + colwidth;
            }
            break;
        case HEDIT_MOVEMENT_LINE_START:
            state->cursor_pos -= state->cursor_pos % colwidth;
            state->left = true;
            break;
        case HEDIT_MOVEMENT_LINE_END:
            state->cursor_pos = MIN(state->cursor_pos + colwidth - (state->cursor_pos % colwidth) - 1, hedit_file_size(hedit->file));
            state->left = false;
            break;
        case HEDIT_MOVEMENT_PAGE_UP:
            if (state->cursor_pos >= pagesize) {
                state->cursor_pos -= pagesize;
            } else {
                state->cursor_pos = 0;
            }
            break;
        case HEDIT_MOVEMENT_PAGE_DOWN:
            if (state->cursor_pos + pagesize < hedit_file_size(hedit->file)) {
                state->cursor_pos += pagesize;
            } else {
                state->cursor_pos = hedit_file_size(hedit->file);
            }
            break;
        case HEDIT_MOVEMENT_ABSOLUTE:
            state->cursor_pos = MIN(arg, hedit_file_size(hedit->file));
            state->left = true;
            break;

        default:
            log_warn("Unknown movement: %d", m);
            return;
    }

    // If the cursor went beyond the last byte, keep it always on the left
    if (state->cursor_pos == hedit_file_size(hedit->file)) {
        state->left = true;
    }

    // Scroll so that the cursor is always visible
    int windowlines = tickit_window_lines(hedit->viewwin);
    int cursor_line = state->cursor_pos / colwidth;
    if (cursor_line < state->scroll_lines) {
        state->scroll_lines = cursor_line;
    } else if (cursor_line > state->scroll_lines + windowlines - 1) {
        state->scroll_lines = cursor_line - windowlines + 1;
    }

    // Ask the current format for a description to show on the statusbar
    FormatIterator* format_it = hedit_format_iter(hedit->format, state->cursor_pos);
    FormatSegment* seg = hedit_format_iter_next(format_it);
    if (seg != NULL && state->cursor_pos >= seg->from && state->cursor_pos <= seg->to) {
        hedit_statusbar_show_message(hedit->statusbar, false, seg->name);
    }
    hedit_format_iter_free(format_it);

    hedit_redraw_view(hedit);
}

static void on_input(HEdit* hedit, const char* key, bool replace) {

    // Accept only hex digits
    int keyvalue = -1;
    if (!str2int(key, 16, &keyvalue)) {
        return;
    }

    ViewState* state = hedit->viewdata;

    if (replace || state->left == false) {

        // Update the digit under the cursor
        unsigned char byte = 0;
        if (!hedit_file_read_byte(hedit->file, state->cursor_pos, &byte)) {
            return;
        }
        if (state->left) {
            byte = 16 * keyvalue + (byte % 16);
        } else {
            byte = (byte - (byte % 16)) + keyvalue;
        }
        if (!hedit_file_replace(hedit->file, state->cursor_pos, &byte, 1)) {
            return;
        }

    } else {

        // Add a new byte at the left of the cursor
        unsigned char byte = 16 * keyvalue;
        if (!hedit_file_insert(hedit->file, state->cursor_pos, &byte, 1)) {
            return;
        }
    
    }

    // Move the cursor to the right
    on_movement(hedit, HEDIT_MOVEMENT_RIGHT, 0);

}

static void on_delete(HEdit* hedit, ssize_t count) {
    ViewState* state = hedit->viewdata;

    if (count == 0) {
        return;
    } else if (count < 0 && state->cursor_pos < hedit_file_size(hedit->file)) {

        // Delete to the right
        hedit_file_delete(hedit->file, state->cursor_pos, -count);

    } else {

        if (count == 1 && state->left == false) {
            // Set the first digit to 0
            unsigned char byte = 0;
            hedit_file_read_byte(hedit->file, state->cursor_pos, &byte);
            byte %= 16;
            if (hedit_file_replace(hedit->file, state->cursor_pos, &byte, 1)) {
                on_movement(hedit, HEDIT_MOVEMENT_LEFT, 0);
            }
        } else {
            // Delete to the left
            ssize_t start = ((ssize_t) state->cursor_pos) - count;
            if (start < 0) {
                count += start;
            }
            if (hedit_file_delete(hedit->file, start, count)) {
                on_movement(hedit, HEDIT_MOVEMENT_LEFT, 0);
                state->left = true; // Always keep on the left
            }
        }
        
    }

    hedit_redraw_view(hedit);    
}

static View definition = {
    .id = HEDIT_VIEW_EDIT,
    .name = "edit",
    .on_enter = on_enter,
    .on_exit = on_exit,
    .on_draw = on_draw,
    .on_input = on_input,
    .on_movement = on_movement,
    .on_delete = on_delete
};

REGISTER_VIEW(HEDIT_VIEW_EDIT, definition)