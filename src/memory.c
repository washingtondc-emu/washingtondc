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

#include "host_branch_pred.h"

#include "memory.h"

void memory_init(struct Memory *mem) {
    mem->mem = (uint8_t*)malloc(sizeof(uint8_t) * MEMORY_SIZE);

    memory_clear(mem);
}

void memory_cleanup(struct Memory *mem) {
    free(mem->mem);
}

void memory_clear(struct Memory *mem) {
    memset(mem->mem, 0, sizeof(mem->mem[0]) * MEMORY_SIZE);
}

