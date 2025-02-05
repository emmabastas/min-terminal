/*
  min-terminal.c does three things:
  1) Has the `main` function, and parses CLI arguments.
  2) Sets up the X11 window, creates the OpenGL context, and sets up the
     terminal state.
  3) Handles the event loop (see `event_loop`), this is where user interaction
     is sent to the shell process, where instructions from the shell process
     to the terminal are parsed and rendered.

  Parts (1) and (2) are a bit dense but relatively uninteresting. You can read
  the documentation for `void event_loop()` in this file if you want an overview
  of how part (3) is done. What follows now is a little overview about terminals
  and shells, what they are and how the communicate.

  * THE TERMINAL AND THE SHELL

    min-terminal is a terminal (emulator), and a terminal i little more than an
    interface to another program, usually a shell (like sh, bash, zsh, etc). The
    terminal records inputs from the user in terms of button presses, mouse
    clicks and so on, and sends that to the shell. The shell in turn does
    something with what it receives and sends back instructions to the terminal
    telling it to display new text. In some sense the terminal is the body with
    it's sensory organs and muscles whereas the shell is the brain.

    The brain to the terminals body can be something other than a shell, for
    instance you can launch a terminal and tell it to have a program like vim be
    it's brain. However, in all of my source code I'm going to refer to the two
    parts as TERMINAL and SHELL because that makes sense to me.

  * HOW DOES THE TERMINAL AND THE SHELL COMMUNICATE?

    The mechanism by which a terminal and a shell communicate is via a
    PSEUDOTERMINAL (abbr. PTY). To understand this a little history might help;
    Back in the day a terminal was an actually physical device, with a keyboard
    and a screen (before screens a teletypewriter, TTY, was used) that was
    connected to a big computer somewhere via a cable. In Linux all such
    peripheral devices are interacted with as if they where files, and so back
    in the day the shell would sit on the computer and read from a special file
    in order to receive input from the terminal, it would then write to the same
    special file in order to send instructions back to the terminal.

    These days the terminal and the shell are processes on the same machine, but
    we still pretend in some sense that they are connected over a peripheral.

    In the code, the line `primary_pty_fd = posix_openpt(O_RDWR);` we ask the
    kernel to give us a PTY, a pair of device files. One is called the
    `primary_pty` and it's the device file that we -- the terminal process --
    use to send and receive data from the shell. The other device file is the
    `secondary_pty` and to the shell process this device file behaves just as if
    it was a device file for a physical terminal connected via a cable.

    We then call `pid_t pid = fork()` to create a child process, in this process
    we make the stdin, stdout and stderr all refer to the `secondary_pty`
    using `dup2(secondary_pty_fd, _);`, and when we've done that and some
    additional setup we use `xecvp(shell_command, args);` to launch the shell
    process and have it take over the child process. In the end it looks a
    little like this:

                                               /----------------\
         min-terminal     <-read & write->     | primary_pty_fd |
                                               \----------------/
                                                         ^
                        some sort of kernel glue -->     |
                                                         v
                                   /----------------------------------------\
  shell process  <-read & write->  | stdin,stdout,stderr = secondary_pty_fd |
                                   \----------------------------------------/

 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <poll.h>
#include <errno.h>
#include <assert.h>
#include <libgen.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

// We're using GLAD as our OpenGL loader, and header files we're using we're
// generated with the following link. The header files themselves are located in
// ./glad/
// https://gen.glad.sh/#generator=c&api=gl%3D4.6%2Cglx%3D1.4&profile=gl%3Dcompatibility%2Cgles1%3Dcommon&extensions=GL_ARB_debug_output%2CGL_KHR_debug%2CGLX_ARB_create_context&options=ALIAS%2CALIAS%2CLOADER
#include <glad/gl.h>
#include <glad/glx.h>

#include "./rendering.h"
#include "./ringbuf.h"
#include "./termbuf.h"
#include "./util.h"

#define RINGBUF_CAPACITY 1024

Display *display;
int window;
int screen;
GLXContext glx_context;

struct termbuf tb;

XEvent event;

XIC input_context;

int primary_pty_fd;    // Used by the terminal process.
int secondary_pty_fd;  // Used by the shell process.
pid_t shell_pid;       // The PID of the shell process.

#if _POSIX_C_SOURCE < 200112L
#error "we don't have posix_openpt\n"
#endif

void run_all_tests();
void xevent();
int exec_shell(char *command, char **args);
void render();
void gl_debug_msg_callback(GLenum source,
                           GLenum type,
                           GLuint id,
                           GLenum severity,
                           GLsizei length,
                           const GLchar *message,
                           const void *userParam);

void render() {
    const int cell_width = 18;
    const int cell_height = 22;

    for (int row = 1; row <= tb.nrows; row ++) {
        for (int col = 1; col <= tb.ncols; col ++) {
            struct termbuf_char *c =
                tb.buf + (row - 1) * tb.ncols + col - 1;

            rendering_render_cell(0, 0, row, col, c);
        }
    }

    // Use this instead if doing double buffering.
    // glXSwapBuffers(display, window);
    glFlush();
}

/*
  TODO: Write about the event loop.
 */
