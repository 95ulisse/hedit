#include "util/buffer.h"
#include "ctest.h"

CTEST(util_buffer, initial_len_is_zero) {
    Buffer* buf = buffer_new();
    ASSERT_EQUAL(0, buffer_get_len(buf));
    buffer_free(buf);
}
