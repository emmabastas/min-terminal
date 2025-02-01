#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include <X11/Xlib.h>
#include <harfbuzz/hb.h>

#include <glad/gl.h>
#include <glad/glx.h>

#include "./termbuf.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "./stb_truetype.h"

static const char *ttf_path =
    "/nix/store/wmdjq77kb88av295fcx600ff13v2vh7k-home-manager-path"
    "/share/fonts/truetype/NerdFonts/FiraCodeNerdFontMono-Regular.ttf";

static hb_blob_t   *blob;
static hb_face_t   *face;
static hb_font_t   *font;
static hb_buffer_t *buf;

struct stbtt_fontinfo font_info;
float font_scale;

static int nrows;
static int ncols;

static int cell_width;
static int cell_height;

static int ascent;
static int descent;
static int line_gap;

Display   *x_display;
int        x_window;

GLXContext gl_context;
GLuint     gl_glyphtexture;
GLuint     gl_vao;
GLuint     gl_vbo;
GLuint     shaderprogram;

void font_initialize(Display *display, int window, GLXContext context) {
    x_display = display;
    x_window = window;
    gl_context = context;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glGenTextures(1, &gl_glyphtexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_glyphtexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenVertexArrays(1, &gl_vao);
    glBindVertexArray(gl_vao);

    const GLfloat vertices[16] = {
        -1.0,   1.0, 0.0, 0.0,
        -1.0,  -1.0, 0.0, 1.0,
         1.0,   1.0, 1.0, 0.0,
         1.0,  -1.0, 1.0, 1.0,
    };

    glGenBuffers(1, &gl_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gl_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 16 * sizeof(GLfloat),
                 vertices,
                 GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    char *vertexsource = "#version 460 \n\
        layout (location = 0) in vec2 in_position; \n\
        layout (location = 1) in vec2 in_tex_coord; \n\
        \n\
        out vec3 fg_color; \n\
        out vec2 tex_coord; \n\
        \n\
        void main(void) { \n\
            gl_Position = vec4(in_position, 0.0, 1.0); \n\
            \n\
            fg_color = vec3(1.0, 0.0, 0.0); \n\
            tex_coord = in_tex_coord; \n\
        }\n";

    GLint vertexshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexshader, 1, (const GLchar**)&vertexsource, 0);
    glCompileShader(vertexshader);

    char *fragmentsource = "#version 460 \n\
        precision highp float; \n\
        precision highp sampler2D; \n\
        \n\
        in vec3 fg_color; \n\
        in vec2 tex_coord; \n\
        uniform sampler2D tex; \n\
        \n\
        layout(location = 0) out vec4 frag_color; \n\
        \n\
        void main(void) { \n\
            float intensity = texture(tex, tex_coord).r; \n\
            frag_color = vec4(vec3(intensity), 1.0); \n\
        }";

    GLint fragmentshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentshader, 1, (const GLchar**)&fragmentsource, 0);
    glCompileShader(fragmentshader);

    shaderprogram = glCreateProgram();
    glAttachShader(shaderprogram, vertexshader);
    glAttachShader(shaderprogram, fragmentshader);
    glLinkProgram(shaderprogram);
    glUseProgram(shaderprogram);

    glBindAttribLocation(shaderprogram, 0, "in_position");
    glUniform1i(glGetUniformLocation(shaderprogram, "tex"), 0);

    blob = hb_blob_create_from_file_or_fail(ttf_path);
    if (blob == NULL) {
        assert(false);
    }

    unsigned int blob_len;
    const unsigned char *blob_data =
        (const unsigned char *) hb_blob_get_data(blob, &blob_len);

    face = hb_face_create(blob, 0);
    font = hb_font_create(face);

    buf = hb_buffer_create();

    int n_fonts = stbtt_GetNumberOfFonts(blob_data);
    assert(n_fonts == 1);

    int font_index = 0;
    int offset_for_index = stbtt_GetFontOffsetForIndex(blob_data, font_index);
    if (offset_for_index == -1) {
        assert(false);
    }

    int ret = stbtt_InitFont(&font_info, blob_data, offset_for_index);
    if (ret == 0) {
        assert(false);
    }
}

