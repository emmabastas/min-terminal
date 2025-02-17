/*
  This module is concearned with one thing only: When a user presses a key on
  their keyboard, what should we send to the shell? This task is suprisingly
  involved, hence it's own module. Before we discuss the details, some
  terminology is in order

  * TERMINOLOGY
    * KEY. Refers to an actual physical button the user presses on their
          keyboard.
    * XKeyPressedEvent. We get this event from X11 whenever a KEY is pressed.
    * KEYCODE. Every KEY has a numerical KEYCODE associated with it. This number
          lives in the XKeyPressedEvent's `xxx` field.
    * MODIFIER KEY. These refer to special KEYs like Shift, Ctrl, and so on,
          they "modify" how other KEYs are to be interpreted, for instance, if
          I type the 'd' while simulataneously holding down Shift and AltGr the
          letter 'Ð' appears on my screen. XKeyPressedEvent's `state` field
          contains an OR'ed bitset of all MODIFIER KEYs pressed down while
          XKeyPressedEvent occurs.
    * LETTERS, SYMBOLS and INPUT CONTEXTs. Altough KEY 38 is just a number to
          the computer, me the human can see that the LETTER 'a' on the keycap,
          and so I expect the computer to understand it as such. Similarily I
          expect the computer to understand that KEY 114 is the right-arrow
          SYMBOL. If I then change from a QWERTY layout to DVORAK, all of a
          sudden I expect the computer to understand KEY 38 as the LETTER 'XX'
          instead. How is this done? Well, X11 has a notion of an INPUT CONTEXT
          which detrmines how it's utility functions like `Xutf8LookupString`
          map KEYs to LETTERS and SYMBOLS.
    * INPUT CONTEXT. A KEYCODE is just a number, when I type 'd' on the computer
          while holding down Shift and AltGr the computer only sees:
          > Key 40 pressed while key 50 and 92 where held down.
          In order to make sense of this we need to map key codes to characters,
          x11 has utility functions for doing so, and the X Input Context (XIC)
          is what determines how the keycodes are mapped. If I change the IC the
          computer will still se the same KEYCODEs, it's just that x11 utility
          functions like `Xutf8LookupString` will map them do different
          charecters.
          * ESCAPE SEQUENCE. If I type KEY 38 and the X Input Context maps this
          to the LETTER 'a' it pretty obvious what to do: The terminal sends the
          byte 0x61 to the shell. However, what happens if I type KEY 114 which
          the XIC tells me is the "right arrow" SYMBOL. There is no letter that
          corresponds to this SYMBOL. The solution is to send special escape
          sequences to the computer, for instance (depending on certain terminal
          flags) I chould send the bytes 0x1b, 0x5b and 0x43 (ESC[C) which the
          shell would iterpret as a the right arrow SYMBOL having been pressed.

          There's some writing about this on Wikipedia:
          https://en.wikipedia.org/wiki/ANSI_escape_code#Terminal_input_sequences

          Since X11 has utility functions for mapping KEYs to LETTERs and
          SYMBOLs we happilly let X11 do this for us. However, we still need to
          map certain SYMBOLs to ESCAPE SEQUENCEs ourselves, this is where the
          complexity commes in. There are two things that influcence how a
          specific SYMBOL is mapped:
          1) MODIFIER KEYs.
          2) Terminal flags. In particular the two flags FLAG_APPLICATION_CURSOR
          and FLAG_APPLICATION_KEYPAD.
          We encode our mappings as a list of "constraints" which is a sturct
          containing an ESCAPE SEQUENCE that should be sent if certain
          constraints are met. So when a SYMBOL is typed we run through this
          list of constraints until one of them is met, and then we know what
          ESCAPE SEQUENCE to send.
*/



#include "./keymap.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>

#define XK_MISCELLANY  // These defines makes keysymde.h include certain things
#define XK_XKB_KEYS    // we want.
#include <X11/keysymdef.h>

#include "./util.h"



static struct termbuf *tb;
static XIC input_context;
static int primary_pty_fd;

void keymap_initialize(struct termbuf *_tb,
                       XIC _input_context,
                       int _primary_pty_fd) {
    tb = _tb;
    input_context = _input_context;
    primary_pty_fd = _primary_pty_fd;
}

