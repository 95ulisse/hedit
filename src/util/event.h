#ifndef __EVENT_H__
#define __EVENT_H__

#include "util/list.h"

typedef struct list_head Event;

struct __event_handler {
    void (*f)(void*, ...);
    void* user;
    struct list_head list;
};

#define event_init(ev) list_init(ev)
void* event_add(Event* ev, void (*f)(void*, ...), void* user);
void event_del(Event* ev, void* token);

#define event_fire(ev, ...) \
    list_for_each_member(h, ev, struct __event_handler, list) { \
        h->f(h->user, ##__VA_ARGS__); \
    }

#endif