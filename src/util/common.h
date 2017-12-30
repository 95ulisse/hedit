#ifndef __COMMON_H__
#define __COMMON_H__

#define MIN(a, b) \
    __extension__({ \
        __typeof__(a) __a = (a); \
        __typeof__(b) __b = (b); \
        __a < __b ? __a : __b; \
    })

#endif