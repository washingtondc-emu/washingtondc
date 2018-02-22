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
#include <stdlib.h>

#include "dreamcast.h"

#include "geo_buf.h"

static unsigned next_frame_stamp;

DEF_ERROR_INT_ATTR(src_blend_factor);
DEF_ERROR_INT_ATTR(dst_blend_factor);
DEF_ERROR_INT_ATTR(display_list_index);
DEF_ERROR_INT_ATTR(geo_buf_group_index);

void geo_buf_init(struct geo_buf *buf) {
    buf->frame_stamp = ++next_frame_stamp;

#ifdef INVARIANTS
    enum display_list_type disp_list;
    for (disp_list = DISPLAY_LIST_FIRST; disp_list < DISPLAY_LIST_COUNT;
         disp_list++) {
        if (buf->lists[disp_list].n_groups != 0)
            RAISE_ERROR(ERROR_INTEGRITY);
    }
#endif
}

unsigned get_cur_frame_stamp(void) {
    return next_frame_stamp;
}
