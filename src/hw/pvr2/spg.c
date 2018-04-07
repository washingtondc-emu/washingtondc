/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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

#include "error.h"
#include "dreamcast.h"
#include "hw/sh4/sh4.h"
#include "hw/sys/holly_intc.h"
#include "dreamcast.h"
#include "log.h"

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
 * SPG vclk frequency is 27MHz, with an optional divide to turn it into 13.5 MHz
 *
 * My way of implementing interlace-scan is to double the vclk.  I don't know if
 * this is how it works on a real Dreamcast, but I have confirmed that the
 * vcount does not skip over every other line when interlace scan is enabled,
 * so this is one possible way that might be implemented.  I suppose the other
 * possibility is that maybe the SPG triggers a vblank at vcount / 2 and again
 * at vcount, but I just don't know.  That wouldn't get me a consistent, perfect
 * 59.97005997 Hz in situations where vcount is odd (and it does seem like it's
 * always odd based on my experiences) but doubling the clock speed does get me
 * a perfect unwavering 59.97005997 Hz clock so that's the implementation I've
 * chosen to go with.
 *
 * So, in general my vclk implementation is 54MHz.  Guest-programs may
 * optionally divide this clock by two to get a 27MHz clock (generally speaking,
 * they'll divide if the video cable is composite NTSC, and they won't divide
 * if the video cable is a VGA). if progressive-scan is enabled, then we divide
 * by two again.
 */
#define SPG_VCLK_DIV (SCHED_FREQUENCY / (54 * 1000 * 1000))

static_assert(SCHED_FREQUENCY % (54 * 1000 * 1000) == 0,
              "scheduler frequency does not cleanly divide by SPG frequency");

/*
 * this should be either 1 (for 27 MHz pixel clock) or
 * 2 (for 13.5 MHz pixel clock).
 *
 * It corresponds to bit 23 of FB_R_CTRL (pclk_div).
 *
 * Only access this through the get_pclk_div method so that you get the proper
 * value including the interlace/progressive divide.
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

static inline bool get_interlace(void);
static inline unsigned get_pclk_div(void);

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
    dc_cycle_stamp_t last_sync_rounded = (get_pclk_div() * SPG_VCLK_DIV) *
        (last_sync / (get_pclk_div() * SPG_VCLK_DIV));

    dc_cycle_stamp_t delta_cycles = cur_time - last_sync_rounded;

    // only update the last_sync timestamp if the values have changed
    unsigned raster_x_inc = delta_cycles / (get_pclk_div() * SPG_VCLK_DIV);
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

    LOG_DBG("vcount is %u\n", get_vcount());
    dc_end_frame();
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

    hblank_event.when = (SPG_VCLK_DIV * get_pclk_div()) *
        (next_hblank_pclk + dc_cycle_stamp() / (SPG_VCLK_DIV * get_pclk_div()));

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
    vblank_in_event.when = (SPG_VCLK_DIV * get_pclk_div()) *
        (pixels_until_vblank_in + dc_cycle_stamp() /
         (SPG_VCLK_DIV * get_pclk_div()));

#ifdef INVARIANTS
    if (vblank_in_event.when - dc_cycle_stamp() >= SCHED_FREQUENCY)
        RAISE_ERROR(ERROR_INTEGRITY);
#endif

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
    vblank_out_event.when = (SPG_VCLK_DIV * get_pclk_div()) *
        (pixels_until_vblank_out + dc_cycle_stamp() /
         (SPG_VCLK_DIV * get_pclk_div()));

#ifdef INVARIANTS
    if (vblank_out_event.when - dc_cycle_stamp() >= SCHED_FREQUENCY)
        RAISE_ERROR(ERROR_INTEGRITY);
#endif

    sched_event(&vblank_out_event);
    vblank_out_event_scheduled = true;
}

void spg_set_pclk_div(unsigned val) {
    if (val != 1 && val != 2)
        RAISE_ERROR(ERROR_INVALID_PARAM);

    spg_sync();
    spg_unsched_all();

    pclk_div = val;

    spg_sync();

    sched_next_hblank_event();
    sched_next_vblank_in_event();
    sched_next_vblank_out_event();
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

uint32_t spg_hblank_int_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return spg_reg[SPG_HBLANK_INT];
}

void spg_hblank_int_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                               uint32_t val) {
    spg_sync();
    spg_unsched_all();

    spg_reg[SPG_HBLANK_INT] = val;

    spg_sync();

    sched_next_hblank_event();
    sched_next_vblank_in_event();
    sched_next_vblank_out_event();
}

uint32_t spg_vblank_int_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return spg_reg[SPG_VBLANK_INT];
}

void spg_vblank_int_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                               uint32_t val) {
    spg_sync();
    spg_unsched_all();

    spg_reg[SPG_VBLANK_INT] = val;

    spg_sync();

    sched_next_hblank_event();
    sched_next_vblank_in_event();
    sched_next_vblank_out_event();
}

uint32_t spg_load_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return spg_reg[SPG_LOAD];
}

void spg_load_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                         uint32_t val) {
    spg_sync();
    spg_unsched_all();

    spg_reg[SPG_LOAD] = val;

    spg_sync();

    sched_next_hblank_event();
    sched_next_vblank_in_event();
    sched_next_vblank_out_event();
}

uint32_t spg_control_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return spg_reg[SPG_CONTROL];
}

void spg_control_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                            uint32_t val) {
    spg_reg[SPG_CONTROL] = val;
}

uint32_t spg_status_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
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

    return spg_stat;
}

uint32_t spg_hblank_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return spg_reg[SPG_HBLANK];
}

void spg_hblank_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                           uint32_t val) {
    // TODO: should I do spg_sync here?
    spg_reg[SPG_HBLANK] = val;
    // TODO: should I do spg_sync + unsched_all + resched here?
}

uint32_t spg_vblank_mmio_read(struct mmio_region_pvr2_core_reg *region, unsigned idx) {
    return spg_reg[SPG_VBLANK];
}

void spg_vblank_mmio_write(struct mmio_region_pvr2_core_reg *region, unsigned idx,
                           uint32_t val) {
    // TODO: should I do spg_sync here?
    spg_reg[SPG_VBLANK] = val;
    // TODO: should I do spg_sync + unsched_all + resched here?
}

static inline bool get_interlace(void) {
    return (bool)(spg_reg[SPG_CONTROL] & (1 << 4));
}

static inline unsigned get_pclk_div(void) {
    return get_interlace() ? pclk_div : pclk_div * 2;
}
