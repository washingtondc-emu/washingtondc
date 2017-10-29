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

#include <string.h>
#include <stdlib.h>

#include "rend_common.h"

#include "gfx_tex_cache.h"

static struct gfx_tex tex_cache[PVR2_TEX_CACHE_SIZE];

void gfx_tex_cache_init(void) {
    memset(tex_cache, 0, sizeof(tex_cache));
}

void gfx_tex_cache_cleanup(void) {
    unsigned idx;
    for (idx = 0; idx < PVR2_TEX_CACHE_SIZE; idx++)
        if (gfx_tex_cache_get(idx)->valid)
            gfx_tex_cache_evict(idx);
}

void gfx_tex_cache_add(unsigned idx, struct gfx_tex const *tex,
                       void const *tex_data) {
    struct gfx_tex *slot = tex_cache + idx;

    if (slot->valid)
        gfx_tex_cache_evict(idx);

    memcpy(slot, tex, sizeof(*slot));

    rend_update_tex(idx, tex_data);
}

/*
 * This function is called to inform the tex cache that the given texture slot
 * does not hold valid data.  The caller does not have to check if there was
 * already valid data or not, so the onus is on this function to make sure it
 * doesn't accidentally double-free something.
 */
void gfx_tex_cache_evict(unsigned idx) {
    tex_cache[idx].valid = false;
}

struct gfx_tex const* gfx_tex_cache_get(unsigned idx) {
    if (idx < PVR2_TEX_CACHE_SIZE)
        return tex_cache + idx;
    return NULL;
}
