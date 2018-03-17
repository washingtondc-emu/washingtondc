/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 ******************************************************************************/

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "digit_0.h"
#include "digit_1.h"
#include "digit_2.h"
#include "digit_3.h"
#include "digit_4.h"
#include "digit_5.h"
#include "digit_6.h"
#include "digit_7.h"
#include "digit_8.h"
#include "digit_9.h"
#include "dot.h"
#include "slash.h"
#include "space.h"
#include "gfx/opengl/opengl_output.h"

#include "font.h"

#define GLYPH_WIDTH 8
#define GLYPH_HEIGHT 16

#define TOTAL_WIDTH (13 * GLYPH_WIDTH)
#define TOTAL_HEIGHT (13 * GLYPH_HEIGHT)

#define TEX_WIDTH 128
#define TEX_HEIGHT 256

#define BYTES_PER_PIX 4

static_assert(GLYPH_WIDTH == DIGIT_0_WIDTH &&
              GLYPH_HEIGHT == DIGIT_0_HEIGHT &&
              GLYPH_WIDTH == DIGIT_1_WIDTH &&
              GLYPH_HEIGHT == DIGIT_1_HEIGHT &&
              GLYPH_WIDTH == DIGIT_2_WIDTH &&
              GLYPH_HEIGHT == DIGIT_2_HEIGHT &&
              GLYPH_WIDTH == DIGIT_3_WIDTH &&
              GLYPH_HEIGHT == DIGIT_3_HEIGHT &&
              GLYPH_WIDTH == DIGIT_4_WIDTH &&
              GLYPH_HEIGHT == DIGIT_4_HEIGHT &&
              GLYPH_WIDTH == DIGIT_5_WIDTH &&
              GLYPH_HEIGHT == DIGIT_5_HEIGHT &&
              GLYPH_WIDTH == DIGIT_6_WIDTH &&
              GLYPH_HEIGHT == DIGIT_6_HEIGHT &&
              GLYPH_WIDTH == DIGIT_7_WIDTH &&
              GLYPH_HEIGHT == DIGIT_7_HEIGHT &&
              GLYPH_WIDTH == DIGIT_8_WIDTH &&
              GLYPH_HEIGHT == DIGIT_8_HEIGHT &&
              GLYPH_WIDTH == DIGIT_9_WIDTH &&
              GLYPH_HEIGHT == DIGIT_9_HEIGHT &&
              GLYPH_WIDTH == DOT_WIDTH &&
              GLYPH_HEIGHT == DOT_HEIGHT &&
              GLYPH_WIDTH == SPACE_WIDTH &&
              GLYPH_HEIGHT == SPACE_HEIGHT &&
              GLYPH_WIDTH == SLASH_WIDTH &&
              GLYPH_HEIGHT == SLASH_HEIGHT,
              "invalid glyph dimensions");

static_assert(TOTAL_WIDTH <= TEX_WIDTH && TOTAL_HEIGHT <= TEX_HEIGHT,
              "need to make tex bigger");

static GLuint tex_obj;
static GLuint vbo, vao, ebo;

// number of floats per vertex.
// that's 3 floats for the position and 2 for the texture coords
#define VERT_LEN 5
#define VERT_COUNT 4
static GLfloat quad_verts[VERT_LEN * VERT_COUNT] = {
    /*
     * it is not a mistake that the texture-coordinates are upside-down
     * this is because dreamcast puts the origin at upper-left corner,
     * but opengl textures put the origin at the lower-left corner
     */

    // position            // texture coordinates
    0.0f, 0.0f, 0.0f,    0.0f, 1.0f,
    0.0f, 1.0f, 0.0f,    0.0f, 0.0f,
    1.0f, 1.0f, 0.0f,    1.0f, 0.0f,
    1.0f, 0.0f, 0.0f,    1.0f, 1.0f
};

#define QUAD_IDX_COUNT 4
GLuint quad_idx[QUAD_IDX_COUNT] = {
    1, 0, 2, 3
};

static void create_tex(void);
static void free_tex(void);
static void create_poly(void);
static void free_poly(void);
static void do_render_ch(char ch, GLfloat pos_x, GLfloat pos_y,
                         GLfloat width, GLfloat height);
static int get_char_idx(char ch);

void font_init(void) {
    create_tex();
    create_poly();
}

void font_cleanup(void) {
    free_poly();
    free_tex();
}

static void add_digit(uint8_t *tex, char ch, unsigned char *dat) {
    int digit = get_char_idx(ch);
    unsigned row, col;
    for (row = 0; row < GLYPH_HEIGHT; row++) {
        unsigned row_start = row * TEX_WIDTH + digit * GLYPH_WIDTH;
        for (col = 0; col < GLYPH_WIDTH; col++) {
            bool set = dat[row] & (1 << col);
            if (set) {
                tex[(row_start + col) * BYTES_PER_PIX] = 0;
                tex[(row_start + col) * BYTES_PER_PIX + 1] = 0;
                tex[(row_start + col) * BYTES_PER_PIX + 2] = 0;
                tex[(row_start + col) * BYTES_PER_PIX + 3] = 255;
            } else {
                tex[(row_start + col) * BYTES_PER_PIX] = 255;
                tex[(row_start + col) * BYTES_PER_PIX + 1] = 255;
                tex[(row_start + col) * BYTES_PER_PIX + 2] = 255;
                tex[(row_start + col) * BYTES_PER_PIX + 3] = 0;
            }
        }
    }
}

