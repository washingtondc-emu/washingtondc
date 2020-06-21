/*******************************************************************************
 *
 * Copyright 2017-2020 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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

    /*
     * we need a pointer to the maple context because maple has a DMA mode that
     * is triggered one scanline before each vblank-out
     */
    struct maple *maple;

    SchedEvent hblank_event, vblank_in_event,
        vblank_out_event, pre_vblank_out_event;
    bool hblank_event_scheduled, vblank_in_event_scheduled,
        vblank_out_event_scheduled, pre_vblank_out_event_scheduled;
};

void spg_init(struct pvr2 *pvr2, struct maple *maple);
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
