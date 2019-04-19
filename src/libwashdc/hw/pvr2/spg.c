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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "washdc/error.h"
#include "dreamcast.h"
#include "hw/sh4/sh4.h"
#include "hw/sys/holly_intc.h"
#include "dreamcast.h"
#include "log.h"
#include "pvr2.h"

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

static void spg_sync(struct pvr2 *pvr2);

static inline unsigned get_hcount(struct pvr2 *pvr2);
static inline unsigned get_vcount(struct pvr2 *pvr2);
__attribute__((unused))
static inline unsigned get_hblank_int_pix(struct pvr2 *pvr2);
static inline unsigned get_hblank_int_mode(struct pvr2 *pvr2);
static inline unsigned get_hblank_int_comp_val(struct pvr2 *pvr2);
static inline unsigned get_vblank_in_int_line(struct pvr2 *pvr2);
static inline unsigned get_vblank_out_int_line(struct pvr2 *pvr2);
static inline unsigned get_hbstart(struct pvr2 *pvr2);
static inline unsigned get_hbend(struct pvr2 *pvr2);
static inline unsigned get_vbstart(struct pvr2 *pvr2);
static inline unsigned get_vbend(struct pvr2 *pvr2);

static void sched_next_hblank_event(struct pvr2 *pvr2);
static void sched_next_vblank_in_event(struct pvr2 *pvr2);
static void sched_next_vblank_out_event(struct pvr2 *pvr2);

static void spg_handle_hblank(SchedEvent *event);
static void spg_handle_vblank_in(SchedEvent *event);
static void spg_handle_vblank_out(SchedEvent *event);

static void spg_unsched_all(struct pvr2 *pvr2);

static inline bool get_interlace(struct pvr2 *pvr2);
static inline unsigned get_pclk_div(struct pvr2 *pvr2);

void spg_init(struct pvr2 *pvr2) {
    struct pvr2_spg *spg = &pvr2->spg;

    spg->pclk_div = 2;

    spg->reg[SPG_HBLANK_INT] = 0x31d << 16;
    spg->reg[SPG_VBLANK_INT] = 0x00150104;
    spg->reg[SPG_HBLANK] = 0x007e0345;
    spg->reg[SPG_VBLANK] = 0x00150104;
    spg->reg[SPG_LOAD] = (0x106 << 16) | 0x359;

    spg->hblank_event.handler = spg_handle_hblank;
    spg->vblank_in_event.handler = spg_handle_vblank_in;
    spg->vblank_out_event.handler = spg_handle_vblank_out;

    spg->hblank_event.arg_ptr = pvr2;
    spg->vblank_in_event.arg_ptr = pvr2;
    spg->vblank_out_event.arg_ptr = pvr2;

    sched_next_hblank_event(pvr2);
    sched_next_vblank_in_event(pvr2);
    sched_next_vblank_out_event(pvr2);
}

void spg_cleanup(struct pvr2 *pvr2) {
}

static void spg_unsched_all(struct pvr2 *pvr2) {
    struct pvr2_spg *spg = &pvr2->spg;

    if (spg->hblank_event_scheduled) {
        cancel_event(pvr2->clk, &spg->hblank_event);
        spg->hblank_event_scheduled = false;
    }

    if (spg->vblank_in_event_scheduled) {
        cancel_event(pvr2->clk, &spg->vblank_in_event);
        spg->vblank_in_event_scheduled = false;
    }

    if (spg->vblank_out_event_scheduled) {
        cancel_event(pvr2->clk, &spg->vblank_out_event);
        spg->vblank_out_event_scheduled = false;
    }
}

static void spg_sync(struct pvr2 *pvr2) {
    struct pvr2_spg *spg = &pvr2->spg;
    dc_cycle_stamp_t cur_time = clock_cycle_stamp(pvr2->clk);
    dc_cycle_stamp_t delta_cycles = cur_time - spg->last_sync_rounded;

    // only update the last_sync timestamp if the values have changed
    unsigned div = get_pclk_div(pvr2) * SPG_VCLK_DIV;
    if (delta_cycles >= div) {
        unsigned hcount = get_hcount(pvr2);
        unsigned vcount = get_vcount(pvr2);
        unsigned raster_x_inc = delta_cycles / div;
        spg->last_sync_rounded = div * (cur_time / div);
        spg->raster_x += raster_x_inc;
        spg->raster_y += spg->raster_x / hcount;
        spg->raster_x %= hcount;
        spg->raster_y %= vcount;
    }
}

