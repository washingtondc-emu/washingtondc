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
#include "MemoryMap.h"
#include "sh4_dmac.h"

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

    printf("WARNING: reading %08x from SH4 DMAC SAR%d register\n",
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

    printf("WARNING: writing %08x to SH4 DMAC SAR%d register\n",
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

    printf("WARNING: reading %08x from SH4 DMAC DAR%d register\n",
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

    printf("WARNING: writing to SH4 DMAC DAR%d register\n", chan);

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

    memcpy(buf, &sh4->dmac.dmatcr[chan], sizeof(&sh4->dmac.dmatcr[chan]));

    printf("WARNING: reading %08x from SH4 DMAC DMATCR%d register\n",
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

    memcpy(&sh4->dmac.dmatcr[chan], buf, sizeof(&sh4->dmac.dmatcr[chan]));

    printf("WARNING: writing %08x to SH4 DMAC DMATCR%d register\n",
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

    printf("WARNING: reading %08x from SH4 DMAC CHCR%d register\n",
           (unsigned)sh4->dmac.chcr[chan], chan);

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

    printf("WARNING: writing %08x to SH4 DMAC CHCR%d register\n",
           (unsigned)sh4->dmac.chcr[chan], chan);

    return 0;
}

int sh4_dmac_dmaor_reg_read_handler(Sh4 *sh4, void *buf,
                                    struct Sh4MemMappedReg const *reg_info) {
    memcpy(buf, &sh4->dmac.dmaor, sizeof(sh4->dmac.dmaor));

    printf("WARNING: reading %08x from SH4 DMAC DMAOR register\n",
           (unsigned)sh4->dmac.dmaor);

    return 0;
}

int sh4_dmac_dmaor_reg_write_handler(Sh4 *sh4, void const *buf,
                                     struct Sh4MemMappedReg const *reg_info) {
    memcpy(&sh4->dmac.dmaor, buf, sizeof(sh4->dmac.dmaor));

    printf("WARNING: writing %08x to SH4 DMAC DMAOR register\n",
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
