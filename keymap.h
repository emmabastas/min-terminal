#ifndef INCLUDED_KEYMAP_H
#define INCLUDED_KEYMAP_H

#include <X11/Xlib.h>

#include "./termbuf.h"

void keymap_initialize(struct termbuf *tb,
                       XIC input_context,
                       int primary_pty_fd);
void keymap_handle_x11_keypress(XKeyPressedEvent event);

#endif /* INCLUDED_KEYMAP_H */
