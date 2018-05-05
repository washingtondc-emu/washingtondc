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

#ifndef AICA_COMMON_H_
#define AICA_COMMON_H_

#include <stddef.h>
#include <stdint.h>

#include "types.h"
#include "MemoryMap.h"

#define AICA_COMMON_LEN (ADDR_AICA_COMMON_LAST - ADDR_AICA_COMMON_FIRST + 1)

struct aica_common {
    uint8_t backing[AICA_COMMON_LEN];
};

void aica_common_init(struct aica_common *cmn);
void aica_common_cleanup(struct aica_common *cmn);

extern struct memory_interface aica_common_intf;

#endif
