#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <libgen.h>
#include <assert.h>

#include "file.h"
#include "util/log.h"
#include "util/common.h"
#include "util/list.h"

/**
 * This is an implementation of a piece chain.
 * A piece chain is a data structure that allows fast insertion and deletion operations,
 * and allows for undo/redo and grouping of operations.
 *
 * Even though this is an hex editor, we need a way to provide undo and redo,
 * and this data structure works really well with text, which is just a bunch a bytes.
 * For the sake of simplicity, during the description of the structure, we will refer
 * to the contents of a file with the word "text", but the piece chain has nothing
 * that prevents it from being used to store non-textual data.
 *
 * The core idea is to keep the whole text in a linked list of pieces:
 *
 * #1                #2        #3
 * +----------+ ---> +--+ ---> +------+
 * |This is so|      |me|      | text!|
 * +----------+ <--- +--+ <--- +------+
 *
 * Evey time we make an insertion, we *replace* the pieces that we should modify with new ones.
 * Pieces are **immutable**, and they are always kept around for undo/redo support (look at the IDs).
 *
 * #1                #2        #4           #5                         #6
 * +----------+ ---> +--+ ---> +-----+ ---> +-------------------+ ---> +-+
 * |This is so|      |me|      | text|      |, and now some more|      |!|
 * +----------+ <--- +--+ <--- +-----+ <--- +-------------------+ <--- +-+
 *
 * This change is recorded as a pair of spans:
 * "Replace the span ranging from piece #2 to #3, with the span from #4 to #6."
 * To undo a change, just swap again the spans.
 *
 * When we start with a new file, we start with an empty piece list, but when we open an existing
 * file, we can mmap it and use its contents as the first piece of the chain. The region can be mapped
 * read-only, because we will never need to change it (the piece chain is immutable, remember).
 *
 * A final note on the management of the memory:
 * we need some kind of custom allocator, which is able to keep track of which block of memory has been
 * mmapped and which one has been allocated on the heap, so that we can free them appropriately.
 * A piece does not own the data, it only has a pointer *inside* a memory block.
 *
 * This is an example showing how a new insertion in the middle of an existing file is represented.
 *
 * +-----------------------------------+ ---> +--------+
 * | Original file contents (mmap R/O) |      |new text|                     Memory blocks
 * +-----------------------------------+ <--- +--------+
 * ^            ^                             ^
 * |            |                             |
 * |            +----------------------+      |
 * |                 +-----------------|------+
 * |                 |                 |
 * +----------+ ---> +----------+ ---> +----------+
 * | Piece #1 |      | Piece #2 |      | Piece #3 |                          Piece chain
 * +----------+ <--- +----------+ <--- +----------+
 *
 *
 *
 * Caching
 * =======
 *
 * While creating a piece is cheap, creating a piece for every single inserted char is a bit excessive,
 * so we make an exception to the rule: all memory blocks **except the last one** are immutable.
 * When we insert a new sequence of bytes, we append it to the last allocated memory block,
 * and then create a piece pointing to that new string.
 *
 *
 *
 * Undo / Redo
 * ===========
 *
 * This implementation keeps a linear undo history, which means that if you undo a revision and then
 * modify the text, then the redo history is discarded.
 *
 * The whole idea of spans + changes + revisions is to allow simple undoing of changes.
 * Changes are grouped in revisions, which are just a mean to undo a group of changes together
 * (think of the replacement of a char as a deletion followed by an insertion: we don't want to undo
 * the deletion and the insertion individually, but the replacement as a whole).
 *
 * The `all_revisions` list keeps track of all the revisions in the history of the file,
 * and the `current_revision` is a pointer to the actual active revision. Note that all the changes
 * the user makes to the file are first stored in the `pending_changes` list, and then committed
 * to a proper revision either when an undo is requested or manually with the function `hedit_file_commit_revision`.
 * At this point, the implementation of undo is just a matter of moving the `current_revision` pointer
 * and repply the changes in the old revision in reverse.
 *
 */

#define BLOCK_SIZE (1024 * 1024) /* 1MiB */

