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

static unsigned volatile frame_stamp;

static pthread_cond_t frame_stamp_update_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t frame_stamp_mtx = PTHREAD_MUTEX_INITIALIZER;

static struct shader pvr_ta_shader;

static GLuint vbo, vao;

static GLfloat tri_verts[] = {
    -1.0, -1.0, 0.0,
    0.0, 1.0, 0.0,
    1.0, -1.0, 0.0
};

/*
 * draws the given geo_buf in whatever context is available (ie without setting
 * the shader, or the framebuffer).
 */
static void render_do_draw(struct geo_buf *geo);

void render_init(void) {
    shader_init_from_file(&pvr_ta_shader, "pvr_ta_vert.glsl", "pvr_ta_frag.glsl");

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tri_verts), tri_verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(POSITION_SLOT);
    glVertexAttribPointer(POSITION_SLOT, 3, GL_FLOAT, GL_FALSE,
                          3 * sizeof(GLfloat), (GLvoid*)0);
    glBindVertexArray(0);
}

void render_cleanup(void) {
    shader_cleanup(&pvr_ta_shader);
}

static void render_do_draw(struct geo_buf *geo) {
    glEnableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    glClearColor(0.0, 0.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void render_next_geo_buf(void) {
    struct geo_buf *geo;
    unsigned bufs_rendered = 0;

    while ((geo = geo_buf_get_cons())) {
        /*
         * dump vertices to stdout for now since we're not actually rendering
         * them
         */
        printf("Vertex dump:\n");
        unsigned vert_no;
        for (vert_no = 0; vert_no < geo->n_verts; vert_no++) {
            float const *vertp = geo->verts + 3 * vert_no;
            printf("\t(%f, %f, %f)\n", vertp[1], vertp[2], vertp[3]);
        }

        opengl_target_begin(geo->screen_width, geo->screen_height);
        glUseProgram(pvr_ta_shader.shader_prog_obj);
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
    while (frame_stamp < stamp && dc_is_running())
        pthread_cond_wait(&frame_stamp_update_cond, &frame_stamp_mtx);
    if (pthread_mutex_unlock(&frame_stamp_mtx) != 0)
        abort(); // TODO: error handling
}
