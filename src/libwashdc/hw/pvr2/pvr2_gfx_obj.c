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

#include "washdc/error.h"
#include "gfx/gfx_obj.h"

#include "pvr2_gfx_obj.h"

/*
 * The purpose of this file is to manage gfx_obj allocation from the emu-side
 * of things.  Because of this, it stores state as global static rather than
 * storing it in struct pvr2.  If there were to be more than one PVR2 on the
 * same system (as is the case with NAOMI 2) then they'd be allocating gfx_objs
 * from the same pool because there's only one gfx backend.
 */
static bool states[GFX_OBJ_COUNT];
static unsigned alloc_count, free_count;

static DEF_ERROR_U32_ATTR(alloc_count)
static DEF_ERROR_U32_ATTR(free_count)

int pvr2_alloc_gfx_obj(void) {
    int idx;
    for (idx = 0; idx < GFX_OBJ_COUNT; idx++)
        if (!states[idx]) {
            states[idx] = true;
            alloc_count++;
            return idx;
        }

    error_set_alloc_count(alloc_count);
    error_set_free_count(free_count);
    RAISE_ERROR(ERROR_OVERFLOW);
}

void pvr2_free_gfx_obj(int obj) {
    free_count++;
    states[obj] = false;
}
