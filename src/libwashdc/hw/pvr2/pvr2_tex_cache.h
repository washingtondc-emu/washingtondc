/*******************************************************************************
 *
 * Copyright 2017-2020 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#ifndef PVR2_TEX_CACHE_H_
#define PVR2_TEX_CACHE_H_

#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "washdc/gfx/tex_cache.h"
#include "pvr2_ta.h"
#include "dc_sched.h"
#include "mem_areas.h"

#define PVR2_TEX_CACHE_SIZE GFX_TEX_CACHE_SIZE
#define PVR2_TEX_CACHE_MASK GFX_TEX_CACHE_MASK

struct pvr2_tex_meta {
    uint32_t addr_first, addr_last;

    unsigned w_shift, h_shift;

    /*
     * Linestride in this context doesn't work the same way it would in most
     * other contexts.  PowerVR2 allows programs to submit textures with
     * non-power-of-two dimensions by setting the stride select bit and writing
     * the desired length to the lowest 5 bits of TEXT_CONTROL.  The linestride
     * is always less than or equal to (1<<w_shift), and texture coordinates
     * are all calculated based on (1<<w_shift) even though that's not actually
     * the width of the texture.
     */
    unsigned linestride;

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
    enum gfx_tex_fmt pix_fmt;
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
    dc_cycle_stamp_t last_update;
    struct pvr2_tex_meta meta;

    // this refers to the gfx_obj bound to the texture
    int obj_no;

    // the frame stamp from the last time this texture was referenced
    unsigned frame_stamp_last_used;

    enum pvr2_tex_state state;
};

/*
 * For the purposes of texture cache invalidation, we divide texture memory
 * into a number of distinct pages.  When a texture is updated, we remember the
 * time-stamp at which that update happened.  When texture-memory is written to,
 * we set the timestamp of that page to the current timestamp.  When a texture
 * is used for rendering, we update it if its timestamp is behind the timestamps
 * of any of the pages it references.
 *
 * This macro defines the page size in bytes.  It must be a power of two.
 */
#define PVR2_TEX_PAGE_SIZE 512
#define PVR2_TEX_MEM_LEN (ADDR_TEX64_LAST - ADDR_TEX64_FIRST + 1)
#define PVR2_TEX_N_PAGES (PVR2_TEX_MEM_LEN / PVR2_TEX_PAGE_SIZE)

struct pvr2_tex_cache {
    dc_cycle_stamp_t page_stamps[PVR2_TEX_N_PAGES];
    struct pvr2_tex tex_cache[PVR2_TEX_CACHE_SIZE];
};

/*
 * insert the given texture into the cache.
 * pal_addr is only used for paletted textures
 */
struct pvr2_tex *pvr2_tex_cache_add(struct pvr2 *pvr2,
                                    uint32_t addr, uint32_t pal_addr,
                                    unsigned w_shift, unsigned h_shift,
                                    unsigned linestride,
                                    int tex_fmt, bool twiddled,
                                    bool vq_compression, bool mipmap,
                                    bool stride_sel);

/*
 * search the cache for the given texture
 * pal_addr is only used for paletted textures
 */
struct pvr2_tex *pvr2_tex_cache_find(struct pvr2 *pvr2,
                                     uint32_t addr, uint32_t pal_addr,
                                     unsigned w_shift, unsigned h_shift,
                                     unsigned linestride,
                                     int tex_fmt, bool twiddled,
                                     bool vq_compression, bool mipmap,
                                     bool stride_sel);

void
pvr2_tex_cache_notify_write(struct pvr2 *pvr2,
                            uint32_t addr_first, uint32_t len);

void
pvr2_tex_cache_notify_palette_write(struct pvr2 *pvr2,
                                    uint32_t addr_first, uint32_t len);
void pvr2_tex_cache_notify_palette_tp_change(struct pvr2 *pvr2);

int pvr2_tex_cache_get_idx(struct pvr2 *pvr2, struct pvr2_tex const *tex);

// this function sends the texture cache over to gfx by way of the gfx_il
void pvr2_tex_cache_xmit(struct pvr2 *pvr2);
void pvr2_tex_cache_xmit_one(struct pvr2 *pvr2, unsigned idx);

/*
 * Read the meta-information of the given texture.  This function will return
 * -1 if the slot indicated by tex_idx points to an invalid texture; else it
 * will return 0.
 */
int pvr2_tex_get_meta(struct pvr2 *pvr2,
                      struct pvr2_tex_meta *meta, unsigned tex_idx);

void pvr2_tex_cache_read(struct pvr2 *pvr2,
                         void **tex_dat_out, size_t *n_bytes_out,
                         struct pvr2_tex_meta const *meta);

void pvr2_tex_cache_init(struct pvr2 *pvr2);
void pvr2_tex_cache_cleanup(struct pvr2 *pvr2);

#endif
