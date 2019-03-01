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

static struct gfx_cfg const gfx_cfg_default = {
    .wireframe = 0,
    .tex_enable = 1,
    .depth_enable = 1,
    .blend_enable = 1,
    .bgcolor_enable = 1,
    .color_enable = 1
};

static struct gfx_cfg const gfx_cfg_wireframe = {
    .wireframe = 1,
    .tex_enable = 0,
    .depth_enable = 0,
    .blend_enable = 0,
    .bgcolor_enable = 0,
    .color_enable = 0
};

static struct gfx_cfg cur_profile = gfx_cfg_default;

bool wireframe_mode = false;

void gfx_config_default(void) {
    cur_profile = gfx_cfg_default;
    wireframe_mode = false;
}

void gfx_config_wireframe(void) {
    cur_profile = gfx_cfg_wireframe;
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
