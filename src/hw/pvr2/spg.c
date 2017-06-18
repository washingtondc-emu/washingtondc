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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "error.h"
#include "dreamcast.h"
#include "video/opengl/framebuffer.h"
#include "window.h"
#include "hw/sh4/sh4.h"
#include "hw/sys/holly_intc.h"

#include "spg.h"

/* This is the code which generates the H-BLANK and V-BLANK interrupts */

static DEF_ERROR_INT_ATTR(raster_x_expect)
static DEF_ERROR_INT_ATTR(raster_y_expect)
static DEF_ERROR_INT_ATTR(raster_x_actual)
static DEF_ERROR_INT_ATTR(raster_y_actual)
static DEF_ERROR_INT_ATTR(hblank_int_comp_val)
static DEF_ERROR_INT_ATTR(hblank_int_mode)

/*
 * algorithm:
 * raster x pos is 0, y pos is 0
 * move horizontally for hcount (SPG_LOAD & 0x3ff) cycles
 * H-BLANK interrupt
 * system is in H-BLANK (H-SYNC?) state for unknown number of cycles
 * raster x pos is now 0 again, y pos is incremented
 * repeat previous steps until y pos is (SPG_LOAD >> 16) & 0x3ff)
 * V-BLANK interrupt
 * System is in V-BLANK (V-SYNC?) state for unknown number of cycles
 * reset x pos, y pos to 0 and go back to beginning.
 *
 * Corrections:
 * The V-BLANK/H-BLANK interrupts happen when the raster is at vbstart/hbstart
 * they continue until the raster reaches vbend/hbend ?
 * vbstart and vbend come from SPG_VBLANK
 * hbstart and hbend come from SPG_HBLANK
 * vertical and horizontal raster positions still roll over at vcount and hcount, respectively
 * However, the actuall interrupts happen based on the SPG_HBLANK_INT and SPG_VBLANK_INT registers?
 */

/*
 * Ugh.  This if statement is really painful to write.  the video clock
 * is supposed to be 27 MHz, which doesn't evenly divide from 200 MHz.
 * I tick it on every 7th cycle, which means that the video clock is
 * actually running a little fast at approx 28.57 MHz.
 *
 * A better way to do this would probably be to track the missed cycles
 * and let them accumulate so that sometimes the video clock ticks
 * after 7 cycles and sometimes it ticks after 8 cycles.  This is not
 * that complicated to do, but my head is in no state to do Algebra
 * right now.
 *
 * The perfect way to do this would be to divide both 27 MHz and
 * 200 MHz from their LCD (which is 5400 MHz according to
 * Wolfram Alpha).  *Maybe* this will be feasible later when I have a
 * scheduler implemented; I can't think of a good reason why it wouldn't
 * be, but it does sound too good to be true.  I'm a mess right now
 * after spending an entire weekend stressing out over this and
 * VBLANK/HBLANK timings so I'm in no mood to contemplate the
 * possibilities.
 */
#define SPG_VCLK_DIV 7

/*
 * this should be either 1 (for 27 MHz pixel clock) or
 * 2 (for 13.5 MHz pixel clock).
 *
 * It corresponds to bit 23 of FB_R_CTRL (pclk_div)
 */
static unsigned pclk_div = 2;

static dc_cycle_stamp_t last_sync;

// whether to double pixels horizontally/vertically
static bool pix_double_x, pix_double_y;

enum {
    SPG_HBLANK_INT,
    SPG_VBLANK_INT,
    SPG_HBLANK,
    SPG_VBLANK,
    SPG_LOAD,
    SPG_CONTROL,

    SPG_REG_COUNT
};

// TODO: put in an initialization function
static reg32_t spg_reg[SPG_REG_COUNT] = {
    0x31d << 16,          // SPG_HBLANK_INT
    0x00150104,           // SPG_VBLANK_INT
    0x007e0345,           // SPG_HBLANK
    0x00150104,           // SPG_VBLANK
    (0x106 << 16) | 0x359 // SPG_LOAD
};

static unsigned raster_x, raster_y;

static SchedEvent hblank_event, vblank_in_event, vblank_out_event;

static bool hblank_event_scheduled, vblank_in_event_scheduled,
             vblank_out_event_scheduled;

static void spg_sync();

static inline unsigned get_hcount();
static inline unsigned get_vcount();
static inline unsigned get_hblank_int_pix();
static inline unsigned get_hblank_int_mode();
static inline unsigned get_hblank_int_comp_val();
static inline unsigned get_vblank_in_int_line();
static inline unsigned get_vblank_out_int_line();
static inline unsigned get_hbstart(void);
static inline unsigned get_hbend(void);
static inline unsigned get_vbstart(void);
static inline unsigned get_vbend(void);

