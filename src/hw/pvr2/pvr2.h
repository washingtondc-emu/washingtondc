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

#ifndef PVR2_H_
#define PVR2_H_

#include <stdint.h>

#include "dc_sched.h"
#include "pvr2_ta.h"
#include "mem_areas.h"
#include "spg.h"

#define N_PVR2_REGS (ADDR_PVR2_LAST - ADDR_PVR2_FIRST + 1)

struct pvr2 {
    uint32_t reg_backing[N_PVR2_REGS / sizeof(uint32_t)];
    struct dc_clock *clk;
    struct ta_state ta;
    struct pvr2_spg spg;
};

void pvr2_init(struct pvr2 *pvr2, struct dc_clock *clk);
void pvr2_cleanup(struct pvr2 *pvr2);

extern struct dc_clock *pvr2_clk;

#endif