void font_calculate_sizes(int screen_height,
                          int screen_width,
                          int char_height,
                          int *nrows_ret,
                          int *ncols_ret) {

    font_scale = stbtt_ScaleForPixelHeight(&font_info, char_height);

    // Calculate the font width-height ratio.
    // This is a little fucky, when I stbtt_GetFontBoundingBox I get:
    // x0: -3555, y0 2400, x1 1480, y1 0.
    // I think the -3555 is a result of the ligatures?
    // If so, then width: 1480 and height: 2400 giving is a ratio of ~61% which
    // seams resonable (60% is considered a good ratio for monospaced fonts).
    int x0, y0, x1, y1;
    stbtt_GetFontBoundingBox(&font_info, &x0, &y0, &x1, &y0);

    // But also stbtt_GetFontVMetrics gives us ascent, descent and line gap of
    // the font's which should converge with the y0 of stbtt_GetFontBoundingBox
    stbtt_GetFontVMetrics(&font_info, &ascent, &descent, &line_gap);
    assert(y0 == (int) ((ascent - descent + line_gap)));

    float ratio = (float) x1 / (float) y0;

    cell_height = char_height;
    cell_width = char_height * ratio;

    // Now that we know the cell size we can calculate how many rows and
    // collumns will fit in the window.
    *nrows_ret = (int) floor((float) screen_height / (float) cell_height);
    *ncols_ret = (int) floor((float) screen_width / (float) cell_width);

    printf("fs %f\n", font_scale);
    printf("descent %d\n", descent);
}

void font_render(int xoffset, int yoffset, int row, int col,
                 struct termbuf_char *c) {

    if ((c->flags & FLAG_LENGTH_MASK) == FLAG_LENGTH_0) {
        return;
    }

    // Hardcode the direction, script and language.
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string("en", -1));

    const short len = c->flags & FLAG_LENGTH_MASK;

    assert(len > 0);

    if (len == 1 && *c->utf8_char == (uint8_t) ' ') {
        return;
    }

    hb_buffer_add_utf8(buf,
                       (char *) c->utf8_char,
                       len,
                       0,
                       len);

    hb_shape (font, buf, NULL, 0);

    assert(hb_buffer_get_length(buf) == 1);

    // Get glyph information and positions out of the buffer.
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buf, NULL);
    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buf, NULL);
    hb_buffer_clear_contents(buf);

    int glyph_index = info->codepoint;
    //int glyph_index = stbtt_FindGlyphIndex(&font_info, 45);//info->codepoint);
    //if (glyph_index == 0) {
    //    assert(false);
    //}

    if (stbtt_IsGlyphEmpty(&font_info, glyph_index) != 0) {
        assert(false);
    }

    int bitmap_width, bitmap_height, bitmap_xoffset, bitmap_yoffset;

    unsigned char *bitmap = stbtt_GetGlyphBitmap(
        &font_info,
        font_scale,
        font_scale,
        glyph_index,
        &bitmap_width,
        &bitmap_height,
        &bitmap_xoffset,
        &bitmap_yoffset);

    if (bitmap == NULL) {
        assert(false);
    }

    glTexImage2D(GL_TEXTURE_2D,    // target
                 0,                // level
                 GL_RED,           // internal format
                 bitmap_width,     // width
                 bitmap_height,    // height
                 0,                // border
                 GL_RED,           // format
                 GL_UNSIGNED_BYTE, // type
                 bitmap);          // data

    stbtt_FreeBitmap(bitmap, NULL);

    glViewport((col - 1) * cell_width + bitmap_xoffset,
               400 - ((row - 1) * cell_height + bitmap_height + bitmap_yoffset + descent * font_scale),
               bitmap_width,
               bitmap_height);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
