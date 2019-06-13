/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
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

#ifndef SHADER_CACHE_H_
#define SHADER_CACHE_H_

#include <GL/gl.h>

#include "washdc/gfx/gl/shader.h"

typedef unsigned shader_key;

#define SHADER_KEY_TEX_ENABLE_SHIFT 0
#define SHADER_KEY_TEX_ENABLE_BIT (1 << SHADER_KEY_TEX_ENABLE_SHIFT)

#define SHADER_KEY_COLOR_ENABLE_SHIFT 1
#define SHADER_KEY_COLOR_ENABLE_BIT (1 << SHADER_KEY_COLOR_ENABLE_SHIFT)

#define SHADER_KEY_PUNCH_THROUGH_SHIFT 2
#define SHADER_KEY_PUNCH_THROUGH_BIT (1 << SHADER_KEY_PUNCH_THROUGH_SHIFT)

enum {
    // only valid if SHADER_KEY_TEX_ENABLE_BIT is set
    SHADER_CACHE_SLOT_BOUND_TEX,
    SHADER_CACHE_SLOT_TEX_INST,

    // only valid if SHADER_KEY_PUNCH_THROUGH_BIT is set
    SHADER_CACHE_SLOT_PT_ALPHA_REF,

    // always valid
    SHADER_CACHE_SLOT_TRANS_MAT,

    SHADER_CACHE_SLOT_COUNT
};

struct shader_cache_ent {
    struct shader_cache_ent *next;
    shader_key key;
    GLint slots[SHADER_CACHE_SLOT_COUNT];
    struct shader shader;
};

struct shader_cache {
    struct shader_cache_ent *ents;
};

void shader_cache_init(struct shader_cache *cache);
void shader_cache_cleanup(struct shader_cache *cache);

struct shader_cache_ent *shader_cache_add_ent(struct shader_cache *cache,
                                              shader_key key);

struct shader_cache_ent *shader_cache_find(struct shader_cache *cache,
                                           shader_key key);

#endif
