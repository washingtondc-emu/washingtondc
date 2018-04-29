/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2018 snickerbockers
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
#include "hw/sh4/sh4.h"
#include "hw/sh4/sh4_ocache.h"
#include "dreamcast.h"
#include "flash_memory.h"
#include "error.h"
#include "mem_code.h"
#include "BiosFile.h"

#include "MemoryMap.h"

static struct Memory *mem;

static uint32_t read_area3_32(uint32_t addr);
static uint16_t read_area3_16(uint32_t addr);
static uint8_t read_area3_8(uint32_t addr);
static float read_area3_float(uint32_t addr);
static double read_area3_double(uint32_t addr);

static void write_area3_float(uint32_t addr, float val);
static void write_area3_double(uint32_t addr, double val);
static void write_area3_32(uint32_t addr, uint32_t val);
static void write_area3_16(uint32_t addr, uint16_t val);
static void write_area3_8(uint32_t addr, uint8_t val);

static float read_ocache_ram_float(uint32_t addr);
static double read_ocache_ram_double(uint32_t addr);
static uint32_t read_ocache_ram_32(uint32_t addr);
static uint16_t read_ocache_ram_16(uint32_t addr);
static uint8_t read_ocache_ram_8(uint32_t addr);

static void write_ocache_ram_float(uint32_t addr, float val);
static void write_ocache_ram_double(uint32_t addr, double val);
static void write_ocache_ram_32(uint32_t addr, uint32_t val);
static void write_ocache_ram_16(uint32_t addr, uint16_t val);
static void write_ocache_ram_8(uint32_t addr, uint8_t val);

static float read_sh4_p4_float(uint32_t addr);
static double read_sh4_p4_double(uint32_t addr);
static uint32_t read_sh4_p4_32(uint32_t addr);
static uint16_t read_sh4_p4_16(uint32_t addr);
static uint8_t read_sh4_p4_8(uint32_t addr);

static void write_sh4_p4_float(uint32_t addr, float val);
static void write_sh4_p4_double(uint32_t addr, double val);
static void write_sh4_p4_32(uint32_t addr, uint32_t val);
static void write_sh4_p4_16(uint32_t addr, uint16_t val);
static void write_sh4_p4_8(uint32_t addr, uint8_t val);

#define MEM_MAP_N_REGIONS 19