static void sched_next_hblank_event();
static void sched_next_vblank_in_event();
static void sched_next_vblank_out_event();

static void spg_handle_hblank(SchedEvent *event);
static void spg_handle_vblank_in(SchedEvent *event);
static void spg_handle_vblank_out(SchedEvent *event);

static void spg_unsched_all();

void spg_init() {
    hblank_event.handler = spg_handle_hblank;
    vblank_in_event.handler = spg_handle_vblank_in;
    vblank_out_event.handler = spg_handle_vblank_out;

    sched_next_hblank_event();
    sched_next_vblank_in_event();
    sched_next_vblank_out_event();
}

void spg_cleanup() {
}

static void spg_unsched_all() {
    if (hblank_event_scheduled) {
        cancel_event(&hblank_event);
        hblank_event_scheduled = false;
    }

    if (vblank_in_event_scheduled) {
        cancel_event(&vblank_in_event);
        vblank_in_event_scheduled = false;
    }

    if (vblank_out_event_scheduled) {
        cancel_event(&vblank_out_event);
        vblank_out_event_scheduled = false;
    }
}

static void spg_sync() {
    unsigned hcount = get_hcount();
    unsigned vcount = get_vcount();
    dc_cycle_stamp_t cur_time = dc_cycle_stamp();
    dc_cycle_stamp_t last_sync_rounded = (pclk_div * SPG_VCLK_DIV) *
        (last_sync / (pclk_div * SPG_VCLK_DIV));

    dc_cycle_stamp_t delta_cycles = cur_time - last_sync_rounded;

    // only update the last_sync timestamp if the values have changed
    unsigned raster_x_inc = delta_cycles / (pclk_div * SPG_VCLK_DIV);
    if (raster_x_inc > 0) {
        last_sync = cur_time;

        raster_x += raster_x_inc;
        raster_y += raster_x / hcount;
        raster_x %= hcount;
        raster_y %= vcount;
    }
}

