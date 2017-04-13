/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#include <iostream>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "shader.hpp"
#include "hw/pvr2/spg.hpp"
#include "hw/pvr2/pvr2_core_reg.hpp"
#include "hw/pvr2/pvr2_tex_mem.hpp"

#include "framebuffer.hpp"

// vertex position (x, y, z)
static const unsigned SLOT_VERT_POS = 0;

// vertex texture coordinates (s, t)
static const unsigned SLOT_VERT_ST = 1;

/*
 * this shader represents the final stage of output, where a single textured
 * quad is drawn covering the entirety of the screen.
 */
static struct shader fb_shader;

static unsigned fb_width, fb_height;

// number of floats per vertex.
// that's 3 floats for the position and 2 for the texture coords
const static unsigned FB_VERT_LEN = 5;
const static unsigned FB_VERT_COUNT = 4;
static GLfloat fb_quad_verts[FB_VERT_LEN * FB_VERT_COUNT] = {
    /*
     * it is not a mistake that the texture-coordinates are upside-down
     * this is because dreamcast puts the origin at upper-left corner,
     * but opengl textures put the origin at the lower-left corner
     */

    // position            // texture coordinates
    -1.0f, -1.0f, 0.0f,    0.0f, 1.0f,
    -1.0f,  1.0f, 0.0f,    0.0f, 0.0f,
     1.0f,  1.0f, 0.0f,    1.0f, 0.0f,
     1.0f, -1.0f, 0.0f,    1.0f, 1.0f
};

const static unsigned FB_QUAD_IDX_COUNT = 4;
GLuint fb_quad_idx[FB_QUAD_IDX_COUNT] = {
    1, 0, 2, 3
};

/*
 * container for the poly's vertex array and its associated buffer objects.
 * this is created by fb_init_poly and never modified.
 *
 * The tex_obj, on the other hand, is modified frequently, as it is OpenGL's
 * view of our framebuffer.
 */
struct fb_poly {
    GLuint vbo; // vertex buffer object
    GLuint vao; // vertex array object
    GLuint ebo; // element buffer object

    GLuint tex_obj; // texture object
} fb_poly;

static void fb_init_poly();

/*
 * this is where we store the client-side version
 * of what becomes the opengl texture
 */
static uint8_t *fb_tex_mem;

/*
 * The concat parameter in these functions corresponds to the fb_concat value
 * in FB_R_CTRL; it is appended as the lower 3/2 bits to each color component
 * to convert that component from 5/6 bits to 8 bits.
 *
 * One "gotcha" to note about the below functions is that
 * conv_rgb555_to_argb8888 and conv_rgb565_to_rgb8888 expect their inputs to be
 * arrays of uint16_t with each element representing one pixel, and
 * conv_rgb0888_to_argb8888 expects its input to be uint32_t with each element
 * representing one pixel BUT conv_rgb888_to_argb8888 expects its input to be
 * uint8_t with every *three* elements representing one pixel.
 */
static void
conv_rgb555_to_argb8888(uint32_t *pixels_out,
                        uint16_t const *pixels_in,
                        unsigned n_pixels, uint8_t concat);
static void
conv_rgb565_to_rgba8888(uint32_t *pixels_out,
                        uint16_t const *pixels_in,
                        unsigned n_pixels, uint8_t concat);
__attribute__((unused))
static void
conv_rgb888_to_argb8888(uint32_t *pixels_out,
                        uint8_t const *pixels_in,
                        unsigned n_pixels);
__attribute__((unused))
static void
conv_rgb0888_to_argb8888(uint32_t *pixels_out,
                         uint32_t const *pixels_in,
                         unsigned n_pixels);

static void conv_rgb555_to_argb8888(uint32_t *pixels_out,
                                    uint16_t const *pixels_in,
                                    unsigned n_pixels,
                                    uint8_t concat) {
    for (unsigned idx = 0; idx < n_pixels; idx++) {
        uint16_t pix = pixels_in[idx];
        uint32_t r = ((pix & (0x1f << 10)) << 3) | concat;
        uint32_t g = ((pix & (0x1f << 5)) << 3) | concat;
        uint32_t b = ((pix & 0x1f) << 3) | concat;
        pixels_out[idx] = (255 // << 24
            ) | (r << 24) | (g << 16) | (b << 8);
    }
}

