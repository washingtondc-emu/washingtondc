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
#include "shader.h"
#include "gfx/gfx.h"
#include "log.h"

static void init_poly();

// vertex position (x, y, z)
#define SLOT_VERT_POS 0

// vertex texture coordinates (s, t)
#define SLOT_VERT_ST 1

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
    /*
     * it is not a mistake that the texture-coordinates are upside-down
     * this is because dreamcast puts the origin at upper-left corner,
     * but opengl textures put the origin at the lower-left corner
     */

    // position            // texture coordinates
    -1.0f, -1.0f, 0.0f,    0.0f, 1.0f,
    -1.0f,  1.0f, 0.0f,    0.0f, 0.0f,
     1.0f,  1.0f, 0.0f,    1.0f, 0.0f,
     1.0f, -1.0f, 0.0f,    1.0f, 1.0f
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
struct fb_poly {
    GLuint vbo; // vertex buffer object
    GLuint vao; // vertex array object
    GLuint ebo; // element buffer object

    GLuint tex_obj; // texture object
} fb_poly;

static void
opengl_video_update_framebuffer(uint32_t const *fb_read,
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

void opengl_video_new_framebuffer(uint32_t const *fb_new,
                                  unsigned fb_new_width,
                                  unsigned fb_new_height) {
    opengl_video_update_framebuffer(fb_new, fb_new_width, fb_new_height);
    opengl_video_present();
    win_update();
}

static void
opengl_video_update_framebuffer(uint32_t const *fb_read,
                                unsigned fb_read_width,
                                unsigned fb_read_height) {
    if (!fb_read)
        return;

    glBindTexture(GL_TEXTURE_2D, fb_poly.tex_obj);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fb_read_width, fb_read_height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, fb_read);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void opengl_video_present() {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_DEPTH_TEST);

    glViewport(0, 0, 640, 480); // TODO: don't hardcode
    glUseProgram(fb_shader.shader_prog_obj);
    glBindTexture(GL_TEXTURE_2D, fb_poly.tex_obj);
    glUniform1i(glGetUniformLocation(fb_shader.shader_prog_obj, "fb_tex"), 0);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(fb_poly.vao);
    glDrawElements(GL_TRIANGLE_STRIP, FB_QUAD_IDX_COUNT, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void init_poly() {
    GLuint vbo, vao, ebo, tex_obj;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 FB_VERT_LEN * FB_VERT_COUNT * sizeof(GLfloat),
                 fb_quad_verts, GL_STATIC_DRAW);
    glVertexAttribPointer(SLOT_VERT_POS, 3, GL_FLOAT, GL_FALSE,
                          FB_VERT_LEN * sizeof(GLfloat),
                          (GLvoid*)0);
    glEnableVertexAttribArray(SLOT_VERT_POS);
    glVertexAttribPointer(SLOT_VERT_ST, 2, GL_FLOAT, GL_FALSE,
                          FB_VERT_LEN * sizeof(GLfloat),
                          (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(SLOT_VERT_ST);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, FB_QUAD_IDX_COUNT * sizeof(GLuint),
                 fb_quad_idx, GL_STATIC_DRAW);

    glBindVertexArray(0);

    // create texture object
    glGenTextures(1, &tex_obj);
    glBindTexture(GL_TEXTURE_2D, tex_obj);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    fb_poly.vbo = vbo;
    fb_poly.vao = vao;
    fb_poly.ebo = ebo;
    fb_poly.tex_obj = tex_obj;
}