static void spg_handle_hblank(SchedEvent *event) {
    spg_sync();

#ifdef INVARIANTS
    unsigned hblank_int_mode = get_hblank_int_mode();
    unsigned hblank_int_comp_val = get_hblank_int_comp_val();

    if (raster_x != 0) {
        error_set_raster_x_expect(0);
        error_set_raster_x_actual(raster_x);
        error_set_raster_y_actual(raster_y);
        error_set_hblank_int_comp_val(hblank_int_comp_val);
        error_set_hblank_int_mode(hblank_int_mode);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    switch (hblank_int_mode) {
    case 0:
        if (raster_y != hblank_int_comp_val) {
            error_set_raster_y_expect(hblank_int_comp_val);
            error_set_raster_y_actual(raster_y);
            error_set_hblank_int_comp_val(hblank_int_comp_val);
            error_set_hblank_int_mode(hblank_int_mode);
            RAISE_ERROR(ERROR_INTEGRITY);
        }
        break;
    case 1:
        if (hblank_int_comp_val && (raster_y % hblank_int_comp_val != 0)) {
            error_set_raster_y_actual(raster_y);
            error_set_hblank_int_comp_val(hblank_int_comp_val);
            error_set_hblank_int_mode(hblank_int_mode);
            RAISE_ERROR(ERROR_INTEGRITY);
        }
        break;
    case 2:
        break;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
#endif

    holly_raise_nrm_int(HOLLY_NRM_INT_HBLANK);

    sched_next_hblank_event();
}

static void spg_handle_vblank_in(SchedEvent *event) {
    spg_sync();
    holly_raise_nrm_int(HOLLY_NRM_INT_VBLANK_IN);
    sched_next_vblank_in_event();

    printf("vcount is %u\n", get_vcount());
    framebuffer_render();
}

static void spg_handle_vblank_out(SchedEvent *event) {
    spg_sync();
    holly_raise_nrm_int(HOLLY_NRM_INT_VBLANK_OUT);
    sched_next_vblank_out_event();
}

/*
 * Make sure you call spg_sync before calling this function
 * also make sure the event isn't already scheduled
 */
static void sched_next_hblank_event() {
    unsigned hblank_int_mode = get_hblank_int_mode();
    unsigned hcount = get_hcount();
    unsigned vcount = get_vcount();
    unsigned hblank_int_comp_val = get_hblank_int_comp_val();
    unsigned next_hblank_line;
    unsigned next_hblank_pclk;

    switch (hblank_int_mode) {
    case 0:
        if (hblank_int_comp_val <= raster_y) {
            next_hblank_pclk =
                (vcount - raster_y + hblank_int_comp_val) * hcount - raster_x;
        } else {
            next_hblank_pclk =
                (hblank_int_comp_val - raster_y) * hcount - raster_x;
        }
        break;
    case 1:
        // round up to nearest multiple of hblank_int_comp_val
        next_hblank_line = (1 + ((raster_y + 1) / hblank_int_comp_val)) *
            hblank_int_comp_val - 1;
        if (next_hblank_line < vcount) {
            next_hblank_pclk =
                (next_hblank_line - raster_y) * hcount - raster_x;
        } else {
            next_hblank_pclk =
                (vcount - raster_y + next_hblank_line) * hcount - raster_x;
        }
        break;
    case 2:
        next_hblank_pclk = hcount - raster_x;
        break;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    // I'm leaving this commented-out here it came in handy for debugging once

    /* printf("current raster_x is %u; there are %u pixels until the next " */
    /*        "hblank\n", (unsigned)raster_x, (unsigned)next_hblank_pclk); */
    /* unsigned raster_y_next = */
    /*     ((raster_x + next_hblank_pclk) / hcount + raster_y) % vcount; */
    /* unsigned raster_x_next = (raster_x + next_hblank_pclk) % hcount; */
    /* printf("when the time comes, the raster pos will be (%u, %u)\n", */
    /*        raster_x_next, raster_y_next); */

    hblank_event.when = (SPG_VCLK_DIV * pclk_div) *
        (next_hblank_pclk + dc_cycle_stamp() / (SPG_VCLK_DIV * pclk_div));

    sched_event(&hblank_event);
    hblank_event_scheduled = true;
}

/*
 * Make sure you call spg_sync before calling this function
 * also make sure the event isn't already scheduled
 */
static void sched_next_vblank_in_event() {
    unsigned hcount = get_hcount();
    unsigned vcount = get_vcount();
    unsigned line = get_vblank_in_int_line();

    unsigned lines_until_vblank_in;
    if (raster_y < line)
        lines_until_vblank_in = line - raster_y;
    else
        lines_until_vblank_in = vcount - raster_y + line;

    unsigned pixels_until_vblank_in =
        lines_until_vblank_in * hcount - raster_x;
    vblank_in_event.when = (SPG_VCLK_DIV * pclk_div) *
        (pixels_until_vblank_in + dc_cycle_stamp() / (SPG_VCLK_DIV * pclk_div));

    assert(vblank_in_event.when - dc_cycle_stamp() < (200 * 1000 * 1000));

    sched_event(&vblank_in_event);
    vblank_in_event_scheduled = true;
}

/*
 * Make sure you call spg_sync before calling this function
 * also make sure the event isn't already scheduled
 */
static void sched_next_vblank_out_event() {
    unsigned hcount = get_hcount();
    unsigned vcount = get_vcount();
    unsigned line = get_vblank_out_int_line();

    unsigned lines_until_vblank_out;
    if (raster_y < line)
        lines_until_vblank_out = line - raster_y;
    else
        lines_until_vblank_out = vcount - raster_y + line;

    unsigned pixels_until_vblank_out =
        lines_until_vblank_out * hcount - raster_x;
    vblank_out_event.when = (SPG_VCLK_DIV * pclk_div) *
        (pixels_until_vblank_out + dc_cycle_stamp() / (SPG_VCLK_DIV * pclk_div));

    assert(vblank_out_event.when - dc_cycle_stamp() < (200 * 1000 * 1000));

    sched_event(&vblank_out_event);
    vblank_out_event_scheduled = true;
}

void spg_set_pclk_div(unsigned val) {
    if (val != 1 && val != 2)
        RAISE_ERROR(ERROR_INVALID_PARAM);

    pclk_div = val;
}

void spg_set_pix_double_x(bool val) {
    pix_double_x = val;
}

void spg_set_pix_double_y(bool val) {
    pix_double_y = val;
}

uint32_t get_spg_control() {
    return spg_reg[SPG_CONTROL];
}

static inline unsigned get_hblank_int_pix() {
    return (spg_reg[SPG_HBLANK_INT] >> 16) & 0x3ff;
}

static inline unsigned get_hcount() {
    return (spg_reg[SPG_LOAD] & 0x3ff) + 1;
}

static inline unsigned get_vcount() {
    // TODO: multiply by 2 ?
    return ((spg_reg[SPG_LOAD] >> 16) & 0x3ff) + 1;
}

static inline unsigned get_hblank_int_mode() {
    return (spg_reg[SPG_HBLANK_INT] >> 12) & 0x3;
}

static inline unsigned get_hblank_int_comp_val() {
    return spg_reg[SPG_HBLANK_INT] & 0x3ff;
}

static inline unsigned get_vblank_in_int_line() {
    return spg_reg[SPG_VBLANK_INT] & 0x3ff;
}

static inline unsigned get_vblank_out_int_line() {
    return (spg_reg[SPG_VBLANK_INT] >> 16) & 0x3ff;
}

static inline unsigned get_hbstart(void) {
    return spg_reg[SPG_HBLANK] & 0x3ff;
}

static inline unsigned get_hbend(void) {
    return (spg_reg[SPG_HBLANK] >> 16) & 0x3ff;
}

static inline unsigned get_vbstart(void) {
    return spg_reg[SPG_VBLANK] & 0x3ff;
}

static inline unsigned get_vbend(void) {
    return (spg_reg[SPG_VBLANK] >> 16) & 0x3ff;
}

int
read_spg_hblank_int(struct pvr2_core_mem_mapped_reg const *reg_info,
                    void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, spg_reg + SPG_HBLANK_INT, len);
    return 0;
}

int
write_spg_hblank_int(struct pvr2_core_mem_mapped_reg const *reg_info,
                     void const *buf, addr32_t addr, unsigned len) {
    spg_sync();
    spg_unsched_all();

    memcpy(spg_reg + SPG_HBLANK_INT, buf, sizeof(reg32_t));

    spg_sync();

    sched_next_hblank_event();
    sched_next_vblank_in_event();
    sched_next_vblank_out_event();

    return 0;
}

int
read_spg_vblank_int(struct pvr2_core_mem_mapped_reg const *reg_info,
                    void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, spg_reg + SPG_VBLANK_INT, len);
    return 0;
}

int
write_spg_vblank_int(struct pvr2_core_mem_mapped_reg const *reg_info,
                     void const *buf, addr32_t addr, unsigned len) {
    spg_sync();
    spg_unsched_all();

    memcpy(spg_reg + SPG_VBLANK_INT, buf, sizeof(reg32_t));

    spg_sync();

    sched_next_hblank_event();
    sched_next_vblank_in_event();
    sched_next_vblank_out_event();

    return 0;
}

int
read_spg_load(struct pvr2_core_mem_mapped_reg const *reg_info,
              void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, spg_reg + SPG_LOAD, len);
    return 0;
}

