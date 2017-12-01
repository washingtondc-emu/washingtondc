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

#include <stdio.h>
#include <string.h>

#include "error.h"

#include "sh4.h"
#include "sh4_reg.h"
#include "sh4_reg_flags.h"
#include "MemoryMap.h"
#include "sh4_dmac.h"
#include "mem_areas.h"
#include "hw/pvr2/pvr2_ta.h"
#include "hw/pvr2/pvr2_tex_mem.h"
#include "hw/sys/holly_intc.h"
#include "log.h"

int sh4_dmac_sar_reg_read_handler(Sh4 *sh4, void *buf,
                                  struct Sh4MemMappedReg const *reg_info) {
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

    LOG_DBG("reading %08x from SH4 DMAC SAR%d register\n",
            (unsigned)sh4->dmac.sar[chan], chan);

    memcpy(buf, &sh4->dmac.sar[chan], sizeof(sh4->dmac.sar[chan]));
    return 0;
}

int sh4_dmac_sar_reg_write_handler(Sh4 *sh4, void const *buf,
                                   struct Sh4MemMappedReg const *reg_info) {
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

    memcpy(&sh4->dmac.sar[chan], buf, sizeof(sh4->dmac.sar[chan]));

    LOG_DBG("writing %08x to SH4 DMAC SAR%d register\n",
            (unsigned)sh4->dmac.sar[chan], chan);

    return 0;
}

int sh4_dmac_dar_reg_read_handler(Sh4 *sh4, void *buf,
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

    memcpy(buf, &sh4->dmac.dar[chan], sizeof(sh4->dmac.dar[chan]));

    LOG_DBG("reading %08x from SH4 DMAC DAR%d register\n",
            (unsigned)sh4->dmac.dar[chan], chan);

    return 0;
}

int sh4_dmac_dar_reg_write_handler(Sh4 *sh4, void const *buf,
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

    memcpy(&sh4->dmac.dar[chan], buf, sizeof(sh4->dmac.dar[chan]));

    LOG_DBG("writing to SH4 DMAC DAR%d register\n", chan);

    return 0;
}

int sh4_dmac_dmatcr_reg_read_handler(Sh4 *sh4, void *buf,
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

    memcpy(buf, &sh4->dmac.dmatcr[chan], sizeof(sh4->dmac.dmatcr[chan]));

    LOG_DBG("reading %08x from SH4 DMAC DMATCR%d register\n",
            (unsigned)sh4->dmac.dmatcr[chan], chan);

    return 0;
}

int sh4_dmac_dmatcr_reg_write_handler(Sh4 *sh4, void const *buf,
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

    memcpy(&sh4->dmac.dmatcr[chan], buf, sizeof(sh4->dmac.dmatcr[chan]));

    LOG_DBG("writing %08x to SH4 DMAC DMATCR%d register\n",
            (unsigned)sh4->dmac.dmatcr[chan], chan);

    return 0;
}

int sh4_dmac_chcr_reg_read_handler(Sh4 *sh4, void *buf,
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

    memcpy(buf, &sh4->dmac.chcr[chan], sizeof(sh4->dmac.chcr[chan]));

    /*
     * TODO: I can't print here because KallistiOS programs seem to be
     * constantly accessing CHCR3, and the printf statememnts end up causing a
     * huge performance drop.  I need to investigate further to determine if
     * this is a result of a bug in WashingtonDC, or if KallistiOS is actually
     * supposed to be doing this.
     */
    /* printf("WARNING: reading %08x from SH4 DMAC CHCR%d register\n", */
    /*        (unsigned)sh4->dmac.chcr[chan], chan); */

    return 0;
}

int sh4_dmac_chcr_reg_write_handler(Sh4 *sh4, void const *buf,
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

    memcpy(&sh4->dmac.chcr[chan], buf, sizeof(sh4->dmac.chcr[chan]));

    /*
     * TODO: I can't print here because KallistiOS programs seem to be
     * constantly accessing CHCR3, and the printf statememnts end up causing a
     * huge performance drop.  I need to investigate further to determine if
     * this is a result of a bug in WashingtonDC, or if KallistiOS is actually
     * supposed to be doing this.
     */
    /* printf("WARNING: writing %08x to SH4 DMAC CHCR%d register\n", */
    /*        (unsigned)sh4->dmac.chcr[chan], chan); */

    return 0;
}