struct memory_map sh4_mem_map = {
    .regions = {
        /*
         * I don't like the idea of putting SH4_AREA_P4 ahead of AREA3 (memory),
         * but this absolutely needs to be at the front of the list because the
         * only distinction between this and the other memory regions is that the
         * upper three bits of the address are all 1, and for the other regions the
         * upper three bits can be anything as long as they are not all 1.
         *
         * SH4_OC_RAM_AREA is also an SH4 on-chip component but as far as I know
         * nothing else in the dreamcast's memory map overlaps with it; this is why
         * have not also put it at the begging of the regions array.
         */
        {
            .first_addr = SH4_AREA_P4_FIRST,
            .last_addr = SH4_AREA_P4_LAST,
            .mask = 0xffffffff,
            .range_mask = 0xffffffff,

            .read32 = read_sh4_p4_32,
            .read16 = read_sh4_p4_16,
            .read8  = read_sh4_p4_8,
            .readdouble = read_sh4_p4_double,
            .readfloat = read_sh4_p4_float,

            .write32 = write_sh4_p4_32,
            .write16 = write_sh4_p4_16,
            .write8  = write_sh4_p4_8,
            .writedouble = write_sh4_p4_double,
            .writefloat = write_sh4_p4_float
        },
        {
            .first_addr = ADDR_AREA3_FIRST,
            .last_addr = ADDR_AREA3_LAST,
            .mask = ADDR_AREA3_MASK,
            .range_mask = 0x1fffffff,
            .id = MEMORY_MAP_REGION_RAM,

            .read32 = read_area3_32,
            .read16 = read_area3_16,
            .read8 = read_area3_8,
            .readfloat = read_area3_float,
            .readdouble = read_area3_double,

            .write32 = write_area3_32,
            .write16 = write_area3_16,
            .write8 = write_area3_8,
            .writefloat = write_area3_float,
            .writedouble = write_area3_double
        },
        {
            .first_addr = ADDR_TEX32_FIRST,
            .last_addr = ADDR_TEX32_LAST,
            .mask = 0x1fffffff,
            .range_mask = 0x1fffffff,

            .read32 = pvr2_tex_mem_area32_read_32,
            .read16 = pvr2_tex_mem_area32_read_16,
            .read8 = pvr2_tex_mem_area32_read_8,
            .readfloat = pvr2_tex_mem_area32_read_float,
            .readdouble = pvr2_tex_mem_area32_read_double,

            .write32 = pvr2_tex_mem_area32_write_32,
            .write16 = pvr2_tex_mem_area32_write_16,
            .write8 = pvr2_tex_mem_area32_write_8,
            .writefloat = pvr2_tex_mem_area32_write_float,
            .writedouble = pvr2_tex_mem_area32_write_double,
        },
        {
            .first_addr = ADDR_TEX64_FIRST,
            .last_addr = ADDR_TEX64_LAST,
            .mask = 0x1fffffff,
            .range_mask = 0x1fffffff,

            .read32 = pvr2_tex_mem_area64_read_32,
            .read16 = pvr2_tex_mem_area64_read_16,
            .read8 = pvr2_tex_mem_area64_read_8,
            .readfloat = pvr2_tex_mem_area64_read_float,
            .readdouble = pvr2_tex_mem_area64_read_double,

            .write32 = pvr2_tex_mem_area64_write_32,
            .write16 = pvr2_tex_mem_area64_write_16,
            .write8 = pvr2_tex_mem_area64_write_8,
            .writefloat = pvr2_tex_mem_area64_write_float,
            .writedouble = pvr2_tex_mem_area64_write_double
        },
        {
            .first_addr = ADDR_TA_FIFO_POLY_FIRST,
            .last_addr = ADDR_TA_FIFO_POLY_LAST,
            .mask = 0x1fffffff,
            .range_mask = 0x1fffffff,

            .read32 = pvr2_ta_fifo_poly_read_32,
            .read16 = pvr2_ta_fifo_poly_read_16,
            .read8 = pvr2_ta_fifo_poly_read_8,
            .readfloat = pvr2_ta_fifo_poly_read_float,
            .readdouble = pvr2_ta_fifo_poly_read_double,

            .write32 = pvr2_ta_fifo_poly_write_32,
            .write16 = pvr2_ta_fifo_poly_write_16,
            .write8 = pvr2_ta_fifo_poly_write_8,
            .writefloat = pvr2_ta_fifo_poly_write_float,
            .writedouble = pvr2_ta_fifo_poly_write_double
        },
        {
            .first_addr = SH4_OC_RAM_AREA_FIRST,
            .last_addr = SH4_OC_RAM_AREA_LAST,
            .mask = 0xffffffff,
            .range_mask = 0xffffffff,

            .read32 = read_ocache_ram_32,
            .read16 = read_ocache_ram_16,
            .read8 = read_ocache_ram_8,
            .readdouble = read_ocache_ram_double,
            .readfloat = read_ocache_ram_float,

            .write32 = write_ocache_ram_32,
            .write16 = write_ocache_ram_16,
            .write8 = write_ocache_ram_8,
            .writedouble = write_ocache_ram_double,
            .writefloat = write_ocache_ram_float
        },
        /*
         * TODO: everything below here needs to stay at the end so that the
         * masking/mirroring doesn't make it pick up addresses that should
         * belong to other parts of the map.  I need to come up with a better
         * way to implement mirroring.
         */
        {
            .first_addr = ADDR_BIOS_FIRST,
            .last_addr = ADDR_BIOS_LAST,
            .mask = ADDR_AREA0_MASK,
            .range_mask = ADDR_AREA0_MASK,

            .read32 = bios_file_read_32,
            .read16 = bios_file_read_16,
            .read8 = bios_file_read_8,
            .readfloat = bios_file_read_float,
            .readdouble = bios_file_read_double,

            .write32 = bios_file_write_32,
            .write16 = bios_file_write_16,
            .write8 = bios_file_write_8,
            .writefloat = bios_file_write_float,
            .writedouble = bios_file_write_double
        },
        {
            .first_addr = ADDR_FLASH_FIRST,
            .last_addr = ADDR_FLASH_LAST,
            .mask = ADDR_AREA0_MASK,
            .range_mask = ADDR_AREA0_MASK,

            .read32 = flash_mem_read_32,
            .read16 = flash_mem_read_16,
            .read8 = flash_mem_read_8,
            .readfloat = flash_mem_read_float,
            .readdouble = flash_mem_read_double,

            .write32 = flash_mem_write_32,
            .write16 = flash_mem_write_16,
            .write8 = flash_mem_write_8,
            .writefloat = flash_mem_write_float,
            .writedouble = flash_mem_write_double
        },
        {
            .first_addr = ADDR_G1_FIRST,
            .last_addr = ADDR_G1_LAST,
            .mask = ADDR_AREA0_MASK,
            .range_mask = ADDR_AREA0_MASK,

            .read32 = g1_reg_read_32,
            .read16 = g1_reg_read_16,
            .read8 = g1_reg_read_8,
            .readfloat = g1_reg_read_float,
            .readdouble = g1_reg_read_double,

            .write32 = g1_reg_write_32,
            .write16 = g1_reg_write_16,
            .write8 = g1_reg_write_8,
            .writefloat = g1_reg_write_float,
            .writedouble = g1_reg_write_double
        },
        {
            .first_addr = ADDR_SYS_FIRST,
            .last_addr = ADDR_SYS_LAST,
            .mask = ADDR_AREA0_MASK,
            .range_mask = ADDR_AREA0_MASK,

            .read32 = sys_block_read_32,
            .read16 = sys_block_read_16,
            .read8 = sys_block_read_8,
            .readfloat = sys_block_read_float,
            .readdouble = sys_block_read_double,

            .write32 = sys_block_write_32,
            .write16 = sys_block_write_16,
            .write8 = sys_block_write_8,
            .writefloat = sys_block_write_float,
            .writedouble = sys_block_write_double
        },
        {
            .first_addr = ADDR_MAPLE_FIRST,
            .last_addr = ADDR_MAPLE_LAST,
            .mask = ADDR_AREA0_MASK,
            .range_mask = ADDR_AREA0_MASK,

            .read32 = maple_reg_read_32,
            .read16 = maple_reg_read_16,
            .read8 = maple_reg_read_8,
            .readfloat = maple_reg_read_float,
            .readdouble = maple_reg_read_double,

            .write32 = maple_reg_write_32,
            .write16 = maple_reg_write_16,
            .write8 = maple_reg_write_8,
            .writefloat = maple_reg_write_float,
            .writedouble = maple_reg_write_double
        },
        {
            .first_addr = ADDR_G2_FIRST,
            .last_addr = ADDR_G2_LAST,
            .mask = ADDR_AREA0_MASK,
            .range_mask = ADDR_AREA0_MASK,

            .read32 = g2_reg_read_32,
            .read16 = g2_reg_read_16,
            .read8 = g2_reg_read_8,
            .readfloat = g2_reg_read_float,
            .readdouble = g2_reg_read_double,

            .write32 = g2_reg_write_32,
            .write16 = g2_reg_write_16,
            .write8 = g2_reg_write_8,
            .writefloat = g2_reg_write_float,
            .writedouble = g2_reg_write_double
        },
        {
            .first_addr = ADDR_PVR2_FIRST,
            .last_addr = ADDR_PVR2_LAST,
            .mask = ADDR_AREA0_MASK,
            .range_mask = ADDR_AREA0_MASK,

            .read32 = pvr2_reg_read_32,
            .read16 = pvr2_reg_read_16,
            .read8 = pvr2_reg_read_8,
            .readfloat = pvr2_reg_read_float,
            .readdouble = pvr2_reg_read_double,

            .write32 = pvr2_reg_write_32,
            .write16 = pvr2_reg_write_16,
            .write8 = pvr2_reg_write_8,
            .writefloat = pvr2_reg_write_float,
            .writedouble = pvr2_reg_write_double
        },
        {
            .first_addr = ADDR_MODEM_FIRST,
            .last_addr = ADDR_MODEM_LAST,
            .mask = ADDR_AREA0_MASK,
            .range_mask = ADDR_AREA0_MASK,

            .read32 = modem_read_32,
            .read16 = modem_read_16,
            .read8 = modem_read_8,
            .readfloat = modem_read_float,
            .readdouble = modem_read_double,

            .write32 = modem_write_32,
            .write16 = modem_write_16,
            .write8 = modem_write_8,
            .writefloat = modem_write_float,
            .writedouble = modem_write_double
        },
        {
            .first_addr = ADDR_PVR2_CORE_FIRST,
            .last_addr = ADDR_PVR2_CORE_LAST,
            .mask = ADDR_AREA0_MASK,
            .range_mask = ADDR_AREA0_MASK,

            .read32 = pvr2_core_reg_read_32,
            .read16 = pvr2_core_reg_read_16,
            .read8 = pvr2_core_reg_read_8,
            .readfloat = pvr2_core_reg_read_float,
            .readdouble = pvr2_core_reg_read_double,

            .write32 = pvr2_core_reg_write_32,
            .write16 = pvr2_core_reg_write_16,
            .write8 = pvr2_core_reg_write_8,
            .writefloat = pvr2_core_reg_write_float,
            .writedouble = pvr2_core_reg_write_double
        },
        {
            .first_addr = ADDR_AICA_FIRST,
            .last_addr = ADDR_AICA_LAST,
            .mask = ADDR_AREA0_MASK,
            .range_mask = ADDR_AREA0_MASK,

            .read32 = aica_reg_read_32,
            .read16 = aica_reg_read_16,
            .read8 = aica_reg_read_8,
            .readfloat = aica_reg_read_float,
            .readdouble = aica_reg_read_double,

            .write32 = aica_reg_write_32,
            .write16 = aica_reg_write_16,
            .write8 = aica_reg_write_8,
            .writefloat = aica_reg_write_float,
            .writedouble = aica_reg_write_double
        },
        {
            .first_addr = ADDR_AICA_WAVE_FIRST,
            .last_addr = ADDR_AICA_WAVE_LAST,
            .mask = ADDR_AREA0_MASK,
            .range_mask = ADDR_AREA0_MASK,

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
        },
        {
            .first_addr = ADDR_AICA_RTC_FIRST,
            .last_addr = ADDR_AICA_RTC_LAST,
            .mask = ADDR_AREA0_MASK,
            .range_mask = ADDR_AREA0_MASK,

            .read32 = aica_rtc_read_32,
            .read16 = aica_rtc_read_16,
            .read8 = aica_rtc_read_8,
            .readfloat = aica_rtc_read_float,
            .readdouble = aica_rtc_read_double,

            .write32 = aica_rtc_write_32,
            .write16 = aica_rtc_write_16,
            .write8 = aica_rtc_write_8,
            .writefloat = aica_rtc_write_float,
            .writedouble = aica_rtc_write_double
        },
        {
            .first_addr = ADDR_GDROM_FIRST,
            .last_addr = ADDR_GDROM_LAST,
            .mask = ADDR_AREA0_MASK,
            .range_mask = ADDR_AREA0_MASK,

            .read32 = gdrom_reg_read_32,
            .read16 = gdrom_reg_read_16,
            .read8 = gdrom_reg_read_8,
            .readfloat = gdrom_reg_read_float,
            .readdouble = gdrom_reg_read_double,

            .write32 = gdrom_reg_write_32,
            .write16 = gdrom_reg_write_16,
            .write8 = gdrom_reg_write_8,
            .writefloat = gdrom_reg_write_float,
            .writedouble = gdrom_reg_write_double
        }
    },
    .n_regions = MEM_MAP_N_REGIONS
};

