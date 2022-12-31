/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2019, 2022 snickerbockers
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

#ifndef WASHDC_MEMORYMAP_H_
#define WASHDC_MEMORYMAP_H_

#include <stdint.h>
#include <stddef.h>

#include "intmath.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_WATCHPOINTS
#define CHECK_R_WATCHPOINT(addr, type) debug_is_r_watch(addr, sizeof(type))
#define CHECK_W_WATCHPOINT(addr, type) debug_is_w_watch(addr, sizeof(type))
#else
#define CHECK_R_WATCHPOINT(addr, type)
#define CHECK_W_WATCHPOINT(addr, type)
#endif

typedef
float(*memory_map_readfloat_func)(uint32_t addr, void *ctxt);
typedef
double(*memory_map_readdouble_func)(uint32_t addr, void *ctxt);
typedef
uint32_t(*memory_map_read32_func)(uint32_t addr, void *ctxt);
typedef
uint16_t(*memory_map_read16_func)(uint32_t addr, void *ctxt);
typedef
uint8_t(*memory_map_read8_func)(uint32_t addr, void *ctxt);

typedef
void(*memory_map_writefloat_func)(uint32_t addr, float val, void *ctxt);
typedef
void(*memory_map_writedouble_func)(uint32_t addr, double val, void *ctxt);
typedef
void(*memory_map_write32_func)(uint32_t addr, uint32_t val, void *ctxt);
typedef
void(*memory_map_write16_func)(uint32_t addr, uint16_t val, void *ctxt);
typedef
void(*memory_map_write8_func)(uint32_t addr, uint8_t val, void *ctxt);

/*
 * read/write functions which will return an error instead of crashing if the
 * requested address has not been implemented.
 *
 * These functions don't need to be fast because they're primarily intended for
 * the debugger's benefit; this is why they take variable lengths instead of
 * having a special case for each variable type like the real read/write
 * handlers do.
 *
 * return 0 on success, nonzero on error
 */
typedef
int(*memory_map_try_readfloat_func)(uint32_t addr, float *val, void *ctxt);
typedef
int(*memory_map_try_readdouble_func)(uint32_t addr, double *val, void *ctxt);
typedef
int(*memory_map_try_read32_func)(uint32_t addr, uint32_t *val, void *ctxt);
typedef
int(*memory_map_try_read16_func)(uint32_t addr, uint16_t *val, void *ctxt);
typedef
int(*memory_map_try_read8_func)(uint32_t addr, uint8_t *val, void *ctxt);

typedef
int(*memory_map_try_writefloat_func)(uint32_t addr, float val, void *ctxt);
typedef
int(*memory_map_try_writedouble_func)(uint32_t addr, double val, void *ctxt);
typedef
int(*memory_map_try_write32_func)(uint32_t addr, uint32_t val, void *ctxt);
typedef
int(*memory_map_try_write16_func)(uint32_t addr, uint16_t val, void *ctxt);
typedef
int(*memory_map_try_write8_func)(uint32_t addr, uint8_t val, void *ctxt);

enum memory_map_region_id {
    MEMORY_MAP_REGION_UNKNOWN,
    MEMORY_MAP_REGION_RAM
};

struct memory_interface {
    /*
     * TODO: there should also be separate try_read/try_write handlers so we
     * don t crash when the debugger tries to access an invalid address that
     * resolves to a valid memory_map_region.
     */
    memory_map_readdouble_func readdouble;
    memory_map_readfloat_func readfloat;
    memory_map_read32_func read32;
    memory_map_read16_func read16;
    memory_map_read8_func read8;

    memory_map_writedouble_func writedouble;
    memory_map_writefloat_func writefloat;
    memory_map_write32_func write32;
    memory_map_write16_func write16;
    memory_map_write8_func write8;

    memory_map_try_readdouble_func try_readdouble;
    memory_map_try_readfloat_func try_readfloat;
    memory_map_try_read32_func try_read32;
    memory_map_try_read16_func try_read16;
    memory_map_try_read8_func try_read8;

    memory_map_try_writedouble_func try_writedouble;
    memory_map_try_writefloat_func try_writefloat;
    memory_map_try_write32_func try_write32;
    memory_map_try_write16_func try_write16;
    memory_map_try_write8_func try_write8;
};

struct memory_map_region {
    uint32_t first_addr, last_addr;
    uint32_t range_mask;

    // Pointer where regions can store whatever context they may need.
    void *ctxt;

    enum memory_map_region_id id;

