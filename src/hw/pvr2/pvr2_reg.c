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

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "mem_code.h"
#include "types.h"
#include "MemoryMap.h"
#include "log.h"
#include "error.h"

#include "pvr2_reg.h"

#define N_PVR2_REGS (ADDR_PVR2_LAST - ADDR_PVR2_FIRST + 1)
static uint32_t reg_backing[N_PVR2_REGS / sizeof(uint32_t)];

#define PVR2_SB_PDSTAP  0
#define PVR2_SB_PDSTAR  1
#define PVR2_SB_PDLEN   2
#define PVR2_SB_PDDIR   3
#define PVR2_SB_PDTSEL  4
#define PVR2_SB_PDEN    5
#define PVR2_SB_PDST    6
#define PVR2_SB_PDAPRO 32

#define PVR2_TRACE(msg, ...)                                            \
    do {                                                                \
        LOG_DBG("PVR2: ");                                              \
        LOG_DBG(msg, ##__VA_ARGS__);                                    \
    } while (0)

#define PVR2_REG_WRITE_CASE(idx_const)                 \
    case idx_const:                                    \
    PVR2_TRACE("Write 0x%08x to " #idx_const "\n",     \
               (unsigned)reg_backing[idx_const]);      \
    break

static void
pvr2_reg_post_write(unsigned idx) {
    switch (idx) {
        PVR2_REG_WRITE_CASE(PVR2_SB_PDSTAP);
        PVR2_REG_WRITE_CASE(PVR2_SB_PDSTAR);
        PVR2_REG_WRITE_CASE(PVR2_SB_PDLEN);
        PVR2_REG_WRITE_CASE(PVR2_SB_PDDIR);
        PVR2_REG_WRITE_CASE(PVR2_SB_PDTSEL);
        PVR2_REG_WRITE_CASE(PVR2_SB_PDEN);
        PVR2_REG_WRITE_CASE(PVR2_SB_PDST);
        PVR2_REG_WRITE_CASE(PVR2_SB_PDAPRO);
    default:
        error_set_index(idx);
        error_set_feature("writing to an unknown PVR2 register");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

#define PVR2_REG_READ_CASE(idx_const)                  \
    case idx_const:                                    \
    PVR2_TRACE("Read 0x%08x from " #idx_const "\n",     \
               (unsigned)reg_backing[idx_const]);      \
    break

static void
pvr2_reg_pre_read(unsigned idx) {
    switch (idx) {
        PVR2_REG_READ_CASE(PVR2_SB_PDSTAP);
        PVR2_REG_READ_CASE(PVR2_SB_PDSTAR);
        PVR2_REG_READ_CASE(PVR2_SB_PDLEN);
        PVR2_REG_READ_CASE(PVR2_SB_PDDIR);
        PVR2_REG_READ_CASE(PVR2_SB_PDTSEL);
        PVR2_REG_READ_CASE(PVR2_SB_PDEN);
        PVR2_REG_READ_CASE(PVR2_SB_PDST);
        PVR2_REG_READ_CASE(PVR2_SB_PDAPRO);
    default:
        error_set_index(idx);
        error_set_feature("reading from an unknown PVR2 register");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

void pvr2_reg_init(void) {
    memset(reg_backing, 0, sizeof(reg_backing));
}

void pvr2_reg_cleanup(void) {
}

#define PVR2_REG_READ_TMPL(tp)                                          \
    tp ret;                                                             \
    if (addr % sizeof(tp) || addr % sizeof(uint32_t)) {                 \
        error_set_feature("unaligned pvr2 register reads\n");           \
        error_set_address(addr);                                        \
        error_set_length(sizeof(tp));                                   \
        RAISE_ERROR(ERROR_UNIMPLEMENTED);                               \
    }                                                                   \
    unsigned offs = addr - ADDR_PVR2_FIRST;                             \
    unsigned idx = offs / sizeof(uint32_t);                             \
    pvr2_reg_pre_read(idx);                                             \
    memcpy(&ret, ((tp*)reg_backing) + offs / sizeof(tp), sizeof(tp));   \
    return ret

#define PVR2_REG_WRITE_TMPL(tp)                                         \
    if (addr % sizeof(tp) || addr % sizeof(uint32_t)) {                 \
        error_set_feature("unaligned pvr2 register writes\n");          \
        error_set_address(addr);                                        \
        error_set_length(sizeof(tp));                                   \
        RAISE_ERROR(ERROR_UNIMPLEMENTED);                               \
    }                                                                   \
    unsigned offs = addr - ADDR_PVR2_FIRST;                             \
    unsigned idx = offs / sizeof(uint32_t);                             \
    memcpy(((tp*)reg_backing) + offs / sizeof(tp), &val, sizeof(tp));   \
    pvr2_reg_post_write(idx)

double pvr2_reg_read_double(addr32_t addr, void *ctxt) {
    PVR2_REG_READ_TMPL(double);
}

void pvr2_reg_write_double(addr32_t addr, double val, void *ctxt) {
    PVR2_REG_WRITE_TMPL(double);
}

float pvr2_reg_read_float(addr32_t addr, void *ctxt) {
    PVR2_REG_READ_TMPL(float);
}

void pvr2_reg_write_float(addr32_t addr, float val, void *ctxt) {
    PVR2_REG_WRITE_TMPL(float);
}

uint32_t pvr2_reg_read_32(addr32_t addr, void *ctxt) {
    PVR2_REG_READ_TMPL(uint32_t);
}

void pvr2_reg_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    PVR2_REG_WRITE_TMPL(uint32_t);
}

uint16_t pvr2_reg_read_16(addr32_t addr, void *ctxt) {
    PVR2_REG_READ_TMPL(uint16_t);
}

void pvr2_reg_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    PVR2_REG_WRITE_TMPL(uint16_t);
}

uint8_t pvr2_reg_read_8(addr32_t addr, void *ctxt) {
    PVR2_REG_READ_TMPL(uint8_t);
}

void pvr2_reg_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    PVR2_REG_WRITE_TMPL(uint8_t);
}

struct memory_interface pvr2_reg_intf = {
    .read32 = pvr2_reg_read_32,
    .read16 = pvr2_reg_read_16,
    .read8 = pvr2_reg_read_8,
    .readfloat = pvr2_reg_read_float,
    .readdouble = pvr2_reg_read_double,

    .write32 = pvr2_reg_write_32,
    .write16 = pvr2_reg_write_16,
    .write8 = pvr2_reg_write_8,
    .writefloat = pvr2_reg_write_float,
    .writedouble = pvr2_reg_write_double
};
