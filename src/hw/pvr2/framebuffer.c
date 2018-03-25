/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "error.h"
#include "hw/pvr2/spg.h"
#include "hw/pvr2/pvr2_core_reg.h"
#include "hw/pvr2/pvr2_tex_mem.h"
#include "hw/pvr2/pvr2_tex_cache.h"
#include "hw/pvr2/pvr2_gfx_obj.h"
#include "gfx/gfx.h"
#include "gfx/gfx_il.h"
#include "gfx/gfx_obj.h"
#include "log.h"

#include "framebuffer.h"

static DEF_ERROR_INT_ATTR(width)
static DEF_ERROR_INT_ATTR(height)

#define OGL_FB_W_MAX (0x3ff + 1)
#define OGL_FB_H_MAX (0x3ff + 1)
#define OGL_FB_BYTES (OGL_FB_W_MAX * OGL_FB_H_MAX * 4)
static uint8_t ogl_fb[OGL_FB_BYTES];
static unsigned fb_width, fb_height;
static unsigned stamp;

enum fb_pix_fmt {
    FB_PIX_FMT_RGB_555,
    FB_PIX_FMT_RGB_565,
    FB_PIX_FMT_RGB_888,
    FB_PIX_FMT_0RGB_0888
};

#define FB_HEAP_SIZE 8
struct framebuffer {
    int obj_handle;
    unsigned fb_width, fb_height;
    /* enum fb_pix_fmt fmt; */
    uint32_t addr_first/* , addr_last */;
    /* bool interlaced; */
    unsigned stamp;
    bool valid;
    bool vert_flip;
};

static struct framebuffer fb_heap[FB_HEAP_SIZE];

/*
 * this is a simple "dumb" memcpy function that doesn't handle the framebuffer
 * state (this is what makes it different from pvr2_tex_mem_area32_write).  It
 * does, however, perform bounds-checking and raise an error for out-of-bounds
 * memory access.
 */
