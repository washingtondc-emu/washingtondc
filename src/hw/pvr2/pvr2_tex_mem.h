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

#ifndef PVR2_TEX_MEM_H_
#define PVR2_TEX_MEM_H_

#include "mem_areas.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * I don't yet understand the 32-bit/64-bit access area dichotomy, so I'm
 * keeping them separated for now.  They might both map th the same memory, I'm
 * just not sure yet.
 */

extern uint8_t pvr2_tex32_mem[ADDR_TEX32_LAST - ADDR_TEX32_FIRST + 1];
extern uint8_t pvr2_tex64_mem[ADDR_TEX64_LAST - ADDR_TEX64_FIRST + 1];

int pvr2_tex_mem_area32_read(void *buf, size_t addr, size_t len);
int pvr2_tex_mem_area32_write(void const *buf, size_t addr, size_t len);

int pvr2_tex_mem_area64_read(void *buf, size_t addr, size_t len);
int pvr2_tex_mem_area64_write(void const *buf, size_t addr, size_t len);

#ifdef __cplusplus
}
#endif

#endif
