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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "pvr2.h"
#include "pvr2_tex_mem.h"
#include "pvr2_gfx_obj.h"
#include "framebuffer.h"
#include "mem_areas.h"
#include "log.h"
#include "washdc/error.h"
#include "gfx/gfx_il.h"
#include "gfx/gfx_tex_cache.h"
#include "dreamcast.h"
#include "pvr2_reg.h"

#include "pvr2_tex_cache.h"

static DEF_ERROR_INT_ATTR(tex_fmt);

#define PVR2_CODE_BOOK_ENTRY_SIZE (4 * sizeof(uint16_t))
#define PVR2_CODE_BOOK_ENTRY_COUNT 256
#define PVR2_CODE_BOOK_LEN (PVR2_CODE_BOOK_ENTRY_COUNT * \
                            PVR2_CODE_BOOK_ENTRY_SIZE)

static enum gfx_tex_fmt pvr2_tex_fmt_to_gfx(enum TexCtrlPixFmt in_fmt);

unsigned static const pixel_sizes[TEX_CTRL_PIX_FMT_COUNT] = {
    [TEX_CTRL_PIX_FMT_ARGB_1555] = 2,
    [TEX_CTRL_PIX_FMT_RGB_565]   = 2,
    [TEX_CTRL_PIX_FMT_ARGB_4444] = 2,
    [TEX_CTRL_PIX_FMT_YUV_422]   = 2,
    [TEX_CTRL_PIX_FMT_BUMP_MAP]  = 0, // TODO: implement this
    [TEX_CTRL_PIX_FMT_4_BPP_PAL] = 0,
    [TEX_CTRL_PIX_FMT_8_BPP_PAL] = 1,
    [TEX_CTRL_PIX_FMT_INVALID]   = 0
};

// byte offsets for mipmaps for 4bpp and 8bpp paletted textures
static unsigned const mipmap_byte_offset_palette[11] = {
    0x3, 0x4, 0x8, 0x18, 0x58, 0x158, 0x558, 0x1558, 0x5558, 0x15558, 0x55558
};

// byte offsets for mipmaps for VQ textures
static unsigned const mipmap_byte_offset_vq[11] = {
    0x0, 0x1, 0x2, 0x6, 0x16, 0x56, 0x156, 0x556, 0x1556, 0x5556, 0x15556
};

// byte offsets for mipmaps for "normal" textures
static unsigned const mipmap_byte_offset_norm[11] = {
    0x6, 0x8, 0x10, 0x30, 0xb0, 0x2b0, 0xab0, 0x2ab0, 0xaab0, 0x2aab0, 0xaaab0
};

static bool pvr2_tex_valid(enum pvr2_tex_state state) {
    return state == PVR2_TEX_READY || state == PVR2_TEX_DIRTY;
}

static unsigned tex_twiddle(unsigned x, unsigned y,
                            unsigned w_shift, unsigned h_shift);

static enum gfx_tex_fmt
translate_palette_to_pix_format(enum palette_tp palette_tp);

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

        if ((x_sq & mask) >= pow)
            twid_idx |= 1 << (shift * 2 + 1);
        if ((y_sq & mask) >= pow)
            twid_idx |= 1 << (shift * 2);
    }

    if (x_resid) {
        twid_idx += (x / min_square) * min_square * min_square;
    } else if (y_resid) {
        twid_idx += (y / min_square) * min_square * min_square;
    }

    return twid_idx;
}

void pvr2_tex_cache_init(struct pvr2 *pvr2) {
    struct pvr2_tex_cache *cache = &pvr2->tex_cache;

    unsigned idx;
    for (idx = 0; idx < PVR2_TEX_CACHE_SIZE; idx++) {
        memset(cache->tex_cache + idx, 0, sizeof(cache->tex_cache[idx]));
        cache->tex_cache[idx].obj_no = -1;
    }

    memset(cache->page_stamps, 0, sizeof(cache->page_stamps));
}

void pvr2_tex_cache_cleanup(struct pvr2 *pvr2) {
    struct pvr2_tex_cache *cache = &pvr2->tex_cache;

    unsigned idx;
    for (idx = 0; idx < PVR2_TEX_CACHE_SIZE; idx++)
        if (cache->tex_cache[idx].obj_no >= 0)
            pvr2_free_gfx_obj(cache->tex_cache[idx].obj_no);
}

