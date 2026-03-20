#include <assert.h>

#include "tabstops.h"

void tabstops_initialize(struct tabstops *ts_ret) {
    ts_ret->bs1 = 0;
    ts_ret->bs2 = 0;
}

void tabstops_set(struct tabstops *ts, int position) {
    assert(0 < position && position < 128);

    // TODO make brancheless?
    int offset = 0;
    if (position > 64) {
        offset = 1;
        position -= 64;
    }

    *((uint64_t *) ts + offset) |= ((uint64_t) 1 << position);
}
