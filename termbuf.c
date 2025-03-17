#include "termbuf.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>

#include "./diagnostics.h"
#include "./util.h"
#include "./CuTest.h"



// Shoutout https://poor.dev/blog/terminal-anatomy/
// and      https://vt100.net/emu/dec_ansi_parser
// and      https://unix.stackexchange.com/questions/157878/non-printing-escape-sequence-when



void unknown_csi(struct termbuf *tb, char final_byte);
void csi_dec_private_mode_set(struct termbuf *tb, char final_byte);



// 3/4 -bit colors.
// According to the standard there are 8 predetermined (though configurable by
// the user) than can be used to specify foreground an background colors. You
// select these colors via SRG parameters 30-37 for foreground colors, and 40-47
// for background colors. For instance ESC[31;42m sets the foreground to clolor
// 1 and background to color 2.
// https://en.wikipedia.org/wiki/ANSI_escape_code#3-bit_and_4-bit
const uint8_t four_bit_colors[16 * 3] = {
    //                 FG/BG Name.
    0  , 0  , 0  ,  // 30/40 Black.
    153, 0  , 0  ,  // 31/41 Red.
    0  , 166, 0  ,  // 32/42 Green.
    153, 153, 153,  // 33/43 Yello.
    0  , 0  , 178,  // 34/44 Blue
    178, 0  , 178,  // 35/45 Magenta.
    0  , 166, 178,  // 36/46 Cyan.
    191, 191, 191,  // 37/47 White
    // bright variants. Selected with codes 90-97 and 100-107 for fg resp. bg.
    102, 102, 102,
    230, 0  , 0  ,
    0  , 217, 0  ,
    230, 230, 0  ,
    0  , 0  , 255,
    230, 0  , 230,
    0  , 230, 230,
    230, 230, 230,
};

// 8-bit colors.
// TODO: The 3/4 bit colors list above should correspond to the first 8/16
//       colors of this list? I.e. we don't need the list above?
// https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit
const uint8_t eight_bit_colors[256 * 3] = {
    0, 0, 0,
    128, 0, 0,
    0, 128, 0,
    128, 128, 0,
    0, 0, 128,
    128, 0, 128,
    0, 128, 128,
    192, 192, 192,
    128, 128, 128,
    255, 0, 0,
    0, 255, 0,
    255, 255, 0,
    0, 0, 255,
    255, 0, 255,
    0, 255, 255,
    255, 255, 255,
    0, 0, 0,
    0, 0, 95,
    0, 0, 135,
    0, 0, 175,
    0, 0, 215,
    0, 0, 255,
    95, 0, 0,
    95, 0, 95,
    95, 0, 135,
    95, 0, 175,
    95, 0, 215,
    95, 0, 255,
    135, 0, 0,
    135, 0, 95,
    135, 0, 135,
    135, 0, 175,
    135, 0, 215,
    135, 0, 255,
    175, 0, 0,
    175, 0, 95,
    175, 0, 135,
    175, 0, 175,
    175, 0, 215,
    175, 0, 255,
    215, 0, 0,
    215, 0, 95,
    215, 0, 135,
    215, 0, 175,
    215, 0, 215,
    215, 0, 255,
    255, 0, 0,
    255, 0, 95,
    255, 0, 135,
    255, 0, 175,
    255, 0, 215,
    255, 0, 255,
    0, 95, 0,
    0, 95, 95,
    0, 95, 135,
    0, 95, 175,
    0, 95, 215,
    0, 95, 255,
    95, 95, 0,
    95, 95, 95,
    95, 95, 135,
    95, 95, 175,
    95, 95, 215,
    95, 95, 255,
    135, 95, 0,
    135, 95, 95,
    135, 95, 135,
    135, 95, 175,
    135, 95, 215,
    135, 95, 255,
    175, 95, 0,
    175, 95, 95,
    175, 95, 135,
    175, 95, 175,
    175, 95, 215,
    175, 95, 255,
    215, 95, 0,
    215, 95, 95,
    215, 95, 135,
    215, 95, 175,
    215, 95, 215,
    215, 95, 255,
    255, 95, 0,
    255, 95, 95,
    255, 95, 135,
    255, 95, 175,
    255, 95, 215,
    255, 95, 255,
    0, 135, 0,
    0, 135, 95,
    0, 135, 135,
    0, 135, 175,
    0, 135, 215,
    0, 135, 255,
    95, 135, 0,
    95, 135, 95,
    95, 135, 135,
    95, 135, 175,
    95, 135, 215,
    95, 135, 255,
    135, 135, 0,
    135, 135, 95,
    135, 135, 135,
    135, 135, 175,
    135, 135, 215,
    135, 135, 255,
    175, 135, 0,
    175, 135, 95,
    175, 135, 135,
    175, 135, 175,
    175, 135, 215,
    175, 135, 255,
    215, 135, 0,
    215, 135, 95,
    215, 135, 135,
    215, 135, 175,
    215, 135, 215,
    215, 135, 255,
    255, 135, 0,
    255, 135, 95,
    255, 135, 135,
    255, 135, 175,
    255, 135, 215,
    255, 135, 255,
    0, 175, 0,
    0, 175, 95,
    0, 175, 135,
    0, 175, 175,
    0, 175, 215,
    0, 175, 255,
    95, 175, 0,
    95, 175, 95,
    95, 175, 135,
    95, 175, 175,
    95, 175, 215,
    95, 175, 255,
    135, 175, 0,
    135, 175, 95,
    135, 175, 135,
    135, 175, 175,
    135, 175, 215,
    135, 175, 255,
    175, 175, 0,
    175, 175, 95,
    175, 175, 135,
    175, 175, 175,
    175, 175, 215,
    175, 175, 255,
    215, 175, 0,
    215, 175, 95,
    215, 175, 135,
    215, 175, 175,
    215, 175, 215,
    215, 175, 255,
    255, 175, 0,
    255, 175, 95,
    255, 175, 135,
    255, 175, 175,
    255, 175, 215,
    255, 175, 255,
    0, 215, 0,
    0, 215, 95,
    0, 215, 135,
    0, 215, 175,
    0, 215, 215,
    0, 215, 255,
    95, 215, 0,
    95, 215, 95,
    95, 215, 135,
    95, 215, 175,
    95, 215, 215,
    95, 215, 255,
    135, 215, 0,
    135, 215, 95,
    135, 215, 135,
    135, 215, 175,
    135, 215, 215,
    135, 215, 255,
    175, 215, 0,
    175, 215, 95,
    175, 215, 135,
    175, 215, 175,
    175, 215, 215,
    175, 215, 255,
    215, 215, 0,
    215, 215, 95,
    215, 215, 135,
    215, 215, 175,
    215, 215, 215,
    215, 215, 255,
    255, 215, 0,
    255, 215, 95,
    255, 215, 135,
    255, 215, 175,
    255, 215, 215,
    255, 215, 255,
    0, 255, 0,
    0, 255, 95,
    0, 255, 135,
    0, 255, 175,
    0, 255, 215,
    0, 255, 255,
    95, 255, 0,
    95, 255, 95,
    95, 255, 135,
    95, 255, 175,
    95, 255, 215,
    95, 255, 255,
    135, 255, 0,
    135, 255, 95,
    135, 255, 135,
    135, 255, 175,
    135, 255, 215,
    135, 255, 255,
    175, 255, 0,
    175, 255, 95,
    175, 255, 135,
    175, 255, 175,
    175, 255, 215,
    175, 255, 255,
    215, 255, 0,
    215, 255, 95,
    215, 255, 135,
    215, 255, 175,
    215, 255, 215,
    215, 255, 255,
    255, 255, 0,
    255, 255, 95,
    255, 255, 135,
    255, 255, 175,
    255, 255, 215,
    255, 255, 255,
    8, 8, 8,
    18, 18, 18,
    28, 28, 28,
    38, 38, 38,
    48, 48, 48,
    58, 58, 58,
    68, 68, 68,
    78, 78, 78,
    88, 88, 88,
    96, 96, 96,
    102, 102, 102,
    118, 118, 118,
    128, 128, 128,
    138, 138, 138,
    148, 148, 148,
    158, 158, 158,
    168, 168, 168,
    178, 178, 178,
    188, 188, 188,
    198, 198, 198,
    208, 208, 208,
    218, 218, 218,
    228, 228, 228,
    238, 238, 238,
};



//////////////////////
// UTILITY FUNCTION //
//////////////////////



/*
  What follows here are utility functions for working with the buffer of
  `termbuf_char`'s. In memory this is just one contigous block of bytes, but
  logically it's a rectangle of height `nrows` and width `ncols`. Many of the
  ANSI escape sequences pertain to manipulating the contents of this rectangle,
  and so we find a need for these utility functions.
 */


/*
  This is just a tuple. It is used to represent a position, a row-collumn (where
  the first row and column have numerical value 1) pair, or a height-width pair.
 */
struct pair_s {
    int y;
    int x;
};

struct pair_s pair(int y, int x) {
    return (struct pair_s) { .y = y, .x = x, };
}

/*
  Given a row-col pair and a height-width pair, check that the rectangle they
  define is contained within the termbuf_char rectange.
 */
void assert_in_bounds(struct termbuf *tb, struct pair_s xy, struct pair_s wh) {
    assert(1 <= xy.x && xy.x <= tb->ncols);
    assert(1 <= xy.y && xy.y <= tb->nrows);
    assert(0 <= wh.x && xy.x - 1 + wh.x <= tb->ncols);
    assert(0 <= wh.y && xy.y - 1 + wh.y <= tb->nrows);
}

/*
  Goven a row-col pair, calculate what inded into memory the one-dimenional
  termbuf_char buffer it corresponds to.
 */
int pair_to_offset(struct termbuf *tb,
                   struct pair_s p) {
    return (p.y - 1) * tb->ncols + p.x - 1;
}

/*
  Given a row-col pair `dest` and a height-width pair `count`, clear out all the
  cells in that rectangle by setting the termbuf_char's `flag` variable to
  `FLAG_LENGTH_0`.
 */
