#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <assert.h>

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "./font.h"
#include "./ringbuf.h"
#include "./termbuf.h"
#include "./util.h"

#define RINGBUF_CAPACITY 1024

char *SHELL =
    //"/bin/sh";
    //"/nix/store/717iy55ncqs0wmhdkwc5fg2vci5wbmq8-bash-5.2p32/bin/bash";
    "/nix/store/p67rlwmrpf6j3q66dlm0ajyid5f48njk-user-environment/bin/nu";

Display *display;
int window;
int screen;

GC gc;

XftDraw *draw;
XftFont *font = NULL;
struct termbuf tb;

XEvent event;

XIC input_context;

int primary_pty_fd;    // AKA the "master" end.
int secondary_pty_fd;  // AKA the "slave" end.

#if _POSIX_C_SOURCE < 200112L
#error "we don't have posix_openpt\n"
#endif

void run_all_tests();
void xevent();
int exec_shell(char *command, char **args);
void render();

void render() {
    const int cell_width = 18;
    const int cell_height = 22;

    XRenderColor fg;
    XftColor color_foreground;

    for (int row = 1; row <= tb.nrows; row ++) {
        for (int col = 1; col <= tb.ncols; col ++) {
            struct termbuf_char *c =
                tb.buf + (row - 1) * tb.ncols + col - 1;

            font_render(0, 0, row, col, c);
        }
    }
}

void event_loop() {
    XSelectInput(display, window, KeyPressMask|FocusChangeMask); // override prev

    render();

    while(True) {
        if (XPending(display) > 0) {
            xevent();
        }

        struct pollfd pfd = {
            .fd = primary_pty_fd,
            .events = POLLIN,
            .revents = POLLIN,
        };
        int ret = poll(&pfd, 1, 0);
        if (ret == -1) {  // means an error occured.
            assert(false);
        }
        if (ret > 0) {  // means we didn't timeout.
            uint8_t buf[64];
            size_t did_read = read(primary_pty_fd, buf, 63);
            if (did_read == 0) {  // the pty is closed!?
                assert(false);
            }
            termbuf_parse(&tb, buf, did_read);
            render();
        }
        usleep(100);
    }
}