    struct memory_interface const *intf;
};

#define MEM_MAP_REGION_BITS 8
#define MEM_MAP_REGION_TBL_LEN (1<<MEM_MAP_REGION_BITS)
#define MEM_MAP_REGION_SHIFT (32 - MEM_MAP_REGION_BITS)
#define MEM_MAP_REGION_MASK ((MEM_MAP_REGION_TBL_LEN-1)<<MEM_MAP_REGION_SHIFT)
#define MEM_MAP_REGION_COUNT (1 << MEM_MAP_REGION_BITS)

struct memory_map {
    struct memory_map_region regions[MEM_MAP_REGION_COUNT];
    unsigned n_regions;

    /*
     * Called when software tries to read/write to an address that is not in
     * any of the regions.
     */
    struct memory_interface const *unmap;
    void *unmap_ctxt;
};

void memory_map_init(struct memory_map *map);
void memory_map_cleanup(struct memory_map *map);

#define RANGE_MASK_NONE BIT_RANGE(0, 31)
#define RANGE_MASK_EXT  BIT_RANGE(0, 28)

#define NO_MASK (~0)

void
memory_map_add(struct memory_map *map,
               uint32_t addr_first,
               uint32_t addr_last,
               uint32_t range_mask,
               enum memory_map_region_id id,
               struct memory_interface const *intf, void *ctxt);

uint8_t
memory_map_read_8(struct memory_map *map, uint32_t addr);
uint16_t
memory_map_read_16(struct memory_map *map, uint32_t addr);
uint32_t
memory_map_read_32(struct memory_map *map, uint32_t addr);
float
memory_map_read_float(struct memory_map *map, uint32_t addr);
double
memory_map_read_double(struct memory_map *map, uint32_t addr);

void
memory_map_write_8(struct memory_map *map, uint32_t addr, uint8_t val);
void
memory_map_write_16(struct memory_map *map, uint32_t addr, uint16_t val);
void
memory_map_write_32(struct memory_map *map, uint32_t addr, uint32_t val);
void
memory_map_write_float(struct memory_map *map, uint32_t addr, float val);
void
memory_map_write_double(struct memory_map *map, uint32_t addr, double val);

/*
 * These functions will return zero if the write was successful and nonzero if
 * it wasn't.  memory_map_write_* would just panic the emulator if something had
 * gone wrong.  This is primarily intended for the benefit of the debugger so
 * that an invalid read coming from the remote GDB frontend doesn't needlessly
 * crash the system.
 *
 * Also, these functions do not check for watchpoints.
 */
int
memory_map_try_write_8(struct memory_map *map, uint32_t addr, uint8_t val);
int
memory_map_try_write_16(struct memory_map *map, uint32_t addr, uint16_t val);
int
memory_map_try_write_32(struct memory_map *map, uint32_t addr, uint32_t val);
int
memory_map_try_write_float(struct memory_map *map, uint32_t addr, float val);
int
memory_map_try_write_double(struct memory_map *map, uint32_t addr, double val);

/*
 * These functions will return zero if the read was successful and nonzero if
 * it wasn't.  memory_map_read_* would just panic the emulator if something had
 * gone wrong.  This is primarily intended for the benefit of the debugger so
 * that an invalid read coming from the remote GDB frontend doesn't needlessly
 * crash the system.
 *
 * Also, these functions do not check for watchpoints.
 */
int
memory_map_try_read_8(struct memory_map *map, uint32_t addr, uint8_t *val);
int
memory_map_try_read_16(struct memory_map *map, uint32_t addr, uint16_t *val);
int
memory_map_try_read_32(struct memory_map *map, uint32_t addr, uint32_t *val);
int
memory_map_try_read_float(struct memory_map *map, uint32_t addr, float *val);
int
memory_map_try_read_double(struct memory_map *map, uint32_t addr, double *val);

static inline struct memory_map_region *
memory_map_get_region(struct memory_map *map,
                      uint32_t first_addr, unsigned n_bytes) {
    uint32_t last_addr = first_addr + (n_bytes - 1);
    unsigned region_no;
    for (region_no = 0; region_no < map->n_regions; region_no++) {
        struct memory_map_region *reg = map->regions + region_no;
        uint32_t range_mask = reg->range_mask;
        if ((first_addr & range_mask) >= reg->first_addr &&
            (last_addr & range_mask) <= reg->last_addr) {
            return reg;
        }
    }
    return NULL;
}

#ifdef __cplusplus
}
#endif

#endif
