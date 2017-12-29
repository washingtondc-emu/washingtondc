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

#ifndef GDROM_REG_H_
#define GDROM_REG_H_

#include "types.h"
#include "hw/g1/g1_reg.h"

void gdrom_reg_init(void);
void gdrom_reg_cleanup(void);

float gdrom_reg_read_float(addr32_t addr);
void gdrom_reg_write_float(addr32_t addr, float val);
double gdrom_reg_read_double(addr32_t addr);
void gdrom_reg_write_double(addr32_t addr, double val);
uint8_t gdrom_reg_read_8(addr32_t addr);
void gdrom_reg_write_8(addr32_t addr, uint8_t val);
uint16_t gdrom_reg_read_16(addr32_t addr);
void gdrom_reg_write_16(addr32_t addr, uint16_t val);
uint32_t gdrom_reg_read_32(addr32_t addr);
void gdrom_reg_write_32(addr32_t addr, uint32_t val);

// these are GD-ROM DMA registers that lie with in the G1 bus' memory range
struct g1_mem_mapped_reg;

uint32_t
gdrom_gdapro_mmio_read(struct mmio_region_g1_reg_32 *region, unsigned idx);

void
gdrom_gdapro_mmio_write(struct mmio_region_g1_reg_32 *region,
                        unsigned idx, uint32_t val);
uint32_t
gdrom_g1gdrc_mmio_read(struct mmio_region_g1_reg_32 *region, unsigned idx);
void
gdrom_g1gdrc_mmio_write(struct mmio_region_g1_reg_32 *region,
                        unsigned idx, uint32_t val);
uint32_t
gdrom_gdstar_mmio_read(struct mmio_region_g1_reg_32 *region, unsigned idx);
void
gdrom_gdstar_mmio_write(struct mmio_region_g1_reg_32 *region,
                        unsigned idx, uint32_t val);
uint32_t
gdrom_gdlen_mmio_read(struct mmio_region_g1_reg_32 *region, unsigned idx);
void
gdrom_gdlen_mmio_write(struct mmio_region_g1_reg_32 *region,
                       unsigned idx, uint32_t val);
uint32_t
gdrom_gddir_mmio_read(struct mmio_region_g1_reg_32 *region, unsigned idx);
void
gdrom_gddir_mmio_write(struct mmio_region_g1_reg_32 *region,
                       unsigned idx, uint32_t val);
uint32_t
gdrom_gden_mmio_read(struct mmio_region_g1_reg_32 *region, unsigned idx);
void
gdrom_gden_mmio_write(struct mmio_region_g1_reg_32 *region,
                      unsigned idx, uint32_t val);
uint32_t
gdrom_gdst_reg_read_handler(struct mmio_region_g1_reg_32 *region,
                            unsigned idx);
void
gdrom_gdst_reg_write_handler(struct mmio_region_g1_reg_32 *region,
                             unsigned idx, uint32_t val);
uint32_t
gdrom_gdlend_mmio_read(struct mmio_region_g1_reg_32 *region, unsigned idx);

#endif
