#ifndef __COMMON_H__
#define __COMMON_H__

#include <limits.h>
#include <ctype.h>
#include <errno.h>

#define MIN(a, b) \
    __extension__({ \
        __typeof__(a) __a = (a); \
        __typeof__(b) __b = (b); \
        __a < __b ? __a : __b; \
    })

static inline bool str2int(const char* s, int* out) {
    char *end;
    if (s[0] == '\0' || isspace((unsigned char) s[0]))
        return false;
    errno = 0;
    long l = strtol(s, &end, 10);
    if (l > INT_MAX || (errno == ERANGE && l == LONG_MAX))
        return false;
    if (l < INT_MIN || (errno == ERANGE && l == LONG_MIN))
        return false;
    if (*end != '\0')
        return false;
    *out = l;
    return true;
}

/** Generic struct to pass a single argument to a callback function. */
typedef struct {
    int i;
    bool b;
} Value;

#endif