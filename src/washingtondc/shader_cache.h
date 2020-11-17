/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019, 2020 snickerbockers
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

#ifdef _WIN32
#include "i_hate_windows.h"
#endif

#include <string.h>
#include <stdlib.h>

#include <GL/gl.h>

#include "../shader.h"

typedef unsigned shader_key;

#define SHADER_KEY_TEX_ENABLE_SHIFT 0
#define SHADER_KEY_TEX_ENABLE_BIT (1 << SHADER_KEY_TEX_ENABLE_SHIFT)

#define SHADER_KEY_COLOR_ENABLE_SHIFT 1
#define SHADER_KEY_COLOR_ENABLE_BIT (1 << SHADER_KEY_COLOR_ENABLE_SHIFT)

#define SHADER_KEY_PUNCH_THROUGH_SHIFT 2
#define SHADER_KEY_PUNCH_THROUGH_BIT (1 << SHADER_KEY_PUNCH_THROUGH_SHIFT)

// two bits
#define SHADER_KEY_TEX_INST_SHIFT 3
#define SHADER_KEY_TEX_INST_MASK (3 << SHADER_KEY_TEX_INST_SHIFT)
#define SHADER_KEY_TEX_INST_DECAL_BIT (0 << SHADER_KEY_TEX_INST_SHIFT)
#define SHADER_KEY_TEX_INST_MOD_BIT (1 << SHADER_KEY_TEX_INST_SHIFT)
#define SHADER_KEY_TEX_INST_DECAL_ALPHA_BIT (2 << SHADER_KEY_TEX_INST_SHIFT)
#define SHADER_KEY_TEX_INST_MOD_ALPHA_BIT (3 << SHADER_KEY_TEX_INST_SHIFT)

#define SHADER_KEY_USER_CLIP_ENABLE_SHIFT 5
#define SHADER_KEY_USER_CLIP_ENABLE_BIT (1 << SHADER_KEY_USER_CLIP_ENABLE_SHIFT)

#define SHADER_KEY_USER_CLIP_INVERT_SHIFT 6
#define SHADER_KEY_USER_CLIP_INVERT_BIT (1 << SHADER_KEY_USER_CLIP_INVERT_SHIFT)

#define SHADER_KEY_OIT_SHIFT 7
#define SHADER_KEY_OIT_BIT (1 << SHADER_KEY_OIT_SHIFT)

enum {
    // only valid if SHADER_KEY_TEX_ENABLE_BIT is set
    SHADER_CACHE_SLOT_BOUND_TEX,

    // only valid if SHADER_KEY_PUNCH_THROUGH_BIT is set
    SHADER_CACHE_SLOT_PT_ALPHA_REF,

    // always valid
    SHADER_CACHE_SLOT_TRANS_MAT,

    // only valid if SHADER_KEY_USER_CLIP_ENABLE_BIT is set
    SHADER_CACHE_SLOT_USER_CLIP,

    SHADER_CACHE_SLOT_MAX_OIT_NODES,

    SHADER_CACHE_SLOT_SRC_BLEND_FACTOR,

    SHADER_CACHE_SLOT_DST_BLEND_FACTOR,

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

static void shader_cache_init(struct shader_cache *cache) {
    memset(cache, 0, sizeof(*cache));
}

static void shader_cache_cleanup(struct shader_cache *cache) {
    struct shader_cache_ent *next = cache->ents;
    while (next) {
        struct shader_cache_ent *ent = next;
        next = next->next;

        shader_cleanup(&ent->shader);
        free(ent);
    }

    memset(cache, 0, sizeof(*cache));
}

static struct shader_cache_ent *
shader_cache_add_ent(struct shader_cache *cache, shader_key key) {
    struct shader_cache_ent *ent =
        (struct shader_cache_ent*)calloc(1, sizeof(struct shader_cache_ent));
    ent->next = cache->ents;
    cache->ents = ent;
    ent->key = key;

    int slot_no;
    for (slot_no = 0; slot_no < SHADER_CACHE_SLOT_COUNT; slot_no++)
        ent->slots[slot_no] = -1;

    return ent;
}

static struct shader_cache_ent *
shader_cache_find(struct shader_cache *cache, shader_key key) {
    struct shader_cache_ent *next = cache->ents;
    while (next) {
        if (next->key == key)
            return next;
        next = next->next;
    }
    return NULL;
}

#endif