void termbuf_memzero(struct termbuf *tb,
                    struct pair_s dest,
                    struct pair_s count) {
    assert_in_bounds(tb, dest, count);

    for (int row = dest.y; row < dest.y + count.y; row ++) {
        for (int col = dest.x ; col < dest.x + count.x; col++) {

            // This makes the terminal display the memzero'ed area in red, good
            // for debugging.
            // tb->buf[pair_to_offset(tb, pair(row, col))] =
            //     (struct termbuf_char) {
            //     .flags = FLAG_LENGTH_1,
            //     .utf8_char = { ' ', 0, 0, 0 },
            //     .bg_color_r = 255,
            // };

            tb->buf[(row - 1) * tb->ncols + col - 1].flags = FLAG_LENGTH_0;
        }
    }
}

/*
  A function analogous to `memmove`, but operating on the termbuf_char
  rectangle. Given a row-col pair `dest`, a row-col pair `src`, and a
  height-width pair `count`, copy the contents of the rectangle defined by
  `src` and `count` into the rectangle defined by `dest` and `count`.
 */
void termbuf_memmove(struct termbuf *tb,
                     struct pair_s dest,
                     struct pair_s src,
                     struct pair_s count) {
    assert_in_bounds(tb, src, count);
    assert_in_bounds(tb, dest, count);

    // TODO: We don't want to dynamically allocate memory if we can help it, so
    //       when we've implemented a scrollback (ring) buffer we can use part
    //       of it as a temporary memory block maybe?
    struct termbuf_char *temp = malloc(count.x * count.y
                                       * sizeof(struct termbuf_char));

    // Write `src` into `temp`.
    struct termbuf_char *p = temp;
    for (int row = src.y; row < src.y + count.y; row ++) {
        for (int col = src.x ; col < src.x + count.x; col++) {
            *p = tb->buf[pair_to_offset(tb, pair(row, col))];
            p++;
        }
    }

    // Write `temp` into `dest`
    p = temp;
    for (int row = dest.y; row < dest.y + count.y; row ++) {
        for (int col = dest.x ; col < dest.x + count.x; col++) {
            tb->buf[pair_to_offset(tb, pair(row, col))] = *p;
            p++;
        }
    }

    free(temp);
}



//////////////////
// REST OF CODE //
//////////////////



void termbuf_initialize(int nrows,
                        int ncols,
                        int pty_fd,
                        struct termbuf *tb_ret) {
    assert(nrows > 0);
    assert(ncols > 0);

    tb_ret->nrows = nrows;
    tb_ret->ncols = ncols;
    tb_ret->row = 1;
    tb_ret->col = 1;
    tb_ret->flags = FLAG_LENGTH_0 | FLAG_APPLICATION_KEYPAD;
    tb_ret->fg_color_r = 255;
    tb_ret->fg_color_g = 255;
    tb_ret->fg_color_b = 255;
    tb_ret->bg_color_r = 10;
    tb_ret->bg_color_g = 10;
    tb_ret->bg_color_b = 10;
    tb_ret->saved_row = -1;
    tb_ret->saved_col = -1;

    tb_ret->pty_fd = pty_fd;

    tb_ret->p_state = P_STATE_GROUND;

    tb_ret->buf = calloc(nrows * ncols, sizeof(struct termbuf_char));
    if (tb_ret->buf == NULL) {
        assert(false);
    }
}

void termbuf_insert(struct termbuf *tb, const uint8_t *utf8_char, int len) {
    assert(len > 0);
    assert(len <= 4);

    if (tb->col > tb->ncols) {
        // Check if we should wrap text or not?
        if ((tb->flags & FLAG_AUTOWRAP_MODE) == 0) {
            // If no wrapping we let this all be a no-op.
            // If this the way it should be? I don't know but I think it's
            // consistent with how st does it.
            return;
        } else { // AAAAH look at all this spagethi ;-;
            tb->col = 1;
            tb->row ++;
            if (tb->row > tb->nrows) {
                tb->row = tb->nrows;
                termbuf_shift(tb);
            }
        }
    }

    size_t index = (tb->row - 1) * tb->ncols + (tb -> col - 1);
    memcpy(tb->buf[index].utf8_char, utf8_char, 4);
    tb->flags = (tb->flags & ~FLAG_LENGTH_MASK) | len;

    if ((tb->flags & FLAG_INVERT_COLORS) == 0) {
        tb->buf[index].flags = tb->flags;
        tb->buf[index].fg_color_r = tb->fg_color_r;
        tb->buf[index].fg_color_g = tb->fg_color_g;
        tb->buf[index].fg_color_b = tb->fg_color_b;
        tb->buf[index].bg_color_r = tb->bg_color_r;
        tb->buf[index].bg_color_g = tb->bg_color_g;
        tb->buf[index].bg_color_b = tb->bg_color_b;
    } else { // When the FLAG_INVERT_COLORS is set we set the fg to the bg and
             // vice-versa.
        tb->buf[index].flags = tb->flags;
        tb->buf[index].fg_color_r = tb->bg_color_r;
        tb->buf[index].fg_color_g = tb->bg_color_g;
        tb->buf[index].fg_color_b = tb->bg_color_b;
        tb->buf[index].bg_color_r = tb->fg_color_r;
        tb->buf[index].bg_color_g = tb->fg_color_g;
        tb->buf[index].bg_color_b = tb->fg_color_b;
    }

    // NB. Here we might end up setting the cursor just outside of the view,
    //     hence the check at the begining of this function.
    tb->col ++;
}

void termbuf_shift(struct termbuf *tb) {
    // TODO: push into scrollback buffer
    // TODO: Implement a ring buffer to get rid of memcpy's
    size_t bytes_per_row = tb->ncols * sizeof(struct termbuf_char);
    memmove(tb->buf,
            tb->buf + tb->ncols,
            (tb->nrows - 1) * bytes_per_row);
    memset(tb->buf + (tb->nrows - 1) * tb->ncols, 0, bytes_per_row);
}

void termbuf_resize(struct termbuf *tb, int nnrows, int nncols) {
    assert(nnrows > 0);
    assert(nncols > 0);

    struct termbuf_char *new_buf = calloc(nnrows * nncols,
                                          sizeof(struct termbuf_char));

    int rows = nnrows < tb->nrows ? nnrows : tb->nrows;
    int cols = nncols < tb->ncols ? nncols : tb->ncols;
    for (int row = 1; row <= rows; row++) {
        for (int col = 1; col <= cols; col++) {
            new_buf[col - 1 + (row - 1) * nncols] =
                tb->buf[col - 1 + (row - 1) * tb->ncols];
        }
    }

    // TODO: What about saved cursor?
    tb->saved_row = 1;
    tb->saved_col = 1;

    // TODO: fix.
    tb->row = 1;
    tb->col = 1;

    free(tb->buf);
    tb->buf = new_buf;
    tb->nrows = nnrows;
    tb->ncols = nncols;
}



/*
 * Parsing logic
 */



struct parser_table_entry {
    enum parser_state new_state;
    void (*action)(struct termbuf *tb, char ch);
};

struct parser_table_entry parser_table[256 * NSTATES];

void termbuf_parse(struct termbuf *tb, uint8_t *data, size_t len) {
    while (len > 0) {
        diagnostics_type(DIAGNOSTICS_TERM_PARSE_INPUT);
        diagnostics_write_string_escape_non_printable((char *) data, 1);

        size_t index = tb->p_state * 256 + *data;
        struct parser_table_entry entry = parser_table[index];

        entry.action(tb, *data);

        // TODO: This constant is redefined in different places, have on shared
        //       map instead.
        if (tb->p_state != entry.new_state) {
            static const char *STATE_STRING_MAP[] = {
                [P_STATE_GROUND]           = "GROUND",
                [P_STATE_CHOMP1]           = "CHOMP1",
                [P_STATE_CHOMP2]           = "CHOMP2",
                [P_STATE_CHOMP3]           = "CHOMP3",
                [P_STATE_ESC]              = "ESC",
                [P_STATE_NF]               = "NF",
                [P_STATE_CSI]              = "CSI",
                [P_STATE_CSI_PARAMS]       = "CSI_P",
                [P_STATE_CSI_INTERMEDIATE] = "SCI_I",
                [P_STATE_OSC]              = "OSC",
                [P_STATE_OSC_ESC]          = "OSC_ESC",
            };

            diagnostics_type(DIAGNOSTICS_TERM_PARSE_STATE);
            diagnostics_write_string("\x1B[35m|", -1);
            diagnostics_write_string(STATE_STRING_MAP[entry.new_state], -1);
            diagnostics_write_string("|\x1B[m", -1);
        }

        tb->p_state = entry.new_state;

        data ++;
        len --;
    }
    diagnostics_flush();
}

void action_noop(struct termbuf *tb, char ch) {}

void action_fail(struct termbuf *tb, char ch) {
    // TODO: A problem here is that we might be displaying parse data from
    //       a previous "round", leading us astray when debugging.

    static const char *STATE_STRING_MAP[] = {
        [P_STATE_GROUND]           = "GROUND",
        [P_STATE_CHOMP1]           = "CHOMP1",
        [P_STATE_CHOMP2]           = "CHOMP2",
        [P_STATE_CHOMP3]           = "CHOMP3",
        [P_STATE_ESC]              = "ESC",
        [P_STATE_NF]               = "NF",
        [P_STATE_CSI]              = "CSI",
        [P_STATE_CSI_PARAMS]       = "CSI_P",
        [P_STATE_CSI_INTERMEDIATE] = "SCI_I",
        [P_STATE_OSC]              = "OSC",
        [P_STATE_OSC_ESC]          = "OSC_ESC",
    };

    fprintf(stderr,
            "\n"
            "Parser failed\n"
            "    state        : %d %s\n"
            "    ch           : %d / '%c'\n",
            tb->p_state,
            STATE_STRING_MAP[tb->p_state],
            ch, ch);

    if (tb->p_state == P_STATE_CSI_PARAMS) {
        fprintf(stderr,
                "    initial_char : %d / '%c'\n"
                "    current_param: %d\n"
                "    param1       : %d\n"
                "    param2       : %d\n"
                "    param3       : %d\n"
                "    param4       : %d\n"
                "    param5       : %d\n",
                tb->p_data.ansi_csi_chomping.initial_char,
                tb->p_data.ansi_csi_chomping.initial_char,
                tb->p_data.ansi_csi_chomping.current_param,
                tb->p_data.ansi_csi_chomping.params[0],
                tb->p_data.ansi_csi_chomping.params[1],
                tb->p_data.ansi_csi_chomping.params[2],
                tb->p_data.ansi_csi_chomping.params[3],
                tb->p_data.ansi_csi_chomping.params[4]);
    }

    if (tb->p_state == P_STATE_OSC || tb->p_state == P_STATE_OSC_ESC) {
        struct ansi_osc_chomping *data = &tb->p_data.ansi_osc_chomping;
        data->data[data->len] = '\0';
        fprintf(stderr,
                "    len : %d\n"
                "    contents \"%s\"\n",
                data->len,
                data->data);
    }

    assert(false);
}

