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
#include "mem_areas.h"

#include "geo_buf.h"

#include "pvr2_tex_cache.h"

unsigned static const pixel_sizes[TEX_CTRL_PIX_FMT_COUNT] = {
    2,
    2,
    4,
    1,
    0, // TODO: wtf is TEX_CTRL_PIX_FMT_BUMP_MAP???
    0, // TODO: wtf is TEX_CTRL_PIX_FMT_4_BPP_PAL
    0, // TODO: wtf is TEX_CTRL_PIX_FMT_8_BPP_PAL
    0
};

static struct pvr2_tex tex_cache[PVR2_TEX_CACHE_SIZE];

static unsigned tex_twiddle(unsigned x, unsigned y,
                            unsigned w_shift, unsigned h_shift);

/*
 * maps from a normal row-major configuration to the
 * pvr2's own "twiddled" format
 */
static unsigned tex_twiddle(unsigned x, unsigned y,
                            unsigned w_shift, unsigned h_shift) {
    assert(x < (1 << w_shift));
    assert(y < (1 << h_shift));

    if (w_shift == 0 && h_shift == 0)
        return 0;

    unsigned w_shift_next = w_shift;
    unsigned h_shift_next = h_shift;
    if (w_shift)
        w_shift_next--;
    if (h_shift)
        h_shift_next--;

    if (x < (1 << w_shift_next) && y < (1 << h_shift_next)) {
        return tex_twiddle(x, y, w_shift_next, h_shift_next);
    } else if (x < (1 << w_shift_next) && y >= (1 << h_shift_next)) {
        return (1 << w_shift_next) * (1 << h_shift_next) +
            tex_twiddle(x, y - (1 << h_shift_next), w_shift_next, h_shift_next);
    } else if (x >= (1 << w_shift_next) && y < (1 << h_shift_next)) {
        return 2 * (1 << w_shift_next) * (1 << h_shift_next) +
            tex_twiddle(x - (1 << w_shift_next), y, w_shift_next, h_shift_next);
    } else {
        return 3 * (1 << w_shift_next) * (1 << h_shift_next) +
            tex_twiddle(x - (1 << w_shift_next), y - (1 << h_shift_next),
                        w_shift_next, h_shift_next);
    }
}

struct pvr2_tex *pvr2_tex_cache_find(uint32_t addr, unsigned w,
                                     unsigned h, int pix_fmt) {
    unsigned idx;
    struct pvr2_tex *tex;
    for (idx = 0; idx < PVR2_TEX_CACHE_SIZE; idx++) {
        tex = tex_cache + idx;
        if (tex->valid && (tex->addr_first == addr) &&
            (tex->w == w) && (tex->h == h) && (tex->pix_fmt == pix_fmt)) {
            return tex;
        }
    }

    return NULL;
}

struct pvr2_tex *pvr2_tex_cache_add(uint32_t addr,
                                    unsigned w, unsigned h,
                                    int pix_fmt) {
    assert(pix_fmt < TEX_CTRL_PIX_FMT_INVALID);

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
    tex->w = w;
    tex->h = h;
    tex->pix_fmt = pix_fmt;

    tex->addr_last = addr - 1 + pixel_sizes[pix_fmt] * w * h;

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
                          tex->addr_first, tex->addr_last)) {
            tex->dirty = true;
        }
    }
}

/*
 * TODO: store the shifts in the textures instead of the width/height
 * so I don't need this stupid function.
 */
static unsigned pvr2_log2(unsigned in) {
    switch (in) {
    case 1:
        return 0;
    case 2:
        return 1;
    case 4:
        return 2;
    case 8:
        return 3;
    case 16:
        return 4;
    case 32:
        return 5;
    case 64:
        return 6;
    case 128:
        return 7;
    case 256:
        return 8;
    case 512:
        return 9;
    case 1024:
        return 10;
    }

    abort();
}

void pvr2_tex_cache_xmit(struct geo_buf *out) {
    unsigned idx;
    for (idx = 0; idx < PVR2_TEX_CACHE_SIZE; idx++) {
        struct pvr2_tex *tex_in = tex_cache + idx;
        struct pvr2_tex *tex_out = out->tex_cache + idx;

        if (tex_in->valid && tex_in->dirty) {
            tex_out->addr_first = tex_in->addr_first;
            tex_out->addr_last = tex_in->addr_last;
            tex_out->w = tex_in->w;
            tex_out->h = tex_in->h;
            tex_out->pix_fmt = tex_in->pix_fmt;

            // TODO: better error-handling
            if ((ADDR_TEX64_LAST - ADDR_TEX64_FIRST + 1) <=
                (tex_in->addr_last - tex_in->addr_first + 1)) {
                abort();
            }

            printf("tex_in->addr_first is 0x%08x\n", tex_in->addr_first);

            size_t n_bytes = sizeof(uint8_t) *
                tex_in->w * tex_in->h * pixel_sizes[tex_in->pix_fmt];
            tex_out->dat = (uint8_t*)malloc(n_bytes);

            // de-twiddle
            /*
             * TODO: don't do this unconditionally
             * not all textures are twiddled
             */
            unsigned row, col;
            uint8_t const *beg = pvr2_tex64_mem + tex_in->addr_first;
            for (row = 0; row < tex_in->h; row++) {
                for (col = 0; col < tex_in->w; col++) {
                    unsigned twid_idx = tex_twiddle(col, row,
                                                   pvr2_log2(tex_in->w),
                                                   pvr2_log2(tex_in->h));


                    memcpy(tex_out->dat + (row * tex_in->w + col) * pixel_sizes[tex_in->pix_fmt],
                           beg + twid_idx * pixel_sizes[tex_in->pix_fmt],
                           pixel_sizes[tex_in->pix_fmt]);
                }
            }

            // this is what you'd have to do for a non-twiddled texture, I think
            /* memcpy(tex_out->dat, pvr2_tex64_mem + tex_in->addr_first, */
            /*        tex_in->addr_last - tex_in->addr_first + 1); */

            tex_in->dirty = false;
            tex_out->dirty = true;
            tex_out->valid = true;
        }
    }
}

int pvr2_tex_cache_get_idx(struct pvr2_tex const *tex) {
    return tex - tex_cache;
}
