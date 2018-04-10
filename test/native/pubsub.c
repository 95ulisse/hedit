#include "util/pubsub.h"
#include "ctest.h"


CTEST_DATA(pubsub) {
    PubSub* pubsub;
};

CTEST_SETUP(pubsub) {
    data->pubsub = pubsub_new();
    ASSERT_NOT_NULL(data->pubsub);
}

CTEST_TEARDOWN(pubsub) {
    pubsub_free(data->pubsub);
}


void increment_handler(PubSub* pubsub, const char* topic, void* data, void* user) {
    (*((int*) user))++;
}


CTEST2(pubsub, subscriptions_can_be_canceled) {
    int count = 0;
    Subscription* sub = pubsub_register(data->pubsub, "A", increment_handler, &count);

    ASSERT_EQUAL(0, count);
    pubsub_publish(data->pubsub, "A", NULL);
    ASSERT_EQUAL(1, count);

    pubsub_unregister(sub);
    
    ASSERT_EQUAL(1, count);
    pubsub_publish(data->pubsub, "A", NULL);
    ASSERT_EQUAL(1, count);
}

CTEST2(pubsub, subscriptions_with_multiple_topics_are_canceled_with_a_single_call_to_unregister) {
    int count = 0;
    Subscription* sub = pubsub_register(data->pubsub, "A,B,C.*", increment_handler, &count);

    ASSERT_EQUAL(0, count);
    pubsub_publish(data->pubsub, "A", NULL);
    ASSERT_EQUAL(1, count);
    pubsub_publish(data->pubsub, "B", NULL);
    ASSERT_EQUAL(2, count);
    pubsub_publish(data->pubsub, "C.Test", NULL);
    ASSERT_EQUAL(3, count);

    pubsub_unregister(sub);
    
    ASSERT_EQUAL(3, count);
    pubsub_publish(data->pubsub, "A", NULL);
    ASSERT_EQUAL(3, count);
    pubsub_publish(data->pubsub, "B", NULL);
    ASSERT_EQUAL(3, count);
    pubsub_publish(data->pubsub, "C.Test", NULL);
    ASSERT_EQUAL(3, count);
}

CTEST2(pubsub, simple_topics_are_correctly_routed) {
    int count1 = 0;
    int count2 = 0;
    pubsub_register(data->pubsub, "A", increment_handler, &count1);
    pubsub_register(data->pubsub, "B", increment_handler, &count2);

    pubsub_publish(data->pubsub, "A", NULL);
    ASSERT_EQUAL(1, count1);
    ASSERT_EQUAL(0, count2);
    
    pubsub_publish(data->pubsub, "B", NULL);
    ASSERT_EQUAL(1, count1);
    ASSERT_EQUAL(1, count2);

    pubsub_publish(data->pubsub, "B", NULL);
    ASSERT_EQUAL(1, count1);
    ASSERT_EQUAL(2, count2);
}

CTEST2(pubsub, topic_filters_comma) {
    int count1 = 0;
    int count2 = 0;
    pubsub_register(data->pubsub, "A,B", increment_handler, &count1);
    pubsub_register(data->pubsub, "C,D", increment_handler, &count2);

    pubsub_publish(data->pubsub, "A", NULL);
    ASSERT_EQUAL(1, count1);
    ASSERT_EQUAL(0, count2);
    
    pubsub_publish(data->pubsub, "B", NULL);
    ASSERT_EQUAL(2, count1);
    ASSERT_EQUAL(0, count2);

    pubsub_publish(data->pubsub, "C", NULL);
    ASSERT_EQUAL(2, count1);
    ASSERT_EQUAL(1, count2);

    pubsub_publish(data->pubsub, "D", NULL);
    ASSERT_EQUAL(2, count1);
    ASSERT_EQUAL(2, count2);
}

CTEST2(pubsub, topic_filters_star) {
    int count1 = 0;
    int count2 = 0;
    pubsub_register(data->pubsub, "A.*", increment_handler, &count1);
    pubsub_register(data->pubsub, "B.*", increment_handler, &count2);

    pubsub_publish(data->pubsub, "A", NULL);
    ASSERT_EQUAL(0, count1);
    ASSERT_EQUAL(0, count2);
    
    pubsub_publish(data->pubsub, "B", NULL);
    ASSERT_EQUAL(0, count1);
    ASSERT_EQUAL(0, count2);

    pubsub_publish(data->pubsub, "A.", NULL);
    ASSERT_EQUAL(1, count1);
    ASSERT_EQUAL(0, count2);

    pubsub_publish(data->pubsub, "B.", NULL);
    ASSERT_EQUAL(1, count1);
    ASSERT_EQUAL(1, count2);

    pubsub_publish(data->pubsub, "A.x", NULL);
    ASSERT_EQUAL(2, count1);
    ASSERT_EQUAL(1, count2);

    pubsub_publish(data->pubsub, "B.x", NULL);
    ASSERT_EQUAL(2, count1);
    ASSERT_EQUAL(2, count2);
}

CTEST2(pubsub, handlers_registered_with_comma_filter_are_called_more_than_once_if_both_filters_match) {
    int count = 0;
    pubsub_register(data->pubsub, "A.*,A.B", increment_handler, &count);

    // A.B matches both A.* and A.B
    pubsub_publish(data->pubsub, "A.B", NULL);
    ASSERT_EQUAL(2, count);
}