void action_print(struct termbuf *tb, char ch) {
    termbuf_insert(tb, (uint8_t *) &ch, 1);
}

// Invoked by the parser to handle most C0 control sequences.
// Notably it is not meant to handle ESC.
// Also does not handle 32 space or 127 delete.
void action_c0(struct termbuf *tb, char ch) {
    assert(0 <= ch && ch <= 31 && ch != '\x1B');
    switch (ch) {
    case '\0':  // NULL.
        assert(false);
    case 1:  // Start of heading.
        assert(false);
    case 2:  // Start of text.
        assert(false);
    case 3:  // End of text.
        assert(false);
    case 4:  // End of transmission.
        assert(false);
    case 5:  // Enquiry.
        assert(false);
    case 6:  // Acknowlegde.
        assert(false);
    case '\a':  // Bell.
        // We don't want any type of bell thing to happen
        return;
    case '\b':  // Backspace.
        assert(tb->col >= 1);
        tb->col --;
        return;
    case '\t':  // Tab stop.
        {
            // and-ing with this bitmasks truncates the number to be multiple of
            // (+1), (assuming the current column is smaller than 2^16 - 1).
            const int MULTIPLE_OF_EIGHT_BITMASK = 65520;  // 0b1111111111110000

            tb->col = ((tb->col + 8) & MULTIPLE_OF_EIGHT_BITMASK) + 1;
            if (tb->col > tb->ncols) {
                tb->col = 1;
                tb->row ++;
                if (tb->row > tb->nrows) {
                    //assert(false);
                    tb->row = 1;
                }
            }
            return;
        }
    case '\n':  // Line feed.
        tb->row ++;
        if (tb->row > tb->nrows) {
            tb->row = tb->nrows;
            termbuf_shift(tb);
        }
        return;
    case '\v':  // Line tabulation.
        assert(false);
    case '\f':  // Form feed.
        assert(false);
    case '\r':  // Carrige return.
        tb->col = 1;
        return;
    case 14:  // Shift out.
        assert(false);
    case 15:  // Shift in.
        assert(false);
    case 16:  // Data line escape.
        assert(false);
    case 17:  // Device control 1.
        assert(false);
    case 18:  // Device control 2.
        assert(false);
    case 19:  // Device control 3.
        assert(false);
    case 20:  // Device control 4.
        assert(false);
    case 21:  // Negative acknowlegde.
        assert(false);
    case 22:  // Synchronous Idle.
        assert(false);
    case 23:  // End of transmission block.
        assert(false);
    case 24:  // Cancel.
        assert(false);
    case 25:  // End of medium.
        assert(false);
    case 26:  // Substitute.
        assert(false);
    case 27:  // Escape.
        assert(false);
    case 28:  // File separator.
        assert(false);
    case 29:  // Group separator.
        assert(false);
    case 30:  // Record separator.
        assert(false);
    case 31:  // Unit separator.
        assert(false);
    }
}

// Invoked by the parser to handle Fe control sequences.
// So if we got a sequence "ESC<n> where <n> is a byte in the range 48--63
// then this function is called.
// See: https://en.wikipedia.org/wiki/ANSI_escape_code#Fp_Escape_sequences
void action_fp(struct termbuf *tb, char ch) {
    assert(48 <= ch && ch <= 63);

    // Save cursor
    if (ch == '7') {
        tb->saved_row = tb->row;
        tb->saved_col = tb->col;
        return;
    }

    // Restore cursor
    if (ch == '8') {
        assert(tb->saved_row != -1 && tb->saved_row != -1);
        tb->row = tb->saved_row;
        tb->col = tb->saved_col;
        return;
    }

    // Application keypad (DECKPAM)
    // https://vt100.net/docs/vt510-rm/DECKPAM.html
    if (ch == '=') {
        tb->flags |= FLAG_APPLICATION_KEYPAD;
        return;
    }

    // Normal keypad (DECKPNM), VT100
    // https://vt100.net/docs/vt510-rm/DECKPNM.html
    if (ch == '>') {
        tb->flags &= ~FLAG_APPLICATION_KEYPAD;
        return;
    }

    printf("\naction_fp, unhandeled parameter %d / '%c'\n", ch, ch);
    assert(false);
}

void action_utf8_chomp_start(struct termbuf *tb, char ch) {
    tb->p_data.utf8_chomping = (struct utf8_chomping) {
        .len = 1,
        .utf8_char = { ch, 0, 0, 0 },
    };
}

void action_utf8_chomp_continue(struct termbuf *tb, char ch) {
    struct utf8_chomping *data = &tb->p_data.utf8_chomping;

    assert(data->len <= 2);

    data->utf8_char[data->len] = ch;
    data->len ++;
}

void action_utf8_chomp_end(struct termbuf *tb, char ch) {
    struct utf8_chomping *data = &tb->p_data.utf8_chomping;

    assert(data->len <= 3);

    data->utf8_char[data->len] = ch;
    data->len ++;

    termbuf_insert(tb, (uint8_t *) &data->utf8_char, data->len);
}

void action_nf_chomp_start(struct termbuf *tb, char ch) {
    assert(32 <= ch <= 47);

    tb->p_data.ansi_nf_chomping = (struct ansi_nf_chomping) {
        .initial_char = ch,
        .len = 1,
    };
}


void action_nf_chomp_continue(struct termbuf *tb, char ch) {
    assert(false);
}

void action_nf_chomp_end(struct termbuf *tb, char final_byte) {
    // https://invisible-island.net/xterm/ctlseqs/ctlseqs.html has some info on
    // some of these types of escape sequences.

    // Check that final_byte is in the range it's supposed to be in.
    assert(48 <= final_byte <= 126);

    struct ansi_nf_chomping *data = &tb->p_data.ansi_nf_chomping;

    // Right now we only support nF sequencees with exactly two bytes:
    // - 1) The initial byte
    // - 2) The final byte
    if (data->len > 1) {
        assert(false);
    }

    // We can subcategorize our nF sequences as follows:
    // 48 `<= final_byte <= 63` ⇒ we have a "private-use escape sequence"
    // There's also something where we look at the 2 least significant bits of
    // the initial char 0.0
    // See: https://en.wikipedia.org/wiki/ISO/IEC_2022#Character_set_designations

    // Designate a G0 resp. G1 character set. Final byte denotes the character
    // set to select. This are mostly obsolete now that we have unicode, however
    // we still have to support some of this :-(
    // There is some information about character sets here:
    //     https://www.man7.org/linux/man-pages/man7/charsets.7.html
    if (data->initial_char == '(' || data->initial_char == ')') {
        // This link enumerates some of the character sets that the final byte
        // denotes
        //  https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Controls-beginning-with-ESC:ESC-lparen-C.F20

        // From man 7 charsets:
        // > There are 4 graphic character sets, called G0, G1, G2, and G3, and
        // > one of them is the current character set for codes with high bit
        // > zero (initially G0), and one of them is the current character set
        // > for codes with high bit one (initially G1).  Each graphic
        // > character set has 94 or 96 characters, and is essentially a 7-bit
        // > character set.  It uses codes either 040–0177 (041–0176) or
        // > 0240–0377 (0241–0376).  G0 always has size 94 and uses codes
        // > 041–0176.

        // From what I gather these character sets dictate how a byte should be
        // mapped to a grapheme lile 'å'. For instance, a Swede like me would
        // have G0 be the latin alphabet + some special characters, what we know
        // and love from ASCII basically. Additionally I would have G1 be the
        // Swedish character set so that I can use 'å' 'ä' 'ö'.

        // Set G0/G1 to "United States (USASCII)", VT100.
        if (final_byte == 'B') {
            // TODO: do something
            return;
        }

        // Set G0/G1 to the "Special Character and Line Drawing Set", VT100.
        if (final_byte == '0') {
            // TODO: do something
            return;
        }
    }

    printf("\nNF %c %c\n", data->initial_char, final_byte);
    assert(false);
}


void action_csi_chomp_start(struct termbuf *tb, char ch) {
    assert(ch == '[');

    tb->p_data.ansi_csi_chomping = (struct ansi_csi_chomping) {
        .initial_char  = '\0',
        .current_param = 0,
        .intermediate = 255,
    };

    for (int i = 0; i < CSI_CHOMPING_MAX_PARAMS; i++) {
        tb->p_data.ansi_csi_chomping.params[i] = -1;
    }
}

void action_csi_chomp_initial_char(struct termbuf *tb, char ch) {
    assert(ch == '?');
    tb->p_data.ansi_csi_chomping.initial_char = '?';
}

void action_csi_chomp_param(struct termbuf *tb, char ch) {
    assert('0' <= ch && ch <= '9');

    struct ansi_csi_chomping *data = &tb->p_data.ansi_csi_chomping;

    if (data->params[data->current_param] == (uint16_t) -1) {
        data->params[data->current_param] = 0;
    }

    data->params[data->current_param] *= 10;
    data->params[data->current_param] += ch - '0';
}

void action_csi_chomp_next_param(struct termbuf *tb, char ch) {
    assert(ch == ';');

    struct ansi_csi_chomping *data = &tb->p_data.ansi_csi_chomping;

    if(data->current_param >= CSI_CHOMPING_MAX_PARAMS - 1) {
        uint8_t ic  = data->initial_char;
        uint8_t len = data->current_param + 1;
        if (data->params[data->current_param] == (uint16_t) -1) {
            len --;
        }
        uint16_t p1 = data->params[0];
        uint16_t p2 = data->params[1];
        uint16_t p3 = data->params[2];
        uint16_t p4 = data->params[3];
        uint16_t p5 = data->params[4];
        printf("\n"
               "Got a CSI sequence with more than %d parameters, which is more "
               "than this terminal supports:\n"
               "    ch            : '%c' (decimal %d).\n"
               "    initial_char  : '%c' (decimal %d).\n"
               "    current_param : %d.\n"
               "    len           : %d.\n"
               "    param1        : %d.\n"
               "    param2        : %d.\n"
               "    param3        : %d.\n"
               "    param4        : %d.\n"
               "    param5        : %d.\n",
               CSI_CHOMPING_MAX_PARAMS,
               ch, ch,
               ic, ic,
               data->current_param,
               len,
               p1, p2, p3, p4, p5);
        assert(false);
    }

    data->current_param ++;
}

