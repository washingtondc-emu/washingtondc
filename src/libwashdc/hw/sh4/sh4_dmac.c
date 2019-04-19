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

static void raise_ch2_dma_int_event_handler(struct SchedEvent *event);

// this is arbitrary
#define CH2_DMA_INT_DELAY 0

struct SchedEvent raise_ch2_dma_int_event = {
    .handler = raise_ch2_dma_int_event_handler
};

static bool ch2_dma_scheduled;

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

    /*
     * TODO: I can't print here because KallistiOS programs seem to be
     * constantly accessing CHCR3, and the printf statememnts end up causing a
     * huge performance drop.  I need to investigate further to determine if
     * this is a result of a bug in WashingtonDC, or if KallistiOS is actually
     * supposed to be doing this.
     */
    /* printf("WARNING: reading %08x from SH4 DMAC CHCR%d register\n", */
    /*        (unsigned)sh4->dmac.chcr[chan], chan); */

    return sh4->dmac.chcr[chan];
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
    if (total_len % 4 == 0) {
        total_len /= 4;
        uint32_t const *dat32 = (uint32_t const*)dat;
        while (total_len) {
            memory_map_write_32(sh4->mem.map,
                                transfer_dst & ~0xe0000000, *dat32);
            transfer_dst += 4;
            dat32++;
            total_len--;
        }
    } else if (total_len % 2 == 0) {
        total_len /= 2;
        uint16_t const *dat16 = (uint16_t const*)dat;
        while (total_len) {
            memory_map_write_16(sh4->mem.map,
                                transfer_dst & ~0xe0000000, *dat16);
            transfer_dst += 2;
            dat16++;
            total_len--;
        }
    } else {
        uint8_t const *dat8 = (uint8_t const*)dat;
        while (total_len) {
            memory_map_write_8(sh4->mem.map,
                               transfer_dst & ~0xe0000000, *dat8);
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

void sh4_dmac_transfer(Sh4 *sh4, addr32_t transfer_src,
                       addr32_t transfer_dst, size_t n_bytes) {
    struct memory_map *map = sh4->mem.map;
    size_t counter;
    for (counter = 0; counter < n_bytes; counter++) {
        uint8_t byte = memory_map_read_8(map, transfer_src++);
        memory_map_write_8(map, transfer_dst++, byte);
    }
}

void sh4_dmac_channel2(Sh4 *sh4, addr32_t transfer_dst, unsigned n_bytes) {
    /*
     * TODO: check DMAOR to make sure DMA is enabled.  Maybe check a few other
     * registers as well (I think CHCR2 has a per-channel enable bit for this?)
     */

    if (n_bytes != (32 * sh4->dmac.dmatcr[2])) {
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

    LOG_DBG("SH4 - initiating %u-byte DMA transfer from 0x%08x to "
            "0x%08x\n", n_bytes, transfer_src, transfer_dst);

    /*
     * TODO: replace this function call with a hook of some sort so that other
     * platforms can have different behavior.  Alternatively, use the
     * memory_map.
     */
    dc_ch2_dma_xfer(transfer_src, transfer_dst, n_words);

    ch2_dma_scheduled = true;
    raise_ch2_dma_int_event.when = clock_cycle_stamp(sh4->clk) + CH2_DMA_INT_DELAY;
    raise_ch2_dma_int_event.arg_ptr = sh4;
    sched_event(sh4->clk, &raise_ch2_dma_int_event);
}

static void raise_ch2_dma_int_event_handler(struct SchedEvent *event) {
    Sh4 *sh4 = event->arg_ptr;

    // raise the interrupt
    sh4->dmac.chcr[2] |= SH4_DMAC_CHCR_TE_MASK;
    sh4_set_interrupt(sh4, SH4_IRQ_DMAC, SH4_EXCP_DMAC_DMTE2);

    ch2_dma_scheduled = false;
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_CHANNEL2_DMA_COMPLETE);
}
