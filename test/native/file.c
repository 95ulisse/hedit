#include "file.h"
#include "ctest.h"


// Silence warnings about pointer signs.
// String literals are char*, while most of the File API wants const char*.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-sign"


// Asserts that the contents of a File are equal to the given string
void ASSERT_FILE(const unsigned char* expected, File* file) {
    FileIterator* it = hedit_file_iter(file, 0, hedit_file_size(file));

    size_t pos = 0;
    const unsigned char* chunk;
    size_t chunk_size;
    while (hedit_file_iter_next(it, &chunk, &chunk_size)) {
        ASSERT_DATA(expected + pos, chunk_size, chunk, chunk_size);
        pos += chunk_size;
    }

    hedit_file_iter_free(it);
    ASSERT_EQUAL(hedit_file_size(file), pos);
}



CTEST_DATA(file) {
    File* file;
};

CTEST_SETUP(file) {
    data->file = hedit_file_open(NULL);
    ASSERT_NOT_NULL(data->file);
}

CTEST_TEARDOWN(file) {
    hedit_file_close(data->file);
}



CTEST2(file, initial_state) {
    ASSERT_NULL(hedit_file_name(data->file));
    ASSERT_EQUAL(0, hedit_file_size(data->file));
    ASSERT_FALSE(hedit_file_is_ro(data->file));
    ASSERT_FALSE(hedit_file_is_dirty(data->file));
}

CTEST2(file, insert) {
    hedit_file_insert(data->file, 0, "hello", 5);
    ASSERT_FILE("hello", data->file);
    hedit_file_insert(data->file, 5, "world", 5);
    ASSERT_FILE("helloworld", data->file);
    hedit_file_insert(data->file, 5, " ", 1);
    ASSERT_FILE("hello world", data->file);
    ASSERT_TRUE(hedit_file_is_dirty(data->file));
}

CTEST2(file, delete) {
    hedit_file_insert(data->file, 0, "hello world", 11);
    hedit_file_delete(data->file, 0, 5);
    ASSERT_FILE(" world", data->file);
    hedit_file_delete(data->file, 1, 5);
    ASSERT_FILE(" ", data->file);
    hedit_file_delete(data->file, 0, 1);
    ASSERT_FILE("", data->file);
    ASSERT_TRUE(hedit_file_is_dirty(data->file));
}

CTEST2(file, insert_and_delete) {
    hedit_file_insert(data->file, 0, "hello", 5);
    hedit_file_delete(data->file, 0, 3); // "lo"
    hedit_file_insert(data->file, 1, "w", 1); // "lwo"
    hedit_file_insert(data->file, 3, "rld", 3); // "lworld"
    hedit_file_delete(data->file, 0, 1); // "world"
    hedit_file_insert(data->file, 0, "hello_", 6); // "hello_world"
    hedit_file_replace(data->file, 5, " ", 1); // "hello world"
    ASSERT_FILE("hello world", data->file);
}

CTEST2(file, undo) {
    hedit_file_insert(data->file, 0, "hello", 5);

    size_t pos;
    ASSERT_TRUE(hedit_file_undo(data->file, &pos));
    ASSERT_EQUAL(0, pos);
    ASSERT_FILE("", data->file);

    hedit_file_insert(data->file, 0, "hello", 5);
    hedit_file_commit_revision(data->file);
    hedit_file_insert(data->file, 5, " world", 6);

    ASSERT_TRUE(hedit_file_undo(data->file, &pos));
    ASSERT_EQUAL(5, pos);
    ASSERT_FILE("hello", data->file);

    ASSERT_TRUE(hedit_file_undo(data->file, &pos));
    ASSERT_EQUAL(0, pos);
    ASSERT_FILE("", data->file);

    ASSERT_FALSE(hedit_file_undo(data->file, &pos));
    ASSERT_EQUAL(0, pos);
    ASSERT_FILE("", data->file);
}

CTEST2(file, redo) {
    hedit_file_insert(data->file, 0, "hello", 5);

    size_t pos;
    ASSERT_FALSE(hedit_file_redo(data->file, &pos));
    ASSERT_FILE("hello", data->file);

    hedit_file_insert(data->file, 5, " world", 6);

    ASSERT_TRUE(hedit_file_undo(data->file, &pos));
    ASSERT_EQUAL(5, pos);
    ASSERT_FILE("hello", data->file);

    ASSERT_TRUE(hedit_file_redo(data->file, &pos));
    ASSERT_EQUAL(5, pos);
    ASSERT_FILE("hello world", data->file);

    ASSERT_TRUE(hedit_file_undo(data->file, &pos));
    ASSERT_TRUE(hedit_file_undo(data->file, &pos));
    ASSERT_FILE("", data->file);

    ASSERT_TRUE(hedit_file_redo(data->file, &pos));
    ASSERT_EQUAL(0, pos);
    ASSERT_FILE("hello", data->file);
    ASSERT_TRUE(hedit_file_redo(data->file, &pos));
    ASSERT_EQUAL(5, pos);
    ASSERT_FILE("hello world", data->file);
    
    ASSERT_FALSE(hedit_file_redo(data->file, &pos));
    ASSERT_FILE("hello world", data->file);
}