void action_csi_intermediate(struct termbuf *tb, char ch) {
    assert(' ' <= ch && ch <= '/');

    struct ansi_csi_chomping *data = &tb->p_data.ansi_csi_chomping;

    if (data->intermediate != 255) {  // We already have an intermediate byte.
        assert(false);
    }

    data->intermediate = ch;
}

void action_csi_chomp_final_byte(struct termbuf *tb, char ch) {
    assert('@' <= ch && ch <= '~');

    struct ansi_csi_chomping *data = &tb->p_data.ansi_csi_chomping;
    uint8_t ic  = data->initial_char;
    uint8_t intermediate = data->intermediate;

    uint8_t len = data->current_param + 1;
    if (data->params[data->current_param] == (uint16_t) -1) {
        len --;
    }

    uint16_t p1 = data->params[0];
    uint16_t p2 = data->params[1];
    uint16_t p3 = data->params[2];
    uint16_t p4 = data->params[3];
    uint16_t p5 = data->params[4];

    // ESC[?<p>h ESC[?<p>l
    // See `void csi_dec_private_mode_set` for documentation on these types of
    // sequences.
    if (ic == '?' && (ch == 'h' || ch == 'l')) {
        assert(len == 1);  // I think all of these sequences always have exactly
                           // ine parameter.
        csi_dec_private_mode_set(tb, ch);
        return;
    }

    // Intermediate is !
    // https://vt100.net/emu/ctrlseq_dec.html
    if (intermediate == '!') {
        // DECSTR Soft Terminal Reset
        // https://vt100.net/docs/vt510-rm/DECSTR.html
        if (ch == 'p') {
            tb->flags &= ~FLAG_HIDE_CURSOR;
            // TODO: IRM.
            // TODO: DECOM.
            tb->flags &= ~FLAG_AUTOWRAP_MODE;
            // TODO: DECNRCM.
            // TODO: KAM.
            // TODO: DECNKM.
            tb->flags &= ~FLAG_APPLICATION_CURSOR;
            // TODO: DECSTBM.
            // TODO: G0, G1, G2, G3, GL, GR
            tb->flags &= ~ (FLAG_BOLD | FLAG_FAINT | FLAG_ITALIC
                            | FLAG_UNDERLINE | FLAG_STRIKEOUT
                            | FLAG_INVERT_COLORS);
            tb->fg_color_r = four_bit_colors[15 * 3];
            tb->fg_color_g = four_bit_colors[15 * 3 + 1];
            tb->fg_color_b = four_bit_colors[15 * 3 + 2];
            tb->bg_color_r = four_bit_colors[0];
            tb->bg_color_g = four_bit_colors[1];
            tb->bg_color_b = four_bit_colors[2];
            // TODO: DECSCA
            // TODO: DECSC
            // TODO: DECAUPSS
            // TODO: DECSASD
            // TODO: DECKPM
            // TODO: DECRLM
            // TODO: DECPCTERM

            return;
        }
        // decVKPPI Print Partial Image
        // https://vt100.net/docs/vt510-rm/decVKPPI.html
        if (ch == 'q') { /* TODO */ assert(false); }
        // DECNVR Nonvolatile RAM Feature Settings
        // https://vt100.net/docs/vt510-rm/DECNVR.html
        if (ch == 'r') { /* TODO */ assert(false); }
        // DECFIL Right Justification
        // https://vt100.net/docs/vt510-rm/DECFIL.html
        if (ch == 's') { /* TODO */ assert(false); }
        // ???
        if (ch == 't') { assert(false); }
        // DECFNVR Loading Factory NVR Settings
        // https://vt100.net/docs/vt510-rm/DECFNVR.html
        if (ch == 'u') { /* TODO */ assert(false); }
        // DECASFC Automatic Sheet Feeder Control
        //     s://vt100.net/docs/vt510-rm/Automatic.html
        if (ch == 'v') { /* TODO */ assert(false); }
        // DECUND Programmable Underline Character
        // https://vt100.net/docs/vt510-rm/DECUND.html
        if (ch == 'w') { /* TODO */ assert(false); }
        // DECPTS Printwheel Table Select
        // https://vt100.net/docs/vt510-rm/DECPTS.html
        if (ch == 'x') { /* TODO */ assert(false); }
        // DECSS Set Space Size
        // https://vt100.net/docs/vt510-rm/DECSS.html
        if (ch == 'y') { /* TODO */ assert(false); }
        // ???
        if (ch == 'z') { assert(false); }
        // ???
        if (ch == '{') { assert(false); }
        // DECVEC Draw Vector
        // https://vt100.net/docs/vt510-rm/DECVEC.html
        if (ch == '|') { /* TODO */ assert(false); }
        // DECFIN Document Finishing
        // https://vt100.net/docs/vt510-rm/DECFIN.html
        if (ch == '}') { /* TODO */ assert(false); }
        // ???
        if (ch == '~') { assert(false); }

        unknown_csi(tb, ch);
        assert(false);
    }

    // Itermediate is "
    // https://vt100.net/emu/ctrlseq_dec.html
    if (intermediate == '"') {

        // Select conformance level (DECSCL)
        // https://vt100.net/docs/vt510-rm/DECSCL.html
        // https://terminalguide.namepad.de/seq/csi_sp_t_quote/
        // https://vt100.net/docs/vt510-rm/chapter4.html
        if (ch == 'p') {
            // This makes no sense but I swear it's what the manuals say.

            bool vt100;
            bool eight_bit;

            if (len < 1 || p1 == 60 || p1 == 61) {
                vt100 = true;
            } else if (p1 == 62 || p1 == 63 || p1 == 64) {
                vt100 = false;
            } else {
                // Invalid paramter.
                assert(false);
            }

            if (len < 2 || p2 == 0 || p2 == 2) {
                eight_bit = true;
            } else if (p2 == 1) {
                eight_bit = false;
            } else {
                // Invalid parameter.
                assert(false);
            }

            // TODO: Do something with this.
            return;
        }

        // Select character attributes (DECSCA)
        // https://vt100.net/docs/vt510-rm/DECSCA.html
        if (ch == 'q') {
            // TODO
            assert(false);
        }

        // ???
        if (ch == 'r') { assert(false); }

        // Page width alignment (DECPWA)
        // https://vt100.net/docs/vt510-rm/DECPWA.html
        if (ch == 's') {
            // TODO
            assert(false);
        }

        // Select refresh rate (DECSRFR)
        // https://vt100.net/docs/vt510-rm/DECSRFR.html
        if (ch == 't') {
            // TODO
            assert(false);
        }

        // Set transmit rate limit (DECSTRL)
        // https://vt100.net/docs/vt510-rm/DECSTRL.html
        if (ch == 'u') {
            // TODO
            assert(false);
        }

        // Request device extent (DECRQDE)
        // https://vt100.net/docs/vt510-rm/DECRQDE.html
        if (ch == 'v') {
            // TODO
            assert(false);
        }

        // Report device extent (DECRPDE)
        // https://vt100.net/docs/vt510-rm/DECRPDE.html
        if (ch == 'w') {
            // TODO
            assert(false);
        }

        // Font configuration request (DECFCR)
        // https://vt100.net/docs/vt510-rm/DECFCR.html
        if (ch == 'x') {
            // TODO
            assert(false);
        }

        // ???
        if (ch == 'y') { assert(false); }

        // Select density (DECDEN)
        // https://vt100.net/docs/vt510-rm/DECDEN.html
        if (ch == 'z') {
            // TODO
            assert(false);
        }

        // Request font status (DECRFS)
        // https://vt100.net/docs/vt510-rm/DECRFS.html
        if (ch == '{') {
            // TODO
            assert(false);
        }

        // ???
        if (ch == '|') { assert(false); }
        // ???
        if (ch == '}') { assert(false); }
        // ???
        if (ch == '~') { assert(false); }

        unknown_csi(tb, ch);
        assert(false);
    }

    // Select Set-Up Language (DECSSL)
    // https://vt100.net/docs/vt510-rm/DECSSL.html
    if (ch == 'p') { /* TODO */ assert(false); }
    // Load LEDs (DECLL)
    // https://vt100.net/docs/vt510-rm/DECLL.html
    if (ch == 'q') { /* TODO */ assert(false); }
    // Set Top and Bottom Margins (DECSTBM)
    // https://vt100.net/docs/vt510-rm/DECSTBM.html
    if (ch == 'r') { /* TODO */ assert(false); }
    // Set Left and Right Margins (DECSLRM)
    // https://vt100.net/docs/vt510-rm/DECSLRM.html
    if (ch == 's') { /* TODO */ assert(false); }
    // Set Lines per Physical Page (DECSLPP)
    // https://vt100.net/docs/vt510-rm/DECSLPP.html
    if (ch == 't' && len <= 1) { /* TODO */ assert(false); }
    // Set Horizontal Tabulation Stops (DECSHTS)
    // https://vt100.net/docs/vt510-rm/DECSHTS.html
    if (ch == 'u') { /* TODO */ assert(false); }
    // Set Vertical Tabulation Stops (DECSVTS)
    // https://vt100.net/docs/vt510-rm/DECSVTS.html
    if (ch == 'v') { /* TODO */ assert(false); }
    // Set Horizontal Pitch (DECSHORP)
    // https://vt100.net/docs/vt510-rm/DECSHORP.html
    if (ch == 'w') { /* TODO */ assert(false); }
    // Request Terminal Parameters (DECREQTPARM)
    // https://vt100.net/docs/vt510-rm/DECREQTPARM.html
    if (ch == 'x') { /* TODO */ assert(false); }
    // Invoke Confidence Test (DECTST)
    // https://vt100.net/docs/vt510-rm/DECTST.html
    if (ch == 'y') { /* TODO */ assert(false); }
    // Set Vertical Pitch (DECVERP)
    // https://vt100.net/docs/vt510-rm/DECVERP.html
    if (ch == 'z') { /* TODO */ assert(false); }
    // ???
    if (ch == '{') { /* TODO */ assert(false); }
    // Select Transmit Termination Character (DECTTC)
    // https://vt100.net/docs/vt510-rm/DECTTC.html
    if (ch == '|') { /* TODO */ assert(false); }
    // Set Protected Field Attributes (DECPRO)
    // https://vt100.net/docs/vt510-rm/DECPRO.html
    if (ch == '}') { /* TODO */ assert(false); }
    // Function Key (DECFNK)
    // https://vt100.net/docs/vt510-rm/DECFNK.htm
    if (ch == '~') { /* TODO */ assert(false); }


    // Window manipulation (XTWINOPS)
    // https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character-lparen-s-rparen:CSI-Ps;Ps;Ps-t.1EB0
    // How to tell when to use this vs DECSLPP?
    if (ch == 't' && 1 <= len && len <= 3) {
        if (p1 == 8) {
            uint16_t nnrows = len < 2 ? tb->nrows : p2;
            uint16_t nncols = len < 3 ? tb->ncols : p3;

            termbuf_resize(tb, nnrows, nncols);

            // TODO: Resize the actual window too.

            return;
        }

        unknown_csi(tb, ch);
        assert(false);
    }

    // ESC n A, CUU, move cursor up
    if (ch == 'A' && (len == 0 || len == 1)) {
        int n = len == 0 ? 1 : p1;
        tb->row = tb->row - n < 1 ? 1 : tb->row - n;
        return;
    }

    // ESC n B, CUD, move cursor down
    if (ch == 'B' && (len == 0 || len == 1)) {
        int n = len == 0 ? 1 : p1;
        tb->row = tb->row + n > tb->nrows ? tb->nrows : tb->row + n;
        return;
    }

    // ESC n C, CUF, move cursor forward
    if (ch == 'C' && (len == 0 || len == 1)) {
        int n = len == 0 ? 1 : p1;
        tb->col = tb->col + n > tb->ncols ? tb->ncols : tb->col + n;
        return;
    }

    // ESC n D, CUB, move cursor backwards
    if (ch == 'D' && (len == 0 || len == 1)) {
        int n = len == 0 ? 1 : p1;
        tb->col = tb->col - n < 1 ? 1 : tb->col - n;
        return;
    }

    // ESC[<n>G Cursor Character Absolute (CHA)
    // See: https://vt100.net/docs/vt510-rm/CHA.html
    if (ch == 'G') {
        assert(len <= 1);
        if (len == 0) { p1 = 1; }
        tb->col = p1;
        return;
    }

    // ESC n ; m H, CUP, set cursor position
    if (ch == 'H' && len <= 2) {
        p1 = p1 == -1 ? 1 : p1;
        p2 = p2 == -1 ? 1 : p2;
        tb->row = p1;
        tb->col = p2;
        return;
    }

    // ESC 0 J, ED, erase display from cursor to end of scree.
    if (ch == 'J' && (len == 0 || (len == 1 && p1 == -1))) {
        for (int i = tb->col; i <= tb->ncols; i++) {
            tb->buf[(tb->row - 1) * tb->ncols + i - 1].flags = FLAG_LENGTH_0;
        }
        return;
    }

    // ESC 1 J, ED, erase display from cursor to begining of screen.
    if (ch == 'J' && len == 1 && p1 == 1) {
        // TODO
        assert(false);
    }

    // ESC 2 J, ED, erase entire display.
    if (ch == 'J' && len == 1 && p1 == 2) {
        memset(tb->buf, 0, tb->ncols * tb->nrows * sizeof(struct termbuf_char));
        return;
    }

    // ESC 3 J, ED, erase entire display and clear the scrollback buffer.
    if (ch == 'J' && len == 1 && p1 == 3) {
        memset(tb->buf, 0, tb->ncols * tb->nrows * sizeof(struct termbuf_char));
        printf("TODO: clear scrollback buffer.\n");
        return;
    }

    // ESC 0 K, EL, erase line from cursor to end of line.
    if (ch == 'K' && (len == 0 || len == 1)) {
        p1 = p1 == -1 ? 1 : p1;

        memset(tb->buf + ((tb->row - 1) * tb->ncols) + tb->col - 1,
               0,
               (tb->ncols - tb->col + 1) * sizeof(struct termbuf_char));
        return;
    }

    // ESC 1 K, EL, erase line from cursor to begining of line.
    if (ch == 'K' && len == 1 && p1 == 1) {
        // TODO
        assert(false);
    }

    // ESC 2 K, EL, clear entire line.
    if (ch == 'K' && len == 1 && p1 == 2) {
        // TODO
        assert(false);
    }

    // CSI Ps M, DL, Delete line.
    // https://vt100.net/docs/vt510-rm/DL.html
    if (ch == 'M') {
        assert(len <= 1);
        if (len == 0) { p1 = 1; }

        // TODO: handle
        assert(tb->row + p1 < tb->nrows);

        struct pair_s dest  = pair(tb->row, 1);
        struct pair_s src   = pair(tb->row + p1, 1);
        struct pair_s count = pair(p1, tb->ncols);
        termbuf_memmove(tb, dest, src, count);
        termbuf_memzero(tb, src, count);
        return;
    }

    // ESC[<n>d Line Position Absolute (VPA)
    // See: https://vt100.net/docs/vt510-rm/VPA.html
    if (ch == 'd') {
        assert(len <= 1);
        if (len == 0) { p1 = 1; }
        tb->row = p1;
        return;
    }

    // ESC[<p₁>;...l Reset mode (RM).
    // See: https://vt100.net/docs/vt510-rm/RM.html
    if (ch == 'l') {
        // You use this sequence to reset either ANSI modes (?) or DEC modes (?)
        // This table:
        //    https://vt100.net/docs/vt510-rm/DECRQM.html#T5-8
        // contains DEC modes and their corresponding parameter.

        // We iterate through each parameter as it specifices a mode to reset.
        for (int i = 0; i < len; i++) {
            uint16_t p = data->params[i];

            switch(p) {
            // Reset scrolling (DECSCLM)
            // See: https://vt100.net/docs/vt510-rm/DECSCLM.html
            case 4:
                // As the situation is right now we haven't implemented DECSCLM
                // anyways so resetting DECSLCL is a no-op.
                continue;
            default:
                // some parameter is unhandelded.
                printf("\nReset mode (RM), unhandeled parameter: %d\n", p);
                assert(false);
            }
        }

        return;
    }

    // ESC[<t>;<b>r Set scrolling region (DECSTBM)
    // See: https://vt100.net/docs/vt510-rm/DECSTBM.html
    if (len <= 2 && ch == 'r') {

        // from vt100.net:
        // > The value of the top margin (Pt) must be less than the bottom
        // > margin (Pb).
        assert(p1 < p2);
        printf("TODO: Handle DECSTBM\n");
        return;
    }

    // ESC[6n "Device status report"
    // We transmit "ESC[n;mR" to the shell where n is row and m is column.
    if (len == 1 && p1 == 6 && ch == 'n') {
        diagnostics_type(DIAGNOSTICS_TERM_RESPONSE);
        diagnostics_write_string("\n\x1B[36mGot a ESC[6n (device status "
                                 "report) from the shell. Responding with \n"
                                 "\"ESC[", -1);
        diagnostics_write_int(tb->row);
        diagnostics_write_string(";", -1);
        diagnostics_write_int(tb->col);
        diagnostics_write_string("R\" to the shell.\x1B[0m\n", -1);

        dprintf(tb->pty_fd, "\x1B[%d;%dR", tb->row, tb->col);
        fsync(tb->pty_fd);
        return;
    }

    // We got a so called select graphics rendition (SGR)
    // https://en.wikipedia.org/wiki/ANSI_escape_code#Select_Graphic_Rendition_parameters
    if (ch == 'm') {
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
            tb->fg_color_r = four_bit_colors[15 * 3];
            tb->fg_color_g = four_bit_colors[15 * 3 + 1];
            tb->fg_color_b = four_bit_colors[15 * 3 + 2];

            // Set background color to "Black".
            tb->bg_color_r = four_bit_colors[0];
            tb->bg_color_g = four_bit_colors[1];
            tb->bg_color_b = four_bit_colors[2];
            return;
        }

        // We got something else, iterate through each parameter
        for (int i = 0; i < len; i++) {
            int param = data->params[i];
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
                    tb->fg_color_r = four_bit_colors[i * 3];
                    tb->fg_color_g = four_bit_colors[i * 3 + 1];
                    tb->fg_color_b = four_bit_colors[i * 3 + 2];
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
                uint8_t q = data->params[i + 1];

                // Set 8-bit foreground color.
                if (q == 5) {
                    // There should be at least one parameter following the '5'.
                    assert(i + 2 < len);
                    uint8_t q2 = data->params[i + 2];
                    q2 = q2 == -1 ? 0 : q2;
                    assert(0 <= q2 && q2 <= 255);
                    tb->fg_color_r = eight_bit_colors[q2 * 3];
                    tb->fg_color_g = eight_bit_colors[q2 * 3 + 1];
                    tb->fg_color_b = eight_bit_colors[q2 * 3 + 2];

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
                    uint8_t q2 = data->params[i + 2]; // red.
                    uint8_t q3 = data->params[i + 3]; // green.
                    uint8_t q4 = data->params[i + 4]; // blue.
                    assert(0 <= q2 && q2 <= 255);
                    assert(0 <= q3 && q3 <= 255);
                    assert(0 <= q4 && q4 <= 255);
                    tb->fg_color_r = q2;
                    tb->fg_color_g = q3;
                    tb->fg_color_b = q4;

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
                    tb->fg_color_r = four_bit_colors[i * 3];
                    tb->fg_color_g = four_bit_colors[i * 3 + 1];
                    tb->fg_color_b = four_bit_colors[i * 3 + 2];
                    continue;
                }
            case 40:  // Background color 1.
            case 41:  // Background color 1.
            case 42:  // Background color 1.
            case 43:  // Background color 1.
            case 44:  // Background color 1.
            case 45:  // Background color 1.
            case 46:  // Background color 1.
            case 47:  // Background color 1.
                {
                    int i = param - 40;
                    assert(0 <= i && i <= 8);
                    tb->bg_color_r = four_bit_colors[i * 3];
                    tb->bg_color_g = four_bit_colors[i * 3 + 1];
                    tb->bg_color_b = four_bit_colors[i * 3 + 2];
                    continue;
                }
            case 48:  // Set 8-bit foreground color or rgb color.
                // For more information, see how case 38 is handlede, this case
                // is the same but for background colors.

                // There should be at least one parameter following the '38'.
                assert(i + 1 < len);

                // We expect `q` to be either '5' or '2'..
                q = data->params[i + 1];

                // Set 8-bit background color.
                if (q == 5) {
                    // There should be at least one parameter following the '5'.
                    assert(i + 2 < len);
                    uint8_t q2 = data->params[i + 2];
                    q2 = q2 == -1 ? 0 : q2;
                    assert(0 <= q2 && q2 <= 255);
                    tb->bg_color_r = eight_bit_colors[q2 * 3];
                    tb->bg_color_g = eight_bit_colors[q2 * 3 + 1];
                    tb->bg_color_b = eight_bit_colors[q2 * 3 + 2];

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
                    uint8_t q2 = data->params[i + 2]; // red.
                    uint8_t q3 = data->params[i + 3]; // green.
                    uint8_t q4 = data->params[i + 4]; // blue.
                    assert(0 <= q2 && q2 <= 255);
                    assert(0 <= q3 && q3 <= 255);
                    assert(0 <= q4 && q4 <= 255);
                    tb->bg_color_r = q2;
                    tb->bg_color_g = q3;
                    tb->bg_color_b = q4;

                    // Continue parsing any potential remaining graphics
                    // parameters.
                    i += 4;
                    continue;

                }

                assert(false);
            case 49:  // Default background color.
                // See: case 39

                // Set background color to black.
                tb->bg_color_r = four_bit_colors[0];
                tb->bg_color_g = four_bit_colors[1];
                tb->bg_color_b = four_bit_colors[2];
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
                    tb->fg_color_r = four_bit_colors[(i + 8) * 3];
                    tb->fg_color_g = four_bit_colors[(i + 8) * 3 + 1];
                    tb->fg_color_b = four_bit_colors[(i + 8) * 3 + 2];
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
                    tb->bg_color_r = four_bit_colors[(i + 8) * 3];
                    tb->bg_color_g = four_bit_colors[(i + 8) * 3 + 1];
                    tb->bg_color_b = four_bit_colors[(i + 8) * 3 + 2];
                    continue;
                }
            }
        }
        return;
    }

    unknown_csi(tb, ch);
    assert(false);
}

