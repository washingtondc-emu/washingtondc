/*******************************************************************************
 *
 * Copyright 2018, 2019 snickerbockers
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
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
