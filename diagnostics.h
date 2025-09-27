#ifndef INCLUDED_DIAGNOSTICS_H
#define INCLUDED_DIAGNOSTICS_H

#include <stddef.h>
#include <stdarg.h>

enum diagnostics_type_e {
    DIAGNOSTICS_MISC             = 1 << 0,
    DIAGNOSTICS_X11_EVENT        = 1 << 1,
    DIAGNOSTICS_TERM_PARSE_INPUT = 1 << 2,
    DIAGNOSTICS_TERM_PARSE_STATE = 1 << 3,
    DIAGNOSTICS_TERM_CODE_ERROR  = 1 << 4,
    DIAGNOSTICS_TERM_RESPONSE    = 1 << 5,
    DIAGNOSTICS_EVENT_LOOP       = 1 << 7,
    DIAGNOSTICS_ALL              = (1 << 8) - 1,
    DIAGNOSTICS_NONE             = 0,
};

void diagnostics_initialize(void);
void diagnostics_type(enum diagnostics_type_e, char *filename, int line);
void diagnostics_write_string(const char *s, int len);
void diagnostics_write_string_escape_non_printable(const char *data, int len);
void diagnostics_write_int(int n);
void diagnostics_printf(const char *format, ...)
    __attribute__((format(printf, 1, 2)));
void diagnostics_vprintf(const char *format, va_list argp);
void diagnostics_printfe(const char *format, ...)
    __attribute__((format(printf, 1, 2)));
void diagnostics_vprintfe(const char *format, va_list argp);
void diagnostics_flush(void);

#endif /* INCLUDED_DIAGNOSTICS_H */
