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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "hw/pvr2/geo_buf.h"
#include "gfx/gfx_tex_cache.h"
#include "gfx/opengl/opengl_target.h"
#include "gfx/opengl/opengl_renderer.h"
#include "dreamcast.h"

#include "rend_common.h"

static unsigned frame_stamp;

static pthread_cond_t frame_stamp_update_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t frame_stamp_mtx = PTHREAD_MUTEX_INITIALIZER;

static struct rend_if const *rend_ifp = &opengl_rend_if;

// initialize and clean up the graphics renderer
void rend_init(void) {
    rend_ifp->init();
}

void rend_cleanup(void) {
    rend_ifp->cleanup();
}

// tell the renderer to update the given texture from the cache
void rend_update_tex(unsigned tex_no) {
    rend_ifp->update_tex(tex_no);
}

// tell the renderer to release the given texture from the cache
void rend_release_tex(unsigned tex_no) {
    rend_ifp->release_tex(tex_no);
}

// draw the given geo_buf
void rend_do_draw_geo_buf(struct geo_buf *geo) {
    rend_ifp->do_draw_geo_buf(geo);
}

void rend_draw_next_geo_buf(void) {
    struct geo_buf *geo;
    unsigned bufs_rendered = 0;

    while ((geo = geo_buf_get_cons())) {
        unsigned tex_no;
        for (tex_no = 0; tex_no < PVR2_TEX_CACHE_SIZE; tex_no++) {
            struct pvr2_tex *tex = geo->tex_cache + tex_no;
            if (tex->valid && tex->dirty) {
                struct gfx_tex new_tex_entry = {
                    .valid =  true,
                    .w_shift = tex->w_shift,
                    .h_shift = tex->h_shift,
                    .pvr2_pix_fmt = tex->pix_fmt,
                    .pvr2_tex_fmt = tex->tex_fmt,
                    .twiddled = tex->twiddled,
                    .vq_compression = tex->vq_compression,
                    .mipmap = tex->mipmap,
                    .addr_first = tex->addr_first,
                    .addr_last = tex->addr_last,
                    .dat = tex->dat
                };
                tex->dat = NULL;
                tex->dirty = false;
                gfx_tex_cache_add(tex_no, &new_tex_entry);
            }
        }

        opengl_target_begin(geo->screen_width, geo->screen_height);
        rend_do_draw_geo_buf(geo);
        opengl_target_end();

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

        enum display_list_type disp_list;
        for (disp_list = DISPLAY_LIST_FIRST; disp_list < DISPLAY_LIST_COUNT;
             disp_list++) {
            struct display_list *list = geo->lists + disp_list;
            if (list->n_groups) {
                /*
                 * current protocol is that list->groups is only valid if
                 * list->n_groups is non-valid; ergo it's safe to leave a
                 * hangning pointer here.
                 */
                free(list->groups);
                list->n_groups = 0;
            }
        }

        geo_buf_consume();
        bufs_rendered++;
    }

    if (bufs_rendered)
        printf("%s - %u geo_bufs rendered\n", __func__, bufs_rendered);
    else
        printf("%s - erm...there's nothing to render here?\n", __func__);
}

void rend_wait_for_frame_stamp(unsigned stamp) {
    if (pthread_mutex_lock(&frame_stamp_mtx) != 0)
        abort(); // TODO: error handling
    while (frame_stamp < stamp && dc_is_running()) {
        printf("waiting for frame_stamp %u (current is %u)\n", stamp, frame_stamp);
        pthread_cond_wait(&frame_stamp_update_cond, &frame_stamp_mtx);
    }
    if (frame_stamp != stamp) {
        printf("ERROR: missed frame stamp %u (you get %u instead)\n",
               stamp, frame_stamp);
    }
    if (pthread_mutex_unlock(&frame_stamp_mtx) != 0)
        abort(); // TODO: error handling
}