void memory_map_init(struct Memory *mem_new) {
    memory_map_set_mem(mem_new);
}

void memory_map_set_mem(struct Memory *mem_new) {
    mem = mem_new;
}

#define MEMORY_MAP_READ_TMPL(type, type_postfix)                        \
    type memory_map_read_##type_postfix(struct memory_map *map,         \
                                        uint32_t addr) {                \
        uint32_t first_addr = addr;                                     \
        uint32_t last_addr = sizeof(type) - 1 + first_addr;             \
                                                                        \
        unsigned region_no;                                             \
        for (region_no = 0; region_no < map->n_regions; region_no++) {  \
            uint32_t range_mask = map->regions[region_no].range_mask;   \
            if ((first_addr & range_mask) >= map->regions[region_no].first_addr && \
                (last_addr & range_mask) <= map->regions[region_no].last_addr) { \
                uint32_t mask = map->regions[region_no].mask;           \
                return map->regions[region_no].read##type_postfix(addr & mask); \
            }                                                           \
        }                                                               \
                                                                        \
        error_set_feature("memory mapping");                            \
        error_set_address(addr);                                        \
        error_set_length(sizeof(type));                                 \
        RAISE_ERROR(ERROR_UNIMPLEMENTED);                               \
    }

MEMORY_MAP_READ_TMPL(uint8_t, 8)
MEMORY_MAP_READ_TMPL(uint16_t, 16)
MEMORY_MAP_READ_TMPL(uint32_t, 32)
MEMORY_MAP_READ_TMPL(float, float)
MEMORY_MAP_READ_TMPL(double, double)

