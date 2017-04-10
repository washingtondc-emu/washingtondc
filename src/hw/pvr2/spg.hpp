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

#ifndef SPG_HPP_
#define SPG_HPP_

#include "types.hpp"
#include "dc_sched.hpp"

void spg_init();
void spg_cleanup();

// val should be either 1 or 2
void spg_set_pclk_div(unsigned val);

void spg_set_pix_double_x(bool val);
void spg_set_pix_double_y(bool val);

uint32_t get_spg_control();

int
read_spg_hblank_int(struct pvr2_core_mem_mapped_reg const *reg_info,
                    void *buf, addr32_t addr, unsigned len);
int
write_spg_hblank_int(struct pvr2_core_mem_mapped_reg const *reg_info,
                     void const *buf, addr32_t addr, unsigned len);
int
read_spg_vblank_int(struct pvr2_core_mem_mapped_reg const *reg_info,
                    void *buf, addr32_t addr, unsigned len);
int
write_spg_vblank_int(struct pvr2_core_mem_mapped_reg const *reg_info,
                     void const *buf, addr32_t addr, unsigned len);
int
read_spg_control(struct pvr2_core_mem_mapped_reg const *reg_info,
                 void *buf, addr32_t addr, unsigned len);
int
write_spg_control(struct pvr2_core_mem_mapped_reg const *reg_info,
                  void const *buf, addr32_t addr, unsigned len);
int
read_spg_hblank(struct pvr2_core_mem_mapped_reg const *reg_info,
                void *buf, addr32_t addr, unsigned len);
int
write_spg_hblank(struct pvr2_core_mem_mapped_reg const *reg_info,
                 void const *buf, addr32_t addr, unsigned len);
int
read_spg_vblank(struct pvr2_core_mem_mapped_reg const *reg_info,
                void *buf, addr32_t addr, unsigned len);
int
write_spg_vblank(struct pvr2_core_mem_mapped_reg const *reg_info,
                 void const *buf, addr32_t addr, unsigned len);
int
read_spg_vo_startx(struct pvr2_core_mem_mapped_reg const *reg_info,
                   void *buf, addr32_t addr, unsigned len);
int
write_spg_vo_startx(struct pvr2_core_mem_mapped_reg const *reg_info,
                    void const *buf, addr32_t addr, unsigned len);
int
read_spg_vo_starty(struct pvr2_core_mem_mapped_reg const *reg_info,
                   void *buf, addr32_t addr, unsigned len);
int
write_spg_vo_starty(struct pvr2_core_mem_mapped_reg const *reg_info,
                    void const *buf, addr32_t addr, unsigned len);
int
read_spg_load(struct pvr2_core_mem_mapped_reg const *reg_info,
              void *buf, addr32_t addr, unsigned len);
int
write_spg_load(struct pvr2_core_mem_mapped_reg const *reg_info,
               void const *buf, addr32_t addr, unsigned len);

#endif
