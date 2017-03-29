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

#ifndef SH4_TMU_HPP_
#define SH4_TMU_HPP_

#include "sh4_reg.hpp"
#include "dc_sched.hpp"

struct Sh4;

struct sh4_tmu {
    unsigned tchan_accum[3];
};

void sh4_tmu_init(Sh4 *sh4);
void sh4_tmu_cleanup(Sh4 *sh4);

int sh4_tmu_tocr_read_handler(Sh4 *sh4, void *buf,
                              struct Sh4MemMappedReg const *reg_info);
int sh4_tmu_tocr_write_handler(Sh4 *sh4, void const *buf,
                               struct Sh4MemMappedReg const *reg_info);

int sh4_tmu_tstr_read_handler(Sh4 *sh4, void *buf,
                              struct Sh4MemMappedReg const *reg_info);
int sh4_tmu_tstr_write_handler(Sh4 *sh4, void const *buf,
                               struct Sh4MemMappedReg const *reg_info);

int sh4_tmu_tcr_read_handler(Sh4 *sh4, void *buf,
                             struct Sh4MemMappedReg const *reg_info);
int sh4_tmu_tcr_write_handler(Sh4 *sh4, void const *buf,
                              struct Sh4MemMappedReg const *reg_info);

void sh4_tmu_tick(SchedEvent *event);

#endif