#define MEMORY_MAP_TRY_READ_TMPL(type, type_postfix)                    \
    int memory_map_try_read_##type_postfix(struct memory_map *map,      \
                                           uint32_t addr, type *val) {  \
        uint32_t first_addr = addr;                                     \
        uint32_t last_addr = sizeof(type) - 1 + first_addr;             \
                                                                        \
        unsigned region_no;                                             \
        for (region_no = 0; region_no < map->n_regions; region_no++) {  \
            uint32_t range_mask = map->regions[region_no].range_mask;   \
            if ((first_addr & range_mask) >= map->regions[region_no].first_addr && \
                (last_addr & range_mask) <= map->regions[region_no].last_addr) { \
                uint32_t mask = map->regions[region_no].mask;           \
                *val = map->regions[region_no].read##type_postfix(addr & mask); \
                return 0;                                               \
            }                                                           \
        }                                                               \
                                                                        \
        return 1;                                                       \
    }

MEMORY_MAP_TRY_READ_TMPL(uint8_t, 8)
MEMORY_MAP_TRY_READ_TMPL(uint16_t, 16)
MEMORY_MAP_TRY_READ_TMPL(uint32_t, 32)
MEMORY_MAP_TRY_READ_TMPL(float, float)
MEMORY_MAP_TRY_READ_TMPL(double, double)

