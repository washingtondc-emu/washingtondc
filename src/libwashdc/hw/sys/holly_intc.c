/*******************************************************************************
 *
 * Copyright 2017, 2018, 2020 snickerbockers
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

#include "hw/sh4/sh4_excp.h"
#include "hw/sh4/sh4_read_inst.h"
#include "dreamcast.h"
#include "log.h"

#include "holly_intc.h"

static reg32_t reg_istnrm, reg_istext, reg_isterr;
static reg32_t reg_iml2nrm, reg_iml2ext, reg_iml2err;
static reg32_t reg_iml4nrm, reg_iml4ext, reg_iml4err;
static reg32_t reg_iml6nrm, reg_iml6ext, reg_iml6err;

struct holly_intp_info {
    char const *desc;
    reg32_t mask;
};

static struct holly_intp_info ext_intp_tbl[HOLLY_EXT_INT_COUNT] = {
    {
        "GD-ROM",
        HOLLY_REG_ISTEXT_GDROM_MASK
    },
    {
        "AICA",
        HOLLY_REG_ISTEXT_AICA_MASK
    }
};

static struct holly_intp_info nrm_intp_tbl[HOLLY_NRM_INT_COUNT] = {
    [HOLLY_NRM_INT_HBLANK] = {
        "H-BLANK",
        HOLLY_REG_ISTNRM_HBLANK_MASK
    },
    [HOLLY_NRM_INT_VBLANK_OUT] = {
        "V-BLANK OUT",
        HOLLY_REG_ISTNRM_VBLANK_OUT_MASK
    },
    [HOLLY_NRM_INT_VBLANK_IN] = {
        "V-BLANK IN",
        HOLLY_REG_ISTNRM_VBLANK_IN_MASK
    },
    [HOLLY_NRM_INT_ISTNRM_PVR_PUNCH_THROUGH_COMPLETE] = {
        "PUNCH-THROUGH POLYGON LIST COMPLETE",
        HOLLY_REG_ISTNRM_PVR_PUNCH_THROUGH_COMPLETE_MASK
    },
    [HOLLY_REG_ISTNRM_PVR_TRANS_MOD_COMPLETE] = {
        "TRANSPARENT POLYGON MODIFIER VOLUME LIST COMPLETE",
        HOLLY_REG_ISTNRM_PVR_TRANS_MOD_COMPLETE_MASK
    },
    [HOLLY_REG_ISTNRM_PVR_TRANS_COMPLETE] = {
        "TRANSPARENT POLYGON LIST COMPLETE",
        HOLLY_REG_ISTNRM_PVR_TRANS_COMPLETE_MASK,
    },
    [HOLLY_REG_ISTNRM_PVR_OPAQUE_MOD_COMPLETE] = {
        "OPAQUE POLYGON MODIFIER VOLUME LIST COMPLETE",
        HOLLY_REG_ISTNRM_PVR_OPAQUE_MOD_COMPLETE_MASK
    },
    [HOLLY_REG_ISTNRM_PVR_OPAQUE_COMPLETE] = {
        "OPAQUE POLYGON LIST COMPLETE",
        HOLLY_REG_ISTNRM_PVR_OPAQUE_COMPLETE_MASK
    },
    [HOLLY_REG_ISTNRM_PVR_RENDER_COMPLETE] = {
        "POWERVR2 RENDER COMPLETE",
        HOLLY_REG_ISTNRM_PVR_RENDER_COMPLETE_MASK
    },
    [HOLLY_MAPLE_ISTNRM_DMA_COMPLETE] = {
        "MAPLE DMA COMPLETE",
        HOLLY_REG_ISTNRM_MAPLE_DMA_COMPLETE_MASK
    },
    [HOLLY_REG_ISTNRM_CHANNEL2_DMA_COMPLETE] = {
        "CHANNEL-2 DMA COMPLETE",
        HOLLY_REG_ISTNRM_CHANNEL2_DMA_COMPLETE_MASK
    },
    [HOLLY_REG_ISTNRM_AICA_DMA_COMPLETE] = {
        "AICA DMA COMPLETE",
        HOLLY_REG_ISTNRM_AICA_DMA_COMPLETE_MASK
    },
    [HOLLY_REG_ISTNRM_PVR_YUV_COMPLETE] = {
        "PVR2 YUV CONVERSION COMPLETE",
        HOLLY_REG_ISTNRM_PVR_YUV_COMPLETE_MASK
    },
    [HOLLY_REG_ISTNRM_GDROM_DMA_COMPLETE] = {
        "GD-ROM DMA COMPLETE",
        HOLLY_REG_ISTNRM_GDROM_DMA_COMPLETE_MASK
    },
    [HOLLY_REG_ISTNRM_SORT_DMA_COMPLETE] = {
        "SORT DMA COMPLETE",
        HOLLY_REG_ISTNRM_SORT_DMA_COMPLETE_MASK
    }
};

void holly_raise_nrm_int(HollyNrmInt int_type) {
    reg32_t mask = nrm_intp_tbl[int_type].mask;

    reg_istnrm |= mask;

    sh4_refresh_intc(dreamcast_get_cpu());
}

void holly_clear_nrm_int(HollyNrmInt int_type) {
    reg32_t mask = nrm_intp_tbl[int_type].mask;
    reg_istnrm &= ~mask;
}

// TODO: what happens if another lower priority interrupt overwrites the IRL
// level before the higher priority interrupt has been cleared?
void holly_raise_ext_int(HollyExtInt int_type) {
    reg32_t mask = ext_intp_tbl[int_type].mask;

    reg_istext |= mask;

    sh4_refresh_intc(dreamcast_get_cpu());
}

void holly_clear_ext_int(HollyExtInt int_type) {
    reg32_t mask = ext_intp_tbl[int_type].mask;
    reg_istext &= ~mask;
}

int holly_intc_irl_line_fn(void *ctx) {
    if ((reg_iml6ext & reg_istext) || (reg_iml6nrm & reg_istnrm))
        return 9;
    else if ((reg_iml4ext & reg_istext) || (reg_iml4nrm & reg_istnrm))
        return 0xb;
    else if ((reg_iml2ext & reg_istext) || (reg_iml2nrm & reg_istnrm))
        return 0xd;
    else
        return 0xf;
}

uint32_t holly_reg_istnrm_mmio_read(struct mmio_region_sys_block *region,
                                    unsigned idx, void *ctxt) {
    reg32_t istnrm_out = reg_istnrm & 0x3fffff;

    istnrm_out |= (!!reg_istext) << 30;
    istnrm_out |= (!!reg_isterr) << 31;

    return istnrm_out;
}

void holly_reg_istnrm_mmio_write(struct mmio_region_sys_block *region,
                                 unsigned idx, uint32_t val, void *ctxt) {
    reg_istnrm &= ~val;
}

uint32_t holly_reg_istext_mmio_read(struct mmio_region_sys_block *region,
                                    unsigned idx, void *ctxt) {
    reg32_t istext_out = reg_istext & 0xf;

    LOG_DBG("Reading %X from ISTEXT\n", (unsigned)istext_out);

    return istext_out;
}

void holly_reg_istext_mmio_write(struct mmio_region_sys_block *region,
                                 unsigned idx, uint32_t val, void *ctxt) {
    /*
     * You can't write to this register from software, you have to make the
     * hardware clear it for you through other means.
     */
}

