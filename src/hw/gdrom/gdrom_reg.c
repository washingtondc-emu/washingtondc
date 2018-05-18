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

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "mount.h"
#include "error.h"
#include "mem_code.h"
#include "types.h"
#include "MemoryMap.h"
#include "hw/sys/holly_intc.h"
#include "hw/sh4/sh4.h"
#include "hw/sh4/sh4_dmac.h"
#include "cdrom.h"
#include "dreamcast.h"
#include "gdrom_response.h"
#include "gdrom.h"
#include "mmio.h"

#include "gdrom_reg.h"

static reg32_t
gdrom_get_status_reg(struct gdrom_status const *stat_in);
static reg32_t
gdrom_get_error_reg(struct gdrom_error const *error_in);
static reg32_t
gdrom_get_int_reason_reg(struct gdrom_int_reason *int_reason_in);

static void
gdrom_set_features_reg(struct gdrom_features *features_out, reg32_t feat_reg);
static void
gdrom_set_sect_cnt_reg(struct gdrom_sector_count *sect_cnt_out,
                       reg32_t sect_cnt_reg);
static void
gdrom_set_dev_ctrl_reg(struct gdrom_dev_ctrl *dev_ctrl_out,
                       reg32_t dev_ctrl_reg);

////////////////////////////////////////////////////////////////////////////////
//
// status flags (for REQ_STAT and the sector-number register)
//
////////////////////////////////////////////////////////////////////////////////

#define SEC_NUM_STATUS_SHIFT 0
#define SEC_NUM_STATUS_MASK (0xf << SEC_NUM_STATUS_SHIFT)

#define SEC_NUM_DISC_TYPE_SHIFT 4
#define SEC_NUM_DISC_TYPE_MASK (0xf << SEC_NUM_DISC_TYPE_SHIFT)

#define SEC_NUM_FMT_SHIFT 4
#define SEC_NUM_FMT_MASK (0xf << SEC_NUM_FMT_SHIFT)

#define N_GDROM_REGS (ADDR_GDROM_LAST - ADDR_GDROM_FIRST + 1)

DECL_MMIO_REGION(gdrom_reg_32, N_GDROM_REGS, ADDR_GDROM_FIRST, uint32_t)
DEF_MMIO_REGION(gdrom_reg_32, N_GDROM_REGS, ADDR_GDROM_FIRST, uint32_t)
DECL_MMIO_REGION(gdrom_reg_16, N_GDROM_REGS, ADDR_GDROM_FIRST, uint16_t)
DEF_MMIO_REGION(gdrom_reg_16, N_GDROM_REGS, ADDR_GDROM_FIRST, uint16_t)
DECL_MMIO_REGION(gdrom_reg_8, N_GDROM_REGS, ADDR_GDROM_FIRST, uint8_t)
DEF_MMIO_REGION(gdrom_reg_8, N_GDROM_REGS, ADDR_GDROM_FIRST, uint8_t)

static uint8_t reg_backing[N_GDROM_REGS];

float gdrom_reg_read_float(addr32_t addr, void *ctxt) {
    uint32_t tmp = mmio_region_gdrom_reg_32_read(&mmio_region_gdrom_reg_32,
                                                 addr);
    float val;
    memcpy(&val, &tmp, sizeof(val));
    return val;
}

void gdrom_reg_write_float(addr32_t addr, float val, void *ctxt) {
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    mmio_region_gdrom_reg_32_write(&mmio_region_gdrom_reg_32, addr, tmp);
}

double gdrom_reg_read_double(addr32_t addr, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void gdrom_reg_write_double(addr32_t addr, double val, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint8_t gdrom_reg_read_8(addr32_t addr, void *ctxt) {
    return mmio_region_gdrom_reg_8_read(&mmio_region_gdrom_reg_8, addr);
}

void gdrom_reg_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    mmio_region_gdrom_reg_8_write(&mmio_region_gdrom_reg_8, addr, val);
}

uint16_t gdrom_reg_read_16(addr32_t addr, void *ctxt) {
    return mmio_region_gdrom_reg_16_read(&mmio_region_gdrom_reg_16, addr);
}

void gdrom_reg_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    mmio_region_gdrom_reg_16_write(&mmio_region_gdrom_reg_16, addr, val);
}

uint32_t gdrom_reg_read_32(addr32_t addr, void *ctxt) {
    return mmio_region_gdrom_reg_32_read(&mmio_region_gdrom_reg_32, addr);
}

void gdrom_reg_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    mmio_region_gdrom_reg_32_write(&mmio_region_gdrom_reg_32, addr, val);
}

