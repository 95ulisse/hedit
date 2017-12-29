#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "buffer.h"

struct Buffer {
    char* start;
    char* gap_start;
    char* gap_end;
    char* end;
};

#define DEFAULT_CAPACITY 1024

#define MIN(a, b) \
    __extension__({ \
        __typeof__(a) __a = (a); \
        __typeof__(b) __b = (b); \
        __a < __b ? __a : __b; \
    })

#define ASSERT_INVARIANT(b) \
    do { \
        assert(b->start <= b->gap_start); \
        assert(b->gap_start <= b->gap_end); \
        assert(b->gap_end <= b->end); \
    } while (false)


static bool ensure_gap_size(Buffer*, size_t);


Buffer* buffer_new() {

    char* data = malloc(DEFAULT_CAPACITY);
    if (data == NULL) {
        return NULL;
    }

    Buffer* buf = malloc(sizeof(Buffer));
    if (buf == NULL) {
        free(data);
        return NULL;
    }

    buf->start = data;
    buf->end = data + DEFAULT_CAPACITY;
    buf->gap_start = buf->start;
    buf->gap_end = buf->end;

    ASSERT_INVARIANT(buf);

    return buf;

}

void buffer_free(Buffer* buf) {
    ASSERT_INVARIANT(buf);
    free(buf->start);
    free(buf);
}

size_t buffer_get_capacity(Buffer* buf) {
    ASSERT_INVARIANT(buf);
    return buf->end - buf->start;
}

size_t buffer_get_len(Buffer* buf) {
    ASSERT_INVARIANT(buf);
    return (size_t)(buf->gap_start - buf->start) +
           (size_t)(buf->end - buf->gap_end);
}

size_t buffer_get_cursor(Buffer* buf) {
    ASSERT_INVARIANT(buf);
    return buf->gap_start - buf->start;
}

void buffer_set_cursor(Buffer* buf, size_t offset) {
    ASSERT_INVARIANT(buf);
    buffer_move_cursor(buf, (int)offset - (int)buffer_get_cursor(buf));
}

void buffer_move_cursor(Buffer* buf, int offset) {
    ASSERT_INVARIANT(buf);
    
    if (offset <= 0) {
        
        // Max allowed movement to stay in the buffer's range
        size_t n = MIN(abs(offset), buf->gap_start - buf->start);
        if (n == 0) {
            return;
        }

        buf->gap_start -= n;
        buf->gap_end -= n;
        memcpy(buf->gap_end, buf->gap_start, n);

    } else {

        // Max allowed movement to stay in the buffer's range
        size_t n = MIN(offset, buf->end - buf->gap_end);
        if (n == 0) {
            return;
        }

        buf->gap_start += n;
        buf->gap_end += n;
        memcpy(buf->gap_start - n, buf->gap_end - n, n);

    }
}

bool buffer_put_char(Buffer* buf, char c) {
    ASSERT_INVARIANT(buf);

    if (!ensure_gap_size(buf, 1)) {
        return false;
    }

    *buf->gap_start = c;
    buf->gap_start++;
    return true;
}

bool buffer_put_string(Buffer* buf, const char* str) {
    ASSERT_INVARIANT(buf);

    size_t n = strlen(str);
    if (!ensure_gap_size(buf, n)) {
        return false;
    }

    memcpy(buf->gap_start, str, n);
    buf->gap_start += n;
    return true;
}

void buffer_del(Buffer* buf, int count) {
    ASSERT_INVARIANT(buf);

    if (count == 0) {
        return;
    } else if (count < 0) {
        size_t n = MIN(abs(count), buf->end - buf->gap_end);
        buf->gap_end += n;
    } else {
        size_t n = MIN(count, buf->gap_start - buf->start);
        buf->gap_start -= n;
    }
}

void buffer_visit(Buffer* buf, void (*visitor)(Buffer*, size_t pos, const char* str, size_t len, void*), void* user) {
    ASSERT_INVARIANT(buf);

    size_t firstsize = buf->gap_start - buf->start;
    size_t secondsize = buf->end - buf->gap_end;

    if (firstsize > 0) {
        visitor(buf, 0, buf->start, buf->gap_start - buf->start, user);
    }
    if (secondsize > 0) {
        visitor(buf, firstsize, buf->gap_end, buf->end - buf->gap_end, user);
    }
}

void buffer_copy_to(Buffer* buf, char* dest) {
    ASSERT_INVARIANT(buf);

    size_t firstsize = buf->gap_start - buf->start;
    size_t secondsize = buf->end - buf->gap_end;

    memcpy(dest, buf->start, firstsize);
    memcpy(dest + firstsize, buf->gap_end, secondsize);
    dest[firstsize + secondsize] = '\0';
}

static bool ensure_gap_size(Buffer* buf, size_t desired) {
    size_t current = buf->gap_end - buf->gap_start;
    if (current >= desired) {
        return true;
    }

    // Double the size of the buffer until the desired size is met
    size_t occupied = buffer_get_len(buf);
    size_t newcapacity = buffer_get_capacity(buf) * 2;
    while (newcapacity < desired + occupied) {
        newcapacity *= 2;
    }

    char* olddata = buf->start;
    size_t firstsize = buf->gap_start - buf->start;
    size_t secondsize = buf->end - buf->gap_end;

    // Reallocate the data array and update the pointers
    char* newdata = malloc(newcapacity);
    if (newdata == NULL) {
        return false;
    }
    memcpy(newdata, olddata, firstsize);
    memcpy(newdata + newcapacity - secondsize, buf->gap_end, secondsize);
    buf->start = newdata;
    buf->end = newdata + newcapacity;
    buf->gap_start = buf->start + firstsize;
    buf->gap_end = buf->end - secondsize;
    free(olddata);

    return true;
}