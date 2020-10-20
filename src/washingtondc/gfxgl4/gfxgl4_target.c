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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "washdc/error.h"

#include "../gfx_obj.h"
#include "gfxgl4_renderer.h"
#include "gfxgl4_target.h"

GLuint gfxgl4_tgt_fbo;
static GLuint depth_buf_tex;
static GLenum draw_buffer = GL_COLOR_ATTACHMENT0;
static unsigned fbo_width, fbo_height;

static void gfxgl4_target_obj_read(struct gfx_obj  *obj, void *out,
                                   size_t n_bytes);
static void gfxgl4_target_grab_pixels(int handle, void *out, GLsizei buf_size);

void gfxgl4_target_init(void) {
    fbo_width = 0;
    fbo_height = 0;

    glGenFramebuffers(1, &gfxgl4_tgt_fbo);
    glGenTextures(1, &depth_buf_tex);
}

void gfxgl4_target_begin(unsigned width, unsigned height, int tgt_handle) {
    if (tgt_handle < 0) {
        fprintf(stderr, "%s - no rendering target is bound\n", __func__);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, gfxgl4_tgt_fbo);

    GLuint color_buf_tex = gfxgl4_renderer_tex(tgt_handle);

    if (gfxgl4_renderer_tex_get_dirty(tgt_handle) ||
        gfxgl4_renderer_tex_get_width(tgt_handle) != width ||
        gfxgl4_renderer_tex_get_height(tgt_handle) != height ||
        gfxgl4_renderer_tex_get_format(tgt_handle) != GL_RGBA ||
        gfxgl4_renderer_tex_get_dat_type(tgt_handle) != GL_UNSIGNED_BYTE) {
        glBindTexture(GL_TEXTURE_2D, color_buf_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gfxgl4_renderer_tex_set_dims(tgt_handle, width, height);
        gfxgl4_renderer_tex_set_format(tgt_handle, GL_RGBA);
        gfxgl4_renderer_tex_set_dat_type(tgt_handle, GL_UNSIGNED_BYTE);
        gfxgl4_renderer_tex_set_dirty(tgt_handle, false);
    }

    if (width != fbo_width || height != fbo_height) {
        // change texture dimensions
        // TODO: is all of this necessary, or just the glTexImage2D stuff?

        fbo_width = width;
        fbo_height = height;

        glBindTexture(GL_TEXTURE_2D, depth_buf_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    /*
     * it is guaranteed that fbo_width == width && fbo_height == height due to
     * the above if statement.
     */
    glViewport(0, 0, width, height);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, color_buf_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, depth_buf_tex, 0);
    glBindTexture(GL_TEXTURE_2D, color_buf_tex);
    glDrawBuffers(1, &draw_buffer);

    GLenum stat;
    if((stat = glCheckFramebufferStatus(GL_FRAMEBUFFER)) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "%s ERROR: framebuffer status is not complete: %d\n",
                __func__, stat);
        switch (stat) {
        case GL_FRAMEBUFFER_UNDEFINED:
            fprintf(stderr, "GL_FRAMEBUFFER_UNDEFINED\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            fprintf(stderr, "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            fprintf(stderr, "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            fprintf(stderr, "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER\n");
            break;
        default:
            fprintf(stderr, "unknown\n");
        }
        abort();
    }
}

void gfxgl4_target_end(int tgt_handle) {
    if (tgt_handle < 0) {
        fprintf(stderr, "%s ERROR: no target bound\n", __func__);
        return;
    }

    static GLenum back_buffer = GL_BACK;
    glDrawBuffers(1, &back_buffer);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // or should i do this in gfxgl4_target_end ?
    gfx_obj_get(tgt_handle)->state = GFX_OBJ_STATE_TEX;
}

static void gfxgl4_target_grab_pixels(int obj_handle, void *out,
                                      GLsizei buf_size) {
    size_t length_expect = fbo_width * fbo_height * 4 * sizeof(uint8_t);

    if (buf_size < length_expect) {
        fprintf(stderr, "need at least 0x%08x bytes (have 0x%08x)\n",
                  (unsigned)length_expect, (unsigned)buf_size);
        error_set_length(buf_size);
        error_set_expected_length(length_expect);
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

    GLuint color_buf_tex = gfxgl4_renderer_tex(obj_handle);
    glBindTexture(GL_TEXTURE_2D, color_buf_tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void gfxgl4_target_bind_obj(struct gfx_il_inst *cmd) {
    int obj_handle = cmd->arg.bind_render_target.gfx_obj_handle;
#ifdef INVARIANTS
    struct gfx_obj *obj = gfx_obj_get(obj_handle);
    if (obj->on_write ||
        (obj->on_read && obj->on_read != gfxgl4_target_obj_read))
        RAISE_ERROR(ERROR_INTEGRITY);
#endif
    /* tgt_handle = obj_handle; */
    gfx_obj_get(obj_handle)->on_read = gfxgl4_target_obj_read;

    // TODO: should I set TEXTURE_MIN_FILTER and TEXTURE_MAG_FILTER here?
}

void gfxgl4_target_unbind_obj(struct gfx_il_inst *cmd) {
    int obj_handle = cmd->arg.unbind_render_target.gfx_obj_handle;
    struct gfx_obj *obj = gfx_obj_get(obj_handle);

    gfx_obj_alloc(obj);
    if (gfx_obj_get(obj_handle)->state == GFX_OBJ_STATE_TEX)
        gfxgl4_target_grab_pixels(gfx_obj_handle(obj), obj->dat, obj->dat_len);

    obj->on_read = NULL;
}

static void gfxgl4_target_obj_read(struct gfx_obj *obj, void *out,
                                   size_t n_bytes) {
    if (obj->state == GFX_OBJ_STATE_TEX) {
        gfxgl4_target_grab_pixels(gfx_obj_handle(obj), out, n_bytes);
    } else {
        gfx_obj_alloc(obj);
        memcpy(out, obj->dat, n_bytes);
    }
}
