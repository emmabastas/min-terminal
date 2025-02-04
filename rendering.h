#ifndef INCLUDED_FONT_H
#define INCLUDED_FONT_H

#include <harfbuzz/hb.h>

#include "./termbuf.h"

void rendering_initialize(Display *display, int window, GLXContext context);
void rendering_calculate_sizes(int screen_height,
                               int screen_width,
                               int char_height,
                               int *nrows_ret,
                               int *ncols_ret);
void rendering_render_cell(int xoffset, int yoffset, int row, int col,
                           struct termbuf_char *c);

#endif /* INCLUDED_FONT_H */
