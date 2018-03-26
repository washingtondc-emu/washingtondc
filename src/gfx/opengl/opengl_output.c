/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <err.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "glfw/window.h"
#include "opengl_output.h"
#include "opengl_renderer.h"
#include "shader.h"
#include "gfx/gfx.h"
#include "gfx/gfx_obj.h"
#include "gfx/opengl/font/font.h"
#include "overlay.h"
#include "log.h"

static void init_poly();

/*
 * this shader represents the final stage of output, where a single textured
 * quad is drawn covering the entirety of the screen.
 */
static struct shader fb_shader;

// number of floats per vertex.
// that's 3 floats for the position and 2 for the texture coords
#define FB_VERT_LEN 5
#define FB_VERT_COUNT 4
static GLfloat fb_quad_verts[FB_VERT_LEN * FB_VERT_COUNT] = {
    // position            // texture coordinates
    -1.0f,  1.0f, 0.0f,    0.0f, 1.0f,
    -1.0f, -1.0f, 0.0f,    0.0f, 0.0f,
     1.0f, -1.0f, 0.0f,    1.0f, 0.0f,
     1.0f,  1.0f, 0.0f,    1.0f, 1.0f
};

#define FB_QUAD_IDX_COUNT 4
GLuint fb_quad_idx[FB_QUAD_IDX_COUNT] = {
    1, 0, 2, 3
};

/*
 * container for the poly's vertex array and its associated buffer objects.
 * this is created by fb_init_poly and never modified.
 *
 * The tex_obj, on the other hand, is modified frequently, as it is OpenGL's
 * view of our framebuffer.
 */
static struct fb_poly {
    GLuint vbo; // vertex buffer object
    GLuint vao; // vertex array object
    GLuint ebo; // element buffer object
} fb_poly;

static int bound_obj_handle;

static GLfloat trans_mat[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f ,0.0f, 0.0f, 1.0f
};

static GLfloat const tex_mat[9] = {
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f
};

static void set_flip(bool flip);

static void
opengl_video_update_framebuffer(int obj_handle,
                                unsigned fb_read_width,
                                unsigned fb_read_height);

void opengl_video_output_init() {
    shader_load_vert_from_file(&fb_shader, "final_vert.glsl");
    shader_load_frag_from_file(&fb_shader, "final_frag.glsl");
    shader_link(&fb_shader);

    init_poly();
}

void opengl_video_output_cleanup() {
    // TODO cleanup OpenGL stuff
}

void opengl_video_new_framebuffer(int obj_handle,
                                  unsigned fb_new_width,
                                  unsigned fb_new_height, bool do_flip) {
    set_flip(do_flip);
    opengl_video_update_framebuffer(obj_handle, fb_new_width, fb_new_height);
    opengl_video_present();
    win_update();
}

static void set_flip(bool flip) {
    if (flip)
        trans_mat[5] = -1.0f;
    else
        trans_mat[5] = 1.0f;
}

static void
opengl_video_update_framebuffer(int obj_handle,
                                unsigned fb_read_width,
                                unsigned fb_read_height) {
    if (obj_handle < 0)
        return;

    struct gfx_obj *obj = gfx_obj_get(obj_handle);

    if (!(obj->state & GFX_OBJ_STATE_TEX)) {
        GLuint tex_obj = opengl_renderer_tex(obj_handle);
        if (obj->dat_len < fb_read_width * fb_read_height * sizeof(uint32_t))
            RAISE_ERROR(ERROR_INTEGRITY);

        glBindTexture(GL_TEXTURE_2D, tex_obj);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fb_read_width, fb_read_height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, obj->dat);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    bound_obj_handle = obj_handle;
}

void opengl_video_present() {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    glViewport(0, 0, 640, 480); // TODO: don't hardcode
    glUseProgram(fb_shader.shader_prog_obj);
    glBindTexture(GL_TEXTURE_2D, opengl_renderer_tex(bound_obj_handle));
    glUniform1i(glGetUniformLocation(fb_shader.shader_prog_obj, "fb_tex"), 0);
    glUniformMatrix4fv(OUTPUT_SLOT_TRANS_MAT, 1, GL_TRUE, trans_mat);
    glUniformMatrix3fv(OUTPUT_SLOT_TEX_MAT, 1, GL_TRUE, tex_mat);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(fb_poly.vao);
    glDrawElements(GL_TRIANGLE_STRIP, FB_QUAD_IDX_COUNT, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    overlay_draw(640, 480);
}

static void init_poly() {
    GLuint vbo, vao, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 FB_VERT_LEN * FB_VERT_COUNT * sizeof(GLfloat),
                 fb_quad_verts, GL_STATIC_DRAW);
    glVertexAttribPointer(OUTPUT_SLOT_VERT_POS, 3, GL_FLOAT, GL_FALSE,
                          FB_VERT_LEN * sizeof(GLfloat),
                          (GLvoid*)0);
    glEnableVertexAttribArray(OUTPUT_SLOT_VERT_POS);
    glVertexAttribPointer(OUTPUT_SLOT_VERT_ST, 2, GL_FLOAT, GL_FALSE,
                          FB_VERT_LEN * sizeof(GLfloat),
                          (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(OUTPUT_SLOT_VERT_ST);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, FB_QUAD_IDX_COUNT * sizeof(GLuint),
                 fb_quad_idx, GL_STATIC_DRAW);

    glBindVertexArray(0);

    fb_poly.vbo = vbo;
    fb_poly.vao = vao;
    fb_poly.ebo = ebo;
}
