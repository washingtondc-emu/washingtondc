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

#include "washdc/MemoryMap.h"
#include "holly_intc.h"
#include "mem_code.h"
#include "dreamcast.h"
#include "hw/sh4/sh4.h"
#include "hw/sh4/sh4_dmac.h"
#include "log.h"
#include "mmio.h"

#include "sys_block.h"

#define N_SYS_REGS (ADDR_SYS_LAST - ADDR_SYS_FIRST + 1)

DEF_MMIO_REGION(sys_block, N_SYS_REGS, ADDR_SYS_FIRST, uint32_t)

static struct mmio_region_sys_block mmio_region_sys_block;

static uint8_t reg_backing[N_SYS_REGS];

static uint32_t reg_sb_c2dstat, reg_sb_c2dlen;

float sys_block_read_float(addr32_t addr, void *ctxt) {
    uint32_t tmp = mmio_region_sys_block_read(&mmio_region_sys_block, addr);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

void sys_block_write_float(addr32_t addr, float val, void *ctxt) {
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    mmio_region_sys_block_write(&mmio_region_sys_block, addr, tmp);
}

double sys_block_read_double(addr32_t addr, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void sys_block_write_double(addr32_t addr, double val, void *ctxt) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint8_t sys_block_read_8(addr32_t addr, void *ctxt) {
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void sys_block_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint16_t sys_block_read_16(addr32_t addr, void *ctxt) {
    error_set_length(2);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void sys_block_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    error_set_length(2);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint32_t sys_block_read_32(addr32_t addr, void *ctxt) {
    return mmio_region_sys_block_read(&mmio_region_sys_block, addr);
}

void sys_block_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    mmio_region_sys_block_write(&mmio_region_sys_block, addr, val);
}

static uint32_t
sb_c2dstat_mmio_read(struct mmio_region_sys_block *region,
                     unsigned idx, void *ctxt) {
    LOG_DBG("reading %08x from SB_C2DSTAT\n", (unsigned)reg_sb_c2dstat);
    return reg_sb_c2dstat;
}

static void
sb_c2dstat_mmio_write(struct mmio_region_sys_block *region,
                      unsigned idx, uint32_t val, void *ctxt) {
    reg_sb_c2dstat = val;
    LOG_DBG("writing %08x to SB_C2DSTAT\n", (unsigned)reg_sb_c2dstat);
}

static uint32_t
sb_c2dlen_mmio_read(struct mmio_region_sys_block *region,
                    unsigned idx, void *ctxt) {
    LOG_DBG("reading %08x from SB_C2DLEN\n", (unsigned)reg_sb_c2dlen);
    return reg_sb_c2dlen;
}

static void
sb_c2dlen_mmio_write(struct mmio_region_sys_block *region,
                     unsigned idx, uint32_t val, void *ctxt) {
    reg_sb_c2dlen = val;
    LOG_DBG("writing %08x to SB_C2DLEN\n", (unsigned)reg_sb_c2dlen);
}

static uint32_t
sb_c2dst_mmio_read(struct mmio_region_sys_block *region,
                   unsigned idx, void *ctxt) {
    LOG_DBG("WARNING: reading 0 from SB_C2DST\n");
    return 0;
}

static void
sb_c2dst_mmio_write(struct mmio_region_sys_block *region,
                    unsigned idx, uint32_t val, void *ctxt) {
    if (val)
        sh4_dmac_channel2(dreamcast_get_cpu(), reg_sb_c2dstat, reg_sb_c2dlen);
}

static uint32_t
sys_sbrev_mmio_read(struct mmio_region_sys_block *region,
                    unsigned idx, void *ctxt) {
    return 16;
}

void sys_block_init(void) {
    init_mmio_region_sys_block(&mmio_region_sys_block, (void*)reg_backing);

    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_C2DSTAT", 0x005f6800,
                                    sb_c2dstat_mmio_read,
                                    sb_c2dstat_mmio_write,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_C2DLEN", 0x005f6804,
                                    sb_c2dlen_mmio_read,
                                    sb_c2dlen_mmio_write,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_C2DST", 0x005f6808,
                                    sb_c2dst_mmio_read,
                                    sb_c2dst_mmio_write,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_SDSTAW", 0x5f6810,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_SDBAAW", 0x5f6814,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_SDWLT", 0x5f6818,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_SDLAS", 0x5f681c,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_SDST", 0x5f6820,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_DBREQM", 0x5f6840,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_BAVLWC", 0x5f6844,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_C2DPRYC", 0x5f6848,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);
    /* TODO: spec says default val if SB_C2DMAXL is 1, but bios writes 0 ? */
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_C2DMAXL", 0x5f684c,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_LMMODE0", 0x5f6884,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_LMMODE1", 0x5f6888,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_FFST", 0x5f688c,
                                    mmio_region_sys_block_silent_read_handler,
                                    mmio_region_sys_block_silent_write_handler,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_SBREV", 0x5f689c,
                                    sys_sbrev_mmio_read,
                                    mmio_region_sys_block_readonly_write_error,
                                    NULL);
    /* TODO: spec says default val if SB_RBSPLT's MSB is 0, but bios writes 1 */
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_RBSPLT", 0x5f68a0,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "UNKNOWN_REG_5f68a4", 0x5f68a4,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "UNKNOWN_REG_5f68ac", 0x5f68ac,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_IML2NRM", 0x5f6910,
                                    holly_reg_iml2nrm_mmio_read,
                                    holly_reg_iml2nrm_mmio_write,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_IML2EXT", 0x5f6914,
                                    holly_reg_iml2ext_mmio_read,
                                    holly_reg_iml2ext_mmio_write,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_IML2ERR", 0x5f6918,
                                    holly_reg_iml2err_mmio_read,
                                    holly_reg_iml2err_mmio_write,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_IML4NRM", 0x5f6920,
                                    holly_reg_iml4nrm_mmio_read,
                                    holly_reg_iml4nrm_mmio_write,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_IML4EXT", 0x5f6924,
                                    holly_reg_iml4ext_mmio_read,
                                    holly_reg_iml4ext_mmio_write,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_IML4ERR", 0x5f6928,
                                    holly_reg_iml4err_mmio_read,
                                    holly_reg_iml4err_mmio_write,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_IML6NRM", 0x5f6930,
                                    holly_reg_iml6nrm_mmio_read,
                                    holly_reg_iml6nrm_mmio_write,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_IML6EXT", 0x5f6934,
                                    holly_reg_iml6ext_mmio_read,
                                    holly_reg_iml6ext_mmio_write,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_IML6ERR", 0x5f6938,
                                    holly_reg_iml6err_mmio_read,
                                    holly_reg_iml6err_mmio_write,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_PDTNRM", 0x5f6940,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_PDTEXT", 0x5f6944,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);

    /* arguably these ones should go into their own hw/g2 subdirectory... */
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_G2DTNRM", 0x5f6950,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_G2DTEXT", 0x5f6954,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    NULL);

    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_ISTNRM", 0x5f6900,
                                    holly_reg_istnrm_mmio_read,
                                    holly_reg_istnrm_mmio_write,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_ISTEXT", 0x5f6904,
                                    holly_reg_istext_mmio_read,
                                    holly_reg_istext_mmio_write,
                                    NULL);
    mmio_region_sys_block_init_cell(&mmio_region_sys_block,
                                    "SB_ISTERR", 0x5f6908,
                                    holly_reg_isterr_mmio_read,
                                    holly_reg_isterr_mmio_write,
                                    NULL);
}

void sys_block_cleanup(void) {
    cleanup_mmio_region_sys_block(&mmio_region_sys_block);
}

struct memory_interface sys_block_intf = {
    .read32 = sys_block_read_32,
    .read16 = sys_block_read_16,
    .read8 = sys_block_read_8,
    .readfloat = sys_block_read_float,
    .readdouble = sys_block_read_double,

    .write32 = sys_block_write_32,
    .write16 = sys_block_write_16,
    .write8 = sys_block_write_8,
    .writefloat = sys_block_write_float,
    .writedouble = sys_block_write_double
};
