#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <stdlib.h>
#include <stdbool.h>

/**
 * Opaque data type for a gap buffer.
 * 
 * A gap buffer is a simple data structure to perform
 * fast insert and delete of characters in the middle of a text.
 * 
 * The core idea of a gap buffer is to have the text split
 * in two separate regions at the position of the cursor separated
 * by a region of unused space.
 * Insertions and deletions will always happen at the cursor position,
 * so they will always be a matter of advancing a pointer.
 */
typedef struct Buffer Buffer;

/** Creates a new buffer with the default size. */
Buffer* buffer_new();

/** Releases all the resources held by a buffer. */
void buffer_free(Buffer*);

/** Returns the total memory allocated by the buffer. */
size_t buffer_get_capacity(Buffer*);

/** Returns the length of the text stored in the buffer. */
size_t buffer_get_len(Buffer*);

/** Returns the position of the cursor in a buffer. */
size_t buffer_get_cursor(Buffer*);

/** Sets the absolute position of the cursor in the buffer. */
void buffer_set_cursor(Buffer*, size_t offset);

/** Moves the cursor of an offset relative to its current position. */
void buffer_move_cursor(Buffer*, int offset);

/** Inserts a new character at the current cursor position. */
bool buffer_put_char(Buffer*, char);

/** Inserts a whole NULL terminated string at the current cursor position. */
bool buffer_put_string(Buffer*, const char*);

/**
 * Deletes the given number of chars from the buffer from the current position.
 * If `count` is negative, the chars following the cursor are deleted.
 * 
 * A positive `count` acts like <Backspace>, while a negative one is like <Delete>.
 */
void buffer_del(Buffer*, int count);

/**
 * Visits the contents of the buffer.
 * The visitor can be called multiple times, and will always be invoked
 * with a NON NULL-terminated string representing a portion of the text contained
 * in the buffer.
 */
void buffer_visit(Buffer*, void (*visitor)(Buffer*, size_t pos, const char* str, size_t len, void*), void* user);

/**
 * Copies the content of this buffer to the given destination.
 * A NULL terminator will be added after the buffer contents.
 */
void buffer_copy_to(Buffer*, char*);

#endif