/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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

#include "jit/jit_il.h"
#include "jit/code_block.h"
#include "mem_areas.h"
#include "dreamcast.h"
#include "memory.h"
#include "washdc/MemoryMap.h"

#include "jit_mem.h"

/*
 * TODO: this only returns the first MEMORY_MAP_REGION_RAM it finds.  Right now
 * that's not a problem because there's only one MEMORY_MAP_REGION_RAM, but in
 * the future it will be a problem if I make the AICA or PVR2 memory identify
 * themselves as MEMORY_MAP_REGION_RAM.
 */
static struct memory_map_region *find_ram(struct memory_map *map) {
    unsigned region_no;
    for (region_no = 0; region_no < map->n_regions; region_no++) {
        struct memory_map_region *region = map->regions + region_no;
        if (region->id == MEMORY_MAP_REGION_RAM)
            return region;
    }
    return NULL;
}

void jit_mem_read_constaddr_32(struct memory_map *map, struct il_code_block *block,
                               addr32_t addr, unsigned slot_no) {
    struct memory_map_region *ram = find_ram(map);

    if (ram) {
        addr32_t addr_first = addr & ram->range_mask;
        addr32_t addr_last = (addr + 3) & ram->range_mask;

        struct Memory *mem = (struct Memory*)ram->ctxt;
        if (addr_first >= ram->first_addr && addr_last <= ram->last_addr) {
            void *ptr = mem->mem + (addr & ram->mask);
            jit_load_slot(block, slot_no, ptr);
            return;
        }
    }

    jit_read_32_constaddr(block, map, addr, slot_no);
}

void jit_mem_read_constaddr_16(struct memory_map *map, struct il_code_block *block,
                               addr32_t addr, unsigned slot_no) {
    struct memory_map_region *ram = find_ram(map);

    if (ram) {
        addr32_t addr_first = addr & ram->range_mask;
        addr32_t addr_last = (addr + 3) & ram->range_mask;

        struct Memory *mem = (struct Memory*)ram->ctxt;
        if (addr_first >= ram->first_addr && addr_last <= ram->last_addr) {
            void *ptr = mem->mem + (addr & ram->mask);
            jit_load_slot16(block, slot_no, ptr);
            return;
        }
    }

    jit_read_16_constaddr(block, map, addr, slot_no);
}
