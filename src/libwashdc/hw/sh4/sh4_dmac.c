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

#include <stdio.h>
#include <string.h>

#include "washdc/error.h"

#include "sh4.h"
#include "sh4_reg.h"
#include "sh4_reg_flags.h"
#include "washdc/MemoryMap.h"
#include "sh4_dmac.h"
#include "mem_areas.h"
#include "hw/sys/holly_intc.h"
#include "log.h"
#include "dc_sched.h"
#include "dreamcast.h"
#include "sh4_read_inst.h"

static void raise_ch2_dma_int_event_handler(struct SchedEvent *event);

struct SchedEvent raise_ch2_dma_int_event = {
    .handler = raise_ch2_dma_int_event_handler
};

static bool ch2_dma_scheduled;

static int sh4_dmac_irq_line(Sh4ExceptionCode *code, void *ctx);

void sh4_dmac_init(Sh4 *sh4) {
    sh4_register_irq_line(sh4, SH4_IRQ_DMAC, sh4_dmac_irq_line, sh4);
}

void sh4_dmac_cleanup(Sh4 *sh4) {
    sh4_register_irq_line(sh4, SH4_IRQ_DMAC, NULL, NULL);
}

sh4_reg_val
sh4_dmac_sar_reg_read_handler(Sh4 *sh4,
                              struct Sh4MemMappedReg const *reg_info) {
    int chan;
    unsigned reg_idx = reg_info->reg_idx;
    switch (reg_idx) {
        /* case SH4_REG_SAR0: */
        /*     chan = 0; */
        /*     break; */
    case SH4_REG_SAR1:
        chan = 1;
        break;
    case SH4_REG_SAR2:
        chan = 2;
        break;
    case SH4_REG_SAR3:
        chan = 3;
        break;
    default:
        RAISE_ERROR(ERROR_INVALID_PARAM);
    }

    LOG_DBG("reading %08x from SH4 DMAC SAR%d register\n",
            (unsigned)sh4->dmac.sar[chan], chan);

    return sh4->dmac.sar[chan];
}

void sh4_dmac_sar_reg_write_handler(Sh4 *sh4,
                                    struct Sh4MemMappedReg const *reg_info,
                                    sh4_reg_val val) {
    int chan;
    switch (reg_info->reg_idx) {
        /* case SH4_REG_SAR0: */
        /*     chan = 0; */
        /*     break; */
    case SH4_REG_SAR1:
        chan = 1;
        break;
    case SH4_REG_SAR2:
        chan = 2;
        break;
    case SH4_REG_SAR3:
        chan = 3;
        break;
    default:
        RAISE_ERROR(ERROR_INVALID_PARAM);
    }

    sh4->dmac.sar[chan] = val;

    LOG_DBG("writing %08x to SH4 DMAC SAR%d register\n",
            (unsigned)sh4->dmac.sar[chan], chan);
}

sh4_reg_val
sh4_dmac_dar_reg_read_handler(Sh4 *sh4,
                              struct Sh4MemMappedReg const *reg_info) {
    int chan;
    switch (reg_info->reg_idx) {
        /* case SH4_REG_DAR0: */
        /*     chan = 0; */
        /*     break; */
    case SH4_REG_DAR1:
        chan = 1;
        break;
    case SH4_REG_DAR2:
        chan = 2;
        break;
    case SH4_REG_DAR3:
        chan = 3;
        break;
    default:
        RAISE_ERROR(ERROR_INVALID_PARAM);
    }

    LOG_DBG("reading %08x from SH4 DMAC DAR%d register\n",
            (unsigned)sh4->dmac.dar[chan], chan);

    return sh4->dmac.dar[chan];
}

void sh4_dmac_dar_reg_write_handler(Sh4 *sh4,
                                    struct Sh4MemMappedReg const *reg_info,
                                    sh4_reg_val val) {
    int chan;
    switch (reg_info->reg_idx) {
        /* case SH4_REG_DAR0: */
        /*     chan = 0; */
        /*     break; */
    case SH4_REG_DAR1:
        chan = 1;
        break;
    case SH4_REG_DAR2:
        chan = 2;
        break;
    case SH4_REG_DAR3:
        chan = 3;
        break;
    default:
        RAISE_ERROR(ERROR_INVALID_PARAM);
    }

    sh4->dmac.dar[chan] = val;

    LOG_DBG("writing to SH4 DMAC DAR%d register\n", chan);
}