void csi_dec_private_mode_set(struct termbuf *tb, char final_byte) {
    // This function is called whenever an escape sequence of the form
    //     ESC[?<p>h or ESC[?<p>l
    // is found. These function are for setting and unsetting "DEC Private
    // modes".. I think "private" means roughly that these types of modes
    // werent't part of any sort of standard. Of course many of these modes have
    // becomme de-facto standard and we wish to implement some of these here.
    //
    // As I understand it, the parameter <p> is a number that corresponds to one
    // specific flag, and when the final byte is 'h' (high) we set the flag, and
    // when the final byte is 'l' (low bit) we reset the flag. As such, the
    // structure of this code is simple a switch statement that sets a local
    // flag (i.e. a bitmask) variable, and in the end we use the flag variable
    // to set the right bit of `tb->flags`.
    //
    // For details on what flags these parameters correspond to, see:
    // https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character-lparen-s-rparen:CSI-?-Pm-h.1D0E

    struct ansi_csi_chomping *data = &tb->p_data.ansi_csi_chomping;

    assert(data->initial_char == '?');
    assert(final_byte == 'h' || final_byte == 'l');
    assert(data->params[0] != (uint16_t) -1);
    assert(data->params[1] == (uint16_t) -1);
    assert(data->params[2] == (uint16_t) -1);
    assert(data->params[3] == (uint16_t) -1);
    assert(data->params[4] == (uint16_t) -1);

    uint16_t flag = 0;

    switch(data->params[0]) {
    case 1:
        // ESC[?1h Set Cursor key mode (DECCKM)
        flag = FLAG_APPLICATION_CURSOR;
        break;
    case 7:
        // ESC[?1h Set autowrap mode (DECAWM)
        flag = FLAG_AUTOWRAP_MODE;
        break;
    case 12:
        // Start / stop blinking cursor.
        // We don't want a blinking cursor for our terminal, so we just ignore
        // this.
        return;
    case 25:
        // ESC[?25h Show/hide the cursor (DECTCEM).
        // https://vt100.net/docs/vt510-rm/DECTCEM.html
        flag = FLAG_HIDE_CURSOR;
        break;
    case 1049:
        // ESC[?1049h enable alternative screen buffer.
        // TODO: handle this.
        printf("TODO handle ESC[?1049h / ESC[?1049l\n");
        return;
    case 2004:
        // ESC[?2004h "Turn on bracketed paste mode."
        flag = FLAG_BRACKETED_PASTE_MODE;
        break;
    default:
        printf("\nWe got a sequence of the form ESC[?%d%c. But we don't know "
               "how to handle parameter %d.\n",
               data->params[0],
               final_byte,
               data->params[0]);
        assert(false);
    }

    if (final_byte == 'h') {
        tb->flags |= flag;
        return;
    }
    if (final_byte == 'l') {
        tb->flags &= ~flag;
        return;
    }

    unknown_csi(tb, final_byte);
    assert(false);
}