typedef struct {
    unsigned char* data;
    size_t size;
    size_t len;
    enum {
        BLOCK_MMAP,
        BLOCK_MALLOC
    } type;
    struct list_head list;
} Block;

typedef struct {
    unsigned char* data;
    size_t size;
    struct list_head global_list;
    struct list_head list; // This is not used as a list at all, so maybe we should not use a `list_head`.
} Piece;

typedef struct {
    Piece* start;
    Piece* end;
    size_t len;
} Span;

typedef struct {
    Span original;
    Span replacement;
    size_t pos;
    struct list_head list;
} Change;

typedef struct {
    struct list_head changes;
    struct list_head list;
} Revision;

struct File {
    char* name;
    bool ro;
    bool dirty;
    size_t size;

    struct list_head all_blocks; // List of all the blocks (for freeing)
    struct list_head all_pieces; // List of all the pieces (for freeing)
    struct list_head all_revisions; // File history

    struct list_head pieces; // Current active pieces

    Revision* current_revision; // Pointer to the current active revision
    struct list_head pending_changes; // Changes not yet attached to a revision
};

struct FileIterator {
    size_t max_off; // Maximum offset requested by the user
    size_t current_off;
    Piece* current_piece;
    const unsigned char* current_data;
    size_t current_len;
};


// Functions to manage blocks
static Block* block_alloc(File*, size_t);
static Block* block_alloc_mmap(File*, int fd, size_t size);
static void block_free(File*, Block*);
static bool block_can_fit(Block*, size_t len);
static unsigned char* block_append(Block*, const unsigned char* data, size_t len);

// Functions to manage pieces
static Piece* piece_alloc(File*);
static void piece_free(Piece*);
static bool piece_find(File*, size_t abs, Piece** piece, size_t* offset);

// Functions to manage spans and changes
static void span_init(Span*, Piece* start, Piece* end);
static void span_swap(File*, Span* original, Span* replacement);
static Change* change_alloc(File*, size_t pos);
static void change_free(Change*, bool free_pieces);

// Functions to manage revisions
static Revision* revision_alloc(File*);
static void revision_free(Revision*, bool free_pieces);
static bool revision_purge(File*);


static Block* block_alloc(File* file, size_t size) {
    
    Block* block = malloc(sizeof(Block));
    if (block == NULL) {
        log_fatal("Out of memory.");
        return NULL;
    }
    block->size = MAX(size, BLOCK_SIZE);
    block->len = 0;
    block->type = BLOCK_MALLOC;
    list_init(&block->list);

    block->data = malloc(sizeof(char) * block->size);
    if (block->data == NULL) {
        log_fatal("Out of memory.");
        free(block);
        return NULL;
    }

    // Add the created block to the list of all blocks for tracking
    list_add_tail(&file->all_blocks, &block->list);

    return block;

}

static Block* block_alloc_mmap(File* file, int fd, size_t size) {
    
    Block* block = malloc(sizeof(Block));
    if (block == NULL) {
        log_fatal("Out of memory.");
        return NULL;
    }
    block->size = size;
    block->len = size;
    block->type = BLOCK_MMAP;
    list_init(&block->list);

    block->data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (block->data == MAP_FAILED) {
        log_error("Cannot mmap: %s.", strerror(errno));
        return NULL;
    }

    // Add the created block to the list of all blocks for tracking
    list_add_tail(&file->all_blocks, &block->list);

    return block;
   
}

static void block_free(File* file, Block* block) {
    switch (block->type) {
        case BLOCK_MALLOC:
            free(block->data);
            break;
        case BLOCK_MMAP:
            munmap(block->data, block->size);
            break;
        default:
            abort();
    }
    list_del(&block->list);
    free(block);
}

static bool block_can_fit(Block* block, size_t n) {
    return block->size - block->len >= n;
}

static unsigned char* block_append(Block* block, const unsigned char* data, size_t len) {
    if (!block_can_fit(block, len)) {
        return NULL;
    }
    unsigned char* ptr = block->data + block->len;
    memmove(ptr, data, len);
    block->len += len;
    return ptr;
}

