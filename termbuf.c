#include "termbuf.h"

#include <stdbool.h>
#include <assert.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "CuTest.h"

// Shoutout https://poor.dev/blog/terminal-anatomy/



void termbuf_initialize(int nrows, int ncols, struct termbuf *tb_ret) {
    assert(nrows > 0);
    assert(ncols > 0);

    tb_ret->nrows = nrows;
    tb_ret->ncols = ncols;
    tb_ret->row = nrows;
    tb_ret->col = 1;
    tb_ret->style_flags = STYLEFLAG_PLAIN;
    tb_ret->fg_color_r = 255;
    tb_ret->fg_color_g = 255;
    tb_ret->fg_color_b = 255;
    tb_ret->bg_color_r = 10;
    tb_ret->bg_color_g = 10;
    tb_ret->bg_color_b = 10;

    tb_ret->buf = calloc(nrows * ncols, sizeof(struct termbuf_char));
    if (tb_ret->buf == NULL) {
        assert(false);
    }
}

void termbuf_insert(struct termbuf *tb, uint32_t codepoint) {
    size_t index = (tb->row - 1) * tb->ncols + (tb -> col - 1);
    tb->buf[index].codepoint = codepoint;
    // TODO pref: use memcpy instead.
    tb->buf[index].style_flags = tb->style_flags;
    tb->buf[index].fg_color_r = tb->fg_color_r;
    tb->buf[index].fg_color_g = tb->fg_color_g;
    tb->buf[index].fg_color_b = tb->fg_color_b;
    tb->buf[index].bg_color_r = tb->bg_color_r;
    tb->buf[index].bg_color_g = tb->bg_color_g;
    tb->buf[index].bg_color_b = tb->bg_color_b;

    tb->col ++;

    if (tb->col > tb->ncols) {
        tb->col = 1;
        tb->row ++;
        if (tb->row > tb->nrows) {
            //assert(false);
            tb->row = 1;
        }
    }
}

void termbuf_render(struct termbuf *tb,
                    Display *display,
                    int window,
                    int screen,
                    XftDraw *draw,
                    XftFont *font,
                    int cell_width,
                    int cell_height) {
    assert(cell_width > 0);
    assert(cell_height > 0);

    XRenderColor fg;
    XftColor color_foreground;
    GC gc = XCreateGC(display,
                      window,
                      0,
                      NULL);

    for (int row = 1; row <= tb->nrows; row ++) {
        for (int col = 1; col <= tb->ncols; col ++) {
            struct termbuf_char *c =
                tb->buf + (row - 1) * tb->ncols + col - 1;

            XSetForeground(display, gc,
                           (c->bg_color_r << 16)
                           + (c->bg_color_g << 8)
                           + c->bg_color_b);

            XFillRectangle(
                display,
                window,
                gc,
                (col - 1) * cell_width,
                (row - 1) * cell_height,
                cell_width,
                cell_height);

            fg = (XRenderColor) { c->fg_color_r << 8,
                                  c->fg_color_g << 8,
                                  c->fg_color_b << 8,
                                  65535 };

            XftColorAllocValue(
                display,
                DefaultVisual(display, screen),
                DefaultColormap(display, screen), &fg,
                &color_foreground);

            XftDrawStringUtf8(
              draw,
              &color_foreground,
              font,
              (col - 1) * cell_width,
              (row - 1) * cell_height + cell_height,
              (XftChar8 *) c,
              1);
        }
    }
}

/*
 * Unit tests bellow
 */

CuSuite *termbuf_test_suite() {
    CuSuite *suite = CuSuiteNew();
    return suite;
}
