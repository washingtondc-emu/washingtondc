/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018, 2020 snickerbockers
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