void unknown_csi(struct termbuf *tb, char ch) {
    struct ansi_csi_chomping *data = &tb->p_data.ansi_csi_chomping;
    uint8_t ic  = data->initial_char;
    uint8_t intermediate = data->intermediate;

    uint8_t len = data->current_param + 1;
    if (data->params[data->current_param] == (uint16_t) -1) {
        len --;
    }

    uint16_t p1 = data->params[0];
    uint16_t p2 = data->params[1];
    uint16_t p3 = data->params[2];
    uint16_t p4 = data->params[3];
    uint16_t p5 = data->params[4];

    printf("\n"
           "Got an unknown ANSI escape sequence with:\n"
           "    ch            : '%c' (decimal %d).\n"
           "    initial_char  : '%c' (decimal %d).\n"
           "    current_param : %d.\n"
           "    len           : %d.\n"
           "    param1        : %d.\n"
           "    param2        : %d.\n"
           "    param3        : %d.\n"
           "    param4        : %d.\n"
           "    param5        : %d.\n"
           "    intermediate  : '%c' (decimal %d).\n",
           ch, ch,
           ic, ic,
           data->current_param,
           len,
           p1, p2, p3, p4, p5,
           data->intermediate, data->intermediate);
}

void action_osc_chomp_start(struct termbuf *tb, char ch) {
    assert(ch == ']');

    tb->p_data.ansi_osc_chomping = (struct ansi_osc_chomping) {
        .len = 0,
        // No need to initialize data.
        // .data =
    };
}

void action_osc_chomp(struct termbuf *tb, char ch) {
    struct ansi_osc_chomping *data = &tb->p_data.ansi_osc_chomping;
    assert(data->len < 1024);
    data->data[data->len] = ch;
    data->len ++;
}

void action_osc_chomp_end(struct termbuf *tb, char ch) {
    // See
    // - https://www.xfree86.org/current/ctlseqs.html
    //   for OSC sequences supported by xterm.
    // - https://wezfurlong.org/wezterm/escape-sequences.html#operating-system-command-sequences
    // - https://iterm2.com/documentation-escape-codes.html
    //   for some other random OSC sequences.

    struct ansi_osc_chomping *data = &tb->p_data.ansi_osc_chomping;

    // ESC]0;<string>ST Set the title and icon nae of the terminal window.
    if(data->len >= 2 && data->data[0] == '0' && data->data[1] == ';') {
        // TODO: Should we handle?
        return;
    }

    // ESC]1;<string>ST Set the icon name of the terminal window.
    if(data->len >= 2 && data->data[0] == '1' && data->data[1] == ';') {
        // TODO: Should we handle?
        return;
    }

    // ESC]2;<string>ST Set the title of the terminal window.
    if(data->len >= 2 && data->data[0] == '2' && data->data[1] == ';') {
        // TODO: Should we handle?
        return;
    }

    // ESC]3;<string>ST Change an X property of the window. If <string> is of
    // the form "<name>=<value> we set the prop to that value. If <string is of
    // the form "<name>" we should delete that property.
    if(data->len >= 2 && data->data[0] == '3' && data->data[1] == ';') {
        // TODO: Handle.
        assert(false);
    }

    // ESC]7;file:/<path>ST Set current working directory to <path>.
    // This instruction has no effect on the shell itself, rather it's just
    // information that the terminal may wish to use sometimes. Quoting
    // wezfurlong.org: "When the current working directory has been set via OSC
    // 7 [on macOS terminal], spawning a new tab will use the current working
    // directory of the current tab, so that you don't have to manually change
    // the directory."
    if(data->len >= 2 && data->data[0] == '7' && data->data[1] == ';') {
        // There's nothing we have to do here, maybe we'd want to use this info
        // for something at some point in the future?
        return;
    }

    // ESC]8;;<hyperlink>ST tells the terminal that the normal output that
    // follows can be associated with <hyperlink> so that it's clickable. For
    // instance, when you do `ls` in NuShell it can output something like
    // ESC]8;;file:///home/emma/foo.txtSTfooESC]8;; which tell's the terminal
    // that the text foo.txt has a link associated with it. A hyperlink region
    // is ended with ESC]8;;
    if(data->len >= 3
       && data->data[0] == '8'
       && data->data[1] == ';'
       && data->data[2] == ';') {
        // There's nothing we have to do here, maybe we'd want to use this info
        // for something at some point in the future?
        return;
    }

    // ESC]133;<A|B|C|D|D;<exit status>>ST Let's the terminal know about
    // different "semantic regions" on the terminal. For instance "hey terminal,
    // just FIY this current is all output of a previous command". This makes it
    // possible for a terminal to implement some inteligent movement command
    // like "jump back in the scrollback buffer to the previous command I
    // executed".
    if(data->len >= 5
       && data->data[0] == '1'
       && data->data[1] == '3'
       && data->data[2] == '3'
       && data->data[3] == ';'
       && data->data[4] >= 'A'
       && data->data[4] <= 'D') {
        // There's nothing we have to do here, maybe we'd want to use this info
        // for something at some point in the future?
        return;
    }

    if (data->len == 1024) {
        data->len --;
    }
    data->data[data->len] = '\0';
    printf("Got an unexpected OSC string \"%s\".\n", data->data);
    assert(false);
}

