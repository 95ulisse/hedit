#ifndef __STATUSBAR_H__
#define __STATUSBAR_H__

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif


/** Opaque Statusbar type */
typedef struct Statusbar Statusbar;

/** Initializes a new instance of the statusbar component. */
Statusbar* hedit_statusbar_init(HEdit* hedit);

/** Releases all the resources held by the given statusbar instance. */
void hedit_statusbar_teardown(Statusbar* statusbar);

/** Forces a redraw of the statusbar. */
void hedit_statusbar_redraw(Statusbar* statusbar);

/** Shows a custom message on the statusbar. */
void hedit_statusbar_show_message(Statusbar* statusbar, bool sticky, const char* msg);


#ifdef __cplusplus
}
#endif

#endif