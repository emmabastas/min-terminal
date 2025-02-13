#include "ringbuf.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "CuTest.h"

size_t min(size_t x, size_t y) {
    if (x < y) {
        return x;
    }
    return y;
}
size_t max(size_t x, size_t y) {
    if (x > y) {
        return x;
    }
    return y;
}

void ringbuf_initialize(size_t capacity, struct ringbuf *rb_ret) {
    assert(capacity == 1 << 2
           || capacity == 1 << 3
           || capacity == 1 << 4
           || capacity == 1 << 5
           || capacity == 1 << 6
           || capacity == 1 << 7
           || capacity == 1 << 8
           || capacity == 1 << 9
           || capacity == 1 << 10
           || capacity == 1 << 11
           || capacity == 1 << 12
           || capacity == 1 << 13
           || capacity == 1 << 14
           || capacity == 1 << 15
           || capacity == 1 << 16
           || capacity == 1 << 17
           || capacity == 1 << 18
           || capacity == 1 << 19
           || capacity == 1 << 20
           || capacity == 1 << 21
           || capacity == 1 << 22
           || capacity == 1 << 23
           || capacity == 1 << 24);

    uint8_t *buf = calloc(capacity, 1);
    if (buf == NULL) {
        // calloc failure
        assert(false);
    }

    rb_ret->buf = buf;
    rb_ret->cursor = 0;
    rb_ret->capacity = capacity;
}

void ringbuf_write(struct ringbuf *rb, uint8_t *data, size_t len) {
    // Say you wan't to write "Hello, World!", and the ring buffer is like
    // this:
    //
    //                              v "newest" data
    //         buf -> [ * * * * * * * * * * * * * * * * * * ]
    //                                ^ "oldest" data
    //
    // We wan't it to now end up like this
    //
    //                    v "newest" data
    //         buf -> [ d ! * * * * * H e l l o , _ W o r l ]
    //                      ^ "oldest" data
    //
    // Basically we can fit <index of newest data> - <index of buf> into the
    // very start of the buffer and we can fit
    // <end of buffer> - <index of newest data> into the very end of the buffer.
    // Our ringbuffuer structure doesn't have pointers for "newest" and "oldest"
    // data, instead we have `cursor` which is an offset such that
    //     buf + cursor = <index of newest data>
    // We also have `capacity`.
    //
    // But what if we want to write more data in one go than can fit in the
    // buffer? Easy, we just write the last `capacity` bytes from that data.
    //
    //     write: "42, is the answer!!!"
    //
    //                                  v "newest" data
    //         buf -> [ a n s w e r ! ! ! , _ i s _ t h e _ ]
    //                                    ^ "oldest" data

    // TODO perf: use `min` and `max` througought for branchless code.

    if (len > rb->capacity) {
        data = data + len - rb->capacity;
        len = rb->capacity;
    }

    size_t end_size = min(rb->capacity - rb->cursor, len);
    assert(rb->cursor + end_size <= rb->capacity);
    assert(end_size <= len);
    memcpy(rb->buf + rb->cursor, data, end_size);

    size_t begining_size = len - end_size;
    assert(end_size + begining_size == len);
    memcpy(rb->buf, data + end_size, begining_size);

    rb->cursor = (rb->cursor + len) & (rb->capacity - 1);
}

uint8_t ringbuf_get(struct ringbuf *rb, size_t offset) {
    return rb->buf[(rb->cursor - 1 - offset) & (rb->capacity - 1)];
}

/*
 * Unit tests for this file bellow
 */

// Writing no data should leave the ringbuffer unaltered
void test_ringbuf_write_empty(CuTest *tc) {
    struct ringbuf rb;
    ringbuf_initialize(64, &rb);

    ringbuf_write(&rb, NULL, 0);

    CuAssertIntEquals(tc, 0, rb.cursor);
    CuAssertIntEquals(tc, 64, rb.capacity);

    uint8_t *empty = calloc(64, 1);
    CuAssertBytesEquals(tc, empty, rb.buf, 64);

    free(empty);
    free(rb.buf);
}

// Writing data of size 1
void test_ringbuf_write_single(CuTest *tc) {
    struct ringbuf rb;
    ringbuf_initialize(64, &rb);

    uint8_t data = '#';
    ringbuf_write(&rb, &data, 1);

    CuAssertIntEquals(tc, 1, rb.cursor);
    CuAssertIntEquals(tc, 64, rb.capacity);

    uint8_t *expected = calloc(64, 1);
    *expected = data;
    CuAssertBytesEquals(tc, expected, rb.buf, 64);

    free(expected);
    free(rb.buf);
}

