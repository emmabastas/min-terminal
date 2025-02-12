#ifndef INCLUDED_DIAGNOSTICS_H
#define INCLUDED_DIAGNOSTICS_H

#include <stddef.h>

enum diagnostics_type_e {
    DIAGNOSTICS_MISC             = 1 << 0,
    DIAGNOSTICS_X11_EVENT        = 1 << 1,
    DIAGNOSTICS_TERM_PARSE_INPUT = 1 << 2,
    DIAGNOSTICS_TERM_PARSE_STATE = 1 << 3,
    DIAGNOSTICS_TERM_RESPONSE    = 1 << 4,
    DIAGNOSTICS_EVENT_LOOP       = 1 << 5,
    DIAGNOSTICS_NONE             = 0,
    DIAGNOSTICS_ALL              = 255,
};

void diagnostics_initialize(void);
void diagnostics_type(enum diagnostics_type_e);
void diagnostics_write_string(char *s, size_t len);
void diagnostics_write_string_escape_non_printable(char *data, size_t len);
void diagnostics_write_int(int n);
void diagnostics_flush(void);

#endif /* INCLUDED_DIAGNOSTICS_H */
