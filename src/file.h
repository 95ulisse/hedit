#ifndef __FILE_H__
#define __FILE_H__

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


/** Opaque structure representing an open file. */
typedef struct File File;

enum FileSaveMode {
    SAVE_MODE_AUTO = 0,
    SAVE_MODE_ATOMIC,
    SAVE_MODE_INPLACE
};

/** Opens the given file. Pass NULL to create an empty file. */
File* hedit_file_open(const char* path);

/** Closes an open file and releases all the resources held. */
void hedit_file_close(File*);

/** Saves the file back to disk. */
bool hedit_file_save(File*, const char* path, enum FileSaveMode);

/** Returns the name associated with the given file. */
const char* hedit_file_name(File*);

/** Returns the size of an opened file. */
size_t hedit_file_size(File*);

/** Returns whether this file is read only or not. */
bool hedit_file_is_ro(File*);

/** Returns whether this file has been modified or not. */
bool hedit_file_is_dirty(File*);

/** Inserts a string at the given offset. */
bool hedit_file_insert(File*, size_t offset, const unsigned char* data, size_t len);

/** Deletes a range of bytes. */
bool hedit_file_delete(File*, size_t offset, size_t len);

/** Replaces a string with another. */
bool hedit_file_replace(File*, size_t offset, const unsigned char* data, size_t len);

/** Commits any pending change in a new revision, snapshotting the current file status. */
bool hedit_file_commit_revision(File*);

/** Undoes a recent modification. `*pos` contains the location of the last change, if the file changed. */
bool hedit_file_undo(File*, size_t* pos);

/** Redoes an undone modification. `*pos` contains the location of the last change, if the file changed. */
bool hedit_file_redo(File*, size_t* pos);

/** Reads a single byte from the file. */
bool hedit_file_read_byte(File*, size_t offset, unsigned char* out);

/**
 * Visits a portion of the contents of this file.
 * The visitor function may be called more than once with different parts of the file.
 */
bool hedit_file_visit(File*, size_t start, size_t len, bool (*visitor)(File*, size_t offset, const unsigned char* data, size_t len, void* user), void* user);


#ifdef __cplusplus
}
#endif

#endif