// Represents a constraint.
struct constraint_s {
    // The symbol we want to map if the following two constraints are met.
    KeySym  keysym;
    // This is a bitset of X11 modifier keys, this constraint is met if it
    // matches the XKeyPressedEvent's `state`. There are also the special
    // bitsets ANY_MOD and NO_MOD defined bellow, there's also a
    // IGNORED_MODIFIERS which influences what constitutes a match.
    uint c1;
    // This constraint concearns the two terminal flags FLAG_APPLICATION_CURSOR
    // and FLAG_APPLICATION_KEYPAD, and wheter or not numlock is activated on
    // the keyboard (indicated by X11's Mod2Mask modifier key). If you want to
    // understand the specifics this this encoding encoding you can check the
    // code, but all you need to know is that there are special constants
    // defined bellow, for instance `_YN` encodes that this constraint is met
    // irrespective of the value of FLAG_APPLICATION_KEYPAD, but
    // FLAG_APPLICATION_CURSOR has to be set and numlock cannot be
    // activated.
    uint8_t c2;
    // The escape sequence to map the symbol to.
    char    *escape_sequence;
};

const static uint ANY_MOD    = UINT_MAX;
const static uint NO_MOD     = 0;
const static uint SWITCH_MOD = (1<<13|1<<14);  // TODO: What is this?
const static uint IGNORED_MODIFIERS = Mod2Mask|SWITCH_MOD;

#define N_SPECIAL_KEYS 209
static struct constraint_s special_keys_map[N_SPECIAL_KEYS];

// These are the functions that determine when the c1 resp. c2 constraints are
// met.

bool match_c1(struct constraint_s constraint, XKeyPressedEvent event) {
    if (constraint.c1 == ANY_MOD) {
        return true;
    }
    return constraint.c1 == (event.state & ~IGNORED_MODIFIERS);
}

bool match_c2(struct constraint_s constraint, XKeyPressedEvent event) {
    /*
      The c1 constraint is encoded in a byte where the 6 least significant bits
      are all that matters.

      |  appkey   | appcursor | numlock  |
      | Yes | No  | Yes | No  | Yes | No |
      |  32   16     8    4      2    1  |

      For instance, _YN => 32 | 16 | 4 | 2
      , NNN => 16 | 4 | 1
      , YYY => 32 | 8 | 2
      , ___ => 32 | 16 | 8 | 4 | 2 | 1
    */

    uint actual = (tb->flags & FLAG_APPLICATION_KEYPAD) ? 32 : 16
        | (tb->flags & FLAG_APPLICATION_CURSOR) ? 8 : 4
        | (event.state & Mod2Mask) ? 2 : 1;

    return (constraint.c2 & actual) == actual;
}

void handle_x11_keypress(XKeyPressedEvent event) {
    char buf[5];
    KeySym keysym = NoSymbol;
    Status status;

    int len = Xutf8LookupString(input_context,
                                &event,
                                buf,
                                4,
                                &keysym,
                                &status);

    // Nothing really happened, ignore.
    if (status == XLookupNone) {
        return;
    }

    // `buf` was too small, panic.
    if (status == XBufferOverflow) {
        assert(false);
    }

    // At this point status must be one of XLookupKeySym, XLookupChars or
    // XLookupBoth
    assert(status == XLookupKeySym
           || status == XLookupChars
           || status == XLookupBoth);

    // The key that was pressed corresponds to some letter.
    if (status == XLookupChars || status == XLookupBoth) {
        printf("\n\x1B[36m> Got key '");
        print_escape_non_printable(buf, len);
        printf("\x1B[36m' from x11.\x1B[0m\n");
        int did_write = write(primary_pty_fd, buf, len);
        if (did_write == -1) {
            assert(false);
        }
    }

    // The key that was pressed was a symbol. Time to iterate through our
    // symbol map to figure out what ANSI escape sequence to send.
    if (status == XLookupKeySym) {

        assert(keysym != NoSymbol);

        for (int i = 0; i < N_SPECIAL_KEYS; i++) {
            struct constraint_s constraint = special_keys_map[i];

            if (constraint.keysym != keysym) {
                continue;
            }

            if (!match_c1(constraint, event)) {
                continue;
            }

            if (!match_c2(constraint, event)) {
                continue;
            }

            printf("\n\x1B[36m> Transmitting special key sequence '");
            print_escape_non_printable(constraint.escape_sequence,
                                       strlen(constraint.escape_sequence));
            printf("\x1B[36m'\x1B[0m\n");
            int did_write = write(primary_pty_fd,
                                  constraint.escape_sequence,
                                  strlen(constraint.escape_sequence));
            if (did_write == -1) {
                assert(false);
            }

            return;
        }
    }
}

