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

#include <stdlib.h>

#include "sh4_excp.h"
#include "sh4_mem.h"
#include "sh4_ocache.h"
#include "sh4.h"
#include "dreamcast.h"
#include "MemoryMap.h"
#include "mem_code.h"
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
#include "hw/sh4/sh4_ocache.h"
#include "BiosFile.h"
#include "flash_memory.h"

#ifdef ENABLE_DEBUGGER
#include "debugger.h"
#endif

static float read_ocache_ram_float(uint32_t addr, void *ctxt);
static double read_ocache_ram_double(uint32_t addr, void *ctxt);
static uint32_t read_ocache_ram_32(uint32_t addr, void *ctxt);
static uint16_t read_ocache_ram_16(uint32_t addr, void *ctxt);
static uint8_t read_ocache_ram_8(uint32_t addr, void *ctxt);

static void write_ocache_ram_float(uint32_t addr, float val, void *ctxt);
static void write_ocache_ram_double(uint32_t addr, double val, void *ctxt);
static void write_ocache_ram_32(uint32_t addr, uint32_t val, void *ctxt);
static void write_ocache_ram_16(uint32_t addr, uint16_t val, void *ctxt);
static void write_ocache_ram_8(uint32_t addr, uint8_t val, void *ctxt);

static float read_sh4_p4_float(uint32_t addr, void *ctxt);
static double read_sh4_p4_double(uint32_t addr, void *ctxt);
static uint32_t read_sh4_p4_32(uint32_t addr, void *ctxt);
static uint16_t read_sh4_p4_16(uint32_t addr, void *ctxt);
static uint8_t read_sh4_p4_8(uint32_t addr, void *ctxt);

static void write_sh4_p4_float(uint32_t addr, float val, void *ctxt);
static void write_sh4_p4_double(uint32_t addr, double val, void *ctxt);
static void write_sh4_p4_32(uint32_t addr, uint32_t val, void *ctxt);
static void write_sh4_p4_16(uint32_t addr, uint16_t val, void *ctxt);
static void write_sh4_p4_8(uint32_t addr, uint8_t val, void *ctxt);

static struct memory_interface sh4_p4_intf = {
    .readdouble = read_sh4_p4_double,
    .readfloat = read_sh4_p4_float,
    .read32 = read_sh4_p4_32,
    .read16 = read_sh4_p4_16,
    .read8 = read_sh4_p4_8,

    .writedouble = write_sh4_p4_double,
    .writefloat = write_sh4_p4_float,
    .write32 = write_sh4_p4_32,
    .write16 = write_sh4_p4_16,
    .write8 = write_sh4_p4_8
};

static struct memory_interface sh4_ora_intf = {
    .readdouble = read_ocache_ram_double,
    .readfloat = read_ocache_ram_float,
    .read32 = read_ocache_ram_32,
    .read16 = read_ocache_ram_16,
    .read8 = read_ocache_ram_8,

    .writedouble = write_ocache_ram_double,
    .writefloat = write_ocache_ram_float,
    .write32 = write_ocache_ram_32,
    .write16 = write_ocache_ram_16,
    .write8 = write_ocache_ram_8
};

static void construct_sh4_mem_map(struct memory_map *map);

void sh4_mem_init(Sh4 *sh4) {
    memory_map_init(&sh4->mem.map);
    construct_sh4_mem_map(&sh4->mem.map);
}

void sh4_mem_cleanup(Sh4 *sh4) {
    memory_map_cleanup(&sh4->mem.map);
}

/*
 * TODO: need to adequately return control to the debugger when there's a memory
 * error and the debugger has its error-handler set up.  longjmp is the obvious
 * solution, but until all the codebase is out of C++ I don't want to risk that.
 */

