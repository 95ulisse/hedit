#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <libgen.h>

#include "file.h"
#include "util/log.h"
#include "util/common.h"

struct File {
    unsigned char* mem;
    size_t size;
    char* name;
    bool ro;
    bool dirty;
};

File* hedit_file_open(const char* path) {

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
                return NULL;
            }
            ro = true;
            break;
        default:
            log_error("Cannot open %s: %s.", path, strerror(errno));
            return NULL;
    }

    // Stat the file to get the size
    struct stat s;
    if (fstat(fd, &s) < 0) {
        log_error("Cannot stat %s: %s.", path, strerror(errno));
        close(fd);
        return NULL;
    }

    // mmap the file to memory
    void* mem = mmap(NULL, s.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (mem == MAP_FAILED) {
        log_error("Cannot mmap %s: %s.", path, strerror(errno));
        close(fd);
        return NULL;
    }

    // Now that we have the file mapped in memory, we can safely close the fd
    close(fd);

    // Initialize a new File structure
    File* file = malloc(sizeof(File));
    if (file == NULL) {
        log_fatal("Cannot allocate memory for File structure.");
        munmap(mem, s.st_size);
        return NULL;
    }
    file->mem = mem;
    file->size = s.st_size;
    file->ro = ro;
    file->dirty = false;

    if ((file->name = strdup(path)) == NULL) {
        log_fatal("Cannot allocate memory for File structure.");
        munmap(mem, s.st_size);
        free(file);
        return NULL;
    }

    log_debug("File opened: %s.", file->name);

    return file;

}

void hedit_file_close(File* file) {
    if (file == NULL) {
        return;
    }

    log_debug("File closed: %s.", file->name);

    munmap(file->mem, file->size);
    free(file->name);
    free(file);
}

static bool hedit_file_set_name(File* file, const char* name) {
    
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

        // The rename method does not work if the target file is a symbolic or hard link
        if (S_ISLNK(oldstat.st_mode) || oldstat.st_nlink > 1) {
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
    const size_t blocksize = 64 * 1024;
    size_t offset = 0;
    while (offset < file->size) {
        ssize_t written;
        while ((written = write(tmpfd, file->mem + offset, MIN(blocksize, file->size - offset))) == -1 && errno == EINTR);
        if (written < 0) {
            log_error("Cannot write to %s: %s.", tmpname, strerror(errno));
            goto error;
        }
        offset += written;
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
    return hedit_file_set_name(file, path);

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

    const size_t blocksize = 64 * 1024;
    size_t offset = 0;
    while (offset < file->size) {
        ssize_t written;
        while ((written = write(fd, file->mem + offset, MIN(blocksize, file->size - offset))) == -1 && errno == EINTR);
        if (written < 0) {
            log_error("Cannot write to %s: %s.", path, strerror(errno));
            close(fd);
            return false;
        }
        offset += written;
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
    return hedit_file_set_name(file, path);

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

bool hedit_file_write_byte(File* file, size_t offset, unsigned char byte) {
    
    // Bounds check
    if (offset >= file->size) {
        log_error("Write out of bounds.");
        return false;
    }

    file->mem[offset] = byte;
    file->dirty = true;
    return true;
}

bool hedit_file_read_byte(File* file, size_t offset, unsigned char* byte) {
    
    // Bounds check
    if (offset >= file->size) {
        log_error("Read out of bounds.");
        return false;
    }

    *byte = file->mem[offset];
    return true;
}

void hedit_file_visit(File* file, size_t start, size_t len, void (*visitor)(File*, size_t offset, const unsigned char* data, size_t len, void* user), void* user) {

    if (start >= file->size || len <= 0) {
        return;
    }

    // We only have a single segment, so call the visitor right away
    visitor(file, start, file->mem + start, MIN(file->size - start, len), user);

}