static Piece* piece_alloc(File* file) {
    Piece* piece = malloc(sizeof(Piece));
    if (piece == NULL) {
        log_fatal("Out of memory.");
        return NULL;
    }
    piece->data = NULL;
    piece->size = 0;
    list_init(&piece->list);
    list_init(&piece->global_list);

    list_add_tail(&file->all_pieces, &piece->global_list);

    return piece;
}

static void piece_free(Piece* piece) {
    list_del(&piece->global_list);
    free(piece);
}

static bool piece_find(File* file, size_t abs, Piece** piece, size_t* offset) {
    if (abs > file->size) {
        return false;
    }

    size_t abspos = 0;
    list_for_each_member(p, &file->pieces, Piece, list) {
        if (abs < abspos + p->size) {
            *piece = p;
            *offset = abs - abspos;
            return true;
        } else {
            abspos += p->size;
        }
    }

    return false;
}

static void span_init(Span* span, Piece* start, Piece* end) {
    span->start = start;
    span->end = end;
    if (start == NULL && end == NULL) {
        span->len = 0;
        return;
    }
    assert(start != NULL && end != NULL);

    list_for_each_interval(p, start, end, Piece, list) {
        span->len += p->size;
    }
}

static void span_swap(File* file, Span* original, Span* replacement) {
    if (original->len == 0 && replacement->len == 0) {
        return;
    } else if (original->len == 0) {
        // An insertion
        replacement->start->list.prev->next = &replacement->start->list;
        replacement->end->list.next->prev = &replacement->end->list;
    } else if (replacement->len == 0) {
        // A deletion
        original->start->list.prev->next = original->end->list.next;
        original->end->list.next->prev = original->start->list.prev;
    } else {
        original->start->list.prev->next = &replacement->start->list;
        original->end->list.next->prev = &replacement->end->list;
    }
    file->size -= original->len;
    file->size += replacement->len;
}

static Change* change_alloc(File* file, size_t pos) {
    Change* change = malloc(sizeof(Change));
    if (change == NULL) {
        log_fatal("Out of memory.");
        return NULL;
    }
    change->pos = pos;
    span_init(&change->original, NULL, NULL);
    span_init(&change->replacement, NULL, NULL);

    list_add_tail(&file->pending_changes, &change->list);
    return change;
}

static void change_free(Change* change, bool free_pieces) {
    // We don't need to free the pieces of the original span, since they will be referenced by a previous change
    if (free_pieces && change->replacement.start != NULL) {
        list_for_each_interval(p, change->replacement.start, change->replacement.end, Piece, list) {
            piece_free(p);
        }
    }
    free(change);
}

static Revision* revision_alloc(File* file) {
    Revision* rev = malloc(sizeof(Revision));
    if (rev == NULL) {
        log_fatal("Out of memory.");
        return NULL;
    }
    list_init(&rev->changes);
    list_init(&rev->list);

    list_add_tail(&file->all_revisions, &rev->list);

    return rev;
}

static void revision_free(Revision* rev, bool free_pieces) {
    list_for_each_rev_member(change, &rev->changes, Change, list) {
        change_free(change, free_pieces);
    }
    free(rev);
}

static bool revision_purge(File* file) {

    // The history of revisions is linear:
    // If you undo a change, than perform a new operation, you cannot redo to the original state.
    // This function purges any revision after the current active one:
    // in other words, discards redo history.

    if (list_empty(&file->all_revisions)) {
        // No revision committed yet
        return false;
    }

    if (list_last(&file->all_revisions, Revision, list) == file->current_revision) {
        // We are already at the last revision
        return false;
    }

    list_for_each_rev_interval(rev, list_next(file->current_revision, Revision, list), list_last(&file->all_revisions, Revision, list), Revision, list) {
        list_del(&rev->list);
        revision_free(rev, true);
    }

    assert(file->current_revision == list_last(&file->all_revisions, Revision, list));

    return true;
}