static uint32_t
gdrom_alt_status_mmio_read(struct mmio_region_gdrom_reg_32 *region,
                           unsigned idx, void *ctxt) {
    reg32_t stat_bin = gdrom_get_status_reg(&gdrom.stat_reg);
    GDROM_TRACE("read 0x%02x from alternate status register\n",
                (unsigned)stat_bin);
    return stat_bin;
}

static void
gdrom_dev_ctrl_mmio_write(struct mmio_region_gdrom_reg_32 *region,
                          unsigned idx, uint32_t val, void *ctxt) {
    gdrom_set_dev_ctrl_reg(&gdrom.dev_ctrl_reg, val);

    GDROM_TRACE("Write %08x to dev_ctrl_reg\n", (unsigned)val);
}

static uint8_t
gdrom_alt_status_mmio_read_8(struct mmio_region_gdrom_reg_8 *region,
                             unsigned idx, void *ctxt) {
    reg32_t stat_bin = gdrom_get_status_reg(&gdrom.stat_reg);

    GDROM_TRACE("read 0x%02x from alternate status register\n",
                (unsigned)stat_bin);
    return stat_bin;
}

static void
gdrom_dev_ctrl_mmio_write_8(struct mmio_region_gdrom_reg_8 *region,
                            unsigned idx, uint8_t val, void *ctxt) {
    gdrom_set_dev_ctrl_reg(&gdrom.dev_ctrl_reg, val);

    GDROM_TRACE("Write %08x to dev_ctrl_reg\n", (unsigned)val);
}

static uint8_t
gdrom_status_mmio_read_8(struct mmio_region_gdrom_reg_8 *region,
                         unsigned idx, void *ctxt) {
    /*
     * XXX
     * For the most part, I try to keep all the logic in gdrom.c and all the
     * encoding/decoding here in gdrom_reg.c (ie, gdrom.c manages the system
     * state and gdrom_reg.c translates data into/from the format the guest
     * software expects it to be in).
     *
     * This part here where I clear the interrupt flag is an exception to that
     * rule because I didn't it was worth it to add a layer of indirection to
     * this single function call.  If this function did more than just read from
     * a register and clear the interrupt flag, then I would have some
     * infrastructure in place to do that on its behalf in gdrom.c
     */
    holly_clear_ext_int(HOLLY_EXT_INT_GDROM);

    reg32_t stat_bin = gdrom_get_status_reg(&gdrom.stat_reg);
    GDROM_TRACE("read 0x%02x from status register\n", (unsigned)stat_bin);
    return stat_bin;
}

static uint32_t
gdrom_status_mmio_read(struct mmio_region_gdrom_reg_32 *region,
                       unsigned idx, void *ctxt) {
    /*
     * XXX
     * For the most part, I try to keep all the logic in gdrom.c and all the
     * encoding/decoding here in gdrom_reg.c (ie, gdrom.c manages the system
     * state and gdrom_reg.c translates data into/from the format the guest
     * software expects it to be in).
     *
     * This part here where I clear the interrupt flag is an exception to that
     * rule because I didn't it was worth it to add a layer of indirection to
     * this single function call.  If this function did more than just read from
     * a register and clear the interrupt flag, then I would have some
     * infrastructure in place to do that on its behalf in gdrom.c
     */
    holly_clear_ext_int(HOLLY_EXT_INT_GDROM);

    reg32_t stat_bin = gdrom_get_status_reg(&gdrom.stat_reg);
    GDROM_TRACE("read 0x%02x from status register\n", (unsigned)stat_bin);
    return stat_bin;
}

static uint32_t
gdrom_error_mmio_read(struct mmio_region_gdrom_reg_32 *region,
                      unsigned idx, void *ctxt) {
    reg32_t tmp = gdrom_get_error_reg(&gdrom.error_reg);
    GDROM_TRACE("read 0x%02x from error register\n", (unsigned)tmp);
    return tmp;
}

static uint8_t
gdrom_error_mmio_read_8(struct mmio_region_gdrom_reg_8 *region,
                        unsigned idx, void *ctxt) {
    reg32_t tmp = gdrom_get_error_reg(&gdrom.error_reg);
    GDROM_TRACE("read 0x%02x from error register\n", (unsigned)tmp);
    return tmp;
}

static void gdrom_cmd_reg_mmio_write_8(struct mmio_region_gdrom_reg_8 *region,
                                       unsigned idx, uint8_t val, void *ctxt) {
    GDROM_TRACE("write 0x%x to command register (4 bytes)\n", (unsigned)val);
    gdrom_input_cmd(val);
}

static void gdrom_cmd_reg_mmio_write(struct mmio_region_gdrom_reg_32 *region,
                                     unsigned idx, uint32_t val, void *ctxt) {
    GDROM_TRACE("write 0x%x to command register (4 bytes)\n", (unsigned)val);
    gdrom_input_cmd(val);
}