struct pvr2_tex *pvr2_tex_cache_find(struct pvr2 *pvr2,
                                     uint32_t addr, uint32_t pal_addr,
                                     unsigned w_shift, unsigned h_shift,
                                     int tex_fmt, bool twiddled,
                                     bool vq_compression, bool mipmap,
                                     bool stride_sel) {
    unsigned idx;
    struct pvr2_tex *tex;
    bool pal_tex =
        tex_fmt == TEX_CTRL_PIX_FMT_4_BPP_PAL ||
        tex_fmt == TEX_CTRL_PIX_FMT_8_BPP_PAL;
    struct pvr2_tex *tex_cache = pvr2->tex_cache.tex_cache;

    for (idx = 0; idx < PVR2_TEX_CACHE_SIZE; idx++) {
        tex = tex_cache + idx;
        if (pvr2_tex_valid(tex->state) && (tex->meta.addr_first == addr) &&
            (tex->meta.w_shift == w_shift) && (tex->meta.h_shift == h_shift) &&
            (tex->meta.tex_fmt == tex_fmt) && (tex->meta.twiddled == twiddled) &&
            (tex->meta.vq_compression == vq_compression) &&
            (mipmap == tex->meta.mipmap) &&
            (!pal_tex || pal_addr == tex->meta.tex_palette_start)) {
            tex->frame_stamp_last_used = get_cur_frame_stamp(pvr2);
            return tex;
        }
    }

    return NULL;
}

struct pvr2_tex *pvr2_tex_cache_add(struct pvr2 *pvr2,
                                    uint32_t addr, uint32_t pal_addr,
                                    unsigned w_shift, unsigned h_shift,
                                    int tex_fmt, bool twiddled,
                                    bool vq_compression, bool mipmap,
                                    bool stride_sel) {
    assert(tex_fmt < TEX_CTRL_PIX_FMT_INVALID);
    unsigned cur_frame_stamp = get_cur_frame_stamp(pvr2);

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
    struct pvr2_tex *tex_cache = pvr2->tex_cache.tex_cache;
    struct pvr2_tex *tex, *oldest_tex = NULL;
    for (idx = 0; idx < PVR2_TEX_CACHE_SIZE; idx++) {
        tex = tex_cache + idx;

        if (!pvr2_tex_valid(tex->state))
            break;

        if (tex->frame_stamp_last_used < cur_frame_stamp) {
            if (!oldest_tex ||
                tex->frame_stamp_last_used < oldest_tex->frame_stamp_last_used)
                oldest_tex = tex;
        }
    }

    if (idx >= PVR2_TEX_CACHE_SIZE) {
        // kick the oldest tex out of the cache to make room
        if (oldest_tex) {
            tex = oldest_tex;
        } else {
            LOG_ERROR("ERROR: TEXTURE CACHE OVERFLOW\n");
            return NULL;
        }

        if (tex->obj_no >= 0) {
            struct gfx_il_inst cmd;
            cmd.op = GFX_IL_FREE_OBJ;
            cmd.arg.free_obj.obj_no = tex->obj_no;
            rend_exec_il(&cmd, 1);
            pvr2_free_gfx_obj(tex->obj_no);
        }
    }

    tex->meta.addr_first = addr;
    tex->meta.w_shift = w_shift;
    tex->meta.h_shift = h_shift;
    tex->meta.tex_fmt = tex_fmt;
    tex->meta.twiddled = twiddled;
    tex->meta.vq_compression = vq_compression;
    tex->meta.mipmap = mipmap;
    tex->meta.stride_sel = stride_sel;
    tex->meta.tex_palette_start = pal_addr;
    tex->frame_stamp_last_used = cur_frame_stamp;
    tex->obj_no = -1;

    if (tex_fmt != TEX_CTRL_PIX_FMT_4_BPP_PAL &&
        tex_fmt != TEX_CTRL_PIX_FMT_8_BPP_PAL) {
        tex->meta.pix_fmt = pvr2_tex_fmt_to_gfx(tex_fmt);
    } else {
        tex->meta.pix_fmt = translate_palette_to_pix_format(get_palette_tp(pvr2));
    }

    if (tex->meta.vq_compression && (tex->meta.w_shift != tex->meta.h_shift)) {
        LOG_WARN("PVR2: WARNING - DISABLING VQ COMPRESSION FOR 0x%x "
                 "DUE TO NON-SQUARE DIMENSIONS\n",
                 (unsigned)tex->meta.addr_first);
        tex->meta.vq_compression = false;
    }

    if (tex->meta.vq_compression) {
        unsigned side_len = 1 << w_shift;
        tex->meta.addr_last = addr - 1 + PVR2_CODE_BOOK_LEN +
            sizeof(uint8_t) * side_len * side_len / 4;
    } else {
        if (tex_fmt == TEX_CTRL_PIX_FMT_4_BPP_PAL) {
            tex->meta.addr_last = addr - 1 +
                ((1 << w_shift) * (1 << h_shift)) / 2;
        } else {
            tex->meta.addr_last = addr - 1 + pixel_sizes[tex_fmt] *
                (1 << w_shift) * (1 << h_shift);
        }
        if (tex->meta.mipmap) {
            if (tex->meta.w_shift != tex->meta.h_shift) {
                error_set_feature("proper response for attempt to enable "
                                  "mipmaps on a rectangular texture");
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
            }
            if (tex->meta.vq_compression) {
                tex->meta.addr_last += mipmap_byte_offset_vq[w_shift];
            } else {
                switch (tex->meta.tex_fmt) {
                case TEX_CTRL_PIX_FMT_ARGB_1555:
                case TEX_CTRL_PIX_FMT_RGB_565:
                case TEX_CTRL_PIX_FMT_YUV_422:
                case TEX_CTRL_PIX_FMT_ARGB_4444:
                    tex->meta.addr_last += mipmap_byte_offset_norm[w_shift];
                    break;
                case TEX_CTRL_PIX_FMT_4_BPP_PAL:
                case TEX_CTRL_PIX_FMT_8_BPP_PAL:
                    tex->meta.addr_last += mipmap_byte_offset_palette[w_shift];
                    break;
                default:
                case TEX_CTRL_PIX_FMT_BUMP_MAP:
                    RAISE_ERROR(ERROR_UNIMPLEMENTED);
                }
            }
        }
    }

    tex->state = PVR2_TEX_DIRTY;
    tex->last_update = 0;
    /*
     * We defer reading the actual data from texture memory until we're ready
     * to transmit this to the rendering thread.
     */

    return tex;
}

