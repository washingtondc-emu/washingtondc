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

#include <string.h>
#include <stdint.h>
#include <stdatomic.h>

#include "gfx_config.h"

static struct gfx_cfg const gfx_cfg_default = {
    .wireframe = false,
    .tex_enable = true,
    .depth_enable = true,
    .blend_enable = true,
    .bgcolor_enable = true,
    .color_enable = true
};

static struct gfx_cfg const gfx_cfg_wireframe = {
    .wireframe = true,
    .tex_enable = false,
    .depth_enable = false,
    .blend_enable = false,
    .bgcolor_enable = false,
    .color_enable = false
};

static atomic_uintptr_t cur_profile_intptr =
    ATOMIC_VAR_INIT((uintptr_t)&gfx_cfg_default);

void gfx_config_default(void) {
    atomic_store_explicit(&cur_profile_intptr, (uintptr_t)&gfx_cfg_default,
                          memory_order_relaxed);
}

void gfx_config_wireframe(void) {
    atomic_store_explicit(&cur_profile_intptr, (uintptr_t)&gfx_cfg_wireframe,
                          memory_order_relaxed);
}

void gfx_config_read(struct gfx_cfg *cfg) {
    void *cur_profile = (void*)atomic_load_explicit(&cur_profile_intptr,
                                                    memory_order_relaxed);
    memcpy(cfg, cur_profile, sizeof(&cfg));
}
