/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016, 2017 snickerbockers
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

#include <stddef.h>

#include "hw/sys/sys_block.h"
#include "hw/maple/maple_reg.h"
#include "hw/g1/g1_reg.h"
#include "hw/g2/g2_reg.h"
#include "hw/g2/modem.h"
#include "hw/pvr2/pvr2_reg.h"
#include "hw/pvr2/pvr2_core_reg.h"
#include "hw/pvr2/pvr2_tex_mem.h"
#include "hw/aica/aica_reg.h"
#include "hw/aica/aica_rtc.h"
#include "hw/aica/aica_wave_mem.h"
#include "hw/gdrom/gdrom_reg.h"
#include "flash_memory.h"
#include "error.h"
#include "mem_code.h"

#include "MemoryMap.h"

static struct BiosFile *bios;
static struct Memory *mem;

static inline int read_area3(void *buf, size_t addr, size_t len);
static inline int write_area3(void const *buf, size_t addr, size_t len);
static inline int read_area0(void *buf, size_t addr, size_t len);
static inline int write_area0(void const *buf, size_t addr, size_t len);

void memory_map_init(BiosFile *bios_new, struct Memory *mem_new) {
    memory_map_set_bios(bios_new);
    memory_map_set_mem(mem_new);
}

void memory_map_set_bios(BiosFile *bios_new) {
    bios = bios_new;
}

void memory_map_set_mem(struct Memory *mem_new) {
    mem = mem_new;
}

