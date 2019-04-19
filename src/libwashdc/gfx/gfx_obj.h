/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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

#ifndef GFX_OBJ_H_
#define GFX_OBJ_H_

#include <stddef.h>
#include <stdlib.h>

#include "washdc/error.h"

/*
 * An obj represents a blob of data sent to the gfx system.  It will be the
 * underlying storage class for textures and render targets.
 */

#define GFX_OBJ_COUNT 768

void gfx_obj_init(int handle, size_t n_bytes);
void gfx_obj_free(int handle);
void gfx_obj_write(int handle, void const *dat, size_t n_bytes);
void gfx_obj_read(int handle, void *dat, size_t n_bytes);

enum gfx_obj_state {
    GFX_OBJ_STATE_INVALID = 0,
    GFX_OBJ_STATE_DAT = 1,
    GFX_OBJ_STATE_TEX = 2,
    GFX_OBJ_STATE_TEX_AND_DAT = 3
};

struct gfx_obj {
    void *dat;
    void *arg;

    // called after the emulation code writes data to the object
    void (*on_write)(struct gfx_obj*, void const *in, size_t n_bytes);

    /*
     * called to read data out to the emulation code.
     * implementations should output the data to out.  They may also edit the
     * obj's data store but this is optional.
     */
    void (*on_read)(struct gfx_obj*, void *out, size_t n_bytes);

    size_t dat_len;

    enum gfx_obj_state state;
};

/*
 * This function should only ever be called from within the gfx code.
 * Code outside of the gfx code should absolutely never handle a gfx_obj
 * directly because that will cause problems in the future when I eventually
 * create a multithreaded software renderer.
 */
struct gfx_obj *gfx_obj_get(int handle);

// Only call this from the gfx code
static inline void gfx_obj_alloc(struct gfx_obj *obj) {
    if (!obj->dat) {
        obj->dat = malloc(obj->dat_len);
        if (!obj->dat)
            RAISE_ERROR(ERROR_FAILED_ALLOC);
    }
}

int gfx_obj_handle(struct gfx_obj *obj);

#endif
