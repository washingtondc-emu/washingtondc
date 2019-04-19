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

#include "mem_code.h"
#include "washdc/MemoryMap.h"
#include "washdc/error.h"
#include "dreamcast.h"
#include "hw/sh4/sh4.h"
#include "config.h"
#include "log.h"

#include "aica_wave_mem.h"

bool aica_log_verbose_val;

void aica_log_verbose(bool verbose) {
    aica_log_verbose_val = verbose;
}

void aica_wave_mem_init(struct aica_wave_mem *wm) {
    memset(wm->mem, 0, sizeof(wm->mem));
}

void aica_wave_mem_cleanup(struct aica_wave_mem *wm) {
}

float aica_wave_mem_read_float(addr32_t addr, void *ctxt) {
    uint32_t val = aica_wave_mem_read_32(addr, ctxt);
    float ret;
    memcpy(&ret, &val, sizeof(ret));
    return ret;
}

void aica_wave_mem_write_float(addr32_t addr, float val, void *ctxt) {
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    aica_wave_mem_write_32(addr, tmp, ctxt);
}

double aica_wave_mem_read_double(addr32_t addr, void *ctxt) {
    error_set_length(sizeof(double));
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void aica_wave_mem_write_double(addr32_t addr, double val, void *ctxt) {
    error_set_length(sizeof(double));
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint8_t aica_wave_mem_read_8(addr32_t addr, void *ctxt) {
    struct aica_wave_mem *wm = (struct aica_wave_mem*)ctxt;

    if ((sizeof(uint8_t) - 1 + addr) >= AICA_WAVE_MEM_LEN) {
        error_set_feature("out-of-bounds AICA memory access");
        error_set_address(addr);
        error_set_length(1);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    uint8_t const *valp = ((uint8_t const*)wm->mem) + addr;

#ifdef ENABLE_LOG_DEBUG
    if (aica_log_verbose_val) {
        __attribute__((unused)) unsigned pc =
            dreamcast_get_cpu()->reg[SH4_REG_PC];
        LOG_DBG("AICA: reading 0x%02x from 0x%08x (PC is 0x%08x)\n",
                (unsigned)*valp, (unsigned)addr, pc);
    }
#endif

    return *valp;
}

void aica_wave_mem_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    struct aica_wave_mem *wm = (struct aica_wave_mem*)ctxt;

    uint8_t *outp = ((uint8_t*)wm->mem) + addr;

#ifdef ENABLE_LOG_DEBUG
    if (aica_log_verbose_val) {
        __attribute__((unused)) unsigned pc =
            dreamcast_get_cpu()->reg[SH4_REG_PC];
        LOG_DBG("AICA: writing 0x%02x to 0x%08x (PC is 0x%08x)\n",
                (unsigned)val, (unsigned)addr, pc);
    }
#endif

    if ((sizeof(uint8_t) - 1 + addr) >= AICA_WAVE_MEM_LEN) {
        error_set_feature("out-of-bounds AICA memory access");
        error_set_address(addr);
        error_set_length(1);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    *outp = val;
}

uint16_t aica_wave_mem_read_16(addr32_t addr, void *ctxt) {
    struct aica_wave_mem *wm = (struct aica_wave_mem*)ctxt;

    if ((sizeof(uint16_t) - 1 + addr) >= AICA_WAVE_MEM_LEN) {
        error_set_feature("out-of-bounds AICA memory access");
        error_set_address(addr);
        error_set_length(2);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    uint16_t ret;
    memcpy(&ret, wm->mem + addr, sizeof(ret));

#ifdef ENABLE_LOG_DEBUG
    if (aica_log_verbose_val) {
        __attribute__((unused)) unsigned pc =
            dreamcast_get_cpu()->reg[SH4_REG_PC];
        LOG_DBG("AICA: reading 0x%04x from 0x%08x (PC is 0x%08x)\n",
                (unsigned)ret, (unsigned)addr, pc);
    }
#endif

    return ret;
}

void aica_wave_mem_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    struct aica_wave_mem *wm = (struct aica_wave_mem*)ctxt;

#ifdef ENABLE_LOG_DEBUG
    if (aica_log_verbose_val) {
        __attribute__((unused)) unsigned pc =
            dreamcast_get_cpu()->reg[SH4_REG_PC];
        LOG_DBG("AICA: writing 0x%04x to 0x%08x (PC is 0x%08x)\n",
                (unsigned)val, (unsigned)addr, pc);
    }
#endif

    if ((sizeof(uint16_t) - 1 + addr) >= AICA_WAVE_MEM_LEN) {
        error_set_feature("out-of-bounds AICA memory access");
        error_set_address(addr);
        error_set_length(2);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    memcpy(wm->mem + addr, &val, sizeof(val));
}

void aica_wave_mem_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    struct aica_wave_mem *wm = (struct aica_wave_mem*)ctxt;

#ifdef ENABLE_LOG_DEBUG
    if (aica_log_verbose_val) {
        __attribute__((unused)) unsigned pc =
            dreamcast_get_cpu()->reg[SH4_REG_PC];
        LOG_DBG("AICA: writing 0x%08x to 0x%08x (PC is 0x%08x)\n",
                (unsigned)val, (unsigned)addr, pc);
    }
#endif

    if ((sizeof(uint32_t) - 1 + addr) >= AICA_WAVE_MEM_LEN) {
        error_set_feature("out-of-bounds AICA memory access");
        error_set_address(addr);
        error_set_length(4);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    memcpy(wm->mem + addr, &val, sizeof(val));
}

struct memory_interface aica_wave_mem_intf = {
    .read32 = aica_wave_mem_read_32,
    .read16 = aica_wave_mem_read_16,
    .read8 = aica_wave_mem_read_8,
    .readfloat = aica_wave_mem_read_float,
    .readdouble = aica_wave_mem_read_double,

    .write32 = aica_wave_mem_write_32,
    .write16 = aica_wave_mem_write_16,
    .write8 = aica_wave_mem_write_8,
    .writefloat = aica_wave_mem_write_float,
    .writedouble = aica_wave_mem_write_double
};
