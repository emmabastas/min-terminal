#include <stdio.h>
#include <stddef.h>

void print_escape_non_printable(unsigned char *data, size_t len) {
    for(size_t i = 0; i < len; i++) {
        unsigned char ch = data[i];
        // Is it a printable char?
        if (32 <= ch && ch <= 126) {
            printf("%c", ch);
            continue;
        }

        // It's non-printable
        printf("\x1B[33m<%d>\x1B[0m", ch);
    }
}