int
write_spg_load(struct pvr2_core_mem_mapped_reg const *reg_info,
               void const *buf, addr32_t addr, unsigned len) {
    spg_sync();
    spg_unsched_all();

    memcpy(spg_reg + SPG_LOAD, buf, sizeof(reg32_t));

    spg_sync();

    sched_next_hblank_event();
    sched_next_vblank_in_event();
    sched_next_vblank_out_event();

    return 0;
}

int
read_spg_control(struct pvr2_core_mem_mapped_reg const *reg_info,
                 void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, spg_reg + SPG_CONTROL, len);
    return 0;
}

int
write_spg_control(struct pvr2_core_mem_mapped_reg const *reg_info,
                  void const *buf, addr32_t addr, unsigned len) {
    memcpy(spg_reg + SPG_CONTROL, buf, len);
    return 0;
}

int
read_spg_status(struct pvr2_core_mem_mapped_reg const *reg_info,
                void *buf, addr32_t addr, unsigned len) {
    uint32_t spg_stat;

    spg_sync();

    spg_stat = 0x3ff & raster_y;

    /*
     * TODO: set the fieldnum bit (bit 10).  this is related to which group of
     * scanlines are currently being updated when interlacing is enabled, IIRC
     */

    // TODO: set the blank bit (bit 11).  I don't know what this is for yet

    if (raster_y < get_vbend() || raster_y >= get_vbstart())
        spg_stat |= (1 << 13);

    if (raster_x < get_hbend() || raster_x >= get_hbstart())
        spg_stat |= (1 << 12);

    memcpy(buf, &spg_stat, len);

    return 0;
}

int
read_spg_hblank(struct pvr2_core_mem_mapped_reg const *reg_info,
                void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, spg_reg + SPG_HBLANK, len);
    return 0;
}

int
write_spg_hblank(struct pvr2_core_mem_mapped_reg const *reg_info,
                 void const *buf, addr32_t addr, unsigned len) {
    memcpy(spg_reg + SPG_HBLANK, buf, len);
    return 0;
}

int
read_spg_vblank(struct pvr2_core_mem_mapped_reg const *reg_info,
                void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, spg_reg + SPG_VBLANK, len);
    return 0;
}

int
write_spg_vblank(struct pvr2_core_mem_mapped_reg const *reg_info,
                 void const *buf, addr32_t addr, unsigned len) {
    memcpy(spg_reg + SPG_VBLANK, buf, len);
    return 0;
}
