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

#include <string.h>
#include <stdlib.h>

#include "error.h"

#include "memory.h"



void memory_init(struct Memory *mem, size_t size) {
    mem->mem = (uint8_t*)malloc(sizeof(uint8_t) * size);
    mem->size = size;

    memory_clear(mem);
}

void memory_cleanup(struct Memory *mem) {
    free(mem->mem);
}

void memory_clear(struct Memory *mem) {
    memset(mem->mem, 0, sizeof(mem->mem[0]) * mem->size);
}

size_t memory_size(struct Memory const *mem) {
    return mem->size;
}

int memory_read(struct Memory const *mem, void *buf,
                size_t addr, size_t len) {
    size_t end_addr = addr + (len - 1);
    if (addr >= mem->size || end_addr >= mem->size || end_addr < addr) {
        error_set_address(addr);
        error_set_length(len);
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

    memcpy(buf, mem->mem + addr, len);

    return 0;
}

int memory_write(struct Memory *mem, void const *buf,
                 size_t addr, size_t len) {
    size_t end_addr = addr + (len - 1);
    if (addr >= mem->size || end_addr >= mem->size || end_addr < addr) {
        error_set_address(addr);
        error_set_length(len);
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

    memcpy(mem->mem + addr, buf, len);

    return 0;
}
