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

void gdrom_reg_init(void);
void gdrom_reg_cleanup(void);

int gdrom_reg_read(void *buf, size_t addr, size_t len);
int gdrom_reg_write(void const *buf, size_t addr, size_t len);

// these are GD-ROM DMA registers that lie with in the G1 bus' memory range
struct g1_mem_mapped_reg;

int
gdrom_gdapro_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len);
int
gdrom_gdapro_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                               void const *buf, addr32_t addr, unsigned len);
int
gdrom_g1gdrc_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len);
int
gdrom_g1gdrc_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                               void const *buf, addr32_t addr, unsigned len);
int
gdrom_gdstar_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len);
int
gdrom_gdstar_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                               void const *buf, addr32_t addr, unsigned len);
int
gdrom_gdlen_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                             void *buf, addr32_t addr, unsigned len);
int
gdrom_gdlen_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                              void const *buf, addr32_t addr, unsigned len);
int
gdrom_gddir_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                             void *buf, addr32_t addr, unsigned len);
int
gdrom_gddir_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                              void const *buf, addr32_t addr, unsigned len);
int
gdrom_gden_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len);
int
gdrom_gden_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len);
int
gdrom_gdst_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len);
int
gdrom_gdst_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len);
int
gdrom_gdlend_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len);

#endif
