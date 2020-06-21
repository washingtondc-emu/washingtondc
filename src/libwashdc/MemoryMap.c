/*******************************************************************************
 *
 * Copyright 2016-2019 snickerbockers
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
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#include <stddef.h>

#include "dreamcast.h"
#include "washdc/error.h"
#include "mem_code.h"

#include "washdc/MemoryMap.h"

void memory_map_init(struct memory_map *map) {
    memset(map, 0, sizeof(*map));
}

void memory_map_cleanup(struct memory_map *map) {
    memset(map, 0, sizeof(*map));
}

#define MEMORY_MAP_READ_TMPL(type, type_postfix)                        \
    type memory_map_read_##type_postfix(struct memory_map *map,         \
                                        uint32_t addr) {                \
        uint32_t first_addr = addr;                                     \
        uint32_t last_addr = sizeof(type) - 1 + first_addr;             \
                                                                        \
        unsigned region_no;                                             \
        for (region_no = 0; region_no < map->n_regions; region_no++) {  \
            struct memory_map_region *reg = map->regions + region_no;   \
            uint32_t range_mask = reg->range_mask;                      \
            if ((first_addr & range_mask) >= reg->first_addr &&         \
                (last_addr & range_mask) <= reg->last_addr) {           \
                struct memory_interface const *intf = reg->intf;        \
                uint32_t mask = reg->mask;                              \
                void *ctxt = reg->ctxt;                                 \
                                                                        \
                CHECK_R_WATCHPOINT(addr, type);                         \
                                                                        \
                return intf->read##type_postfix(addr & mask, ctxt);     \
            }                                                           \
        }                                                               \
                                                                        \
        struct memory_interface const *unmap = map->unmap;              \
        if (unmap && unmap->read##type_postfix)                         \
            return unmap->read##type_postfix(addr, map->unmap_ctxt);    \
        else {                                                          \
            error_set_feature("memory mapping");                        \
            error_set_address(addr);                                    \
            error_set_length(sizeof(type));                             \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
    }

MEMORY_MAP_READ_TMPL(uint8_t, 8)
MEMORY_MAP_READ_TMPL(uint16_t, 16)
MEMORY_MAP_READ_TMPL(uint32_t, 32)
MEMORY_MAP_READ_TMPL(float, float)
MEMORY_MAP_READ_TMPL(double, double)

#define MEMORY_MAP_TRY_READ_TMPL(type, type_postfix)                    \
    int memory_map_try_read_##type_postfix(struct memory_map *map,      \
                                           uint32_t addr, type *val) {  \
        uint32_t first_addr = addr;                                     \
        uint32_t last_addr = sizeof(type) - 1 + first_addr;             \
                                                                        \
        unsigned region_no;                                             \
        for (region_no = 0; region_no < map->n_regions; region_no++) {  \
            struct memory_map_region *reg = map->regions + region_no;   \
            uint32_t range_mask = reg->range_mask;                      \
            if ((first_addr & range_mask) >= reg->first_addr &&         \
                (last_addr & range_mask) <= reg->last_addr) {           \
                struct memory_interface const *intf = reg->intf;        \
                uint32_t mask = reg->mask;                              \
                void *ctxt = reg->ctxt;                                 \
                if (intf->try_read##type_postfix) {                     \
                    return intf->try_read##type_postfix(addr & mask,    \
                                                        val, ctxt);     \
                } else {                                                \
                    *val = intf->read##type_postfix(addr & mask, ctxt); \
                }                                                       \
                return 0;                                               \
            }                                                           \
        }                                                               \
                                                                        \
        return 1;                                                       \
    }

MEMORY_MAP_TRY_READ_TMPL(uint8_t, 8)
MEMORY_MAP_TRY_READ_TMPL(uint16_t, 16)
MEMORY_MAP_TRY_READ_TMPL(uint32_t, 32)
MEMORY_MAP_TRY_READ_TMPL(float, float)
MEMORY_MAP_TRY_READ_TMPL(double, double)

#define MEM_MAP_WRITE_TMPL(type, type_postfix)                          \
    void memory_map_write_##type_postfix(struct memory_map *map,        \
                                         uint32_t addr, type val) {     \
        uint32_t first_addr = addr;                                     \
        uint32_t last_addr = sizeof(type) - 1 + first_addr;             \
                                                                        \
        unsigned region_no;                                             \
        for (region_no = 0; region_no < map->n_regions; region_no++) {  \
            struct memory_map_region *reg = map->regions + region_no;   \
            uint32_t range_mask = reg->range_mask;                      \
            if ((first_addr & range_mask) >= reg->first_addr &&         \
                (last_addr & range_mask) <= reg->last_addr) {           \
                struct memory_interface const *intf = reg->intf;        \
                uint32_t mask = reg->mask;                              \
                void *ctxt = reg->ctxt;                                 \
                                                                        \
                CHECK_W_WATCHPOINT(addr, type);                         \
                                                                        \
                intf->write##type_postfix(addr & mask, val, ctxt);      \
                return;                                                 \
            }                                                           \
        }                                                               \
                                                                        \
        struct memory_interface const *unmap = map->unmap;              \
        if (unmap && unmap->read##type_postfix)                         \
            unmap->write##type_postfix(addr, val, map->unmap_ctxt);     \
        else {                                                          \
            error_set_feature("memory mapping");                        \
            error_set_address(addr);                                    \
            error_set_length(sizeof(type));                             \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
    }

MEM_MAP_WRITE_TMPL(uint8_t, 8)
MEM_MAP_WRITE_TMPL(uint16_t, 16)
MEM_MAP_WRITE_TMPL(uint32_t, 32)
MEM_MAP_WRITE_TMPL(float, float)
MEM_MAP_WRITE_TMPL(double, double)

#define MEM_MAP_TRY_WRITE_TMPL(type, type_postfix)                      \
    int memory_map_try_write_##type_postfix(struct memory_map *map,     \
                                            uint32_t addr, type val) {  \
        uint32_t first_addr = addr;                                     \
        uint32_t last_addr = sizeof(type) - 1 + first_addr;             \
                                                                        \
        unsigned region_no;                                             \
        for (region_no = 0; region_no < map->n_regions; region_no++) {  \
            struct memory_map_region *reg = map->regions + region_no;   \
            uint32_t range_mask = reg->range_mask;                      \
            if ((first_addr & range_mask) >= reg->first_addr &&         \
                (last_addr & range_mask) <= reg->last_addr) {           \
                struct memory_interface const *intf = reg->intf;        \
                uint32_t mask = reg->mask;                              \
                void *ctxt = reg->ctxt;                                 \
                if (intf->try_write##type_postfix) {                    \
                    return intf->try_write##type_postfix(addr & mask,   \
                                                         val, ctxt);    \
                } else {                                                \
                    intf->write##type_postfix(addr & mask, val, ctxt);  \
                }                                                       \
                return 0;                                               \
            }                                                           \
        }                                                               \
        return 1;                                                       \
    }                                                                   \

MEM_MAP_TRY_WRITE_TMPL(uint8_t, 8)
MEM_MAP_TRY_WRITE_TMPL(uint16_t, 16)
MEM_MAP_TRY_WRITE_TMPL(uint32_t, 32)
MEM_MAP_TRY_WRITE_TMPL(float, float)
MEM_MAP_TRY_WRITE_TMPL(double, double)

void
memory_map_add(struct memory_map *map,
               uint32_t addr_first,
               uint32_t addr_last,
               uint32_t range_mask,
               uint32_t mask,
               enum memory_map_region_id id,
               struct memory_interface const *intf,
               void *ctxt) {
    if (map->n_regions >= MAX_MEM_MAP_REGIONS)
        RAISE_ERROR(ERROR_OVERFLOW);

    struct memory_map_region *reg = map->regions + map->n_regions++;

    reg->first_addr = addr_first;
    reg->last_addr = addr_last;
    reg->range_mask = range_mask;
    reg->mask = mask;
    reg->id = id;
    reg->intf = intf;
    reg->ctxt = ctxt;
}
