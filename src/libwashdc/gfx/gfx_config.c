/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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
#include <stdint.h>
#include <stdbool.h>

#include "gfx_config.h"

static struct gfx_cfg cur_profile = {
    .wireframe = 0,
    .tex_enable = 1,
    .depth_enable = 1,
    .blend_enable = 1,
    .bgcolor_enable = 1,
    .color_enable = 1,
    .depth_sort_enable = 1
};

bool wireframe_mode = false;

void gfx_config_default(void) {
    cur_profile.wireframe = 0;
    cur_profile.tex_enable = 1;
    cur_profile.depth_enable = 1;
    cur_profile.blend_enable = 1;
    cur_profile.bgcolor_enable = 1;
    cur_profile.color_enable = 1;

    wireframe_mode = false;
}

void gfx_config_wireframe(void) {
    cur_profile.wireframe = 1;
    cur_profile.tex_enable = 0;
    cur_profile.depth_enable = 0;
    cur_profile.blend_enable = 0;
    cur_profile.bgcolor_enable = 0;
    cur_profile.color_enable = 0;

    wireframe_mode = true;
}

void gfx_config_toggle_wireframe(void) {
    if (wireframe_mode)
        gfx_config_default();
    else
        gfx_config_wireframe();
}

struct gfx_cfg gfx_config_read(void) {
    return cur_profile;
}

void gfx_config_oit_enable(void) {
    cur_profile.depth_sort_enable = 1;
}
void gfx_config_oit_disable(void) {
    cur_profile.depth_sort_enable = 0;
}
