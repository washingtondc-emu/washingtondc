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

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Memory {
    size_t size;
    uint8_t *mem;
};

void memory_init(struct Memory *mem, size_t size);

void memory_cleanup(struct Memory *mem);

/* zero out all the memory */
void memory_clear(struct Memory *mem);

size_t memory_size(struct Memory const *mem);

int memory_read(struct Memory const *mem, void *buf, size_t addr, size_t len);
int memory_write(struct Memory *mem, void const *buf, size_t addr, size_t len);

#ifdef __cplusplus
}
#endif

#endif