void pvr2_tex_cache_notify_write(struct pvr2 *pvr2,
                                 uint32_t addr_first, uint32_t len) {
#ifdef INVARIANTS
    uint32_t addr_last_abs = addr_first + (len - 1);
    if (addr_first < ADDR_TEX64_FIRST || addr_first > ADDR_TEX64_LAST ||
        addr_last_abs < ADDR_TEX64_FIRST || addr_last_abs > ADDR_TEX64_LAST) {
        error_set_address(addr_first);
        error_set_length(len);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
#endif
    addr_first -= ADDR_TEX64_FIRST;
    uint32_t addr_last = addr_first + (len - 1);
    unsigned page_first = addr_first / PVR2_TEX_PAGE_SIZE;
    unsigned page_last = addr_last / PVR2_TEX_PAGE_SIZE;
    dc_cycle_stamp_t time = clock_cycle_stamp(pvr2->clk);
    struct pvr2_tex_cache *cache = &pvr2->tex_cache;
    dc_cycle_stamp_t *page_stamps = cache->page_stamps;

    unsigned page_no;
    for (page_no = page_first; page_no <= page_last; page_no++)
        page_stamps[page_no] = time;
}

void
pvr2_tex_cache_notify_palette_write(struct pvr2 *pvr2,
                                    uint32_t addr_first, uint32_t len) {
    /*
     * TODO: all paletted textures will be invalidated whenever there is a
     * write to the PVR2's palette memory.  Obviously this is suboptimal
     * compared to only invalidating the textures that reference the part of
     * palette memory which is being written to, but I'm keeping it this way
     * for now because it is simpler.
     */
    pvr2_tex_cache_notify_palette_tp_change(pvr2);
}

void pvr2_tex_cache_notify_palette_tp_change(struct pvr2 *pvr2) {
    unsigned idx;
    struct pvr2_tex *tex_cache = pvr2->tex_cache.tex_cache;
    for (idx = 0; idx < PVR2_TEX_CACHE_SIZE; idx++) {
        struct pvr2_tex *tex = tex_cache + idx;
        if (tex->state == PVR2_TEX_READY &&
            (tex->meta.tex_fmt == TEX_CTRL_PIX_FMT_4_BPP_PAL ||
             tex->meta.tex_fmt == TEX_CTRL_PIX_FMT_8_BPP_PAL))
            tex->state = PVR2_TEX_DIRTY;
    }
}

/*
 * de-twiddle src into dst.  Both src and dst must be preallocated buffers with
 * a length of (1 << tex_w_shift) * (1 << tex_h_shift) * bytes_per_pix.
 */
static void pvr2_tex_detwiddle(void *dst, void const *src,
                               unsigned tex_w_shift, unsigned tex_h_shift,
                               unsigned bytes_per_pix) {
    uint8_t *dst8 = (uint8_t*)dst;
    unsigned tex_w = 1 << tex_w_shift, tex_h = 1 << tex_h_shift;
    unsigned row, col;
    for (row = 0; row < tex_h; row++) {
        for (col = 0; col < tex_w; col++) {
            unsigned twid_idx = tex_twiddle(col, row, tex_w_shift, tex_h_shift);

#ifdef INVARIANTS
            if (twid_idx >= tex_w * tex_h)
                RAISE_ERROR(ERROR_INTEGRITY);
#endif

            memcpy(dst8 + (row * tex_w + col) * bytes_per_pix,
                   src + twid_idx * bytes_per_pix,
                   bytes_per_pix);
        }
    }
}

/*
 * special version of pvr2_tex_detwiddle for 4bpp paletted textures.
 *
 * The normal version of pvr2_tex_detwiddle won't work since each byte contains
 * two packed pixels.
 */
static void pvr2_tex_detwiddle_4bpp(void *dst, void const *src,
                                    unsigned tex_w_shift, unsigned tex_h_shift) {
    uint8_t *dst8 = (uint8_t*)dst;
    uint8_t const *src8 = (uint8_t*)src;
    unsigned tex_w = 1 << tex_w_shift, tex_h = 1 << tex_h_shift;
    unsigned row, col;
    for (row = 0; row < tex_h; row++) {
        for (col = 0; col < tex_w; col++) {
            unsigned twid_idx = tex_twiddle(col, row, tex_w_shift, tex_h_shift);

#ifdef INVARIANTS
            if (twid_idx >= tex_w * tex_h)
                RAISE_ERROR(ERROR_INTEGRITY);
#endif
            unsigned dst_idx = row * tex_w + col;

            uint8_t in_px;
            if (twid_idx % 2 == 0)
                in_px = src8[twid_idx / 2] & 0xf;
            else
                in_px = src8[twid_idx / 2] >> 4;

            if (dst_idx % 2 == 0) {
                dst8[dst_idx / 2] &= ~0xf;
                dst8[dst_idx / 2] |= in_px;
            } else {
                dst8[dst_idx / 2] &= ~0xf0;
                dst8[dst_idx / 2] |= (in_px << 4);
            }
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
static void pvr2_tex_vq_decompress(void *dst, void const *code_book,
                                   void const *src, unsigned side_shift) {
    unsigned dst_side = 1 << side_shift;
    unsigned src_side_shift = side_shift - 1;
    unsigned src_side = 1 << src_side_shift;
    unsigned row, col;
    uint8_t const *code_book_src = (uint8_t const*)code_book;
    uint8_t const *img_dat_src = (uint8_t const*)src;
    uint16_t *dst_img = (uint16_t*)dst;

    for (row = 0; row < src_side; row++) {
        for (col = 0; col < src_side; col++) {
            unsigned twid_idx = tex_twiddle(col, row,
                                            src_side_shift, src_side_shift);

            // code book index
            unsigned idx = img_dat_src[twid_idx];
            uint16_t color[4];
            memcpy(color, code_book_src + PVR2_CODE_BOOK_ENTRY_SIZE * idx,
                   PVR2_CODE_BOOK_ENTRY_SIZE);

            unsigned dst_row = row * 2, dst_col = col * 2;
            memcpy(dst_img + dst_row * dst_side + dst_col,
                   color, sizeof(color[0]));
            memcpy(dst_img + (dst_row + 1) * dst_side + dst_col,
                   color + 1, sizeof(color[1]));
            memcpy(dst_img + dst_row * dst_side + dst_col + 1,
                   color + 2, sizeof(color[2]));
            memcpy(dst_img + (dst_row + 1) * dst_side + dst_col + 1,
                   color + 3, sizeof(color[3]));
        }
    }
}

void pvr2_tex_cache_read(struct pvr2 *pvr2,
                         void **tex_dat_out, size_t *n_bytes_out,
                         struct pvr2_tex_meta const *meta) {
    unsigned tex_w = 1 << meta->w_shift, tex_h = 1 << meta->h_shift;

    // TODO: better error-handling
    if ((ADDR_TEX64_LAST - ADDR_TEX64_FIRST + 1) <=
        (meta->addr_last - meta->addr_first + 1)) {
        abort();
    }

    size_t n_bytes;

    if (meta->tex_fmt == TEX_CTRL_PIX_FMT_4_BPP_PAL) {
        n_bytes = (tex_w * tex_h) / 2;
    } else {
        unsigned px_sz = pixel_sizes[meta->tex_fmt];
        if (!px_sz) {
            error_set_tex_fmt(meta->tex_fmt);
            error_set_feature("some texture format");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
        n_bytes = tex_w * tex_h * px_sz;
    }

    void *tex_dat = NULL;
    if (n_bytes)
        tex_dat = malloc(n_bytes * sizeof(uint8_t));

    if (!tex_dat)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    uint8_t const *beg;
    uint8_t const *code_book; // points to the code book if this is VQ

    /*
     * handle mipmaps.
     *
     * Currently they're not actually implemented, I just select the
     * highest-order mipmap (which is the one at the end).  To
     * accomplish this, we have to offset the addr_first by the offset
     * to the highest-order mipmap.
     */
    uint8_t const *pvr2_tex64_mem = pvr2->mem.tex64;
    if (meta->mipmap) {
        if (meta->w_shift != meta->h_shift) {
            error_set_feature("proper response for attempting to "
                              "enable mipmapping on a rectangular "
                              "texture");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        if (meta->tex_fmt == TEX_CTRL_PIX_FMT_YUV_422) {
            error_set_feature("mipmapped YUV422 textures\n");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        unsigned side_shift = meta->w_shift;

        if (meta->vq_compression) {
            code_book = pvr2_tex64_mem + meta->addr_first;
            beg = code_book + PVR2_CODE_BOOK_LEN +
                mipmap_byte_offset_vq[side_shift];
        } else {
            switch (meta->tex_fmt) {
            case TEX_CTRL_PIX_FMT_ARGB_1555:
            case TEX_CTRL_PIX_FMT_RGB_565:
            case TEX_CTRL_PIX_FMT_YUV_422:
            case TEX_CTRL_PIX_FMT_ARGB_4444:
                beg = pvr2_tex64_mem + meta->addr_first +
                    mipmap_byte_offset_norm[meta->w_shift];
                break;
            case TEX_CTRL_PIX_FMT_4_BPP_PAL:
            case TEX_CTRL_PIX_FMT_8_BPP_PAL:
                beg = pvr2_tex64_mem + meta->addr_first +
                    mipmap_byte_offset_palette[meta->w_shift];
                break;
            default:
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
            }
        }
    } else {
        /*
         * mipmaps are disabled, tex_in->addr_first is actually the
         * first byte of the texture.
         */
        beg = pvr2_tex64_mem + meta->addr_first;
        if (meta->vq_compression) {
            code_book = beg;
            beg += PVR2_CODE_BOOK_LEN;
        }
    }

    if (meta->vq_compression) {
        if (meta->tex_fmt == TEX_CTRL_PIX_FMT_4_BPP_PAL ||
            meta->tex_fmt == TEX_CTRL_PIX_FMT_8_BPP_PAL) {
            /*
             * 4BPP paletted VQ textures store 4x4 blocks in the code-book
             * instead of 2x2.  8BPP paletted VQ textures store 2x4 blocks
             * in the code-book.
             */
            error_set_feature("VQ compressed palette textures");
            error_set_tex_fmt(meta->tex_fmt);
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        if (meta->tex_fmt == TEX_CTRL_PIX_FMT_YUV_422) {
            error_set_feature("VQ-compressed YUV422 textures\n");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        if (pixel_sizes[meta->tex_fmt] != 2) {
            error_set_tex_fmt(meta->tex_fmt);
            error_set_feature("proper response for an attempt to use "
                              "VQ compression on a non-RGB texture");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        if (meta->w_shift != meta->h_shift) {
            error_set_feature("proper response for an attempt to use "
                              "VQ compression on a non-square texture");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        pvr2_tex_vq_decompress(tex_dat, code_book, beg, meta->w_shift);
    } else if (meta->twiddled) {
        if (meta->tex_fmt == TEX_CTRL_PIX_FMT_4_BPP_PAL) {
            pvr2_tex_detwiddle_4bpp(tex_dat, beg, meta->w_shift, meta->h_shift);
        } else {
            pvr2_tex_detwiddle(tex_dat, beg,
                               meta->w_shift, meta->h_shift,
                               pixel_sizes[meta->tex_fmt]);
        }
    } else {
        memcpy(tex_dat, beg, n_bytes * sizeof(uint8_t));
    }

    if (meta->tex_fmt == TEX_CTRL_PIX_FMT_8_BPP_PAL) {
        uint32_t tex_size_actual;
        enum palette_tp palette_tp = get_palette_tp(pvr2);
        switch (palette_tp) {
        case PALETTE_TP_ARGB_1555:
            tex_size_actual = 2;
            break;
        case PALETTE_TP_RGB_565:
            tex_size_actual = 2;
            break;
        case PALETTE_TP_ARGB_4444:
            tex_size_actual = 2;
            break;
        case PALETTE_TP_ARGB_8888:
            tex_size_actual = 4;
            break;
        default:
            RAISE_ERROR(ERROR_INTEGRITY);
        }
        n_bytes = tex_size_actual * tex_w * tex_h;
        uint8_t *tex_dat_no_palette =
            (uint8_t*)malloc(n_bytes);
        if (!tex_dat_no_palette)
            RAISE_ERROR(ERROR_FAILED_ALLOC);

        uint32_t pal_start = (meta->tex_palette_start & 0x30) << 4;
        uint8_t const *tex_dat8 = (uint8_t const*)tex_dat;

        unsigned row, col;
        uint8_t *pal_ram = pvr2_get_palette_ram(pvr2);
        for (row = 0; row < tex_h; row++) {
            for (col = 0; col < tex_w; col++) {
                unsigned pix_idx = row * tex_w + col;
                uint8_t *pix_out = tex_dat_no_palette +
                    pix_idx * tex_size_actual;
                uint8_t pix_in = tex_dat8[pix_idx];
                uint32_t palette_addr =
                    (pal_start | (uint32_t)pix_in) * 4;
                memcpy(pix_out, pal_ram + palette_addr, tex_size_actual);
            }
        }
        free(tex_dat);
        tex_dat = tex_dat_no_palette;
        LOG_DBG("PVR2 paletted texture: tex_palette_start is 0x%04x\n",
               (unsigned)meta->tex_palette_start);
    } else if (meta->tex_fmt == TEX_CTRL_PIX_FMT_4_BPP_PAL) {
        uint32_t tex_size_actual;
        enum palette_tp palette_tp = get_palette_tp(pvr2);
        switch (palette_tp) {
        case PALETTE_TP_ARGB_1555:
            tex_size_actual = 2;
            break;
        case PALETTE_TP_RGB_565:
            tex_size_actual = 2;
            break;
        case PALETTE_TP_ARGB_4444:
            tex_size_actual = 2;
            break;
        case PALETTE_TP_ARGB_8888:
            tex_size_actual = 4;
            break;
        default:
            RAISE_ERROR(ERROR_INTEGRITY);
        }
        n_bytes = tex_size_actual * tex_w * tex_h;
        uint8_t *tex_dat_no_palette =
            (uint8_t*)malloc(n_bytes);
        if (!tex_dat_no_palette)
            RAISE_ERROR(ERROR_FAILED_ALLOC);

        uint32_t pal_start = meta->tex_palette_start << 4;
        uint8_t const *tex_dat8 = (uint8_t const*)tex_dat;

        uint8_t *pal_ram = pvr2_get_palette_ram(pvr2);
        unsigned row, col;
        for (row = 0; row < tex_h; row++) {
            for (col = 0; col < tex_w; col++) {
                unsigned pix_idx = row * tex_w + col;

                uint8_t *pix_out = tex_dat_no_palette +
                    pix_idx * tex_size_actual;
                uint8_t pix_in;

                if (pix_idx % 2 == 0)
                    pix_in = tex_dat8[pix_idx / 2] & 0xf;
                else
                    pix_in = tex_dat8[pix_idx / 2] >> 4;

                uint32_t palette_addr =
                    (pal_start | (uint32_t)pix_in) * 4;
                memcpy(pix_out, pal_ram + palette_addr, tex_size_actual);
            }
        }
        free(tex_dat);
        tex_dat = tex_dat_no_palette;
        LOG_DBG("PVR2 paletted texture: tex_palette_start is 0x%04x\n",
               (unsigned)meta->tex_palette_start);
    }

    *tex_dat_out = tex_dat;
    *n_bytes_out = n_bytes;
}

void pvr2_tex_cache_xmit(struct pvr2 *pvr2) {
    unsigned idx;
    unsigned cur_frame_stamp = get_cur_frame_stamp(pvr2);
    struct gfx_il_inst cmd;
    struct pvr2_tex_cache *cache = &pvr2->tex_cache;
    dc_cycle_stamp_t *page_stamps = cache->page_stamps;
    struct pvr2_tex *tex_cache = pvr2->tex_cache.tex_cache;

    for (idx = 0; idx < PVR2_TEX_CACHE_SIZE; idx++) {
        struct pvr2_tex *tex_in = tex_cache + idx;

        if (tex_in->state != PVR2_TEX_INVALID) {
            pvr2_framebuffer_notify_texture(pvr2,
                                            tex_in->meta.addr_first +
                                            ADDR_TEX64_FIRST,
                                            tex_in->meta.addr_last +
                                            ADDR_TEX64_FIRST);
        }

        bool need_update = false;
        if (tex_in->state == PVR2_TEX_DIRTY)
            need_update = true;
        else if (tex_in->state == PVR2_TEX_READY) {
            unsigned page = tex_in->meta.addr_first / PVR2_TEX_PAGE_SIZE;
            unsigned last_page = tex_in->meta.addr_last / PVR2_TEX_PAGE_SIZE;
            while (page <= last_page) {
                if (page_stamps[page++] > tex_in->last_update) {
                    need_update = true;
                    break;
                }
            }
        }

        if (need_update) {
            /*
             * If the texture has been written to this frame but it is not
             * actively in use then tell the gfx system to evict it from the
             * cache.
             */
            if (tex_in->frame_stamp_last_used != cur_frame_stamp) {
                tex_in->state = PVR2_TEX_INVALID;

                cmd.op = GFX_IL_UNBIND_TEX;
                cmd.arg.unbind_tex.tex_no = idx;
                rend_exec_il(&cmd, 1);

                cmd.op = GFX_IL_FREE_OBJ;
                cmd.arg.free_obj.obj_no = tex_in->obj_no;
                rend_exec_il(&cmd, 1);

                pvr2_free_gfx_obj(tex_in->obj_no);
                tex_in->obj_no = -1;

                continue;
            }

            if (tex_in->obj_no < 0) {
                /*
                 * This is a new texture; we need to create a data store,
                 * upload the texture and bind the store to the texture object.
                 */
                tex_in->obj_no = pvr2_alloc_gfx_obj();

                void *tex_dat;
                size_t n_bytes;
                struct pvr2_tex_meta tmp = tex_in->meta;
                if (tex_in->meta.tex_fmt == TEX_CTRL_PIX_FMT_8_BPP_PAL ||
                    tex_in->meta.tex_fmt == TEX_CTRL_PIX_FMT_4_BPP_PAL) {
                    tmp.pix_fmt =
                        translate_palette_to_pix_format(get_palette_tp(pvr2));
                }
                pvr2_tex_cache_read(pvr2, &tex_dat, &n_bytes, &tmp);

                cmd.op = GFX_IL_INIT_OBJ;
                cmd.arg.init_obj.obj_no = tex_in->obj_no;
                cmd.arg.init_obj.n_bytes = n_bytes;
                rend_exec_il(&cmd, 1);

                cmd.op = GFX_IL_WRITE_OBJ;
                cmd.arg.write_obj.dat = tex_dat;
                cmd.arg.write_obj.obj_no = tex_in->obj_no;
                cmd.arg.write_obj.n_bytes = n_bytes;
                rend_exec_il(&cmd, 1);
                free(tex_dat);

                cmd.op = GFX_IL_BIND_TEX;
                cmd.arg.bind_tex.gfx_obj_handle = tex_in->obj_no;
                cmd.arg.bind_tex.tex_no = idx;
                cmd.arg.bind_tex.pix_fmt = tmp.pix_fmt;
                cmd.arg.bind_tex.width = 1 << tex_in->meta.w_shift;
                cmd.arg.bind_tex.height = 1 << tex_in->meta.h_shift;

                rend_exec_il(&cmd, 1);
            } else {
                /*
                 * This is a pre-existing texture; since the data-store has
                 * already been created and bound, all we have to do is write
                 * to it.
                 */
                struct pvr2_tex_meta tmp = tex_in->meta;
                if (tex_in->meta.tex_fmt == TEX_CTRL_PIX_FMT_8_BPP_PAL ||
                    tex_in->meta.tex_fmt == TEX_CTRL_PIX_FMT_4_BPP_PAL) {
                    tmp.pix_fmt = translate_palette_to_pix_format(get_palette_tp(pvr2));
                }
                void *tex_dat;
                size_t n_bytes;
                pvr2_tex_cache_read(pvr2, &tex_dat, &n_bytes, &tmp);
                cmd.op = GFX_IL_WRITE_OBJ;
                cmd.arg.write_obj.dat = tex_dat;
                cmd.arg.write_obj.obj_no = tex_in->obj_no;
                cmd.arg.write_obj.n_bytes = n_bytes;
                rend_exec_il(&cmd, 1);
                free(tex_dat);
            }

            tex_in->state = PVR2_TEX_READY;
            tex_in->last_update = clock_cycle_stamp(pvr2->clk);
        }
    }
}

int pvr2_tex_cache_get_idx(struct pvr2 *pvr2, struct pvr2_tex const *tex) {
    return tex - pvr2->tex_cache.tex_cache;
}

int pvr2_tex_get_meta(struct pvr2 *pvr2,
                      struct pvr2_tex_meta *meta, unsigned tex_idx) {
    struct pvr2_tex const *tex_in = pvr2->tex_cache.tex_cache + tex_idx;
    if (pvr2_tex_valid(tex_in->state)) {
        memcpy(meta, &tex_in->meta, sizeof(struct pvr2_tex_meta));
        return 0;
    }
    return -1;
}

static enum gfx_tex_fmt pvr2_tex_fmt_to_gfx(enum TexCtrlPixFmt in_fmt) {
    switch (in_fmt) {
    case TEX_CTRL_PIX_FMT_ARGB_1555:
        return GFX_TEX_FMT_ARGB_1555;
    case TEX_CTRL_PIX_FMT_RGB_565:
        return GFX_TEX_FMT_RGB_565;
    case TEX_CTRL_PIX_FMT_ARGB_4444:
        return GFX_TEX_FMT_ARGB_4444;
    case TEX_CTRL_PIX_FMT_YUV_422:
        return GFX_TEX_FMT_YUV_422;
    case TEX_CTRL_PIX_FMT_4_BPP_PAL:
    case TEX_CTRL_PIX_FMT_8_BPP_PAL:
        /*
         * for paletted textures you need to call
         * translate_palette_to_pix_format(get_palette_tp())
         */
        RAISE_ERROR(ERROR_INTEGRITY);
    default:
        error_set_tex_fmt(in_fmt);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

static enum gfx_tex_fmt
translate_palette_to_pix_format(enum palette_tp palette_tp) {
    switch (palette_tp) {
    case PALETTE_TP_ARGB_1555:
        return GFX_TEX_FMT_ARGB_1555;
    case PALETTE_TP_RGB_565:
        return GFX_TEX_FMT_RGB_565;
    case PALETTE_TP_ARGB_4444:
        return GFX_TEX_FMT_ARGB_4444;
    case PALETTE_TP_ARGB_8888:
        return GFX_TEX_FMT_ARGB_8888;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}
