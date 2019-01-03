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

#ifndef SPG_H_
#define SPG_H_

#include <stdint.h>
#include <stdbool.h>

#include "types.h"

struct pvr2_core_mem_mapped_reg;

void spg_init();
void spg_cleanup();

// val should be either 1 or 2
void spg_set_pclk_div(unsigned val);

void spg_set_pix_double_x(bool val);
void spg_set_pix_double_y(bool val);

uint32_t get_spg_control();

uint32_t pvr2_spg_get_hblank_int(void);
void pvr2_spg_set_hblank_int(uint32_t val);

uint32_t pvr2_spg_get_vblank_int(void);
void pvr2_spg_set_vblank_int(uint32_t val);

uint32_t pvr2_spg_get_control(void);
void pvr2_spg_set_control(uint32_t val);

uint32_t pvr2_spg_get_hblank(void);
void pvr2_spg_set_hblank(uint32_t val);

uint32_t pvr2_spg_get_load(void);
void pvr2_spg_set_load(uint32_t val);

uint32_t pvr2_spg_get_vblank(void);
void pvr2_spg_set_vblank(uint32_t val);

uint32_t pvr2_spg_get_status(void);

#endif