int memory_map_read(void *buf, size_t addr, size_t len) {
    size_t first_addr = addr;
    size_t last_addr = addr + (len - 1);

    // check RAM first because that's the case we want to optimize for
    if (first_addr >= ADDR_AREA3_FIRST && last_addr <= ADDR_AREA3_LAST) {
        return read_area3(buf, addr, len);
    } else if (first_addr >= ADDR_TEX32_FIRST && last_addr <= ADDR_TEX32_LAST) {
        return pvr2_tex_mem_area32_read(buf, addr, len);
    } else if (first_addr >= ADDR_TEX64_FIRST && last_addr <= ADDR_TEX64_LAST) {
        return pvr2_tex_mem_area64_read(buf, addr, len);
    } else if (addr >= ADDR_AREA0_FIRST && addr <= ADDR_AREA0_LAST) {
        return read_area0(buf, addr, len);
    }

    error_set_feature("memory mapping");
    error_set_address(addr);
    error_set_length(len);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;

boundary_cross:
    /*
     * this label is where we go to when the requested read is not
     * contained entirely withing a single mapping.
     */
    error_set_feature("proper response for when the guest reads past a memory "
                      "map's end");
    error_set_length(len);
    error_set_address(addr);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

int memory_map_write(void const *buf, size_t addr, size_t len) {
    size_t first_addr = addr;
    size_t last_addr = addr + (len - 1);

    // check RAM first because that's the case we want to optimize for
    if (first_addr >= ADDR_AREA3_FIRST && last_addr <= ADDR_AREA3_LAST) {
        return write_area3(buf, addr, len);
    } else if (first_addr >= ADDR_TEX32_FIRST && last_addr <= ADDR_TEX32_LAST) {
        return pvr2_tex_mem_area32_write(buf, addr, len);
    } else if (first_addr >= ADDR_TEX64_FIRST && last_addr <= ADDR_TEX64_LAST) {
        return pvr2_tex_mem_area64_write(buf, addr, len);
    } else if (first_addr >= ADDR_AREA0_FIRST && last_addr <= ADDR_AREA0_LAST) {
        return write_area0(buf, addr, len);
    }

    error_set_feature("memory mapping");
    error_set_address(addr);
    error_set_length(len);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;

boundary_cross:
    /* when the write is not contained entirely within one mapping */
    error_set_feature("proper response for when the guest writes past a memory "
                      "map's end");
    error_set_length(len);
    error_set_address(addr);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static inline int read_area0(void *buf, size_t addr, size_t len) {
    addr &= ADDR_AREA0_MASK;
    size_t first_addr = addr;
    size_t last_addr = addr + (len - 1);

    if (last_addr <= ADDR_BIOS_LAST) {
        /*
         * XXX In case you were wondering: we don't check to see if
         * addr >= ADDR_BIOS_FIRST because ADDR_BIOS_FIRST is 0
         */
        return bios_file_read(bios, buf, addr - ADDR_BIOS_FIRST, len);
    } else if (first_addr >= ADDR_FLASH_FIRST && last_addr <= ADDR_FLASH_LAST) {
        return flash_mem_read(buf, addr, len);
    } else if (first_addr >= ADDR_G1_FIRST && last_addr <= ADDR_G1_LAST) {
        return g1_reg_read(buf, addr, len);
    } else if (first_addr >= ADDR_SYS_FIRST && last_addr <= ADDR_SYS_LAST) {
        return sys_block_read(buf, addr, len);
    } else if (first_addr >= ADDR_MAPLE_FIRST && last_addr <= ADDR_MAPLE_LAST) {
        return maple_reg_read(buf, addr, len);
    } else if (first_addr >= ADDR_G2_FIRST && last_addr <= ADDR_G2_LAST) {
        return g2_reg_read(buf, addr, len);
    } else if (first_addr >= ADDR_PVR2_FIRST && last_addr <= ADDR_PVR2_LAST) {
        return pvr2_reg_read(buf, addr, len);
    } else if (first_addr >= ADDR_MODEM_FIRST && last_addr <= ADDR_MODEM_LAST) {
        return modem_read(buf, addr, len);
    } else if (first_addr >= ADDR_PVR2_CORE_FIRST &&
               last_addr <= ADDR_PVR2_CORE_LAST) {
        return pvr2_core_reg_read(buf, addr, len);
    } else if(first_addr >= ADDR_AICA_FIRST && last_addr <= ADDR_AICA_LAST) {
        return aica_reg_read(buf, addr, len);
    } else if (first_addr >= ADDR_AICA_WAVE_FIRST &&
               last_addr <= ADDR_AICA_WAVE_LAST) {
        return aica_wave_mem_read(buf, addr, len);
    } else if (first_addr >= ADDR_AICA_RTC_FIRST &&
               last_addr <= ADDR_AICA_RTC_LAST) {
        return aica_rtc_read(buf, addr, len);
    } else if (first_addr >= ADDR_GDROM_FIRST && last_addr <= ADDR_GDROM_LAST) {
        return gdrom_reg_read(buf, addr, len);
    }

boundary_cross:
    /* when the write is not contained entirely within one mapping */
    error_set_feature("proper response for when the guest writes past a memory "
                      "map's end");
    error_set_length(len);
    error_set_address(addr);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static inline int write_area0(void const *buf, size_t addr, size_t len) {
    addr &= ADDR_AREA0_MASK;
    size_t first_addr = addr;
    size_t last_addr = addr + (len - 1);

    if (last_addr <= ADDR_BIOS_LAST) {
        /*
         * XXX In case you were wondering: we don't check to see if
         * addr >= ADDR_BIOS_FIRST because ADDR_BIOS_FIRST is 0
         */
        goto boundary_cross;
    } else if (first_addr >= ADDR_FLASH_FIRST && last_addr <= ADDR_FLASH_LAST) {
        return flash_mem_write(buf, addr, len);
    } else if (first_addr >= ADDR_G1_FIRST && last_addr <= ADDR_G1_LAST) {
        return g1_reg_write(buf, addr, len);
    } else if (first_addr >= ADDR_SYS_FIRST && last_addr <= ADDR_SYS_LAST) {
        return sys_block_write(buf, addr, len);
    } else if (first_addr >= ADDR_MAPLE_FIRST && last_addr <= ADDR_MAPLE_LAST) {
        return maple_reg_write(buf, addr, len);
    } else if (first_addr >= ADDR_G2_FIRST && last_addr <= ADDR_G2_LAST) {
        return g2_reg_write(buf, addr, len);
    } else if (first_addr >= ADDR_PVR2_FIRST && last_addr <= ADDR_PVR2_LAST) {
        return pvr2_reg_write(buf, addr, len);
    } else if (first_addr >= ADDR_MODEM_FIRST && last_addr <= ADDR_MODEM_LAST) {
        return modem_write(buf, addr, len);
    } else if (first_addr >= ADDR_PVR2_CORE_FIRST &&
               last_addr <= ADDR_PVR2_CORE_LAST) {
        return pvr2_core_reg_write(buf, addr, len);
    } else if(first_addr >= ADDR_AICA_FIRST && last_addr <= ADDR_AICA_LAST) {
        return aica_reg_write(buf, addr, len);
    } else if (first_addr >= ADDR_AICA_WAVE_FIRST &&
               last_addr <= ADDR_AICA_WAVE_LAST) {
        return aica_wave_mem_write(buf, addr, len);
    } else if (first_addr >= ADDR_AICA_RTC_FIRST &&
               last_addr <= ADDR_AICA_RTC_LAST) {
        return aica_rtc_write(buf, addr, len);
    } else if (first_addr >= ADDR_GDROM_FIRST && last_addr <= ADDR_GDROM_LAST) {
        return gdrom_reg_write(buf, addr, len);
    }

boundary_cross:
    /* when the write is not contained entirely within one mapping */
    error_set_feature("proper response for when the guest writes past a memory "
                      "map's end");
    error_set_length(len);
    error_set_address(addr);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static inline int read_area3(void *buf, size_t addr, size_t len) {
    return memory_read(mem, buf, addr & ADDR_AREA3_MASK, len);
}

static inline int write_area3(void const *buf, size_t addr, size_t len) {
    return memory_write(mem, buf, addr & ADDR_AREA3_MASK, len);
}
