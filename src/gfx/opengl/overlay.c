/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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

#include <string.h>
#include <stdio.h>

#include "dreamcast.h"
#include "hw/pvr2/pvr2.h"
#include "gfx/opengl/font/font.h"

#include "overlay.h"

static double framerate, virt_framerate;
static bool not_hidden = true;

void overlay_show(bool do_show) {
    not_hidden = do_show;
}

void overlay_draw(unsigned screen_width, unsigned screen_height) {
    if (not_hidden) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%.2f / %.2f", framerate, virt_framerate);
        tmp[63] = '\0';

        font_render(tmp, 0, 0, screen_width, screen_height);

        struct pvr2_stat stat;
        dc_get_pvr2_stats(&stat);

        /*
         * TODO: put in list names when we have a font that can display text
         * characters
         */
        snprintf(tmp, sizeof(tmp), "%u",
                 stat.poly_count[DISPLAY_LIST_OPAQUE]);
        tmp[63] = '\0';
        font_render(tmp, 0, 1, screen_width, screen_height);

        snprintf(tmp, sizeof(tmp), "%u",
                 stat.poly_count[DISPLAY_LIST_OPAQUE_MOD]);
        tmp[63] = '\0';
        font_render(tmp, 0, 2, screen_width, screen_height);

        snprintf(tmp, sizeof(tmp), "%u",
                 stat.poly_count[DISPLAY_LIST_TRANS]);
        tmp[63] = '\0';
        font_render(tmp, 0, 3, screen_width, screen_height);

        snprintf(tmp, sizeof(tmp), "%u",
                 stat.poly_count[DISPLAY_LIST_TRANS_MOD]);
        tmp[63] = '\0';
        font_render(tmp, 0, 4, screen_width, screen_height);

        snprintf(tmp, sizeof(tmp), "%u",
                 stat.poly_count[DISPLAY_LIST_PUNCH_THROUGH]);
        tmp[63] = '\0';
        font_render(tmp, 0, 5, screen_width, screen_height);
    }
}

void overlay_set_fps(double fps) {
    framerate = fps;
}

void overlay_set_virt_fps(double fps) {
    virt_framerate = fps;
}
