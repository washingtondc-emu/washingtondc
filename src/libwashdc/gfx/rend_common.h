/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2020 snickerbockers
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

/*
 * common structures and interfaces that are used between the rendering code
 * and the non-rendering code.  Everything here pertains exclusively to the
 * gfx_thread.
 */

#ifndef REND_COMMON_H_
#define REND_COMMON_H_

#include "gfx/gfx_il.h"
#include "washdc/gfx/gfx_all.h"

// initialize and clean up the graphics renderer
void rend_init(struct rend_if const *rend_if);
void rend_cleanup(void);

// tell the renderer to update the given texture from the cache
void rend_update_tex(unsigned tex_no);

// tell the renderer to release the given texture from the cache
void rend_release_tex(unsigned tex_no);

extern struct rend_if const * gfx_rend_ifp;

#endif
