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

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "mem_code.h"
#include "MemoryMap.h"
#include "error.h"
#include "dreamcast.h"
#include "hw/sh4/sh4.h"

atomic_bool aica_log_verbose_val = ATOMIC_VAR_INIT(false);

static uint8_t aica_wave_mem[ADDR_AICA_WAVE_LAST - ADDR_AICA_WAVE_FIRST + 1];

int aica_wave_mem_read(void *buf, size_t addr, size_t len) {
    void const *start_addr = aica_wave_mem + (addr - ADDR_AICA_WAVE_FIRST);

    if (addr < ADDR_AICA_WAVE_FIRST || addr > ADDR_AICA_WAVE_LAST ||
        ((addr - 1 + len) > ADDR_AICA_WAVE_LAST) ||
        ((addr - 1 + len) < ADDR_AICA_WAVE_FIRST)) {
        error_set_feature("aw fuck it");
        PENDING_ERROR(ERROR_UNIMPLEMENTED);
        return MEM_ACCESS_FAILURE;
    }

    memcpy(buf, start_addr, len);

    if (atomic_load_explicit(&aica_log_verbose_val, memory_order_relaxed)) {
        unsigned pc = dreamcast_get_cpu()->reg[SH4_REG_PC];
        if (len == 4) {
            uint32_t frak;
            memcpy(&frak, buf, sizeof(frak));
            printf("AICA: reading 0x%08x from 0x%08x (PC is 0x%08x)\n",
                   (unsigned)frak, (unsigned)addr, pc);
        } else {
            printf("AICA: reading %u bytes from 0x%08x (PC is 0x%08x)\n",
                   (unsigned)len, (unsigned)addr, pc);
        }
    }

    return MEM_ACCESS_SUCCESS;
}

int aica_wave_mem_write(void const *buf, size_t addr, size_t len) {
    void *start_addr = aica_wave_mem + (addr - ADDR_AICA_WAVE_FIRST);

    if (atomic_load_explicit(&aica_log_verbose_val, memory_order_relaxed)) {
        unsigned pc = dreamcast_get_cpu()->reg[SH4_REG_PC];
        if (len == 4) {
            uint32_t frak;
            memcpy(&frak, buf, sizeof(frak));
            printf("AICA: writing 0x%08x to 0x%08x (PC is 0x%08x)\n",
                   (unsigned)frak, (unsigned)addr, pc);
        } else {
            printf("AICA: writing %u bytes to 0x%08x (PC is 0x%08x)\n",
                   (unsigned)len, (unsigned)addr, pc);
        }
    }

    if (addr < ADDR_AICA_WAVE_FIRST || addr > ADDR_AICA_WAVE_LAST ||
        ((addr - 1 + len) > ADDR_AICA_WAVE_LAST) ||
        ((addr - 1 + len) < ADDR_AICA_WAVE_FIRST)) {
        error_set_feature("aw fuck it");
        PENDING_ERROR(ERROR_UNIMPLEMENTED);
        return MEM_ACCESS_FAILURE;
    }

    memcpy(start_addr, buf, len);
    return MEM_ACCESS_SUCCESS;
}

void aica_log_verbose(bool verbose) {
    atomic_store_explicit(&aica_log_verbose_val, verbose, memory_order_relaxed);
}
