#ifndef INCLUDED_RENDERING_H
#define INCLUDED_RENDERING_H

#include <harfbuzz/hb.h>
#include <glad/glx.h>

#include "./termbuf.h"

void rendering_initialize(Display *display,
                          int window,
                          GLXContext context,
                          const char *ttf_path);
void rendering_calculate_sizes(int screen_height,
                               int screen_width,
                               int char_height,
                               int *nrows_ret,
                               int *ncols_ret);
void rendering_render_cell(int xoffset, int yoffset, int row, int col,
                           struct termbuf_char *c);
void rendering_render_rect(int srow, int scol, int nrows, int ncols,
                           struct termbuf_char *c, int stride);


#endif /* INCLUDED_RENDERING_H */
