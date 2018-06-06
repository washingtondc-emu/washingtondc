/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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
#include "MemoryMap.h"
#include "error.h"
#include "dreamcast.h"
#include "hw/sh4/sh4.h"
#include "config.h"
#include "log.h"

#include "aica_wave_mem.h"

static struct aica_mem_hack {
    uint32_t addr;
    uint32_t val;
    bool end;
} no_aica_hack[] = {
    /*
     * this value needs to be non-zero.  I don't think it matters what the value is
     * as long as it is non-zero.
     *
     * This is read by Power Stone at PC=0xc0e5596.  It will spin forever until
     * this value is non-zero
     */
    { .addr = 0x0000005c, .val = 1 },

    /*
     * This value needs to point to AICA waveform memory.  at PC=0xc0e657c,
     * Power Stone will read from this memory location, add 0x7ff to the value and
     * then write it back to this same memory location at PC=0xc0e6586.
     *
     * The value used here is not the correct value; I do not know what the correct
     * value is.  Since I don't have a working ARM7 CPU implementation, the safest
     * bet is to choose somewhere that probably stores executable code.  That way I
     * know I'm not accidentally trampling over some other value that the SH4 CPU
     * software tries to access.
     */
    { .addr = 0x00000284, .val = 0x00800000 },

    /*
     * This value needs to point to AICA waveform memory.  at PC=0xc0e65ae,
     * Power Stone will read from this memory location, add 0x7ff to the value and
     * then write it back to this same memory location at PC=0xc0e65b8.
     *
     * The value used here is not the correct value; I do not know what the correct
     * value is.  Since I don't have a working ARM7 CPU implementation, the safest
     * bet is to choose somewhere that probably stores executable code.  That way I
     * know I'm not accidentally trampling over some other value that the SH4 CPU
     * software tries to access.
     */
    { .addr = 0x00000288, .val = 0x00800004 },

    /*
     * This value needs to point to AICA waveform memory.  at PC=0xc0e657c,
     * Power Stone will read from this memory location, add 0x7ff to the value and
     * then write it back to this same memory location at PC=0xc0e6586.
     *
     * The value used here is not the correct value; I do not know what the correct
     * value is.  Since I don't have a working ARM7 CPU implementation, the safest
     * bet is to choose somewhere that probably stores executable code.  That way I
     * know I'm not accidentally trampling over some other value that the SH4 CPU
     * software tries to access.
     */
    { .addr = 0x000002e4, .val = 0x00800008 },

    /*
     * Once again, in Power Stone at PC=0x0c0e65ae the SH4 will read a 4-byte
     * pointer from this address and write something to the location it points
     * to.  I'm not sure what the correct address is, but having it write to
     * (probable) ARM7 instruction memory makes things work.
     */
    { .addr = 0x000002e8, .val = 0x0080000c },

    /*
     * Crazy Taxi reads from this one location at PC=0x0c07f462.  If it is
     * nonzero, it interprets that value as a pointer, and reads from what that
     * points to.  That value is then ANDed with 0x7ff and written back.
     *
     * So the value at address 0x00800104 needs to be a pointer to a place where
     * Crazy Taxi can read a 4-byte integer, and it with 0x7ff and write back.
     */
    { .addr = 0x00000104, .val = 0x00800010 },

    /*
     * another Crazy Taxi situation, similar to the previous one.  This one
     * happens at PC=0x0c07f462.  I'm just assuming that it wants to see a
     * pointer to aica memory again.  All I know for sure is that it will hang
     * until this is nonzero.
     */
    { .addr = 0x00000164, .val = 0x00800014 },

    /*
     * Crazy Taxi again.  As before, I don't know what the correct value is, I
     * just know that it should be non-zero.  Current value chosen is a pointer
     * to (presumably) program data in the audio memory.
     *
     * This happens in Crazy Taxi at PC=0x0c07f462
     */
    { .addr = 0x00000224, .val = 0x00800018 },

    /*
     * More Crazy Taxi.
     * AICA: reading 0x00000000 from 0x008001c4 (PC is 0x0c07f462)
     */
    { .addr = 0x000001c4, .val = 0x0080001c },

    { .end = true }
};

bool aica_log_verbose_val;

