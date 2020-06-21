/*******************************************************************************
 *
 * Copyright 2019 snickerbockers
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

#ifndef WASHDC_GFX_GFX_CONFIG_H_
#define WASHDC_GFX_GFX_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The purpose of this file is to store settings for the graphics renderer.
 * There's a "default" configuration that renders everything the way you'd
 * expect (ie like a real Dreamcast would), but users can make changes to those
 * settings over the CLI to render things differently.  The primary usecase for
 * this is that sometimes I have to debug rendering bugs (like incorrect
 * depth-sorting), and I think it would be helpful to be able to do things like
 * render in wireframe or selectively disable polygons based on display lists,
 * etc.
 */

struct gfx_cfg {
    // if true, the renderer will render polygons as lines
    int wireframe : 1;

    // if false, textures will be forcibly disabled
    int tex_enable : 1;

    // if false, depth-testing will be forcibly disabled
    int depth_enable : 1;

    // if false, blending will be forcibly disabled
    int blend_enable : 1;

    // if false, the background color will always be black
    int bgcolor_enable : 1;

    // if false, all polygons will be white
    int color_enable : 1;

    // if true, enable order-independent transparency
    int depth_sort_enable : 1;

    // if true, allow punch-through polygons.  if false then don't.
    int pt_enable : 1;
};

struct gfx_cfg gfx_config_read(void);

void gfx_config_oit_enable(void);
void gfx_config_oit_disable(void);

#ifdef __cplusplus
}
#endif

#endif
