#ifndef INCLUDED_RINGBUF_H
#define INCLUDED_RINGBUF_H

#include <stdlib.h>
#include <stdint.h>

#include "CuTest.h"

struct ringbuf {
    uint8_t *buf;
    size_t cursor;
    size_t capacity;
};

void ringbuf_initialize(size_t capacity, struct ringbuf *rb_ret);
void ringbuf_write(struct ringbuf *rb, uint8_t *data, size_t len);
uint8_t ringbuf_get(struct ringbuf *rb, size_t offset);

CuSuite *ringbuf_test_suite();

#endif /* INCLUDED_RINGBUF_H */
