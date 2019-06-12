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