static uint16_t
gdrom_data_mmio_read_16(struct mmio_region_gdrom_reg_16 *region,
                        unsigned idx, void *ctxt) {
    uint16_t buf;
    gdrom_read_data((uint8_t*)&buf, sizeof(buf));
    return buf;
}

static uint32_t
gdrom_data_mmio_read(struct mmio_region_gdrom_reg_32 *region,
                     unsigned idx, void *ctxt) {
    uint32_t buf;
    gdrom_read_data((uint8_t*)&buf, sizeof(buf));
    return buf;
}

static void
gdrom_data_mmio_write_16(struct mmio_region_gdrom_reg_16 *region, unsigned idx,
                         uint16_t val, void *ctxt) {
    gdrom_write_data((uint8_t*)&val, sizeof(val));
}

static void
gdrom_data_mmio_write(struct mmio_region_gdrom_reg_32 *region, unsigned idx,
                      uint32_t val, void *ctxt) {
    gdrom_write_data((uint8_t*)&val, sizeof(val));
}

static void
gdrom_features_mmio_write_8(struct mmio_region_gdrom_reg_8 *region, unsigned idx,
                            uint8_t val, void *ctxt) {
    GDROM_TRACE("write 0x%08x to the features register\n", (unsigned)val);
    gdrom_set_features_reg(&gdrom.feat_reg, val);
}

static void
gdrom_features_mmio_write(struct mmio_region_gdrom_reg_32 *region, unsigned idx,
                          uint32_t val, void *ctxt) {
    GDROM_TRACE("write 0x%08x to the features register\n", (unsigned)val);
    gdrom_set_features_reg(&gdrom.feat_reg, val);
}

static void
gdrom_sect_cnt_mmio_write(struct mmio_region_gdrom_reg_32 *region,
                          unsigned idx, uint32_t val, void *ctxt) {
    GDROM_TRACE("Write %08x to sec_cnt_reg\n", (unsigned)val);
    gdrom_set_sect_cnt_reg(&gdrom.sect_cnt_reg, val);
}

static void
gdrom_sect_cnt_mmio_write_8(struct mmio_region_gdrom_reg_8 *region,
                          unsigned idx, uint8_t val, void *ctxt) {
    GDROM_TRACE("Write %08x to sec_cnt_reg\n", (unsigned)val);
    gdrom_set_sect_cnt_reg(&gdrom.sect_cnt_reg, val);
}

static uint32_t
gdrom_int_reason_mmio_read(struct mmio_region_gdrom_reg_32 *region,
                           unsigned idx, void *ctxt) {
    reg32_t tmp = gdrom_get_int_reason_reg(&gdrom.int_reason_reg);
    GDROM_TRACE("int_reason is 0x%08x\n", (unsigned)tmp);
    return tmp;
}

static uint8_t
gdrom_int_reason_mmio_read_8(struct mmio_region_gdrom_reg_8 *region,
                             unsigned idx, void *ctxt) {
    reg32_t tmp = gdrom_get_int_reason_reg(&gdrom.int_reason_reg);

    GDROM_TRACE("int_reason is 0x%08x\n", (unsigned)tmp);
    return tmp;
}

static uint8_t
gdrom_sector_num_mmio_read_8(struct mmio_region_gdrom_reg_8 *region,
                             unsigned idx, void *ctxt) {
    uint8_t status;
    status = ((uint8_t)gdrom_get_drive_state() << SEC_NUM_STATUS_SHIFT) |
        ((uint8_t)gdrom_get_disc_type() << SEC_NUM_DISC_TYPE_SHIFT);
    return status;
}

static uint32_t
gdrom_sector_num_mmio_read(struct mmio_region_gdrom_reg_32 *region,
                           unsigned idx, void *ctxt) {
    uint32_t status;
    status = ((uint32_t)gdrom_get_drive_state() << SEC_NUM_STATUS_SHIFT) |
        ((uint32_t)gdrom_get_disc_type() << SEC_NUM_DISC_TYPE_SHIFT);
    return status;
}

static uint8_t
gdrom_byte_count_low_mmio_read_8(struct mmio_region_gdrom_reg_8 *region,
                                 unsigned idx, void *ctxt) {
    uint8_t low = gdrom.data_byte_count & 0xff;

    GDROM_TRACE("read 0x%02x from byte_count_low\n", (unsigned)low);
    return low;
}

static uint32_t
gdrom_byte_count_low_mmio_read(struct mmio_region_gdrom_reg_32 *region,
                               unsigned idx, void *ctxt) {
    uint32_t low = gdrom.data_byte_count & 0xff;

    GDROM_TRACE("read 0x%02x from byte_count_low\n", (unsigned)low);
    return low;
}

