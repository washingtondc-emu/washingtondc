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

#include "hw/pvr2/geo_buf.h"

#include "opengl_renderer.h"

static unsigned volatile frame_stamp;

static pthread_cond_t frame_stamp_update_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t frame_stamp_mtx = PTHREAD_MUTEX_INITIALIZER;

void render_next_geo_buf(void) {
    struct geo_buf *geo = geo_buf_get_cons();

    if (!geo) {
        printf("%s - erm...there's nothing to render here?\n", __func__);
        return;
    }

    printf("Vertex dump:\n");

    unsigned vert_no;
    for (vert_no = 0; vert_no < geo->n_verts; vert_no++) {
        float const *vertp = geo->verts + 3 * vert_no;
        printf("\t(%f, %f, %f)\n", vertp[1], vertp[2], vertp[3]);
    }

    // TODO: I wish I had a good idea for how to handle this without a mutex/condition var
    if (pthread_mutex_lock(&frame_stamp_mtx) != 0)
        abort(); // TODO: error handling
    frame_stamp = geo->frame_stamp;
    if (pthread_cond_signal(&frame_stamp_update_cond) != 0)
        abort(); // TODO: error handling
    if (pthread_mutex_unlock(&frame_stamp_mtx) != 0)
        abort(); // TODO: error handling

    geo_buf_consume();
}

void render_wait_for_frame_stamp(unsigned stamp) {
    if (pthread_mutex_lock(&frame_stamp_mtx) != 0)
        abort(); // TODO: error handling
    while (frame_stamp < stamp)
        pthread_cond_wait(&frame_stamp_update_cond, &frame_stamp_mtx);
    if (pthread_mutex_unlock(&frame_stamp_mtx) != 0)
        abort(); // TODO: error handling
}
