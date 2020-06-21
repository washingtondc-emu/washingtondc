/*******************************************************************************
 *
 * Copyright 2019 snickerbockers
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

#include <string.h>
#include <stdlib.h>

#include "shader_cache.h"

void shader_cache_init(struct shader_cache *cache) {
    memset(cache, 0, sizeof(*cache));
}

void shader_cache_cleanup(struct shader_cache *cache) {
    struct shader_cache_ent *next = cache->ents;
    while (next) {
        struct shader_cache_ent *ent = next;
        next = next->next;

        shader_cleanup(&ent->shader);
        free(ent);
    }

    memset(cache, 0, sizeof(*cache));
}

struct shader_cache_ent *shader_cache_add_ent(struct shader_cache *cache,
                                              shader_key key) {
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

struct shader_cache_ent *shader_cache_find(struct shader_cache *cache,
                                           shader_key key) {
    struct shader_cache_ent *next = cache->ents;
    while (next) {
        if (next->key == key)
            return next;
        next = next->next;
    }
    return NULL;
}
