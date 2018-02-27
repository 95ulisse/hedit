// This file includes an empty implementation of the format API
// to use only when compilation with V8 is disabled.

#include "format.h"

void hedit_set_format(HEdit* hedit, Format* format) {
    hedit_redraw_view(hedit);
}

void hedit_format_guess(HEdit* hedit) {
}

void hedit_format_free(Format* format) {
}

FormatIterator* hedit_format_iter(Format* format, size_t from) {
    return NULL;
}

FormatSegment* hedit_format_iter_current(FormatIterator* it) {
    return NULL;
}

FormatSegment* hedit_format_iter_next(FormatIterator* it) {
    return NULL;
}

void hedit_format_iter_free(FormatIterator* it) {
}
