#ifndef __JS_H__
#define __JS_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initializer for V8. */
bool hedit_js_init(int argc, char** argv);

/** Releases all the resources held by V8. */
void hedit_js_teardown();

#ifdef __cplusplus
}
#endif

#endif
