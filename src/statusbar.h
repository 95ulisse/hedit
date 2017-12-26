#ifndef __STATUSBAR_H__
#define __STATUSBAR_H__

#include "core.h"

/** Opaque Statusbar type */
typedef struct Statusbar Statusbar;

/**
 * Initializes a new instance of the statusbar component.
 */
Statusbar* hedit_statusbar_init(HEdit* hedit);

/**
 * Releases all the resources held by the given statusbar instance.
 */
void hedit_statusbar_teardown(Statusbar* statusbar);

/**
 * Notifies the statusbar of the new dimensions of the terminal.
 * @param lines New number of lines available.
 * @param cols New number of columns available.
 */
void hedit_statusbar_on_resize(Statusbar* statusbar, int lines, int cols);

#endif