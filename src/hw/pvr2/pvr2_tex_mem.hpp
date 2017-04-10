/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#ifndef PVR2_TEX_MEM_HPP_
#define PVR2_TEX_MEM_HPP_

#include "MemoryMap.hpp"

extern uint8_t pvr2_tex_mem[ADDR_TEX_LAST - ADDR_TEX_FIRST + 1];

int pvr2_tex_mem_read(void *buf, size_t addr, size_t len);
int pvr2_tex_mem_write(void const *buf, size_t addr, size_t len);

#endif
