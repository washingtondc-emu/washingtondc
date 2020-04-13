/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2020 snickerbockers
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
#include "hw/pvr2/pvr2_ta.h"
#include "intmath.h"

#include "sys_block.h"

#define N_SYS_REGS (ADDR_SYS_LAST - ADDR_SYS_FIRST + 1)

DEF_MMIO_REGION(sys_block, N_SYS_REGS, ADDR_SYS_FIRST, uint32_t)

#define SB_REG_IDX(paddr) ((paddr - ADDR_SYS_FIRST) / 4)

#define SB_IDX_C2DSTAT SB_REG_IDX(0x5f6800)
#define SB_IDX_C2DLEN  SB_REG_IDX(0x5f6804)

// sdstaw - Sort-DMA link address
#define SB_IDX_SDSTAW SB_REG_IDX(0x5f6810)
#define SB_IDX_SDBAAW SB_REG_IDX(0x5f6814)

// 0 for 16-bit Sort-DMA link address, 1 for 32-bit Sort-DMA link address
#define SB_IDX_SDWLT  SB_REG_IDX(0x5f6818)

// if 0, then Sort-DMA link addresses are scaled by 32.  Else, not.
#define SB_IDX_SDLAS  SB_REG_IDX(0x5f681c)

/*
 * write 1 to initiate Sort-DMA.  write 0 to cancel it.
 * read 1 to confirm Sort-DMA in progress, 0 to confirm it's not in progress
 */
#define SB_IDX_SDST   SB_REG_IDX(0x5f6820)

