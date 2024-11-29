#ifndef INCLUDED_TERMBUF_H
#define INCLUDED_TERMBUF_H

#include <stdlib.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "./CuTest.h"

#define FLAG_LENGTH_0 0    // 0b00000000
#define FLAG_LENGTH_1 1    // 0b00000001
#define FLAG_LENGTH_2 2    // 0b00000010
#define FLAG_LENGTH_3 3    // 0b00000011
#define FLAG_LENGTH_4 4    // 0b00000100
#define FLAG_LENGTH_MASK 7 // 0b00000111
#define LAG_BOLD      8    // 0b00001000
#define LAG_FAINT     16   // 0b00010000
#define LAG_ITALIC    32   // 0b00100000
#define LAG_UNDERLINE 64   // 0b01000000
#define LAG_STRIKEOUT 128  // 0b10000000

// Represents a single unicode codepoint along with styling information such as
// color, if it's bold, italic, etc. Fits snuggly in 88-bits.
struct termbuf_char {
    uint8_t utf8_char[4];
    uint8_t flags;
    uint8_t fg_color_r;
    uint8_t fg_color_g;
    uint8_t fg_color_b;
    uint8_t bg_color_r;
    uint8_t bg_color_g;
    uint8_t bg_color_b;
};

enum parser_state {
    P_STATE_GROUND = 0,
    P_STATE_CHOMP1 = 1,
    P_STATE_CHOMP2 = 2,
    P_STATE_CHOMP3 = 3,
};
#define NSTATES 4

union parser_data {
    struct utf8_chomping {
        uint8_t len;
        uint8_t utf8_char[4];
    } utf8_chomping;
};

struct termbuf {
    int nrows;
    int ncols;
    int row;
    int col;
    uint8_t flags;
    uint8_t fg_color_r;
    uint8_t fg_color_g;
    uint8_t fg_color_b;
    uint8_t bg_color_r;
    uint8_t bg_color_g;
    uint8_t bg_color_b;
    enum  parser_state p_state;
    union parser_data  p_data;
    struct termbuf_char *buf;
};

void termbuf_initialize(int nrows, int ncols, struct termbuf *tb_ret);
void termbuf_parse(struct termbuf *tb, uint8_t *data, size_t len);
void termbuf_insert(struct termbuf *tb, uint8_t *utf8_char, int len);
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
