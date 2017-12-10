/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016, 2017 snickerbockers
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

#ifndef MEMORY_HPP_
#define MEMORY_HPP_

#include <stdint.h>

#include "error.h"
#include "types.h"
#include "mem_code.h"
#include "host_branch_pred.h"

#define MEMORY_SIZE_SHIFT 24
#define MEMORY_SIZE (1 << MEMORY_SIZE_SHIFT)

struct Memory {
    uint8_t mem[MEMORY_SIZE];
};

void memory_init(struct Memory *mem);

void memory_cleanup(struct Memory *mem);

/* zero out all the memory */
void memory_clear(struct Memory *mem);

static inline int
memory_read(struct Memory const *mem, void *buf, size_t addr, size_t len) {
    size_t end_addr = addr + (len - 1);
    if (unlikely(end_addr & ~(MEMORY_SIZE - 1))) {
        error_set_address(addr);
        error_set_length(len);
        PENDING_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
        return MEM_ACCESS_FAILURE;
    }

    memcpy(buf, mem->mem + addr, len);

    return 0;
}

static inline int
memory_write(struct Memory *mem, void const *buf, size_t addr, size_t len) {
    size_t end_addr = addr + (len - 1);
    if (unlikely(end_addr & ~(MEMORY_SIZE - 1))) {
        error_set_address(addr);
        error_set_length(len);
        PENDING_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
        return MEM_ACCESS_FAILURE;
    }

    memcpy(mem->mem + addr, buf, len);

    return 0;
}

static inline uint16_t
memory_read16(struct Memory *mem, addr32_t addr) {
    return ((uint16_t*)mem->mem)[addr >> 1];
}

#endif