void event_loop() {
    XSelectInput(display, window, KeyPressMask|FocusChangeMask); // override prev

    render();

    while(true) {
        // Check if the shell process has terminated.
        int shell_status;
        int ret = waitpid(shell_pid, &shell_status, WNOHANG);
        if (ret == -1) {  // Some error occured.
            assert(false);
        }
        if (ret != 0) {  // 0 indicates that the shell process hasn't changed
                         // state, so ret != 0 means something happened
            // TODO: do somethings here.
            // There are macros to get information from `shell_status`, see
            // `man 2 wait`
            printf("Child process has terminated.\n");
            while(true) { usleep(1000); }
        }

        // Check if the shell has given us any output to parse/display, and if
        // so handle it.
        struct pollfd pfd = {
            .fd = primary_pty_fd,
            .events = POLLIN,
            .revents = POLLIN,
        };
        ret = poll(&pfd, 1, 0);
        if (ret == -1) {  // means an error occured.
            assert(false);
        }
        if (ret > 0) {  // means we didn't timeout.
            #define BUFSIZE 4096
            uint8_t buf[BUFSIZE];
            size_t did_read = read(primary_pty_fd, buf, BUFSIZE);
            if (did_read == 0) {  // the pty is closed!?
                assert(false);
            }

            if (did_read == BUFSIZE) {
                // TODO: Maybe we should output info/warning that the buffer
                //       wasn't big enough to read all available data and that
                //       maybe we should consider making the buffer larger?
                assert(false);
            }

            termbuf_parse(&tb, buf, did_read);
            render();
        }
        usleep(100);
    }
}


void *event_loop_x11_event(void *arguments) {
    assert(arguments == NULL);

    // NOTES ON THREAD SAFETY
    // * `fprintf` on POSIX fprintf appears to be thread safe in the sense that
    //    two threads in the same process can use fprintf in the same file
    //    without the outputs getting interleaved. However, if I do multiple
    //    printfps's then they could get interleaved, so that's something I
    //    should fix in this function.
    //    https://stackoverflow.com/a/64485222
    // *  `write` I use `write` once to write to `primary_pty_fd`, i.e. send
    //    data to the shell process. This can occur simultaneously as the main
    //    thread does a `read(primary_pty_fd, _, _);` I honestly don't konw if
    //    I'm in trouble if these happen at the same time..

 start_over:

    XNextEvent(display, &event);

    if (event.type == FocusIn) {
        printf("\n\x1B[36m> FocusIn event\x1B[0m\n");
        goto start_over;
    }

    if (event.type == FocusOut) {
        printf("\n\x1B[36m> FocusOut event\x1B[0m\n");
        goto start_over;
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
            goto start_over;
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
            goto start_over;
        }

        printf("\n\x1B[36m> Got key '");
        print_escape_non_printable(buf, len);
        printf("' from x11.\x1B[0m\n");
        int did_write = write(primary_pty_fd, buf, len);
        if (did_write == -1) {
            assert(false);
        }
        goto start_over;
    }

    if (event.type == KeyRelease) {
        printf("\n\x1B[36m> KeyRelease event\x1B[0m\n");
        goto start_over;
    }

    // Got a message from a client who sent it with `XSendEvent`
    // https://tronche.com/gui/x/xlib/events/client-communication/client-message.html
    if (event.type == ClientMessage) {
        printf("\n\x1B[36m> ClientMessage event\x1B[0m\n");
        goto start_over;
    }

    // Window state changed
    // https://tronche.com/gui/x/xlib/events/window-state-change/configure.html
    if (event.type == ConfigureNotify) {
        printf("\n\x1B[36m> ConfigureNotify event\x1B[0m\n");
        goto start_over;
    }

    // https://tronche.com/gui/x/xlib/events/window-state-change/map.html
    // TODO: What does this indicate?
    if (event.type == MapNotify) {
        printf("\n\x1B[36m> MapNotify event\x1B[0m\n");
        goto start_over;
    }

    // https://tronche.com/gui/x/xlib/events/window-state-change/visibility.html
    // TODO: Keep track of when window is visible and not to avoid
    // unecessary graphics operations.
    if (event.type == VisibilityNotify) {
        printf("\n\x1B[36m> VisibilityNotify event\x1B[0m\n");
        goto start_over;
    }

    // https://tronche.com/gui/x/xlib/events/exposure/expose.html
    // TODO: What does this indicate?
    if (event.type == Expose) {
        printf("\n\x1B[36m> Expose event\x1B[0m\n");
        goto start_over;
    }

    // We have a new parent window.
    // https://tronche.com/gui/x/xlib/events/window-state-change/reparent.html
    // TODO: Should I do the resizing here?
    if (event.type == ReparentNotify) {
        printf("\n\x1B[36m> ReparentNotify\x1B[0m\n");
        goto start_over;
    }

    // We missed some event, error
    printf("Unhandeled XEvent %d %s\n",
           event.type,
           util_xevent_to_string(event.type));
    assert(false);
}