const static uint8_t NNN = 16 | 4 | 1;               // 0b010101
const static uint8_t NNY = 16 | 4 | 2;               // 0b010110
const static uint8_t NN_ = 16 | 4 | 2 | 1;           // 0b010111
const static uint8_t NYN = 16 | 8 | 1;               // 0b011001
const static uint8_t NYY = 16 | 8 | 2;               // 0b011010
const static uint8_t NY_ = 16 | 8 | 2 | 1;           // 0b011011
const static uint8_t N_N = 16 | 8 | 4 | 1;           // 0b011101
const static uint8_t N_Y = 16 | 8 | 4 | 2;           // 0b011110
const static uint8_t N__ = 16 | 8 | 4 | 2 | 1;       // 0b011111
const static uint8_t YNN = 32 | 4 | 1;               // 0b100101
const static uint8_t YNY = 32 | 4 | 2;               // 0b100110
const static uint8_t YN_ = 32 | 4 | 2 | 1;           // 0b100111
const static uint8_t YYN = 32 | 8 | 1;               // 0b101001
const static uint8_t YYY = 32 | 8 | 2;               // 0b101010
const static uint8_t YY_ = 32 | 8 | 2 | 1;           // 0b101011
const static uint8_t Y_N = 32 | 8 | 4 | 1;           // 0b101101
const static uint8_t Y_Y = 32 | 8 | 4 | 2;           // 0b101110
const static uint8_t Y__ = 32 | 8 | 4 | 2 | 1;       // 0b101111
const static uint8_t _NN = 32 | 16 | 4 | 1;          // 0b110101
const static uint8_t _NY = 32 | 16 | 4 | 2;          // 0b110110
const static uint8_t _N_ = 32 | 16 | 4 | 2 | 1;      // 0b110111
const static uint8_t _YN = 32 | 16 | 8 | 1;          // 0b111001
const static uint8_t _YY = 32 | 16 | 8 | 2;          // 0b111010
const static uint8_t _Y_ = 32 | 16 | 8 | 2 | 1;      // 0b111011
const static uint8_t __N = 32 | 16 | 8 | 4 | 1;      // 0b111101
const static uint8_t __Y = 32 | 16 | 8 | 4 | 2;      // 0b111110
const static uint8_t ___ = 32 | 16 | 8 | 4 | 2 | 1;  // 0b111111