static void spg_handle_hblank(SchedEvent *event) {
    struct pvr2 *pvr2 = (struct pvr2*)event->arg_ptr;

    spg_sync(pvr2);

#ifdef INVARIANTS
    unsigned hblank_int_mode = get_hblank_int_mode(pvr2);
    unsigned hblank_int_comp_val = get_hblank_int_comp_val(pvr2);
    struct pvr2_spg *spg = &pvr2->spg;

    if (spg->raster_x != 0) {
        error_set_raster_x_expect(0);
        error_set_raster_x_actual(spg->raster_x);
        error_set_raster_y_actual(spg->raster_y);
        error_set_hblank_int_comp_val(hblank_int_comp_val);
        error_set_hblank_int_mode(hblank_int_mode);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    switch (hblank_int_mode) {
    case 0:
        if (spg->raster_y != hblank_int_comp_val) {
            error_set_raster_y_expect(hblank_int_comp_val);
            error_set_raster_y_actual(spg->raster_y);
            error_set_hblank_int_comp_val(hblank_int_comp_val);
            error_set_hblank_int_mode(hblank_int_mode);
            RAISE_ERROR(ERROR_INTEGRITY);
        }
        break;
    case 1:
        if (hblank_int_comp_val && (spg->raster_y % hblank_int_comp_val != 0)) {
            error_set_raster_y_actual(spg->raster_y);
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

    sched_next_hblank_event(pvr2);
}

static void spg_handle_vblank_in(SchedEvent *event) {
    struct pvr2 *pvr2 = (struct pvr2*)event->arg_ptr;

    spg_sync(pvr2);
    holly_raise_nrm_int(HOLLY_NRM_INT_VBLANK_IN);
    sched_next_vblank_in_event(pvr2);

    LOG_DBG("vcount is %u\n", get_vcount(pvr2));
    dc_end_frame();
}

static void spg_handle_vblank_out(SchedEvent *event) {
    struct pvr2 *pvr2 = (struct pvr2*)event->arg_ptr;

    spg_sync(pvr2);
    holly_raise_nrm_int(HOLLY_NRM_INT_VBLANK_OUT);
    sched_next_vblank_out_event(pvr2);
}

/*
 * Make sure you call spg_sync before calling this function
 * also make sure the event isn't already scheduled
 */
static void sched_next_hblank_event(struct pvr2 *pvr2) {
    struct pvr2_spg *spg = &pvr2->spg;
    unsigned hblank_int_mode = get_hblank_int_mode(pvr2);
    unsigned hcount = get_hcount(pvr2);
    unsigned vcount = get_vcount(pvr2);
    unsigned hblank_int_comp_val = get_hblank_int_comp_val(pvr2);
    unsigned next_hblank_line;
    unsigned next_hblank_pclk;

    switch (hblank_int_mode) {
    case 0:
        if (hblank_int_comp_val <= spg->raster_y) {
            next_hblank_pclk =
                (vcount - spg->raster_y + hblank_int_comp_val) * hcount -
                spg->raster_x;
        } else {
            next_hblank_pclk =
                (hblank_int_comp_val - spg->raster_y) * hcount - spg->raster_x;
        }
        break;
    case 1:
        // round up to nearest multiple of hblank_int_comp_val
        next_hblank_line = (1 + ((spg->raster_y + 1) / hblank_int_comp_val)) *
            hblank_int_comp_val - 1;
        if (next_hblank_line < vcount) {
            next_hblank_pclk =
                (next_hblank_line - spg->raster_y) * hcount - spg->raster_x;
        } else {
            next_hblank_pclk =
                (vcount - spg->raster_y + next_hblank_line) * hcount -
                spg->raster_x;
        }
        break;
    case 2:
        next_hblank_pclk = hcount - spg->raster_x;
        break;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    spg->hblank_event.when = (SPG_VCLK_DIV * get_pclk_div(pvr2)) *
        (next_hblank_pclk + clock_cycle_stamp(pvr2->clk) / (SPG_VCLK_DIV * get_pclk_div(pvr2)));

    sched_event(pvr2->clk, &spg->hblank_event);
    spg->hblank_event_scheduled = true;
}

/*
 * Make sure you call spg_sync before calling this function
 * also make sure the event isn't already scheduled
 */
static void sched_next_vblank_in_event(struct pvr2 *pvr2) {
    struct pvr2_spg *spg = &pvr2->spg;
    unsigned hcount = get_hcount(pvr2);
    unsigned vcount = get_vcount(pvr2);
    unsigned line = get_vblank_in_int_line(pvr2);

    unsigned lines_until_vblank_in;
    if (spg->raster_y < line)
        lines_until_vblank_in = line - spg->raster_y;
    else
        lines_until_vblank_in = vcount - spg->raster_y + line;

    unsigned pixels_until_vblank_in =
        lines_until_vblank_in * hcount - spg->raster_x;
    spg->vblank_in_event.when = (SPG_VCLK_DIV * get_pclk_div(pvr2)) *
        (pixels_until_vblank_in + clock_cycle_stamp(pvr2->clk) /
         (SPG_VCLK_DIV * get_pclk_div(pvr2)));

#ifdef INVARIANTS
    if (spg->vblank_in_event.when - clock_cycle_stamp(pvr2->clk) >=
        SCHED_FREQUENCY)
        RAISE_ERROR(ERROR_INTEGRITY);
#endif

    sched_event(pvr2->clk, &spg->vblank_in_event);
    spg->vblank_in_event_scheduled = true;
}

/*
 * Make sure you call spg_sync before calling this function
 * also make sure the event isn't already scheduled
 */
static void sched_next_vblank_out_event(struct pvr2 *pvr2) {
    struct pvr2_spg *spg = &pvr2->spg;
    unsigned hcount = get_hcount(pvr2);
    unsigned vcount = get_vcount(pvr2);
    unsigned line = get_vblank_out_int_line(pvr2);

    unsigned lines_until_vblank_out;
    if (spg->raster_y < line)
        lines_until_vblank_out = line - spg->raster_y;
    else
        lines_until_vblank_out = vcount - spg->raster_y + line;

    unsigned pixels_until_vblank_out =
        lines_until_vblank_out * hcount - spg->raster_x;
    spg->vblank_out_event.when = (SPG_VCLK_DIV * get_pclk_div(pvr2)) *
        (pixels_until_vblank_out + clock_cycle_stamp(pvr2->clk) /
         (SPG_VCLK_DIV * get_pclk_div(pvr2)));

#ifdef INVARIANTS
    if (spg->vblank_out_event.when - clock_cycle_stamp(pvr2->clk) >=
        SCHED_FREQUENCY)
        RAISE_ERROR(ERROR_INTEGRITY);
#endif

    sched_event(pvr2->clk, &spg->vblank_out_event);
    spg->vblank_out_event_scheduled = true;
}

void spg_set_pclk_div(struct pvr2 *pvr2, unsigned val) {
    if (val != 1 && val != 2)
        RAISE_ERROR(ERROR_INVALID_PARAM);

    spg_sync(pvr2);
    spg_unsched_all(pvr2);

    pvr2->spg.pclk_div = val;

    spg_sync(pvr2);

    sched_next_hblank_event(pvr2);
    sched_next_vblank_in_event(pvr2);
    sched_next_vblank_out_event(pvr2);
}

void spg_set_pix_double_x(struct pvr2 *pvr2, bool val) {
    pvr2->spg.pix_double_x = val;
}

void spg_set_pix_double_y(struct pvr2 *pvr2, bool val) {
    pvr2->spg.pix_double_y = val;
}

uint32_t get_spg_control(struct pvr2 *pvr2) {
    return pvr2->spg.reg[SPG_CONTROL];
}

static inline unsigned get_hblank_int_pix(struct pvr2 *pvr2) {
    return (pvr2->spg.reg[SPG_HBLANK_INT] >> 16) & 0x3ff;
}

static inline unsigned get_hcount(struct pvr2 *pvr2) {
    return (pvr2->spg.reg[SPG_LOAD] & 0x3ff) + 1;
}

static inline unsigned get_vcount(struct pvr2 *pvr2) {
    // TODO: multiply by 2 ?
    return ((pvr2->spg.reg[SPG_LOAD] >> 16) & 0x3ff) + 1;
}

static inline unsigned get_hblank_int_mode(struct pvr2 *pvr2) {
    return (pvr2->spg.reg[SPG_HBLANK_INT] >> 12) & 0x3;
}

static inline unsigned get_hblank_int_comp_val(struct pvr2 *pvr2) {
    return pvr2->spg.reg[SPG_HBLANK_INT] & 0x3ff;
}

static inline unsigned get_vblank_in_int_line(struct pvr2 *pvr2) {
    return pvr2->spg.reg[SPG_VBLANK_INT] & 0x3ff;
}

static inline unsigned get_vblank_out_int_line(struct pvr2 *pvr2) {
    return (pvr2->spg.reg[SPG_VBLANK_INT] >> 16) & 0x3ff;
}

static inline unsigned get_hbstart(struct pvr2 *pvr2) {
    return pvr2->spg.reg[SPG_HBLANK] & 0x3ff;
}

static inline unsigned get_hbend(struct pvr2 *pvr2) {
    return (pvr2->spg.reg[SPG_HBLANK] >> 16) & 0x3ff;
}

static inline unsigned get_vbstart(struct pvr2 *pvr2) {
    return pvr2->spg.reg[SPG_VBLANK] & 0x3ff;
}

static inline unsigned get_vbend(struct pvr2 *pvr2) {
    return (pvr2->spg.reg[SPG_VBLANK] >> 16) & 0x3ff;
}

uint32_t pvr2_spg_get_hblank_int(struct pvr2 *pvr2) {
    return pvr2->spg.reg[SPG_HBLANK_INT];
}

void pvr2_spg_set_hblank_int(struct pvr2 *pvr2, uint32_t val) {
    spg_sync(pvr2);
    spg_unsched_all(pvr2);

    pvr2->spg.reg[SPG_HBLANK_INT] = val;

    spg_sync(pvr2);

    sched_next_hblank_event(pvr2);
    sched_next_vblank_in_event(pvr2);
    sched_next_vblank_out_event(pvr2);
}

uint32_t pvr2_spg_get_vblank_int(struct pvr2 *pvr2) {
    return pvr2->spg.reg[SPG_VBLANK_INT];
}

void pvr2_spg_set_vblank_int(struct pvr2 *pvr2, uint32_t val) {
    spg_sync(pvr2);
    spg_unsched_all(pvr2);

    pvr2->spg.reg[SPG_VBLANK_INT] = val;

    spg_sync(pvr2);

    sched_next_hblank_event(pvr2);
    sched_next_vblank_in_event(pvr2);
    sched_next_vblank_out_event(pvr2);
}

uint32_t pvr2_spg_get_load(struct pvr2 *pvr2) {
    return pvr2->spg.reg[SPG_LOAD];
}

void pvr2_spg_set_load(struct pvr2 *pvr2, uint32_t val) {
    spg_sync(pvr2);
    spg_unsched_all(pvr2);

    pvr2->spg.reg[SPG_LOAD] = val;

    spg_sync(pvr2);

    sched_next_hblank_event(pvr2);
    sched_next_vblank_in_event(pvr2);
    sched_next_vblank_out_event(pvr2);
}

uint32_t pvr2_spg_get_control(struct pvr2 *pvr2) {
    return pvr2->spg.reg[SPG_CONTROL];
}

void pvr2_spg_set_control(struct pvr2 *pvr2, uint32_t val) {
    pvr2->spg.reg[SPG_CONTROL] = val;
}

uint32_t pvr2_spg_get_status(struct pvr2 *pvr2) {
    uint32_t spg_stat;
    struct pvr2_spg *spg = &pvr2->spg;

    spg_sync(pvr2);

    spg_stat = 0x3ff & spg->raster_y;

    /*
     * TODO: set the fieldnum bit (bit 10).  this is related to which group of
     * scanlines are currently being updated when interlacing is enabled, IIRC
     */

    // TODO: set the blank bit (bit 11).  I don't know what this is for yet

    if (spg->raster_y < get_vbend(pvr2) || spg->raster_y >= get_vbstart(pvr2))
        spg_stat |= (1 << 13);

    if (spg->raster_x < get_hbend(pvr2) || spg->raster_x >= get_hbstart(pvr2))
        spg_stat |= (1 << 12);

    return spg_stat;
}

uint32_t pvr2_spg_get_hblank(struct pvr2 *pvr2) {
    return pvr2->spg.reg[SPG_HBLANK];
}

void pvr2_spg_set_hblank(struct pvr2 *pvr2, uint32_t val) {
    // TODO: should I do spg_sync here?
    pvr2->spg.reg[SPG_HBLANK] = val;
    // TODO: should I do spg_sync + unsched_all + resched here?
}

uint32_t pvr2_spg_get_vblank(struct pvr2 *pvr2) {
    return pvr2->spg.reg[SPG_VBLANK];
}

void pvr2_spg_set_vblank(struct pvr2 *pvr2, uint32_t val) {
    // TODO: should I do spg_sync here?
    pvr2->spg.reg[SPG_VBLANK] = val;
    // TODO: should I do spg_sync + unsched_all + resched here?
}

static inline bool get_interlace(struct pvr2 *pvr2) {
    return (bool)(pvr2->spg.reg[SPG_CONTROL] & (1 << 4));
}

static inline unsigned get_pclk_div(struct pvr2 *pvr2) {
    return get_interlace(pvr2) ? pvr2->spg.pclk_div : pvr2->spg.pclk_div * 2;
}
