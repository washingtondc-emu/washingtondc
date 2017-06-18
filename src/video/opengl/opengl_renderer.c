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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "hw/pvr2/geo_buf.h"
#include "shader.h"
#include "opengl_target.h"
#include "framebuffer.h"
#include "dreamcast.h"

#include "opengl_renderer.h"

#define POSITION_SLOT 0
#define SCREEN_DIMS_SLOT 1
#define COLOR_SLOT 2

static unsigned volatile frame_stamp;

static pthread_cond_t frame_stamp_update_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t frame_stamp_mtx = PTHREAD_MUTEX_INITIALIZER;

static struct shader pvr_ta_shader;

static GLuint vbo, vao;
/*
 * draws the given geo_buf in whatever context is available (ie without setting
 * the shader, or the framebuffer).
 */
static void render_do_draw(struct geo_buf *geo);

void render_init(void) {
    shader_init_from_file(&pvr_ta_shader, "pvr2_ta_vert.glsl",
                          "pvr2_ta_frag.glsl");

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
}

void render_cleanup(void) {
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    shader_cleanup(&pvr_ta_shader);

    vao = 0;
    vbo = 0;
}

static void render_do_draw(struct geo_buf *geo) {
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(float) * geo->n_verts * GEO_BUF_VERT_LEN,
                 geo->verts, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(POSITION_SLOT);
    glEnableVertexAttribArray(COLOR_SLOT);
    glVertexAttribPointer(POSITION_SLOT, 3, GL_FLOAT, GL_FALSE,
                          GEO_BUF_VERT_LEN * sizeof(float), (GLvoid*)0);
    glVertexAttribPointer(COLOR_SLOT, 4, GL_FLOAT, GL_FALSE,
                          GEO_BUF_VERT_LEN * sizeof(float),
                          (GLvoid*)(3 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, geo->n_verts);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void render_next_geo_buf(void) {
    struct geo_buf *geo;
    unsigned bufs_rendered = 0;

    while ((geo = geo_buf_get_cons())) {
        opengl_target_begin(geo->screen_width, geo->screen_height);
        glUseProgram(pvr_ta_shader.shader_prog_obj);
        glUniform2f(SCREEN_DIMS_SLOT, (GLfloat)geo->screen_width * 0.5f,
                    (GLfloat)geo->screen_height * 0.5f);
        render_do_draw(geo);
        opengl_target_end();

        framebuffer_set_current(FRAMEBUFFER_CURRENT_HOST);

        /*
         * TODO: I wish I had a good idea for how to handle this without a
         * mutex/condition var
         */
        if (pthread_mutex_lock(&frame_stamp_mtx) != 0)
            abort(); // TODO: error handling
        frame_stamp = geo->frame_stamp;
        if (pthread_cond_signal(&frame_stamp_update_cond) != 0)
            abort(); // TODO: error handling
        if (pthread_mutex_unlock(&frame_stamp_mtx) != 0)
            abort(); // TODO: error handling

        printf("frame_stamp %u rendered\n", frame_stamp);

        geo_buf_consume();
        bufs_rendered++;
    }

    if (bufs_rendered)
        printf("%s - %u geo_bufs rendered\n", __func__, bufs_rendered);
    else
        printf("%s - erm...there's nothing to render here?\n", __func__);
}

void render_wait_for_frame_stamp(unsigned stamp) {
    if (pthread_mutex_lock(&frame_stamp_mtx) != 0)
        abort(); // TODO: error handling
    while (frame_stamp < stamp && dc_is_running()) {
        printf("waiting for frame_stamp %u (current is %u)\n", stamp, frame_stamp);
        pthread_cond_wait(&frame_stamp_update_cond, &frame_stamp_mtx);
    }
    if (pthread_mutex_unlock(&frame_stamp_mtx) != 0)
        abort(); // TODO: error handling
}
