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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "pvr2_tex_mem.h"
#include "pvr2_core_reg.h"
#include "mem_areas.h"

#include "geo_buf.h"

#include "pvr2_tex_cache.h"

#define PVR2_CODE_BOOK_ENTRY_SIZE 8
#define PVR2_CODE_BOOK_ENTRY_COUNT 256
#define PVR2_CODE_BOOK_LEN (PVR2_CODE_BOOK_ENTRY_COUNT * \
                            PVR2_CODE_BOOK_ENTRY_SIZE)

unsigned static const pixel_sizes[TEX_CTRL_PIX_FMT_COUNT] = {
    [TEX_CTRL_PIX_FMT_ARGB_1555] = 2,
    [TEX_CTRL_PIX_FMT_RGB_565]   = 2,
    [TEX_CTRL_PIX_FMT_ARGB_4444] = 2,
    [TEX_CTRL_PIX_FMT_YUV_422]   = 1,
    [TEX_CTRL_PIX_FMT_BUMP_MAP]  = 0, // TODO: implement this
    [TEX_CTRL_PIX_FMT_4_BPP_PAL] = 0, // TODO: implement this
    [TEX_CTRL_PIX_FMT_8_BPP_PAL] = 1,
    [TEX_CTRL_PIX_FMT_INVALID]   = 0
};

static struct pvr2_tex tex_cache[PVR2_TEX_CACHE_SIZE];

static unsigned tex_twiddle(unsigned x, unsigned y,
                            unsigned w_shift, unsigned h_shift);

/*
 * maps from a normal row-major configuration to the
 * pvr2's own "twiddled" format.
 *
 * The twiddled format is a recursive way of ordering pixels in which the image
 * is divided up into four sub-images.  Those four subimages are stored in the
 * following order: upper-left, lower-left, upper-right, lower-right.  Each of
 * these subimages are themselves twiddled into four smaller subimages, and this
 * recursion continues until you reach the point where each subimage is a single
 * pixel.
 *
 * twiddled rectangular textures are stored as a series of squares each
 * with a width and height of min(w, h) (where w and h denote the width and
 * height of the full rectangular texture).
 *
 * Each one of these squares is twiddled internally, but the squares
 * themselves are stored in order from left to right (when width > height)
 * or from top to bottom (when height > width).
 */
static unsigned tex_twiddle(unsigned x, unsigned y, unsigned w_shift, unsigned h_shift) {
    unsigned twid_idx = 0;
    int quadrant_side_shift;

    unsigned w = 1 << w_shift;
    unsigned h = 1 << h_shift;
    unsigned min_square, min_square_shift;
    if (w < h) {
        min_square = w;
        min_square_shift = w_shift;
    } else {
        min_square = h;
        min_square_shift = h_shift;
    }

    unsigned x_resid = x & ~(min_square - 1);
    unsigned y_resid = y & ~(min_square - 1);
    unsigned x_sq = x & (min_square - 1);
    unsigned y_sq = y & (min_square - 1);

    unsigned shift;

    for (shift = 0; shift <= min_square_shift; shift++) {
        unsigned mask = (1 << (shift + 1)) - 1;
        unsigned pow = 1 << shift;

        if (shift <= min_square_shift) {
            if ((x_sq & mask) >= pow)
                twid_idx |= 1 << (shift * 2 + 1);
            if ((y_sq & mask) >= pow)
                twid_idx |= 1 << (shift * 2);
        }
    }

    if (x_resid) {
        twid_idx += (x / min_square) * min_square * min_square;
    } else if (y_resid) {
        twid_idx += (y / min_square) * min_square * min_square;
    }

    return twid_idx;
}

struct pvr2_tex *pvr2_tex_cache_find(uint32_t addr,
                                     unsigned w_shift, unsigned h_shift,
                                     int tex_fmt, bool twiddled,
                                     bool vq_compression) {
    unsigned idx;
    struct pvr2_tex *tex;
    for (idx = 0; idx < PVR2_TEX_CACHE_SIZE; idx++) {
        tex = tex_cache + idx;
        if (tex->valid && (tex->addr_first == addr) &&
            (tex->w_shift == w_shift) && (tex->h_shift == h_shift) &&
            (tex->tex_fmt == tex_fmt) && (tex->twiddled == twiddled) &&
            (tex->vq_compression == vq_compression)) {
            return tex;
        }
    }

    return NULL;
}