// Writing data small enoguh to not wrap
void test_ringbuf_write_nowrap(CuTest *tc) {
    struct ringbuf rb;
    ringbuf_initialize(64, &rb);

    uint8_t *data = (uint8_t *) "0123456789abcdefghijklmnopqrstuvwxys";
    ringbuf_write(&rb, data, strlen((char *) data));

    CuAssertIntEquals(tc, strlen((char *) data), rb.cursor);
    CuAssertIntEquals(tc, 64, rb.capacity);

    uint8_t *expected_buf = calloc(64, 1);
    memcpy(expected_buf, data, strlen((char *) data));
    CuAssertBytesEquals(tc, expected_buf, rb.buf, 64);

    free(expected_buf);
    free(rb.buf);
}

// Writing data that wraps around
void test_ringbuf_write_wrap_around(CuTest *tc) {
    struct ringbuf rb;
    ringbuf_initialize(64, &rb);
    rb.cursor = 60;

    uint8_t *data = (uint8_t *) "0123456789abcdefghijklmnopqrstuvwxys";
    ringbuf_write(&rb, data, strlen((char *) data));

    CuAssertIntEquals(tc, strlen((char *) data) - 4, rb.cursor);
    CuAssertIntEquals(tc, 64, rb.capacity);

    uint8_t *expected_buf = calloc(64, 1);
    memcpy(expected_buf + 60, data, 4);
    memcpy(expected_buf, data + 4, strlen((char *) data) - 4);
    CuAssertBytesEquals(tc, expected_buf, rb.buf, 64);

    free(expected_buf);
    free(rb.buf);
}

// Writing data whose size equals the capacity
void test_ringbuf_write_capacity(CuTest *tc) {
    struct ringbuf rb;
    ringbuf_initialize(64, &rb);

    uint8_t *data = malloc(64);
    for (int i = 0; i < 64; i ++) {
        data[i] = i;
    }
    ringbuf_write(&rb, data, 64);

    CuAssertIntEquals(tc, 0, rb.cursor);
    CuAssertIntEquals(tc, 64, rb.capacity);

    CuAssertBytesEquals(tc, data, rb.buf, 64);

    free(data);
    free(rb.buf);
}

void test_ringbuf_write_many_wrap_around(CuTest *tc) {
    // TODO
}

// Simple test that ringbuf_get wraps properly
void test_ringbuf_get_wrap_around(CuTest *tc) {
    struct ringbuf rb;
    ringbuf_initialize(8, &rb);

    ringbuf_write(&rb, (uint8_t *) "01234567", 8);

    for (uint8_t i = 0; i < 8; i++) {
        CuAssertIntEquals(tc, '7' - i, ringbuf_get(&rb, i));
        CuAssertIntEquals(tc, '7' - i, ringbuf_get(&rb, i+8));
        CuAssertIntEquals(tc, '7' - i, ringbuf_get(&rb, i+16));
        CuAssertIntEquals(tc, '7' - i, ringbuf_get(&rb, i+32));
        CuAssertIntEquals(tc, '7' - i, ringbuf_get(&rb, i+(1 << 31)));
        CuAssertIntEquals(tc, '7' - i, ringbuf_get(&rb, i-8));
        CuAssertIntEquals(tc, '7' - i, ringbuf_get(&rb, i-16));
        CuAssertIntEquals(tc, '7' - i, ringbuf_get(&rb, i-32));
        CuAssertIntEquals(tc, '7' - i, ringbuf_get(&rb, i-(1 << 31)));
    }

    free(rb.buf);
}

CuSuite *ringbuf_test_suite() {
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_ringbuf_write_empty);
    SUITE_ADD_TEST(suite, test_ringbuf_write_single);
    SUITE_ADD_TEST(suite, test_ringbuf_write_nowrap);
    SUITE_ADD_TEST(suite, test_ringbuf_write_wrap_around);
    SUITE_ADD_TEST(suite, test_ringbuf_write_capacity);
    SUITE_ADD_TEST(suite, test_ringbuf_write_many_wrap_around);
    SUITE_ADD_TEST(suite, test_ringbuf_get_wrap_around);
    return suite;
}