int main(int argc, char **argv) {
    char *shell_command;

    if (argc == 1) {
        shell_command = secure_getenv("SHELL");

        // SHELL environment variable wasn't set (or "secure execution" is
        // required but I won't handle that case).
        if (shell_command == NULL) {
            printf("Environment variable SHELL wasn't set, either give it a ");
            printf("value or run `min-terminal [command]`\n");
            return -1;
        }
    }

    if (argc == 2) {
        shell_command = argv[1];
    }

    if (argc > 2) {
        // When I do more sophisticated argument parsing I should do this maybe?
        //printf("Usage: min-terminal [[-e] command [args ...]]\n");
        printf("Usage: min-terminal [command]\n");
        return -1;
    }

    display = XOpenDisplay(NULL);
    if (!display) { assert(false); }

    screen = DefaultScreen(display);
    int root = DefaultRootWindow(display);

    int glx_version = gladLoaderLoadGLX(display, screen);
    if (!glx_version) {
        printf("Unable to load GLX.\n");
        return 1;
    }
    printf("Loaded GLX %d.%d\n", GLAD_VERSION_MAJOR(glx_version), GLAD_VERSION_MINOR(glx_version));

    int visual_attribs[13] = {
        GLX_RENDER_TYPE,    GLX_RGBA_BIT,
        GLX_RED_SIZE,       8,
        GLX_GREEN_SIZE,     8,
        GLX_BLUE_SIZE,      8,
        GLX_DEPTH_SIZE,     24,
        GLX_DOUBLEBUFFER,   False,
        None,
    };

    int n_attribs;
    GLXFBConfig *fbconfig = glXChooseFBConfig(display,
                                              DefaultScreen(display),
                                              visual_attribs,
                                              &n_attribs);

    if (fbconfig == NULL) {  // config != NULL implies that n_attribs > 0.
        assert(false);
    }

    GLXFBConfig best_bfconfig = fbconfig[0];
    XFree(fbconfig);

    XVisualInfo *visual_info = glXGetVisualFromFBConfig(display, best_bfconfig);
    if (visual_info == NULL) {
        assert(false);
    }

    Colormap colormap = XCreateColormap(display,
                                        root,
                                        visual_info->visual,
                                        AllocNone);

    const int SCREEN_WIDTH = 800;
    const int SCREEN_HEIGHT = 400;

    // If I want to control window placement and not let the WM decide I should
    // add CWOverrideRedirect flag to the set of flags when calling
    // XCreateWindow.
    // https://tronche.com/gui/x/xlib/window/attributes/override-redirect.html

    XSetWindowAttributes win_attributes;
    win_attributes.override_redirect = True;
    win_attributes.background_pixel = 0x505050;
    win_attributes.colormap = colormap;
    win_attributes.event_mask =
        ExposureMask
        | KeyPressMask
        | VisibilityChangeMask
        | StructureNotifyMask;
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

    // See: https://stackoverflow.com/a/22256131 for why XSync before
    // XSetInputFocus.
    XSync(display, False);

    XSetInputFocus(display, window, RevertToParent, CurrentTime);

    int context_attribs[3] = {
        GLX_CONTEXT_FLAGS_ARB,
        //GLX_CONTEXT_DEBUG_BIT_ARB | GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB,
        GLX_CONTEXT_DEBUG_BIT_ARB,
        None,
    };

    // glx_context = glXCreateContext(display, visual_info, NULL, GL_TRUE);
    glx_context = glXCreateContextAttribsARB(display,
                                             best_bfconfig,
                                             NULL,
                                             GL_TRUE,
                                             context_attribs);
    if (glx_context == NULL) {
        assert(false);
    }

    Bool success = glXMakeCurrent(display, window, glx_context);
    if (success == False) {
        assert(false);
    }

    int gl_version = gladLoaderLoadGL();
    if (!gl_version) {
        printf("Unable to load GL.\n");
        return 1;
    }
    printf("Loaded GL %d.%d\n", GLAD_VERSION_MAJOR(gl_version), GLAD_VERSION_MINOR(gl_version));

    GLint flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT == 0) {
        assert(false);
    }

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(gl_debug_msg_callback, NULL);
    glDebugMessageControl(GL_DONT_CARE, // source
                          GL_DONT_CARE, // type
                          GL_DONT_CARE, // severity
                          0,            // count
                          NULL,         // ids
                          GL_TRUE);     // enabled


    int nrows, ncols;
    rendering_initialize(display, window, glx_context);
    rendering_calculate_sizes(SCREEN_HEIGHT, SCREEN_WIDTH, 21, &nrows, &ncols);

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
    if (pid == 0) {  // The child process that will becomme the shell process.
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
        args[0] = shell_command;
        args[1] = NULL;

        //unsetenv("COLUMNS");
        //unsetenv("LINES");
        //unsetenv("TERMCAP");
        //setenv("LOGNAME", pw->pw_name, 1);
        //setenv("USER", pw->pw_name, 1);
        //setenv("USER", "emma", 1);
        setenv("SHELL", shell_command, 1);
        //setenv("HOME", pw->pw_dir, 1);
        //setenv("HOME", "/home/emma", 1);
        setenv("TERM", "st-256color", 1);

        signal(SIGCHLD, SIG_DFL);
        signal(SIGHUP,  SIG_DFL);
        signal(SIGINT,  SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGALRM, SIG_DFL);

        // If execvp fails it returns -1,
        // if it succeeds then the new program takes over execution and this
        // branch effectively halts.
        int ret = execvp(shell_command, args);
        if (ret == -1) {
            printf("Error executing shell command `%s`, errno %d: %s.\n",
                   shell_command,
                   errno,
                   strerror(errno));
            return -1;
        }
        assert(false);
    }

    // We're in the parent process here
    shell_pid = pid;

    termbuf_initialize(nrows, ncols, primary_pty_fd, &tb);

    run_all_tests();

    pthread_t thread_x11_event;
    ret = pthread_create(&thread_x11_event,
                         NULL,
                         event_loop_x11_event,
                         NULL);
    if (ret != 0) {
        assert(false);
    }

    event_loop();

    assert(false);
}

// https://gist.github.com/liam-middlebrook/c52b069e4be2d87a6d2f
void gl_debug_msg_callback(GLenum source,
                           GLenum type,
                           GLuint id,
                           GLenum severity,
                           GLsizei length,
                           const GLchar *message,
                           const void *userParam) {
    printf("\x1b[31mGL error message:\x1B[m \"%s\"\n", message);
    assert(false);
}

void run_all_tests() {
    CuString *output = CuStringNew();
    CuSuite *suite = ringbuf_test_suite();
    CuSuiteRun(suite);
    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);
    printf("%s\n", output->buffer);
}
