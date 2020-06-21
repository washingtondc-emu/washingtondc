/*******************************************************************************
 *
 * Copyright 2017-2019 snickerbockers
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

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gfx_config.h"

static struct gfx_cfg cur_profile = {
    .wireframe = 0,
    .tex_enable = 1,
    .depth_enable = 1,
    .blend_enable = 1,
    .bgcolor_enable = 1,
    .color_enable = 1,
    .depth_sort_enable = 1,
    .pt_enable = 1
};

bool wireframe_mode = false;

void gfx_config_default(void) {
    cur_profile.wireframe = 0;
    cur_profile.tex_enable = 1;
    cur_profile.depth_enable = 1;
    cur_profile.blend_enable = 1;
    cur_profile.bgcolor_enable = 1;
    cur_profile.color_enable = 1;
    cur_profile.pt_enable = 1;

    wireframe_mode = false;
}

void gfx_config_wireframe(void) {
    cur_profile.wireframe = 1;
    cur_profile.tex_enable = 0;
    cur_profile.depth_enable = 0;
    cur_profile.blend_enable = 0;
    cur_profile.bgcolor_enable = 0;
    cur_profile.color_enable = 0;
    cur_profile.pt_enable = 0;

    wireframe_mode = true;
}

void gfx_config_toggle_wireframe(void) {
    if (wireframe_mode)
        gfx_config_default();
    else
        gfx_config_wireframe();
}

struct gfx_cfg gfx_config_read(void) {
    return cur_profile;
}

void gfx_config_oit_enable(void) {
    cur_profile.depth_sort_enable = 1;
}
void gfx_config_oit_disable(void) {
    cur_profile.depth_sort_enable = 0;
}
