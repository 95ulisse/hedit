#ifndef __FORMAT_H__
#define __FORMAT_H__

#include <stdlib.h>
#include "build-config.h"


#define MAX_SEGMENT_NAME_LEN 256

/** A segment of bytes with a specific meaning. */
typedef struct {
    const char* name;
    size_t from;
    size_t to;
    int color;
} FormatSegment;


#if defined(__cplusplus) && defined(WITH_V8)

#include <v8.h>

// Forward declaration
class JsFormatIterator;

/** Holds a reference to a JS array describing the segments making up a format. */
class JsFormat {
public:
    JsFormat(v8::Isolate* isolate, v8::Local<v8::Context> ctx, v8::Local<v8::Object> obj)
        : _isolate(isolate),
          _ctx(isolate, ctx),
          _obj(isolate, obj) {}

    ~JsFormat() {
        _ctx.Reset();
        _obj.Reset();
    }

    JsFormatIterator* Iterator();

private:
    v8::Isolate* _isolate;
    v8::Persistent<v8::Context> _ctx;
    v8::Persistent<v8::Object> _obj;
};

/** Simple iterator over the JS object held inside a JsFormat. */
class JsFormatIterator {
public:
    JsFormatIterator(v8::Isolate* isolate, v8::Local<v8::Object> jsIterator);

    ~JsFormatIterator() {
        _jsIterator.Reset();
        _nextFunction.Reset();
        _seekFunction.Reset();
    }

    FormatSegment* Seek(size_t pos);
    FormatSegment* Next();
    FormatSegment* Current();

private:
    v8::Isolate* _isolate;
    v8::Persistent<v8::Object> _jsIterator;
    v8::Persistent<v8::Function> _nextFunction;
    v8::Persistent<v8::Function> _seekFunction;
    FormatSegment _current;
    char _currentName[MAX_SEGMENT_NAME_LEN];
    bool _initialized = false;
    bool _done = false;

    bool AdvanceIterator();
    void UnpackJsSegment(v8::Local<v8::Object> seg);
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
#if defined(__cplusplus) && defined(WITH_V8)
typedef JsFormat Format;
#else
typedef void Format;
#endif

/** Iterator over all the format segments. */
#if defined(__cplusplus) && defined(WITH_V8)
typedef JsFormatIterator FormatIterator;
#else
typedef void FormatIterator;
#endif


#include "core.h"
#include "file.h"


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
FormatIterator* hedit_format_iter(Format* format);

/** Advances the iterator up to `pos` bytes. */
FormatSegment* hedit_format_iter_seek(FormatIterator* it, size_t pos);

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
