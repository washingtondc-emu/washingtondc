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
#include "compiler_bullshit.h"

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
        WASHDC_UNUSED unsigned pc =
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
