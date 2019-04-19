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

#include "dc_sched.h"
#include "washdc/types.h"

struct pvr2;
struct pvr2_core_mem_mapped_reg;

enum {
    SPG_HBLANK_INT,
    SPG_VBLANK_INT,
    SPG_HBLANK,
    SPG_VBLANK,
    SPG_LOAD,
    SPG_CONTROL,

    SPG_REG_COUNT
};

struct pvr2_spg {
    reg32_t reg[SPG_REG_COUNT];

    /*
     * this should be either 1 (for 27 MHz pixel clock) or
     * 2 (for 13.5 MHz pixel clock).
     *
     * It corresponds to bit 23 of FB_R_CTRL (pclk_div).
     *
     * Only access this through the get_pclk_div method so that you get the proper
     * value including the interlace/progressive divide.
     */
    unsigned pclk_div;

    dc_cycle_stamp_t last_sync_rounded;

    // whether to double pixels horizontally/vertically
    bool pix_double_x, pix_double_y;

    unsigned raster_x, raster_y;

    SchedEvent hblank_event, vblank_in_event, vblank_out_event;
    bool hblank_event_scheduled, vblank_in_event_scheduled,
        vblank_out_event_scheduled;
};

void spg_init(struct pvr2 *pvr2);
void spg_cleanup(struct pvr2 *pvr2);

// val should be either 1 or 2
void spg_set_pclk_div(struct pvr2 *pvr2, unsigned val);

void spg_set_pix_double_x(struct pvr2 *pvr2, bool val);
void spg_set_pix_double_y(struct pvr2 *pvr2, bool val);

uint32_t get_spg_control(struct pvr2 *pvr2);

uint32_t pvr2_spg_get_hblank_int(struct pvr2 *pvr2);
void pvr2_spg_set_hblank_int(struct pvr2 *pvr2, uint32_t val);

uint32_t pvr2_spg_get_vblank_int(struct pvr2 *pvr2);
void pvr2_spg_set_vblank_int(struct pvr2 *pvr2, uint32_t val);

uint32_t pvr2_spg_get_control(struct pvr2 *pvr2);
void pvr2_spg_set_control(struct pvr2 *pvr2, uint32_t val);

uint32_t pvr2_spg_get_hblank(struct pvr2 *pvr2);
void pvr2_spg_set_hblank(struct pvr2 *pvr2, uint32_t val);

uint32_t pvr2_spg_get_load(struct pvr2 *pvr2);
void pvr2_spg_set_load(struct pvr2 *pvr2, uint32_t val);

uint32_t pvr2_spg_get_vblank(struct pvr2 *pvr2);
void pvr2_spg_set_vblank(struct pvr2 *pvr2, uint32_t val);

uint32_t pvr2_spg_get_status(struct pvr2 *pvr2);

#endif
