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

#include "BaseException.hpp"
#include "hw/sh4/sh4.hpp"
#include "hw/sys/holly_intc.hpp"

#include "spg.hpp"

/* This is the code which generates the H-BLANK and V-BLANK interrupts */

/*
 * this should be either 1 (for 27 MHz pixel clock) or
 * 2 (for 13.5 MHz pixel clock).
 *
 * It corresponds to bit 23 of FB_R_CTRL (vclk_div)
 */
__attribute__((unused)) static unsigned vclk_div = 2;

/*
 * This increments once per tick (should be 27 MHz).
 *
 * when it's equal to vclk_div, the pixel clock ticks.
 */
__attribute__((unused)) static unsigned vclk;

// whether to double pixels horizontally/vertically
static bool pix_double_x, pix_double_y;

enum {
    SPG_HBLANK_INT,
    SPG_VBLANK_INT,
    SPG_CONTROL,
    SPG_HBLANK,
    SPG_VBLANK,
    SPG_VO_STARTX,
    SPG_VO_STARTY,
    SPG_LOAD,

    SPG_REG_COUNT
};

// TODO: put in an initialization function
static reg32_t spg_reg[SPG_REG_COUNT] = {
    0x31d << 16, // SPG_HBLANK_INT
    0x00150104   // SPG_VBLANK_INT
};

static unsigned raster_x, raster_y;

// pixel clock, called every vclk_div or vclk_div / 2 cycles of vclck
static void spg_pclk_tick();

void spg_tick() {
    vclk++;
    if (vclk < vclk_div)
        return;

    vclk = 0;
    spg_pclk_tick();
}

static void spg_pclk_tick() {
    // TODO: take interlacing into account
    unsigned hcount = (spg_reg[SPG_LOAD] & 0x3ff) + 1;
    unsigned vcount = ((spg_reg[SPG_LOAD] >> 16) & 0x3ff) + 1;
    unsigned hblank_int_pix_no = (spg_reg[SPG_HBLANK_INT] >> 16) & 0x3ff;
    unsigned hblank_int_mode = (spg_reg[SPG_HBLANK_INT] >> 12) & 0x3;
    unsigned hblank_int_comp_val = spg_reg[SPG_HBLANK_INT] & 0x3ff;
    unsigned vblank_in_int_line_no = spg_reg[SPG_VBLANK_INT] & 0x3ff;
    unsigned vblank_out_int_line_no = (spg_reg[SPG_VBLANK_INT] >> 16) & 0x3ff;

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

    if (raster_x == hblank_int_pix_no) {
        switch (hblank_int_mode) {
        case 0:
            if (raster_y == hblank_int_comp_val) {
                holly_raise_nrm_int(HOLLY_NRM_INT_HBLANK);
            }
            break;
        case 1:
            // TODO: check for divide by zero here
            if (raster_y % hblank_int_comp_val == 0) {
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

    if (raster_x >= hcount) {
        raster_x = 0;

        if (raster_y == vblank_in_int_line_no) {
            holly_raise_nrm_int(HOLLY_NRM_INT_VBLANK_IN);
        }

        if (raster_y == vblank_out_int_line_no) {
            holly_raise_nrm_int(HOLLY_NRM_INT_VBLANK_OUT);
        }

        if (raster_y >= vcount)
            raster_y = 0;
        else
            raster_y++;
    } else {
        raster_x++;
    }
}

void spg_set_vclk_div(unsigned val) {
    if (val != 1 && val != 2)
        BOOST_THROW_EXCEPTION(InvalidParamError());

    vclk_div = val;
}

void spg_set_pix_double_x(bool val) {
    pix_double_x = val;
}

void spg_set_pix_double_y(bool val) {
    pix_double_y = val;
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
    memcpy(spg_reg + SPG_HBLANK_INT, buf, sizeof(reg32_t));
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
    memcpy(spg_reg + SPG_VBLANK_INT, buf, sizeof(reg32_t));
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
    memcpy(spg_reg + SPG_CONTROL, buf, sizeof(reg32_t));
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
    memcpy(spg_reg + SPG_HBLANK, buf, sizeof(reg32_t));
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
    memcpy(spg_reg + SPG_VBLANK, buf, sizeof(reg32_t));
    return 0;
}

int
read_spg_vo_startx(struct pvr2_core_mem_mapped_reg const *reg_info,
                   void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, spg_reg + SPG_VO_STARTX, len);
    return 0;
}

int
write_spg_vo_startx(struct pvr2_core_mem_mapped_reg const *reg_info,
                    void const *buf, addr32_t addr, unsigned len) {
    memcpy(spg_reg + SPG_VO_STARTX, buf, sizeof(reg32_t));
    return 0;
}

int
read_spg_vo_starty(struct pvr2_core_mem_mapped_reg const *reg_info,
                   void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, spg_reg + SPG_VO_STARTY, len);
    return 0;
}

int
write_spg_vo_starty(struct pvr2_core_mem_mapped_reg const *reg_info,
                    void const *buf, addr32_t addr, unsigned len) {
    memcpy(spg_reg + SPG_VO_STARTY, buf, sizeof(reg32_t));
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
    memcpy(spg_reg + SPG_LOAD, buf, sizeof(reg32_t));
    return 0;
}
