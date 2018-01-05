#ifndef __FILE_H__
#define __FILE_H__

#include <stdlib.h>

/** Opaque structure representing an open file. */
typedef struct File File;

/** Opens the given file. */
File* hedit_file_open(const char* path);

/** Closes an open file and releases all the resources held. */
void hedit_file_close(File*);

/** Saves the file back to disk. */
bool hedit_file_save(File*);

/** Returns the name associated with the given file. */
const char* hedit_file_name(File*);

/** Changes the name associated with the given file. */
bool hedit_file_set_name(File*, const char*);

/** Returns the size of an opened file. */
size_t hedit_file_size(File*);

/** Returns whether this file is read only or not. */
bool hedit_file_is_ro(File*);

/** Returns whether this file has been modified or not. */
bool hedit_file_is_dirty(File*);

/** Read a single byte at the given offset. */
bool hedit_file_read_byte(File*, size_t offset, unsigned char* byte);

/** Writes a single byte at the given offset. */
bool hedit_file_write_byte(File*, size_t offset, unsigned char byte);

/**
 * Visits a portion of the contents of this file.
 * The visitor function may be called more than once with different parts of the file.
 */
void hedit_file_visit(File*, size_t start, size_t len, void (*visitor)(File*, size_t offset, const unsigned char* data, size_t len, void* user), void* user);

#endif