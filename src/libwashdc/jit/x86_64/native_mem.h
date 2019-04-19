/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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

#ifndef NATIVE_MEM_H_
#define NATIVE_MEM_H_

#include "washdc/MemoryMap.h"

void native_mem_init(void);
void native_mem_cleanup(void);

void native_mem_register(struct memory_map const *map);

/*
 * Normal calling convention rules about which registers are and are not saved
 * apply here.  Obviously dst_reg will not get preserved no matter what.
 */
void native_mem_read_16(struct memory_map const *map);
void native_mem_read_32(struct memory_map const *map);
void native_mem_write_32(struct memory_map const *map);

#endif