static struct constraint_s special_keys_map[N_SPECIAL_KEYS] = {
    //                                       appkey appcursor numlock
    // keysym           c1                              c2
    { XK_KP_Home,       ShiftMask,                      _N_, "\033[2J"   },
    { XK_KP_Home,       ShiftMask,                      _Y_, "\033[1;2H" },
    { XK_KP_Home,       ANY_MOD,                        _N_, "\033[H"    },
    { XK_KP_Home,       ANY_MOD,                        _Y_, "\033[1~"   },
    { XK_KP_Up,         ANY_MOD,                        Y__, "\033Ox"    },
    { XK_KP_Up,         ANY_MOD,                        _N_, "\033[A"    },
    { XK_KP_Up,         ANY_MOD,                        _Y_, "\033OA"    },
    { XK_KP_Down,       ANY_MOD,                        Y__, "\033Or"    },
    { XK_KP_Down,       ANY_MOD,                        _N_, "\033[B"    },
    { XK_KP_Down,       ANY_MOD,                        _Y_, "\033OB"    },
    { XK_KP_Left,       ANY_MOD,                        Y__, "\033Ot"    },
    { XK_KP_Left,       ANY_MOD,                        _N_, "\033[D"    },
    { XK_KP_Left,       ANY_MOD,                        _Y_, "\033OD"    },
    { XK_KP_Right,      ANY_MOD,                        Y__, "\033Ov"    },
    { XK_KP_Right,      ANY_MOD,                        _N_, "\033[C"    },
    { XK_KP_Right,      ANY_MOD,                        _Y_, "\033OC"    },
    { XK_KP_Prior,      ShiftMask,                      ___, "\033[5;2~" },
    { XK_KP_Prior,      ANY_MOD,                        ___, "\033[5~"   },
    { XK_KP_Begin,      ANY_MOD,                        ___, "\033[E"    },
    { XK_KP_End,        ControlMask,                    N__, "\033[J"    },
    { XK_KP_End,        ControlMask,                    Y__, "\033[1;5F" },
    { XK_KP_End,        ShiftMask,                      N__, "\033[K"    },
    { XK_KP_End,        ShiftMask,                      Y__, "\033[1;2F" },
    { XK_KP_End,        ANY_MOD,                        ___, "\033[4~"   },
    { XK_KP_Next,       ShiftMask,                      ___, "\033[6;2~" },
    { XK_KP_Next,       ANY_MOD,                        ___, "\033[6~"   },
    { XK_KP_Insert,     ShiftMask,                      Y__, "\033[2;2~" },
    { XK_KP_Insert,     ShiftMask,                      N__, "\033[4l"   },
    { XK_KP_Insert,     ControlMask,                    N__, "\033[L"    },
    { XK_KP_Insert,     ControlMask,                    Y__, "\033[2;5~" },
    { XK_KP_Insert,     ANY_MOD,                        N__, "\033[4h"   },
    { XK_KP_Insert,     ANY_MOD,                        Y__, "\033[2~"   },
    { XK_KP_Delete,     ControlMask,                    N__, "\033[M"    },
    { XK_KP_Delete,     ControlMask,                    Y__, "\033[3;5~" },
    { XK_KP_Delete,     ShiftMask,                      N__, "\033[2K"   },
    { XK_KP_Delete,     ShiftMask,                      Y__, "\033[3;2~" },
    { XK_KP_Delete,     ANY_MOD,                        N__, "\033[P"    },
    { XK_KP_Delete,     ANY_MOD,                        Y__, "\033[3~"   },
    { XK_KP_Multiply,   ANY_MOD,                        Y_Y, "\033Oj"    },
    { XK_KP_Add,        ANY_MOD,                        Y_Y, "\033Ok"    },
    { XK_KP_Enter,      ANY_MOD,                        Y_Y, "\033OM"    },
    { XK_KP_Enter,      ANY_MOD,                        N__, "\r"        },
    { XK_KP_Subtract,   ANY_MOD,                        Y_Y, "\033Om"    },
    { XK_KP_Decimal,    ANY_MOD,                        Y_Y, "\033On"    },
    { XK_KP_Divide,     ANY_MOD,                        Y_Y, "\033Oo"    },
    { XK_KP_0,          ANY_MOD,                        Y_Y, "\033Op"    },
    { XK_KP_1,          ANY_MOD,                        Y_Y, "\033Oq"    },
    { XK_KP_2,          ANY_MOD,                        Y_Y, "\033Or"    },
    { XK_KP_3,          ANY_MOD,                        Y_Y, "\033Os"    },
    { XK_KP_4,          ANY_MOD,                        Y_Y, "\033Ot"    },
    { XK_KP_5,          ANY_MOD,                        Y_Y, "\033Ou"    },
    { XK_KP_6,          ANY_MOD,                        Y_Y, "\033Ov"    },
    { XK_KP_7,          ANY_MOD,                        Y_Y, "\033Ow"    },
    { XK_KP_8,          ANY_MOD,                        Y_Y, "\033Ox"    },
    { XK_KP_9,          ANY_MOD,                        Y_Y, "\033Oy"    },
    { XK_Up,            ShiftMask,                      ___, "\033[1;2A" },
    { XK_Up,            Mod1Mask,                       ___, "\033[1;3A" },
    { XK_Up,            ShiftMask|Mod1Mask,             ___, "\033[1;4A" },
    { XK_Up,            ControlMask,                    ___, "\033[1;5A" },
    { XK_Up,            ShiftMask|ControlMask,          ___, "\033[1;6A" },
    { XK_Up,            ControlMask|Mod1Mask,           ___, "\033[1;7A" },
    { XK_Up,            ShiftMask|ControlMask|Mod1Mask, ___, "\033[1;8A" },
    { XK_Up,            ANY_MOD,                        _N_, "\033[A"    },
    { XK_Up,            ANY_MOD,                        _Y_, "\033OA"    },
    { XK_Down,          ShiftMask,                      ___, "\033[1;2B" },
    { XK_Down,          Mod1Mask,                       ___, "\033[1;3B" },
    { XK_Down,          ShiftMask|Mod1Mask,             ___, "\033[1;4B" },
    { XK_Down,          ControlMask,                    ___, "\033[1;5B" },
    { XK_Down,          ShiftMask|ControlMask,          ___, "\033[1;6B" },
    { XK_Down,          ControlMask|Mod1Mask,           ___, "\033[1;7B" },
    { XK_Down,          ShiftMask|ControlMask|Mod1Mask, ___, "\033[1;8B" },
    { XK_Down,          ANY_MOD,                        _N_, "\033[B"    },
    { XK_Down,          ANY_MOD,                        _Y_, "\033OB"    },
    { XK_Left,          ShiftMask,                      ___, "\033[1;2D" },
    { XK_Left,          Mod1Mask,                       ___, "\033[1;3D" },
    { XK_Left,          ShiftMask|Mod1Mask,             ___, "\033[1;4D" },
    { XK_Left,          ControlMask,                    ___, "\033[1;5D" },
    { XK_Left,          ShiftMask|ControlMask,          ___, "\033[1;6D" },
    { XK_Left,          ControlMask|Mod1Mask,           ___, "\033[1;7D" },
    { XK_Left,          ShiftMask|ControlMask|Mod1Mask, ___, "\033[1;8D" },
    { XK_Left,          ANY_MOD,                        _N_, "\033[D"    },
    { XK_Left,          ANY_MOD,                        _Y_, "\033OD"    },
    { XK_Right,         ShiftMask,                      ___, "\033[1;2C" },
    { XK_Right,         Mod1Mask,                       ___, "\033[1;3C" },
    { XK_Right,         ShiftMask|Mod1Mask,             ___, "\033[1;4C" },
    { XK_Right,         ControlMask,                    ___, "\033[1;5C" },
    { XK_Right,         ShiftMask|ControlMask,          ___, "\033[1;6C" },
    { XK_Right,         ControlMask|Mod1Mask,           ___, "\033[1;7C" },
    { XK_Right,         ShiftMask|ControlMask|Mod1Mask, ___, "\033[1;8C" },
    { XK_Right,         ANY_MOD,                        _N_, "\033[C"    },
    { XK_Right,         ANY_MOD,                        _Y_, "\033OC"    },
    { XK_ISO_Left_Tab,  ShiftMask,                      ___, "\033[Z"    },
    { XK_Return,        Mod1Mask,                       ___, "\033\r"    },
    { XK_Return,        ANY_MOD,                        ___, "\r"        },
    { XK_Insert,        ShiftMask,                      N__, "\033[4l"   },
    { XK_Insert,        ShiftMask,                      Y__, "\033[2;2~" },
    { XK_Insert,        ControlMask,                    N__, "\033[L"    },
    { XK_Insert,        ControlMask,                    Y__, "\033[2;5~" },
    { XK_Insert,        ANY_MOD,                        N__, "\033[4h"   },
    { XK_Insert,        ANY_MOD,                        Y__, "\033[2~"   },
    { XK_Delete,        ControlMask,                    N__, "\033[M"    },
    { XK_Delete,        ControlMask,                    Y__, "\033[3;5~" },
    { XK_Delete,        ShiftMask,                      N__, "\033[2K"   },
    { XK_Delete,        ShiftMask,                      Y__, "\033[3;2~" },
    { XK_Delete,        ANY_MOD,                        N__, "\033[P"    },
    { XK_Delete,        ANY_MOD,                        Y__, "\033[3~"   },
    { XK_BackSpace,     NO_MOD,                         ___, "\177"      },
    { XK_BackSpace,     Mod1Mask,                       ___, "\033\177"  },
    { XK_Home,          ShiftMask,                      _N_, "\033[2J"   },
    { XK_Home,          ShiftMask,                      _Y_, "\033[1;2H" },
    { XK_Home,          ANY_MOD,                        _N_, "\033[H"    },
    { XK_Home,          ANY_MOD,                        _Y_, "\033[1~"   },
    { XK_End,           ControlMask,                    N__, "\033[J"    },
    { XK_End,           ControlMask,                    Y__, "\033[1;5F" },
    { XK_End,           ShiftMask,                      N__, "\033[K"    },
    { XK_End,           ShiftMask,                      Y__, "\033[1;2F" },
    { XK_End,           ANY_MOD,                        ___, "\033[4~"   },
    { XK_Prior,         ControlMask,                    ___, "\033[5;5~" },
    { XK_Prior,         ShiftMask,                      ___, "\033[5;2~" },
    { XK_Prior,         ANY_MOD,                        ___, "\033[5~"   },
    { XK_Next,          ControlMask,                    ___, "\033[6;5~" },
    { XK_Next,          ShiftMask,                      ___, "\033[6;2~" },
    { XK_Next,          ANY_MOD,                        ___, "\033[6~"   },
    { XK_F1,            NO_MOD,                         ___, "\033OP"    },
    { XK_F1, /* F13 */  ShiftMask,                      ___, "\033[1;2P" },
    { XK_F1, /* F25 */  ControlMask,                    ___, "\033[1;5P" },
    { XK_F1, /* F37 */  Mod4Mask,                       ___, "\033[1;6P" },
    { XK_F1, /* F49 */  Mod1Mask,                       ___, "\033[1;3P" },
    { XK_F1, /* F61 */  Mod3Mask,                       ___, "\033[1;4P" },
    { XK_F2,            NO_MOD,                         ___, "\033OQ"    },
    { XK_F2, /* F14 */  ShiftMask,                      ___, "\033[1;2Q" },
    { XK_F2, /* F26 */  ControlMask,                    ___, "\033[1;5Q" },
    { XK_F2, /* F38 */  Mod4Mask,                       ___, "\033[1;6Q" },
    { XK_F2, /* F50 */  Mod1Mask,                       ___, "\033[1;3Q" },
    { XK_F2, /* F62 */  Mod3Mask,                       ___, "\033[1;4Q" },
    { XK_F3,            NO_MOD,                         ___, "\033OR"    },
    { XK_F3, /* F15 */  ShiftMask,                      ___, "\033[1;2R" },
    { XK_F3, /* F27 */  ControlMask,                    ___, "\033[1;5R" },
    { XK_F3, /* F39 */  Mod4Mask,                       ___, "\033[1;6R" },
    { XK_F3, /* F51 */  Mod1Mask,                       ___, "\033[1;3R" },
    { XK_F3, /* F63 */  Mod3Mask,                       ___, "\033[1;4R" },
    { XK_F4,            NO_MOD,                         ___, "\033OS"    },
    { XK_F4, /* F16 */  ShiftMask,                      ___, "\033[1;2S" },
    { XK_F4, /* F28 */  ControlMask,                    ___, "\033[1;5S" },
    { XK_F4, /* F40 */  Mod4Mask,                       ___, "\033[1;6S" },
    { XK_F4, /* F52 */  Mod1Mask,                       ___, "\033[1;3S" },
    { XK_F5,            NO_MOD,                         ___, "\033[15~"  },
    { XK_F5, /* F17 */  ShiftMask,                      ___, "\033[15;2~"},
    { XK_F5, /* F29 */  ControlMask,                    ___, "\033[15;5~"},
    { XK_F5, /* F41 */  Mod4Mask,                       ___, "\033[15;6~"},
    { XK_F5, /* F53 */  Mod1Mask,                       ___, "\033[15;3~"},
    { XK_F6,            NO_MOD,                         ___, "\033[17~"  },
    { XK_F6, /* F18 */  ShiftMask,                      ___, "\033[17;2~"},
    { XK_F6, /* F30 */  ControlMask,                    ___, "\033[17;5~"},
    { XK_F6, /* F42 */  Mod4Mask,                       ___, "\033[17;6~"},
    { XK_F6, /* F54 */  Mod1Mask,                       ___, "\033[17;3~"},
    { XK_F7,            NO_MOD,                         ___, "\033[18~"  },
    { XK_F7, /* F19 */  ShiftMask,                      ___, "\033[18;2~"},
    { XK_F7, /* F31 */  ControlMask,                    ___, "\033[18;5~"},
    { XK_F7, /* F43 */  Mod4Mask,                       ___, "\033[18;6~"},
    { XK_F7, /* F55 */  Mod1Mask,                       ___, "\033[18;3~"},
    { XK_F8,            NO_MOD,                         ___, "\033[19~"  },
    { XK_F8, /* F20 */  ShiftMask,                      ___, "\033[19;2~"},
    { XK_F8, /* F32 */  ControlMask,                    ___, "\033[19;5~"},
    { XK_F8, /* F44 */  Mod4Mask,                       ___, "\033[19;6~"},
    { XK_F8, /* F56 */  Mod1Mask,                       ___, "\033[19;3~"},
    { XK_F9,            NO_MOD,                         ___, "\033[20~"  },
    { XK_F9, /* F21 */  ShiftMask,                      ___, "\033[20;2~"},
    { XK_F9, /* F33 */  ControlMask,                    ___, "\033[20;5~"},
    { XK_F9, /* F45 */  Mod4Mask,                       ___, "\033[20;6~"},
    { XK_F9, /* F57 */  Mod1Mask,                       ___, "\033[20;3~"},
    { XK_F10,           NO_MOD,                         ___, "\033[21~"  },
    { XK_F10, /* F22 */ ShiftMask,                      ___, "\033[21;2~"},
    { XK_F10, /* F34 */ ControlMask,                    ___, "\033[21;5~"},
    { XK_F10, /* F46 */ Mod4Mask,                       ___, "\033[21;6~"},
    { XK_F10, /* F58 */ Mod1Mask,                       ___, "\033[21;3~"},
    { XK_F11,           NO_MOD,                         ___, "\033[23~"  },
    { XK_F11, /* F23 */ ShiftMask,                      ___, "\033[23;2~"},
    { XK_F11, /* F35 */ ControlMask,                    ___, "\033[23;5~"},
    { XK_F11, /* F47 */ Mod4Mask,                       ___, "\033[23;6~"},
    { XK_F11, /* F59 */ Mod1Mask,                       ___, "\033[23;3~"},
    { XK_F12,           NO_MOD,                         ___, "\033[24~"  },
    { XK_F12, /* F24 */ ShiftMask,                      ___, "\033[24;2~"},
    { XK_F12, /* F36 */ ControlMask,                    ___, "\033[24;5~"},
    { XK_F12, /* F48 */ Mod4Mask,                       ___, "\033[24;6~"},
    { XK_F12, /* F60 */ Mod1Mask,                       ___, "\033[24;3~"},
    { XK_F13,           NO_MOD,                         ___, "\033[1;2P" },
    { XK_F14,           NO_MOD,                         ___, "\033[1;2Q" },
    { XK_F15,           NO_MOD,                         ___, "\033[1;2R" },
    { XK_F16,           NO_MOD,                         ___, "\033[1;2S" },
    { XK_F17,           NO_MOD,                         ___, "\033[15;2~"},
    { XK_F18,           NO_MOD,                         ___, "\033[17;2~"},
    { XK_F19,           NO_MOD,                         ___, "\033[18;2~"},
    { XK_F20,           NO_MOD,                         ___, "\033[19;2~"},
    { XK_F21,           NO_MOD,                         ___, "\033[20;2~"},
    { XK_F22,           NO_MOD,                         ___, "\033[21;2~"},
    { XK_F23,           NO_MOD,                         ___, "\033[23;2~"},
    { XK_F24,           NO_MOD,                         ___, "\033[24;2~"},
    { XK_F25,           NO_MOD,                         ___, "\033[1;5P" },
    { XK_F26,           NO_MOD,                         ___, "\033[1;5Q" },
    { XK_F27,           NO_MOD,                         ___, "\033[1;5R" },
    { XK_F28,           NO_MOD,                         ___, "\033[1;5S" },
    { XK_F29,           NO_MOD,                         ___, "\033[15;5~"},
    { XK_F30,           NO_MOD,                         ___, "\033[17;5~"},
    { XK_F31,           NO_MOD,                         ___, "\033[18;5~"},
    { XK_F32,           NO_MOD,                         ___, "\033[19;5~"},
    { XK_F34,           NO_MOD,                         ___, "\033[21;5~"},
    { XK_F33,           NO_MOD,                         ___, "\033[20;5~"},
    { XK_F35,           NO_MOD,                         ___, "\033[23;5~"},
};