/*[[[cog
import cog

def r(start, end):
    return range(start, end + 1)

P_STATE_GROUND              = "P_STATE_GROUND"
P_STATE_CHOMP1              = "P_STATE_CHOMP1"
P_STATE_CHOMP2              = "P_STATE_CHOMP2"
P_STATE_CHOMP3              = "P_STATE_CHOMP3"
P_STATE_ESC                 = "P_STATE_ESC"
P_STATE_CSI                 = "P_STATE_CSI"
P_STATE_CSI_PARAMS          = "P_STATE_CSI_PARAMS"
P_STATE_CSI_INTERMEDIATE    = "P_STATE_CSI_INTERMEDIATE"
P_STATE_OSC                 = "P_STATE_OSC"
P_STATE_OSC_ESC             = "P_STATE_OSC_ESC"
P_STATE_NF                  = "P_STATE_NF"

action_noop                   = "action_noop"
action_fail                   = "action_fail"
action_print                  = "action_print"
action_c0                     = "action_c0"
action_fp                     = "action_fp"
action_utf8_chomp_start       = "action_utf8_chomp_start"
action_utf8_chomp_continue    = "action_utf8_chomp_continue"
action_utf8_chomp_end         = "action_utf8_chomp_end"
action_nf_chomp_start         = "action_nf_chomp_start"
action_nf_chomp_continue      = "action_nf_chomp_continue"
action_nf_chomp_end           = "action_nf_chomp_end"
action_csi_chomp_start        = "action_csi_chomp_start"
action_csi_chomp_initial_char = "action_csi_chomp_initial_char"
action_csi_chomp_param        = "action_csi_chomp_param"
action_csi_chomp_next_param   = "action_csi_chomp_next_param"
action_csi_intermediate       = "action_csi_intermediate"
action_csi_chomp_final_byte   = "action_csi_chomp_final_byte"
action_osc_chomp_start        = "action_osc_chomp_start"
action_osc_chomp              = "action_osc_chomp"
action_osc_chomp_end          = "action_osc_chomp_end"

table = [
  ##################
  # P_STATE_GROUND #
  ##################
  # Got a non-ESC C0 control character.
  [ P_STATE_GROUND, r(0  , 26 ), P_STATE_GROUND, action_c0                    ],
  # Got the C0 control character "ESC".
  [ P_STATE_GROUND, [ 27 ]     , P_STATE_ESC   , action_noop                  ],
  # Got a non-ESC C0 control character.
  [ P_STATE_GROUND, r(28 , 31 ), P_STATE_GROUND, action_c0                    ],
  # Got an printable ASCII character / single-byte utf8.
  [ P_STATE_GROUND, r(32 , 126), P_STATE_GROUND, action_print                 ],
  # Got the _almost_ C0 control character "DEL".
  [ P_STATE_GROUND, [ 127 ]    , P_STATE_GROUND, action_fail                  ],
  # Got a utf8 contiuation byte when not expecting it.
  [ P_STATE_GROUND, r(128, 191), P_STATE_GROUND, action_fail                  ],
  # Got an "unused" byte.
  [ P_STATE_GROUND, [192, 193] , P_STATE_GROUND, action_fail                  ],
  # Got the start of a 2-byte utf-8.
  [ P_STATE_GROUND, r(194, 223), P_STATE_CHOMP1, action_utf8_chomp_start      ],
  # Got the start of a 3-byte utf-8.
  [ P_STATE_GROUND, r(224, 239), P_STATE_CHOMP2, action_utf8_chomp_start      ],
  # Got the start of a 4-byte utf-8.
  [ P_STATE_GROUND, r(240, 244), P_STATE_CHOMP3, action_utf8_chomp_start      ],
  # Go an "unused" byte.
  [ P_STATE_GROUND, r(245, 255), P_STATE_GROUND, action_fail                  ],

  ##################
  # P_STATE_CHOMP1 #
  ##################
  # Got an unexpected ASCII character / single-byte utf8.
  [ P_STATE_CHOMP1, r(0  , 127), P_STATE_GROUND, action_fail                  ],
  # Got the final utf8 contiuation byte.
  [ P_STATE_CHOMP1, r(128, 191), P_STATE_GROUND, action_utf8_chomp_end        ],
  # Got an "unused" byte.
  [ P_STATE_CHOMP1, [192, 193] , P_STATE_GROUND, action_fail                  ],
  # Got an unexpected start of a 2-byte utf-8.
  [ P_STATE_CHOMP1, r(194, 223), P_STATE_CHOMP1, action_fail                  ],
  # Got an unexpected start of a 3-byte utf-8.
  [ P_STATE_CHOMP1, r(224, 239), P_STATE_CHOMP2, action_fail                  ],
  # Got an unexpected start of a 4-byte utf-8.
  [ P_STATE_CHOMP1, r(240, 244), P_STATE_CHOMP3, action_fail                  ],
  # Go an "unused" byte.
  [ P_STATE_CHOMP1, r(245, 255), P_STATE_GROUND, action_fail                  ],

  ##################
  # P_STATE_CHOMP2 #
  ##################
  # Got an unexpected ASCII character / single-byte utf8.
  [ P_STATE_CHOMP2, r(0  , 127), P_STATE_GROUND, action_fail                  ],
  # Got a utf8 contiuation byte.
  [ P_STATE_CHOMP2, r(128, 191), P_STATE_CHOMP1, action_utf8_chomp_continue   ],
  # Got an "unused" byte.
  [ P_STATE_CHOMP2, [192, 193] , P_STATE_GROUND, action_fail                  ],
  # Got an unexpected start of a 2-byte utf-8.
  [ P_STATE_CHOMP2, r(194, 223), P_STATE_CHOMP1, action_fail                  ],
  # Got an unexpected start of a 3-byte utf-8.
  [ P_STATE_CHOMP2, r(224, 239), P_STATE_CHOMP2, action_fail                  ],
  # Got an unexpected start of a 4-byte utf-8.
  [ P_STATE_CHOMP2, r(240, 244), P_STATE_CHOMP3, action_fail                  ],
  # Go an "unused" byte.
  [ P_STATE_CHOMP2, r(245, 255), P_STATE_GROUND, action_fail                  ],

  ##################
  # P_STATE_CHOMP3 #
  ##################
  # Got an unexpected ASCII character / single-byte utf8.
  [ P_STATE_CHOMP3, r(0  , 127), P_STATE_GROUND, action_fail                  ],
  # Got a utf8 contiuation byte.
  [ P_STATE_CHOMP3, r(128, 191), P_STATE_CHOMP2, action_utf8_chomp_continue   ],
  # Got an "unused" byte.
  [ P_STATE_CHOMP3, [192, 193] , P_STATE_GROUND, action_fail                  ],
  # Got an unexpected start of a 2-byte utf-8.
  [ P_STATE_CHOMP3, r(194, 223), P_STATE_CHOMP1, action_fail                  ],
  # Got an unexpected start of a 3-byte utf-8.
  [ P_STATE_CHOMP3, r(224, 239), P_STATE_CHOMP2, action_fail                  ],
  # Got an unexpected start of a 4-byte utf-8.
  [ P_STATE_CHOMP3, r(240, 244), P_STATE_CHOMP3, action_fail                  ],
  # Go an "unused" byte.
  [ P_STATE_CHOMP3, r(245, 255), P_STATE_GROUND, action_fail                  ],

  ###############
  # P_STATE_ESC #
  ###############
  # Got some follow-up byte we we're not expecting
  [ P_STATE_ESC, r(0, 31),   P_STATE_GROUND, action_fail                      ],
  # Got a so called "nF" escape sequence
  [ P_STATE_ESC, r(32, 47),  P_STATE_NF,     action_nf_chomp_start            ],
  # Got a so called "Fp" escape sequence
  [ P_STATE_ESC, r(48, 63),  P_STATE_GROUND, action_fp                        ],
  # Got some follow-up byte we we're not expecting
  [ P_STATE_ESC, r(64, 90),  P_STATE_GROUND, action_fail                      ],
  # Got '['. "ESC[" is a "control sequence introducer" (CSI). Basically we have
  # the start of an ANSI escape sequence now.
  [ P_STATE_ESC, [ 91 ],     P_STATE_CSI   , action_csi_chomp_start           ],
  # Got some follow-up byte we we're not expecting
  [ P_STATE_ESC, [ 92 ],     P_STATE_GROUND, action_fail                      ],
  # Got ']'. "ESC]" marks the begining of an "operating system command" (OSC).
  [ P_STATE_ESC, [ 93 ],     P_STATE_OSC   , action_osc_chomp_start           ],
  # Got some follow-up byte we we're not expecting
  [ P_STATE_ESC, r(94, 255), P_STATE_GROUND, action_fail                      ],


  ##############
  # P_STATE_NF #
  ##############
  # Got something unexpected.
  [ P_STATE_NF, r(0, 31),    P_STATE_GROUND,  action_fail                     ],
  # We got a continuation byte.
  [ P_STATE_NF, r(32, 47),   P_STATE_NF,      action_nf_chomp_continue        ],
  # We got a byte marking the end of the nF sequence.
  [ P_STATE_NF, r(48, 126),  P_STATE_GROUND,  action_nf_chomp_end             ],
  # Got something unexpected.
  [ P_STATE_NF, r(127, 255), P_STATE_GROUND,  action_fail                     ],


  ###############
  # P_STATE_CSI #
  ###############
  # See
  # https://en.wikipedia.org/wiki/ANSI_escape_code#Control_Sequence_Introducer_commands#Control_Sequence_Introducer_commands
  # for information on how to interpret these.

  # Got something unexpected
  [ P_STATE_CSI, r(0, 0x1F)       , P_STATE_GROUND, action_fail               ],
  # Got an "intermediate" byte
  [ P_STATE_CSI_PARAMS, r(0x20, 0x2F), P_STATE_CSI_INTERMEDIATE, action_csi_intermediate],
  # Got a character 0-9.
  [ P_STATE_CSI, r(0x30 , 57), P_STATE_CSI_PARAMS, action_csi_chomp_param       ],
  # Got something unexpected
  [ P_STATE_CSI, r(58 , 62)       , P_STATE_GROUND, action_fail               ],
  # Got a '?' has the initial char
  [ P_STATE_CSI, [ 63 ], P_STATE_CSI_PARAMS, action_csi_chomp_initial_char ],
  # Got a "final byte".
  [ P_STATE_CSI_PARAMS, r(64, 126), P_STATE_GROUND, action_csi_chomp_final_byte],
  [ P_STATE_CSI, r(127, 255)      , P_STATE_GROUND, action_fail               ],


  ######################
  # P_STATE_CSI_PARAMS #
  ######################
  # Got something unexpected
  [ P_STATE_CSI_PARAMS, r(0, 0x1F),    P_STATE_GROUND, action_fail            ],
  # Got an "intermediate" byte
  [ P_STATE_CSI_PARAMS, r(0x20, 0x2F), P_STATE_CSI_INTERMEDIATE, action_csi_intermediate],
  # We got a number!
  [ P_STATE_CSI_PARAMS, r(48, 57),  P_STATE_CSI_PARAMS, action_csi_chomp_param],
  # Got something unexpected
  [ P_STATE_CSI_PARAMS, [58]     ,   P_STATE_GROUND, action_fail              ],
  # Got something unexpected
  # Got a ';', meaning we should start chomping a new parameter.
  [ P_STATE_CSI_PARAMS, [59], P_STATE_CSI_PARAMS, action_csi_chomp_next_param ],
  # Got something unexpected
  [ P_STATE_CSI_PARAMS, r(60, 63), P_STATE_GROUND, action_fail              ],
  # Go a "final byte" signaling the the ANSI escape sequence is now over
  [ P_STATE_CSI_PARAMS, r(64, 126),  P_STATE_GROUND, action_csi_chomp_final_byte],
  # Got something unexpected
  [ P_STATE_CSI_PARAMS, [127]      , P_STATE_GROUND, action_fail              ],
  # Got something unexpected
  [ P_STATE_CSI_PARAMS, r(128, 255), P_STATE_GROUND, action_fail              ],



  ############################
  # P_STATE_CSI_INTERMEDIATE #
  ############################
  # Got something unexpected
  [ P_STATE_CSI_INTERMEDIATE, r(0, 0x1F), P_STATE_GROUND, action_fail ],
  # Got another intermediate byte
  [ P_STATE_CSI_INTERMEDIATE, r(0x20, 0x2F), P_STATE_CSI_INTERMEDIATE, action_csi_intermediate ],
  # Got something unexpected
  [ P_STATE_CSI_INTERMEDIATE, r(0x30, 0x3F), P_STATE_GROUND, action_fail ],
  # Got a final byte
  [ P_STATE_CSI_INTERMEDIATE, r(0x40, 0x7E), P_STATE_GROUND, action_csi_chomp_final_byte ],
  # Got something unexpected
  [ P_STATE_CSI_INTERMEDIATE, r(0x7F, 0xFF), P_STATE_GROUND, action_fail ],



  ###############
  # P_STATE_OSC #
  ###############
  [ P_STATE_OSC, r(0, 6 ), P_STATE_OSC, action_osc_chomp ],
  # Bell '\a' can be used to terminate an OSC sequence
  [ P_STATE_OSC, [ 7 ], P_STATE_GROUND, action_osc_chomp_end ],
  [ P_STATE_OSC, r(8, 26), P_STATE_OSC, action_osc_chomp ],
  [ P_STATE_OSC, [ 27 ], P_STATE_OSC_ESC, action_noop ],
  [ P_STATE_OSC, r(28, 255), P_STATE_OSC, action_osc_chomp ],


  ###################
  # P_STATE_OSC_ESC #
  ###################
  # We got something unexpected, go back to P_STATE_OSC
  [ P_STATE_OSC_ESC, r(0, 91), P_STATE_OSC, action_osc_chomp ],
  # We got a '\' after having recived an "ESC" which marks the end of the
  # string
  [ P_STATE_OSC_ESC, [ 92 ], P_STATE_GROUND, action_osc_chomp_end ],
  # We got something unexpexted, go pack to P_STATE_OSC
  [ P_STATE_OSC_ESC, r(93, 255), P_STATE_OSC, action_osc_chomp ],
]

cog.outl("struct parser_table_entry parser_table[256 * NSTATES] = {")
n = 0
i = 0
for [state, events, newstate, action] in table:
    for event in events:
        if event != i:
            cog.error("sate: %s got event %d while expecting %d."
                      % (state, event, i))

        cog.outl("    { .new_state = %s," % newstate)
        cog.outl("      .action = &%s, }," % action)
        n += 1
        i = (i + 1) % 256
cog.outl("};")

if n % 256 != 0:
    cog.error("""I think you made some misstake because the array i crated does\
 not have a length that's a multiple of 256, I got %d.""" % n)
]]]*/
struct parser_table_entry parser_table[256 * NSTATES] = {
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_ESC,
      .action = &action_noop, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_c0, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_print, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_utf8_chomp_start, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_utf8_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_utf8_chomp_continue, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP1,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP2,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_fail, },
    { .new_state = P_STATE_CHOMP3,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_start, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_start, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_start, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_start, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_start, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_start, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_start, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_start, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_start, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_start, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_start, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_start, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_start, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_start, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_start, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_start, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_CSI,
      .action = &action_csi_chomp_start, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp_start, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_continue, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_continue, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_continue, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_continue, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_continue, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_continue, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_continue, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_continue, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_continue, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_continue, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_continue, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_continue, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_continue, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_continue, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_continue, },
    { .new_state = P_STATE_NF,
      .action = &action_nf_chomp_continue, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_nf_chomp_end, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_initial_char, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_param, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_CSI_PARAMS,
      .action = &action_csi_chomp_next_param, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_CSI_INTERMEDIATE,
      .action = &action_csi_intermediate, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_csi_chomp_final_byte, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_GROUND,
      .action = &action_fail, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_osc_chomp_end, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC_ESC,
      .action = &action_noop, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_GROUND,
      .action = &action_osc_chomp_end, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
    { .new_state = P_STATE_OSC,
      .action = &action_osc_chomp, },
};
//[[[end]]]



