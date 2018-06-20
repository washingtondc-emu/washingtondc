/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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

#ifndef AICA_H_
#define AICA_H_

#include <stdint.h>

#include "aica_wave_mem.h"

struct arm7;

#define AICA_SYS_LEN 0x8000
#define AICA_SYS_MASK (AICA_SYS_LEN - 1)

struct aica {
    struct aica_wave_mem mem;
    struct arm7 *arm7;

    // the purpose of the + 8 is to prevent buffer over-runs
    uint32_t sys_reg[(AICA_SYS_LEN / 4) + 8];
};

void aica_init(struct aica *aica, struct arm7 *arm7);
void aica_cleanup(struct aica *aica);

extern struct memory_interface aica_sys_intf;

#endif
