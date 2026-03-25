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

// Invoked by the following
// ESC 8
// CSI u
// CSI ? 1048 l
//
// More info: https://github.com/ThomasDickey/esctest2/blob/master/esctest/tests/save_restore_cursor.py
void handle_restore_cursor(struct termbuf *tb) {
    // TODO handle these
    assert(tb->saved_row <= tb->nrows);
    assert(tb->saved_col <= tb->ncols);

    tb->row = tb->saved_row;
    tb->col = tb->saved_col;
}

// Invoked by the following
// ESC 7
// CSI s
// CSI ? 1048 h
void handle_save_cursor(struct termbuf *tb) {
    tb->saved_row = tb->row;
    tb->saved_col = tb->col;
}


// https://en.wikipedia.org/wiki/ANSI_escape_code#Select_Graphic_Rendition_parameters
void handle_select_graphics_rendition(struct termbuf *tb,
                                      uint16_t params[CSI_CHOMPING_MAX_PARAMS],
                                      int len) {
    uint16_t p1 = params[0];

    // ESC[m, or ESC[0m. Reset all graphical rendition flags.
    if (len == 0 || (len == 1 && p1 == 0)) {
        // These are all the flags that are set to zero.
        tb->flags &= ~(FLAG_BOLD
                       | FLAG_FAINT
                       | FLAG_ITALIC
                       | FLAG_UNDERLINE
                       | FLAG_STRIKEOUT
                       | FLAG_INVERT_COLORS);

        // Set foreground to "Bright white".
        tb->fg.r = tb->palette[15 * 3];
        tb->fg.g = tb->palette[15 * 3 + 1];
        tb->fg.b = tb->palette[15 * 3 + 2];

        // Set background color to "Black".
        tb->bg.r = tb->palette[0];
        tb->bg.g = tb->palette[1];
        tb->bg.b = tb->palette[2];
        return;
    }

    // We got something else, iterate through each parameter
    for (int i = 0; i < len; i++) {
        int param = params[i];
        switch (param) {
        case 1:  // Bold.
            tb->flags |= FLAG_BOLD;
            continue;
        case 2:  // Faint.
            tb->flags |= FLAG_FAINT;
            continue;
        case 3:  // Italic.
            tb->flags |= FLAG_ITALIC;
            continue;
        case 4:  // Underline.
            tb->flags |= FLAG_UNDERLINE;
            continue;
        case 5:  // Slow blink.
            assert(false);
        case 6:  // Rapid blink.
            assert(false);
        case 7:  // Swap foreground and background colors.
            tb->flags |= FLAG_INVERT_COLORS;
            continue;
        case 8:  // Invisible text.
            assert(false);
        case 9:  // Strikeout.
            assert(false);
        case 10:  // Prmiary font.
            assert(false);
        case 11:  //  Alernative font 1.
            assert(false);
        case 12:  //  Alernative font 2.
            assert(false);
        case 13:  //  Alernative font 3.
            assert(false);
        case 14:  //  Alernative font 4.
            assert(false);
        case 15:  //  Alernative font 5.
            assert(false);
        case 16:  //  Alernative font 6.
            assert(false);
        case 17:  //  Alernative font 7.
            assert(false);
        case 18:  //  Alernative font 8.
            assert(false);
        case 19:  //  Alernative font 9.
            assert(false);
        case 20:  // Fraktur font.
            assert(false);
        case 21:  // Doubly underlined or not bold.
            assert(false);
        case 22:  // Neither bold nor faint.
            tb->flags &= ~FLAG_BOLD;
            tb->flags &= ~FLAG_FAINT;
            continue;
        case 23:  // Neither italic "blackletter". (?)
            // Right now we don't support blackletter, so we only unset the
            // italic flag
            tb->flags &= ~ FLAG_ITALIC;
            continue;
        case 24:  // Not underlined.
            tb->flags &= ~FLAG_UNDERLINE;
            continue;
        case 25:  // Not blinking.
            assert(false);
        case 26:  // Proportional spacing (not known to be used on terms).
            assert(false);
        case 27:  // Not reversed (i.e. undo case 7).
            tb->flags &= ~FLAG_INVERT_COLORS;
            continue;
        case 28:  // Not concealed.
            assert(false);
        case 29:  // Not crossed out.
            assert(false);
        case 30:  // Foreground color 1.
        case 31:  // Foreground color 2.
        case 32:  // Foreground color 3.
        case 33:  // Foreground color 4.
        case 34:  // Foreground color 5.
        case 35:  // Foreground color 7.
        case 36:  // Foreground color 7.
        case 37:  // Foreground color 8.
            {
                int i = param - 30;
                assert(0 <= i && i <= 8);
                tb->fg.r = tb->palette[i * 3];
                tb->fg.g = tb->palette[i * 3 + 1];
                tb->fg.b = tb->palette[i * 3 + 2];
                continue;
            }
        case 38:  // Set 8-bit foreground color or rgb color.
            // We should have recived a sequence in one of the two forms
            // 1)
            //    ESC[5;<n>m where <n> is some number in the range 0-255.
            // In this case <n> is one of 256 preset colors.
            // 2)
            //    ESC[2;<r>;<g>;<b>m
            // In this case we have a specific rgb color specified.

            // Before we do this we must handle an important case!
            // It if possible we recive a string like ESC[1;38;200 which we
            // should interpret as "Set bold font (1) and set fg color 200
            // (38;200). I.e. it could very well be that '38' is not the
            // first param and '200' is not the second, all we not is
            // the param we we're currently lookin at (param i) is '38'
            // and the next one should be '200'.

            // There should be at least one parameter following the '38'.
            assert(i + 1 < len);

            // We expect `q` to be either '5' or '2'..
            uint8_t q = params[i + 1];

            // Set 8-bit foreground color.
            if (q == 5) {
                // There should be at least one parameter following the '5'.
                assert(i + 2 < len);
                uint16_t q2 = params[i + 2];
                q2 = q2 == (uint16_t) -1 ? 0 : q2;
                assert(q2 <= 255);
                tb->fg.r = tb->palette[q2 * 3];
                tb->fg.g = tb->palette[q2 * 3 + 1];
                tb->fg.b = tb->palette[q2 * 3 + 2];

                // Continue parsing any potential remaining graphics
                // parameters.
                i += 2;
                continue;
            }

            // Set rgb color.
            if (q == 2) {
                // There should be at least three parameters following the
                // '2'.
                assert(i + 4 < len);
                uint16_t q2 = params[i + 2]; // red.
                uint16_t q3 = params[i + 3]; // green.
                uint16_t q4 = params[i + 4]; // blue.
                assert(q2 <= 255);
                assert(q3 <= 255);
                assert(q4 <= 255);
                tb->fg.r = q2;
                tb->fg.g = q3;
                tb->fg.b = q4;

                // Continue parsing any potential remaining graphics
                // parameters.
                i += 4;
                continue;
            }

            assert(false);
        case 39:  // Default foreground color.
            // Here we as the implementor apparently get to pick a color we
            // like to be the default foreground color
            // (according to wikipedia). Let's just pick the 4-bit "bright
            // white" to be our default.
            {
                int i = 15;
                tb->fg.r = tb->palette[i * 3];
                tb->fg.g = tb->palette[i * 3 + 1];
                tb->fg.b = tb->palette[i * 3 + 2];
                continue;
            }
        case 40:  // Background color 1.
        case 41:  // Background color 2.
        case 42:  // Background color 3.
        case 43:  // Background color 4.
        case 44:  // Background color 5.
        case 45:  // Background color 6.
        case 46:  // Background color 7.
        case 47:  // Background color 8.
            {
                int i = param - 40;
                assert(0 <= i && i <= 8);
                tb->bg.r = tb->palette[i * 3];
                tb->bg.g = tb->palette[i * 3 + 1];
                tb->bg.b = tb->palette[i * 3 + 2];
                continue;
            }
        case 48:  // Set 8-bit foreground color or rgb color.
            // For more information, see how case 38 is handlede, this case
            // is the same but for background colors.

            // There should be at least one parameter following the '38'.
            assert(i + 1 < len);

            // We expect `q` to be either '5' or '2'..
            q = params[i + 1];

            // Set 8-bit background color.
            if (q == 5) {
                // There should be at least one parameter following the '5'.
                assert(i + 2 < len);
                uint16_t q2 = params[i + 2];
                q2 = q2 == (uint16_t) -1 ? 0 : q2;
                assert(q2 <= 255);
                tb->bg.r = tb->palette[q2 * 3];
                tb->bg.g = tb->palette[q2 * 3 + 1];
                tb->bg.b = tb->palette[q2 * 3 + 2];

                // Continue parsing any potential remaining graphics
                // parameters.
                i += 2;
                continue;
            }

            // Set rgb color.
            if (q == 2) {
                // There should be at least three parameters following the
                // '2'.
                assert(i + 4 < len);
                uint16_t q2 = params[i + 2]; // red.
                uint16_t q3 = params[i + 3]; // green.
                uint16_t q4 = params[i + 4]; // blue.
                assert(q2 <= 255);
                assert(q3 <= 255);
                assert(q4 <= 255);
                tb->bg.r = q2;
                tb->bg.g = q3;
                tb->bg.b = q4;

                // Continue parsing any potential remaining graphics
                // parameters.
                i += 4;
                continue;

            }

            assert(false);
        case 49:  // Default background color.
            // See: case 39

            // Set background color to black.
            tb->bg.r = tb->palette[0];
            tb->bg.g = tb->palette[1];
            tb->bg.b = tb->palette[2];
            continue;
        case 50:
            assert(false);
        case 51:
            assert(false);
        case 52:
            assert(false);
        case 53:
            assert(false);
        case 54:
            assert(false);
        case 55:
            assert(false);
        case 56:
            assert(false);
        case 57:
            assert(false);
        case 58:
            assert(false);
        case 59:
            assert(false);
        case 60:
            assert(false);
        case 61:
            assert(false);
        case 62:
            assert(false);
        case 63:
            assert(false);
        case 64:
            assert(false);
        case 65:
            assert(false);
        case 66:
            assert(false);
        case 67:
            assert(false);
        case 68:
            assert(false);
        case 69:
            assert(false);
        case 70:
            assert(false);
        case 71:
            assert(false);
        case 72:
            assert(false);
        case 73:
            assert(false);
        case 74:
            assert(false);
        case 75:
            assert(false);
        case 76:
            assert(false);
        case 77:
            assert(false);
        case 78:
            assert(false);
        case 79:
            assert(false);
        case 80:
            assert(false);
        case 81:
            assert(false);
        case 82:
            assert(false);
        case 83:
            assert(false);
        case 84:
            assert(false);
        case 85:
            assert(false);
        case 86:
            assert(false);
        case 87:
            assert(false);
        case 88:
            assert(false);
        case 89:
            assert(false);
        case 90:  // Set bright foregrund color 1.
        case 91:  // Set bright foregrund color 2.
        case 92:  // Set bright foregrund color 3.
        case 93:  // Set bright foregrund color 4.
        case 94:  // Set bright foregrund color 5.
        case 95:  // Set bright foregrund color 6.
        case 96:  // Set bright foregrund color 7.
        case 97:  // Set bright foregrund color 8.
            {
                int i = param - 90;
                assert(0 <= i && i <= 8);
                tb->fg.r = tb->palette[(i + 8) * 3];
                tb->fg.g = tb->palette[(i + 8) * 3 + 1];
                tb->fg.b = tb->palette[(i + 8) * 3 + 2];
                continue;
            }
        case 98:
            assert(false);
        case 99:
            assert(false);
        case 100:  // Set bright background color 1.
        case 101:  // Set bright background color 2.
        case 102:  // Set bright background color 3.
        case 103:  // Set bright background color 4.
        case 104:  // Set bright background color 5.
        case 105:  // Set bright background color 6.
        case 106:  // Set bright background color 7.
        case 107:  // Set bright background color 8.
            {
                int i = param - 100;
                assert(0 <= i && i <= 8);
                tb->bg.r = tb->palette[(i + 8) * 3];
                tb->bg.g = tb->palette[(i + 8) * 3 + 1];
                tb->bg.b = tb->palette[(i + 8) * 3 + 2];
                continue;
            }
        }
    }
    return;

}
