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

#ifndef GEO_BUF_H_
#define GEO_BUF_H_

#include <assert.h>

#include "error.h"
#include "gfx/gfx.h"
#include "gfx/gfx_tex_cache.h"

/*
 * a geo_buf is a pre-allocated buffer used to pass data from the emulation
 * thread to the gfx_thread.  They are stored in a ringbuffer in which the
 * emulation code produces and the rendering code consumes.  Currently this code
 * supports only triangles, but it will eventually grow to encapsulate
 * everything.
 */

/*
 * There are five display lists:
 *
 * Opaque
 * Punch-through polygon
 * Opaque/punch-through modifier volume
 * Translucent
 * Translucent modifier volume
 *
 * They are rendered by the opengl backend in that order.
 * Currently all 5 lists are treated as being opaque; this is obviously
 * incorrect.
 */
struct display_list {
    unsigned n_groups;
};

enum display_list_type {
    DISPLAY_LIST_FIRST,
    DISPLAY_LIST_OPAQUE = DISPLAY_LIST_FIRST,
    DISPLAY_LIST_OPAQUE_MOD,
    DISPLAY_LIST_TRANS,
    DISPLAY_LIST_TRANS_MOD,
    DISPLAY_LIST_PUNCH_THROUGH,
    DISPLAY_LIST_LAST = DISPLAY_LIST_PUNCH_THROUGH,

    // These three list types are invalid, but I do see DISPLAY_LIST_7 sometimes
    DISPLAY_LIST_5,
    DISPLAY_LIST_6,
    DISPLAY_LIST_7,

    DISPLAY_LIST_COUNT,

    DISPLAY_LIST_NONE = -1
};

struct geo_buf {
    // each group of polygons has a distinct texture, shader, depth-sorting etc.
    struct display_list lists[DISPLAY_LIST_COUNT];

    unsigned frame_stamp;
};

void geo_buf_init(struct geo_buf *buf);

unsigned get_cur_frame_stamp(void);

ERROR_INT_ATTR(src_blend_factor);
ERROR_INT_ATTR(dst_blend_factor);
ERROR_INT_ATTR(display_list_index);
ERROR_INT_ATTR(geo_buf_group_index);

#endif
