#include <stdlib.h>

#include "util/log.h"
#include "util/event.h"

void* __event_add(Event* ev, EventHandler f, void* user) {

    // Allocate a new handler
    struct __event_handler* handler = malloc(sizeof(struct __event_handler));
    if (handler == NULL) {
        log_fatal("Cannot add event handler: out of memory.");
        return NULL;
    }
    handler->f = f;
    handler->user = user;
    list_init(&handler->list);

    // Insert the new handler at the end
    list_add_tail(ev, &handler->list);

    return (void*) handler;

}

void event_del(void* token) {

    // Remove the handler from the list
    struct __event_handler* handler = token;
    list_del(&handler->list);
    free(handler);

}

void event_free(Event* ev) {
    list_for_each_member(h, ev, struct __event_handler, list) {
        list_del(&h->list);
        free(h);
    }
}
