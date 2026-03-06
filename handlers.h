#ifndef INCLUDED_HANDLERS_H
#define INCLUDED_HANDLERS_H

#include <stdint.h>
#include "./termbuf.h"

void handle_xterm_winops(struct termbuf *tb,
                         uint16_t p1,
                         uint16_t p2,
                         uint16_t p3,
                         int len);

#endif /* INCLUDED_HANDLERS_H */