static struct aica_mem_hack const *check_hack(addr32_t addr) {
    struct aica_mem_hack const *cursor = &no_aica_hack[0];
    while (!cursor->end) {
        if (addr == cursor->addr)
            return cursor;
        cursor++;
    }
    return NULL;
}

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

    if (aica_log_verbose_val) {
        __attribute__((unused)) unsigned pc =
            dreamcast_get_cpu()->reg[SH4_REG_PC];
        LOG_DBG("AICA: reading 0x%02x from 0x%08x (PC is 0x%08x)\n",
                (unsigned)*valp, (unsigned)addr, pc);
    }

    return *valp;
}

void aica_wave_mem_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    struct aica_wave_mem *wm = (struct aica_wave_mem*)ctxt;

    uint8_t *outp = ((uint8_t*)wm->mem) + addr;

    if (aica_log_verbose_val) {
        __attribute__((unused)) unsigned pc =
            dreamcast_get_cpu()->reg[SH4_REG_PC];
        LOG_DBG("AICA: writing 0x%02x to 0x%08x (PC is 0x%08x)\n",
                (unsigned)val, (unsigned)addr, pc);
    }

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

      uint16_t const *valp = ((uint16_t const*)wm->mem) + addr / 2;

    if (aica_log_verbose_val) {
        __attribute__((unused)) unsigned pc =
            dreamcast_get_cpu()->reg[SH4_REG_PC];
        LOG_DBG("AICA: reading 0x%04x from 0x%08x (PC is 0x%08x)\n",
                (unsigned)*valp, (unsigned)addr, pc);
    }

    return *valp;
}

void aica_wave_mem_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    struct aica_wave_mem *wm = (struct aica_wave_mem*)ctxt;

    uint16_t *outp = ((uint16_t*)wm->mem) + addr / 2;

    if (aica_log_verbose_val) {
        __attribute__((unused)) unsigned pc =
            dreamcast_get_cpu()->reg[SH4_REG_PC];
        LOG_DBG("AICA: writing 0x%04x to 0x%08x (PC is 0x%08x)\n",
                (unsigned)val, (unsigned)addr, pc);
    }

    if ((sizeof(uint16_t) - 1 + addr) >= AICA_WAVE_MEM_LEN) {
        error_set_feature("out-of-bounds AICA memory access");
        error_set_address(addr);
        error_set_length(2);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    *outp = val;
}

uint32_t aica_wave_mem_read_32(addr32_t addr, void *ctxt) {
    struct aica_wave_mem *wm = (struct aica_wave_mem*)ctxt;

    if (config_get_hack_power_stone_no_aica()) {
        struct aica_mem_hack const *hack = check_hack(addr);
        if (hack) {
            if (aica_log_verbose_val) {
                LOG_DBG("AICA: reading %u from 0x%08x due to the no-AICA "
                        "Power Stone hack\n",
                        (unsigned)hack->val, (unsigned)hack->addr);
            }
            return hack->val;
        }
    }

    if ((sizeof(uint32_t) - 1 + addr) >= AICA_WAVE_MEM_LEN) {
        error_set_feature("out-of-bounds AICA memory access");
        error_set_address(addr);
        error_set_length(4);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    uint32_t const *valp = ((uint32_t const*)wm->mem) + addr / 4;

    if (aica_log_verbose_val) {
        __attribute__((unused)) unsigned pc =
            dreamcast_get_cpu()->reg[SH4_REG_PC];
        LOG_DBG("AICA: reading 0x%08x from 0x%08x (PC is 0x%08x)\n",
                (unsigned)*valp, (unsigned)addr, pc);
    }

    return *valp;
}

void aica_wave_mem_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    struct aica_wave_mem *wm = (struct aica_wave_mem*)ctxt;

    uint32_t *outp = ((uint32_t*)wm->mem) + addr / 4;

    if (aica_log_verbose_val) {
        __attribute__((unused)) unsigned pc =
            dreamcast_get_cpu()->reg[SH4_REG_PC];
        LOG_DBG("AICA: writing 0x%08x to 0x%08x (PC is 0x%08x)\n",
                (unsigned)val, (unsigned)addr, pc);
    }

    if ((sizeof(uint32_t) - 1 + addr) >= AICA_WAVE_MEM_LEN) {
        error_set_feature("out-of-bounds AICA memory access");
        error_set_address(addr);
        error_set_length(4);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    *outp = val;
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
