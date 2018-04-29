/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2018 snickerbockers
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

#ifndef MEMORYMAP_H_
#define MEMORYMAP_H_

#include <stdint.h>

#include "memory.h"
#include "mem_areas.h"

typedef float(*memory_map_readfloat_func)(uint32_t addr);
typedef double(*memory_map_readdouble_func)(uint32_t addr);
typedef uint32_t(*memory_map_read32_func)(uint32_t addr);
typedef uint16_t(*memory_map_read16_func)(uint32_t addr);
typedef uint8_t(*memory_map_read8_func)(uint32_t addr);

typedef void(*memory_map_writefloat_func)(uint32_t addr, float val);
typedef void(*memory_map_writedouble_func)(uint32_t addr, double val);
typedef void(*memory_map_write32_func)(uint32_t addr, uint32_t val);
typedef void(*memory_map_write16_func)(uint32_t addr, uint16_t val);
typedef void(*memory_map_write8_func)(uint32_t addr, uint8_t val);

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
typedef float(*memory_map_readfloat_func)(uint32_t addr);
typedef double(*memory_map_readdouble_func)(uint32_t addr);
typedef uint32_t(*memory_map_read32_func)(uint32_t addr);
typedef uint16_t(*memory_map_read16_func)(uint32_t addr);
typedef uint8_t(*memory_map_read8_func)(uint32_t addr);

typedef void(*memory_map_writefloat_func)(uint32_t addr, float val);
typedef void(*memory_map_writedouble_func)(uint32_t addr, double val);
typedef void(*memory_map_write32_func)(uint32_t addr, uint32_t val);
typedef void(*memory_map_write16_func)(uint32_t addr, uint16_t val);
typedef void(*memory_map_write8_func)(uint32_t addr, uint8_t val);

enum memory_map_region_id {
    MEMORY_MAP_REGION_UNKNOWN,
    MEMORY_MAP_REGION_RAM
};

struct memory_map_region {
    uint32_t first_addr, last_addr;

    uint32_t range_mask;

    uint32_t mask;

    enum memory_map_region_id id;

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
};

#define MAX_MEM_MAP_REGIONS 32

struct memory_map {
    struct memory_map_region regions[MAX_MEM_MAP_REGIONS];
    unsigned n_regions;
};

void memory_map_init(struct Memory *mem_new);
void memory_map_set_mem(struct Memory *mem_new);

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

extern struct memory_map sh4_mem_map;

#endif
