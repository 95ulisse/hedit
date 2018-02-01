#ifndef __EVENT_H__
#define __EVENT_H__

#include "util/list.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct list_head Event;
typedef void (*EventHandler)(void*, ...);

struct __event_handler {
    EventHandler f;
    void* user;
    struct list_head list;
};

#define event_init(ev) list_init(ev)
// This indirection on event_add is just to automatically add the cast
// and suppress compiler warnings abount mismatching function types.
#define event_add(ev, f, user) __event_add((ev), (EventHandler)(f), (user)) 
void* __event_add(Event* ev, EventHandler f, void* user);
void event_del(Event* ev, void* token);

#define event_fire(ev, ...) \
    list_for_each_member(h, ev, struct __event_handler, list) { \
        h->f(h->user, ##__VA_ARGS__); \
    }


#ifdef __cplusplus
}
#endif

#endif