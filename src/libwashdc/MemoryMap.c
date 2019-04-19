/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2019 snickerbockers
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

#ifdef ENABLE_WATCHPOINTS
#define CHECK_R_WATCHPOINT(addr, type) debug_is_r_watch(addr, sizeof(type))
#define CHECK_W_WATCHPOINT(addr, type) debug_is_w_watch(addr, sizeof(type))
#else
#define CHECK_R_WATCHPOINT(addr, type)
#define CHECK_W_WATCHPOINT(addr, type)
#endif

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
