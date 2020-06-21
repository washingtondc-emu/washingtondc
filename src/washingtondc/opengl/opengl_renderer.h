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

#ifndef OPENGL_RENDERER_H_
#define OPENGL_RENDERER_H_

#include <GL/gl.h>

#include "washdc/gfx/gfx_all.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * functions the renderer calls to interact with stuff like the windowing
 * system and overlay.
 */
struct opengl_renderer_callbacks {
    // tells the window to check for events.  This is optional and can be NULL
    void (*win_update)(void);

    // tells the overlay to draw using OpenGL.  This is optional and can be NULL
    void (*overlay_draw)(void);
};
void
opengl_renderer_set_callbacks(struct opengl_renderer_callbacks const
                              *callbacks);

extern struct rend_if const opengl_rend_if;

GLuint opengl_renderer_tex(unsigned obj_no);

unsigned opengl_renderer_tex_get_width(unsigned obj_no);
unsigned opengl_renderer_tex_get_height(unsigned obj_no);

void opengl_renderer_tex_set_dims(unsigned obj_no,
                                  unsigned width, unsigned height);
void opengl_renderer_tex_set_format(unsigned obj_no, GLenum fmt);
void opengl_renderer_tex_set_dat_type(unsigned obj_no, GLenum dat_tp);
void opengl_renderer_tex_set_dirty(unsigned obj_no, bool dirty);
GLenum opengl_renderer_tex_get_format(unsigned obj_no);
GLenum opengl_renderer_tex_get_dat_type(unsigned obj_no);
bool opengl_renderer_tex_get_dirty(unsigned obj_no);

void opengl_renderer_update_tex(unsigned tex_obj);
void opengl_renderer_release_tex(unsigned tex_obj);

#ifdef __cplusplus
}
#endif

#endif
