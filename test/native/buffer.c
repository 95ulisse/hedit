#include "util/buffer.h"
#include "ctest.h"

CTEST_DATA(buffer) {
    Buffer* buf;
};

CTEST_SETUP(buffer) {
    data->buf = buffer_new();
    ASSERT_NOT_NULL(data->buf);
}

CTEST_TEARDOWN(buffer) {
    buffer_free(data->buf);
}

CTEST2(buffer, initial_len_is_zero) {
    ASSERT_EQUAL(0, buffer_get_len(data->buf));
}

CTEST2(buffer, initial_capacity_is_not_zero) {
    ASSERT_NOT_EQUAL(0, buffer_get_capacity(data->buf));
}

CTEST2(buffer, initial_cursor_position_is_zero) {
    ASSERT_EQUAL(0, buffer_get_cursor(data->buf));
}

CTEST2(buffer, string_append) {
    buffer_put_string(data->buf, "hello");
    ASSERT_EQUAL(5, buffer_get_len(data->buf));
    ASSERT_EQUAL(5, buffer_get_cursor(data->buf));
    char str[6];
    buffer_copy_to(data->buf, str);
    ASSERT_STR("hello", str);
}

CTEST2(buffer, string_insert_middle) {
    buffer_put_string(data->buf, "held");
    buffer_move_cursor(data->buf, -2);
    buffer_put_string(data->buf, "llo wor");
    char str[11];
    buffer_copy_to(data->buf, str);
    ASSERT_STR("hello world", str);
}

CTEST2(buffer, cursor_does_not_move_beyond_bounds) {
    buffer_put_string(data->buf, "hello");
    buffer_move_cursor(data->buf, -10);
    ASSERT_EQUAL(0, buffer_get_cursor(data->buf));
    buffer_move_cursor(data->buf, 10);
    ASSERT_EQUAL(5, buffer_get_cursor(data->buf));
    buffer_set_cursor(data->buf, 3);
    ASSERT_EQUAL(3, buffer_get_cursor(data->buf));
}

CTEST2(buffer, string_deletion) {
    buffer_put_string(data->buf, "hello");

    buffer_del(data->buf, 3);
    ASSERT_EQUAL(2, buffer_get_len(data->buf));
    ASSERT_EQUAL(2, buffer_get_cursor(data->buf));
    char str[5];
    buffer_copy_to(data->buf, str);
    ASSERT_STR("he", str);

    buffer_set_cursor(data->buf, 0);
    buffer_del(data->buf, -1);
    ASSERT_EQUAL(1, buffer_get_len(data->buf));
    ASSERT_EQUAL(0, buffer_get_cursor(data->buf));
    buffer_copy_to(data->buf, str);
    ASSERT_STR("e", str);
}

CTEST2(buffer, string_deletion_bounds_check) {
    buffer_put_string(data->buf, "hello");

    buffer_del(data->buf, -1);
    ASSERT_EQUAL(5, buffer_get_len(data->buf));
    ASSERT_EQUAL(5, buffer_get_cursor(data->buf));
    char str[6];
    buffer_copy_to(data->buf, str);
    ASSERT_STR("hello", str);

    buffer_set_cursor(data->buf, 0);
    buffer_del(data->buf, 1);
    ASSERT_EQUAL(5, buffer_get_len(data->buf));
    ASSERT_EQUAL(0, buffer_get_cursor(data->buf));
    buffer_copy_to(data->buf, str);
    ASSERT_STR("hello", str);
}

size_t visitor_count = 0;
void visitor(Buffer* buf, size_t pos, const char* str, size_t len, void* user) {
    const unsigned char* whole = user;
    ASSERT_DATA(whole + pos, len, (const unsigned char*) str, len);
    visitor_count++;
}

CTEST2(buffer, visitor_visits_the_whole_buffer) {
    buffer_put_string(data->buf, "hello");
    buffer_move_cursor(data->buf, -2);
    char str[6];
    buffer_copy_to(data->buf, str);
    buffer_visit(data->buf, visitor, str);
    ASSERT_EQUAL(2, visitor_count);
}
