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

        switch (ch) {
        case '\0':
            printf("\x1B[33m(\\0)<%d>\x1B[0m", ch);
            break;
        case '\a':
            printf("\x1B[33m(\\a)<%d>\x1B[0m", ch);
            break;
        case '\r':
            printf("\x1B[33m(\\r)<%d>\x1B[0m", ch);
            break;
        case '\n':
            printf("\x1B[33m(\\n)<%d>\x1B[0m", ch);
            break;
        case 0x1B:
            printf("\x1B[33m(ESC)<%d>\x1B[0m", ch);
            break;
        default:
            printf("\x1B[33m<%d>\x1B[0m", ch);
        }
    }
}
