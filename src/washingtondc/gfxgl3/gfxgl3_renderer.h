/*******************************************************************************
 *
 * Copyright 2017-2020 snickerbockers
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

#ifdef _WIN32
#include "i_hate_windows.h"
#endif

#ifndef gfxgl3_renderer_H_
#define gfxgl3_renderer_H_

#include <GL/gl.h>

#include "../renderer.h"

#include "washdc/gfx/gfx_all.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct gfx_rend_if const gfxgl3_rend_if;
extern struct renderer const gfxgl3_renderer;

GLuint gfxgl3_renderer_tex(unsigned obj_no);

unsigned gfxgl3_renderer_tex_get_width(unsigned obj_no);
unsigned gfxgl3_renderer_tex_get_height(unsigned obj_no);

void gfxgl3_renderer_tex_set_dims(unsigned obj_no,
                                  unsigned width, unsigned height);
void gfxgl3_renderer_tex_set_format(unsigned obj_no, GLenum fmt);
void gfxgl3_renderer_tex_set_dat_type(unsigned obj_no, GLenum dat_tp);
void gfxgl3_renderer_tex_set_dirty(unsigned obj_no, bool dirty);
GLenum gfxgl3_renderer_tex_get_format(unsigned obj_no);
GLenum gfxgl3_renderer_tex_get_dat_type(unsigned obj_no);
bool gfxgl3_renderer_tex_get_dirty(unsigned obj_no);

void gfxgl3_renderer_update_tex(unsigned tex_obj);
void gfxgl3_renderer_release_tex(unsigned tex_obj);

#ifdef __cplusplus
}
#endif

#endif