File* hedit_file_open(const char* path) {

    // Initialize a new File structure
    File* file = calloc(1, sizeof(File));
    if (file == NULL) {
        log_fatal("Out of memory.");
        return NULL;
    }
    list_init(&file->all_blocks);
    list_init(&file->all_revisions);
    list_init(&file->all_pieces);
    list_init(&file->pieces);
    list_init(&file->pending_changes);

    if (path == NULL) {

        // Allocate an initial empty revision
        Revision* initial_rev = revision_alloc(file);
        if (initial_rev == NULL) {
            free(file);
            return NULL;
        }
        file->current_revision = initial_rev;

        return file;
    }

    // Store the file name
    if ((file->name = strdup(path)) == NULL) {
        log_fatal("Out of memory.");
        hedit_file_close(file);
        return NULL;
    }

    // Open the file r/w, if we fail try r/o
    int fd;
    bool ro;
    errno = 0;
    while ((fd = open(path, O_RDWR)) == -1 && errno == EINTR);
    switch (errno) {
        case 0:
            ro = false;
            break;
        case EACCES:
            errno = 0;
            while ((fd = open(path, O_RDONLY)) == -1 && errno == EINTR);
            if (errno != 0) {
                log_error("Cannot open %s: %s.", path, strerror(errno));
                hedit_file_close(file);
                return NULL;
            }
            ro = true;
            break;
        default:
            log_error("Cannot open %s: %s.", path, strerror(errno));
            hedit_file_close(file);
            return NULL;
    }
    file->ro = ro;

    // Stat the file to get the size
    struct stat s;
    if (fstat(fd, &s) < 0) {
        log_error("Cannot stat %s: %s.", path, strerror(errno));
        close(fd);
        hedit_file_close(file);
        return NULL;
    }

    // mmap the file to memory and create the initial piece
    Piece* p = NULL;
    if (s.st_size > 0) {
        Block* b = block_alloc_mmap(file, fd, s.st_size);
        if (b == NULL) {
            close(fd);
            hedit_file_close(file);
            return NULL;
        }
        p = piece_alloc(file);
        if (p == NULL) {
            close(fd);
            hedit_file_close(file);
            return NULL;
        }
        p->data = b->data;
        p->size = b->size;
        p->list.prev = &file->pieces;
        p->list.next = &file->pieces;
    }

    // Prepare the initial change
    Change* change = change_alloc(file, 0);
    if (change == NULL) {
        hedit_file_close(file);
        return NULL;
    }
    span_init(&change->original, NULL, NULL);
    span_init(&change->replacement, p, p);
    span_swap(file, &change->original, &change->replacement);

    // Commit the change to a revision
    if (!hedit_file_commit_revision(file)) {
        hedit_file_close(file);
        return NULL;
    }

    // Now that we have the file mapped in memory, we can safely close the fd
    close(fd);

    log_debug("File opened: %s.", file->name);

    return file;

}

void hedit_file_close(File* file) {
    if (file == NULL) {
        return;
    }

    log_debug("Closing file: %s.", file->name);

    hedit_file_commit_revision(file); // Commits any pending change

    list_for_each_rev_member(rev, &file->all_revisions, Revision, list) {
        // Note: here we are freeing revisions and changes, but not pieces.
        // This is because we are going to free all the pieces later,
        // and if we free some pieces without a proper undo, we end up with
        // a screwed chain.
        revision_free(rev, false);
    }

    list_for_each_rev_member(p, &file->all_pieces, Piece, global_list) {
        piece_free(p);
    }

    list_for_each_rev_member(b, &file->all_blocks, Block, list) {
        block_free(file, b);
    }

    if (file->name != NULL) {
        free(file->name);
    }

    free(file);
}

static bool set_name(File* file, const char* name) {
    
    // Switch the old file name with the new one
    char* dup = strdup(name);
    if (dup == NULL) {
        log_fatal("Out of memory.");
        return false;
    }
    free(file->name);
    file->name = dup;

    return true;
}

static bool write_to_fd_visitor(File* file, size_t unused, const unsigned char* data, size_t len, void* user) {
    int fd = (int)(long) user;

    const size_t blocksize = 64 * 1024;
    size_t offset = 0;
    while (offset < len) {
        ssize_t written;
        while ((written = write(fd, data + offset, MIN(blocksize, len - offset))) == -1 && errno == EINTR);
        if (written < 0) {
            log_error("Cannot write: %s.", strerror(errno));
            return false;
        }
        offset += written;
    }

    return true;
}

