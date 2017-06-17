/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "opengl_target.h"

static GLuint fbo;
static GLuint tex;
static GLenum draw_buffer = GL_COLOR_ATTACHMENT0;
static unsigned fbo_width, fbo_height;

void opengl_target_init(void) {
    fbo_width = fbo_height = 0;
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &tex);
}

void opengl_target_begin(unsigned width, unsigned height) {
    if (width != fbo_width || height != fbo_height) {
        // change texture dimensions
        // TODO: is all of this necessary, or just the glTexImage2D stuff?

        fbo_width = width;
        fbo_height = height;

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glBindTexture(GL_TEXTURE_2D, tex);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindTexture(GL_TEXTURE_2D, tex);
    glDrawBuffers(1, &draw_buffer);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        abort();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, fbo_width, fbo_height);
}

void opengl_target_end(void) {
    static GLenum back_buffer = GL_BACK;
    glDrawBuffers(1, &back_buffer);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void opengl_target_grab_pixels(void *out, GLsizei buf_size) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glGetnTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf_size, out);
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint opengl_target_get_tex(void) {
    return tex;
}
