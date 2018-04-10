#include <stdlib.h>
#include <string.h>

#include "util/pubsub.h"
#include "util/list.h"
#include "util/map.h"


struct PubSub {
    Map* topics;                           // Topic filter => list of HandlerNodes
    struct list_head active_subscriptions; // List of active subscriptions
};

typedef struct HandlerNode {
    PubSubHandler handler;
    void* user;
    struct list_head list;              // Pointers for PubSub.topics
    struct list_head subscription_list; // Pointers for Subscription.nodes
} HandlerNode;

struct Subscription {
    struct list_head nodes; // list of the HandlerNodes registered with this subscription
    struct list_head list;  // Pointers for PubSub.active_subscriptions
};

// Thread-local default pubsub context
static __thread PubSub* def_pubsub;


PubSub* pubsub_new() {
    PubSub* pubsub = malloc(sizeof(*pubsub));
    if (pubsub == NULL) {
        return NULL;
    }

    Map* topics = map_new();
    if (topics == NULL) {
        free(pubsub);
        return NULL;
    }

    pubsub->topics = topics;
    list_init(&pubsub->active_subscriptions);

    return pubsub;
}

PubSub* pubsub_default() {
    if (def_pubsub == NULL) {
        def_pubsub = pubsub_new();
        if (def_pubsub == NULL) {
            abort();
        }
    }
    return def_pubsub;
}

void pubsub_free(PubSub* pubsub) {
    list_for_each_member(sub, &pubsub->active_subscriptions, Subscription, list) {
        pubsub_unregister(sub);
    }
    map_free_full(pubsub->topics);
    free(pubsub);
}

static struct list_head* get_list_head_for_topic(PubSub* pubsub, const char* topic) {

    // Return head if already created
    struct list_head* head = map_get(pubsub->topics, topic);
    if (head != NULL) {
        return head;
    }

    head = malloc(sizeof(*head));
    if (head == NULL) {
        return NULL;
    }
    list_init(head);

    if (!map_put(pubsub->topics, topic, head)) {
        free(head);
        return NULL;
    }

    return head;
    
}

Subscription* pubsub_register(PubSub* pubsub, const char* topic, PubSubHandler handler, void* user) {

    // Allocate a new subscription
    Subscription* sub = malloc(sizeof(*sub));
    if (sub == NULL) {
        return NULL;
    }
    list_init(&sub->nodes);
    list_add_tail(&pubsub->active_subscriptions, &sub->list);    

    // Duplicate the topic string
    char* topicdup = strdup(topic);
    if (topicdup == NULL) {
        goto error;
    }

    // For each comma-separated topic filter in the argument `topic`
    char* start = topicdup;
    size_t len = strlen(topicdup);
    for (int i = 0; i <= len; i++) {

        // Iterate until we meet a , or a \0
        bool restore_comma = false;
        if (topicdup[i] == ',') {
            topicdup[i] = '\0';
            restore_comma = true;
        }
        if (topicdup[i] != '\0') {
            continue;
        }

        // List head for the topic
        struct list_head* head = get_list_head_for_topic(pubsub, start);
        if (head == NULL) {
            goto error;
        }

        // Alocate a new node
        HandlerNode* node = malloc(sizeof(*node));
        if (node == NULL) {
            goto error;
        }
        node->handler = handler;
        node->user = user;
        list_add_tail(head, &node->list);
        list_add_tail(&sub->nodes, &node->subscription_list);

        // Restore the comma and advance the pointers
        if (restore_comma) {
            topicdup[i] = ',';
        }
        start = &(topicdup[i + 1]);

    }

    free(topicdup);
    return sub;

error:
    if (topicdup != NULL) {
        free(topicdup);
    }
    pubsub_unregister(sub);
    return NULL;
}

void pubsub_unregister(Subscription* sub) {

    // Delete all the HandlerNodes associated with this subscription
    list_for_each_member(node, &sub->nodes, HandlerNode, subscription_list) {
        list_del(&node->list); // Remove the node from the list of the topic handlers
        free(node);
    }

    // Delete the subscription from the list of the active ones
    list_del(&sub->list);
    free(sub);
    
}

struct PublishVisitorData {
    PubSub* pubsub;
    const char* topic;
    void* data;
};

static bool filter_match(const char* filter, const char* topic) {
    size_t filter_len = strlen(filter);
    
    for (int i = 0; i <= filter_len; i++) {
        switch (filter[i]) {
            case '\0':
                return topic[i] == '\0';
            case '*':
                return true;
            default:
                if (filter[i] != topic[i]) {
                    return false;
                }
                break;
        }
    }

    return true;
}

bool publish_visitor(const char* key, void* value, void* user) {
    struct PublishVisitorData* data = user;

    // TODO: Improve the performance by exploiting the ordered iteration.

    // Invoke all the handlers if the topic filter matches
    if (filter_match(key, data->topic)) {
        struct list_head* head = value;
        list_for_each_member(node, head, HandlerNode, list) {
            node->handler(data->pubsub, data->topic, data->data, node->user);
        }
    }

    return true;
    
}

void pubsub_publish(PubSub* pubsub, const char* topic, void* data) {
    struct PublishVisitorData publish_visitor_data = {
        .pubsub = pubsub,
        .topic = topic,
        .data = data
    };
    map_iterate(pubsub->topics, publish_visitor, &publish_visitor_data);
}
