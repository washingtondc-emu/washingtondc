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

#ifndef AICA_WAVE_MEM_H_
#define AICA_WAVE_MEM_H_

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "washdc/error.h"
#include "washdc/types.h"
#include "washdc/MemoryMap.h"
#include "dreamcast.h"

#define AICA_WAVE_MEM_LEN (0x009fffff - 0x00800000 + 1)

static_assert(!(AICA_WAVE_MEM_LEN & (AICA_WAVE_MEM_LEN - 1)),
              "non-power-of-two aica memory length");

#define AICA_WAVE_MEM_MASK (AICA_WAVE_MEM_LEN - 1)

struct aica_wave_mem {
    uint8_t mem[AICA_WAVE_MEM_LEN];
};

float aica_wave_mem_read_float(addr32_t addr, void *ctxt);
void aica_wave_mem_write_float(addr32_t addr, float val, void *ctxt);
double aica_wave_mem_read_double(addr32_t addr, void *ctxt);
void aica_wave_mem_write_double(addr32_t addr, double val, void *ctxt);
uint8_t aica_wave_mem_read_8(addr32_t addr, void *ctxt);
void aica_wave_mem_write_8(addr32_t addr, uint8_t val, void *ctxt);
uint16_t aica_wave_mem_read_16(addr32_t addr, void *ctxt);
void aica_wave_mem_write_16(addr32_t addr, uint16_t val, void *ctxt);
void aica_wave_mem_write_32(addr32_t addr, uint32_t val, void *ctxt);


extern bool aica_log_verbose_val;
/*
 * XXX the reason why this is the only one that gets inlined is that the arm7
 * code calls it directly every time there's an instruction fetch.
 */
static inline uint32_t aica_wave_mem_read_32(addr32_t addr, void *ctxt) {
    struct aica_wave_mem *wm = (struct aica_wave_mem*)ctxt;

    if ((sizeof(uint32_t) - 1 + addr) >= AICA_WAVE_MEM_LEN) {
        error_set_feature("out-of-bounds AICA memory access");
        error_set_address(addr);
        error_set_length(4);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    uint32_t ret;
    memcpy(&ret, wm->mem + addr, sizeof(ret));

#ifdef ENABLE_LOG_DEBUG
    if (aica_log_verbose_val) {
        __attribute__((unused)) unsigned pc =
            dreamcast_get_cpu()->reg[SH4_REG_PC];
        LOG_DBG("AICA: reading 0x%08x from 0x%08x (PC is 0x%08x)\n",
                (unsigned)ret, (unsigned)addr, pc);
    }
#endif

    return ret;
}

void aica_log_verbose(bool verbose);

extern struct memory_interface aica_wave_mem_intf;

void aica_wave_mem_init(struct aica_wave_mem *wm);
void aica_wave_mem_cleanup(struct aica_wave_mem *wm);

#endif
