#ifndef INCLUDED_TERMBUF_H
#define INCLUDED_TERMBUF_H

#include <stdlib.h>
#include <stdint.h>
#include <X11/Xlib.h>

#include "./ringbuf.h"

#include "./CuTest.h"

extern const uint8_t four_bit_colors[16 * 3];
extern const uint8_t eight_bit_colors[256 * 3];

// These flags are used in two places
// 1) Each terminal cell (termbuf_char) has a flags field that represents it's
//    apperance.
// 2) The terminal itself (termbuf) uses these flags to represent part of it's
//    state.
// The terminal cells only use the first 8-bits (i.e. the flags up to and
// including `FLAG_STRIKEOUT` and hence a uint8_t is enough. The terminal state
// uses all flags except for the first FLAG_LENGT_n flags and so in theory it
// need no more than an uint8_t as well, but right now we have reserved an
// uint16_t for it.
#define FLAG_LENGTH_0 0                // 0b0000000000000000
#define FLAG_LENGTH_1 1                // 0b0000000000000001
#define FLAG_LENGTH_2 2                // 0b0000000000000010
#define FLAG_LENGTH_3 3                // 0b0000000000000011
#define FLAG_LENGTH_4 4                // 0b0000000000000100
#define FLAG_LENGTH_MASK 7             // 0b0000000000000111
#define FLAG_BOLD      8               // 0b0000000000001000
#define FLAG_FAINT     16              // 0b0000000000010000
#define FLAG_ITALIC    32              // 0b0000000000100000
#define FLAG_UNDERLINE 64              // 0b0000000001000000
#define FLAG_STRIKEOUT 128             // 0b0000000010000000
// These are only used by the `struct termbuf`
#define FLAG_BRACKETED_PASTE_MODE 256  // 0b0000000100000000
// Hide/show the cursor.
// https://vt100.net/docs/vt510-rm/DECTCEM.html
#define FLAG_DECTCEM 512               // 0b0000001000000000
// This flag determines wheter or not text should wrap to the next line if there
// is no space on the current line.
// see: https://vt100.net/docs/vt510-rm/DECAWM.html
#define FLAG_DECAWM   1024             // 0b0000010000000000
// When this flag is set new screen output has the foreground and background
// colors swapped. Set and unset with ESC[7m resp. ESC[27m.
#define FLAG_INVERT_COLORS   2048      // 0b0000100000000000
// DECCKM and DECKPAM influence how the terminal communicates which
// cursor/keypad are pressed to the terminal. See ./keymap.c for how these flags
// comme into play.
// See: https://vt100.net/docs/vt510-rm/DECCKM.html
// See: https://vt100.net/docs/vt510-rm/DECKPAM.html
#define FLAG_DECCKM  4096             // 0b0001000000000000
#define FLAG_DECKPAM 8192             // 0b0010000000000000

// Represents a single unicode codepoint along with styling information such as
// color, if it's bold, italic, etc.
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
    // The parser encountered the C0/C1 control character "ESC".
    P_STATE_ESC    = 4,
    // The parser has now encountered "ESC" followed by a byte in the range
    // 32-47 (decimal), meaning we've started parsing an "nF" escape sequence.
    // See: https://en.wikipedia.org/wiki/ANSI_escape_code#nF_Escape_sequences
    P_STATE_NF = 5,
    // The parser has now encountered "ESC[", meaning we started a CSI escape
    // sequence.
    P_STATE_CSI              = 6,
    P_STATE_CSI_PARAMS       = 7,
    P_STATE_CSI_INTERMEDIATE = 8,
    // The parser has now encountered "ESC]", meaning we started an OSC escape
    // sequence We won't do anything with the OSC sequences so we just chomp
    // them til their end (marked by "ESC\" (C1 "ST")) and then ignore the
    // results.
    P_STATE_OSC        = 9,
    // We got the "ESC" in what we pressume to be a C1 string terminator "ESC\".
    P_STATE_OSC_ESC    = 10,
};
#define NSTATES 11

#define CSI_CHOMPING_MAX_PARAMS 6

union parser_data {
    struct utf8_chomping {
        uint8_t len;
        uint8_t utf8_char[4];
    } utf8_chomping;
    struct ansi_nf_chomping {
        uint8_t  initial_char;  // in the range 32--47
        uint16_t len;           // the number of bytes in the sequence
    } ansi_nf_chomping;
    struct ansi_csi_chomping {
        uint8_t  initial_char;  // one of '\0' (no initial char) or '?'
        uint8_t  current_param;
        // if a param is -1 (i.e. largest uint16_t) then we interpret it as
        // missing, for instance ESC[;10H would give something like:
        // `params = { -1, 10, -1, -1, -1}
        uint16_t params[CSI_CHOMPING_MAX_PARAMS];
        uint8_t  intermediate;  // Byte in range 0x20--0x2F.
                                // -1 means no intermediate byte.
                                // Any number of these can occur after
                                // parameters and befire the finaly byte, for
                                // now we support only one intermediate byte
    } ansi_csi_chomping;
    struct ansi_osc_chomping {
        uint16_t len;
        uint8_t data[1024];
    } ansi_osc_chomping;
};

struct termbuf {
    int nrows;
    int ncols;
    int row;
    int col;
    uint16_t flags;
    uint8_t fg_color_r;
    uint8_t fg_color_g;
    uint8_t fg_color_b;
    uint8_t bg_color_r;
    uint8_t bg_color_g;
    uint8_t bg_color_b;
    // There are two so called "Fe" escape sequences that instructs the terminal
    // to save resp. restore the cursor position, so we save this here.
    // TODO: We should also save things like "shift state" and "formatting
    //       attributes".
    // FYI the two sequences are "ESC7" to save and "ESC8" to restore.
    // If the value of these are -1 then not cursor has been saved.
    int saved_row;
    int saved_col;
    // Sometimes we need to transmit data back to the shell after having recived
    // certain escape sequences from the shell. This is the file descriptor we
    // write to.
    int pty_fd;
    enum  parser_state p_state;
    union parser_data  p_data;
    struct termbuf_char *buf;
    // The scrollback buffer
    struct ringbuf scrollback;
    // The number of rows that the user has scrolled into the scrollback buffer.
    int scroll_position;
    size_t scroll_offset;
};

void termbuf_initialize(int nrows,
                        int ncols,
                        int pty_fd,
                        struct termbuf *tb_ret);

// Parses bytes that we're sent by the shell, including things like C0, C1, and
// Fe escape sequences, and does the appropriate thing.
void termbuf_parse(struct termbuf *tb, uint8_t *data, size_t len);

// Insert a single utf8 encoded character with the styling (bold, italic,
// foreground color, etc.) that the terminal currently has, and advance to
// cursor appropriately.
void termbuf_insert(struct termbuf *tb, const uint8_t *utf8_char, int len);

// When the cursor is at the bottom at the terminal, and we encounter a line
// feed '\n' we should push the topmost row into the scrollback buffer and shift
// all other lines up one row to make room for a new empty row. This function
// does that
void termbuf_shift(struct termbuf *tb);

void termbuf_resize(struct termbuf *tb, int nnrows, int nncols);


void termbuf_scrollback_push_row(struct termbuf *tb,
                                 struct termbuf_char *data,
                                 int length);
void termbuf_scrollback_get_row(struct termbuf *tb,
                                int offset,
                                const char **ascii_ret,
                                int *length_ret);

CuSuite *termbuf_test_suite();

#endif /* INCLUDED_TERMBUF_H */