static bool write_to_fd(File* file, int fd) {
    return hedit_file_visit(file, 0, file->size, write_to_fd_visitor, (void*)(long) fd);
}

static bool hedit_file_save_atomic(File* file, const char* path) {

    // File is first saved to a temp directory,
    // then moved to the target destination with a rename.
    // This will fail of the original file is a synbolic or hard link,
    // and if there are problems in restoring the original file permissions.

    int oldfd = -1;
    int dirfd = -1;
    int tmpfd = -1;
    char* tmpname = NULL;
    char* pathdup = strdup(path);

    if (pathdup == NULL) {
        log_fatal("Out of memory.");
        goto error;
    }

    // Open the original file and get some info about it
    while ((oldfd = open(path, O_RDONLY)) == -1 && errno == EINTR);
    if (oldfd < 0 && errno != ENOENT) {
        log_error("Cannot open %s: %s.", path, strerror(errno));
        goto error;
    }
    struct stat oldstat = { 0 };
    if (oldfd != -1) {
        if (lstat(path, &oldstat) < 0) {
            log_error("Cannot stat %s: %s.", path, strerror(errno));
            goto error;
        }

        // The rename method does not work if the target is not a regular file or if it is a hard link
        if (!S_ISREG(oldstat.st_mode) || oldstat.st_nlink > 1) {
			goto error;
        }
    }

    size_t tmpnamelen = strlen(path) + 6 /* ~~save */ + 1 /* '\0' */;
    tmpname = malloc(sizeof(char) * tmpnamelen);
    if (tmpname == NULL) {
        log_fatal("Out of memory.");
        goto error;
    }
    snprintf(tmpname, tmpnamelen, "%s~~save", path);

    // Create the temp file
    while ((tmpfd = open(tmpname, O_WRONLY | O_CREAT | O_TRUNC, oldfd == -1 ? 0666 : oldstat.st_mode)) == -1 && errno == EINTR);
    if (tmpfd < 0) {
        log_error("Cannot open %s: %s.", tmpname, strerror(errno));
        goto error;
    }

    // If the old file existed, try to copy the owner to the temp file
    int res;
    if (oldfd != -1) {
        if (oldstat.st_uid != getuid()) {
            while ((res = fchown(tmpfd, oldstat.st_uid, (uid_t) -1)) == -1 && errno == EINTR);
            if (res < 0) {
                goto error;
            }
        }
        if (oldstat.st_gid != getgid()) {
            while ((res = fchown(tmpfd, (uid_t) -1, oldstat.st_gid)) == -1 && errno == EINTR);
            if (res < 0) {
                goto error;
            }
        }
    }

    // We don't need the old file anymore
    if (oldfd != -1 ) {
        close(oldfd);
        oldfd = -1;
    }

    // Write to the temp file
    if (!write_to_fd(file, tmpfd)) {
        goto error;
    }
    
    while ((res = fsync(tmpfd)) == -1 && errno == EINTR);
    if (res < 0) {
        log_error("Cannot fsync %s: %s.", tmpname, strerror(errno));
        goto error;
    }

    while ((res = close(tmpfd)) == -1 && errno == EINTR);
    if (res < 0) {
        log_error("Cannot close %s: %s.", tmpname, strerror(errno));
        goto error;
    }

    // Move the temp file over the original one
    while ((res = rename(tmpname, path)) == -1 && errno == EINTR);
    if (res < 0) {
        log_error("Cannot rename %s to %s: %s.", tmpname, path, strerror(errno));
        goto error;
    }

    // Open the parent directory and sync it to be sure that the rename has been committed to disk
    while ((dirfd = open(dirname(pathdup), O_DIRECTORY | O_RDONLY)) == -1 && errno == EINTR);
    if (dirfd < 0) {
        log_error("Cannot open directory %s: %s.", dirname(pathdup), strerror(errno));
        goto error;
    }
    while ((res = fsync(dirfd)) == -1 && errno == EINTR);
    if (res < 0) {
        log_error("Cannot fsync directory %s: %s.", dirname(pathdup), strerror(errno));
        goto error;
    }
    close(dirfd);

    free(tmpname);
    free(pathdup);
    log_debug("Saved atomically: %s.", path);
    return set_name(file, path);

error:
    if (oldfd != -1) {
        close(oldfd);
    }
    if (tmpfd != -1) {
        close(tmpfd);
    }
    if (dirfd != -1) {
        close(dirfd);
    }
    if (tmpname != NULL) {
        unlink(tmpname);
        free(tmpname);
    }
    if (pathdup != NULL) {
        free(pathdup);
    }
    return false;

}

