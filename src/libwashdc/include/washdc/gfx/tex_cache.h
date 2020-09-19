/*******************************************************************************
 *
 * Copyright 2019, 2020 snickerbockers
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

#ifndef WASHDC_GFX_TEX_CACHE_H_
#define WASHDC_GFX_TEX_CACHE_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum gfx_tex_fmt {
    GFX_TEX_FMT_ARGB_1555,
    GFX_TEX_FMT_RGB_565,
    GFX_TEX_FMT_ARGB_4444,
    GFX_TEX_FMT_ARGB_8888,
    GFX_TEX_FMT_YUV_422,

    GFX_TEX_FMT_COUNT
};

/*
 * This is the gfx_thread's copy of the texture cache.  It mirrors the one
 * in the geo_buf code, and is updated every time a new geo_buf is submitted by
 * the PVR2 STARTRENDER command.
 */

#define GFX_TEX_CACHE_SIZE 512
#define GFX_TEX_CACHE_MASK (GFX_TEX_CACHE_SIZE - 1)

#ifdef __cplusplus
}
#endif

#endif