CTEST2(file, undo_redo_insert_and_delete) {

    // Create a small history
    hedit_file_insert(data->file, 0, "hello", 5);  // "hello"
    hedit_file_commit_revision(data->file);
    hedit_file_delete(data->file, 0, 3);           // "lo"
    hedit_file_commit_revision(data->file);
    hedit_file_insert(data->file, 1, "w", 1);      // "lwo"
    hedit_file_commit_revision(data->file);
    hedit_file_insert(data->file, 3, "rld", 3);    // "lworld"
    hedit_file_commit_revision(data->file);
    hedit_file_delete(data->file, 0, 1);           // "world"
    hedit_file_commit_revision(data->file);
    hedit_file_insert(data->file, 0, "hello_", 6); // "hello_world"
    hedit_file_commit_revision(data->file);
    hedit_file_replace(data->file, 5, " ", 1);     // "hello world"
    hedit_file_commit_revision(data->file);
    ASSERT_FILE("hello world", data->file);

    // Start navigating revisions to test undoing of both insertion and deletions

    size_t pos;
    ASSERT_FALSE(hedit_file_redo(data->file, &pos));

    ASSERT_TRUE(hedit_file_undo(data->file, &pos));
    ASSERT_EQUAL(5, pos);
    ASSERT_FILE("hello_world", data->file);

    ASSERT_TRUE(hedit_file_undo(data->file, &pos));
    ASSERT_EQUAL(0, pos);
    ASSERT_FILE("world", data->file);

    ASSERT_TRUE(hedit_file_undo(data->file, &pos));
    ASSERT_EQUAL(0, pos);
    ASSERT_FILE("lworld", data->file);

    ASSERT_TRUE(hedit_file_redo(data->file, &pos));
    ASSERT_EQUAL(0, pos);
    ASSERT_FILE("world", data->file);

    ASSERT_TRUE(hedit_file_redo(data->file, &pos));
    ASSERT_EQUAL(0, pos);
    ASSERT_FILE("hello_world", data->file);

    ASSERT_TRUE(hedit_file_redo(data->file, &pos));
    ASSERT_EQUAL(5, pos);
    ASSERT_FILE("hello world", data->file);

    ASSERT_FALSE(hedit_file_redo(data->file, &pos));

    // Unroll to the beginning and then to the end to count the number of revisions

    int nrevisions = 0;
    while (hedit_file_undo(data->file, &pos)) {
        nrevisions++;
    }
    ASSERT_FILE("", data->file);
    ASSERT_EQUAL(7, nrevisions);

    nrevisions = 0;
    while (hedit_file_redo(data->file, &pos)) {
        nrevisions++;
    }
    ASSERT_FILE("hello world", data->file);
    ASSERT_EQUAL(7, nrevisions);

}

bool visitor1(File* file, size_t offset, const unsigned char* data, size_t len, void* user) {
    ASSERT_EQUAL(3, offset);
    ASSERT_EQUAL(6, len);
    ASSERT_DATA("lo wor", len, data, len);
    (*((int*) user))++;
    return true;
}

CTEST2(file, visitor_can_visit_also_portions) {
    hedit_file_insert(data->file, 0, "hello world", 11);

    int invocations = 0;
    ASSERT_TRUE(hedit_file_visit(data->file, 3, 6, visitor1, &invocations));
    ASSERT_EQUAL(1, invocations);
}

bool visitor2(File* file, size_t offset, const unsigned char* data, size_t len, void* user) {
    int* n = user;
    (*n)++;
    return *n <= 1;
}

CTEST2(file, visit_stops_if_visitor_returns_false) {
    hedit_file_insert(data->file, 0, "hello", 5);
    hedit_file_commit_revision(data->file);
    hedit_file_insert(data->file, 5, " world", 6);
    hedit_file_commit_revision(data->file);
    hedit_file_insert(data->file, 11, "!", 1);

    int invocations = 0;
    ASSERT_FALSE(hedit_file_visit(data->file, 0, 1, visitor2, &invocations));
    ASSERT_EQUAL(2, invocations);
}


#pragma GCC diagnostic pop