static void conv_rgb565_to_rgba8888(uint32_t *pixels_out,
                                    uint16_t const *pixels_in,
                                    unsigned n_pixels, uint8_t concat) {
    for (unsigned idx = 0; idx < n_pixels; idx++) {
        uint16_t pix = pixels_in[idx];
        uint32_t r = (((pix & 0xf800) >> 11) << 3) | concat;
        uint32_t g = (((pix & 0x07e0) >> 5) << 2) | (concat & 0x3);
        uint32_t b = ((pix & 0x001f) << 3) | concat;

        pixels_out[idx] = (255 << 24) | (b << 16) | (g << 8) | r;
    }
}

static void
conv_rgb888_to_argb8888(uint32_t *pixels_out,
                        uint8_t const *pixels_in,
                        unsigned n_pixels) {
    for (unsigned idx = 0; idx < n_pixels; idx++) {
        uint8_t const *pix = pixels_in + idx * 3;
        uint32_t r = pix[0];
        uint32_t g = pix[1];
        uint32_t b = pix[2];

        pixels_out[idx] = (255 << 24) | (r << 16) | (g << 8) | b;
    }
}

static void
conv_rgb0888_to_argb8888(uint32_t *pixels_out,
                         uint32_t const *pixels_in,
                         unsigned n_pixels) {
    for (unsigned idx = 0; idx < n_pixels; idx++) {
        uint32_t pix = pixels_in[idx];
        pixels_out[idx] = pix | (255 << 24);
    }
}

#if 0
void read_framebuffer_rgb565(uint32_t *pixels_out, void const *pixels_in,
                             unsigned width, unsigned height, unsigned mod,
                             uint16_t concat) {
    unsigned row;
    uint8_t const *cur_row_start = (uint8_t const*)pixels_in;

    for (row = 0; row < height; row++) {
        // uint16_t const *in_col_start = pixels_in + stride * row;
        uint32_t *out_col_start = pixels_out + row * width;

        conv_rgb565_to_rgba8888(out_col_start,
                                (uint16_t const*)cur_row_start,
                                width, concat);

        cur_row_start += width;
        cur_row_start += 4 * mod;
    }
}
#endif

void read_framebuffer_rgb565(uint32_t *pixels_out, uint16_t const *pixels_in,
                             unsigned width, unsigned height, unsigned stride,
                             uint16_t concat) {
    unsigned row;
    for (row = 0; row < height; row++) {
        uint16_t const *in_col_start = pixels_in + stride * row;
        uint32_t *out_col_start = pixels_out + row * width;

        conv_rgb565_to_rgba8888(out_col_start, in_col_start, width, concat);
    }
}

void read_framebuffer_rgb555(uint32_t *pixels_out, uint16_t const *pixels_in,
                             unsigned width, unsigned height, unsigned stride,
                             uint16_t concat) {
    unsigned row;
    for (row = 0; row < height; row++) {
        uint16_t const *in_col_start = pixels_in + stride * row;
        uint32_t *out_col_start = pixels_out + row * width;

        conv_rgb555_to_argb8888(out_col_start, in_col_start, width, concat);
    }
}

static void fb_init_poly() {
    GLuint vbo, vao, ebo, tex_obj;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 FB_VERT_LEN * FB_VERT_COUNT * sizeof(GLfloat),
                 fb_quad_verts, GL_STATIC_DRAW);
    glVertexAttribPointer(SLOT_VERT_POS, 3, GL_FLOAT, GL_FALSE,
                          FB_VERT_LEN * sizeof(GLfloat),
                          (GLvoid*)0);
    glEnableVertexAttribArray(SLOT_VERT_POS);
    glVertexAttribPointer(SLOT_VERT_ST, 2, GL_FLOAT, GL_FALSE,
                          FB_VERT_LEN * sizeof(GL_FLOAT),
                          (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(SLOT_VERT_ST);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, FB_QUAD_IDX_COUNT * sizeof(GLuint),
                 fb_quad_idx, GL_STATIC_DRAW);

    glBindVertexArray(0);

    // create texture object
    glGenTextures(1, &tex_obj);
    glBindTexture(GL_TEXTURE_2D, tex_obj);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    fb_poly.vbo = vbo;
    fb_poly.vao = vao;
    fb_poly.ebo = ebo;
    fb_poly.tex_obj = tex_obj;
}