struct pvr2_tex *pvr2_tex_cache_add(uint32_t addr,
                                    unsigned w_shift, unsigned h_shift,
                                    int tex_fmt, bool twiddled,
                                    bool vq_compression) {
    assert(tex_fmt < TEX_CTRL_PIX_FMT_INVALID);

#ifdef INVARIANTS
    if (w_shift > 10 || h_shift > 10 || w_shift < 3 || h_shift < 3) {
        /*
         * this should not be possible because the width/height shifts are
         * taken from a 3-bit integer with +3 added, so the smallest possible
         * value is 3 and the largest is 10.
         */
        RAISE_ERROR(ERROR_INTEGRITY);
    }
#endif

    unsigned idx;// = addr & PVR2_TEX_CACHE_MASK;
    /* struct pvr2_tex *tex = tex_cache + idx; */
    struct pvr2_tex *tex;
    for (idx = 0; idx < PVR2_TEX_CACHE_SIZE; idx++) {
        tex = tex_cache + idx;
        if (!tex->valid)
            break;
    }

    if (idx >= PVR2_TEX_CACHE_SIZE) {
        // TODO: This is where we should evict an old texture
        fprintf(stderr, "ERROR: TEXTURE CACHE OVERFLOW\n");
        return NULL;
    }

    tex->addr_first = addr;
    tex->w_shift = w_shift;
    tex->h_shift = h_shift;
    tex->tex_fmt = tex_fmt;
    tex->twiddled = twiddled;
    tex->vq_compression = vq_compression;

    if (tex->vq_compression && (tex->w_shift != tex->h_shift)) {
        fprintf(stderr, "PVR2: WARNING - DISABLING VQ COMPRESSION FOR 0x%x "
                "DUE TO NON-SQUARE DIMENSIONS\n", (unsigned)tex->addr_first);
        tex->vq_compression = false;
    }

    if (tex->vq_compression) {
        unsigned side_len = 1 << w_shift;
        tex->addr_last = addr - 1 + PVR2_CODE_BOOK_LEN +
            sizeof(uint8_t) * side_len * side_len / 4;
    } else {
        tex->addr_last = addr - 1 + pixel_sizes[tex_fmt] *
            (1 << w_shift) * (1 << h_shift);
    }

    tex->valid = true;
    tex->dirty = true;
    /*
     * We defer reading the actual data from texture memory until we're ready
     * to transmit this to the rendering thread.
     */

    return tex;
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

void pvr2_tex_cache_notify_write(uint32_t addr_first, uint32_t len) {
    unsigned idx;
    uint32_t addr_last = addr_first + (len - 1);

    for (idx = 0; idx < PVR2_TEX_CACHE_SIZE; idx++) {
        struct pvr2_tex *tex = tex_cache + idx;
        if (tex->valid &&
            check_overlap(addr_first, addr_last,
                          tex->addr_first + ADDR_TEX64_FIRST,
                          tex->addr_last + ADDR_TEX64_FIRST)) {
            tex->dirty = true;
        }
    }
}

void pvr2_tex_cache_notify_palette_write(uint32_t addr_first, uint32_t len) {
    /*
     * TODO: all paletted textures will be invalidated whenever there is a
     * write to the PVR2's palette memory.  Obviously this is suboptimal
     * compared to only invalidating the textures that reference the part of
     * palette memory which is being written to, but I'm keeping it this way
     * for now because it is simpler.
     */
    pvr2_tex_cache_notify_palette_tp_change();
}

void pvr2_tex_cache_notify_palette_tp_change(void) {
    unsigned idx;
    for (idx = 0; idx < PVR2_TEX_CACHE_SIZE; idx++) {
        struct pvr2_tex *tex = tex_cache + idx;
        if (tex->valid && (tex->tex_fmt == TEX_CTRL_PIX_FMT_4_BPP_PAL ||
                           tex->tex_fmt == TEX_CTRL_PIX_FMT_8_BPP_PAL))
            tex->dirty = true;
    }
}

/*
 * de-twiddle src into dst.  Both src and dst must be preallocated buffers with
 * a length of (1 << tex_w_shift) * (1 << tex_h_shift) * bytes_per_pix.
 */
static void pvr2_tex_detwiddle(void *dst, void const *src,
                               unsigned tex_w_shift, unsigned tex_h_shift,
                               unsigned bytes_per_pix) {
    unsigned tex_w = 1 << tex_w_shift, tex_h = 1 << tex_h_shift;
    unsigned row, col;
    for (row = 0; row < tex_h; row++) {
        for (col = 0; col < tex_w; col++) {
            unsigned twid_idx = tex_twiddle(col, row, tex_w_shift, tex_h_shift);

#ifdef INVARIANTS
            if (twid_idx * bytes_per_pix >= tex_w * tex_h * bytes_per_pix)
                RAISE_ERROR(ERROR_INTEGRITY);
#endif

            memcpy(dst + (row * tex_w + col) * bytes_per_pix,
                   src + twid_idx * bytes_per_pix,
                   bytes_per_pix);
        }
    }
}

/*
 * decompress src into dst.
 *
 * src must be a VQ-encoded texture with a length of
 * PVR2_CODE_BOOK_LEN + (1 << side_shift) * (1 << side_shift) / 4.
 *
 * dst must be a buffer with a length of
 * 2 * (1 << side_shift) * (1 << side_shift) bytes.  This is because the data
 * will be uncompressed into dst, and because only 2-byte pixel formats are
 * supported (TEX_CTRL_PIX_FMT_ARGB_1555, TEX_CTRL_PIX_FMT_RGB_565,
 * TEX_CTRL_PIX_FMT_ARGB_4444).
 */
static void pvr2_tex_vq_decompress(void *dst, void const *src,
                                   unsigned side_shift) {
    unsigned dst_side = 1 << side_shift;
    unsigned src_side_shift = side_shift - 1;
    unsigned src_side = 1 << src_side_shift;
    unsigned row, col;
    for (row = 0; row < src_side; row++) {
        for (col = 0; col < src_side; col++) {
            unsigned twid_idx = tex_twiddle(col, row,
                                            src_side_shift, src_side_shift);

            // code book index
            unsigned idx =
                ((uint8_t*)src)[PVR2_CODE_BOOK_LEN + twid_idx];
            uint16_t color[4];
            memcpy(color, ((uint8_t*)src) + PVR2_CODE_BOOK_ENTRY_SIZE * idx,
                   PVR2_CODE_BOOK_ENTRY_SIZE);

            unsigned dst_row = row * 2, dst_col = col * 2;
            memcpy((uint16_t*)dst + dst_row * dst_side + dst_col,
                   color, sizeof(color[0]));
            memcpy((uint16_t*)dst + (dst_row + 1) * dst_side + dst_col,
                   color + 1, sizeof(color[1]));
            memcpy((uint16_t*)dst + dst_row * dst_side + dst_col + 1,
                   color + 2, sizeof(color[2]));
            memcpy((uint16_t*)dst + (dst_row + 1) * dst_side + dst_col + 1,
                   color + 3, sizeof(color[3]));
        }
    }
}

void pvr2_tex_cache_xmit(struct geo_buf *out) {
    unsigned idx;
    for (idx = 0; idx < PVR2_TEX_CACHE_SIZE; idx++) {
        struct pvr2_tex *tex_in = tex_cache + idx;
        struct pvr2_tex *tex_out = out->tex_cache + idx;
        unsigned tex_w = 1 << tex_in->w_shift,
            tex_h = 1 << tex_in->h_shift;

        if (tex_in->valid && tex_in->dirty) {
            tex_out->addr_first = tex_in->addr_first;
            tex_out->addr_last = tex_in->addr_last;
            tex_out->w_shift = tex_in->w_shift;
            tex_out->h_shift = tex_in->h_shift;
            tex_out->tex_fmt = tex_in->tex_fmt;
            tex_out->twiddled = tex_in->twiddled;
            tex_out->vq_compression = tex_in->vq_compression;

            // TODO: better error-handling
            if ((ADDR_TEX64_LAST - ADDR_TEX64_FIRST + 1) <=
                (tex_in->addr_last - tex_in->addr_first + 1)) {
                abort();
            }

            printf("tex_in->addr_first is 0x%08x\n", tex_in->addr_first);

            size_t n_bytes = sizeof(uint8_t) *
                (1 << tex_in->w_shift) * (1 << tex_in->h_shift) *
                pixel_sizes[tex_in->tex_fmt];

            void *tex_dat = NULL;
            if (n_bytes)
                tex_dat = malloc(n_bytes);

            if (!tex_dat)
                RAISE_ERROR(ERROR_INTEGRITY);

            // de-twiddle
            uint8_t const *beg = pvr2_tex64_mem + tex_in->addr_first;
            if (tex_in->vq_compression) {
                if (pixel_sizes[tex_in->tex_fmt] != 2) {
                    error_set_feature("proper response for an attempt to use "
                                      "VQ compression on a non-RGB texture");
                    RAISE_ERROR(ERROR_UNIMPLEMENTED);
                }

                if (tex_in->w_shift != tex_in->h_shift) {
                    error_set_feature("proper response for an attempt to use "
                                      "VQ compression on a non-square texture");
                    RAISE_ERROR(ERROR_UNIMPLEMENTED);
                }

                pvr2_tex_vq_decompress(tex_dat, beg, tex_in->w_shift);
            } else if (tex_in->twiddled) {
                pvr2_tex_detwiddle(tex_dat, beg,
                                   tex_in->w_shift, tex_in->h_shift,
                                   pixel_sizes[tex_in->tex_fmt]);
            } else {
                memcpy(tex_dat, beg,
                       pixel_sizes[tex_in->tex_fmt] * tex_w * tex_h);
            }

            if (tex_in->tex_fmt == TEX_CTRL_PIX_FMT_8_BPP_PAL) {
                uint32_t tex_size_actual;
                enum palette_tp palette_tp = get_palette_tp();
                switch (palette_tp) {
                case PALETTE_TP_ARGB_1555:
                    tex_out->pix_fmt = TEX_CTRL_PIX_FMT_ARGB_1555;
                    tex_size_actual = 2;
                    break;
                case PALETTE_TP_RGB_565:
                    tex_out->pix_fmt = TEX_CTRL_PIX_FMT_RGB_565;
                    tex_size_actual = 2;
                    break;
                case PALETTE_TP_ARGB_4444:
                    tex_out->pix_fmt = TEX_CTRL_PIX_FMT_ARGB_4444;
                    tex_size_actual = 2;
                    break;
                case PALETTE_TP_ARGB_8888:
                    tex_size_actual = 4;
                    RAISE_ERROR(ERROR_UNIMPLEMENTED);
                    break;
                default:
                    RAISE_ERROR(ERROR_INTEGRITY);
                }
                uint8_t *tex_dat_no_palette =
                    (uint8_t*)malloc(tex_size_actual * tex_w * tex_h);
                if (!tex_dat_no_palette)
                    RAISE_ERROR(ERROR_FAILED_ALLOC);

                uint32_t pal_start = (tex_out->tex_palette_start & 0x30) << 4;
                uint8_t const *tex_dat8 = (uint8_t const*)tex_dat;

                unsigned row, col;
                for (row = 0; row < tex_h; row++) {
                    for (col = 0; col < tex_w; col++) {
                        unsigned pix_idx = row * tex_w + col;
                        uint8_t *pix_out = tex_dat_no_palette +
                            pix_idx * tex_size_actual;
                        uint8_t pix_in = tex_dat8[pix_idx];
                        uint32_t palette_addr =
                            (pal_start | (uint32_t)pix_in) * 4;
                        memcpy(pix_out, pvr2_palette_ram + palette_addr, tex_size_actual);
                    }
                }
                tex_out->dat = tex_dat_no_palette;
                free(tex_dat);
                printf("PVR2 paletted texture: tex_palette_start is 0x%04x\n",
                       (unsigned)tex_out->tex_palette_start);
            } else if (tex_out->tex_fmt == TEX_CTRL_PIX_FMT_4_BPP_PAL) {
                error_set_feature("4BPP paletted textures");
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
            } else {
                tex_out->pix_fmt = tex_in->tex_fmt;
                tex_out->dat = tex_dat;
            }

            tex_in->dirty = false;
            tex_out->dirty = true;
            tex_out->valid = true;
        }
    }
}

int pvr2_tex_cache_get_idx(struct pvr2_tex const *tex) {
    return tex - tex_cache;
}
