/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018-2020 snickerbockers
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
#include "washdc/gfx/obj.h"

#ifdef __cplusplus
extern "C" {
#endif

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

void gfx_obj_init(int handle, size_t n_bytes);
void gfx_obj_free(int handle);
void gfx_obj_write(int handle, void const *dat, size_t n_bytes);
void gfx_obj_read(int handle, void *dat, size_t n_bytes);

// Only call this from the gfx code
static inline void gfx_obj_alloc(struct gfx_obj *obj) {
    if (!obj->dat) {
        obj->dat = malloc(obj->dat_len);
        if (!obj->dat) {
            fprintf(stderr, "ERROR: FAILED ALLOC\n");
            abort();
        }
    }
}

struct gfx_obj *gfx_obj_get(int handle);

int gfx_obj_handle(struct gfx_obj *obj);

#ifdef __cplusplus
}
#endif

#endif