void framebuffer_init(unsigned width, unsigned height) {
    fb_width = width;
    fb_height = height;

    fb_tex_mem = new uint8_t[fb_width * fb_height * 4];

    shader_init_from_file(&fb_shader, "final_vert.glsl", "final_frag.glsl");
    fb_init_poly();
}

void framebuffer_render() {
    // update the texture
    bool interlace = get_spg_control() & (1 << 4);
    uint32_t fb_r_ctrl = get_fb_r_ctrl();
    uint32_t fb_r_size = get_fb_r_size();
    uint32_t fb_r_sof1 = get_fb_r_sof1() & ~3;
    // uint32_t fb_r_sof2 = get_fb_r_sof2() & ~3;

    unsigned width = (fb_r_size & 0x3ff) + 1;
    unsigned height = ((fb_r_size >> 10) & 0x3ff) + 1;
    unsigned modulus = (fb_r_size >> 20) & 0x3ff;

    if (!(fb_r_ctrl & 1)) {
        // framebuffer is not enabled.
        // TODO: display all-white or all black here instead of letting
        // the screen look corrupted?
        return;
    }

    if (interlace)
        std::cout << "ITNERLACED DISPLAY" << std::endl;
    else
        std::cout << "NOT ITNERLACED DISPLAY" << std::endl;

    std::cout << "modulus is " << std::hex << modulus << std::endl;

    switch ((fb_r_ctrl & 0xc) >> 2) {
    case 0:
    case 1:
        // we double width because width is in terms of 32-bits,
        // and this format uses 16-bit pixels
        width <<= 1;
        break;
    default:
        break;
    }

    if (fb_width != width || fb_height != height) {
        delete[] fb_tex_mem;
        fb_width = width;
        fb_height = height;
        fb_tex_mem = new uint8_t[fb_width * fb_height * 4];
    }

    addr32_t first_byte, last_byte;
    switch ((fb_r_ctrl & 0xc) >> 2) {
    case 0:
        // 16-bit 555 RGB
        break;
    case 1:
        // 16-bit 565 RGB

        /*
         * bounds checking
         *
         * TODO: is it really necessary to test for
         * (last_byte < ADDR_TEX_FIRST || first_byte > ADDR_TEX_LAST) ?
         */
        last_byte = fb_r_sof1 + ADDR_TEX_FIRST + fb_width * fb_height * 2;
        first_byte = fb_r_sof1 + ADDR_TEX_FIRST;
        if (last_byte > ADDR_TEX_LAST || first_byte < ADDR_TEX_FIRST ||
            last_byte < ADDR_TEX_FIRST || first_byte > ADDR_TEX_LAST) {
            BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                  errinfo_feature("whatever happens when "
                                                  "FB_R_SOF1 is configured to "
                                                  "read outside of texture "
                                                  "memory") <<
                                  errinfo_guest_addr(fb_r_sof1));
        }

        read_framebuffer_rgb565((uint32_t*)fb_tex_mem,
                                (uint16_t*)(pvr2_tex_mem + fb_r_sof1),
                                fb_width, fb_height, fb_width,
                                (fb_r_ctrl >> 4) & 7);
        break;
    case 2:
        // 24-bit 888 RGB
        break;
    case 3:
        // 32-bit 08888 RGB
        break;
    }

    // if (interlace) {
    // } else {
    //     for (
    // }

    // for (unsigned row = 0; row < fb_height; row++)
    //     for (unsigned col = 0; col < fb_width; col++) {
    //         uint8_t *pix = fb_tex_mem + (row * fb_width + col) * 4;
    //         pix[0] = 0;
    //         pix[1] = 255;
    //         pix[2] = 0;
    //         pix[3] = 255;
    //     }

    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(fb_shader.shader_prog_obj);
    glBindTexture(GL_TEXTURE_2D, fb_poly.tex_obj);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fb_width, fb_height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, fb_tex_mem);
    glGenerateMipmap(GL_TEXTURE_2D);
    glUniform1i(glGetUniformLocation(fb_shader.shader_prog_obj, "fb_tex"), 0);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(fb_poly.vao);
    glDrawElements(GL_TRIANGLE_STRIP, FB_QUAD_IDX_COUNT, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    // glFlush();

    // glClearColor((float)0x17 / 255.0f,
    //              (float)0x56 / 255.0f,
    //              (float)0x9b / 255.0f, 1.0f);
    // glClear(GL_COLOR_BUFFER_BIT);
}