#define MEM_MAP_WRITE_TMPL(type, type_postfix)                          \
    void memory_map_write_##type_postfix(struct memory_map *map,        \
                                         uint32_t addr, type val) {     \
        uint32_t first_addr = addr;                                     \
        uint32_t last_addr = sizeof(type) - 1 + first_addr;             \
                                                                        \
        unsigned region_no;                                             \
        for (region_no = 0; region_no < map->n_regions; region_no++) {  \
            uint32_t range_mask = map->regions[region_no].range_mask;   \
            if ((first_addr & range_mask) >= map->regions[region_no].first_addr && \
                (last_addr & range_mask) <= map->regions[region_no].last_addr) { \
                uint32_t mask = map->regions[region_no].mask;           \
                map->regions[region_no].write##type_postfix(addr & mask, val); \
                return;                                                 \
            }                                                           \
        }                                                               \
        error_set_feature("memory mapping");                            \
        error_set_address(addr);                                        \
        error_set_length(sizeof(val));                                  \
        RAISE_ERROR(ERROR_UNIMPLEMENTED);                               \
    }                                                                   \

MEM_MAP_WRITE_TMPL(uint8_t, 8)
MEM_MAP_WRITE_TMPL(uint16_t, 16)
MEM_MAP_WRITE_TMPL(uint32_t, 32)
MEM_MAP_WRITE_TMPL(float, float)
MEM_MAP_WRITE_TMPL(double, double)

#define MEM_MAP_TRY_WRITE_TMPL(type, type_postfix)                      \
    int memory_map_try_write_##type_postfix(struct memory_map *map,     \
                                            uint32_t addr, type val) {  \
        uint32_t first_addr = addr;                                     \
        uint32_t last_addr = sizeof(type) - 1 + first_addr;             \
                                                                        \
        unsigned region_no;                                             \
        for (region_no = 0; region_no < map->n_regions; region_no++) {  \
            uint32_t range_mask = map->regions[region_no].range_mask;   \
            if ((first_addr & range_mask) >= map->regions[region_no].first_addr && \
                (last_addr & range_mask) <= map->regions[region_no].last_addr) { \
                uint32_t mask = map->regions[region_no].mask;           \
                map->regions[region_no].write##type_postfix(addr & mask, val); \
                return 0;                                               \
            }                                                           \
        }                                                               \
        return 1;                                                       \
    }                                                                   \

MEM_MAP_TRY_WRITE_TMPL(uint8_t, 8)
MEM_MAP_TRY_WRITE_TMPL(uint16_t, 16)
MEM_MAP_TRY_WRITE_TMPL(uint32_t, 32)
MEM_MAP_TRY_WRITE_TMPL(float, float)
MEM_MAP_TRY_WRITE_TMPL(double, double)

