/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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

#include <stddef.h>
#include <stdint.h>

#include "washdc/types.h"
#include "mem_areas.h"
#include "washdc/MemoryMap.h"

/*
 * I don't yet understand the 32-bit/64-bit access area dichotomy, so I'm
 * keeping them separated for now.  They might both map th the same memory, I'm
 * just not sure yet.
 */
struct pvr2_tex_mem {
    uint8_t tex32[ADDR_TEX32_LAST - ADDR_TEX32_FIRST + 1];
    uint8_t tex64[ADDR_TEX64_LAST - ADDR_TEX64_FIRST + 1];
};

uint8_t pvr2_tex_mem_area32_read_8(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area32_write_8(addr32_t addr, uint8_t val, void *ctxt);
uint16_t pvr2_tex_mem_area32_read_16(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area32_write_16(addr32_t addr, uint16_t val, void *ctxt);
uint32_t pvr2_tex_mem_area32_read_32(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area32_write_32(addr32_t addr, uint32_t val, void *ctxt);
float pvr2_tex_mem_area32_read_float(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area32_write_float(addr32_t addr, float val, void *ctxt);
double pvr2_tex_mem_area32_read_double(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area32_write_double(addr32_t addr, double val, void *ctxt);

uint8_t pvr2_tex_mem_area64_read_8(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area64_write_8(addr32_t addr, uint8_t val, void *ctxt);
uint16_t pvr2_tex_mem_area64_read_16(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area64_write_16(addr32_t addr, uint16_t val, void *ctxt);
uint32_t pvr2_tex_mem_area64_read_32(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area64_write_32(addr32_t addr, uint32_t val, void *ctxt);
float pvr2_tex_mem_area64_read_float(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area64_write_float(addr32_t addr, float val, void *ctxt);
double pvr2_tex_mem_area64_read_double(addr32_t addr, void *ctxt);
void pvr2_tex_mem_area64_write_double(addr32_t addr, double val, void *ctxt);

extern struct memory_interface pvr2_tex_mem_area32_intf,
    pvr2_tex_mem_area64_intf;

#endif
