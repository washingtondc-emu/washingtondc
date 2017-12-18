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
#include "hw/pvr2/pvr2_ta.h"
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

static inline int read_area0(void *buf, size_t addr, size_t len);
static inline int write_area0(void const *buf, size_t addr, size_t len);
static inline int read_area3(void *buf, size_t addr, size_t len);
static inline int write_area3(void const *buf, size_t addr, size_t len);
static inline int read_area4(void *buf, size_t addr, size_t len);
static inline int write_area4(void const *buf, size_t addr, size_t len);

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

#define MEMORY_MAP_READ_TMPL(size)                                      \
    uint##size##_t memory_map_read_##size(size_t addr) {                \
        size_t first_addr = addr;                                       \
        size_t last_addr = sizeof(uint##size##_t) - 1 + first_addr;     \
                                                                        \
        if (first_addr >= ADDR_AREA3_FIRST && last_addr <= ADDR_AREA3_LAST) { \
            return memory_read##size(mem, addr & ADDR_AREA3_MASK);      \
        } else if (first_addr >= ADDR_TEX32_FIRST && last_addr <=       \
                   ADDR_TEX32_LAST) {                                   \
            uint##size##_t tmp;                                         \
            if (pvr2_tex_mem_area32_read(&tmp, addr, sizeof(tmp)) ==    \
                MEM_ACCESS_SUCCESS)                                     \
                return tmp;                                             \
            else                                                        \
                RAISE_ERROR(get_error_pending());                       \
        } else if (first_addr >= ADDR_TEX64_FIRST && last_addr <=       \
                   ADDR_TEX64_LAST) {                                   \
            uint##size##_t tmp;                                         \
            if (pvr2_tex_mem_area64_read(&tmp, addr, sizeof(tmp)) ==    \
                MEM_ACCESS_SUCCESS)                                     \
                return tmp;                                             \
            else                                                        \
                RAISE_ERROR(get_error_pending());                       \
        } else if (addr >= ADDR_AREA0_FIRST && addr <= ADDR_AREA0_LAST) { \
            uint##size##_t tmp;                                         \
            if (read_area0(&tmp, addr, sizeof(tmp)) == MEM_ACCESS_SUCCESS) \
                return tmp;                                             \
            else                                                        \
                RAISE_ERROR(get_error_pending());                       \
        } else if (first_addr >= ADDR_AREA4_FIRST && last_addr <=       \
                   ADDR_AREA4_LAST) {                                   \
            uint##size##_t tmp;                                         \
            if (read_area4(&tmp, addr, sizeof(tmp)) == MEM_ACCESS_SUCCESS) \
                return tmp;                                             \
            else                                                        \
                RAISE_ERROR(get_error_pending());                       \
        }                                                               \
                                                                        \
        error_set_feature("memory mapping");                            \
        error_set_address(addr);                                        \
        error_set_length(sizeof(uint##size##_t));                       \
        RAISE_ERROR(ERROR_UNIMPLEMENTED);                               \
    }

MEMORY_MAP_READ_TMPL(8)
MEMORY_MAP_READ_TMPL(16)
MEMORY_MAP_READ_TMPL(32)

#define MEMORY_MAP_WRITE_TMPL(size)                                     \
    void memory_map_write_##size(uint##size##_t val, size_t addr) {     \
        size_t first_addr = addr;                                       \
        size_t last_addr = sizeof(uint##size##_t) - 1 + first_addr;     \
                                                                        \
        /* check RAM first because that's the case we want to optimize for */ \
        if (first_addr >= ADDR_AREA3_FIRST && last_addr <= ADDR_AREA3_LAST) { \
            memory_write##size(mem, addr & ADDR_AREA3_MASK, val);       \
            return;                                                     \
        } else if (first_addr >= ADDR_TEX32_FIRST && last_addr <=       \
            ADDR_TEX32_LAST) {                                          \
            if (pvr2_tex_mem_area32_write(&val, addr, sizeof(val)) ==   \
            MEM_ACCESS_SUCCESS)                                         \
                return;                                                 \
            else                                                        \
                RAISE_ERROR(get_error_pending());                       \
        } else if (first_addr >= ADDR_TEX64_FIRST && last_addr <=       \
                   ADDR_TEX64_LAST) {                                   \
            if (pvr2_tex_mem_area64_write(&val, addr, sizeof(val)) ==   \
                MEM_ACCESS_SUCCESS)                                     \
                return;                                                 \
            else                                                        \
                RAISE_ERROR(get_error_pending());                       \
        } else if (first_addr >= ADDR_AREA0_FIRST && last_addr <=       \
                   ADDR_AREA0_LAST) {                                   \
            if (write_area0(&val, addr, sizeof(val)) == MEM_ACCESS_SUCCESS) \
                return;                                                 \
            else                                                        \
                RAISE_ERROR(get_error_pending());                       \
        } else if (first_addr >= ADDR_AREA4_FIRST && last_addr <=       \
                   ADDR_AREA4_LAST) {                                   \
            if (write_area4(&val, addr, sizeof(val)) == MEM_ACCESS_SUCCESS) \
                return;                                                 \
            else                                                        \
                RAISE_ERROR(get_error_pending());                       \
        }                                                               \
                                                                        \
        error_set_feature("memory mapping");                            \
        error_set_address(addr);                                        \
        error_set_length(sizeof(val));                                  \
        RAISE_ERROR(ERROR_UNIMPLEMENTED);                               \
    }

MEMORY_MAP_WRITE_TMPL(8)
MEMORY_MAP_WRITE_TMPL(16)
MEMORY_MAP_WRITE_TMPL(32)

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
    } else if (first_addr >= ADDR_AREA4_FIRST && last_addr <= ADDR_AREA4_LAST) {
        return read_area4(buf, addr, len);
    }

    error_set_feature("memory mapping");
    error_set_address(addr);
    error_set_length(len);
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
    } else if (first_addr >= ADDR_AREA4_FIRST && last_addr <= ADDR_AREA4_LAST) {
        return write_area4(buf, addr, len);
    }

    error_set_feature("memory mapping");
    error_set_address(addr);
    error_set_length(len);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static inline int read_area0(void *buf, size_t addr, size_t len) {
    addr32_t addr_orig = addr;
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

    /* when the write is not contained entirely within one mapping */
    error_set_feature("proper response for when the guest writes past a memory "
                      "map's end");
    error_set_length(len);
    error_set_address(addr_orig);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static inline int write_area0(void const *buf, size_t addr, size_t len) {
    addr32_t addr_orig = addr;
    addr &= ADDR_AREA0_MASK;
    size_t first_addr = addr;
    size_t last_addr = addr + (len - 1);

    if (last_addr <= ADDR_BIOS_LAST) {
        /*
         * XXX In case you were wondering: we don't check to see if
         * addr >= ADDR_BIOS_FIRST because ADDR_BIOS_FIRST is 0
         */
        error_set_feature("proper response for when the guest tries to write "
                          "to the bios");
        error_set_length(len);
        error_set_address(addr_orig);
        PENDING_ERROR(ERROR_UNIMPLEMENTED);
        return MEM_ACCESS_FAILURE;
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

    /* when the write is not contained entirely within one mapping */
    error_set_feature("proper response for when the guest writes past a memory "
                      "map's end");
    error_set_length(len);
    error_set_address(addr_orig);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static inline int read_area3(void *buf, size_t addr, size_t len) {
    return memory_read(mem, buf, addr & ADDR_AREA3_MASK, len);
}

static inline int write_area3(void const *buf, size_t addr, size_t len) {
    return memory_write(mem, buf, addr & ADDR_AREA3_MASK, len);
}

static inline int read_area4(void *buf, size_t addr, size_t len) {
    if (addr >= ADDR_TA_FIFO_POLY_FIRST && addr <= ADDR_TA_FIFO_POLY_LAST)
        return pvr2_ta_fifo_poly_read(buf, addr, len);

    error_set_feature("AREA4 readable memory map");
    error_set_length(len);
    error_set_address(addr);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static inline int write_area4(void const *buf, size_t addr, size_t len) {
    if (addr >= ADDR_TA_FIFO_POLY_FIRST && addr <= ADDR_TA_FIFO_POLY_LAST)
        return pvr2_ta_fifo_poly_write(buf, addr, len);

    error_set_feature("AREA4 writable memory map");
    error_set_length(len);
    error_set_address(addr);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}