static void
gdrom_byte_count_low_mmio_write(struct mmio_region_gdrom_reg_32 *region,
                                unsigned idx, uint32_t val, void *ctxt) {
    gdrom.data_byte_count = (gdrom.data_byte_count & ~0xff) | (val & 0xff);

    GDROM_TRACE("write 0x%02x to byte_count_low\n", (unsigned)(val & 0xff));
}

static void
gdrom_byte_count_low_mmio_write_8(struct mmio_region_gdrom_reg_8 *region,
                                  unsigned idx, uint8_t val, void *ctxt) {
    gdrom.data_byte_count = (gdrom.data_byte_count & ~0xff) | (val & 0xff);

    GDROM_TRACE("write 0x%02x to byte_count_low\n", (unsigned)(val & 0xff));
}

static uint8_t
gdrom_byte_count_high_mmio_read_8(struct mmio_region_gdrom_reg_8 *region,
                                unsigned idx, void *ctxt) {
    uint8_t high = (gdrom.data_byte_count & 0xff00) >> 8;

    GDROM_TRACE("read 0x%02x from byte_count_high\n", (unsigned)high);
    return high;
}

static void
gdrom_byte_count_high_mmio_write_8(struct mmio_region_gdrom_reg_8 *region,
                                   unsigned idx, uint8_t val, void *ctxt) {
    gdrom.data_byte_count =
        (gdrom.data_byte_count & ~0xff00) | ((val & 0xff) << 8);

    GDROM_TRACE("write 0x%02x to byte_count_high\n",
                (unsigned)((val & 0xff) << 8));
}

static uint32_t
gdrom_byte_count_high_mmio_read(struct mmio_region_gdrom_reg_32 *region,
                                unsigned idx, void *ctxt) {
    uint32_t high = (gdrom.data_byte_count & 0xff00) >> 8;

    GDROM_TRACE("read 0x%02x from byte_count_high\n", (unsigned)high);
    return high;
}

static void
gdrom_byte_count_high_mmio_write(struct mmio_region_gdrom_reg_32 *region,
                                 unsigned idx, uint32_t val, void *ctxt) {
    gdrom.data_byte_count =
        (gdrom.data_byte_count & ~0xff00) | ((val & 0xff) << 8);

    GDROM_TRACE("write 0x%02x to byte_count_high\n",
                (unsigned)((val & 0xff) << 8));
}

uint32_t
gdrom_gdapro_mmio_read(struct mmio_region_g1_reg_32 *region,
                       unsigned idx, void *ctxt) {
    GDROM_TRACE("read %08x from GDAPRO\n", gdrom.gdapro_reg);
    return gdrom.gdapro_reg;
}

void
gdrom_gdapro_mmio_write(struct mmio_region_g1_reg_32 *region,
                        unsigned idx, uint32_t val, void *ctxt) {
    // check security code
    if ((val & 0xffff0000) != 0x88430000)
        return;

    gdrom.gdapro_reg = val;

    GDROM_TRACE("GDAPRO (0x%08x) - allowing writes from 0x%08x through "
                "0x%08x\n",
                gdrom.gdapro_reg, gdrom_dma_prot_top(), gdrom_dma_prot_bot());
}

uint32_t
gdrom_g1gdrc_mmio_read(struct mmio_region_g1_reg_32 *region,
                       unsigned idx, void *ctxt) {
    GDROM_TRACE("read %08x from G1GDRC\n", gdrom.g1gdrc_reg);
    return gdrom.g1gdrc_reg;
}

void
gdrom_g1gdrc_mmio_write(struct mmio_region_g1_reg_32 *region,
                        unsigned idx, uint32_t val, void *ctxt) {
    GDROM_TRACE("write %08x to G1GDRC\n", gdrom.g1gdrc_reg);
    gdrom.g1gdrc_reg = val;
}

uint32_t
gdrom_gdstar_mmio_read(struct mmio_region_g1_reg_32 *region,
                       unsigned idx, void *ctxt) {
    GDROM_TRACE("read %08x from GDSTAR\n", gdrom.dma_start_addr_reg);
    return gdrom.dma_start_addr_reg;
}

void
gdrom_gdstar_mmio_write(struct mmio_region_g1_reg_32 *region,
                        unsigned idx, uint32_t val, void *ctxt) {
    gdrom.dma_start_addr_reg = val;
    gdrom.dma_start_addr_reg &= ~0xe0000000;
    GDROM_TRACE("write %08x to GDSTAR\n", gdrom.dma_start_addr_reg);
}

