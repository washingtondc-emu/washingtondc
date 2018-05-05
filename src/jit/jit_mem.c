/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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

#include "jit/jit_il.h"
#include "jit/code_block.h"
#include "mem_areas.h"
#include "dreamcast.h"
#include "memory.h"
#include "MemoryMap.h"

#include "jit_mem.h"

void jit_sh4_mem_read_constaddr_32(struct Sh4 *sh4, struct il_code_block *block,
                                   addr32_t addr, unsigned slot_no) {
    addr32_t addr_first = addr & 0x1fffffff;
    addr32_t addr_last = (addr + 3) & 0x1fffffff;
    if (addr_first >= ADDR_AREA3_FIRST && addr_last <= ADDR_AREA3_LAST) {
        void *ptr = dc_mem.mem + (addr & ADDR_AREA3_MASK);
        jit_load_slot(block, slot_no, ptr);
    } else {
        jit_read_32_constaddr(block, sh4->mem.map, addr, slot_no);
    }
}

void jit_sh4_mem_read_constaddr_16(struct Sh4 *sh4, struct il_code_block *block,
                                   addr32_t addr, unsigned slot_no) {
    addr32_t addr_first = addr & 0x1fffffff;
    addr32_t addr_last = (addr + 1) & 0x1fffffff;
    if (addr_first >= ADDR_AREA3_FIRST && addr_last <= ADDR_AREA3_LAST) {
        void *ptr = dc_mem.mem + (addr & ADDR_AREA3_MASK);
        jit_load_slot16(block, slot_no, ptr);
    } else {
        jit_read_16_constaddr(block, sh4->mem.map, addr, slot_no);
    }
}