void xevent() {
    XNextEvent(display, &event);

    if (event.type == FocusIn) {
        printf("\n\x1B[36m> FocusIn event\x1B[0m\n");
        return;
    }

    if (event.type == FocusOut) {
        printf("\n\x1B[36m> FocusOut event\x1B[0m\n");
        return;
    }

    if (event.type == KeyPress) {
        XKeyPressedEvent key_event = event.xkey;

        char buf[5];
        KeySym keysym = NoSymbol;
        Status status;

        int len = Xutf8LookupString(input_context,
                                    &key_event,
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
        if (status != XLookupKeySym
            && status != XLookupChars
            && status != XLookupBoth) {
            assert(false);
        }

        // We didn't write anything to `buf`, ignore.
        if (len == 0) {
            return;
        }

        printf("\n\x1B[36m> Got key '");
        print_escape_non_printable(buf, len);
        printf("' from x11.\x1B[0m\n");
        write(primary_pty_fd, buf, len);
        return;
    }

    // Got a message from a client who sent it with `XSendEvent`
    // https://tronche.com/gui/x/xlib/events/client-communication/client-message.html
    if (event.type == ClientMessage) {
        printf("\n\x1B[36m> ClientMessage event\x1B[0m\n");
        return;
    }

    // Window state changed
    // https://tronche.com/gui/x/xlib/events/window-state-change/configure.html
    if (event.type == ConfigureNotify) {
        printf("\n\x1B[36m> ConfigureNotify event\x1B[0m\n");
        return;
    }

    // https://tronche.com/gui/x/xlib/events/window-state-change/map.html
    // TODO: What does this indicate?
    if (event.type == MapNotify) {
        printf("\n\x1B[36m> MapNotify event\x1B[0m\n");
        return;
    }

    // https://tronche.com/gui/x/xlib/events/window-state-change/visibility.html
    // TODO: Keep track of when window is visible and not to avoid
    // unecessary graphics operations.
    if (event.type == VisibilityNotify) {
        printf("\n\x1B[36m> VisibilityNotify event\x1B[0m\n");
        return;
    }

    // https://tronche.com/gui/x/xlib/events/exposure/expose.html
    // TODO: What does this indicate?
    if (event.type == Expose) {
        printf("\n\x1B[36m> Expose event\x1B[0m\n");
        return;
    }

    // We have a new parent window.
    // https://tronche.com/gui/x/xlib/events/window-state-change/reparent.html
    // TODO: Should I do the resizing here?
    if (event.type == ReparentNotify) {
        printf("\n\x1B[36m> ReparentNotify\x1B[0m\n");
        return;
    }

    // We missed some event, error
    printf("Unhandeled XEvent %d %s\n",
           event.type,
           util_xevent_to_string(event.type));
    assert(false);
}

// From simpleterminal.
int exec_shell(char *cmd, char **args)
{
	char *prog, *arg;

	int errno = 0;

    //char *shell_name = getenv("SHELL");
    char *shell_name = SHELL;
    if (shell_name == NULL) {
        assert(false);
    }

    if (args) {
        prog = args[0];
        arg = NULL;
        //} else if (scroll) {
        //	prog = scroll;
        //	arg = utmp ? utmp : sh;
        //} else if (utmp) {
        //	prog = utmp;
        //	arg = NULL;
    } else {
        prog = shell_name;
        arg = NULL;
    }
    //DEFAULT(args, ((char *[]) {prog, arg, NULL}));

    //unsetenv("COLUMNS");
    //unsetenv("LINES");
    //unsetenv("TERMCAP");
    //setenv("LOGNAME", pw->pw_name, 1);
    //setenv("USER", pw->pw_name, 1);
    setenv("USER", "emma", 1);
    setenv("SHELL", shell_name, 1);
    //setenv("HOME", pw->pw_dir, 1);
    setenv("HOME", "/home/emma", 1);
    setenv("TERM", "st-256color", 1);

    signal(SIGCHLD, SIG_DFL);
    signal(SIGHUP,  SIG_DFL);
    signal(SIGINT,  SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGALRM, SIG_DFL);

    // if execvp fails we return -1, otherwise we never return.
    return execvp(prog, args);
}

int main(int argc, char **argv) {
    display = XOpenDisplay(NULL);
    if (!display) { assert(false); }

    screen = DefaultScreen(display);
    int root = DefaultRootWindow(display);

    const int SCREEN_WIDTH = 800;
    const int SCREEN_HEIGHT = 400;

    // If I want to control window placement and not let the WM decide I should
    // add CWOverrideRedirect flag to the set of flags when calling
    // XCreateWindow.
    // https://tronche.com/gui/x/xlib/window/attributes/override-redirect.html

    XSetWindowAttributes win_attributes;
    win_attributes.override_redirect = True;
    win_attributes.background_pixel = 0x505050;
    win_attributes.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask | StructureNotifyMask;
    window = XCreateWindow(
        display,             // display
        root,                // root
        0,                   // x
        0,                   // y
        SCREEN_WIDTH,        // width
        SCREEN_HEIGHT,       // height
        0,                   // border_width
        CopyFromParent,      // depth
        CopyFromParent,      // class
        visual_info->visual, // visual
        CWBackPixel | CWEventMask | CWColormap, // valuemask
        &win_attributes);    // attributes

    // Set window attributes

    // https://tronche.com/gui/x/xlib/ICC/client-to-window-manager/wm-normal-hints.html#XSizeHints
    XSizeHints *size_hints = XAllocSizeHints();
    const int BORDERPX = 2;
    size_hints->flags = 0;
    size_hints->flags = PSize | PResizeInc | PBaseSize | PMinSize | PWinGravity;
    size_hints->height = SCREEN_HEIGHT;
    size_hints->width = SCREEN_WIDTH;
    size_hints->height_inc = 10;
    size_hints->width_inc = 10;
    size_hints->base_height = 2 * BORDERPX;
    size_hints->base_width = 2 * BORDERPX;
    size_hints->min_height = SCREEN_HEIGHT + 2 * BORDERPX;
    size_hints->min_width = SCREEN_WIDTH + 2 * BORDERPX;
    size_hints->win_gravity = SouthEastGravity;

    // https://tronche.com/gui/x/xlib/ICC/client-to-window-manager/wm-hints.html#XWMHints
    XWMHints *wm_hints = XAllocWMHints();
    wm_hints->flags = InputHint | StateHint;
    wm_hints->input = True;
    wm_hints->initial_state = NormalState;

    // https://tronche.com/gui/x/xlib/ICC/client-to-window-manager/wm-class.html#XClassHint
    // TODO be moreposix compliant: https://tronche.com/gui/x/icccm/sec-4.html#WM_CLASS
    XClassHint *class_hints = XAllocClassHint();
    char *progpath = calloc(256, 1);
    strncpy(progpath, argv[0], 255);
    class_hints->res_name = basename(progpath);
    class_hints->res_class = "min-terminal";

    XSetWMProperties(display,  // display
                     window,  // window
                     NULL,  // window_name
                     NULL,  // icon_name
                     NULL,  // argv
                     0,  // argc
                     size_hints,    // normal_hints
                     wm_hints,      // wm_hints
                     class_hints);  // class_hints

    XFree(size_hints);
    XFree(wm_hints);
    XFree(class_hints);

    // Set some attributes on the window
    Atom a_net_wm_window_type = XInternAtom(display,
                                            "_NET_WM_WINDOW_TYPE",
                                            False);
    Atom a_net_wm_window_type_normal = XInternAtom(display,
                                                   "_NET_WM_WINDOW_TYPE_NORMAL",
                                                   False);

    XChangeProperty(display,
                    window,
                    a_net_wm_window_type,
                    XA_ATOM,
                    32,
                    PropModeReplace,
                    (unsigned char *)&a_net_wm_window_type_normal, 1);

    Atom a_net_wm_state = XInternAtom(display,
                                    "_NET_WM_STATE",
                                    False);
    Atom a_net_wm_state_above = XInternAtom(display,
                                          "_NET_WM_STATE_ABOVE",
                                          False);

    XChangeProperty(display,
                    window,
                    a_net_wm_state,
                    XA_ATOM,
                    32,
                    PropModeReplace,
                    (unsigned char *)&a_net_wm_state_above, 1);

    // Get the window onto the display
    XMapRaised(display, window);

    XSetInputFocus(display, window, RevertToParent, CurrentTime);

    // Make a draw (whatever that is?)
    draw = XftDrawCreate(
        display,
        window,
        DefaultVisual(display, screen),
        DefaultColormap(display, screen));

    // Specify the font
    font = XftFontOpenName(display, screen, "FiraCode Nerd Font");
    if (font == NULL) {
        assert(false);
    }

    gc = XCreateGC(display,
                      window,
                      0,
                      NULL);

    // Make an input context, used later to decode keypresses

    XIM input_method = XOpenIM(display, NULL, NULL, NULL);
    if (input_method == NULL) {
      fprintf(stderr, "XOpenIM failed: could not open input device");
      return 1;
    }

    input_context = XCreateIC(
      input_method,
      XNInputStyle,
      XIMPreeditNothing | XIMStatusNothing,
      XNClientWindow,
      window,
      XNFocusWindow,
      window,
      NULL);

    int nrows, ncols;

    font_initialize(display, window, gc);
    font_calculate_sizes(SCREEN_HEIGHT, SCREEN_WIDTH, 21, &nrows, &ncols);

    primary_pty_fd = posix_openpt(O_RDWR);
    if (primary_pty_fd == -1) {
        assert(false);
    }

    int ret = grantpt(primary_pty_fd);
    if (ret == -1) {
        assert(false);
    }

    ret = unlockpt(primary_pty_fd);
    if (ret == -1) {
        assert(false);
    }

    char primary_pty_name[100];
    ret = ptsname_r(primary_pty_fd, primary_pty_name, 100);
    if (ret == -1) {
        assert(false);
    }
    printf("The pty is in %s.\n", primary_pty_name);

    pid_t pid = fork();
    if (pid < 0) {
        assert(false);
    }
    if (pid == 0) {
        secondary_pty_fd = open(primary_pty_name, O_RDWR);
        if (secondary_pty_fd == -1) {
            assert(false);
        }

        // Create a new process group (which you want when launching a new
        // terminal?).
        setsid();
        dup2(secondary_pty_fd, 0);  // use secondary_pty_fd for STDIN
        dup2(secondary_pty_fd, 1);  // use secondary_pty_fd for STDOUT
        dup2(secondary_pty_fd, 2);  // use secondary_pty_fd for STDERR

        // TODO: "Use of ioctl() makes for nonportable programs. Use the POSIX
        //       interface described in termios(3) whenever possible."
        //       Can we get rid of ioctl?

        // "Make the given terminal the controling terminal of the calling
        // process"?
        ret = ioctl(secondary_pty_fd, TIOCSCTTY, NULL);
        if (ret == -1) {
            assert(false);
        }

        // Set the dimensions of the terminal
        struct winsize ws = {
            .ws_row = nrows,
            .ws_col = ncols,
            .ws_xpixel = 0,  // unused.
            .ws_ypixel = 0,  // unused.
        };
        ret = ioctl(secondary_pty_fd, TIOCSWINSZ, &ws);
        if (ret == -1) {
            assert(false);
        }

        // We can close the secondary_pty_fd handle now, the STDIN, STDOUT and
        // STDERR handles we just created are still valid.
        ret = close(secondary_pty_fd);
        if (ret == -1) {
            assert(false);
        }

        char *args[2];
        args[0] = SHELL;
        args[1] = NULL;
        ret = exec_shell(SHELL, args);
        if (ret == -1) {
            assert(false);
        }
        assert(false);
    }

    termbuf_initialize(nrows, ncols, primary_pty_fd, &tb);

    run_all_tests();

    event_loop();

    return 0;
}

void run_all_tests() {
    CuString *output = CuStringNew();
    CuSuite *suite = ringbuf_test_suite();
    CuSuiteRun(suite);
    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);
    printf("%s\n", output->buffer);
}
