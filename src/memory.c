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

#include <string.h>
#include <stdlib.h>

#include "host_branch_pred.h"

#include "memory.h"

void memory_init(struct Memory *mem) {
    memory_clear(mem);
}

void memory_cleanup(struct Memory *mem) {
}

void memory_clear(struct Memory *mem) {
    memset(mem->mem, 0, sizeof(mem->mem[0]) * MEMORY_SIZE);
}

struct memory_interface ram_intf = {
    .readdouble = memory_read_double,
    .readfloat = memory_read_float,
    .read32 = memory_read_32,
    .read16 = memory_read_16,
    .read8 = memory_read_8,

    .writedouble = memory_write_double,
    .writefloat = memory_write_float,
    .write32 = memory_write_32,
    .write16 = memory_write_16,
    .write8 = memory_write_8
};
