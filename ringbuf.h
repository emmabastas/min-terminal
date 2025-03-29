#ifndef INCLUDED_RINGBUF_H
#define INCLUDED_RINGBUF_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "CuTest.h"

struct ringbuf {
    uint8_t *buf;
    uint8_t *extra;
    bool continous_memory;
    size_t cursor;
    size_t size;
    size_t capacity;
};

enum ringbuf_capacity {
    RINGBUF_CAPACITY_1 = 1,
    RINGBUF_CAPACITY_2 = 2,
    RINGBUF_CAPACITY_4 = 4,
    RINGBUF_CAPACITY_8 = 8,
    RINGBUF_CAPACITY_16 = 16,
    RINGBUF_CAPACITY_32 = 32,
    RINGBUF_CAPACITY_64 = 64,
    RINGBUF_CAPACITY_128 = 128,
    RINGBUF_CAPACITY_256 = 256,
    RINGBUF_CAPACITY_512 = 512,
    RINGBUF_CAPACITY_1KiB = 1024,
    RINGBUF_CAPACITY_2KiB = 2048,
    RINGBUF_CAPACITY_4KiB = 4096,
    RINGBUF_CAPACITY_8KiB = 8192,
    RINGBUF_CAPACITY_16KiB = 16384,
    RINGBUF_CAPACITY_32KiB = 32768,
    RINGBUF_CAPACITY_64KiB = 65536,
};

enum offset_result {
    RINGBUF_SUCCESS = 0,
    RINGBUF_OUT_OF_BOUNDS = 1,
    RINGBUF_TOO_LARGE = 2,
};

void ringbuf_initialize(enum ringbuf_capacity cap,
                        bool continous_memory,
                        struct ringbuf *rb_ret);
void ringbuf_free(struct ringbuf *rb);
void ringbuf_write(struct ringbuf *rb, uint8_t *data, size_t len);
uint8_t ringbuf_get(struct ringbuf *rb, size_t offset);
enum offset_result ringbuf_getp(struct ringbuf *rb,
                                size_t offset,
                                size_t len,
                                void **data_ret);

CuSuite *ringbuf_test_suite();

#endif /* INCLUDED_RINGBUF_H */
