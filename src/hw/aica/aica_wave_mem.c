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

#include "mem_code.h"
#include "MemoryMap.h"
#include "error.h"

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
    return MEM_ACCESS_SUCCESS;
}

int aica_wave_mem_write(void const *buf, size_t addr, size_t len) {
    void *start_addr = aica_wave_mem + (addr - ADDR_AICA_WAVE_FIRST);

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