uint32_t
gdrom_gdlen_mmio_read(struct mmio_region_g1_reg_32 *region,
                      unsigned idx, void *ctxt) {
    GDROM_TRACE("read %08x from GDLEN\n", gdrom.dma_len_reg);
    return gdrom.dma_len_reg;
}

void
gdrom_gdlen_mmio_write(struct mmio_region_g1_reg_32 *region,
                       unsigned idx, uint32_t val, void *ctxt) {
    gdrom.dma_len_reg = val;
    GDROM_TRACE("write %08x to GDLEN\n", gdrom.dma_len_reg);
}

uint32_t
gdrom_gddir_mmio_read(struct mmio_region_g1_reg_32 *region,
                      unsigned idx, void *ctxt) {
    GDROM_TRACE("read %08x from GDDIR\n", gdrom.dma_dir_reg);
    return gdrom.dma_dir_reg;
}

void
gdrom_gddir_mmio_write(struct mmio_region_g1_reg_32 *region,
                       unsigned idx, uint32_t val, void *ctxt) {
    gdrom.dma_dir_reg = val;
    GDROM_TRACE("write %08x to GDDIR\n", gdrom.dma_dir_reg);
}

uint32_t
gdrom_gden_mmio_read(struct mmio_region_g1_reg_32 *region,
                     unsigned idx, void *ctxt) {
    GDROM_TRACE("read %08x from GDEN\n", gdrom.dma_en_reg);
    return gdrom.dma_en_reg;
}

void
gdrom_gden_mmio_write(struct mmio_region_g1_reg_32 *region,
                      unsigned idx, uint32_t val, void *ctxt) {
    gdrom.dma_en_reg = val;
    GDROM_TRACE("write %08x to GDEN\n", gdrom.dma_en_reg);
}

uint32_t
gdrom_gdst_reg_read_handler(struct mmio_region_g1_reg_32 *region,
                            unsigned idx, void *ctxt) {
    GDROM_TRACE("read %08x from GDST\n", gdrom.dma_start_reg);
    return gdrom.dma_start_reg;
}

void
gdrom_gdst_reg_write_handler(struct mmio_region_g1_reg_32 *region,
                             unsigned idx, uint32_t val, void *ctxt) {
    gdrom.dma_start_reg = val;
    GDROM_TRACE("write %08x to GDST\n", gdrom.dma_start_reg);
    gdrom_start_dma();
}

uint32_t
gdrom_gdlend_mmio_read(struct mmio_region_g1_reg_32 *region,
                       unsigned idx, void *ctxt) {
    GDROM_TRACE("read %08x from GDLEND\n", gdrom.gdlend_reg);
    return gdrom.gdlend_reg;
}

////////////////////////////////////////////////////////////////////////////////
//
// Error register flags
//
////////////////////////////////////////////////////////////////////////////////

#define GDROM_ERROR_SENSE_KEY_SHIFT 4
#define GDROM_ERROR_SENSE_KEY_MASK (0xf << GDROM_ERROR_SENSE_KEY_SHIFT)

#define GDROM_ERROR_MCR_SHIFT 3
#define GDROM_ERROR_MCR_MASK (1 << GDROM_ERROR_MCR_SHIFT)

#define GDROM_ERROR_ABRT_SHIFT 2
#define GDROM_ERROR_ABRT_MASK (1 << GDROM_ERROR_ABRT_SHIFT)

#define GDROM_ERROR_EOMF_SHIFT 1
#define GDROM_ERROR_EOMF_MASK (1 << GDROM_ERROR_EOMF_SHIFT)

#define GDROM_ERROR_ILI_SHIFT 0
#define GDROM_ERROR_ILI_MASK (1 << GDROM_ERROR_ILI_SHIFT)

static reg32_t gdrom_get_error_reg(struct gdrom_error const *error_in) {
    reg32_t error_reg =
        (((reg32_t)error_in->sense_key) << GDROM_ERROR_SENSE_KEY_SHIFT) &
        GDROM_ERROR_SENSE_KEY_MASK;

    if (error_in->ili)
        error_reg |= GDROM_ERROR_ILI_MASK;
    if (error_in->eomf)
        error_reg |= GDROM_ERROR_EOMF_MASK;
    if (error_in->abrt)
        error_reg |= GDROM_ERROR_ABRT_MASK;
    if (error_in->mcr)
        error_reg |= GDROM_ERROR_MCR_MASK;

    return error_reg;
}

////////////////////////////////////////////////////////////////////////////////
//
// Status register flags
//
////////////////////////////////////////////////////////////////////////////////

// the drive is processing a command
#define GDROM_STAT_BSY_SHIFT 7
#define GDROM_STAT_BSY_MASK (1 << GDROM_STAT_BSY_SHIFT)

