#ifndef __FORMAT_H__
#define __FORMAT_H__

#include <stdlib.h>
#include "core.h"
#include "file.h"


#define MAX_SEGMENT_NAME_LEN 256

/** A segment of bytes with a specific meaning. */
typedef struct {
    const char* name;
    size_t from;
    size_t to;
    int color;
} FormatSegment;


#ifdef __cplusplus

#include <v8.h>

// Forward declaration
class JsFormatIterator;

/** Holds a reference to a JS array describing the segments making up a format. */
class JsFormat {
public:
    JsFormat(v8::Isolate* isolate, v8::Local<v8::Context> ctx, v8::Local<v8::Array> arr)
        : _isolate(isolate),
          _ctx(isolate, ctx),
          _arr(isolate, arr) {}

    ~JsFormat() {
        _ctx.Reset();
        _arr.Reset();
    }

    JsFormatIterator* Iterator(size_t from);

    uint32_t GetSegmentsCount() {
        v8::HandleScope handle_scope(_isolate);
        return _arr.Get(_isolate)->Length();
    }

    v8::Local<v8::Object> GetSegment(uint32_t index) {
        v8::EscapableHandleScope handle_scope(_isolate);
        v8::Local<v8::Value> val = _arr.Get(_isolate)->Get(_ctx.Get(_isolate), index).ToLocalChecked();
        return handle_scope.Escape(v8::Local<v8::Object>::Cast(val));
    }

private:
    v8::Isolate* _isolate;
    v8::Persistent<v8::Context> _ctx;
    v8::Persistent<v8::Array> _arr;
};

/** Simple iterator over the array of JS object held inside a JsFormat. */
class JsFormatIterator {
public:
    JsFormatIterator(v8::Isolate* isolate, JsFormat* format, uint32_t index)
        : _isolate(isolate),
          _format(format),
          _index(index)
    {
        _current.name = (const char*) &_currentName;
    }

    FormatSegment* Next();
    FormatSegment* Current();

private:
    v8::Isolate* _isolate;
    JsFormat* _format;
    FormatSegment _current;
    char _currentName[MAX_SEGMENT_NAME_LEN];
    uint32_t _index;
    bool _initialized = false;
};

#endif


/**
 * A Format holds information about the binary structure of a file.
 * This information is used to highlight some parts of the binary file,
 * as well as providing some kind of "guidance" while navigating the document
 * with a bit of help text.
 *
 * Actual format descriptions are implemented in JavaScript.
 * See all the files in src/js/format.
 */
#ifdef __cplusplus
typedef JsFormat Format;
#else
typedef void Format;
#endif

/** Iterator over all the format segments. */
#ifdef __cplusplus
typedef JsFormatIterator FormatIterator;
#else
typedef void FormatIterator;
#endif


#ifdef __cplusplus
extern "C" {
#endif

/** Sets a specific file format and redraws the UI. */
void hedit_set_format(HEdit* hedit, Format* format);

/**
 * Tries to automatically guess the format of the current file.
 * If the guess fails, sets the empty format.
 */
void hedit_format_guess(HEdit* hedit);

/** Releases all the resources held by this format. */
void hedit_format_free(Format* format);

/** Starts a new iterator from the segment including the `from` byte. */
FormatIterator* hedit_format_iter(Format* format, size_t from);

/** Returns the current segment without advancing the iterator. */
FormatSegment* hedit_format_iter_current(FormatIterator* it);

/**
 * Advances the iterator to the next available segment.
 * If no more segments are available, `NULL` is returned.
 */
FormatSegment* hedit_format_iter_next(FormatIterator* it);

/** Releases all the resources held by the given iterator. */
void hedit_format_iter_free(FormatIterator* it);


#ifdef __cplusplus
}
#endif

#endif