static bool hedit_file_save_inplace(File* file, const char* path) {
    
    int fd;
    while ((fd = open(path, O_WRONLY | O_CREAT, 0666)) == -1 && errno == EINTR);
    if (fd < 0) {
        log_error("Cannot open %s: %s.", path, strerror(errno));
        return false;
    }

    if (!write_to_fd(file, fd)) {
        close(fd);
        return false;
    }

    int res;
    while ((res = fsync(fd)) == -1 && errno == EINTR);
    if (res < 0) {
        log_error("Cannot fsync %s: %s.", path, strerror(errno));
        close(fd);
        return false;
    }

    close(fd);

    log_debug("Saved in place: %s.", path);
    return set_name(file, path);

}

bool hedit_file_save(File* file, const char* path, enum FileSaveMode savemode) {    

    bool success = false;
    switch (savemode) {
        
        case SAVE_MODE_ATOMIC:
            success = hedit_file_save_atomic(file, path);
            break;
        
        case SAVE_MODE_INPLACE:
            success = hedit_file_save_inplace(file, path);
            break;
        
        case SAVE_MODE_AUTO:
            success = hedit_file_save_atomic(file, path);
            if (!success) {
                success = hedit_file_save_inplace(file, path);
            }
            break;

        default:
            abort();
    }

    if (success) {
        file->dirty = false;
        file->ro = false;
    }
    return success;

}

const char* hedit_file_name(File* file) {
    return file->name;
}

size_t hedit_file_size(File* file) {
    return file->size;
}

bool hedit_file_is_ro(File* file) {
    return file->ro;
}

bool hedit_file_is_dirty(File* file) {
    return file->dirty;
}

bool hedit_file_insert(File* file, size_t offset, const unsigned char* data, size_t len) {

    if (len == 0) {
        return true;
    }
    if (offset > file->size) {
        return false;
    }

    // Find the piece at offset `offset`
    Piece* piece;
    size_t piece_offset;
    if (!piece_find(file, offset, &piece, &piece_offset)) {
        if (list_empty(&file->pieces)) {
            piece = NULL;
            piece_offset = 0;
        } else if (offset == file->size) {
            piece = list_last(&file->pieces, Piece, list);
            piece_offset = piece->size;
        } else {
            return false;
        }
    }

    // Discard any redo history
    revision_purge(file);

    // Let's see if we can reuse the last block to store the new data
    Block* b;
    if (!list_empty(&file->all_blocks) && block_can_fit(list_last(&file->all_blocks, Block, list), len)) {
        b = list_last(&file->all_blocks, Block, list);
    } else {
        b = block_alloc(file, len);
        if (b == NULL) {
            return false;
        }
    }

    unsigned char* ptr = block_append(b, data, len);
    if (ptr == NULL) {
        return false;
    }

    // There might be two cases for insertion, depending on the offset:
    // - At the boundary of an already existing piece
    // - In the middle of an existing piece
    // In the first case, we can just allocate a new piece and insert it,
    // while in the other we have to replace the existing piece with three new ones.

    Change* change = change_alloc(file, offset);
    if (change == NULL) {
        return false;
    }

    if (piece == NULL) {
        // We have no piece to attach to because this is the first insertion to an empty file

        Piece* new = piece_alloc(file);
        if (new == NULL) {
            return false;
        }
        new->data = ptr;
        new->size = len;

        // Insert as the first piece
        new->list.prev = new->list.next = &file->pieces;

        span_init(&change->original, NULL, NULL);
        span_init(&change->replacement, new, new);       

    } else if (piece_offset == 0 || piece_offset == piece->size) {
        // For how we counted offsets, the only way that the `piece_offset == piece->size` condition
        // can be true is when we are inserting at the end of the file.

        Piece* new = piece_alloc(file);
        if (new == NULL) {
            return false;
        }
        new->data = ptr;
        new->size = len;

        // Insert before or after the piece
        if (piece_offset == 0) {
            new->list.prev = piece->list.prev;
            new->list.next = &piece->list;
        } else {
            new->list.prev = &piece->list;
            new->list.next = piece->list.next;
        }
        
        span_init(&change->original, NULL, NULL);
        span_init(&change->replacement, new, new);

    } else {

        Piece* before = piece_alloc(file);
        Piece* middle = piece_alloc(file);
        Piece* after = piece_alloc(file);
        if (before == NULL || middle == NULL || after == NULL) {
            return false;
        }

        // Split the data among the three pieces
        before->data = piece->data;
        before->size = piece_offset;
        middle->data = ptr;
        middle->size = len;
        after->data = piece->data + piece_offset;
        after->size = piece->size - piece_offset;

        // Join the three pieces together
        before->list.prev = piece->list.prev;
        before->list.next = &middle->list;
        middle->list.prev = &before->list;
        middle->list.next = &after->list;
        after->list.prev = &middle->list;
        after->list.next = piece->list.next;

        span_init(&change->original, piece, piece);
        span_init(&change->replacement, before, after);
    
    }

    // Apply the prepared change
    span_swap(file, &change->original, &change->replacement);
    file->dirty = true;
    return true;

}

