/*******************************************************************************
 *
 * Copyright 2017, 2018, 2020 snickerbockers
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
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>

#include "tex_cache.h"
#include "../gfx_obj.h"

static struct gfxgl3_tex tex_cache[GFX_TEX_CACHE_SIZE];

static void update_tex_from_obj(struct gfx_obj *obj, void const *in, size_t n_bytes);

void gfxgl3_tex_cache_init(void) {
    memset(tex_cache, 0, sizeof(tex_cache));
}

void gfxgl3_tex_cache_cleanup(void) {
    unsigned idx;
    for (idx = 0; idx < GFX_TEX_CACHE_SIZE; idx++)
        if (tex_cache[idx].valid)
            gfxgl3_tex_cache_evict(idx);
}

void gfxgl3_tex_cache_bind(unsigned tex_no, int obj_no, unsigned width,
                    unsigned height, enum gfx_tex_fmt tex_fmt) {
    struct gfx_obj *obj = gfx_obj_get(obj_no);
    struct gfxgl3_tex *tex = tex_cache + tex_no;

    tex->obj_handle = obj_no;
    tex->tex_fmt = tex_fmt;
    tex->width = width;
    tex->height = height;
    tex->valid = true;

    obj->arg = tex;
    obj->on_write = update_tex_from_obj;

    gfxgl3_renderer_update_tex(tex_no);
}

void gfxgl3_tex_cache_unbind(unsigned tex_no) {
    gfxgl3_tex_cache_evict(tex_no);
}

/*
 * This function is called to inform the tex cache that the given texture slot
 * does not hold valid data.  The caller does not have to check if there was
 * already valid data or not, so the onus is on this function to make sure it
 * doesn't accidentally double-free something.
 */
void gfxgl3_tex_cache_evict(unsigned idx) {
    tex_cache[idx].valid = false;
    struct gfx_obj *obj = gfx_obj_get(tex_cache[idx].obj_handle);
    obj->on_write = NULL;
    obj->arg = NULL;
}

struct gfxgl3_tex const* gfx_gfxgl3_tex_cache_get(unsigned idx) {
    if (idx < GFX_TEX_CACHE_SIZE)
        return tex_cache + idx;
    return NULL;
}

static void update_tex_from_obj(struct gfx_obj *obj,
                                void const *in, size_t n_bytes) {
    gfx_obj_alloc(obj);
    memcpy(obj->dat, in, n_bytes);

    obj->state = GFX_OBJ_STATE_DAT;

    struct gfxgl3_tex *tex = (struct gfxgl3_tex*)obj->arg;
    gfxgl3_renderer_update_tex(tex - tex_cache);
}