static void create_tex(void) {
    static uint8_t tex_dat[BYTES_PER_PIX * TEX_WIDTH * TEX_HEIGHT];

    add_digit(tex_dat, '0', digit_0_bits);
    add_digit(tex_dat, '1', digit_1_bits);
    add_digit(tex_dat, '2', digit_2_bits);
    add_digit(tex_dat, '3', digit_3_bits);
    add_digit(tex_dat, '4', digit_4_bits);
    add_digit(tex_dat, '5', digit_5_bits);
    add_digit(tex_dat, '6', digit_6_bits);
    add_digit(tex_dat, '7', digit_7_bits);
    add_digit(tex_dat, '8', digit_8_bits);
    add_digit(tex_dat, '9', digit_9_bits);
    add_digit(tex_dat, '.', dot_bits);
    add_digit(tex_dat, '/', slash_bits);
    add_digit(tex_dat, ' ', space_bits);

    glGenTextures(1, &tex_obj);

    glBindTexture(GL_TEXTURE_2D, tex_obj);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_WIDTH, TEX_HEIGHT, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, tex_dat);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
}

static void free_tex(void) {
    glDeleteTextures(1, &tex_obj);
}

static void create_poly(void) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 VERT_LEN * VERT_COUNT * sizeof(GLfloat),
                 quad_verts, GL_STATIC_DRAW);

    glVertexAttribPointer(OUTPUT_SLOT_VERT_POS, 3, GL_FLOAT, GL_FALSE,
                          VERT_LEN * sizeof(GLfloat),
                          (GLvoid*)0);
    glEnableVertexAttribArray(OUTPUT_SLOT_VERT_POS);
    glVertexAttribPointer(OUTPUT_SLOT_VERT_ST, 2, GL_FLOAT, GL_FALSE,
                          VERT_LEN * sizeof(GLfloat),
                          (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(OUTPUT_SLOT_VERT_ST);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, QUAD_IDX_COUNT * sizeof(GLuint),
                 quad_idx, GL_STATIC_DRAW);

    glBindVertexArray(0);
}

static void free_poly(void) {
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

void font_render(char const *txt, unsigned col, unsigned row,
                 float screen_w, float screen_h) {
    while (*txt)
        font_render_char(*txt++, col++, row, screen_w, screen_h);
}

void font_render_char(char ch, unsigned col, unsigned row,
                      float screen_w, float screen_h) {
    float glyph_clip_width = 2.0f * GLYPH_WIDTH / screen_w;
    float glyph_clip_height = 2.0f * GLYPH_HEIGHT / screen_h;

    unsigned n_rows = screen_h / GLYPH_HEIGHT;

    float pos_y = (n_rows - row - 1) * glyph_clip_height - 1.0f;
    float pos_x = col * glyph_clip_width - 1.0f;

    do_render_ch(ch, pos_x, pos_y, glyph_clip_width, glyph_clip_height);
}

static void do_render_ch(char ch, GLfloat pos_x, GLfloat pos_y,
                         GLfloat width, GLfloat height) {
    int digit = get_char_idx(ch);
    if (digit < 0)
        return;
    GLfloat uv_width = (GLfloat)GLYPH_WIDTH / (GLfloat)TEX_WIDTH;
    GLfloat uv_height = (GLfloat)GLYPH_HEIGHT / (GLfloat)TEX_HEIGHT;
    GLfloat u_tex = (GLfloat)(digit * GLYPH_WIDTH) / (GLfloat)TEX_WIDTH;
    GLfloat v_tex = 0.0f;

    GLfloat mview_mat[16] = {
        width, 0.0f, 0.0f, pos_x,
        0.0f, height, 0.0f, pos_y,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    GLfloat tex_mat[9] = {
        uv_width, 0.0f, u_tex,
        0.0f, uv_height, v_tex,
        0.0f, 0.0f, 1.0f
    };

    glUniformMatrix4fv(OUTPUT_SLOT_TRANS_MAT, 1, GL_TRUE, mview_mat);
    glUniformMatrix3fv(OUTPUT_SLOT_TEX_MAT, 1, GL_TRUE, tex_mat);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_obj);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLE_STRIP, QUAD_IDX_COUNT, GL_UNSIGNED_INT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static int get_char_idx(char ch) {
    switch (ch) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return ch - '0';
    case '.':
        return 10;
    case ' ':
        return 11;
    case '/':
        return 12;
    default:
        return -1;
    }
}
