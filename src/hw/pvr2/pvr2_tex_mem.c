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

uint8_t pvr2_tex32_mem[ADDR_TEX32_LAST - ADDR_TEX32_FIRST + 1];
uint8_t pvr2_tex64_mem[ADDR_TEX64_LAST - ADDR_TEX64_FIRST + 1];

int pvr2_tex_mem_area32_read(void *buf, size_t addr, size_t len) {
    void const *start_addr = pvr2_tex32_mem + (addr - ADDR_TEX32_FIRST);

    if (addr < ADDR_TEX32_FIRST || addr > ADDR_TEX32_LAST ||
        ((addr - 1 + len) > ADDR_TEX32_LAST) ||
        ((addr - 1 + len) < ADDR_TEX32_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        PENDING_ERROR(ERROR_UNIMPLEMENTED);
        return MEM_ACCESS_FAILURE;
    }

    memcpy(buf, start_addr, len);
    return MEM_ACCESS_SUCCESS;
}

int pvr2_tex_mem_area32_write(void const *buf, size_t addr, size_t len) {
    void *start_addr = pvr2_tex32_mem + (addr - ADDR_TEX32_FIRST);

    if (addr < ADDR_TEX32_FIRST || addr > ADDR_TEX32_LAST ||
        ((addr - 1 + len) > ADDR_TEX32_LAST) ||
        ((addr - 1 + len) < ADDR_TEX32_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        PENDING_ERROR(ERROR_UNIMPLEMENTED);
        return MEM_ACCESS_FAILURE;
    }

    memcpy(start_addr, buf, len);
    return MEM_ACCESS_SUCCESS;
}

int pvr2_tex_mem_area64_read(void *buf, size_t addr, size_t len) {
    void const *start_addr = pvr2_tex64_mem + (addr - ADDR_TEX64_FIRST);

    if (addr < ADDR_TEX64_FIRST || addr > ADDR_TEX64_LAST ||
        ((addr - 1 + len) > ADDR_TEX64_LAST) ||
        ((addr - 1 + len) < ADDR_TEX64_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        PENDING_ERROR(ERROR_UNIMPLEMENTED);
        return MEM_ACCESS_FAILURE;
    }

    memcpy(buf, start_addr, len);
    return MEM_ACCESS_SUCCESS;
}

int pvr2_tex_mem_area64_write(void const *buf, size_t addr, size_t len) {
    void *start_addr = pvr2_tex64_mem + (addr - ADDR_TEX64_FIRST);

    if (addr < ADDR_TEX64_FIRST || addr > ADDR_TEX64_LAST ||
        ((addr - 1 + len) > ADDR_TEX64_LAST) ||
        ((addr - 1 + len) < ADDR_TEX64_FIRST)) {
        error_set_feature("out-of-bounds PVR2 texture memory read");
        PENDING_ERROR(ERROR_UNIMPLEMENTED);
        return MEM_ACCESS_FAILURE;
    }

    memcpy(start_addr, buf, len);
    return MEM_ACCESS_SUCCESS;
}