static void copy_to_tex_mem32(void const *in, addr32_t offs, size_t len);

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
static void
conv_rgb0888_to_rgba8888(uint32_t *pixels_out,
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
conv_rgb0888_to_rgba8888(uint32_t *pixels_out,
                         uint32_t const *pixels_in,
                         unsigned n_pixels) {
    for (unsigned idx = 0; idx < n_pixels; idx++) {
        uint32_t pix = pixels_in[idx];
        uint32_t r = (pix & 0x00ff0000) >> 16;
        uint32_t g = (pix & 0x0000ff00) >> 8;
        uint32_t b = (pix & 0x000000ff);
        pixels_out[idx] = (255 << 24) | (b << 16) | (g << 8) | r;
    }
}

static int pick_fb(unsigned width, unsigned height, uint32_t addr);

void read_framebuffer_rgb565_prog(uint32_t *pixels_out, addr32_t start_addr,
                                  unsigned width, unsigned height, unsigned stride,
                                  uint16_t concat) {
    uint16_t const *pixels_in = (uint16_t*)(pvr2_tex32_mem + start_addr);
    /*
     * bounds checking
     *
     * TODO: is it really necessary to test for
     * (last_byte < ADDR_TEX32_FIRST || first_byte > ADDR_TEX32_LAST) ?
     */
    addr32_t last_byte = start_addr + ADDR_TEX32_FIRST + width * height * 2;
    addr32_t first_byte = start_addr + ADDR_TEX32_FIRST;
    if (last_byte > ADDR_TEX32_LAST || first_byte < ADDR_TEX32_FIRST ||
        last_byte < ADDR_TEX32_FIRST || first_byte > ADDR_TEX32_LAST) {
        error_set_feature("whatever happens when START_ADDR is configured to "
                          "read outside of texture memory");
        error_set_address(start_addr);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    unsigned row;
    for (row = 0; row < height; row++) {
        uint16_t const *in_col_start = pixels_in + stride * row;
        uint32_t *out_col_start = pixels_out + row * width;

        conv_rgb565_to_rgba8888(out_col_start, in_col_start, width, concat);
    }
}

/*
 * The way I handle interlace-scan here isn't terribly accurate.
 * instead of alternating between the two different fields on every frame
 * like a real TV would do, this function will read both fields every frame
 * and construct a full image.
 *
 * fb_width is expected to be the width of the framebuffer image *and* the
 * width of the texture in terms of pixels.
 * fb_height is expected to be the height of a single field in terms of pixels;
 * the full height of the framebuffer and also the height of the texture must
 * therefore be equal to fb_height*2.
 */
void read_framebuffer_rgb565_intl(uint32_t *pixels_out,
                                  unsigned fb_width, unsigned fb_height,
                                  uint32_t row_start_field1, uint32_t row_start_field2,
                                  unsigned modulus, unsigned concat) {
    /*
     * field_adv represents the distand between the start of one row and the
     * start of the next row in the same field in terms of bytes.
     */
    unsigned field_adv = (fb_width << 1) + (modulus << 2) - 4;

    /*
     * bounds checking.
     *
     * TODO: it is not impossible that the algebra for last_addr_field1 and
     * last_addr_field2 are a little off here, I'm *kinda* drunk.
     */
    addr32_t first_addr_field1 = ADDR_TEX32_FIRST + row_start_field1;
    addr32_t last_addr_field1 = ADDR_TEX32_FIRST + row_start_field1 +
        field_adv * (fb_height - 1) + 2 * (fb_width - 1);
    addr32_t first_addr_field2 = ADDR_TEX32_FIRST + row_start_field2;
    addr32_t last_addr_field2 = ADDR_TEX32_FIRST + row_start_field2 +
        field_adv * (fb_height - 1) + 2 * (fb_width - 1);
    if (first_addr_field1 < ADDR_TEX32_FIRST ||
        first_addr_field1 > ADDR_TEX32_LAST ||
        last_addr_field1 < ADDR_TEX32_FIRST ||
        last_addr_field1 > ADDR_TEX32_LAST ||
        first_addr_field2 < ADDR_TEX32_FIRST ||
        first_addr_field2 > ADDR_TEX32_LAST ||
        last_addr_field2 < ADDR_TEX32_FIRST ||
        last_addr_field2 > ADDR_TEX32_LAST) {
        error_set_feature("whatever happens when a framebuffer is configured "
                          "to read outside of texture memory");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    unsigned row;
    for (row = 0; row < fb_height; row++) {
        uint16_t const *ptr_row1 =
            (uint16_t const*)(pvr2_tex32_mem + row_start_field1);
        uint16_t const *ptr_row2 =
            (uint16_t const*)(pvr2_tex32_mem + row_start_field2);

        conv_rgb565_to_rgba8888(pixels_out + (row << 1) * fb_width,
                                ptr_row1, fb_width, concat);
        conv_rgb565_to_rgba8888(pixels_out + ((row << 1) + 1) * fb_width,
                                ptr_row2, fb_width, concat);

        row_start_field1 += field_adv;
        row_start_field2 += field_adv;
    }
}

void read_framebuffer_rgb0888_prog(uint32_t *pixels_out, addr32_t start_addr,
                                   unsigned width, unsigned height) {
    uint32_t const *pixels_in = (uint32_t const*)(pvr2_tex32_mem + start_addr);
    /*
     * bounds checking
     *
     * TODO: is it really necessary to test for
     * (last_byte < ADDR_TEX32_FIRST || first_byte > ADDR_TEX32_LAST) ?
     */
    addr32_t last_byte = start_addr + ADDR_TEX32_FIRST + width * height * 4;
    addr32_t first_byte = start_addr + ADDR_TEX32_FIRST;
    if (last_byte > ADDR_TEX32_LAST || first_byte < ADDR_TEX32_FIRST ||
        last_byte < ADDR_TEX32_FIRST || first_byte > ADDR_TEX32_LAST) {
        error_set_feature("whatever happens when START_ADDR is configured to "
                          "read outside of texture memory");
        error_set_address(start_addr);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    unsigned row;
    for (row = 0; row < height; row++) {
        uint32_t const *in_col_start = pixels_in + width * row;
        uint32_t *out_col_start = pixels_out + row * width;

        conv_rgb0888_to_rgba8888(out_col_start, in_col_start, width);
    }
}

void read_framebuffer_rgb0888_intl(uint32_t *pixels_out,
                                   unsigned fb_width, unsigned fb_height,
                                   uint32_t row_start_field1,
                                   uint32_t row_start_field2,
                                   unsigned modulus) {
    /*
     * field_adv represents the distand between the start of one row and the
     * start of the next row in the same field in terms of bytes.
     */
    unsigned field_adv = (fb_width << 2) + (modulus << 2) - 4;

    /*
     * bounds checking.
     *
     * TODO: it is not impossible that the algebra for last_addr_field1 and
     * last_addr_field2 are a little off here, I'm *kinda* drunk.
     */
    addr32_t first_addr_field1 = ADDR_TEX32_FIRST + row_start_field1;
    addr32_t last_addr_field1 = ADDR_TEX32_FIRST + row_start_field1 +
        field_adv * (fb_height - 1) + 4 * (fb_width - 1);
    addr32_t first_addr_field2 = ADDR_TEX32_FIRST + row_start_field2;
    addr32_t last_addr_field2 = ADDR_TEX32_FIRST + row_start_field2 +
        field_adv * (fb_height - 1) + 4 * (fb_width - 1);
    if (first_addr_field1 < ADDR_TEX32_FIRST ||
        first_addr_field1 > ADDR_TEX32_LAST ||
        last_addr_field1 < ADDR_TEX32_FIRST ||
        last_addr_field1 > ADDR_TEX32_LAST ||
        first_addr_field2 < ADDR_TEX32_FIRST ||
        first_addr_field2 > ADDR_TEX32_LAST ||
        last_addr_field2 < ADDR_TEX32_FIRST ||
        last_addr_field2 > ADDR_TEX32_LAST) {
        error_set_feature("whatever happens when a framebuffer is configured "
                          "to read outside of texture");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    unsigned row;
    for (row = 0; row < fb_height; row++) {
        uint32_t const *ptr_row1 =
            (uint32_t const*)(pvr2_tex32_mem + row_start_field1);
        uint32_t const *ptr_row2 =
            (uint32_t const*)(pvr2_tex32_mem + row_start_field2);

        conv_rgb0888_to_rgba8888(pixels_out + (row << 1) * fb_width,
                                 ptr_row1, fb_width);
        conv_rgb0888_to_rgba8888(pixels_out + ((row << 1) + 1) * fb_width,
                                 ptr_row2, fb_width);

        row_start_field1 += field_adv;
        row_start_field2 += field_adv;
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

void framebuffer_init(unsigned width, unsigned height) {
    struct gfx_il_inst cmd[2];

    /* fb_obj_handle = pvr2_alloc_gfx_obj(); */

    int fb_no;
    for (fb_no = 0; fb_no < FB_HEAP_SIZE; fb_no++) {
        fb_heap[fb_no].obj_handle = pvr2_alloc_gfx_obj();

        cmd[0].op = GFX_IL_INIT_OBJ;
        cmd[0].arg.init_obj.obj_no = fb_heap[fb_no].obj_handle;
        cmd[0].arg.init_obj.n_bytes = OGL_FB_W_MAX * OGL_FB_H_MAX *
            4 * sizeof(uint8_t);

        rend_exec_il(cmd, 1);
    }
    /* cmd[0].op = GFX_IL_INIT_OBJ; */
    /* cmd[0].arg.init_obj.obj_no = fb_obj_handle; */
    /* cmd[0].arg.init_obj.n_bytes = OGL_FB_W_MAX * OGL_FB_H_MAX * */
    /*     4 * sizeof(uint8_t); */

    /* cmd[1].op = GFX_IL_BIND_RENDER_TARGET; */
    /* cmd[1].arg.bind_render_target.gfx_obj_handle = fb_obj_handle; */

    /* rend_exec_il(cmd, 2); */

    /* fb_width = width; */
    /* fb_height = height; */
}

void framebuffer_render() {
    // update the texture
    bool interlace = get_spg_control() & (1 << 4);
    uint32_t fb_r_ctrl = get_fb_r_ctrl();
    uint32_t fb_r_size = get_fb_r_size();
    uint32_t fb_r_sof1 = get_fb_r_sof1() & ~3;
    uint32_t fb_r_sof2 = get_fb_r_sof2() & ~3;

    unsigned width = (fb_r_size & 0x3ff) + 1;
    unsigned height = ((fb_r_size >> 10) & 0x3ff) + 1;

    if (!(fb_r_ctrl & 1)) {
        LOG_DBG("framebuffer disabled\n");
        // framebuffer is not enabled.
        // TODO: display all-white or all black here instead of letting
        // the screen look corrupted?
        return;
    }

    struct gfx_il_inst cmd;

    int fb_idx;
    for (fb_idx = 0; fb_idx < FB_HEAP_SIZE; fb_idx++) {
        struct framebuffer *fb = fb_heap + fb_idx;
        if (fb->fb_width == width &&
            fb->fb_height == height &&
            fb->addr_first == fb_r_sof1) {
            goto submit_the_fb;
        }
    }
    fb_idx = pick_fb(width, height, fb_r_sof1);

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

    if (interlace)
        height <<= 1;

    if (fb_width != width || fb_height != height) {
        if (width > OGL_FB_W_MAX || height > OGL_FB_H_MAX) {
            LOG_ERROR("need to increase max framebuffer dims\n");
            error_set_width(width);
            error_set_height(height);
            RAISE_ERROR(ERROR_OVERFLOW);
        }

        fb_width = width;
        fb_height = height;
    }

    switch ((fb_r_ctrl & 0xc) >> 2) {
    case 0:
        // 16-bit 555 RGB
        error_set_feature("video mode RGB555");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
        break;
    case 1:
        // 16-bit 565 RGB
        if (interlace) {
            unsigned modulus = (fb_r_size >> 20) & 0x3ff;
            uint16_t concat = (fb_r_ctrl >> 4) & 7;
            read_framebuffer_rgb565_intl((uint32_t*)ogl_fb,
                                         fb_width, fb_height >> 1,
                                         fb_r_sof1, fb_r_sof2,
                                         modulus, concat);
        } else {
            read_framebuffer_rgb565_prog((uint32_t*)ogl_fb,
                                         fb_r_sof1,
                                         fb_width, fb_height, fb_width,
                                         (fb_r_ctrl >> 4) & 7);
        }
        break;
    case 2:
        // 24-bit 888 RGB
        error_set_feature("video mode RGB888");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
        break;
    case 3:
        // 32-bit 08888 RGB
        if (interlace) {
            unsigned modulus = (fb_r_size >> 20) & 0x3ff;
            read_framebuffer_rgb0888_intl((uint32_t*)ogl_fb,
                                          fb_width, fb_height >> 1,
                                          fb_r_sof1, fb_r_sof2, modulus);
        } else {
            read_framebuffer_rgb0888_prog((uint32_t*)ogl_fb, fb_r_sof1,
                                          fb_width, fb_height);
        }
        break;
    }

    LOG_DBG("passing framebuffer dimensions=(%u, %u)\n", fb_width, fb_height);
    LOG_DBG("interlacing is %s\n", interlace ? "enabled" : "disabled");

    fb_heap[fb_idx].valid = true;
    fb_heap[fb_idx].vert_flip = true;
    fb_heap[fb_idx].fb_width = width;
    fb_heap[fb_idx].fb_height = height;
    fb_heap[fb_idx].addr_first = fb_r_sof1;
    fb_heap[fb_idx].stamp = stamp++;

    cmd.op = GFX_IL_WRITE_OBJ;
    cmd.arg.write_obj.dat = ogl_fb;
    cmd.arg.write_obj.obj_no = fb_heap[fb_idx].obj_handle;
    cmd.arg.write_obj.n_bytes = OGL_FB_W_MAX * OGL_FB_H_MAX * 4;

    rend_exec_il(&cmd, 1);

submit_the_fb:
    cmd.op = GFX_IL_POST_FRAMEBUFFER;
    cmd.arg.post_framebuffer.obj_handle = fb_heap[fb_idx].obj_handle;
    cmd.arg.post_framebuffer.width = fb_heap[fb_idx].fb_width;
    cmd.arg.post_framebuffer.height = fb_heap[fb_idx].fb_height;
    cmd.arg.post_framebuffer.vert_flip = fb_heap[fb_idx].vert_flip;

    rend_exec_il(&cmd, 1);
}

__attribute__((unused))
static void framebuffer_sync_from_host_0555_krgb(void) {
    // TODO: this is almost certainly not the correct way to get the screen
    // dimensions as they are seen by PVR
    unsigned width = (get_fb_r_size() & 0x3ff) + 1;
    unsigned height = ((get_fb_r_size() >> 10) & 0x3ff) + 1;

    // we double width because width is in terms of 32-bits,
    // and this format uses 16-bit pixels
    width <<= 1;

    uint32_t fb_w_ctrl = get_fb_w_ctrl();
    uint16_t k_val = fb_w_ctrl & 0x8000;
    unsigned stride = get_fb_w_linestride() * 8;

    assert((width * height * 4) < OGL_FB_BYTES);
    /* assert(width <= stride); */

    unsigned row, col;
    for (row = 0; row < height; row++) {
        // TODO: take interlacing into account here
        unsigned line_offs = get_fb_w_sof1() + (height - (row + 1)) * stride;

        for (col = 0; col < width; col++) {
            unsigned ogl_fb_idx = row * width + col;

            uint16_t pix_out = ((ogl_fb[4 * ogl_fb_idx + 2] & 0xf8) >> 3) |
                ((ogl_fb[4 * ogl_fb_idx + 1] & 0xf8) << 2) |
                ((ogl_fb[4 * ogl_fb_idx] & 0xf8) << 7) | k_val;

            /*
             * XXX this is suboptimal because it does the bounds-checking once
             * per pixel.
             */
            copy_to_tex_mem32(&pix_out, line_offs + 2 * col, sizeof(pix_out));
        }
    }
}

__attribute__((unused))
static void framebuffer_sync_from_host_0565_krgb(void) {
    unsigned tile_w = get_glob_tile_clip_x() << 5;
    unsigned tile_h = get_glob_tile_clip_y() << 5;
    unsigned x_clip_min = get_fb_x_clip_min();
    unsigned x_clip_max = get_fb_x_clip_max();
    unsigned y_clip_min = get_fb_y_clip_min();
    unsigned y_clip_max = get_fb_y_clip_max();

    unsigned x_min = x_clip_min;
    unsigned y_min = y_clip_min;
    unsigned x_max = tile_w < x_clip_max ? tile_w : x_clip_max;
    unsigned y_max = tile_h < y_clip_max ? tile_h : y_clip_max;
    unsigned width = x_max - x_min + 1;
    unsigned height = y_max - y_min + 1;

    uint32_t fb_w_ctrl = get_fb_w_ctrl();
    uint16_t k_val = fb_w_ctrl & 0x8000;
    unsigned stride = get_fb_w_linestride() * 8;

    assert((width * height * 4) < OGL_FB_BYTES);
    /* assert(width <= stride); */

    unsigned row, col;
    for (row = y_min; row <= y_max; row++) {
        // TODO: take interlacing into account here
        unsigned line_offs = get_fb_w_sof1() + (height - (row + 1)) * stride;

        for (col = x_min; col <= x_max; col++) {
            unsigned ogl_fb_idx = row * width + col;

            uint16_t pix_out = ((ogl_fb[4 * ogl_fb_idx + 2] & 0xf8) >> 3) |
                ((ogl_fb[4 * ogl_fb_idx + 1] & 0xfc) << 3) |
                ((ogl_fb[4 * ogl_fb_idx] & 0xf8) << 8) | k_val;

            /*
             * XXX this is suboptimal because it does the bounds-checking once
             * per pixel.
             */
            copy_to_tex_mem32(&pix_out, line_offs + 2 * col, sizeof(pix_out));
        }
    }
}

/* static void framebuffer_sync_from_host(void) { */
/*     // update the framebuffer from the opengl target */

/*     uint32_t fb_w_ctrl = get_fb_w_ctrl(); */

/*     struct gfx_il_inst cmd = { */
/*         .op = GFX_IL_READ_OBJ, */
/*         .arg = { .read_obj = { */
/*             .dat = ogl_fb, */
/*             .obj_no = fb_obj_handle, */
/*             .n_bytes = /\* sizeof(ogl_fb) *\/OGL_FB_W_MAX * OGL_FB_H_MAX * 4 */
/*             } } */
/*     }; */
/*     rend_exec_il(&cmd, 1); */

/*     switch (fb_w_ctrl & 0x7) { */
/*     case 0: */
/*         // 0555 KRGB 16-bit */
/*         framebuffer_sync_from_host_0555_krgb(); */
/*         break; */
/*     case 1: */
/*         // 565 RGB 16-bit */
/*         framebuffer_sync_from_host_0565_krgb(); */
/*         break; */
/*     case 2: */
/*         // 4444 ARGB 16-bit */
/*         error_set_feature("framebuffer packmode 2"); */
/*         RAISE_ERROR(ERROR_UNIMPLEMENTED); */
/*         break; */
/*     case 3: */
/*         // 1555 ARGB 16-bit */
/*         error_set_feature("framebuffer packmode 3"); */
/*         RAISE_ERROR(ERROR_UNIMPLEMENTED); */
/*         break; */
/*     case 4: */
/*         // 888 RGB 24-bit */
/*         error_set_feature("framebuffer packmode 4"); */
/*         RAISE_ERROR(ERROR_UNIMPLEMENTED); */
/*         break; */
/*     case  5: */
/*         // 0888 KRGB 32-bit */
/*         error_set_feature("framebuffer packmode 5"); */
/*         RAISE_ERROR(ERROR_UNIMPLEMENTED); */
/*         break; */
/*     case 6: */
/*         // 8888 ARGB 32-bit */
/*         error_set_feature("framebuffer packmode 6"); */
/*         RAISE_ERROR(ERROR_UNIMPLEMENTED); */
/*         break; */
/*     default: */
/*         error_set_feature("unknown framebuffer packmode"); */
/*         RAISE_ERROR(ERROR_UNIMPLEMENTED); */
/*     } */

/*     current_fb = FRAMEBUFFER_CURRENT_VIRT; */
/* } */

/*
 * returns a pointer to the area that addr belongs in.
 * Keep in mind that this pointer points to the beginning of that area,
 * it doesn NOT point to the actual byte that corresponds to the addr.
 */
static uint8_t *get_tex_mem_area(addr32_t addr) {
    switch (addr & 0xff000000) {
    case 0x04000000:
    case 0x06000000:
        return pvr2_tex64_mem;
    case 0x05000000:
    case 0x07000000:
        return pvr2_tex32_mem;
    default:
        return NULL;
    }
}

static void copy_to_tex_mem32(void const *in, addr32_t offs, size_t len) {
    addr32_t last_byte = offs - 1 + len;

    if ((last_byte & 0xff000000) != (offs & 0xff000000)) {
        error_set_length(len);
        error_set_address(offs + ADDR_TEX32_FIRST);
        error_set_feature("texture memory writes across boundaries");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    uint8_t *tex_mem_ptr = get_tex_mem_area(offs + ADDR_TEX32_FIRST);
    if (!tex_mem_ptr) {
        error_set_length(len);
        error_set_address(offs + ADDR_TEX32_FIRST);
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

    /*
     * AND'ing offs with 0x7fffff here serves two purposes: it makes the offs
     * relative to whatever memor area tex_mem_ptr points at, and it also
     * implements mirroring across adjacent versions of the same area (the
     * memory areas are laid out as two mirrors of the 64-bit area at
     * 0x04000000 and 0x04800000, followed by two mirrors of the 32-bit area at
     * 0x05000000 and 0x05800000, and so forth up until 0x08000000)
     */
    offs &= 0x7fffff;
    memcpy(tex_mem_ptr + offs, in, len);

    /*
     * let the texture tracking system know we may have just overwritten a
     * texture in the cache.
     */
    if (tex_mem_ptr == pvr2_tex64_mem)
        pvr2_tex_cache_notify_write(offs + ADDR_TEX64_FIRST, len);
}

static int pick_fb(unsigned width, unsigned height, uint32_t addr) {
    int first_invalid = -1;
    int idx;
    int oldest_stamp = stamp;
    int oldest_stamp_idx = -1;
    for (idx = 0; idx < FB_HEAP_SIZE; idx++) {
        if (fb_heap[idx].valid) {
            if (fb_heap[idx].fb_width == width &&
                fb_heap[idx].fb_height == height &&
                fb_heap[idx].addr_first == addr) {
                break;
            }
            if (fb_heap[idx].stamp <= oldest_stamp) {
                oldest_stamp = fb_heap[idx].stamp;
                oldest_stamp_idx = idx;
            }
        } else if (first_invalid < 0) {
            first_invalid = idx;
        }
    }

    // If there are no unused framebuffers
    if (idx == FB_HEAP_SIZE) {
        if (first_invalid >= 0) {
            idx = first_invalid;
        } else {
            idx = oldest_stamp_idx;
        }

        /*
         * TODO: sync the framebuffer to memory here (since it's about to get
         * overwritten)
         */
    }

    return idx;
}

void framebuffer_set_render_target(void) {
    // TODO: this is almost certainly not the correct way to get the screen
    // dimensions as they are seen by PVR
    unsigned width = (get_fb_r_size() & 0x3ff) + 1;
    unsigned height = ((get_fb_r_size() >> 10) & 0x3ff) + 1;
    uint32_t addr = get_fb_w_sof1();

    int idx = pick_fb(width, height, addr);

    fb_heap[idx].valid = true;
    fb_heap[idx].vert_flip = false;
    fb_heap[idx].fb_width = width;
    fb_heap[idx].fb_height = height;
    fb_heap[idx].addr_first = addr;
    fb_heap[idx].stamp = stamp++;

    struct gfx_il_inst cmd;
    cmd.op = GFX_IL_BIND_RENDER_TARGET;
    cmd.arg.bind_render_target.gfx_obj_handle = fb_heap[idx].obj_handle;
    rend_exec_il(&cmd, 1);
}
