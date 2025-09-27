#include "./diagnostics.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>



static int current_type;
static bool matches;
static const int MASK = DIAGNOSTICS_ALL;

// Temporary buffer used to avoid malloc etc.
#define DIAGNOSTICS_TMP_BUF_SIZE 1024
static char DIAGNOSTICS_TMP_BUF[DIAGNOSTICS_TMP_BUF_SIZE];

void diagnostics_initialize(void) {
    diagnostics_type(DIAGNOSTICS_MISC, __FILE__, __LINE__);
}

void diagnostics_type(enum diagnostics_type_e t, char *filename, int line) {
    current_type = t;
    matches = (MASK & t) != 0;
    if (!matches) {
        return;
    }

    char *s = "";
    switch(t) {
    case DIAGNOSTICS_TERM_CODE_ERROR:
        s = "\x1B[34mCODE_ERROR> \x1B[0m";
        break;
    default:
        return;
    }
    fprintf(stderr, "\n%s:%d ", filename, line);
    fwrite(s, sizeof(char), strlen(s), stderr);
}

void diagnostics_write_string(const char *s, int len) {
    assert(len == -1 || len >= 0);

    if (!matches) {
        return;
    }

    fwrite(s, sizeof(char), len > -1 ? (size_t) len : strlen(s), stderr);
}

void diagnostics_write_string_escape_non_printable(const char *data, int len) {
    if (!matches) {
        return;
    }

    for(int i = 0; i < len; i++) {
        unsigned char ch = data[i];
        // Is it a printable char?
        if (32 <= ch && ch <= 126) {
            fprintf(stderr, "%c", ch);
            continue;
        }

        switch (ch) {
        case '\0':
            fprintf(stderr, "\x1B[33m(\\0)<%d>\x1B[0m", ch);
            break;
        case '\a':
            fprintf(stderr, "\x1B[33m(\\a)<%d>\x1B[0m", ch);
            break;
        case '\r':
            fprintf(stderr, "\x1B[33m(\\r)<%d>\x1B[0m", ch);
            break;
        case '\n':
            fprintf(stderr, "\x1B[33m(\\n)<%d>\x1B[0m", ch);
            break;
        case 0x1B:
            fprintf(stderr, "\x1B[33m(ESC)<%d>\x1B[0m", ch);
            break;
        default:
            fprintf(stderr, "\x1B[33m<%d>\x1B[0m", ch);
        }
    }
}

void diagnostics_write_int(int n) {
    if (!matches) {
        return;
    }
    fprintf(stderr, "%d", n);
}

void diagnostics_printfe(const char *format, ...) {
    va_list argp;
    va_start(argp, format);
    diagnostics_vprintfe(format, argp);
    va_end(argp);
}

void diagnostics_vprintfe(const char *format, va_list argp) {
    if (!matches) {
        return;
    }

    int did_write = vsnprintf(DIAGNOSTICS_TMP_BUF,
                              DIAGNOSTICS_TMP_BUF_SIZE,
                              format,
                              argp);

    // Output was truncated.
    if (did_write >= DIAGNOSTICS_TMP_BUF_SIZE) {
        assert(false);
    }

    // Error occured.
    if (did_write == -1) {
        assert(false);
    }

    diagnostics_write_string_escape_non_printable(DIAGNOSTICS_TMP_BUF, did_write);
}

void diagnostics_printf(const char *format, ...) {
    va_list argp;
    va_start(argp, format);
    diagnostics_vprintf(format, argp);
    va_end(argp);
}

void diagnostics_vprintf(const char *format, va_list argp) {
    if (!matches) {
        return;
    }

    const int ret = vprintf(format, argp);

    // Error occured.
    if (ret == -1) {
        assert(false);
    }
}

void diagnostics_flush(void) {
    fflush(stderr);
}