sh4_reg_val sh4_dmac_dmatcr_reg_read_handler(Sh4 *sh4,
                                             struct Sh4MemMappedReg const *reg_info) {
    int chan;
    switch (reg_info->reg_idx) {
        /* case SH4_REG_DMATCR0: */
        /*     chan = 0; */
        /*     break; */
    case SH4_REG_DMATCR1:
        chan = 1;
        break;
    case SH4_REG_DMATCR2:
        chan = 2;
        break;
    case SH4_REG_DMATCR3:
        chan = 3;
        break;
    default:
        RAISE_ERROR(ERROR_INVALID_PARAM);
    }

    LOG_DBG("reading %08x from SH4 DMAC DMATCR%d register\n",
            (unsigned)sh4->dmac.dmatcr[chan], chan);

    return sh4->dmac.dmatcr[chan];
}

void sh4_dmac_dmatcr_reg_write_handler(Sh4 *sh4,
                                       struct Sh4MemMappedReg const *reg_info,
                                       sh4_reg_val val) {
    int chan;
    switch (reg_info->reg_idx) {
        /* case SH4_REG_DMATCR0: */
        /*     chan = 0; */
        /*     break; */
    case SH4_REG_DMATCR1:
        chan = 1;
        break;
    case SH4_REG_DMATCR2:
        chan = 2;
        break;
    case SH4_REG_DMATCR3:
        chan = 3;
        break;
    default:
        RAISE_ERROR(ERROR_INVALID_PARAM);
    }

    sh4->dmac.dmatcr[chan] = val;

    LOG_DBG("writing %08x to SH4 DMAC DMATCR%d register\n",
            (unsigned)sh4->dmac.dmatcr[chan], chan);
}

sh4_reg_val
sh4_dmac_chcr_reg_read_handler(Sh4 *sh4,
                               struct Sh4MemMappedReg const *reg_info) {
    int chan;
    switch (reg_info->reg_idx) {
        /* case SH4_REG_CHCR0: */
        /*     chan = 0; */
        /*     break; */
    case SH4_REG_CHCR1:
        chan = 1;
        break;
    case SH4_REG_CHCR2:
        chan = 2;
        break;
    case SH4_REG_CHCR3:
        chan = 3;
        break;
    default:
        RAISE_ERROR(ERROR_INVALID_PARAM);
    }

    sh4_reg_val ret = sh4->dmac.chcr[chan];

    if (ret & SH4_DMAC_CHCR_TE_MASK)
        sh4->dmac.dma_ack[chan] = true;

    /*
     * TODO: I can't print here because KallistiOS programs seem to be
     * constantly accessing CHCR3, and the printf statememnts end up causing a
     * huge performance drop.  I need to investigate further to determine if
     * this is a result of a bug in WashingtonDC, or if KallistiOS is actually
     * supposed to be doing this.
     */
    /* printf("WARNING: reading %08x from SH4 DMAC CHCR%d register\n", */
    /*        (unsigned)sh4->dmac.chcr[chan], chan); */

    return ret;
}

void sh4_dmac_chcr_reg_write_handler(Sh4 *sh4,
                                     struct Sh4MemMappedReg const *reg_info,
                                     sh4_reg_val val) {
    int chan;
    switch (reg_info->reg_idx) {
        /* case SH4_REG_CHCR0: */
        /*     chan = 0; */
        /*     break; */
    case SH4_REG_CHCR1:
        chan = 1;
        break;
    case SH4_REG_CHCR2:
        chan = 2;
        break;
    case SH4_REG_CHCR3:
        chan = 3;
        break;
    default:
        RAISE_ERROR(ERROR_INVALID_PARAM);
    }

    sh4_reg_val cur = sh4->dmac.chcr[chan];

    if (val & SH4_DMAC_CHCR_TE_MASK) {
        // only let them write to TE if it's already set
        if (!(cur & SH4_DMAC_CHCR_TE_MASK))
            val &= ~SH4_DMAC_CHCR_TE_MASK;
    } else {
        if (cur & SH4_DMAC_CHCR_TE_MASK) {
            // user might be trying to clear the bit
            if (sh4->dmac.dma_ack[chan]) {
                // let them do it
                sh4->dmac.dma_ack[chan] = false;
            } else {
                // don't let them do it.
                val |= SH4_DMAC_CHCR_TE_MASK;
            }
        }
    }

    sh4->dmac.chcr[chan] = val;

    /*
     * TODO: I can't print here because KallistiOS programs seem to be
     * constantly accessing CHCR3, and the printf statememnts end up causing a
     * huge performance drop.  I need to investigate further to determine if
     * this is a result of a bug in WashingtonDC, or if KallistiOS is actually
     * supposed to be doing this.
     */
    /* printf("WARNING: writing %08x to SH4 DMAC CHCR%d register\n", */
    /*        (unsigned)sh4->dmac.chcr[chan], chan); */
}

