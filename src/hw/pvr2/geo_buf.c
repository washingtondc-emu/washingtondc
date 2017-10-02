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

#include "dreamcast.h"

#include "geo_buf.h"

#define GEO_BUF_COUNT 4

// producer and consumer indices
static volatile unsigned prod_idx, cons_idx;

static struct geo_buf ringbuf[GEO_BUF_COUNT];

static unsigned next_frame_stamp;

DEF_ERROR_INT_ATTR(src_blend_factor);
DEF_ERROR_INT_ATTR(dst_blend_factor);
DEF_ERROR_INT_ATTR(display_list_index);
DEF_ERROR_INT_ATTR(geo_buf_group_index);

struct geo_buf *geo_buf_get_cons(void) {
    if (prod_idx == cons_idx)
        return NULL;
    else
        return ringbuf + cons_idx;
}

struct geo_buf *geo_buf_get_prod(void) {
    return ringbuf + prod_idx;
}

void geo_buf_consume(void) {
    if (prod_idx == cons_idx) {
        fprintf(stderr, "WARNING: attempt to consume from empty geo_buf "
                "ring\n");
    } else {
        cons_idx = (cons_idx + 1) % GEO_BUF_COUNT;
    }
}

static void init_geo_buf(struct geo_buf *buf) {
    buf->frame_stamp = ++next_frame_stamp;

    buf->clip_min = -1.0f;
    buf->clip_max = 1.0f;

#ifdef INVARIANTS
    enum display_list_type disp_list;
    for (disp_list = DISPLAY_LIST_FIRST; disp_list < DISPLAY_LIST_COUNT;
         disp_list++) {
        if (buf->lists[disp_list].n_groups != 0)
            RAISE_ERROR(ERROR_INTEGRITY);
    }
#endif
}

void geo_buf_produce(void) {
    unsigned next_prod_idx = (prod_idx + 1) % GEO_BUF_COUNT;

    if (next_prod_idx == cons_idx) {
        fprintf(stderr, "WARNING %s: prod_idx == %u, cons_idx == %u.  "
                "This thread will spin while the ring drains...\n", __func__, prod_idx, cons_idx);
        while (next_prod_idx == cons_idx && dc_emu_thread_is_running())
            ;
        fprintf(stderr, "the ring has drained\n");
    }

    prod_idx = next_prod_idx;

    init_geo_buf(ringbuf + prod_idx);
}

unsigned get_cur_frame_stamp(void) {
    return next_frame_stamp;
}