bool hedit_file_delete(File* file, size_t offset, size_t len) {

    if (len == 0) {
        return true;
    }
    if (offset > file->size) {
        return false;
    }

    // Find the piece at the begin of the range to delete
    Piece* start_piece;
    size_t start_piece_offset;
    Piece* end_piece;
    size_t end_piece_offset;
    if (!piece_find(file, offset, &start_piece, &start_piece_offset)) {
        return false;
    }
    if (!piece_find(file, offset + len, &end_piece, &end_piece_offset)) {
        // If the end of the delete range fell out of the file, delete up to the last char
        end_piece = list_last(&file->pieces, Piece, list);
        end_piece_offset = end_piece->size;
    }  
    assert(start_piece != NULL);
    assert(end_piece != NULL);

    // Discard any redo history
    revision_purge(file);

    // Deletion range can both start and end up in the middle of a piece.
    // This means that we might have to create new pieces to account for the split pieces.

    Change* change = change_alloc(file, offset);
    if (change == NULL) {
        return false;
    }

    bool split_start = start_piece_offset != 0;
    bool split_end = end_piece_offset != end_piece->size;
    
    struct list_head* before = start_piece->list.prev;
    struct list_head* after = end_piece->list.next;

    Piece* new_start = NULL;
    Piece* new_end = NULL;

    if (split_start) {
        new_start = piece_alloc(file);
        if (new_start == NULL) {
            return false;
        }
        new_start->data = start_piece->data;
        new_start->size = start_piece_offset;
        new_start->list.prev = before;
        new_start->list.next = after;
    }

    if (split_end) {
        new_end = piece_alloc(file);
        if (new_end == NULL) {
            return false;
        }
        new_end->data = end_piece->data + end_piece_offset;
        new_end->size = end_piece->size - end_piece_offset;
        new_end->list.prev = before;
        new_end->list.next = after;
        if (split_start) {
            new_end->list.prev = &new_start->list;
            new_start->list.next = &new_end->list;
        }
    }

    if (new_start == NULL && new_end != NULL) {
        new_start = new_end;
    } else if (new_start != NULL && new_end == NULL) {
        new_end = new_start;
    }

    span_init(&change->original, start_piece, end_piece);
    span_init(&change->replacement, new_start, new_end);
    span_swap(file, &change->original, &change->replacement);

    file->dirty = true;
    return true;

}

bool hedit_file_replace(File* file, size_t offset, const unsigned char* data, size_t len) {
    // A replacement is just a convenience shortcut for insertion and deletion
    if (hedit_file_delete(file, offset, len)) {
        return hedit_file_insert(file, offset, data, len);
    } else {
        return false;
    }
}