#define SH4_DO_WRITE_P4_TMPL(type, postfix)                             \
    void sh4_do_write_p4_##postfix(Sh4 *sh4, addr32_t addr, type val) { \
        if ((addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL) {             \
            sh4_sq_write_##postfix(sh4, addr, val);                     \
        } else if (addr >= SH4_P4_REGSTART && addr < SH4_P4_REGEND) {   \
            sh4_write_mem_mapped_reg_##postfix(sh4, addr, val);         \
        } else if (addr >= SH4_OC_ADDR_ARRAY_FIRST &&                   \
                   addr <= SH4_OC_ADDR_ARRAY_LAST) {                    \
            sh4_ocache_write_addr_array_##postfix(sh4, addr, val);  \
        } else {                                                        \
            error_set_address(addr);                                    \
            error_set_length(sizeof(val));                              \
            error_set_feature("writing to part of the P4 memory region"); \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
    }

SH4_DO_WRITE_P4_TMPL(uint8_t, 8)
SH4_DO_WRITE_P4_TMPL(uint16_t, 16)
SH4_DO_WRITE_P4_TMPL(uint32_t, 32)
SH4_DO_WRITE_P4_TMPL(float, float)
SH4_DO_WRITE_P4_TMPL(double, double)

#define SH4_DO_READ_P4_TMPL(type, postfix)                              \
    type sh4_do_read_p4_##postfix(Sh4 *sh4, addr32_t addr) {            \
        type tmp_val;                                                   \
                                                                        \
        if ((addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL) {             \
            return sh4_sq_read_##postfix(sh4, addr);                    \
        } else if (addr >= SH4_P4_REGSTART && addr < SH4_P4_REGEND) {   \
            return sh4_read_mem_mapped_reg_##postfix(sh4, addr);        \
        } else if (addr >= SH4_OC_ADDR_ARRAY_FIRST &&                   \
                   addr <= SH4_OC_ADDR_ARRAY_LAST) {                    \
            return sh4_ocache_read_addr_array_##postfix(sh4, addr);     \
        } else {                                                        \
            error_set_length(sizeof(type));                             \
            error_set_address(addr);                                    \
            error_set_feature("reading from part of the P4 memory region"); \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
                                                                        \
        return tmp_val;                                                 \
    }

SH4_DO_READ_P4_TMPL(uint8_t, 8)
SH4_DO_READ_P4_TMPL(uint16_t, 16)
SH4_DO_READ_P4_TMPL(uint32_t, 32)
SH4_DO_READ_P4_TMPL(float, float)
SH4_DO_READ_P4_TMPL(double, double)

void construct_sh4_mem_map(struct memory_map *map) {
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
    memory_map_add(map, SH4_AREA_P4_FIRST, SH4_AREA_P4_LAST,
                   0xffffffff, 0xffffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &sh4_p4_intf, NULL);
    memory_map_add(map, ADDR_AREA3_FIRST, ADDR_AREA3_LAST,
                   0x1fffffff, ADDR_AREA3_MASK, MEMORY_MAP_REGION_RAM,
                   &ram_intf, &dc_mem);
    memory_map_add(map, ADDR_TEX32_FIRST, ADDR_TEX32_LAST,
                   0x1fffffff, 0x1fffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_tex_mem_area32_intf, NULL);
    memory_map_add(map, ADDR_TEX64_FIRST, ADDR_TEX64_LAST,
                   0x1fffffff, 0x1fffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_tex_mem_area64_intf, NULL);
    memory_map_add(map, ADDR_TA_FIFO_POLY_FIRST, ADDR_TA_FIFO_POLY_LAST,
                   0x1fffffff, 0x1fffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_ta_fifo_intf, NULL);
    memory_map_add(map, SH4_OC_RAM_AREA_FIRST, SH4_OC_RAM_AREA_LAST,
                   0xffffffff, 0xffffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &sh4_ora_intf, NULL);

    /*
     * TODO: everything below here needs to stay at the end so that the
     * masking/mirroring doesn't make it pick up addresses that should
     * belong to other parts of the map.  I need to come up with a better
     * way to implement mirroring.
     */
    memory_map_add(map, ADDR_BIOS_FIRST, ADDR_BIOS_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &bios_file_intf, NULL);
    memory_map_add(map, ADDR_FLASH_FIRST, ADDR_FLASH_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &flash_mem_intf, NULL);
    memory_map_add(map, ADDR_G1_FIRST, ADDR_G1_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &g1_intf, NULL);
    memory_map_add(map, ADDR_SYS_FIRST, ADDR_SYS_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &sys_block_intf, NULL);
    memory_map_add(map, ADDR_MAPLE_FIRST, ADDR_MAPLE_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &maple_intf, NULL);
    memory_map_add(map, ADDR_G2_FIRST, ADDR_G2_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &g2_intf, NULL);
    memory_map_add(map, ADDR_PVR2_FIRST, ADDR_PVR2_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_reg_intf, NULL);
    memory_map_add(map, ADDR_MODEM_FIRST, ADDR_MODEM_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &modem_intf, NULL);
    memory_map_add(map, ADDR_PVR2_CORE_FIRST, ADDR_PVR2_CORE_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_core_reg_intf, NULL);
    memory_map_add(map, ADDR_AICA_FIRST, ADDR_AICA_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_reg_intf, NULL);
    memory_map_add(map, ADDR_AICA_WAVE_FIRST, ADDR_AICA_WAVE_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_wave_mem_intf, NULL);
    memory_map_add(map, ADDR_AICA_RTC_FIRST, ADDR_AICA_RTC_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_rtc_intf, NULL);
    memory_map_add(map, ADDR_GDROM_FIRST, ADDR_GDROM_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &gdrom_reg_intf, NULL);
}

#define READ_OCACHE_RAM_TMPL(type, postfix)                     \
    static type read_ocache_ram_##postfix(uint32_t addr, void *ctxt) {  \
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
    static void write_ocache_ram_##postfix(uint32_t addr, type val,     \
                                           void *ctxt) {                \
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

#define READ_SH4_P4_TMPL(type, postfix)                                 \
    static type read_sh4_p4_##postfix(uint32_t addr, void *ctxt) {      \
        Sh4 *sh4 = dreamcast_get_cpu();                                 \
        return sh4_do_read_p4_##postfix(sh4, addr);                     \
    }

READ_SH4_P4_TMPL(double, double)
READ_SH4_P4_TMPL(float, float)
READ_SH4_P4_TMPL(uint32_t, 32)
READ_SH4_P4_TMPL(uint16_t, 16)
READ_SH4_P4_TMPL(uint8_t, 8)

#define WRITE_SH4_P4_TMPL(type, postfix)                                \
    static void write_sh4_p4_##postfix(uint32_t addr, type val, void *ctxt) { \
        Sh4 *sh4 = dreamcast_get_cpu();                                 \
        sh4_do_write_p4_##postfix(sh4, addr, val);                      \
    }

WRITE_SH4_P4_TMPL(double, double)
WRITE_SH4_P4_TMPL(float, float)
WRITE_SH4_P4_TMPL(uint32_t, 32)
WRITE_SH4_P4_TMPL(uint16_t, 16)
WRITE_SH4_P4_TMPL(uint8_t, 8)
