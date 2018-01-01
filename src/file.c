#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "file.h"
#include "util/log.h"
#include "util/common.h"

struct File {
    char* mem;
    size_t size;
    char* name;
    bool ro;
    bool dirty;

    enum {
        SAVE_MSYNC,
        SAVE_WRITE
    } save_mode;
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
    void* mem = mmap(NULL, s.st_size, ro ? PROT_READ : (PROT_READ | PROT_WRITE), MAP_PRIVATE, fd, 0);
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
    file->save_mode = SAVE_MSYNC;

    if ((file->name = strdup(path)) == NULL) {
        log_fatal("Cannot allocate memory for File structure.");
        munmap(mem, s.st_size);
        free(file);
        return NULL;
    }

    return file;

}

void hedit_file_close(File* file) {
    if (file == NULL) {
        return;
    }

    munmap(file->mem, file->size);
    free(file->name);
    free(file);
}

bool hedit_file_save(File* file) {

    switch (file->save_mode) {

        case SAVE_MSYNC: {

            // If the file is read only, we cannot commit the mmapped memory region
            // and we need to be fed another file name.
            if (file->ro) {
                log_error("Read only file.");
                return false;
            }

            // Sync the mmapped memory back to disk
            if (msync(file->mem, file->size, MS_SYNC) < 0) {
                log_error("Cannot sync changes to disk: %s.", strerror(errno));
                return false;
            }

            break;
        }

        case SAVE_WRITE: {

            int fd;
            while ((fd = open(file->name, O_WRONLY | O_CREAT, 0666)) == -1 && errno == EINTR);
            if (fd < 0) {
                log_error("Cannot open %s: %s.", file->name, strerror(errno));
                return false;
            }

            const size_t blocksize = 64 * 1024;
            size_t offset = 0;
            while (offset < file->size) {
                ssize_t written;
                while ((written = write(fd, file->mem + offset, MIN(blocksize, file->size - offset))) == -1 && errno == EINTR);
                if (written < 0) {
                    log_error("Cannot write to %s: %s.", file->name, strerror(errno));
                    close(fd);
                    return false;
                }
                offset += written;
            }

            close(fd);

            break;
        }

    }

    file->dirty = false;
    file->ro = false;
    return true;

}

const char* hedit_file_name(File* file) {
    return file->name;
}

bool hedit_file_set_name(File* file, const char* name) {
    
    // Switch the old file name with the new one
    char* dup = strdup(name);
    if (dup == NULL) {
        log_fatal("Out of memory.");
        return false;
    }
    free(file->name);
    file->name = dup;

    // Since we have been given a new file name, we cannot save anymore
    // using msync, but we need to perform a full write on another file.
    file->save_mode = SAVE_WRITE;
    file->ro = false;

    return true;
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

bool hedit_file_write_byte(File* file, size_t offset, char byte) {
    
    // Bounds check
    if (offset >= file->size) {
        log_error("Write out of bounds.");
        return false;
    }

    file->mem[offset] = byte;
    file->dirty = true;
    return true;
}

void hedit_file_visit(File* file, void (*visitor)(File*, size_t offset, const char* data, size_t len, void* user), void* user) {

    // We only have a single segment, so call the visitor right away
    visitor(file, 0, file->mem, file->size, user);

}