uint32_t holly_reg_isterr_mmio_read(struct mmio_region_sys_block *region,
                                    unsigned idx, void *ctxt) {
    return reg_isterr;
}

void holly_reg_isterr_mmio_write(struct mmio_region_sys_block *region,
                                 unsigned idx, uint32_t val, void *ctxt) {
    reg_isterr &= ~val;
}

uint32_t holly_reg_iml2nrm_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt) {
    return reg_iml2nrm;
}

void holly_reg_iml2nrm_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt) {
    reg_iml2nrm = val & 0x3fffff;
}

uint32_t holly_reg_iml2err_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt) {
    return reg_iml2err;
}

void holly_reg_iml2err_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt) {
    reg_iml2err = val;
}

uint32_t holly_reg_iml2ext_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt) {
    return reg_iml2ext;
}

void holly_reg_iml2ext_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt) {
    reg_iml2ext = val & 0xf;
}

uint32_t holly_reg_iml4nrm_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt) {
    return reg_iml4nrm;
}

void holly_reg_iml4nrm_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt) {
    reg_iml4nrm = val & 0x3fffff;
}

uint32_t holly_reg_iml4err_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt) {
    return reg_iml4err;
}

void holly_reg_iml4err_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt) {
    reg_iml4err = val;
}

uint32_t holly_reg_iml4ext_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt) {
    return reg_iml4ext;
}

void holly_reg_iml4ext_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt) {
    reg_iml4ext = val & 0xf;
}

uint32_t holly_reg_iml6nrm_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt) {
    return reg_iml6nrm;
}

void holly_reg_iml6nrm_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt) {
    reg_iml6nrm = val & 0x3fffff;
}

uint32_t holly_reg_iml6err_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt) {
    return reg_iml6err;
}

void holly_reg_iml6err_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt) {
    reg_iml6err = val;
}

uint32_t holly_reg_iml6ext_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt) {
    return reg_iml6ext;
}

void holly_reg_iml6ext_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt) {
    reg_iml6ext = val & 0xf;
}