float sys_block_read_float(addr32_t addr, void *argp) {
    struct sys_block_ctxt *ctxt = (struct sys_block_ctxt *)argp;
    uint32_t tmp =
        mmio_region_sys_block_read(&ctxt->mmio_region_sys_block, addr);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

void sys_block_write_float(addr32_t addr, float val, void *argp) {
    struct sys_block_ctxt *ctxt = (struct sys_block_ctxt *)argp;
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    mmio_region_sys_block_write(&ctxt->mmio_region_sys_block,
                                addr, tmp);
}

double sys_block_read_double(addr32_t addr, void *argp) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void sys_block_write_double(addr32_t addr, double val, void *argp) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint8_t sys_block_read_8(addr32_t addr, void *argp) {
    if ((addr & BIT_RANGE(0, 28)) == 0x5f689c)
        return 16; // SB_SBREV
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void sys_block_write_8(addr32_t addr, uint8_t val, void *argp) {
    error_set_length(1);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint16_t sys_block_read_16(addr32_t addr, void *argp) {
    error_set_length(2);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void sys_block_write_16(addr32_t addr, uint16_t val, void *argp) {
    error_set_length(2);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint32_t sys_block_read_32(addr32_t addr, void *argp) {
    struct sys_block_ctxt *ctxt = (struct sys_block_ctxt *)argp;
    return mmio_region_sys_block_read(&ctxt->mmio_region_sys_block, addr);
}

void sys_block_write_32(addr32_t addr, uint32_t val, void *argp) {
    struct sys_block_ctxt *ctxt = (struct sys_block_ctxt *)argp;
    mmio_region_sys_block_write(&ctxt->mmio_region_sys_block,
                                addr, val);
}

static uint32_t
sb_c2dst_mmio_read(struct mmio_region_sys_block *region,
                   unsigned idx, void *argp) {
    LOG_DBG("WARNING: reading 0 from SB_C2DST\n");
    return 0;
}

static void
sb_c2dst_mmio_write(struct mmio_region_sys_block *region,
                    unsigned idx, uint32_t val, void *argp) {
    if (val) {
        struct sys_block_ctxt *ctxt = (struct sys_block_ctxt *)argp;
        sh4_dmac_channel2(ctxt->sh4, ctxt->reg_backing[SB_IDX_C2DSTAT],
                          ctxt->reg_backing[SB_IDX_C2DLEN]);
    }
}

static uint32_t
sys_sbrev_mmio_read(struct mmio_region_sys_block *region,
                    unsigned idx, void *argp) {
    return 16;
}

static uint32_t
tfrem_reg_read_handler(struct mmio_region_sys_block *region,
                       unsigned idx, void *argp) {
    return pvr2_ta_fifo_rem_bytes() / 32;
}

static uint32_t lmmode0_reg_read_handler(struct mmio_region_sys_block *region,
                                         unsigned idx, void *argp) {
    return dc_get_lmmode0();
}

static void lmmode0_reg_write_handler(struct mmio_region_sys_block *region,
                                      unsigned idx, uint32_t val, void *argp) {
    dc_set_lmmode0(val & 1);
}

static uint32_t lmmode1_reg_read_handler(struct mmio_region_sys_block *region,
                                         unsigned idx, void *argp) {
    return dc_get_lmmode1();
}

static void lmmode1_reg_write_handler(struct mmio_region_sys_block *region,
                                      unsigned idx, uint32_t val, void *argp) {
    dc_set_lmmode1(val & 1);
}

static uint32_t sdst_reg_read_handler(struct mmio_region_sys_block *region,
                                      unsigned idx, void *argp) {
    struct sys_block_ctxt *ctxt = (struct sys_block_ctxt *)argp;
    if (ctxt->sort_dma_in_progress) {
        LOG_DBG("reading 1 from SDST\n");
        return 1;
    } else {
        LOG_DBG("reading 0 from SDST\n");
        return 0;
    }
}

// TODO: compe up with a realistic timing value for this
#define SORT_DMA_COMPLETE_INT_DELAY 0

static DEF_ERROR_U32_ATTR(sdstaw_reg)
static DEF_ERROR_U32_ATTR(sdbaaw_reg)
static DEF_ERROR_U32_ATTR(sdwlt_reg)
static DEF_ERROR_U32_ATTR(sdlas_reg)
static DEF_ERROR_U32_ATTR(sdst_reg)

static uint32_t sort_dma_process_link(struct sys_block_ctxt *ctxt, uint32_t link_addr, uint32_t link_base);

static void
sys_block_sort_dma_complete_int_event_handler(struct SchedEvent *event) {
    struct sys_block_ctxt *ctxt = (struct sys_block_ctxt *)event->arg_ptr;
    if (!ctxt->sort_dma_in_progress)
        RAISE_ERROR(ERROR_INTEGRITY);
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_SORT_DMA_COMPLETE);
    ctxt->sort_dma_in_progress = false;
}

static void sdst_reg_write_handler(struct mmio_region_sys_block *region,
                                   unsigned idx, uint32_t val, void *argp) {
    struct sys_block_ctxt *ctxt = (struct sys_block_ctxt *)argp;
    if (ctxt->sort_dma_in_progress) {
        error_set_feature("writing to SDST when Sort-DMA is already in-progress");
        error_set_value(val);
        error_set_sdstaw_reg(ctxt->reg_backing[SB_IDX_SDSTAW]);
        error_set_sdbaaw_reg(ctxt->reg_backing[SB_IDX_SDBAAW]);
        error_set_sdwlt_reg(ctxt->reg_backing[SB_IDX_SDWLT]);
        error_set_sdlas_reg(ctxt->reg_backing[SB_IDX_SDLAS]);
        error_set_sdst_reg(ctxt->reg_backing[SB_IDX_SDST]);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    if (val) {
        ctxt->sort_dma_in_progress = true;
        struct Memory *main_memory = ctxt->main_memory;

        // Oh boy!  It's Sort-DMA!
        LOG_DBG("Sort-DMA transaction begins!\n");

        if (!(ctxt->reg_backing[SB_IDX_SDWLT] & 1)) {
            error_set_feature("16-bit Sort_DMA link addresses");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        if (ctxt->reg_backing[SB_IDX_SDLAS] & 1) {
            /*
             * this is actually super-easy, all we really have to
             * do is divide the link address by 32 or something like that.
             */
            error_set_feature("sort-dma scaling by 32");
            error_set_value(val);
            error_set_sdstaw_reg(ctxt->reg_backing[SB_IDX_SDSTAW]);
            error_set_sdbaaw_reg(ctxt->reg_backing[SB_IDX_SDBAAW]);
            error_set_sdwlt_reg(ctxt->reg_backing[SB_IDX_SDWLT]);
            error_set_sdlas_reg(ctxt->reg_backing[SB_IDX_SDLAS]);
            error_set_sdst_reg(ctxt->reg_backing[SB_IDX_SDST]);
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        uint32_t link_base =
            (ctxt->reg_backing[SB_IDX_SDBAAW] & BIT_RANGE(5, 26)) | (1 << 27);
        uint32_t link_table_start =
            (ctxt->reg_backing[SB_IDX_SDSTAW] & BIT_RANGE(5, 26)) | (1 << 27);

        if (link_table_start < 0x0c000000 || link_table_start >= 0x0d000000) {
            error_set_feature("Sort-DMA memory mirrors");
            error_set_address(link_table_start);
            error_set_value(val);
            error_set_sdstaw_reg(ctxt->reg_backing[SB_IDX_SDSTAW]);
            error_set_sdbaaw_reg(ctxt->reg_backing[SB_IDX_SDBAAW]);
            error_set_sdwlt_reg(ctxt->reg_backing[SB_IDX_SDWLT]);
            error_set_sdlas_reg(ctxt->reg_backing[SB_IDX_SDLAS]);
            error_set_sdst_reg(ctxt->reg_backing[SB_IDX_SDST]);
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        uint32_t link_addr = memory_read_32(link_table_start & MEMORY_MASK,
                                            main_memory);

        while (link_addr != 2) {
            LOG_DBG("the next link addr is %08X\n", (unsigned)link_addr);
            if (link_addr == 1) {
                // end of link
                link_table_start += 4;
                LOG_DBG("link_table_start incremented to %08X\n",
                        (unsigned)link_table_start);
                link_addr = memory_read_32(link_table_start & MEMORY_MASK,
                                           main_memory);
                continue;
            }

            link_addr = sort_dma_process_link(ctxt, link_addr, link_base);
        }

        // end of DMA
        LOG_ERROR("END OF SORT-DMA; FINAL LINK TABLE START IS %08X\n",
                  (unsigned)link_table_start);
        /*
         * TODO: I'm not 100% sure if it's actually correct to write this back.
         * I *think* it is but I could be wrong.
         */
        ctxt->reg_backing[SB_IDX_SDSTAW] = link_table_start;

        struct dc_clock *clk = ctxt->clk;
        ctxt->sort_dma_in_progress = true;
        ctxt->sort_dma_complete_int_event.when = clock_cycle_stamp(clk) +
            SORT_DMA_COMPLETE_INT_DELAY;
        sched_event(clk, &ctxt->sort_dma_complete_int_event);
    }
}

static uint32_t sort_dma_process_link(struct sys_block_ctxt *ctxt, uint32_t link_addr, uint32_t link_base) {
    uint32_t link_ptr = link_addr + link_base;
    struct Memory *main_memory = ctxt->main_memory;

    LOG_DBG("link_address is %08X\n", (unsigned)link_addr);
    LOG_DBG("link_ptr is %08X\n", (unsigned)link_ptr);

    uint32_t param_tp = memory_read_32(link_ptr & MEMORY_MASK, main_memory);

    LOG_DBG("parameter control word is %08X\n", param_tp);

    struct pvr2_ta_param_dims dims = pvr2_ta_get_param_dims(param_tp);

    if (dims.is_vert) {
        error_set_feature("they sent a vertex parameter at the beginning "
                          "of a sort-DMA...\n");
        error_set_sdstaw_reg(ctxt->reg_backing[SB_IDX_SDSTAW]);
        error_set_sdbaaw_reg(ctxt->reg_backing[SB_IDX_SDBAAW]);
        error_set_sdwlt_reg(ctxt->reg_backing[SB_IDX_SDWLT]);
        error_set_sdlas_reg(ctxt->reg_backing[SB_IDX_SDLAS]);
        error_set_sdst_reg(ctxt->reg_backing[SB_IDX_SDST]);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    uint32_t n_bytes =
        memory_read_32((link_ptr + 0x18) & MEMORY_MASK, main_memory);
    if (n_bytes > 255) {
        error_set_feature("when there's a Sort-DMA link that's too long");
        error_set_sdstaw_reg(ctxt->reg_backing[SB_IDX_SDSTAW]);
        error_set_sdbaaw_reg(ctxt->reg_backing[SB_IDX_SDBAAW]);
        error_set_sdwlt_reg(ctxt->reg_backing[SB_IDX_SDWLT]);
        error_set_sdlas_reg(ctxt->reg_backing[SB_IDX_SDLAS]);
        error_set_sdst_reg(ctxt->reg_backing[SB_IDX_SDST]);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    } else if (n_bytes == 0) {
        n_bytes = 8192;
    } else {
        n_bytes *= 32;
    }

    uint32_t next_link_addr =
        memory_read_32((link_ptr + 0x1c) & MEMORY_MASK, main_memory);
    int vtx_len = dims.vtx_len;

    uint32_t cur_ptr = link_ptr;
    LOG_DBG("this link in the chain is %u bytes long\n", (unsigned)n_bytes);
    while (n_bytes) {
        param_tp = memory_read_32(link_ptr & MEMORY_MASK, main_memory);
        dims = pvr2_ta_get_param_dims(param_tp);

        int this_pkt_dwords;
        if (dims.is_vert) {
            this_pkt_dwords = vtx_len;
            LOG_DBG("Sort-DMA vertex parameter len %u bytes pointer %08X\n",
                    vtx_len * 4, cur_ptr);
        } else {
            vtx_len = dims.vtx_len;
            this_pkt_dwords = dims.hdr_len;
            LOG_DBG("Sort-DMA packet header len %u bytes pointer %08X\n",
                    dims.hdr_len * 4, cur_ptr);
        }

        while (this_pkt_dwords) {
            uint32_t dword = memory_read_32(cur_ptr & MEMORY_MASK,
                                            main_memory);
            pvr2_tafifo_input(ctxt->pvr2, dword);
            this_pkt_dwords--;
            cur_ptr += 4;
            n_bytes -= 4;
        }
    }

    return next_link_addr;
}

void
sys_block_init(struct sys_block_ctxt *ctxt, struct dc_clock *clk,
               struct Sh4 *sh4, struct Memory *main_memory, struct pvr2 *pvr2) {
    memset(ctxt, 0, sizeof(*ctxt));

    ctxt->sh4 = sh4;
    ctxt->main_memory = main_memory;
    ctxt->pvr2 = pvr2;
    ctxt->clk = clk;

    ctxt->sort_dma_complete_int_event.handler =
        sys_block_sort_dma_complete_int_event_handler;
    ctxt->sort_dma_complete_int_event.arg_ptr = ctxt;

    init_mmio_region_sys_block(&ctxt->mmio_region_sys_block, ctxt->reg_backing);

    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_C2DSTAT", 0x005f6800,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_C2DLEN", 0x005f6804,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_C2DST", 0x005f6808,
                                    sb_c2dst_mmio_read,
                                    sb_c2dst_mmio_write,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_SDSTAW", 0x5f6810,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_SDBAAW", 0x5f6814,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_SDWLT", 0x5f6818,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_SDLAS", 0x5f681c,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_SDST", 0x5f6820,
                                    sdst_reg_read_handler,
                                    sdst_reg_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_DBREQM", 0x5f6840,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_BAVLWC", 0x5f6844,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_C2DPRYC", 0x5f6848,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);
    /* TODO: spec says default val if SB_C2DMAXL is 1, but bios writes 0 ? */
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_C2DMAXL", 0x5f684c,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_TFREM", 0x5f6880,
                                    tfrem_reg_read_handler,
                                    mmio_region_sys_block_readonly_write_error,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_LMMODE0", 0x5f6884,
                                    lmmode0_reg_read_handler,
                                    lmmode0_reg_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_LMMODE1", 0x5f6888,
                                    lmmode1_reg_read_handler,
                                    lmmode1_reg_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_FFST", 0x5f688c,
                                    mmio_region_sys_block_silent_read_handler,
                                    mmio_region_sys_block_silent_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_SBREV", 0x5f689c,
                                    sys_sbrev_mmio_read,
                                    mmio_region_sys_block_readonly_write_error,
                                    ctxt);
    /* TODO: spec says default val if SB_RBSPLT's MSB is 0, but bios writes 1 */
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_RBSPLT", 0x5f68a0,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "UNKNOWN_REG_5f68a4", 0x5f68a4,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "UNKNOWN_REG_5f68ac", 0x5f68ac,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_IML2NRM", 0x5f6910,
                                    holly_reg_iml2nrm_mmio_read,
                                    holly_reg_iml2nrm_mmio_write,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_IML2EXT", 0x5f6914,
                                    holly_reg_iml2ext_mmio_read,
                                    holly_reg_iml2ext_mmio_write,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_IML2ERR", 0x5f6918,
                                    holly_reg_iml2err_mmio_read,
                                    holly_reg_iml2err_mmio_write,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_IML4NRM", 0x5f6920,
                                    holly_reg_iml4nrm_mmio_read,
                                    holly_reg_iml4nrm_mmio_write,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_IML4EXT", 0x5f6924,
                                    holly_reg_iml4ext_mmio_read,
                                    holly_reg_iml4ext_mmio_write,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_IML4ERR", 0x5f6928,
                                    holly_reg_iml4err_mmio_read,
                                    holly_reg_iml4err_mmio_write,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_IML6NRM", 0x5f6930,
                                    holly_reg_iml6nrm_mmio_read,
                                    holly_reg_iml6nrm_mmio_write,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_IML6EXT", 0x5f6934,
                                    holly_reg_iml6ext_mmio_read,
                                    holly_reg_iml6ext_mmio_write,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_IML6ERR", 0x5f6938,
                                    holly_reg_iml6err_mmio_read,
                                    holly_reg_iml6err_mmio_write,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_PDTNRM", 0x5f6940,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_PDTEXT", 0x5f6944,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);

    /* arguably these ones should go into their own hw/g2 subdirectory... */
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_G2DTNRM", 0x5f6950,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_G2DTEXT", 0x5f6954,
                                    mmio_region_sys_block_warn_read_handler,
                                    mmio_region_sys_block_warn_write_handler,
                                    ctxt);

    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_ISTNRM", 0x5f6900,
                                    holly_reg_istnrm_mmio_read,
                                    holly_reg_istnrm_mmio_write,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_ISTEXT", 0x5f6904,
                                    holly_reg_istext_mmio_read,
                                    holly_reg_istext_mmio_write,
                                    ctxt);
    mmio_region_sys_block_init_cell(&ctxt->mmio_region_sys_block,
                                    "SB_ISTERR", 0x5f6908,
                                    holly_reg_isterr_mmio_read,
                                    holly_reg_isterr_mmio_write,
                                    ctxt);
}

void sys_block_cleanup(struct sys_block_ctxt *ctxt) {
    cleanup_mmio_region_sys_block(&ctxt->mmio_region_sys_block);
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
