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
#include "config.h"

/*
 * this value needs to be non-zero.  I don't think it matters what the value is
 * as long as it is non-zero.
 *
 * This is read by Power Stone at PC=0xc0e5596.  It will spin forever until
 * this value is non-zero
 */
#define POWER_STONE_HACK_ADDR_1 0x0080005c
#define POWER_STONE_HACK_VAL_1  1

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
#define POWER_STONE_HACK_ADDR_2 0x00800284
#define POWER_STONE_HACK_VAL_2  0x00800000

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
#define POWER_STONE_HACK_ADDR_3 0x00800288
#define POWER_STONE_HACK_VAL_3  0x00800004

atomic_bool aica_log_verbose_val = ATOMIC_VAR_INIT(false);

static uint8_t aica_wave_mem[ADDR_AICA_WAVE_LAST - ADDR_AICA_WAVE_FIRST + 1];

int aica_wave_mem_read(void *buf, size_t addr, size_t len) {
    void const *start_addr = aica_wave_mem + (addr - ADDR_AICA_WAVE_FIRST);

    /*
     * If enabled, trick power-stone into thinking we have a working CPU.
     *
     * It reads from POWER_STONE_HACK_ADDR_1 at PC=0xc0e5596 and will not progress
     * unless the value it reads is non-zero.
     */
    if (addr == POWER_STONE_HACK_ADDR_1 &&
        config_get_hack_power_stone_no_aica()) {
        uint32_t val = POWER_STONE_HACK_VAL_1;
        if (len > sizeof(val)) {
            error_set_feature("reads of greater than 4 bytes when using the "
                              "Power Stone no-AICA hack");
            PENDING_ERROR(ERROR_UNIMPLEMENTED);
            return MEM_ACCESS_FAILURE;
        }

        memcpy(buf, &val, len);
        if (atomic_load_explicit(&aica_log_verbose_val, memory_order_relaxed)) {
            printf("AICA: reading %u from 0x%08x due to the no-AICA Power "
                   "Stone hack\n", POWER_STONE_HACK_VAL_1, POWER_STONE_HACK_ADDR_1);
        }
        return MEM_ACCESS_SUCCESS;
    } else if (addr == POWER_STONE_HACK_ADDR_2 &&
        config_get_hack_power_stone_no_aica()) {
        uint32_t val = POWER_STONE_HACK_VAL_2;
        if (len > sizeof(val)) {
            error_set_feature("reads of greater than 4 bytes when using the "
                              "Power Stone no-AICA hack");
            PENDING_ERROR(ERROR_UNIMPLEMENTED);
            return MEM_ACCESS_FAILURE;
        }

        memcpy(buf, &val, len);
        if (atomic_load_explicit(&aica_log_verbose_val, memory_order_relaxed)) {
            printf("AICA: reading %u from 0x%08x due to the no-AICA Power "
                   "Stone hack\n", POWER_STONE_HACK_VAL_2, POWER_STONE_HACK_ADDR_2);
        }
        return MEM_ACCESS_SUCCESS;
    } else if (addr == POWER_STONE_HACK_ADDR_3 &&
        config_get_hack_power_stone_no_aica()) {
        uint32_t val = POWER_STONE_HACK_VAL_3;
        if (len > sizeof(val)) {
            error_set_feature("reads of greater than 4 bytes when using the "
                              "Power Stone no-AICA hack");
            PENDING_ERROR(ERROR_UNIMPLEMENTED);
            return MEM_ACCESS_FAILURE;
        }

        memcpy(buf, &val, len);
        if (atomic_load_explicit(&aica_log_verbose_val, memory_order_relaxed)) {
            printf("AICA: reading %u from 0x%08x due to the no-AICA Power "
                   "Stone hack\n", POWER_STONE_HACK_VAL_3, POWER_STONE_HACK_ADDR_3);
        }
        return MEM_ACCESS_SUCCESS;
    }

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