////////////////
// UNIT TESTS //
////////////////


void cu_assert_buf_equals(CuTest *tc, struct termbuf *tb1, struct termbuf *tb2)
{
    CuAssertIntEquals(tc, tb1->nrows, tb2->nrows);
    CuAssertIntEquals(tc, tb1->ncols, tb2->ncols);

    int nrows = tb1->nrows;
    int ncols = tb1->ncols;

    unsigned char *tmp1 = calloc(nrows, sizeof(struct termbuf_char));
    unsigned char *tmp2 = calloc(nrows, sizeof(struct termbuf_char));

    for(int row = 1; row <= tb1->nrows; row++) {
        for (int col = 1; col <= tb1->ncols; col++) {
            int index = col - 1 + (row - 1) * ncols;
            struct termbuf_char c1 = tb1->buf[index];
            struct termbuf_char c2 = tb1->buf[index];
            int len1 = c1.flags & FLAG_LENGTH_MASK;
            int len2 = c1.flags & FLAG_LENGTH_MASK;
            CuAssertIntEquals(tc, 1, len1);
            CuAssertIntEquals(tc, 1, len2);
            tmp1[index] = c1.utf8_char[0];
            tmp2[index] = c1.utf8_char[0];
        }
    }

    CuAssertBytesEquals(tc,
                        tmp1,
                        tmp2,
                        tb1->nrows * tb1->ncols);

    free(tmp1);
    free(tmp2);
}

void insert_termbuf_contents(struct termbuf *tb, const char *contents) {
    int old_flags = tb->flags;
    tb->flags = FLAG_AUTOWRAP_MODE;
    while(*contents != '\0') {
        termbuf_insert(tb, ((uint8_t *) contents), 1);
        contents ++;
    }
    tb->flags = old_flags;
}

void test_buffer_resize_noop(CuTest *tc) {
    int dummy_pty = 0;

    const char *content =
        "12345"
        "abcde"
        "xyzwh"
        "ijklm";

    struct termbuf tb1;
    termbuf_initialize(4, 5, dummy_pty, &tb1);
    insert_termbuf_contents(&tb1, content);

    struct termbuf tb2;
    termbuf_initialize(4, 5, dummy_pty, &tb2);
    insert_termbuf_contents(&tb2, content);
    termbuf_resize(&tb2, 4, 5);  // Resize to the same contents as before.

    cu_assert_buf_equals(tc, &tb1, &tb2);
}

void test_buffer_resize_shrink(CuTest *tc) {
    int dummy_pty = 0;

    const char *content1 =
        "123"
        "abc";

    const char *content2 =
        "12345"
        "abcde"
        "xyzwh"
        "ijklm";

    struct termbuf tb1;
    termbuf_initialize(2, 3, dummy_pty, &tb1);
    insert_termbuf_contents(&tb1, content1);

    struct termbuf tb2;
    termbuf_initialize(4, 5, dummy_pty, &tb2);
    insert_termbuf_contents(&tb2, content2);
    termbuf_resize(&tb2, 2, 3);

    cu_assert_buf_equals(tc, &tb1, &tb2);
}

void test_buffer_resize_grow_shrink(CuTest *tc) {
    int dummy_pty = 0;

    const char *content1 =
        "123"
        "abc";

    const char *content2 =
        "123"
        "abc";

    struct termbuf tb1;
    termbuf_initialize(2, 3, dummy_pty, &tb1);
    insert_termbuf_contents(&tb1, content1);

    struct termbuf tb2;
    termbuf_initialize(2, 3, dummy_pty, &tb2);
    insert_termbuf_contents(&tb2, content2);
    termbuf_resize(&tb2, 4, 5);
    termbuf_resize(&tb2, 2, 3);

    cu_assert_buf_equals(tc, &tb1, &tb2);
}

CuSuite *termbuf_test_suite() {
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_buffer_resize_noop);
    SUITE_ADD_TEST(suite, test_buffer_resize_shrink);
    SUITE_ADD_TEST(suite, test_buffer_resize_grow_shrink);
    return suite;
}
