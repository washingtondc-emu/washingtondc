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

#include "error.h"
#include "gfx/gfx_obj.h"

#include "pvr2_gfx_obj.h"

static bool states[GFX_OBJ_COUNT];

int pvr2_alloc_gfx_obj(void) {
    int idx;
    for (idx = 0; idx < GFX_OBJ_COUNT; idx++)
        if (!states[idx]) {
            states[idx] = true;
            return idx;
        }

    RAISE_ERROR(ERROR_OVERFLOW);
}

void pvr2_free_gfx_obj(int obj) {
    states[obj] = false;
}
