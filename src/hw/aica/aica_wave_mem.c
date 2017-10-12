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
    { .addr = 0x0080005c, .val = 1 },

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
    { .addr = 0x00800284, .val = 0x00800000 },

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
    { .addr = 0x00800288, .val = 0x00800004 },

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
    { .addr = 0x008002e4, .val = 0x00800008 },

    /*
     * Once again, in Power Stone at PC=0x0c0e65ae the SH4 will read a 4-byte
     * pointer from this address and write something to the location it points
     * to.  I'm not sure what the correct address is, but having it write to
     * (probable) ARM7 instruction memory makes things work.
     */
    { .addr = 0x008002e8, .val = 0x0080000c },

    /*
     * Crazy Taxi reads from this one location at PC=0x0c07f462.  If it is
     * nonzero, it interprets that value as a pointer, and reads from what that
     * points to.  That value is then ANDed with 0x7ff and written back.
     *
     * So the value at address 0x00800104 needs to be a pointer to a place where
     * Crazy Taxi can read a 4-byte integer, and it with 0x7ff and write back.
     */
    { .addr = 0x00800104, .val = 0x00800010 },

    /*
     * another Crazy Taxi situation, similar to the previous one.  This one
     * happens at PC=0x0c07f462.  I'm just assuming that it wants to see a
     * pointer to aica memory again.  All I know for sure is that it will hang
     * until this is nonzero.
     */
    { .addr = 0x00800164, .val = 0x00800014 },

    /*
     * Crazy Taxi again.  As before, I don't know what the correct value is, I
     * just know that it should be non-zero.  Current value chosen is a pointer
     * to (presumably) program data in the audio memory.
     *
     * This happens in Crazy Taxi at PC=0x0c07f462
     */
    { .addr = 0x00800224, .val = 0x00800018 },

    /*
     * More Crazy Taxi.
     * AICA: reading 0x00000000 from 0x008001c4 (PC is 0x0c07f462)
     */
    { .addr = 0x008001c4, .val = 0x0080001c },

    { .end = true }
};

atomic_bool aica_log_verbose_val = ATOMIC_VAR_INIT(false);

static uint8_t aica_wave_mem[ADDR_AICA_WAVE_LAST - ADDR_AICA_WAVE_FIRST + 1];

int aica_wave_mem_read(void *buf, size_t addr, size_t len) {
    void const *start_addr = aica_wave_mem + (addr - ADDR_AICA_WAVE_FIRST);

    // If enabled, trick games into thinking we have a working AICA CPU.
    if (config_get_hack_power_stone_no_aica()) {
        struct aica_mem_hack const *cursor = &no_aica_hack[0];
        while (!cursor->end) {
            if (addr == cursor->addr) {
                if (len > sizeof(cursor->val)) {
                    error_set_feature("reads of greater than 4 bytes when "
                                      "using the Power Stone no-AICA hack");
                    PENDING_ERROR(ERROR_UNIMPLEMENTED);
                    return MEM_ACCESS_FAILURE;
                }

                memcpy(buf, &cursor->val, len);
                if (atomic_load_explicit(&aica_log_verbose_val,
                                         memory_order_relaxed)) {
                    printf("AICA: reading %u from 0x%08x due to the no-AICA "
                           "Power Stone hack\n",
                           (unsigned)cursor->val, (unsigned)cursor->addr);
                }
                return MEM_ACCESS_SUCCESS;
            }

            cursor++;
        }
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
