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

#ifndef PVR2_GFX_OBJ_H_
#define PVR2_GFX_OBJ_H_

/*
 * this is a simple infra for tracking which gfx_objs are in use and which are
 * available.  gfx statically allocates a set number of objects, and leaves the
 * actual management of those objects to the emulation code.e
 */

/*
 * All these functions do is mark the given object as being in-use/not in-use
 * from PVR's perspective.  They don't actually modify the obj's state.
 */
int pvr2_alloc_gfx_obj(void);
void pvr2_free_gfx_obj(int obj);

#endif
