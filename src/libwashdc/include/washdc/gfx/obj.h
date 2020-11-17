/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019, 2020 snickerbockers
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

#ifndef WASHDC_GFX_OBJ_H_
#define WASHDC_GFX_OBJ_H_

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * An obj represents a blob of data sent to the gfx system.  It will be the
 * underlying storage class for textures and render targets.
 */

#define GFX_OBJ_COUNT 768

enum gfx_obj_state {
    GFX_OBJ_STATE_INVALID = 0,
    GFX_OBJ_STATE_DAT = 1,
    GFX_OBJ_STATE_TEX = 2,
    GFX_OBJ_STATE_TEX_AND_DAT = 3
};

#ifdef __cplusplus
}
#endif

#endif