sh4_reg_val
sh4_dmac_dmaor_reg_read_handler(Sh4 *sh4,
                                struct Sh4MemMappedReg const *reg_info) {
    LOG_DBG("reading %08x from SH4 DMAC DMAOR register\n",
            (unsigned)sh4->dmac.dmaor);

    return sh4->dmac.dmaor;
}

void sh4_dmac_dmaor_reg_write_handler(Sh4 *sh4,
                                      struct Sh4MemMappedReg const *reg_info,
                                      sh4_reg_val val) {
    sh4->dmac.dmaor = val;

    LOG_DBG("writing %08x to SH4 DMAC DMAOR register\n",
            (unsigned)sh4->dmac.dmaor);
}

void sh4_dmac_transfer_to_mem(Sh4 *sh4, addr32_t transfer_dst, size_t unit_sz,
                              size_t n_units, void const *dat) {
    size_t total_len = unit_sz * n_units;
    struct memory_map_region *region =
        memory_map_get_region(sh4->mem.map,
                              transfer_dst & ~0xe0000000, total_len);
    uint32_t addr_mask = region->mask;
    void *ctx = region->ctxt;

    if (total_len % 4 == 0) {
        memory_map_write32_func write32 = region->intf->write32;
        total_len /= 4;
        uint32_t const *dat32 = (uint32_t const*)dat;
        while (total_len) {
            write32(transfer_dst & addr_mask, *dat32, ctx);
            transfer_dst += 4;
            dat32++;
            total_len--;
        }
    } else if (total_len % 2 == 0) {
        memory_map_write16_func write16 = region->intf->write16;
        total_len /= 2;
        uint16_t const *dat16 = (uint16_t const*)dat;
        while (total_len) {
            write16(transfer_dst & addr_mask, *dat16, ctx);
            transfer_dst += 2;
            dat16++;
            total_len--;
        }
    } else {
        memory_map_write8_func write8 = region->intf->write8;
        uint8_t const *dat8 = (uint8_t const*)dat;
        while (total_len) {
            write8(transfer_dst & addr_mask, *dat8, ctx);
            transfer_dst++;
            dat8++;
            total_len--;
        }
    }
}

void sh4_dmac_transfer_from_mem(Sh4 *sh4, addr32_t transfer_src, size_t unit_sz,
                                size_t n_units, void *dat) {
    size_t total_len = unit_sz * n_units;
    if (total_len % 4 == 0) {
        total_len /= 4;
        uint32_t *dat32 = (uint32_t*)dat;
        while (total_len) {
            *dat32 = memory_map_read_32(sh4->mem.map,
                                        transfer_src & ~0xe0000000);
            dat32++;
            total_len--;
            transfer_src += 4;
        }
    } else if (total_len % 2 == 0) {
        total_len /= 2;
        uint16_t *dat16 = (uint16_t*)dat;
        while (total_len) {
            *dat16 = memory_map_read_16(sh4->mem.map,
                                        transfer_src & ~0xe0000000);
            dat16++;
            total_len--;
            transfer_src += 2;
        }
    } else {
        uint8_t *dat8 = (uint8_t*)dat;
        while (total_len) {
            *dat8 = memory_map_read_8(sh4->mem.map,
                                      transfer_src & ~0xe0000000);
            dat8++;
            total_len--;
            transfer_src++;
        }
    }
}

static DEF_ERROR_U32_ATTR(dma_xfer_src)
static DEF_ERROR_U32_ATTR(dma_xfer_dst)

