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
#include "washdc/gfx/obj.h"

void gfx_obj_init(int handle, size_t n_bytes);
void gfx_obj_free(int handle);
void gfx_obj_write(int handle, void const *dat, size_t n_bytes);
void gfx_obj_read(int handle, void *dat, size_t n_bytes);

#endif
