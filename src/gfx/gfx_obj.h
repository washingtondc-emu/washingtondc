/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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

/*
 * An obj represents a blob of data sent to the gfx system.  It will be the
 * underlying storage class for textures and render targets.
 */

#define GFX_OBJ_COUNT 1024

void gfx_obj_alloc(int handle, size_t n_bytes);
void gfx_obj_free(int handle);
void gfx_obj_write(int handle, void const *dat, size_t n_bytes);
void gfx_obj_read(int handle, void *dat, size_t n_bytes);

struct gfx_obj {
    void *dat;
    void *arg;
    void (*on_update)(struct gfx_obj*);
    size_t dat_len;
};

/*
 * This function should only ever be called from within the gfx code.
 * Code outside of the gfx code should absolutely never handle a gfx_obj
 * directly because that will cause problems in the future when I eventually
 * create a multithreaded software renderer.
 */
struct gfx_obj *gfx_obj_get(int handle);

#endif
