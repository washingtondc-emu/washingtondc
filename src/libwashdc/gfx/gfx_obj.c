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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "washdc/error.h"

#include "gfx_obj.h"

static struct gfx_obj obj_array[GFX_OBJ_COUNT];

void gfx_obj_init(int handle, size_t n_bytes) {
    struct gfx_obj *obj = obj_array + handle;
    if (obj->dat_len)
        RAISE_ERROR(ERROR_INTEGRITY);
    obj->dat = NULL;
    obj->dat_len = n_bytes;
    obj->state = GFX_OBJ_STATE_DAT;
}

void gfx_obj_free(int handle) {
    struct gfx_obj *obj = obj_array + handle;
    free(obj->dat);
    obj->dat = NULL;
    obj->on_read = NULL;
    obj->on_write = NULL;
    obj->dat_len = 0;
}

void gfx_obj_write(int handle, void const *dat, size_t n_bytes) {
    struct gfx_obj *obj = obj_array + handle;
    if (n_bytes != obj->dat_len)
        RAISE_ERROR(ERROR_OVERFLOW);

    if (obj->on_write) {
        obj->on_write(obj, dat, n_bytes);
    } else {
        gfx_obj_alloc(obj);
        memcpy(obj->dat, dat, n_bytes);
        obj->state = GFX_OBJ_STATE_DAT;
    }
}

void gfx_obj_read(int handle, void *dat, size_t n_bytes) {
    struct gfx_obj *obj = obj_array + handle;
    if (n_bytes != obj->dat_len)
        RAISE_ERROR(ERROR_OVERFLOW);

    if (obj->on_read) {
        obj->on_read(obj, dat, n_bytes);
    } else {
        gfx_obj_alloc(obj);
        memcpy(dat, obj->dat, n_bytes);
    }
}

struct gfx_obj *gfx_obj_get(int handle) {
    return obj_array + handle;
}

int gfx_obj_handle(struct gfx_obj *obj) {
    return obj - obj_array;
}