// response to ATA command is possible
#define GDROM_STAT_DRDY_SHIFT 6
#define GDROM_STAT_DRDY_MASK (1 << GDROM_STAT_DRDY_SHIFT)

// drive fault
#define GDROM_STAT_DF_SHIFT 5
#define GDROM_STAT_DF_MASK (1 << GDROM_STAT_DF_SHIFT)

// seek processing is complete
#define GDROM_STAT_DSC_SHIFT 4
#define GDROM_STAT_DSC_MASK (1 << GDROM_STAT_DSC_SHIFT)

// data transfer possible
#define GDROM_STAT_DRQ_SHIFT 3
#define GDROM_STAT_DRQ_MASK (1 << GDROM_STAT_DRQ_SHIFT)

// correctable error flag
#define GDROM_STAT_CORR_SHIFT 2
#define GDROM_STAT_CORR_MASK (1 << GDROM_STAT_CORR_SHIFT)

// error flag
#define GDROM_STAT_CHECK_SHIFT 0
#define GDROM_STAT_CHECK_MASK (1 << GDROM_STAT_CHECK_SHIFT)

static reg32_t gdrom_get_status_reg(struct gdrom_status const *stat_in) {
    reg32_t stat_reg = 0;

    if (stat_in->bsy)
        stat_reg |= GDROM_STAT_BSY_MASK;
    if (stat_in->drdy)
        stat_reg |= GDROM_STAT_DRDY_MASK;
    if (stat_in->df)
        stat_reg |= GDROM_STAT_DF_MASK;
    if (stat_in->dsc)
        stat_reg |= GDROM_STAT_DSC_MASK;
    if (stat_in->drq)
        stat_reg |= GDROM_STAT_DRQ_MASK;
    if (stat_in->corr)
        stat_reg |= GDROM_STAT_CORR_MASK;
    if (stat_in->check)
        stat_reg |= GDROM_STAT_CHECK_MASK;

    return stat_reg;
}

////////////////////////////////////////////////////////////////////////////////
//
// feature register flags
//
////////////////////////////////////////////////////////////////////////////////

#define FEAT_REG_DMA_SHIFT 0
#define FEAT_REG_DMA_MASK (1 << FEAT_REG_DMA_SHIFT)

static void
gdrom_set_features_reg(struct gdrom_features *features_out, reg32_t feat_reg) {
    if (feat_reg & FEAT_REG_DMA_MASK)
        features_out->dma_enable = true;
    else
        features_out->dma_enable = false;

    if ((feat_reg & 0x7f) == 3)
        features_out->set_feat_enable = true;
    else
        features_out->set_feat_enable = false;
}

////////////////////////////////////////////////////////////////////////////////
//
// Transfer Modes (for the sector count register in GDROM_CMD_SEAT_FEAT)
//
////////////////////////////////////////////////////////////////////////////////

#define TRANS_MODE_PIO_DFLT_MASK        0xfe
#define TRANS_MODE_PIO_DFLT_VAL         0x00

#define TRANS_MODE_PIO_FLOW_CTRL_MASK   0xf8
#define TRANS_MODE_PIO_FLOW_CTRL_VAL    0x08

#define TRANS_MODE_SINGLE_WORD_DMA_MASK 0xf8
#define TRANS_MODE_SINGLE_WORD_DMA_VAL  0x10

#define TRANS_MODE_MULTI_WORD_DMA_MASK  0xf8
#define TRANS_MODE_MULTI_WORD_DMA_VAL   0x20

#define TRANS_MODE_PSEUDO_DMA_MASK      0xf8
#define TRANS_MODE_PSEUDO_DMA_VAL       0x18

#define SECT_CNT_MODE_VAL_SHIFT 0
#define SECT_CNT_MODE_VAL_MASK (0xf << SECT_CNT_MODE_VAL_SHIFT)

