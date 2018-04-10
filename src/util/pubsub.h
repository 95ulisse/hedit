#ifndef __PUBSUB_H__
#define __PUBSUB_H__


#ifdef __cplusplus
extern "C" {
#endif


/*
 * This is a very simple implementation of a publish/subscribe broker.
 * Subscribers can register an interest for a topic or a group of topics,
 * and publishers can push messages to those topics.
 */

/** A publish/subscribe context. It keeps track of the topics and the interests. */
typedef struct PubSub PubSub;

/**
 * Subscription of an handler to a specific topic.
 * Can be used to cancel the subscription using `pubsub_unregister`.
 */
typedef struct Subscription Subscription;

/** Handler function for the publishing of a new event. */
typedef void (*PubSubHandler)(PubSub*, const char* topic, void* data, void* user);

/** Creates a new publish/subscribe context. */
PubSub* pubsub_new();

/**
 * Returns the default PubSub context for this thread.
 * Crashes the program if there's an error during its allocation.
 */
PubSub* pubsub_default();

/**
 * Releases all the resources of the given publish/subscribe context,
 * and automatically unregisters all the listeners.
 * This will also invalidate all the subscriptions in this context.
 */
void pubsub_free(PubSub*);

/**
 * Registers a new handler for a given topic.
 * A topic can be a single specific topic, or a more complex filter:
 *
 * - "A": will match only the messages published within the exact topic "A"
 * - "A,B": will match all the messages published either within "A" or "B"
 * - "A.*": will match all the messages published within topics whose name start with "A."
 *
 * As an example, "A1,B.*" matches the topics "A1", "B.B1", but not the topic "B".
 *
 * Returns an opaque token that can be used to cancel the subscription
 * using `pubsub_unregister`.
 */
Subscription* pubsub_register(PubSub*, const char* topic, PubSubHandler handler, void* user);

/** Cancels a subscription registered with `pubsub_register`. */
void pubsub_unregister(Subscription*);

/**
 * Publishes a new message within a topic.
 * Note that `topic` must be a specific topic, not a filter (i.e. it cannot contain `*` or `,`).
 */
void pubsub_publish(PubSub*, const char* topic, void* data);


#ifdef __cplusplus
}
#endif

#endif
