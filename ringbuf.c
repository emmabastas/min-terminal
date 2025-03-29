#include "ringbuf.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>

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

void ringbuf_initialize(enum ringbuf_capacity cap,
                        bool continous_memory,
                        struct ringbuf *rb_ret) {
    assert(cap == 1 << 2
           || cap == 1 << 3
           || cap == 1 << 4
           || cap == 1 << 5
           || cap == 1 << 6
           || cap == 1 << 7
           || cap == 1 << 8
           || cap == 1 << 9
           || cap == 1 << 10
           || cap == 1 << 11
           || cap == 1 << 12
           || cap == 1 << 13
           || cap == 1 << 14
           || cap == 1 << 15
           || cap == 1 << 16
           || cap == 1 << 17
           || cap == 1 << 18
           || cap == 1 << 19
           || cap == 1 << 20
           || cap == 1 << 21
           || cap == 1 << 22
           || cap == 1 << 23
           || cap == 1 << 24);

    if (continous_memory) {
        const long page_size = sysconf(_SC_PAGE_SIZE);

        // align capacity uppwards to the nearest page bondary.
        size_t aligned_capacity =
            ((cap + page_size - 1) / page_size) * page_size;

        uint8_t *buf = mmap(NULL,
                            aligned_capacity,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS,
                            -1,
                            0);
        if (buf == MAP_FAILED) {
            assert(false);
        }

        uint8_t *extra = mmap(buf,
                              page_size,
                              PROT_READ | PROT_WRITE,
                              // Acording to section: Using MAP_FIXED safely of
                              // man 2 mmap MAP_ANONYMOUS can be considered
                              // because we're mapping over memro that  we've
                              // already mmap'ed.
                              MAP_PRIVATE | MAP_ANONYMOUS | MAP_ANONYMOUS,
                              -1,
                              0);
        if (buf == MAP_FAILED) {
            assert(false);
        }


        rb_ret->buf = buf;
        rb_ret->extra = extra;
        rb_ret->continous_memory = true;
        rb_ret->cursor = 0;
        rb_ret->size = 0;
        rb_ret->capacity = aligned_capacity;
    } else {
        uint8_t *buf = calloc(cap, 1);
        if (buf == NULL) {
            // calloc failure
            assert(false);
        }

        rb_ret->buf = buf;
        rb_ret->extra = NULL;
        rb_ret->continous_memory = false;
        rb_ret->cursor = 0;
        rb_ret->size = 0;
        rb_ret->capacity = cap;
    }
}

void ringbuf_free(struct ringbuf *rb) {
    if (rb->continous_memory) {
        const long page_size = sysconf(_SC_PAGE_SIZE);
        munmap(rb->buf, rb->capacity);
        munmap(rb->extra, page_size);
    } else {
        free(rb->buf);
        assert(rb->extra == NULL);
    }
}

void ringbuf_write(struct ringbuf *rb, void *data, size_t len) {
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
        data = (char *)data + len - rb->capacity;
        len = rb->capacity;
    }

    size_t end_size = min(rb->capacity - rb->cursor, len);
    assert(rb->cursor + end_size <= rb->capacity);
    assert(end_size <= len);
    memcpy((char *)rb->buf + rb->cursor, data, end_size);

    size_t begining_size = len - end_size;
    assert(end_size + begining_size == len);
    memcpy((char *)rb->buf, (char *)data + end_size, begining_size);

    rb->cursor = (rb->cursor + len) & (rb->capacity - 1);
    rb->size = (rb->size + len) & (rb->capacity - 1);
}

uint8_t ringbuf_get(struct ringbuf *rb, size_t offset) {
    return ((uint8_t *) rb->buf)[(rb->cursor - 1 - offset) & (rb->capacity - 1)];
}

enum offset_result ringbuf_getp(struct ringbuf *rb,
                                size_t offset,
                                size_t len,
                                void **data_ret) {
    const long page_size = sysconf(_SC_PAGE_SIZE);

    if (!rb->continous_memory) {
        return RINGBUF_DISCONTINOUS_MEMORY;
    }

    if (len > page_size) {
        return RINGBUF_TOO_LARGE;
    }

    if (offset + len > rb->size) {
        return RINGBUF_OUT_OF_BOUNDS;
    }

    *data_ret = (char *)rb->buf + offset;
    return RINGBUF_SUCCESS;
}



////////////////
// UNIT TESTS //
////////////////


// Writing no data should leave the ringbuffer unaltered
void test_ringbuf_write_empty(CuTest *tc) {
    struct ringbuf rb;
    ringbuf_initialize(64, false, &rb);

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
    ringbuf_initialize(64, false, &rb);

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
    ringbuf_initialize(64, false, &rb);

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
    ringbuf_initialize(64, false, &rb);
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
    ringbuf_initialize(64, false, &rb);
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
    ringbuf_initialize(8, false, &rb);

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

// Ringbufs with continous memory have their capacity as a multiple of the page
// size.
void test_ringbuf_page_aligned(CuTest *tc) {
    const long page_size = sysconf(_SC_PAGE_SIZE);

    // Assert that page size is a power of two.
    assert((page_size & (page_size - 1)) == 0);

    size_t cap1, cap2, cap3;

    struct ringbuf rb;
    ringbuf_initialize(page_size / 2, true, &rb);
    cap1 = rb.capacity;
    ringbuf_free(&rb);

    ringbuf_initialize(page_size, true, &rb);
    cap2 = rb.capacity;
    ringbuf_free(&rb);

    ringbuf_initialize(page_size * 2, true, &rb);
    cap3 = rb.capacity;
    ringbuf_free(&rb);

    CuAssertIntEquals(tc, page_size, cap1);
    CuAssertIntEquals(tc, page_size, cap2);
    CuAssertIntEquals(tc, 2 * page_size, cap3);
}

// Test that continous_memory gives the illusion of continous memory over the
// buffer boundary.
void test_ringbuf_continous_memory(CuTest *tc) {
    const long page_size = sysconf(_SC_PAGE_SIZE);

    struct ringbuf rb;
    ringbuf_initialize(page_size * 2, true, &rb);

    // Fill the first `page_size - 3` bytes with 'x'
    char *data = malloc(page_size - 3);
    memset(data, 'x', page_size - 3);
    ringbuf_write(&rb, (uint8_t *) data, page_size - 3);

    // Add an additional 10 bytes
    ringbuf_write(&rb, (uint8_t *) "0123456789", 10);

    // Now we expect the ring buffer to look like this:
    // ╭──────────────────────────────────────────────╮
    // │3456789xxxxx...                     ...xxxx012│
    // ╰──────────────────────────────────────────────╯
    //                                             ^ page_size - 3
    //
    // However, if we call ringbuf_getp with `offset = page_size - 3` and
    // `len = 10` it should give us a pointer to this chunk of (virtual) memory:
    // 0123456789xxxx...

    char *result;
    ringbuf_getp(&rb, page_size - 3, 10, (void **) &result);

    CuAssertBytesEquals(tc, (uint8_t *) "0123456789", (uint8_t *) result, 10);
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
    SUITE_ADD_TEST(suite, test_ringbuf_page_aligned);
    SUITE_ADD_TEST(suite, test_ringbuf_continous_memory);
    return suite;
}