int sh4_dmac_dmaor_reg_read_handler(Sh4 *sh4, void *buf,
                                    struct Sh4MemMappedReg const *reg_info) {
    memcpy(buf, &sh4->dmac.dmaor, sizeof(sh4->dmac.dmaor));

    LOG_DBG("reading %08x from SH4 DMAC DMAOR register\n",
            (unsigned)sh4->dmac.dmaor);

    return 0;
}

int sh4_dmac_dmaor_reg_write_handler(Sh4 *sh4, void const *buf,
                                     struct Sh4MemMappedReg const *reg_info) {
    memcpy(&sh4->dmac.dmaor, buf, sizeof(sh4->dmac.dmaor));

    LOG_DBG("writing %08x to SH4 DMAC DMAOR register\n",
            (unsigned)sh4->dmac.dmaor);

    return 0;
}

void sh4_dmac_transfer_to_mem(addr32_t transfer_dst, size_t unit_sz,
                              size_t n_units, void const *dat) {
    if (memory_map_write(dat, transfer_dst & ~0xe0000000, unit_sz * n_units) !=
        MEM_ACCESS_SUCCESS) {
        RAISE_ERROR(get_error_pending());
    }
}

void sh4_dmac_transfer_from_mem(addr32_t transfer_src, size_t unit_sz,
                                size_t n_units, void *dat) {
    if (memory_map_read(dat, transfer_src & ~0xe0000000, unit_sz * n_units) !=
        MEM_ACCESS_SUCCESS) {
        RAISE_ERROR(get_error_pending());
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

    if ((transfer_dst >= ADDR_TA_FIFO_POLY_FIRST) &&
        (transfer_dst <= ADDR_TA_FIFO_POLY_LAST)) {
        while (n_words--) {
            uint32_t buf;
            if (sh4_do_read_mem(sh4, &buf, transfer_src, sizeof(buf))
                != MEM_ACCESS_SUCCESS) {
                RAISE_ERROR(get_error_pending());
            }

            pvr2_ta_fifo_poly_write(&buf, transfer_dst, sizeof(buf));
            transfer_dst += sizeof(buf);
            transfer_src += sizeof(buf);
        }
    } else if ((transfer_dst >= ADDR_AREA4_TEX64_FIRST) &&
               (transfer_dst <= ADDR_AREA4_TEX64_LAST)) {
        // TODO: do tex DMA transfers in large chuks instead of 4-byte increments
        transfer_dst = transfer_dst - ADDR_AREA4_TEX64_FIRST + ADDR_TEX64_FIRST;

        while (n_words--) {
            uint32_t buf;
            if (sh4_do_read_mem(sh4, &buf, transfer_src, sizeof(buf))
                != MEM_ACCESS_SUCCESS) {
                RAISE_ERROR(get_error_pending());
            }

            if (pvr2_tex_mem_area64_write(&buf, transfer_dst, sizeof(buf)) !=
                MEM_ACCESS_SUCCESS) {
                RAISE_ERROR(get_error_pending());
            }
            transfer_dst += sizeof(buf);
            transfer_src += sizeof(buf);
        }
    } else if ((transfer_dst >= ADDR_AREA4_TEX32_FIRST) &&
               (transfer_dst <= ADDR_AREA4_TEX32_LAST)) {
        // TODO: do tex DMA transfers in large chuks instead of 4-byte increments
        transfer_dst = transfer_dst - ADDR_AREA4_TEX32_FIRST + ADDR_TEX32_FIRST;

        while (n_words--) {
            uint32_t buf;
            if (sh4_do_read_mem(sh4, &buf, transfer_src, sizeof(buf))
                != MEM_ACCESS_SUCCESS) {
                RAISE_ERROR(get_error_pending());
            }

            if (pvr2_tex_mem_area32_write(&buf, transfer_dst, sizeof(buf)) !=
                MEM_ACCESS_SUCCESS) {
                RAISE_ERROR(get_error_pending());
            }
            transfer_dst += sizeof(buf);
            transfer_src += sizeof(buf);
        }
    } else {
        error_set_address(transfer_dst);
        error_set_length(n_bytes);
        error_set_feature("channel-2 DMA transfers to an unknown destination");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    // raise the interrupt
    sh4->dmac.chcr[2] |= SH4_DMAC_CHCR_TE_MASK;
    sh4_set_interrupt(sh4, SH4_IRQ_DMAC, SH4_EXCP_DMAC_DMTE2);

    holly_raise_nrm_int(HOLLY_REG_ISTNRM_CHANNEL2_DMA_COMPLETE);
}
