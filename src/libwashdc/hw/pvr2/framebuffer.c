/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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

#include "washdc/types.h"
#include "washdc/error.h"
#include "hw/pvr2/pvr2.h"
#include "hw/pvr2/spg.h"
#include "hw/pvr2/pvr2_reg.h"
#include "hw/pvr2/pvr2_tex_mem.h"
#include "hw/pvr2/pvr2_tex_cache.h"
#include "hw/pvr2/pvr2_gfx_obj.h"
#include "gfx/gfx.h"
#include "gfx/gfx_il.h"
#include "gfx/gfx_obj.h"
#include "log.h"
#include "title.h"

#include "framebuffer.h"

static DEF_ERROR_INT_ATTR(width)
static DEF_ERROR_INT_ATTR(height)
static DEF_ERROR_INT_ATTR(fb_pix_fmt)

enum fb_state {
    FB_STATE_INVALID = 0,
    FB_STATE_VIRT = 1,
    FB_STATE_GFX = 2,
    FB_STATE_VIRT_AND_GFX = 3
};

enum fb_pix_fmt {
    FB_PIX_FMT_RGB_555,
    FB_PIX_FMT_RGB_565,
    FB_PIX_FMT_RGB_888,
    FB_PIX_FMT_0RGB_0888,
    FB_PIX_FMT_ARGB_8888,
    FB_PIX_FMT_ARGB_1555
};

