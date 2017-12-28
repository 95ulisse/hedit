#include <stdlib.h>

#include "log.h"
#include "util/event.h"

void* event_add(Event* ev, void (*f)(void*, ...), void* user) {

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

void event_del(Event* ev, void* token) {

    // Remove the handler from the list
    struct __event_handler* handler = token;
    list_del(&handler->list);
    free(handler);

}