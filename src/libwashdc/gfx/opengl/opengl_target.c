/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "washdc/error.h"
#include "log.h"
#include "gfx/gfx_obj.h"
#include "gfx/opengl/opengl_renderer.h"

#include "opengl_target.h"

static GLuint fbo;
static GLuint depth_buf_tex;
static GLenum draw_buffer = GL_COLOR_ATTACHMENT0;
static unsigned fbo_width, fbo_height;

static void opengl_target_obj_read(struct gfx_obj  *obj, void *out,
                                   size_t n_bytes);
static void opengl_target_grab_pixels(int handle, void *out, GLsizei buf_size);

void opengl_target_init(void) {
    fbo_width = 0;
    fbo_height = 0;

    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &depth_buf_tex);
}

void opengl_target_begin(unsigned width, unsigned height, int tgt_handle) {
    if (tgt_handle < 0) {
        LOG_ERROR("%s - no rendering target is bound\n", __func__);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLuint color_buf_tex = opengl_renderer_tex(tgt_handle);

    if (opengl_renderer_tex_get_dirty(tgt_handle) ||
        opengl_renderer_tex_get_width(tgt_handle) != width ||
        opengl_renderer_tex_get_height(tgt_handle) != height ||
        opengl_renderer_tex_get_format(tgt_handle) != GL_RGBA ||
        opengl_renderer_tex_get_dat_type(tgt_handle) != GL_UNSIGNED_BYTE) {
        glBindTexture(GL_TEXTURE_2D, color_buf_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        opengl_renderer_tex_set_dims(tgt_handle, width, height);
        opengl_renderer_tex_set_format(tgt_handle, GL_RGBA);
        opengl_renderer_tex_set_dat_type(tgt_handle, GL_UNSIGNED_BYTE);
        opengl_renderer_tex_set_dirty(tgt_handle, false);
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
        LOG_ERROR("%s ERROR: framebuffer status is not complete: %d\n", __func__, stat);
        switch (stat) {
        case GL_FRAMEBUFFER_UNDEFINED:
            LOG_ERROR("GL_FRAMEBUFFER_UNDEFINED\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            LOG_ERROR("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            LOG_ERROR("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            LOG_ERROR("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER\n");
            break;
        default:
            LOG_ERROR("unknown\n");
        }
        abort();
    }
}

void opengl_target_end(int tgt_handle) {
    if (tgt_handle < 0) {
        LOG_ERROR("%s ERROR: no target bound\n", __func__);
        return;
    }

    static GLenum back_buffer = GL_BACK;
    glDrawBuffers(1, &back_buffer);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // or should i do this in opengl_target_end ?
    gfx_obj_get(tgt_handle)->state = GFX_OBJ_STATE_TEX;
}

static void opengl_target_grab_pixels(int obj_handle, void *out,
                                      GLsizei buf_size) {
    size_t length_expect = fbo_width * fbo_height * 4 * sizeof(uint8_t);

    if (buf_size < length_expect) {
        LOG_ERROR("need at least 0x%08x bytes (have 0x%08x)\n",
                  (unsigned)length_expect, (unsigned)buf_size);
        error_set_length(buf_size);
        error_set_expected_length(length_expect);
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

    GLuint color_buf_tex = opengl_renderer_tex(obj_handle);
    glBindTexture(GL_TEXTURE_2D, color_buf_tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void opengl_target_bind_obj(int obj_handle) {
#ifdef INVARIANTS
    struct gfx_obj *obj = gfx_obj_get(obj_handle);
    if (obj->on_write ||
        (obj->on_read && obj->on_read != opengl_target_obj_read))
        RAISE_ERROR(ERROR_INTEGRITY);
#endif
    /* tgt_handle = obj_handle; */
    gfx_obj_get(obj_handle)->on_read = opengl_target_obj_read;

    // TODO: should I set TEXTURE_MIN_FILTER and TEXTURE_MAG_FILTER here?
}

void opengl_target_unbind_obj(int obj_handle) {
    struct gfx_obj *obj = gfx_obj_get(obj_handle);

    gfx_obj_alloc(obj);
    if (gfx_obj_get(obj_handle)->state == GFX_OBJ_STATE_TEX)
        opengl_target_grab_pixels(gfx_obj_handle(obj), obj->dat, obj->dat_len);

    obj->on_read = NULL;
}

static void opengl_target_obj_read(struct gfx_obj *obj, void *out,
                                   size_t n_bytes) {
    if (obj->state == GFX_OBJ_STATE_TEX) {
        opengl_target_grab_pixels(gfx_obj_handle(obj), out, n_bytes);
    } else {
        gfx_obj_alloc(obj);
        memcpy(out, obj->dat, n_bytes);
    }
}
