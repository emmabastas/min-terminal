#include <stdio.h>
#include <assert.h>

#include "./handlers.h"
#include "./min-terminal.h"

void xterm_winops_report(struct termbuf *tb, int n, int x, int y);

/* CSI Ps t -- xterm Window Operations.

   Reference:
   - https://tintin.mudhalla.net/info/xterm/
   - https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character-lparen-s-rparen:CSI-Ps;Ps;Ps-t.1EB0

 */
void handle_xterm_winops(struct termbuf *tb,
                         uint16_t p1,
                         uint16_t p2,
                         uint16_t p3,
                         int len) {
    // CSI 1 t -- De-iconify window
    // CSI 2 t -- Iconify window
    if ((p1 == 1 || p1 == 2) && len == 1) {
        return; // We don't do icons.
    }

    // CSI 8 Ph Pw t -- Resize window.
    // Resize text area to given width and height.
    // Parameter ommited => use current width / height.
    // Parameter is 0 => use max width / height possible.
    if (p1 == 8 && len >= 1) {
        uint16_t nnrows = len < 2 ? tb->nrows : p2;
        uint16_t nncols = len < 3 ? tb->ncols : p3;

        termbuf_resize(tb, nnrows, nncols);

        // TODO: Resize the actual window too.

        return;
    }

    // CSI 18 t -- Report SIZE in characters.
    if (p1 == 18 && len == 1) {
        xterm_winops_report(tb, 8, tb->nrows, tb->ncols);
        return;
    }

    // CSI 23 Ps t
    // These are all related to pushing and popping window titles and icons
    // to /from on a stack. Since we're not interested in implementing this
    // we just ignore.
    if (len >= 1 && p1 == 23) {
        assert(p2 <= 2);
        return;
    }

    unknown_csi(tb, 't', __FILE__, __LINE__);

    if (ON_UNKNOWN_SEQUENCE == FAIL) {
        exit(-1);
    }
    return;
}

void xterm_winops_report(struct termbuf *tb, int n, int x, int y) {
    min_terminal_write_to_shellf(tb->pty_fd, "\x1B[%d;%d;%dt", n, x, y);
}
