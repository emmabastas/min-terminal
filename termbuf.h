#ifndef INCLUDED_TERMBUF_H
#define INCLUDED_TERMBUF_H

#include <stdlib.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "CuTest.h"

// Here are some styling attributes that can be set by ANSCI escape codes
// https://en.wikipedia.org/wiki/ANSI_escape_code#Select_Graphic_Rendition_parameters
// Not everyting is supported, in particular selecting alternative fonts with
// ESC[11m - ESC[19m.
#define STYLEFLAG_PLAIN     (1 << 0)
#define STYLEFLAG_BOLD      (1 << 1)
#define STYLEFLAG_FAINT     (1 << 2)
#define STYLEFLAG_ITALIC    (1 << 3)
#define STYLEFLAG_UNDERLINE (1 << 4)
#define STYLEFLAG_STRIKEOUT (1 << 5)
#define STYLEFLAG_UNUSED5   (1 << 6)
#define STYLEFLAG_UNUSED6   (1 << 7)
// Indicates that the entire termbuf_char represents the absence of data which
// is distinct from data that meerely looks empty, like a '\0' character.
#define STYLEFLAG_NO_DATA   0

// Represents a single unicode codepoint along with styling information such as
// color, if it's bold, italic, etc. Fits snuggly in 88-bits.
struct termbuf_char {
    uint32_t codepoint;
    uint8_t  style_flags;
    uint8_t  fg_color_r;
    uint8_t  fg_color_g;
    uint8_t  fg_color_b;
    uint8_t  bg_color_r;
    uint8_t  bg_color_g;
    uint8_t  bg_color_b;
};

// #if sizeof(termbuf_char) != 8
// #error "struct termbuf_char should be 64 bits."
// #endif

struct termbuf {
    int nrows;
    int ncols;
    int row;
    int col;
    uint8_t style_flags;
    uint8_t fg_color_r;
    uint8_t fg_color_g;
    uint8_t fg_color_b;
    uint8_t bg_color_r;
    uint8_t bg_color_g;
    uint8_t bg_color_b;
    struct termbuf_char *buf;
};

void termbuf_initialize(int nrows, int ncols, struct termbuf *tb_ret);
void termbuf_insert(struct termbuf *tb, uint32_t codepoint);
void termbuf_render(struct termbuf *tb,
                    Display *display,
                    int window,
                    int screen,
                    XftDraw *draw,
                    XftFont *font,
                    int cell_width,
                    int cell_height);

CuSuite *termbuf_test_suite();

#endif /* INCLUDED_TERMBUF_H */