static float read_area3_float(uint32_t addr) {
    return memory_read_float(mem, addr);
}

static double read_area3_double(uint32_t addr) {
    return memory_read_double(mem, addr);
}

static uint32_t read_area3_32(uint32_t addr) {
    return memory_read_32(mem, addr);
}

static uint16_t read_area3_16(uint32_t addr) {
    return memory_read_16(mem, addr);
}

static uint8_t read_area3_8(uint32_t addr) {
    return memory_read_8(mem, addr);
}

static void write_area3_float(uint32_t addr, float val) {
    memory_write_float(mem, addr, val);
}

static void write_area3_double(uint32_t addr, double val) {
    memory_write_double(mem, addr, val);
}

static void write_area3_32(uint32_t addr, uint32_t val) {
    memory_write_32(mem, addr, val);
}

static void write_area3_16(uint32_t addr, uint16_t val) {
    memory_write_16(mem, addr, val);
}

static void write_area3_8(uint32_t addr, uint8_t val) {
    memory_write_8(mem, addr, val);
}

#define READ_OCACHE_RAM_TMPL(type, postfix)                     \
    static type read_ocache_ram_##postfix(uint32_t addr) {      \
        Sh4 *sh4 = dreamcast_get_cpu();                         \
        if (!(sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) ||      \
            !(sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK) ||      \
            !sh4_ocache_in_ram_area(addr)) {                    \
            error_set_address(addr);                            \
            RAISE_ERROR(ERROR_INTEGRITY);                       \
        }                                                       \
        return sh4_ocache_do_read_ora_##postfix(sh4, addr);     \
    }

READ_OCACHE_RAM_TMPL(double, double)
READ_OCACHE_RAM_TMPL(float, float)
READ_OCACHE_RAM_TMPL(uint32_t, 32)
READ_OCACHE_RAM_TMPL(uint16_t, 16)
READ_OCACHE_RAM_TMPL(uint8_t, 8)

#define WRITE_OCACHE_RAM_TMPL(type, postfix)                            \
    static void write_ocache_ram_##postfix(uint32_t addr, type val) {   \
        Sh4 *sh4 = dreamcast_get_cpu();                                 \
        if (!(sh4->reg[SH4_REG_CCR] & SH4_CCR_OCE_MASK) ||              \
            !(sh4->reg[SH4_REG_CCR] & SH4_CCR_ORA_MASK) ||              \
            !sh4_ocache_in_ram_area(addr)) {                            \
            error_set_address(addr);                                    \
            RAISE_ERROR(ERROR_INTEGRITY);                               \
        }                                                               \
        sh4_ocache_do_write_ora_##postfix(sh4, addr, val);              \
    }

WRITE_OCACHE_RAM_TMPL(double, double)
WRITE_OCACHE_RAM_TMPL(float, float)
WRITE_OCACHE_RAM_TMPL(uint32_t, 32)
WRITE_OCACHE_RAM_TMPL(uint16_t, 16)
WRITE_OCACHE_RAM_TMPL(uint8_t, 8)

#define READ_SH4_P4_TMPL(type, postfix)                 \
    static type read_sh4_p4_##postfix(uint32_t addr) {  \
        Sh4 *sh4 = dreamcast_get_cpu();                 \
        return sh4_do_read_p4_##postfix(sh4, addr);     \
    }

READ_SH4_P4_TMPL(double, double)
READ_SH4_P4_TMPL(float, float)
READ_SH4_P4_TMPL(uint32_t, 32)
READ_SH4_P4_TMPL(uint16_t, 16)
READ_SH4_P4_TMPL(uint8_t, 8)

#define WRITE_SH4_P4_TMPL(type, postfix)                                \
    static void write_sh4_p4_##postfix(uint32_t addr, type val) {       \
        Sh4 *sh4 = dreamcast_get_cpu();                                 \
        sh4_do_write_p4_##postfix(sh4, addr, val);                      \
    }

WRITE_SH4_P4_TMPL(double, double)
WRITE_SH4_P4_TMPL(float, float)
WRITE_SH4_P4_TMPL(uint32_t, 32)
WRITE_SH4_P4_TMPL(uint16_t, 16)
WRITE_SH4_P4_TMPL(uint8_t, 8)