void sh4_dmac_transfer_words(Sh4 *sh4, addr32_t transfer_src,
                             addr32_t transfer_dst, size_t n_words) {
    struct memory_map *map = sh4->mem.map;
    size_t counter;

    struct memory_map_region *src_region = memory_map_get_region(map, transfer_src,
                                                                 n_words * sizeof(uint32_t));
    struct memory_map_region *dst_region = memory_map_get_region(map, transfer_dst,
                                                                 n_words * sizeof(uint32_t));

    if (!src_region || !dst_region) {
        error_set_dma_xfer_src(transfer_src);
        error_set_dma_xfer_dst(transfer_dst);
        error_set_length(n_words * sizeof(uint32_t));
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    memory_map_read32_func read32 = src_region->intf->read32;
    memory_map_write32_func write32 = dst_region->intf->write32;
    uint32_t src_mask = src_region->mask;
    uint32_t dst_mask = dst_region->mask;
    void *src_ctx = src_region->ctxt;
    void *dst_ctx = dst_region->ctxt;

    for (counter = 0; counter < n_words; counter++) {
        CHECK_R_WATCHPOINT(transfer_src, uint32_t);
        CHECK_W_WATCHPOINT(transfer_dst, uint32_t);

        uint32_t word = read32(transfer_src & src_mask, src_ctx);
        write32(transfer_dst & dst_mask, word, dst_ctx);

        transfer_src += sizeof(uint32_t);
        transfer_dst += sizeof(uint32_t);
    }
}

void sh4_dmac_channel2(Sh4 *sh4, addr32_t transfer_dst, unsigned n_bytes) {
    /*
     * TODO: check DMAOR to make sure DMA is enabled.  Maybe check a few other
     * registers as well (I think CHCR2 has a per-channel enable bit for this?)
     */

    unsigned xfer_unit;
    switch ((sh4->dmac.chcr[2] >> 4) & 7) {
    case 0:
        xfer_unit = 8;
        break;
    case 1:
        xfer_unit = 1;
        break;
    case 2:
        xfer_unit = 2;
        break;
    case 3:
        xfer_unit = 4;
        break;
    case 4:
        xfer_unit = 32;
        break;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    if (xfer_unit != 32) {
        /*
         * It seems a real dreamcast will not allow for transfers which are not
         * done in 32-byte increments.  Whenever I try it in one of my hardware
         * tests, the system freezes.  Maybe there's an exception that should
         * be raised, IDK.  All I know is that you can't do this on a real
         * Dreamcast.
         */
        error_set_feature("The App requested a DMA tranfer in units other than "
                          "32-bytes");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    if (n_bytes != (xfer_unit * sh4->dmac.dmatcr[2])) {
        error_set_feature("whatever happens when there's a channel-2 DMA "
                          "length mismatch");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    /*
     * n_bytes has already been established to be divisible by 32,
     * so it must also be divisible by 4
     */
    unsigned n_words = n_bytes / 4;

    addr32_t transfer_src = sh4->dmac.sar[2];

    if (transfer_src % xfer_unit) {
        /*
         * transfers must be properly aligned.
         * if you don't do this, it won't work on a real dreamcast.  Might as
         * well raise an error and crash the emulator.
         */
        error_set_feature("non-aligned CH2 DMA transfer source address");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    if (transfer_dst % xfer_unit) {
        /*
         * transfers must be properly aligned.
         * if you don't do this, it won't work on a real dreamcast.  Might as
         * well raise an error and crash the emulator.
         */
        error_set_feature("non-aligned CH2 DMA transfer destination address");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    LOG_DBG("SH4 - initiating %u-byte DMA transfer from 0x%08x to "
            "0x%08x\n", n_bytes, transfer_src, transfer_dst);

    sh4->dmac.sar_pending[2] = transfer_src + n_bytes;

    /*
     * TODO: replace this function call with a hook of some sort so that other
     * platforms can have different behavior.  Alternatively, use the
     * memory_map.
     */
    dc_cycle_stamp_t n_cycles =
        dc_ch2_dma_xfer(transfer_src, transfer_dst, n_words);

    ch2_dma_scheduled = true;

    /*
     * the n_cycles delay was returned from dc_ch2_dma_xfer so that it could
     * be different for different dma destinations.
     */
    raise_ch2_dma_int_event.when = clock_cycle_stamp(sh4->clk) + n_cycles;
    raise_ch2_dma_int_event.arg_ptr = sh4;
    sched_event(sh4->clk, &raise_ch2_dma_int_event);
}

static void raise_ch2_dma_int_event_handler(struct SchedEvent *event) {
    Sh4 *sh4 = event->arg_ptr;

    /*
     * TODO: i think ideally these registers should continually update during
     * the transfer.
     */
    sh4->dmac.dmatcr[2] = 0;
    sh4->dmac.sar[2] = sh4->dmac.sar_pending[2];

    // raise the interrupt
    sh4->dmac.chcr[2] |= SH4_DMAC_CHCR_TE_MASK;
    sh4->dmac.dma_ack[2] = false;
    sh4_refresh_intc(sh4);

    ch2_dma_scheduled = false;
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_CHANNEL2_DMA_COMPLETE);
}

static int sh4_dmac_irq_line(Sh4ExceptionCode *code, void *ctx) {
    Sh4 *sh4 = (Sh4*)ctx;
    if (sh4->dmac.chcr[2] & SH4_DMAC_CHCR_TE_MASK) {
        *code = SH4_EXCP_DMAC_DMTE2;
        return 1;
    }
    return 0;
}
