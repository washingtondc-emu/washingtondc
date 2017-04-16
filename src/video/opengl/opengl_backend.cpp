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

/*
 * I'm using printf instead of std::cout for logging from now on because it
 * occured to me that iostream might not be thread-safe.
 *
 * Also, in general I'm still mulling the possibility of porting what I have
 * so far from c++03 to C11 because I'm starting to finally understand why it
 * is that people don't like C++.
 */
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <err.h>
#include <pthread.h>

#include "opengl_backend.hpp"
#include "shader.hpp"

static void init_poly();

// vertex position (x, y, z)
static const unsigned SLOT_VERT_POS = 0;

// vertex texture coordinates (s, t)
static const unsigned SLOT_VERT_ST = 1;

/*
 * this shader represents the final stage of output, where a single textured
 * quad is drawn covering the entirety of the screen.
 */
static struct shader fb_shader;

// number of floats per vertex.
// that's 3 floats for the position and 2 for the texture coords
const static unsigned FB_VERT_LEN = 5;
const static unsigned FB_VERT_COUNT = 4;
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

const static unsigned FB_QUAD_IDX_COUNT = 4;
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

/*
 * fb_read is allocated by backend_new_framebuffer from outside of the
 * graphics thread.  It is freed upon consumption by the graphics thread,
 * or upon obsoletion by the emulation thread.
 *
 * TODO: Ideally it would not need a mutex to protect it.  I'm imagining maybe
 * some sort of a ringbuffer wherein there are multiple fb_reads and only the
 * newest one is valid
 */
static uint32_t *fb_read;
static unsigned fb_read_width, fb_read_height;
static pthread_mutex_t fb_read_lock = PTHREAD_MUTEX_INITIALIZER;

static void backend_update_framebuffer();
static void backend_present();

void opengl_backend_init() {
    shader_init_from_file(&fb_shader, "final_vert.glsl", "final_frag.glsl");
    init_poly();
}

void opengl_backend_cleanup() {
    // TODO cleanup OpenGL stuff

    if (fb_read)
        free(fb_read);
}

void backend_new_framebuffer(uint32_t const *fb_new,
                             unsigned fb_new_width, unsigned fb_new_height) {
    int ret_code;
    size_t fb_size = fb_new_width * fb_new_height * sizeof(uint32_t);

    if ((ret_code = pthread_mutex_lock(&fb_read_lock)) != 0)
        err(errno, "unable to acquire fb_read_lock");

    if (fb_read) {
        /*
         * I don't know if this is what people are referring to when they talk
         * about dropped frames, but this is a frame which did not get displayed
         * ergo...a dropped frame
         */
        printf("WARNING: frame dropped by OpenGL backend\n");

        /*
         * free fb_read if the new framebuffer isn't the same size as the old
         * one; else we seize the oppurtunity to recycle the old framebuffer
         */
        if ((fb_read_width != fb_new_width) ||
            (fb_read_height != fb_new_height)) {
            free(fb_read);
            fb_read = NULL;
        }
    }

    if (!fb_read) {
        fb_read_width = fb_new_width;
        fb_read_height = fb_new_height;

        fb_read = (uint32_t*)malloc(fb_size);
        if (!fb_read)
            err(errno, "unable to allocate memory for %ux%u framebuffer",
                fb_read_width, fb_read_height);
    }

    memcpy(fb_read, fb_new, fb_size);

    if ((ret_code = pthread_mutex_unlock(&fb_read_lock)) != 0)
        err(errno, "unable to release fb_read_lock");

    /*
     * TODO: move this into the thread's main loop when the separate window
     * thread gets implemented
     */
    backend_update_framebuffer();
    backend_present();
}

static void backend_update_framebuffer() {
    int ret_code;
    unsigned img_width, img_height;
    uint32_t *img_data;

    if ((ret_code = pthread_mutex_lock(&fb_read_lock)) != 0)
        err(errno, "unable to acquire fb_read_lock");

    if (!fb_read) {
        if ((ret_code = pthread_mutex_unlock(&fb_read_lock)) != 0)
            err(errno, "unable to release fb_read_lock");
        return;
    }

    img_width = fb_read_width;
    img_height = fb_read_height;
    img_data = fb_read;
    fb_read = NULL;

    if ((ret_code = pthread_mutex_unlock(&fb_read_lock)) != 0)
        err(errno, "unable to release fb_read_lock");

    glBindTexture(GL_TEXTURE_2D, fb_poly.tex_obj);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_width, img_height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, img_data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    free(img_data);
}

static void backend_present() {
    glClear(GL_COLOR_BUFFER_BIT);

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
                          FB_VERT_LEN * sizeof(GL_FLOAT),
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
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    fb_poly.vbo = vbo;
    fb_poly.vao = vao;
    fb_poly.ebo = ebo;
    fb_poly.tex_obj = tex_obj;
}
