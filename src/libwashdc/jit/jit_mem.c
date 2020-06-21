/*******************************************************************************
 *
 * Copyright 2018, 2019 snickerbockers
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
