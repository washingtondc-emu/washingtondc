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

#ifndef PVR2_TEX_CACHE_H_
#define PVR2_TEX_CACHE_H_

#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "gfx/gfx_tex_cache.h"
#include "pvr2_ta.h"

#define PVR2_TEX_CACHE_SIZE GFX_TEX_CACHE_SIZE
#define PVR2_TEX_CACHE_MASK GFX_TEX_CACHE_MASK

struct pvr2_tex_meta {
    uint32_t addr_first, addr_last;

    unsigned w_shift, h_shift;

    /*
     * The difference between pix_fmt and tex_fmt is that pix_fmt documents
     * what the format is in the texture cache, while tex_fmt documents what the
     * format is in the PVR2 texture memory.  Usually they're both the same
     * thing, but in the case of paletted textures they can be different.  The
     * pix_fmt is what is used for rendering; tex_fmt exists mainly so that it's
     * available for debugging and the tex-info cli command.  tex_fmt *is* one
     * of the parameters that has to match for the pvr2_tex_cache_find function,
     * so there's that too.
     */
    int pix_fmt;
    int tex_fmt;

    /*
     * this is the upper 2-bits (for 8BPP) or 6 bits (for 4BPP) of every
     * palette address referenced by this texture.  It needs to be shifted left
     * by 2 or 6 bits and ORed with pixel values to get palette addresses.
     *
     * this field only holds meaning if tex_fmt is TEX_CTRL_PIX_FMT_4_BPP_PAL
     * or TEX_CTRL_PIX_FMT_8_BPP_PAL; otherwise it is meaningless.
     */
    uint32_t tex_palette_start;

    bool twiddled;

    bool stride_sel;

    bool vq_compression;

    bool mipmap;
};

enum pvr2_tex_state {
    // the texture in this slot is invalid
    PVR2_TEX_INVALID,

    /*
     * if this is the state, it means that this entry in the texture cache has
     * changed since the last update.  This is the only state for which the
     * data in dat is not valid (although the data in the corresponding entry
     * in OpenGL's tex cache is).
     */
    PVR2_TEX_DIRTY,

    // texture is valid and has already been submitted to the renderer
    PVR2_TEX_READY
};

struct pvr2_tex {
    struct pvr2_tex_meta meta;

    // this refers to the gfx_obj bound to the texture
    int obj_no;

    // the frame stamp from the last time this texture was referenced
    unsigned frame_stamp_last_used;

    enum pvr2_tex_state state;
};

/*
 * insert the given texture into the cache.
 * pal_addr is only used for paletted textures
 */
struct pvr2_tex *pvr2_tex_cache_add(uint32_t addr, uint32_t pal_addr,
                                    unsigned w_shift, unsigned h_shift,
                                    int tex_fmt, bool twiddled,
                                    bool vq_compression, bool mipmap,
                                    bool stride_sel);

/*
 * search the cache for the given texture
 * pal_addr is only used for paletted textures
 */
struct pvr2_tex *pvr2_tex_cache_find(uint32_t addr, uint32_t pal_addr,
                                     unsigned w_shift, unsigned h_shift,
                                     int tex_fmt, bool twiddled,
                                     bool vq_compression, bool mipmap,
                                     bool stride_sel);

void pvr2_tex_cache_notify_write(uint32_t addr_first, uint32_t len);

void pvr2_tex_cache_notify_palette_write(uint32_t addr_first, uint32_t len);
void pvr2_tex_cache_notify_palette_tp_change(void);

int pvr2_tex_cache_get_idx(struct pvr2_tex const *tex);

// this function sends the texture cache over to gfx by way of the gfx_il
void pvr2_tex_cache_xmit(void);

/*
 * Read the meta-information of the given texture.  This function will return
 * -1 if the slot indicated by tex_idx points to an invalid texture; else it
 * will return 0.
 */
int pvr2_tex_get_meta(struct pvr2_tex_meta *meta, unsigned tex_idx);

void pvr2_tex_cache_read(void **tex_dat_out, size_t *n_bytes_out,
                         struct pvr2_tex_meta const *meta);

void pvr2_tex_cache_init(void);
void pvr2_tex_cache_cleanup(void);

#endif