bool hedit_file_commit_revision(File* file) {

    // Allocate a new revision only if there are pending changes not yet committed
    if (!list_empty(&file->pending_changes)) {
        Revision* rev = revision_alloc(file);
        if (rev == NULL) {
            return false;
        }
        file->current_revision = rev;
        
        // Move the changes from the temporary list to the revision
        rev->changes.next = file->pending_changes.next;
        rev->changes.prev = file->pending_changes.prev;
        file->pending_changes.next->prev = &rev->changes;
        file->pending_changes.prev->next = &rev->changes;
        list_init(&file->pending_changes);
    }

    return true;
}

bool hedit_file_undo(File* file, size_t* pos) {

    // Commit any pending change
    if (!hedit_file_commit_revision(file)) {
        return false;
    }

    if (list_first(&file->all_revisions, Revision, list) == file->current_revision) {
        return false;
    }

    // Revert all the changes in the last revision
    list_for_each_rev_member(c, &file->current_revision->changes, Change, list) {
        span_swap(file, &c->replacement, &c->original);
        *pos = c->pos;
    }
    file->current_revision = list_prev(file->current_revision, Revision, list);
    return true;

}

bool hedit_file_redo(File* file, size_t* pos) {
    
    // Commit any pending change
    if (!hedit_file_commit_revision(file)) {
        return false;
    }

    // Exit if there's nothing to redo
    if (list_last(&file->all_revisions, Revision, list) == file->current_revision) {
        return false;
    }

    // Reapply the changes in the revision
    Revision* rev = list_next(file->current_revision, Revision, list);
    list_for_each_member(c, &rev->changes, Change, list) {
        span_swap(file, &c->original, &c->replacement);
        *pos = c->pos;
    }
    file->current_revision = rev;
    return true;

}

bool hedit_file_read_byte(File* file, size_t offset, unsigned char* out) {
    Piece* p;
    size_t p_offset;
    if (!piece_find(file, offset, &p, &p_offset)) {
        return false;
    }

    *out = p->data[p_offset];
    return true;
}

bool hedit_file_visit(File* file, size_t start, size_t len, bool (*visitor)(File*, size_t offset, const unsigned char* data, size_t len, void* user), void* user) {
    if (start >= file->size || len == 0) {
        return true;
    }

    // The current text is made up of the current active pieces
    size_t off = 0;
    list_for_each_member(p, &file->pieces, Piece, list) {
        if (off + p->size >= start) {
            size_t piece_start = off <= start ? start - off : 0;
            if (!visitor(file, off + piece_start, p->data + piece_start, p->size - piece_start, user)) {
                return false;
            }
        }
        off += p->size;
    }

    return true;
}

FileIterator* hedit_file_iter(File* file, size_t start, size_t len) {

    FileIterator* it = calloc(1, sizeof(FileIterator));
    if (it == NULL) {
        log_fatal("Out of memory.");
        return NULL;
    }
    
    if (start >= file->size || len == 0) {
        // Return an empty iterator
        return it;
    }

    // Find the piece containing the first byte
    size_t off = 0;
    list_for_each_member(p, &file->pieces, Piece, list) {
        if (off + p->size >= start) {
            size_t piece_start = off <= start ? start - off : 0;
            it->max_off = MIN(off + piece_start + len, file->size);
            it->current_off = off;
            it->current_piece = p;
            it->current_data = p->data + piece_start;
            it->current_len = p->size - piece_start;
            break;
        }
        off += p->size;
    }

    assert(it->current_data != NULL);
    assert(it->current_len > 0);

    return it;

}

bool hedit_file_iter_next(FileIterator* it, const unsigned char** data, size_t* len) {
    if (it->current_off >= it->max_off) {
        return false;
    }

    // In the iterator, there's a cached version of the next data
    *data = it->current_data;
    *len = it->current_len;

    // Advance the iterator and compute the next data to return
    Piece* p = list_next(it->current_piece, Piece, list);
    it->current_piece = p;
    it->current_off += p->size;
    it->current_data = p->data;
    it->current_len = p->size - (it->current_off > it->max_off ? it->current_off - it->max_off : 0);

    return true;

}

void hedit_file_iter_free(FileIterator* it) {
    free(it);
}