static unsigned bytes_per_pix(uint32_t fb_r_ctrl) {
    unsigned px_tp = (fb_r_ctrl & 0xc) >> 2;

    switch (px_tp) {
    case 0:
    case 1:
        return 2;
    case 2:
        return 3;
    case 3:
        return 4;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

static uint8_t *get_tex_mem_area(struct pvr2 *pvr2, addr32_t addr);

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
conv_rgb565_to_rgba8888(uint32_t *pixels_out,
                        uint16_t const *pixels_in,
                        unsigned n_pixels, uint8_t concat);
static void
conv_rgb555_to_rgba8888(uint32_t *pixels_out,
                        uint16_t const *pixels_in,
                        unsigned n_pixels, uint8_t concat);
static void
conv_rgb888_to_rgba8888(uint32_t *pixels_out,
                        uint8_t const *pixels_in,
                        unsigned n_pixels);
static void
conv_rgb0888_to_rgba8888(uint32_t *pixels_out,
                         uint32_t const *pixels_in,
                         unsigned n_pixels);

static void
sync_fb_from_tex_mem_rgb565_intl(struct pvr2 *pvr2, struct framebuffer *fb,
                                 unsigned fb_width, unsigned fb_height,
                                 uint32_t sof1, uint32_t sof2,
                                 unsigned modulus, unsigned concat) {
    /*
     * field_adv represents the distand between the start of one row and the
     * start of the next row in the same field in terms of bytes.
     */
    unsigned field_adv = fb_width * 2 + modulus * 4 - 4;
    unsigned rows_per_field = fb_height;

    addr32_t first_addr_field1 = sof1;
    addr32_t last_addr_field1 = sof1 +
        field_adv * (rows_per_field - 1) + 2 * (fb_width - 1);
    addr32_t first_addr_field2 = sof2;
    addr32_t last_addr_field2 = sof2 +
        field_adv * (rows_per_field - 1) + 2 * (fb_width - 1);

    // bounds checking.
    addr32_t bounds_field1[2] = {
        first_addr_field1 + ADDR_TEX32_FIRST,
        last_addr_field1 + ADDR_TEX32_FIRST
    };
    addr32_t bounds_field2[2] = {
        first_addr_field2 + ADDR_TEX32_FIRST,
        last_addr_field2 + ADDR_TEX32_FIRST
    };
    if (bounds_field1[0] < ADDR_TEX32_FIRST ||
        bounds_field1[0] > ADDR_TEX32_LAST ||
        bounds_field1[1] < ADDR_TEX32_FIRST ||
        bounds_field1[1] > ADDR_TEX32_LAST ||
        bounds_field2[0] < ADDR_TEX32_FIRST ||
        bounds_field2[0] > ADDR_TEX32_LAST ||
        bounds_field2[1] < ADDR_TEX32_FIRST ||
        bounds_field2[1] > ADDR_TEX32_LAST) {
        error_set_feature("whatever happens when a framebuffer is configured "
                          "to read outside of texture memory");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    uint32_t *dst_fb = (uint32_t*)pvr2->fb.ogl_fb;

    unsigned row;
    uint8_t const *pvr2_tex32_mem = pvr2->mem.tex32;
    for (row = 0; row < rows_per_field; row++) {
        uint16_t const *ptr_row1 =
            (uint16_t const*)(pvr2_tex32_mem + sof1 + row * field_adv);
        uint16_t const *ptr_row2 =
            (uint16_t const*)(pvr2_tex32_mem + sof2 + row * field_adv);

        conv_rgb565_to_rgba8888(dst_fb + row * 2 * fb_width,
                                ptr_row1, fb_width, concat);
        conv_rgb565_to_rgba8888(dst_fb + (row * 2 + 1) * fb_width,
                                ptr_row2, fb_width, concat);
    }

    fb->addr_key = first_addr_field1  < first_addr_field2 ?
        first_addr_field1 : first_addr_field2;

    fb->addr_first[0] = first_addr_field1;
    fb->addr_first[1] = first_addr_field2;
    fb->addr_last[0] = last_addr_field1;
    fb->addr_last[1] = last_addr_field2;

    fb->fb_read_width = fb_width;
    fb->fb_read_height = fb_height;

    fb->flags.state = FB_STATE_VIRT_AND_GFX;

    fb->flags.vert_flip = true;
    fb->stamp = pvr2->fb.stamp;
    fb->flags.fmt = FB_PIX_FMT_RGB_565;

    struct gfx_il_inst cmd;

    cmd.op = GFX_IL_WRITE_OBJ;
    cmd.arg.write_obj.dat = dst_fb;
    cmd.arg.write_obj.obj_no = fb->obj_handle;
    cmd.arg.write_obj.n_bytes = OGL_FB_W_MAX * OGL_FB_H_MAX * 4;

    rend_exec_il(&cmd, 1);
}

static void
sync_fb_from_tex_mem_rgb565_prog(struct pvr2 *pvr2, struct framebuffer *fb,
                                 unsigned fb_width, unsigned fb_height,
                                 uint32_t sof1, unsigned concat) {
    unsigned field_adv = fb_width;
    uint16_t const *pixels_in = (uint16_t*)(pvr2->mem.tex32 + sof1);
    /*
     * bounds checking
     *
     * TODO: is it really necessary to test for
     * (last_byte < ADDR_TEX32_FIRST || first_byte > ADDR_TEX32_LAST) ?
     */
    addr32_t last_byte = sof1 + fb_width * fb_height * 2;
    addr32_t first_byte = sof1;

    // bounds checking.
    addr32_t bounds_field1[2] = {
        first_byte + ADDR_TEX32_FIRST,
        last_byte + ADDR_TEX32_FIRST
    };
    if (bounds_field1[0] < ADDR_TEX32_FIRST ||
        bounds_field1[0] > ADDR_TEX32_LAST ||
        bounds_field1[1] < ADDR_TEX32_FIRST ||
        bounds_field1[1] > ADDR_TEX32_LAST) {
        error_set_feature("whatever happens when a framebuffer is configured "
                          "to read outside of texture memory");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    uint32_t *dst_fb = (uint32_t*)pvr2->fb.ogl_fb;
    memset(pvr2->fb.ogl_fb, 0xff, sizeof(pvr2->fb.ogl_fb));

    unsigned row;
    for (row = 0; row < fb_height; row++) {
        uint16_t const *in_col_start = pixels_in + field_adv * row;
        uint32_t *out_col_start = dst_fb + row * fb_width;

        conv_rgb565_to_rgba8888(out_col_start, in_col_start, fb_width, concat);
    }

    fb->fb_read_width = fb_width;
    fb->fb_read_height = fb_height;
    fb->addr_key = first_byte;
    fb->addr_first[0] = first_byte;
    fb->addr_first[1] = first_byte;
    fb->addr_last[0] = last_byte;
    fb->addr_last[1] = last_byte;

    fb->flags.state = FB_STATE_VIRT_AND_GFX;

    fb->flags.vert_flip = true;
    fb->stamp = pvr2->fb.stamp;
    fb->flags.fmt = FB_PIX_FMT_RGB_565;

    struct gfx_il_inst cmd;

    cmd.op = GFX_IL_WRITE_OBJ;
    cmd.arg.write_obj.dat = dst_fb;
    cmd.arg.write_obj.obj_no = fb->obj_handle;
    cmd.arg.write_obj.n_bytes = OGL_FB_W_MAX * OGL_FB_H_MAX * 4;

    rend_exec_il(&cmd, 1);
}

static void
sync_fb_from_tex_mem_rgb555_intl(struct pvr2 *pvr2, struct framebuffer *fb,
                                 unsigned fb_width, unsigned fb_height,
                                 uint32_t sof1, uint32_t sof2,
                                 unsigned modulus, unsigned concat) {
    /*
     * field_adv represents the distand between the start of one row and the
     * start of the next row in the same field in terms of bytes.
     */
    unsigned field_adv = fb_width * 2 + modulus * 4 - 4;
    unsigned rows_per_field = fb_height;

    addr32_t first_addr_field1 = sof1;
    addr32_t last_addr_field1 = sof1 +
        field_adv * (rows_per_field - 1) + 2 * (fb_width - 1);
    addr32_t first_addr_field2 = sof2;
    addr32_t last_addr_field2 = sof2 +
        field_adv * (rows_per_field - 1) + 2 * (fb_width - 1);

    // bounds checking.
    addr32_t bounds_field1[2] = {
        first_addr_field1 + ADDR_TEX32_FIRST,
        last_addr_field1 + ADDR_TEX32_FIRST
    };
    addr32_t bounds_field2[2] = {
        first_addr_field2 + ADDR_TEX32_FIRST,
        last_addr_field2 + ADDR_TEX32_FIRST
    };
    if (bounds_field1[0] < ADDR_TEX32_FIRST ||
        bounds_field1[0] > ADDR_TEX32_LAST ||
        bounds_field1[1] < ADDR_TEX32_FIRST ||
        bounds_field1[1] > ADDR_TEX32_LAST ||
        bounds_field2[0] < ADDR_TEX32_FIRST ||
        bounds_field2[0] > ADDR_TEX32_LAST ||
        bounds_field2[1] < ADDR_TEX32_FIRST ||
        bounds_field2[1] > ADDR_TEX32_LAST) {
        error_set_feature("whatever happens when a framebuffer is configured "
                          "to read outside of texture memory");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    uint32_t *dst_fb = (uint32_t*)pvr2->fb.ogl_fb;

    uint8_t const *pvr2_tex32_mem = pvr2->mem.tex32;
    unsigned row;
    for (row = 0; row < rows_per_field; row++) {
        uint16_t const *ptr_row1 =
            (uint16_t const*)(pvr2_tex32_mem + sof1 + row * field_adv);
        uint16_t const *ptr_row2 =
            (uint16_t const*)(pvr2_tex32_mem + sof2 + row * field_adv);

        conv_rgb555_to_rgba8888(dst_fb + row * 2 * fb_width,
                                ptr_row1, fb_width, concat);
        conv_rgb555_to_rgba8888(dst_fb + (row * 2 + 1) * fb_width,
                                ptr_row2, fb_width, concat);
    }

    fb->addr_key = first_addr_field1  < first_addr_field2 ?
        first_addr_field1 : first_addr_field2;

    fb->addr_first[0] = first_addr_field1;
    fb->addr_first[1] = first_addr_field2;
    fb->addr_last[0] = last_addr_field1;
    fb->addr_last[1] = last_addr_field2;

    fb->fb_read_width = fb_width;
    fb->fb_read_height = fb_height;

    fb->flags.state = FB_STATE_VIRT_AND_GFX;

    fb->flags.vert_flip = true;
    fb->stamp = pvr2->fb.stamp;
    fb->flags.fmt = FB_PIX_FMT_RGB_555;

    struct gfx_il_inst cmd;

    cmd.op = GFX_IL_WRITE_OBJ;
    cmd.arg.write_obj.dat = dst_fb;
    cmd.arg.write_obj.obj_no = fb->obj_handle;
    cmd.arg.write_obj.n_bytes = OGL_FB_W_MAX * OGL_FB_H_MAX * 4;

    rend_exec_il(&cmd, 1);
}

static void
sync_fb_from_tex_mem_rgb888_intl(struct pvr2 *pvr2, struct framebuffer *fb,
                                 unsigned fb_width, unsigned fb_height,
                                 uint32_t sof1, uint32_t sof2,
                                 unsigned modulus) {
    /*
     * field_adv represents the distand between the start of one row and the
     * start of the next row in the same field in terms of bytes.
     */
    unsigned field_adv = fb_width * 3 + modulus * 4 - 4;
    unsigned rows_per_field = fb_height;

    addr32_t first_addr_field1 = sof1;
    addr32_t last_addr_field1 = sof1 +
        field_adv * (rows_per_field - 1) + 3 * (fb_width - 1);
    addr32_t first_addr_field2 = sof2;
    addr32_t last_addr_field2 = sof2 +
        field_adv * (rows_per_field - 1) + 3 * (fb_width - 1);

    // bounds checking.
    addr32_t bounds_field1[2] = {
        first_addr_field1 + ADDR_TEX32_FIRST,
        last_addr_field1 + ADDR_TEX32_FIRST
    };
    addr32_t bounds_field2[2] = {
        first_addr_field2 + ADDR_TEX32_FIRST,
        last_addr_field2 + ADDR_TEX32_FIRST
    };
    if (bounds_field1[0] < ADDR_TEX32_FIRST ||
        bounds_field1[0] > ADDR_TEX32_LAST ||
        bounds_field1[1] < ADDR_TEX32_FIRST ||
        bounds_field1[1] > ADDR_TEX32_LAST ||
        bounds_field2[0] < ADDR_TEX32_FIRST ||
        bounds_field2[0] > ADDR_TEX32_LAST ||
        bounds_field2[1] < ADDR_TEX32_FIRST ||
        bounds_field2[1] > ADDR_TEX32_LAST) {
        error_set_feature("whatever happens when a framebuffer is configured "
                          "to read outside of texture memory");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    uint32_t *dst_fb = (uint32_t*)pvr2->fb.ogl_fb;

    uint8_t const *pvr2_tex32_mem = pvr2->mem.tex32;
    unsigned row;
    for (row = 0; row < rows_per_field; row++) {
        uint8_t const *ptr_row1 =
            pvr2_tex32_mem + sof1 + row * field_adv;
        uint8_t const *ptr_row2 =
            pvr2_tex32_mem + sof2 + row * field_adv;

        conv_rgb888_to_rgba8888(dst_fb + row * 2 * fb_width,
                                ptr_row1, fb_width);
        conv_rgb888_to_rgba8888(dst_fb + (row * 2 + 1) * fb_width,
                                ptr_row2, fb_width);
    }

    fb->addr_key = first_addr_field1  < first_addr_field2 ?
        first_addr_field1 : first_addr_field2;

    fb->addr_first[0] = first_addr_field1;
    fb->addr_first[1] = first_addr_field2;
    fb->addr_last[0] = last_addr_field1;
    fb->addr_last[1] = last_addr_field2;

    fb->fb_read_width = fb_width;
    fb->fb_read_height = fb_height;

    fb->flags.state = FB_STATE_VIRT_AND_GFX;

    fb->flags.vert_flip = true;
    fb->stamp = pvr2->fb.stamp;
    fb->flags.fmt = FB_PIX_FMT_RGB_888;

    struct gfx_il_inst cmd;

    cmd.op = GFX_IL_WRITE_OBJ;
    cmd.arg.write_obj.dat = dst_fb;
    cmd.arg.write_obj.obj_no = fb->obj_handle;
    cmd.arg.write_obj.n_bytes = OGL_FB_W_MAX * OGL_FB_H_MAX * 4;

    rend_exec_il(&cmd, 1);
}

static void
sync_fb_from_tex_mem_rgb555_prog(struct pvr2 *pvr2, struct framebuffer *fb,
                                 unsigned fb_width, unsigned fb_height,
                                 uint32_t sof1, unsigned concat) {
    unsigned field_adv = fb_width;
    uint16_t const *pixels_in = (uint16_t*)(pvr2->mem.tex32 + sof1);
    /*
     * bounds checking
     *
     * TODO: is it really necessary to test for
     * (last_byte < ADDR_TEX32_FIRST || first_byte > ADDR_TEX32_LAST) ?
     */
    addr32_t last_byte = sof1 + fb_width * fb_height * 2;
    addr32_t first_byte = sof1;

    // bounds checking.
    addr32_t bounds_field1[2] = {
        first_byte + ADDR_TEX32_FIRST,
        last_byte + ADDR_TEX32_FIRST
    };
    if (bounds_field1[0] < ADDR_TEX32_FIRST ||
        bounds_field1[0] > ADDR_TEX32_LAST ||
        bounds_field1[1] < ADDR_TEX32_FIRST ||
        bounds_field1[1] > ADDR_TEX32_LAST) {
        error_set_feature("whatever happens when a framebuffer is configured "
                          "to read outside of texture memory");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    uint32_t *dst_fb = (uint32_t*)pvr2->fb.ogl_fb;
    memset(pvr2->fb.ogl_fb, 0xff, sizeof(pvr2->fb.ogl_fb));

    unsigned row;
    for (row = 0; row < fb_height; row++) {
        uint16_t const *in_col_start = pixels_in + field_adv * row;
        uint32_t *out_col_start = dst_fb + row * fb_width;

        conv_rgb555_to_rgba8888(out_col_start, in_col_start, fb_width, concat);
    }

    fb->fb_read_width = fb_width;
    fb->fb_read_height = fb_height;
    fb->addr_key = first_byte;
    fb->addr_first[0] = first_byte;
    fb->addr_first[1] = first_byte;
    fb->addr_last[0] = last_byte;
    fb->addr_last[1] = last_byte;

    fb->flags.state = FB_STATE_VIRT_AND_GFX;

    fb->flags.vert_flip = true;
    fb->stamp = pvr2->fb.stamp;
    fb->flags.fmt = FB_PIX_FMT_RGB_555;

    struct gfx_il_inst cmd;

    cmd.op = GFX_IL_WRITE_OBJ;
    cmd.arg.write_obj.dat = dst_fb;
    cmd.arg.write_obj.obj_no = fb->obj_handle;
    cmd.arg.write_obj.n_bytes = OGL_FB_W_MAX * OGL_FB_H_MAX * 4;

    rend_exec_il(&cmd, 1);
}

static void
sync_fb_from_tex_mem_rgb0888_intl(struct pvr2 *pvr2, struct framebuffer *fb,
                                  unsigned fb_width, unsigned fb_height,
                                  uint32_t sof1, uint32_t sof2,
                                  unsigned modulus) {
    /*
     * field_adv represents the distand between the start of one row and the
     * start of the next row in the same field in terms of bytes.
     */
    unsigned field_adv = (fb_width * 4) + (modulus * 4) - 4;
    unsigned rows_per_field = fb_height /* / 2 */;

    addr32_t first_addr_field1 = sof1;
    addr32_t last_addr_field1 = sof1 +
        field_adv * (rows_per_field - 1) + 2 * (fb_width - 1);
    addr32_t first_addr_field2 = sof2;
    addr32_t last_addr_field2 = sof2 +
        field_adv * (rows_per_field - 1) + 2 * (fb_width - 1);

    // bounds checking.
    addr32_t bounds_field1[2] = {
        first_addr_field1 + ADDR_TEX32_FIRST,
        last_addr_field1 + ADDR_TEX32_FIRST
    };
    addr32_t bounds_field2[2] = {
        first_addr_field2 + ADDR_TEX32_FIRST,
        last_addr_field2 + ADDR_TEX32_FIRST
    };
    if (bounds_field1[0] < ADDR_TEX32_FIRST ||
        bounds_field1[0] > ADDR_TEX32_LAST ||
        bounds_field1[1] < ADDR_TEX32_FIRST ||
        bounds_field1[1] > ADDR_TEX32_LAST ||
        bounds_field2[0] < ADDR_TEX32_FIRST ||
        bounds_field2[0] > ADDR_TEX32_LAST ||
        bounds_field2[1] < ADDR_TEX32_FIRST ||
        bounds_field2[1] > ADDR_TEX32_LAST) {
        error_set_feature("whatever happens when a framebuffer is configured "
                          "to read outside of texture memory");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    uint32_t *dst_fb = (uint32_t*)pvr2->fb.ogl_fb;

    uint8_t const *pvr2_tex32_mem = pvr2->mem.tex32;
    unsigned row;
    for (row = 0; row < rows_per_field; row++) {
        uint32_t const *ptr_row1 =
            (uint32_t const*)(pvr2_tex32_mem + sof1 + row * field_adv);
        uint32_t const *ptr_row2 =
            (uint32_t const*)(pvr2_tex32_mem + sof2 + row * field_adv);

        conv_rgb0888_to_rgba8888(dst_fb + (row << 1) * fb_width,
                                 ptr_row1, fb_width);
        conv_rgb0888_to_rgba8888(dst_fb + ((row << 1) + 1) * fb_width,
                                 ptr_row2, fb_width);
    }

    fb->fb_read_width = fb_width;
    fb->fb_read_height = fb_height;

    fb->addr_key = first_addr_field1 < first_addr_field2 ?
        first_addr_field1 : first_addr_field2;

    fb->addr_first[0] = first_addr_field1;
    fb->addr_first[1] = first_addr_field2;
    fb->addr_last[0] = last_addr_field1;
    fb->addr_last[1] = last_addr_field2;

    fb->flags.state = FB_STATE_VIRT_AND_GFX;

    fb->flags.vert_flip = true;
    fb->stamp = pvr2->fb.stamp;
    fb->flags.fmt = FB_PIX_FMT_0RGB_0888;

    struct gfx_il_inst cmd;

    cmd.op = GFX_IL_WRITE_OBJ;
    cmd.arg.write_obj.dat = dst_fb;
    cmd.arg.write_obj.obj_no = fb->obj_handle;
    cmd.arg.write_obj.n_bytes = OGL_FB_W_MAX * OGL_FB_H_MAX * 4;

    rend_exec_il(&cmd, 1);
}

static void
sync_fb_from_tex_mem_rgb0888_prog(struct pvr2 *pvr2, struct framebuffer *fb,
                                  unsigned fb_width, unsigned fb_height,
                                  uint32_t sof1) {
    uint32_t const *pixels_in = (uint32_t const*)(pvr2->mem.tex32 + sof1);
    addr32_t last_byte = sof1 + fb_width * fb_height * 4;
    addr32_t first_byte = sof1;

    /*
     * bounds checking
     *
     * TODO: is it really necessary to test for
     * (last_byte < ADDR_TEX32_FIRST || first_byte > ADDR_TEX32_LAST) ?
     */
    addr32_t bounds_field1[2] = {
        first_byte + ADDR_TEX32_FIRST,
        last_byte + ADDR_TEX32_FIRST
    };
    if (bounds_field1[0] < ADDR_TEX32_FIRST ||
        bounds_field1[0] > ADDR_TEX32_LAST ||
        bounds_field1[1] < ADDR_TEX32_FIRST ||
        bounds_field1[1] > ADDR_TEX32_LAST) {
        error_set_feature("whatever happens when a framebuffer is configured "
                          "to read outside of texture memory");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    uint32_t *dst_fb = (uint32_t*)pvr2->fb.ogl_fb;

    unsigned row;
    for (row = 0; row < fb_height; row++) {
        uint32_t const *in_col_start = pixels_in + fb_width * row;
        uint32_t *out_col_start = dst_fb + row * fb_width;

        conv_rgb0888_to_rgba8888(out_col_start, in_col_start, fb_width);
    }

    fb->fb_read_width = fb_width;
    fb->fb_read_height = fb_height;
    fb->addr_key = first_byte;
    fb->addr_first[0] = first_byte;
    fb->addr_first[1] = first_byte;
    fb->addr_last[0] = last_byte;
    fb->addr_last[1] = last_byte;

    fb->flags.state = FB_STATE_VIRT_AND_GFX;

    fb->flags.vert_flip = true;
    fb->stamp = pvr2->fb.stamp;
    fb->flags.fmt = FB_PIX_FMT_0RGB_0888;

    struct gfx_il_inst cmd;

    cmd.op = GFX_IL_WRITE_OBJ;
    cmd.arg.write_obj.dat = dst_fb;
    cmd.arg.write_obj.obj_no = fb->obj_handle;
    cmd.arg.write_obj.n_bytes = OGL_FB_W_MAX * OGL_FB_H_MAX * 4;

    rend_exec_il(&cmd, 1);
}

static void
sync_fb_from_tex_mem(struct pvr2 *pvr2, struct framebuffer *fb,
                     unsigned width, unsigned height,
                     unsigned modulus, unsigned concat) {
    bool interlace = get_spg_control(pvr2) & (1 << 4);

    uint32_t fb_r_sof1 = get_fb_r_sof1(pvr2) & ~3;
    uint32_t fb_r_sof2 = get_fb_r_sof2(pvr2) & ~3;

    uint32_t fb_r_ctrl = get_fb_r_ctrl(pvr2);
    unsigned px_tp = (fb_r_ctrl & 0xc) >> 2;
    switch (px_tp) {
    case 0:
        // 16-bit 555 RGB
        if (interlace) {
            sync_fb_from_tex_mem_rgb555_intl(pvr2, fb, width, height, fb_r_sof1,
                                             fb_r_sof2, modulus, concat);
        } else {
            sync_fb_from_tex_mem_rgb555_prog(pvr2, fb, width, height,
                                             fb_r_sof1, concat);
        }
        break;
    case 1:
        // 16-bit 565 RGB
        if (interlace) {
            sync_fb_from_tex_mem_rgb565_intl(pvr2, fb, width, height, fb_r_sof1,
                                             fb_r_sof2, modulus, concat);
        } else {
            sync_fb_from_tex_mem_rgb565_prog(pvr2, fb, width, height,
                                             fb_r_sof1, concat);
        }
        break;
    case 2:
        // 24-bit 888 RGB
        if (interlace) {
            sync_fb_from_tex_mem_rgb888_intl(pvr2, fb, width, height, fb_r_sof1,
                                             fb_r_sof2, modulus);
        } else {
            error_set_feature("video mode RGB888 (progressive scan)");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
        break;
    case 3:
        // 32-bit 08888 RGB
        if (interlace) {
            sync_fb_from_tex_mem_rgb0888_intl(pvr2, fb, width, height, fb_r_sof1,
                                              fb_r_sof2, modulus);
        } else {
            sync_fb_from_tex_mem_rgb0888_prog(pvr2, fb, width, height,
                                              fb_r_sof1);
        }
    }
}

/*
 * this is a simple "dumb" memcpy function that doesn't handle the framebuffer
 * state (this is what makes it different from pvr2_tex_mem_area32_write).  It
 * does, however, perform bounds-checking and raise an error for out-of-bounds
 * memory access.
 */
static void copy_to_tex_mem(struct pvr2 *pvr2, void const *in,
                            addr32_t offs, size_t len);

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
conv_rgb555_to_rgba8888(uint32_t *pixels_out,
                        uint16_t const *pixels_in,
                        unsigned n_pixels, uint8_t concat) {
    for (unsigned idx = 0; idx < n_pixels; idx++) {
        uint16_t pix = pixels_in[idx];

        uint32_t b = ((pix & 0x001f) << 3) | concat;
        uint32_t g = (((pix & 0x03e0) >> 5) << 2) | (concat & 3);
        uint32_t r = (((pix & 0xec00) >> 10) << 2) | concat;

        pixels_out[idx] = (255 << 24) | (b << 16) | (g << 8) | r;
    }
}

static void
conv_rgb888_to_rgba8888(uint32_t *pixels_out,
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

static int
pick_fb(struct pvr2 *pvr2, unsigned width, unsigned height, uint32_t addr);

// reset all members except the gfx_obj handle
static void fb_reset(struct framebuffer *fb) {
    fb->fb_read_width = 0;
    fb->fb_read_height = 0;
    fb->linestride = 0;
    fb->addr_first[0] = 0;
    fb->addr_first[1] = 0;
    fb->addr_last[0] = 0;
    fb->addr_last[1] = 0;
    fb->addr_key = 0;
    fb->stamp = 0;
    fb->tile_w = 0;
    fb->tile_h = 0;
    fb->x_clip_min = 0;
    fb->x_clip_max = 0;
    fb->y_clip_min = 0;
    fb->y_clip_max = 0;
    fb->flags.state = FB_STATE_INVALID;
    fb->flags.fmt = FB_PIX_FMT_RGB_555;
    fb->flags.vert_flip = false;
}

void pvr2_framebuffer_init(struct pvr2 *pvr2) {
    struct gfx_il_inst cmd;
    struct framebuffer *fb_heap = pvr2->fb.fb_heap;

    int fb_no;
    for (fb_no = 0; fb_no < FB_HEAP_SIZE; fb_no++) {
        fb_reset(fb_heap + fb_no);
        fb_heap[fb_no].obj_handle = pvr2_alloc_gfx_obj();

        cmd.op = GFX_IL_INIT_OBJ;
        cmd.arg.init_obj.obj_no = fb_heap[fb_no].obj_handle;
        cmd.arg.init_obj.n_bytes = OGL_FB_W_MAX * OGL_FB_H_MAX *
            4 * sizeof(uint8_t);

        rend_exec_il(&cmd, 1);
    }
}

void pvr2_framebuffer_cleanup(struct pvr2 *pvr2) {
}

void framebuffer_render(struct pvr2 *pvr2) {
    uint32_t fb_r_ctrl = get_fb_r_ctrl(pvr2);
    if (!(fb_r_ctrl & 1)) {
        LOG_DBG("framebuffer disabled\n");
        // framebuffer is not enabled.
        // TODO: display all-white or all black here instead of letting
        // the screen look corrupted?
        return;
    }

    bool interlace = get_spg_control(pvr2) & (1 << 4);
    uint32_t fb_r_size = get_fb_r_size(pvr2);
    uint32_t fb_r_sof1 = get_fb_r_sof1(pvr2) & ~3;

    unsigned modulus = (fb_r_size >> 20) & 0x3ff;
    unsigned concat = (fb_r_ctrl >> 4) & 7;

    unsigned pix_sz = bytes_per_pix(fb_r_ctrl);
    unsigned width = ((fb_r_size & 0x3ff) + 1) * 4;
    if (width % pix_sz) {
        LOG_ERROR("fb x size is %u\n", width);
        LOG_ERROR("px_sz is %u\n", pix_sz);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
    width /= pix_sz;
    unsigned height = ((fb_r_size >> 10) & 0x3ff) + 1;

    struct gfx_il_inst cmd;

    uint32_t addr_first = fb_r_sof1;
    if (interlace) {
        uint32_t fb_r_sof2 = get_fb_r_sof2(pvr2) & ~3;
        if (fb_r_sof2 < addr_first)
            addr_first = fb_r_sof2;
    }

    struct framebuffer *fb_heap = pvr2->fb.fb_heap;

    int fb_idx;
    for (fb_idx = 0; fb_idx < FB_HEAP_SIZE; fb_idx++) {
        struct framebuffer *fb = fb_heap + fb_idx;
        if (fb->fb_read_width == width &&
            fb->fb_read_height == height &&
            fb->addr_key == addr_first &&
            fb->flags.state != FB_STATE_INVALID) {

            if (!(fb_heap[fb_idx].flags.state & FB_STATE_GFX)) {
                sync_fb_from_tex_mem(pvr2, fb_heap + fb_idx, width,
                                     height, modulus, concat);
            }

            goto submit_the_fb;
        }
    }

    fb_idx = pick_fb(pvr2, width, height, fb_r_sof1);
    sync_fb_from_tex_mem(pvr2, fb_heap + fb_idx,
                         width, height, modulus, concat);

submit_the_fb:
    pvr2->fb.stamp++;

    cmd.op = GFX_IL_POST_FRAMEBUFFER;
    cmd.arg.post_framebuffer.obj_handle = fb_heap[fb_idx].obj_handle;
    cmd.arg.post_framebuffer.width = fb_heap[fb_idx].fb_read_width;
    cmd.arg.post_framebuffer.height = fb_heap[fb_idx].fb_read_height;
    if (interlace)
        cmd.arg.post_framebuffer.height *= 2;
    cmd.arg.post_framebuffer.vert_flip = fb_heap[fb_idx].flags.vert_flip;

    title_set_resolution(cmd.arg.post_framebuffer.width,
                         cmd.arg.post_framebuffer.height);
    title_set_interlace(interlace);

    char const *pix_fmt_str;
    switch ((fb_r_ctrl & 0xc) >> 2) {
    case 0:
        pix_fmt_str = "555 RGB";
        break;
    case 1:
        pix_fmt_str = "565 RGB";
        break;
    case 2:
        pix_fmt_str = "888 RGB";
        break;
    case 3:
        pix_fmt_str = "0888 RGB";
        break;
    default:
        pix_fmt_str = "<unknown>";
    }
    title_set_pix_fmt(pix_fmt_str);

    rend_exec_il(&cmd, 1);
}

static void
fb_sync_from_host_0565_krgb(struct pvr2 *pvr2, struct framebuffer *fb) {
    unsigned x_min = fb->x_clip_min;
    unsigned y_min = fb->y_clip_min;
    unsigned x_max = fb->tile_w < fb->x_clip_max ? fb->tile_w : fb->x_clip_max;
    unsigned y_max = fb->tile_h < fb->y_clip_max ? fb->tile_h : fb->y_clip_max;
    unsigned width = x_max - x_min + 1;
    unsigned height = y_max - y_min + 1;

    unsigned stride = fb->linestride;
    uint32_t const *addr = fb->addr_first;

    uint16_t const k_val = 0;

    assert((width * height * 4) < OGL_FB_BYTES);

    unsigned row, col;
    uint8_t *ogl_fb = pvr2->fb.ogl_fb;
    for (row = y_min; row <= y_max; row++) {
        unsigned line_offs = addr[0] + (height - (row + 1)) * stride;
        for (col = x_min; col <= x_max; col++) {
            unsigned fb_idx = row * width + col;
            uint16_t pix_out = ((ogl_fb[4 * fb_idx + 2] & 0xf8) >> 3) |
                ((ogl_fb[4 * fb_idx + 1] & 0xfc) << 3) |
                ((ogl_fb[4 * fb_idx] & 0xf8) << 8) | k_val;
            copy_to_tex_mem(pvr2, &pix_out,
                            line_offs + 2 * col, sizeof(pix_out));
        }
    }
}

static void
fb_sync_from_host_0555_krgb(struct pvr2 *pvr2, struct framebuffer *fb) {
    unsigned x_min = fb->x_clip_min;
    unsigned y_min = fb->y_clip_min;
    unsigned x_max = fb->tile_w < fb->x_clip_max ? fb->tile_w : fb->x_clip_max;
    unsigned y_max = fb->tile_h < fb->y_clip_max ? fb->tile_h : fb->y_clip_max;
    unsigned width = x_max - x_min + 1;
    unsigned height = y_max - y_min + 1;

    unsigned stride = fb->linestride;
    uint32_t const *addr = fb->addr_first;

    uint16_t const k_val = 0;

    assert((width * height * 4) < OGL_FB_BYTES);

    unsigned row, col;
    uint8_t *ogl_fb = pvr2->fb.ogl_fb;
    for (row = y_min; row <= y_max; row++) {
        unsigned line_offs = addr[0] + (height - (row + 1)) * stride;
        for (col = x_min; col <= x_max; col++) {
            unsigned fb_idx = row * width + col;
            uint16_t pix_out = ((ogl_fb[4 * fb_idx + 2] & 0xf8) >> 3) |
                ((ogl_fb[4 * fb_idx + 1] & 0xf8) << 3) |
                ((ogl_fb[4 * fb_idx] & 0xf8) << 7) | k_val;
            copy_to_tex_mem(pvr2, &pix_out,
                            line_offs + 2 * col, sizeof(pix_out));
        }
    }
}


static void
fb_sync_from_host_1555_argb(struct pvr2 *pvr2, struct framebuffer *fb) {
    unsigned x_min = fb->x_clip_min;
    unsigned y_min = fb->y_clip_min;
    unsigned x_max = fb->tile_w < fb->x_clip_max ? fb->tile_w : fb->x_clip_max;
    unsigned y_max = fb->tile_h < fb->y_clip_max ? fb->tile_h : fb->y_clip_max;
    unsigned width = x_max - x_min + 1;
    unsigned height = y_max - y_min + 1;

    unsigned stride = fb->linestride;
    uint32_t const *addr = fb->addr_first;

    assert((width * height * 4) < OGL_FB_BYTES);

    unsigned row, col;
    uint8_t *ogl_fb = pvr2->fb.ogl_fb;
    for (row = y_min; row <= y_max; row++) {
        /*
         * TODO: figure out how this is supposed to work with interlacing.
         *
         * The below code implements this as if it was progressive-scan, and
         * it works perfectly.  Obviously this means that either my
         * understanding of interlace-scan is incorrect, or it's actually
         * supposed to be progressive-scan and WashingtonDC is not figuring
         * that out right.
         */
        unsigned line_offs = addr[0] + (height - (row + 1)) * stride;
        for (col = x_min; col <= x_max; col++) {
            unsigned fb_idx = row * width + col;

            uint8_t const *pix_in = ogl_fb + 4 * fb_idx;
            uint16_t red = (pix_in[0] & 0xf8) >> 3;
            uint16_t green = (pix_in[1] & 0xf8) >> 3;
            uint16_t blue = (pix_in[2] * 0xf8) >> 3;
            uint16_t alpha = pix_in[3] ? 1 : 0;

            uint16_t pix_out =
                (alpha << 15) | (red << 10) | (green << 5) | blue;

            copy_to_tex_mem(pvr2, &pix_out,
                            line_offs + 2 * col, sizeof(pix_out));
        }
    }
}

static void fb_sync_from_host_rgb0888(struct pvr2 *pvr2, struct framebuffer *fb) {
    /*
     * TODO: don't get width, height from fb_read_width and fb_read_height
     * (see fb_sync_from_host_0565_krgb_intl for an example of how this should
     * work).
     */
    unsigned width = fb->fb_read_width;
    unsigned height = fb->fb_read_height;
    unsigned stride = fb->linestride;
    uint32_t const *fb_in = (uint32_t*)pvr2->fb.ogl_fb;
    unsigned rows_per_field = height / 2;
    unsigned const *addr = fb->addr_first;

    assert((width * height * 4) < OGL_FB_BYTES);

    unsigned row, col;
    for (row = 0; row < rows_per_field; row++) {
        unsigned row_actual[2] = { 2 * row, 2 * row + 1 };
        unsigned line_offs[2] = {
            addr[0] + (rows_per_field - (row + 1)) * stride,
            addr[1] + (rows_per_field - (row + 1)) * stride
        };

        for (col = 0; col < width; col++) {
            unsigned ogl_fb_idx[2] = {
                row_actual[0] * width + col,
                row_actual[1] * width + col
            };

            uint32_t pix_in[2] = {
                fb_in[ogl_fb_idx[0]],
                fb_in[ogl_fb_idx[1]]
            };

            uint32_t pix_out[2] = {
                pix_in[0] & 0x00ffffff,
                pix_in[1] & 0x00ffffff
            };

            copy_to_tex_mem(pvr2, pix_out + 0,
                            line_offs[0] + 2 * col, sizeof(pix_out[0]));
            copy_to_tex_mem(pvr2, pix_out + 1,
                            line_offs[1] + 2 * col, sizeof(pix_out[1]));
        }
    }
}

static void fb_sync_from_host_argb8888(struct pvr2 *pvr2, struct framebuffer *fb) {
    /*
     * TODO: don't get width, height from fb_read_width and fb_read_height
     * (see fb_sync_from_host_0565_krgb_intl for an example of how this should
     * work).
     */
    unsigned width = fb->fb_read_width;
    unsigned height = fb->fb_read_height;
    unsigned stride = fb->linestride;
    uint32_t const *fb_in = (uint32_t*)pvr2->fb.ogl_fb;
    unsigned rows_per_field = height / 2;
    unsigned const *addr = fb->addr_first;

    assert((width * height * 4) < OGL_FB_BYTES);

    unsigned row, col;
    for (row = 0; row < rows_per_field; row++) {
        unsigned row_actual[2] = { 2 * row, 2 * row + 1 };
        unsigned line_offs[2] = {
            addr[0] + (rows_per_field - (row + 1)) * stride,
            addr[1] + (rows_per_field - (row + 1)) * stride
        };

        for (col = 0; col < width; col++) {
            unsigned ogl_fb_idx[2] = {
                row_actual[0] * width + col,
                row_actual[1] * width + col
            };

            uint32_t pix_in[2] = {
                fb_in[ogl_fb_idx[0]],
                fb_in[ogl_fb_idx[1]]
            };

            uint32_t pix_out[2] = {
                pix_in[0],
                pix_in[1]
            };

            copy_to_tex_mem(pvr2, pix_out + 0,
                            line_offs[0] + 2 * col, sizeof(pix_out[0]));
            copy_to_tex_mem(pvr2, pix_out + 1,
                            line_offs[1] + 2 * col, sizeof(pix_out[1]));
        }
    }
}

static void
sync_fb_to_tex_mem(struct pvr2 *pvr2, struct framebuffer *fb) {
    if (fb->flags.state != FB_STATE_GFX)
        return;

    fb->flags.state |= ~FB_STATE_VIRT;

    struct gfx_il_inst cmd = {
        .op = GFX_IL_READ_OBJ,
        .arg = { .read_obj = {
            .dat = pvr2->fb.ogl_fb,
            .obj_no = fb->obj_handle,
            .n_bytes = sizeof(pvr2->fb.ogl_fb)/* OGL_FB_W_MAX * OGL_FB_H_MAX * 4 */
            } }
    };
    rend_exec_il(&cmd, 1);
    switch (fb->flags.fmt) {
    case FB_PIX_FMT_RGB_555:
        fb_sync_from_host_0555_krgb(pvr2, fb);
        break;
    case FB_PIX_FMT_RGB_565:
        fb_sync_from_host_0565_krgb(pvr2, fb);
        break;
    case FB_PIX_FMT_0RGB_0888:
        fb_sync_from_host_rgb0888(pvr2, fb);
        break;
    case FB_PIX_FMT_ARGB_8888:
        fb_sync_from_host_argb8888(pvr2, fb);
        break;
    case FB_PIX_FMT_ARGB_1555:
        fb_sync_from_host_1555_argb(pvr2, fb);
        break;
    default:
        LOG_ERROR("fb->flags.fmt is %d\n", fb->flags.fmt);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

/*
 * returns a pointer to the area that addr belongs in.
 * Keep in mind that this pointer points to the beginning of that area,
 * it doesn NOT point to the actual byte that corresponds to the addr.
 */
static uint8_t *get_tex_mem_area(struct pvr2 *pvr2, addr32_t addr) {
    switch (addr & 0xff000000) {
    case 0x04000000:
    case 0x06000000:
        return pvr2->mem.tex64;
    case 0x05000000:
    case 0x07000000:
        return pvr2->mem.tex32;
    default:
        return NULL;
    }
}

static uint32_t get_tex_mem_offs(addr32_t addr) {
    switch (addr & 0xff000000) {
    case 0x04000000:
    case 0x06000000:
        return ADDR_TEX64_FIRST;
    case 0x05000000:
    case 0x07000000:
        return ADDR_TEX32_FIRST;
    default:
        LOG_ERROR("%s - 0x%08x is not a texture memory pointer\n",
                  __func__, (unsigned)addr);
        return ADDR_TEX32_FIRST;
    }
}

#define TEX_MIRROR_MASK 0x7fffff

static void copy_to_tex_mem(struct pvr2 *pvr2, void const *in,
                            addr32_t offs, size_t len) {
    addr32_t last_byte = offs - 1 + len;

    if ((last_byte & 0xff000000) != (offs & 0xff000000)) {
        error_set_length(len);
        error_set_address(offs + ADDR_TEX32_FIRST);
        error_set_feature("texture memory writes across boundaries");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    uint8_t *tex_mem_ptr = get_tex_mem_area(pvr2, offs + ADDR_TEX32_FIRST);
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
    offs &= TEX_MIRROR_MASK;
    memcpy(tex_mem_ptr + offs, in, len);

    /*
     * let the texture tracking system know we may have just overwritten a
     * texture in the cache.
     */
    if (tex_mem_ptr == pvr2->mem.tex64)
        pvr2_tex_cache_notify_write(pvr2, offs + ADDR_TEX64_FIRST, len);
}

static int
pick_fb(struct pvr2 *pvr2, unsigned width, unsigned height, uint32_t addr) {
    int first_invalid = -1;
    int idx;
    int oldest_stamp = pvr2->fb.stamp;
    int oldest_stamp_idx = -1;
    struct framebuffer *fb_heap = pvr2->fb.fb_heap;

    for (idx = 0; idx < FB_HEAP_SIZE; idx++) {
        if (fb_heap[idx].flags.state != FB_STATE_INVALID) {
            if (fb_heap[idx].fb_read_width == width &&
                fb_heap[idx].fb_read_height == height &&
                fb_heap[idx].addr_key == addr) {
                return idx;
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

        // sync the framebuffer to memory because it's about to get overwritten
        struct framebuffer *fb = fb_heap + idx;
        sync_fb_to_tex_mem(pvr2, fb);
        fb_reset(fb);
    }

    fb_reset(fb_heap + idx);
    return idx;
}

int framebuffer_set_render_target(struct pvr2 *pvr2) {
    struct framebuffer *fb_heap = pvr2->fb.fb_heap;

    /*
     * XXX
     *
     * Games will often configure PVR2 to render a couple extra rows of pixels
     * that the video hardware is not ocnfigured to send to the display
     * (read-height is less than write-height).  This presents a problem for
     * WashingtonDC because it needs the read-dimensions and the
     * write-dimensions to match up when it's searching the fb_heap for a
     * framebuffer.  My solution to this problem is to store the read-width and
     * read-height in the struct framebuffer, and calculate the write-width and
     * write-height when the framebuffer actually gets synced to tex memory.
     *
     * This could potentially cause problems if the fb_r_size register changes
     * between the point where the framebuffer gets rendered and the vblank
     * interrupt, but I don't know any better way to solve this problem.  This is
     * something to keep in mind for the future.
     */
    unsigned pix_sz = bytes_per_pix(get_fb_r_ctrl(pvr2));
    uint32_t fb_r_size = get_fb_r_size(pvr2);
    unsigned width = (((fb_r_size & 0x3ff) + 1) * 4);
    if (width % pix_sz) {
        LOG_ERROR("fb x size is %u\n", width);
        LOG_ERROR("px_sz is %u\n", pix_sz);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
    width /= pix_sz;
    unsigned height = ((fb_r_size >> 10) & 0x3ff) + 1;
    uint32_t sof1 = get_fb_w_sof1(pvr2) & ~3;
    uint32_t addr_key = sof1;

    int idx = pick_fb(pvr2, width, height, addr_key);

    struct framebuffer *fb = pvr2->fb.fb_heap + idx;

    fb->flags.state = FB_STATE_GFX;
    fb->flags.vert_flip = false;
    fb->fb_read_width = width;
    fb->fb_read_height = height;
    fb->stamp = pvr2->fb.stamp;
    fb->linestride = get_fb_w_linestride(pvr2) * 8;

    // set addr_first and addr_last
    uint32_t first_addr_field1, last_addr_field1;
    unsigned field_adv;
    unsigned modulus = (fb_r_size >> 20) & 0x3ff;
    uint32_t fb_w_ctrl = get_fb_w_ctrl(pvr2);

    // TODO: the k-bit
    switch (fb_w_ctrl & 0x7) {
    case 2:
        // 16-bit 4444 RGB
    case 7:
        // absolutely haram
        error_set_fb_pix_fmt(fb_w_ctrl & 0x7);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    case 0:
        // 16-bit 555 KRGB
        field_adv = width * 2 + modulus * 4 - 4;
        first_addr_field1 = sof1;
        last_addr_field1 = sof1 +
            field_adv * (height - 1) + 2 * (width - 1);

        fb->addr_key = addr_key;

        fb->addr_first[0] = first_addr_field1;
        fb->addr_first[1] = first_addr_field1;
        fb->addr_last[0] = last_addr_field1;
        fb->addr_last[1] = last_addr_field1;

        fb_heap[idx].flags.fmt = FB_PIX_FMT_RGB_555;
        break;
    case 1:
        // 16-bit 565 RGB
        field_adv = width * 2 + modulus * 4 - 4;
        first_addr_field1 = sof1;
        last_addr_field1 = sof1 +
            field_adv * (height - 1) + 2 * (width - 1);

        fb->addr_key = addr_key;

        fb->addr_first[0] = first_addr_field1;
        fb->addr_first[1] = first_addr_field1;
        fb->addr_last[0] = last_addr_field1;
        fb->addr_last[1] = last_addr_field1;

        fb_heap[idx].flags.fmt = FB_PIX_FMT_RGB_565;
        break;
    case 3:
        // 16-bit 1555 ARGB
        field_adv = width * 2 + modulus * 4 - 4;
        first_addr_field1 = sof1;
        last_addr_field1 = sof1 +
            field_adv * (height - 1) + 2 * (width - 1);

        fb->addr_key = addr_key;

        fb->addr_first[0] = first_addr_field1;
        fb->addr_first[1] = first_addr_field1;
        fb->addr_last[0] = last_addr_field1;
        fb->addr_last[1] = last_addr_field1;

        fb_heap[idx].flags.fmt = FB_PIX_FMT_ARGB_1555;
        break;
    case 4:
        // 24-bit 888 RGB
        field_adv = width * 3 + modulus * 4 - 4;
        first_addr_field1 = sof1;
        last_addr_field1 = sof1 +
            field_adv * (height - 1) + 3 * (width - 1);

        fb->addr_key = addr_key;

        fb->addr_first[0] = first_addr_field1;
        fb->addr_first[1] = first_addr_field1;
        fb->addr_last[0] = last_addr_field1;
        fb->addr_last[1] = last_addr_field1;

        fb_heap[idx].flags.fmt = FB_PIX_FMT_RGB_888;
        break;
    case 5:
        // 32-bit 0888 KRGB
        field_adv = (width * 4) + (modulus * 4) - 4;
        first_addr_field1 = sof1;
        last_addr_field1 = sof1 +
            field_adv * (height - 1) + 4 * (width - 1);

        fb->addr_key = addr_key;

        fb->addr_first[0] = first_addr_field1;
        fb->addr_first[1] = first_addr_field1;
        fb->addr_last[0] = last_addr_field1;
        fb->addr_last[1] = last_addr_field1;

        fb_heap[idx].flags.fmt = FB_PIX_FMT_0RGB_0888;
        break;
    case 6:
        // 32-bit 8888 ARGB
        field_adv = (width * 4) + (modulus * 4) - 4;
        first_addr_field1 = sof1;
        last_addr_field1 = sof1 +
            field_adv * (height - 1) + 4 * (width - 1);

        fb->addr_key = addr_key;

        fb->addr_first[0] = first_addr_field1;
        fb->addr_first[1] = first_addr_field1;
        fb->addr_last[0] = last_addr_field1;
        fb->addr_last[1] = last_addr_field1;

        // TODO: fix this part
        fb_heap[idx].flags.fmt = FB_PIX_FMT_ARGB_8888;
        break;
    }

    fb->tile_w = get_glob_tile_clip_x(pvr2) << 5;
    fb->tile_h = get_glob_tile_clip_y(pvr2) << 5;
    fb->x_clip_min = get_fb_x_clip_min(pvr2);
    fb->x_clip_max = get_fb_x_clip_max(pvr2);
    fb->y_clip_min = get_fb_y_clip_min(pvr2);
    fb->y_clip_max = get_fb_y_clip_max(pvr2);

    /*
     * It's safe to re-bind an object that is already bound as a render target
     * without first unbinding it.
     */
    struct gfx_il_inst cmd;
    cmd.op = GFX_IL_BIND_RENDER_TARGET;
    cmd.arg.bind_render_target.gfx_obj_handle = fb_heap[idx].obj_handle;
    rend_exec_il(&cmd, 1);

    return fb_heap[idx].obj_handle;
}

void framebuffer_get_render_target_dims(struct pvr2 *pvr2, int tgt,
                                        unsigned *width, unsigned *height) {
    struct framebuffer *fb = pvr2->fb.fb_heap + tgt;
    *width = fb->fb_read_width;
    *height = fb->fb_read_height;
}

static inline bool check_overlap(uint32_t range1_start, uint32_t range1_end,
                                 uint32_t range2_start, uint32_t range2_end) {
    if ((range1_start >= range2_start) && (range1_start <= range2_end))
        return true;
    if ((range1_end >= range2_start) && (range1_end <= range2_end))
        return true;
    if ((range2_start >= range1_start) && (range2_start <= range1_end))
        return true;
    if ((range2_end >= range1_start) && (range2_end <= range1_end))
        return true;
    return false;
}

void pvr2_framebuffer_notify_write(struct pvr2 *pvr2, uint32_t addr,
                                   unsigned n_bytes) {
    uint32_t first_byte = addr - ADDR_TEX32_FIRST;
    uint32_t last_byte = n_bytes - 1 + first_byte;

    unsigned fb_idx;
    struct framebuffer *fb_heap = pvr2->fb.fb_heap;
    for (fb_idx = 0; fb_idx < FB_HEAP_SIZE; fb_idx++) {
        /*
         * TODO: this overlap check is naive because it will issue a
         * false-postive in situations where the bytes written to fall between
         * the beginning and end of a field but aren't supposed to be part of
         * the field because the linestride would skip over them.  So far this
         * doesn't seem to be causing any troubles, but it is something to keep
         * in mind.
         */
        struct framebuffer *fb = fb_heap + fb_idx;
        if ((fb->flags.state & FB_STATE_GFX) &&
            (check_overlap(first_byte, last_byte,
                          fb->addr_first[0],
                          fb->addr_last[0]) ||
            check_overlap(first_byte, last_byte,
                          fb->addr_first[1],
                          fb->addr_last[1]))) {
            fb->flags.state = FB_STATE_VIRT;
        }
    }
}

void pvr2_framebuffer_notify_texture(struct pvr2 *pvr2, uint32_t first_tex_addr,
                                     uint32_t last_tex_addr) {
    first_tex_addr &= TEX_MIRROR_MASK;
    last_tex_addr &= TEX_MIRROR_MASK;

    int sync_count = 0;
    unsigned fb_idx;
    struct framebuffer *fb_heap = pvr2->fb.fb_heap;
    for (fb_idx = 0; fb_idx < FB_HEAP_SIZE; fb_idx++) {
        if (fb_heap[fb_idx].flags.state != FB_STATE_GFX)
            continue;

        uint32_t const *addr_first = fb_heap[fb_idx].addr_first;
        uint32_t const *addr_last = fb_heap[fb_idx].addr_last;

        uint32_t addr_first_total[2] = {
            addr_first[0] + ADDR_TEX32_FIRST,
            addr_first[1] + ADDR_TEX32_FIRST
        };

        uint32_t addr_last_total[2] = {
            addr_last[0] + ADDR_TEX32_FIRST,
            addr_last[1] + ADDR_TEX32_FIRST
        };

        uint32_t offs =  get_tex_mem_offs(addr_first_total[0]);

        if (get_tex_mem_offs(addr_first_total[1]) != offs ||
            get_tex_mem_offs(addr_last_total[0]) != offs ||
            get_tex_mem_offs(addr_last_total[1]) != offs) {
            error_set_feature("framebuffers that span the 32-bit and 64-bit "
                              "texture memory areas");
        }

        if (offs != ADDR_TEX64_FIRST)
            continue;

        if (check_overlap(first_tex_addr, last_tex_addr,
                          addr_first[0] & TEX_MIRROR_MASK, addr_last[0] & TEX_MIRROR_MASK) ||
            check_overlap(first_tex_addr, last_tex_addr,
                          addr_first[1] & TEX_MIRROR_MASK, addr_last[1] & TEX_MIRROR_MASK)) {
            sync_fb_to_tex_mem(pvr2, fb_heap + fb_idx);
            fb_heap[fb_idx].flags.state = FB_STATE_VIRT_AND_GFX;
            sync_count++;
        }
    }
}
