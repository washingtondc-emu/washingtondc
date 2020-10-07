/*******************************************************************************
 *
 * Copyright 2019, 2020 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 ******************************************************************************/

#ifndef SHADER_CACHE_H_
#define SHADER_CACHE_H_

#ifdef _WIN32
#include "i_hate_windows.h"
#endif

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

enum {
    // only valid if SHADER_KEY_TEX_ENABLE_BIT is set
    SHADER_CACHE_SLOT_BOUND_TEX,

    // only valid if SHADER_KEY_PUNCH_THROUGH_BIT is set
    SHADER_CACHE_SLOT_PT_ALPHA_REF,

    // always valid
    SHADER_CACHE_SLOT_TRANS_MAT,

    // only valid if SHADER_KEY_USER_CLIP_ENABLE_BIT is set
    SHADER_CACHE_SLOT_USER_CLIP,

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
