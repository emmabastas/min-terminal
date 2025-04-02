#include "./diagnostics.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>



static int current_type;
static bool matches;
static const int MASK = DIAGNOSTICS_ALL;

void diagnostics_initialize(void) {
    diagnostics_type(DIAGNOSTICS_MISC);
}

void diagnostics_type(enum diagnostics_type_e t) {
    current_type = t;
    matches = (MASK & t) != 0;
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

void diagnostics_flush(void) {
    fflush(stderr);
}
