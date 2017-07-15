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

#include "hw/gdrom/gdrom_reg.h"
#include "hw/sh4/sh4_excp.h"
#include "dreamcast.h"

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
    }
};

void holly_raise_nrm_int(HollyNrmInt int_type) {
    reg32_t mask = nrm_intp_tbl[int_type].mask;

    reg_istnrm |= mask;

    if (reg_iml6nrm & mask)
        sh4_set_irl_interrupt(dreamcast_get_cpu(), 0x9);
    else if (reg_iml4nrm & mask)
        sh4_set_irl_interrupt(dreamcast_get_cpu(), 0xb);
    else if (reg_iml2nrm & mask)
        sh4_set_irl_interrupt(dreamcast_get_cpu(), 0xd);
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

    if (reg_iml6ext & mask)
        sh4_set_irl_interrupt(dreamcast_get_cpu(), 0x9);
    else if (reg_iml4ext & mask)
        sh4_set_irl_interrupt(dreamcast_get_cpu(), 0xb);
    else if (reg_iml2ext & mask)
        sh4_set_irl_interrupt(dreamcast_get_cpu(), 0xd);
}

void holly_clear_ext_int(HollyExtInt int_type) {
    reg32_t mask = ext_intp_tbl[int_type].mask;
    reg_istext &= ~mask;
}

int
holly_reg_istext_read_handler(struct sys_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len) {
    reg32_t istext_out = reg_istext & 0xf;

    memcpy(buf, &istext_out, sizeof(istext_out));

    printf("Reading %X from ISTEXT\n", (unsigned)istext_out);

    return 0;
}

int
holly_reg_istnrm_read_handler(struct sys_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len) {
    reg32_t istnrm_out = reg_istnrm & 0x3fffff;

    istnrm_out |= (!!reg_istext) << 30;
    istnrm_out |= (!!reg_isterr) << 31;

    memcpy(buf, &istnrm_out, sizeof(istnrm_out));

    return 0;
}

int
holly_reg_istnrm_write_handler(struct sys_mapped_reg const *reg_info,
                               void const *buf, addr32_t addr, unsigned len) {
    reg32_t in_val;

    memcpy(&in_val, buf, sizeof(in_val));
    reg_istnrm &= ~in_val;

    return 0;
}

int
holly_reg_istext_write_handler(struct sys_mapped_reg const *reg_info,
                               void const *buf, addr32_t addr, unsigned len) {
    /*
     * You can't write to this register from software, you have to make the
     * hardware clear it for you through other means.
     */
    return 0;
}

int
holly_reg_isterr_read_handler(struct sys_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &reg_isterr, sizeof(reg_isterr));

    return 0;
}

int
holly_reg_isterr_write_handler(struct sys_mapped_reg const *reg_info,
                               void const *buf, addr32_t addr, unsigned len) {
    reg32_t in_val;

    memcpy(&in_val, buf, sizeof(in_val));
    reg_isterr &= ~in_val;

    return 0;
}

int
holly_reg_iml2nrm_read_handler(struct sys_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &reg_iml2nrm, sizeof(reg_iml2nrm));

    return 0;
}

int
holly_reg_iml2nrm_write_handler(struct sys_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    memcpy(&reg_iml2nrm, buf, sizeof(reg_iml2nrm));
    reg_iml2nrm &= 0x3fffff;
    return 0;
}

int
holly_reg_iml2err_read_handler(struct sys_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &reg_iml2err, sizeof(reg_iml2err));

    return 0;
}


int
holly_reg_iml2err_write_handler(struct sys_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    memcpy(&reg_iml2err, buf, sizeof(reg_iml2err));
    return 0;
}

int
holly_reg_iml2ext_read_handler(struct sys_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &reg_iml2ext, sizeof(reg_iml2ext));

    return 0;
}

int
holly_reg_iml2ext_write_handler(struct sys_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    memcpy(&reg_iml2ext, buf, sizeof(reg_iml2ext));
    reg_iml2ext &= 0xf;
    return 0;
}

int
holly_reg_iml4nrm_read_handler(struct sys_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &reg_iml4nrm, sizeof(reg_iml4nrm));

    return 0;
}

int
holly_reg_iml4nrm_write_handler(struct sys_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    memcpy(&reg_iml4nrm, buf, sizeof(reg_iml4nrm));
    reg_iml4nrm &= 0x3fffff;
    return 0;
}

int
holly_reg_iml4err_read_handler(struct sys_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &reg_iml4err, sizeof(reg_iml4err));

    return 0;
}

int
holly_reg_iml4err_write_handler(struct sys_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    memcpy(&reg_iml4err, buf, sizeof(reg_iml4err));
    return 0;
}


int
holly_reg_iml4ext_read_handler(struct sys_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &reg_iml4ext, sizeof(reg_iml4ext));

    return 0;
}

int
holly_reg_iml4ext_write_handler(struct sys_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    memcpy(&reg_iml4ext, buf, sizeof(reg_iml4ext));
    reg_iml4ext &= 0xf;
    return 0;
}

int
holly_reg_iml6nrm_read_handler(struct sys_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &reg_iml6nrm, sizeof(reg_iml6nrm));

    return 0;
}

int
holly_reg_iml6nrm_write_handler(struct sys_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    memcpy(&reg_iml6nrm, buf, sizeof(reg_iml6nrm));
    reg_iml6nrm &= 0x3fffff;
    return 0;
}

int
holly_reg_iml6err_read_handler(struct sys_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &reg_iml6err, sizeof(reg_iml6err));

    return 0;
}

int
holly_reg_iml6err_write_handler(struct sys_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    memcpy(&reg_iml6err, buf, sizeof(reg_iml6err));
    return 0;
}

int
holly_reg_iml6ext_read_handler(struct sys_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &reg_iml6ext, sizeof(reg_iml6ext));

    return 0;
}

int
holly_reg_iml6ext_write_handler(struct sys_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    memcpy(&reg_iml6ext, buf, sizeof(reg_iml6ext));
    reg_iml6ext &= 0xf;
    return 0;
}
