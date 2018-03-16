/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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

#include "gfx/opengl/font/font.h"

#include "overlay.h"

static float framerate;
static bool not_hidden = true;

void overlay_show(bool do_show) {
    not_hidden = do_show;
}

void overlay_draw(unsigned screen_width, unsigned screen_height) {
    if (not_hidden) {
        char tmp[64];
        memset(tmp, 0, sizeof(tmp));
        snprintf(tmp, sizeof(tmp), "%u", (unsigned)framerate);
        tmp[63] = '\0';

        font_render(tmp, 0, 0, screen_width, screen_height);
    }
}

void overlay_set_fps(float fps) {
    framerate = fps;
}
