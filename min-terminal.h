#ifndef INCLUDED_MIN_TERMINAL_H
#define INCLUDED_MIN_TERMINAL_H

void min_terminal_scroll_forward();
void min_terminal_scroll_backward();

int min_terminal_write_to_shellf(int pty, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

#endif /* INCLUDED_MIN_TERMINAL_H */