static void
gdrom_set_sect_cnt_reg(struct gdrom_sector_count *sect_cnt_out,
                       reg32_t sect_cnt_reg) {
    unsigned mode_val =
        (sect_cnt_reg & SECT_CNT_MODE_VAL_MASK) >> SECT_CNT_MODE_VAL_SHIFT;
    if ((sect_cnt_reg & TRANS_MODE_PIO_DFLT_MASK) ==
        TRANS_MODE_PIO_DFLT_VAL) {
        sect_cnt_out->trans_mode = TRANS_MODE_PIO_DFLT;
    } else if ((sect_cnt_reg & TRANS_MODE_PIO_FLOW_CTRL_MASK) ==
               TRANS_MODE_PIO_FLOW_CTRL_VAL) {
        sect_cnt_out->trans_mode = TRANS_MODE_PIO_FLOW_CTRL;
    } else if ((sect_cnt_reg & TRANS_MODE_SINGLE_WORD_DMA_MASK) ==
               TRANS_MODE_SINGLE_WORD_DMA_VAL) {
        sect_cnt_out->trans_mode = TRANS_MODE_SINGLE_WORD_DMA;
    } else if ((sect_cnt_reg & TRANS_MODE_MULTI_WORD_DMA_MASK) ==
               TRANS_MODE_MULTI_WORD_DMA_VAL) {
        sect_cnt_out->trans_mode = TRANS_MODE_MULTI_WORD_DMA;
    } else if ((sect_cnt_reg & TRANS_MODE_PSEUDO_DMA_MASK) ==
               TRANS_MODE_PSEUDO_DMA_VAL) {
        sect_cnt_out->trans_mode = TRANS_MODE_PSEUDO_DMA;
    } else {
        // TODO: maybe this should be a soft warning instead of an error
        GDROM_TRACE("unrecognized transfer mode (sec_cnt_reg is 0x%08x)\n",
                    sect_cnt_reg);
        error_set_feature("unrecognized transfer mode\n");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    sect_cnt_out->mode_val = mode_val;
}

////////////////////////////////////////////////////////////////////////////////
//
// Interrupt Reason register flags
//
////////////////////////////////////////////////////////////////////////////////

// ready to receive command
#define INT_REASON_COD_SHIFT 0
#define INT_REASON_COD_MASK (1 << INT_REASON_COD_SHIFT)

/*
 * ready to receive data from software to drive if set
 * ready to send data from drive to software if not set
 */
#define INT_REASON_IO_SHIFT 1
#define INT_REASON_IO_MASK (1 << INT_REASON_IO_SHIFT)

static reg32_t gdrom_get_int_reason_reg(struct gdrom_int_reason *int_reason_in) {
    reg32_t reg_out = 0;

    if (int_reason_in->cod)
        reg_out |= INT_REASON_COD_MASK;
    if (int_reason_in->io)
        reg_out |= INT_REASON_IO_MASK;

    return reg_out;
}

////////////////////////////////////////////////////////////////////////////////
//
// Device control register flags
//
////////////////////////////////////////////////////////////////////////////////

#define DEV_CTRL_NIEN_SHIFT 1
#define DEV_CTRL_NIEN_MASK (1 << DEV_CTRL_NIEN_SHIFT)

#define DEV_CTRL_SRST_SHIFT 2
#define DEV_CTRL_SRST_MASK (1 << DEV_CTRL_SRST_SHIFT)

static void
gdrom_set_dev_ctrl_reg(struct gdrom_dev_ctrl *dev_ctrl_out,
                       reg32_t dev_ctrl_reg) {
    dev_ctrl_out->nien = (bool)(dev_ctrl_reg & DEV_CTRL_NIEN_MASK);
    dev_ctrl_out->srst = (bool)(dev_ctrl_reg & DEV_CTRL_SRST_MASK);
}

void gdrom_reg_init(void) {
    init_mmio_region_gdrom_reg_32(&mmio_region_gdrom_reg_32, (void*)reg_backing);
    init_mmio_region_gdrom_reg_16(&mmio_region_gdrom_reg_16, (void*)reg_backing);
    init_mmio_region_gdrom_reg_8(&mmio_region_gdrom_reg_8, (void*)reg_backing);

    mmio_region_gdrom_reg_8_init_cell(&mmio_region_gdrom_reg_8,
                                      "alt status/device control", 0x5f7018,
                                      gdrom_alt_status_mmio_read_8,
                                      gdrom_dev_ctrl_mmio_write_8, NULL);
    mmio_region_gdrom_reg_8_init_cell(&mmio_region_gdrom_reg_8,
                                      "Error/features", 0x5f7084,
                                      gdrom_error_mmio_read_8,
                                      gdrom_features_mmio_write_8, NULL);
    mmio_region_gdrom_reg_8_init_cell(&mmio_region_gdrom_reg_8,
                                      "Interrupt reason/sector count", 0x5f7088,
                                      gdrom_int_reason_mmio_read_8,
                                      gdrom_sect_cnt_mmio_write_8, NULL);
    mmio_region_gdrom_reg_8_init_cell(&mmio_region_gdrom_reg_8,
                                      "status/command", 0x5f709c,
                                      gdrom_status_mmio_read_8,
                                      gdrom_cmd_reg_mmio_write_8, NULL);
    mmio_region_gdrom_reg_8_init_cell(&mmio_region_gdrom_reg_8,
                                      "Sector number", 0x5f708c,
                                      gdrom_sector_num_mmio_read_8,
                                      mmio_region_gdrom_reg_8_warn_write_handler,
                                      NULL);
    mmio_region_gdrom_reg_8_init_cell(&mmio_region_gdrom_reg_8,
                                      "Byte Count (low)", 0x5f7090,
                                      gdrom_byte_count_low_mmio_read_8,
                                      gdrom_byte_count_low_mmio_write_8,
                                      NULL);
    mmio_region_gdrom_reg_8_init_cell(&mmio_region_gdrom_reg_8,
                                      "Byte Count (high)", 0x5f7094,
                                      gdrom_byte_count_high_mmio_read_8,
                                      gdrom_byte_count_high_mmio_write_8,
                                      NULL);
    mmio_region_gdrom_reg_8_init_cell(&mmio_region_gdrom_reg_8,
                                      "Drive Select", 0x5f7098,
                                      mmio_region_gdrom_reg_8_warn_read_handler,
                                      mmio_region_gdrom_reg_8_warn_write_handler,
                                      NULL);

    mmio_region_gdrom_reg_16_init_cell(&mmio_region_gdrom_reg_16,
                                       "GD-ROM Data", 0x5f7080,
                                       gdrom_data_mmio_read_16,
                                       gdrom_data_mmio_write_16,
                                       NULL);

    mmio_region_gdrom_reg_32_init_cell(&mmio_region_gdrom_reg_32,
                                       "Drive Select", 0x5f7098,
                                       mmio_region_gdrom_reg_32_warn_read_handler,
                                       mmio_region_gdrom_reg_32_warn_write_handler,
                                       NULL);
    mmio_region_gdrom_reg_32_init_cell(&mmio_region_gdrom_reg_32,
                                       "alt status/device control", 0x5f7018,
                                       gdrom_alt_status_mmio_read,
                                       gdrom_dev_ctrl_mmio_write,
                                       NULL);
    mmio_region_gdrom_reg_32_init_cell(&mmio_region_gdrom_reg_32,
                                       "status/command", 0x5f709c,
                                       gdrom_status_mmio_read,
                                       gdrom_cmd_reg_mmio_write,
                                       NULL);
    mmio_region_gdrom_reg_32_init_cell(&mmio_region_gdrom_reg_32,
                                       "GD-ROM Data", 0x5f7080,
                                       gdrom_data_mmio_read,
                                       gdrom_data_mmio_write, NULL);
    mmio_region_gdrom_reg_32_init_cell(&mmio_region_gdrom_reg_32,
                                       "Error/features", 0x5f7084,
                                       gdrom_error_mmio_read,
                                       gdrom_features_mmio_write,
                                       NULL);
    mmio_region_gdrom_reg_32_init_cell(&mmio_region_gdrom_reg_32,
                                       "Interrupt reason/sector count", 0x5f7088,
                                       gdrom_int_reason_mmio_read,
                                       gdrom_sect_cnt_mmio_write, NULL);
    mmio_region_gdrom_reg_32_init_cell(&mmio_region_gdrom_reg_32,
                                       "Sector number", 0x5f708c,
                                       gdrom_sector_num_mmio_read,
                                       mmio_region_gdrom_reg_32_warn_write_handler,
                                       NULL);
    mmio_region_gdrom_reg_32_init_cell(&mmio_region_gdrom_reg_32,
                                       "Byte Count (low)", 0x5f7090,
                                       gdrom_byte_count_low_mmio_read,
                                       gdrom_byte_count_low_mmio_write,
                                       NULL);
    mmio_region_gdrom_reg_32_init_cell(&mmio_region_gdrom_reg_32,
                                       "Byte Count (high)", 0x5f7094,
                                       gdrom_byte_count_high_mmio_read,
                                       gdrom_byte_count_high_mmio_write,
                                       NULL);
}

void gdrom_reg_cleanup(void) {
    cleanup_mmio_region_gdrom_reg_8(&mmio_region_gdrom_reg_8);
    cleanup_mmio_region_gdrom_reg_16(&mmio_region_gdrom_reg_16);
    cleanup_mmio_region_gdrom_reg_32(&mmio_region_gdrom_reg_32);
}

struct memory_interface gdrom_reg_intf = {
    .read32 = gdrom_reg_read_32,
    .read16 = gdrom_reg_read_16,
    .read8 = gdrom_reg_read_8,
    .readfloat = gdrom_reg_read_float,
    .readdouble = gdrom_reg_read_double,

    .write32 = gdrom_reg_write_32,
    .write16 = gdrom_reg_write_16,
    .write8 = gdrom_reg_write_8,
    .writefloat = gdrom_reg_write_float,
    .writedouble = gdrom_reg_write_double
};
