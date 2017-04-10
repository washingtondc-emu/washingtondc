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
#include "video/opengl/framebuffer.hpp"
#include "window.hpp"
#include "hw/sh4/sh4.hpp"
#include "hw/sys/holly_intc.hpp"

#include "spg.hpp"

/* This is the code which generates the H-BLANK and V-BLANK interrupts */

typedef boost::error_info<struct tag_raster_x_expect_error_info,
                          unsigned> errinfo_raster_x_expect;
typedef boost::error_info<struct tag_raster_x_actual_error_info,
                          unsigned> errinfo_raster_x_actual;
typedef boost::error_info<struct tag_raster_y_expect_error_info,
                          unsigned> errinfo_raster_y_expect;
typedef boost::error_info<struct tag_raster_y_actual_error_info,
                          unsigned> errinfo_raster_y_actual;
typedef boost::error_info<struct tag_hblank_int_comp_val_error_info,
                          unsigned> errinfo_hblank_int_comp_val;
typedef boost::error_info<struct tag_hblank_int_mode_error_info,
                          unsigned> errinfo_hblank_int_mode;

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

typedef uint64_t spg_vclk_cycle_t;

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
static const unsigned SPG_VCLK_DIV = 7;

/*
 * this should be either 1 (for 27 MHz pixel clock) or
 * 2 (for 13.5 MHz pixel clock).
 *
 * It corresponds to bit 23 of FB_R_CTRL (pclk_div)
 */
static unsigned pclk_div = 2;

static spg_vclk_cycle_t last_sync;

// whether to double pixels horizontally/vertically
static bool pix_double_x, pix_double_y;

enum {
    SPG_HBLANK_INT,
    SPG_VBLANK_INT,
    SPG_LOAD,
    SPG_CONTROL,

    SPG_REG_COUNT
};

// TODO: put in an initialization function
static reg32_t spg_reg[SPG_REG_COUNT] = {
    0x31d << 16,          // SPG_HBLANK_INT
    0x00150104,           // SPG_VBLANK_INT
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

static void sched_next_hblank_event();
static void sched_next_vblank_in_event();
static void sched_next_vblank_out_event();

static void spg_handle_hblank(SchedEvent *event);
static void spg_handle_vblank_in(SchedEvent *event);
static void spg_handle_vblank_out(SchedEvent *event);

static void spg_unsched_all();

static inline spg_vclk_cycle_t spg_cycle_stamp();

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
    spg_vclk_cycle_t cur_time = spg_cycle_stamp();
    spg_vclk_cycle_t delta_cycles = cur_time - last_sync;
    last_sync = cur_time;

    raster_x += delta_cycles / pclk_div;
    raster_y += raster_x / hcount;
    raster_x %= hcount;
    raster_y %= vcount;
}

static void spg_handle_hblank(SchedEvent *event) {
    spg_sync();

#ifdef INVARIANTS
    unsigned hblank_int_mode = get_hblank_int_mode();
    unsigned hblank_int_comp_val = get_hblank_int_comp_val();

    if (raster_x != 0) {
        BOOST_THROW_EXCEPTION(IntegrityError() <<
                              errinfo_raster_x_expect(0) <<
                              errinfo_raster_x_actual(raster_x) <<
                              errinfo_raster_y_actual(raster_y) <<
                              errinfo_hblank_int_comp_val(hblank_int_comp_val) <<
                              errinfo_hblank_int_mode(hblank_int_mode));
    }

    switch (hblank_int_mode) {
    case 0:
        if (raster_y == hblank_int_comp_val) {
            BOOST_THROW_EXCEPTION(IntegrityError() <<
                                  errinfo_raster_y_expect(hblank_int_comp_val) <<
                                  errinfo_raster_y_actual(raster_y) <<
                                  errinfo_hblank_int_comp_val(hblank_int_comp_val) <<
                              errinfo_hblank_int_mode(hblank_int_mode));
        }
        break;
    case 1:
        if (hblank_int_comp_val && (raster_y % hblank_int_comp_val == 0)) {
            BOOST_THROW_EXCEPTION(IntegrityError() <<
                                  errinfo_raster_y_actual(raster_y) <<
                                  errinfo_hblank_int_comp_val(hblank_int_comp_val) <<
                              errinfo_hblank_int_mode(hblank_int_mode));
        }
        break;
    case 2:
        break;
    default:
        BOOST_THROW_EXCEPTION(UnimplementedError());
    }
#endif

    holly_raise_nrm_int(HOLLY_NRM_INT_HBLANK);

    sched_next_hblank_event();
}

static void spg_handle_vblank_in(SchedEvent *event) {
    spg_sync();
    holly_raise_nrm_int(HOLLY_NRM_INT_VBLANK_IN);
    sched_next_vblank_in_event();

    framebuffer_render();
    win_update();
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
    spg_vclk_cycle_t next_hblank_pclk;

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
        BOOST_THROW_EXCEPTION(UnimplementedError());
    }

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
    if (raster_y <= line)
        lines_until_vblank_in = vcount - raster_y + line;
    else
        lines_until_vblank_in = line - raster_y;

    unsigned pixels_until_vblank_in =
        lines_until_vblank_in * hcount - raster_x;
    vblank_in_event.when = (SPG_VCLK_DIV * pclk_div) *
        (pixels_until_vblank_in + dc_cycle_stamp() / (SPG_VCLK_DIV * pclk_div));
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
    sched_event(&vblank_out_event);
    vblank_out_event_scheduled = true;
}

static inline spg_vclk_cycle_t spg_cycle_stamp() {
    return dc_cycle_stamp() / SPG_VCLK_DIV;
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
