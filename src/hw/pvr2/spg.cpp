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

#include <boost/cstdint.hpp>

#include "BaseException.hpp"
#include "Dreamcast.hpp"
#include "hw/sh4/sh4.hpp"
#include "hw/sys/holly_intc.hpp"

#include "spg.hpp"

/* This is the code which generates the H-BLANK and V-BLANK interrupts */

typedef uint64_t spg_vclk_cycle_t;

static const unsigned SPG_VCLK_DIV = 7;

/*
 * this should be either 1 (for 27 MHz pixel clock) or
 * 2 (for 13.5 MHz pixel clock).
 *
 * It corresponds to bit 23 of FB_R_CTRL (pclk_div)
 */
static unsigned pclk_div = 2;

/*
 * This increments once per tick (should be 27 MHz).
 *
 * when it's equal to pclk_div, the pixel clock ticks.
 */
static unsigned vclk;

static spg_vclk_cycle_t last_sync;

// whether to double pixels horizontally/vertically
static bool pix_double_x, pix_double_y;

enum {
    SPG_HBLANK_INT,
    SPG_VBLANK_INT,
    SPG_LOAD,

    SPG_REG_COUNT
};

// TODO: put in an initialization function
static reg32_t spg_reg[SPG_REG_COUNT] = {
    0x31d << 16,          // SPG_HBLANK_INT
    0x00150104,           // SPG_VBLANK_INT
    (0x106 << 16) | 0x359 // SPG_LOAD
};

static unsigned raster_x, raster_y;

static SchedEvent spg_tick_event;

// pixel clock, called every pclk_div or pclk_div / 2 cycles of vclck
static void spg_pclk_tick();

static void spg_tick(SchedEvent *event);

static void spg_sync();

static inline unsigned get_hcount();
static inline unsigned get_vcount();
static inline unsigned get_hblank_int_pix();
static inline unsigned get_hblank_int_mode();
static inline unsigned get_hblank_int_comp_val();
static inline unsigned get_vblank_in_int_line();
static inline unsigned get_vblank_out_int_line();

void spg_init() {
    spg_tick_event.when = dc_cycle_stamp() + SPG_VCLK_DIV;
    spg_tick_event.handler = spg_tick;

    sched_event(&spg_tick_event);
}

void spg_cleanup() {
}

static void spg_sync() {
    unsigned hcount = get_hcount();
    unsigned vcount = get_vcount();
    spg_vclk_cycle_t cur_time = dc_cycle_stamp() / SPG_VCLK_DIV;
    spg_vclk_cycle_t delta_cycles = cur_time - last_sync;
    last_sync = cur_time;

    raster_x += delta_cycles / pclk_div;
    raster_y += raster_x / hcount;
    raster_x %= hcount;
    raster_y %= vcount;
}

static void spg_tick(SchedEvent *event) {
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
    spg_tick_event.when = dc_cycle_stamp() + SPG_VCLK_DIV;
    sched_event(&spg_tick_event);

    vclk++;
    if (vclk < pclk_div)
        return;

    vclk = 0;
    spg_pclk_tick();
}

static void spg_pclk_tick() {
    // TODO: take interlacing into account
    unsigned hblank_int_pix_no = get_hblank_int_pix();
    unsigned hblank_int_mode = get_hblank_int_mode();
    unsigned hblank_int_comp_val = get_hblank_int_comp_val();
    unsigned vblank_in_int_line_no = get_vblank_in_int_line();
    unsigned vblank_out_int_line_no = get_vblank_out_int_line();

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

    spg_sync();

    if (raster_x == hblank_int_pix_no) {
        switch (hblank_int_mode) {
        case 0:
            if (raster_y == hblank_int_comp_val) {
                holly_raise_nrm_int(HOLLY_NRM_INT_HBLANK);
            }
            break;
        case 1:
            if (hblank_int_comp_val && (raster_y % hblank_int_comp_val == 0)) {
                holly_raise_nrm_int(HOLLY_NRM_INT_HBLANK);
            }
            break;
        case 2:
            holly_raise_nrm_int(HOLLY_NRM_INT_HBLANK);
            break;
        default:
            BOOST_THROW_EXCEPTION(UnimplementedError());
        }
    }

    if (raster_x == 0) {
        if (raster_y == vblank_in_int_line_no) {
            holly_raise_nrm_int(HOLLY_NRM_INT_VBLANK_IN);
        }

        if (raster_y == vblank_out_int_line_no) {
            holly_raise_nrm_int(HOLLY_NRM_INT_VBLANK_OUT);
        }
    }
}

void spg_set_pclk_div(unsigned val) {
    if (val != 1 && val != 2)
        BOOST_THROW_EXCEPTION(InvalidParamError());

    pclk_div = val;
}

void spg_set_pix_double_x(bool val) {
    pix_double_x = val;
}

void spg_set_pix_double_y(bool val) {
    pix_double_y = val;
}

static inline unsigned get_hblank_int_pix() {
    return (spg_reg[SPG_HBLANK_INT] >> 16) & 0x3ff;
}

static inline unsigned get_hcount() {
    return (spg_reg[SPG_LOAD] & 0x3ff) + 1;
}

static inline unsigned get_vcount() {
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
    memcpy(spg_reg + SPG_HBLANK_INT, buf, sizeof(reg32_t));
    spg_sync();
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
    memcpy(spg_reg + SPG_VBLANK_INT, buf, sizeof(reg32_t));
    spg_sync();
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
    memcpy(spg_reg + SPG_LOAD, buf, sizeof(reg32_t));
    spg_sync();
    return 0;
}
