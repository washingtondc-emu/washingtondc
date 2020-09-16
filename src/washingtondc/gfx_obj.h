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
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 ******************************************************************************/

#ifndef GFX_OBJ_H_
#define GFX_OBJ_H_

#include <stddef.h>
#include <stdlib.h>

#include "washdc/error.h"
#include "washdc/gfx/obj.h"

#ifdef __cplusplus
extern "C" {
#endif

struct gfx_obj {
    void *dat;
    void *arg;

    // called after the emulation code writes data to the object
    void (*on_write)(struct gfx_obj*, void const *in, size_t n_bytes);

    /*
     * called to read data out to the emulation code.
     * implementations should output the data to out.  They may also edit the
     * obj's data store but this is optional.
     */
    void (*on_read)(struct gfx_obj*, void *out, size_t n_bytes);

    size_t dat_len;

    enum gfx_obj_state state;
};

void gfx_obj_init(int handle, size_t n_bytes);
void gfx_obj_free(int handle);
void gfx_obj_write(int handle, void const *dat, size_t n_bytes);
void gfx_obj_read(int handle, void *dat, size_t n_bytes);

// Only call this from the gfx code
static inline void gfx_obj_alloc(struct gfx_obj *obj) {
    if (!obj->dat) {
        obj->dat = malloc(obj->dat_len);
        if (!obj->dat) {
            fprintf(stderr, "ERROR: FAILED ALLOC\n");
            abort();
        }
    }
}

struct gfx_obj *gfx_obj_get(int handle);

int gfx_obj_handle(struct gfx_obj *obj);

#ifdef __cplusplus
}
#endif

#endif
