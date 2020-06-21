/*******************************************************************************
 *
 * Copyright 2018-2020 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#include "washdc/error.h"
#include "washdc/gfx/obj.h"

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
