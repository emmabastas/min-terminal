#ifndef INCLUDED_TABSTOPS_H
#define INCLUDED_TABSTOPS_H

#include <stdint.h>

struct __attribute__((packed)) tabstops {
    uint64_t bs1;
    uint64_t bs2;
};

void tabstops_initialize(struct tabstops *ts_ret);
void tabstops_set(struct tabstops *ts, int position);
void tabstops_clear(struct tabstops *ts, int position);
void tabstops_clear_all(struct tabstops *ts);

#endif /* INCLUDED_TABSTOPS_H */
