/*******************************************************************************
 *
 *
 *    Copyright (C) 2022 snickerbockers
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

#ifndef AREA7_H_
#define AREA7_H_

#include "washdc/MemoryMap.h"

struct Sh4;

/*
 * SH4 memory area 7 - internal SH4 memory
 * 0x1c000000-0x1fffffff
 *
 * contains on-chip mmio, operand-cache-as-ram (when enabled) and also some
 * address arrays you can use to access cache (also maybe mmu too, i forget).
 */

struct area7 {
    struct Sh4 *sh4;
};

void area7_init(struct area7 *area7, struct Sh4 *sh4);
void area7_cleanup(struct area7 *area7);

extern struct memory_interface area7_intf;

#endif
