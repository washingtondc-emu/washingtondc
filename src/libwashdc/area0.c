/*******************************************************************************
 *
 *
 *    Copyright (C) 2022 snickerbockers
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

#include "hw/boot_rom.h"
#include "hw/flash_mem.h"
#include "hw/sys/sys_block.h"
#include "hw/maple/maple_reg.h"
#include "hw/gdrom/gdrom.h"
#include "hw/pvr2/pvr2_reg.h"
#include "hw/aica/aica.h"
#include "mem_areas.h"
#include "hw/g1/g1_reg.h"
#include "hw/g2/g2_reg.h"
#include "hw/g2/modem.h"
#include "hw/g2/external_dev.h"
#include "hw/aica/aica_rtc.h"
#include "trace_proxy.h"

#include "area0.h"

void area0_init(struct area0 *area,
                struct boot_rom *bios,
                struct flash_mem *flash,
                struct sys_block_ctxt *sys_block,
                struct maple *maple,
                struct gdrom_ctxt *gdrom,
                struct pvr2 *pvr2,
                struct aica *aica,
                struct aica_rtc *rtc,
                washdc_hostfile pvr2_trace_file,
                washdc_hostfile aica_trace_file) {
    memory_map_init(&area->map);

    area->bios = bios;
    area->flash = flash;
    area->sys_block = sys_block;
    area->maple = maple;
    area->gdrom = gdrom;
    area->pvr2 = pvr2;
    area->aica = aica;
    area->rtc = rtc;

    if (pvr2_trace_file != WASHDC_HOSTFILE_INVALID) {
        static struct trace_proxy pvr2_reg_traceproxy;
        trace_proxy_create(&pvr2_reg_traceproxy, pvr2_trace_file,
                           TRACE_SOURCE_SH4, &pvr2_reg_intf, pvr2);
        memory_map_add(&area->map, ADDR_PVR2_FIRST, ADDR_PVR2_LAST,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &trace_proxy_memory_interface, &pvr2_reg_traceproxy);
        memory_map_add(&area->map, ADDR_PVR2_FIRST + 0x02000000, ADDR_PVR2_LAST + 0x02000000,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &trace_proxy_memory_interface, &pvr2_reg_traceproxy);
    } else {
        memory_map_add(&area->map, ADDR_PVR2_FIRST, ADDR_PVR2_LAST,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &pvr2_reg_intf, pvr2);
        memory_map_add(&area->map, ADDR_PVR2_FIRST + 0x02000000, ADDR_PVR2_LAST + 0x02000000,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &pvr2_reg_intf, pvr2);
    }

    if (aica_trace_file  != WASHDC_HOSTFILE_INVALID) {
        static struct trace_proxy aica_mem_traceproxy, aica_reg_traceproxy;
        trace_proxy_create(&aica_mem_traceproxy, aica_trace_file,
                           TRACE_SOURCE_SH4, &aica_wave_mem_intf, &aica->mem);
        trace_proxy_create(&aica_reg_traceproxy, aica_trace_file,
                           TRACE_SOURCE_SH4, &aica_sys_intf, aica);

        memory_map_add(&area->map, ADDR_AICA_WAVE_FIRST, ADDR_AICA_WAVE_LAST,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &trace_proxy_memory_interface, &aica_mem_traceproxy);
        memory_map_add(&area->map, 0x00700000, 0x00707fff,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &trace_proxy_memory_interface, &aica_reg_traceproxy);
        memory_map_add(&area->map, ADDR_AICA_WAVE_FIRST + 0x02000000, ADDR_AICA_WAVE_LAST + 0x02000000,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &trace_proxy_memory_interface, &aica_mem_traceproxy);
        memory_map_add(&area->map, 0x00700000 + 0x02000000, 0x00707fff + 0x02000000,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &trace_proxy_memory_interface, &aica_reg_traceproxy);
    } else {
        memory_map_add(&area->map, ADDR_AICA_WAVE_FIRST, ADDR_AICA_WAVE_LAST,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &aica_wave_mem_intf, &aica->mem);
        memory_map_add(&area->map, 0x00700000, 0x00707fff,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &aica_sys_intf, aica);
        memory_map_add(&area->map, ADDR_AICA_WAVE_FIRST + 0x02000000, ADDR_AICA_WAVE_LAST + 0x02000000,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &aica_wave_mem_intf, &aica->mem);
        memory_map_add(&area->map, 0x00700000 + 0x02000000, 0x00707fff + 0x02000000,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &aica_sys_intf, aica);
    }


    memory_map_add(&area->map, ADDR_BIOS_FIRST, ADDR_BIOS_LAST,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &boot_rom_intf, bios);
    memory_map_add(&area->map, ADDR_FLASH_FIRST, ADDR_FLASH_LAST,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &flash_mem_intf, flash);
    memory_map_add(&area->map, ADDR_G1_FIRST, ADDR_G1_LAST,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &g1_intf, NULL);
    memory_map_add(&area->map, ADDR_SYS_FIRST, ADDR_SYS_LAST,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &sys_block_intf, sys_block);
    memory_map_add(&area->map, ADDR_MAPLE_FIRST, ADDR_MAPLE_LAST,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &maple_intf, maple);
    memory_map_add(&area->map, ADDR_G2_FIRST, ADDR_G2_LAST,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &g2_intf, NULL);
    memory_map_add(&area->map, ADDR_MODEM_FIRST, ADDR_MODEM_LAST,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &modem_intf, NULL);
    memory_map_add(&area->map, ADDR_AICA_RTC_FIRST, ADDR_AICA_RTC_LAST,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_rtc_intf, rtc);
    memory_map_add(&area->map, ADDR_GDROM_FIRST, ADDR_GDROM_LAST,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &gdrom_reg_intf, gdrom);
    memory_map_add(&area->map, ADDR_EXT_DEV_FIRST, ADDR_EXT_DEV_LAST,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &ext_dev_intf, NULL);

    memory_map_add(&area->map, ADDR_BIOS_FIRST + 0x02000000, ADDR_BIOS_LAST + 0x02000000,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &boot_rom_intf, bios);
    memory_map_add(&area->map, ADDR_FLASH_FIRST + 0x02000000, ADDR_FLASH_LAST + 0x02000000,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &flash_mem_intf, flash);
    memory_map_add(&area->map, ADDR_G1_FIRST + 0x02000000, ADDR_G1_LAST + 0x02000000,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &g1_intf, NULL);
    memory_map_add(&area->map, ADDR_SYS_FIRST + 0x02000000, ADDR_SYS_LAST + 0x02000000,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &sys_block_intf, NULL);
    memory_map_add(&area->map, ADDR_MAPLE_FIRST + 0x02000000, ADDR_MAPLE_LAST + 0x02000000,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &maple_intf, NULL);
    memory_map_add(&area->map, ADDR_G2_FIRST + 0x02000000, ADDR_G2_LAST + 0x02000000,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &g2_intf, NULL);
    memory_map_add(&area->map, ADDR_MODEM_FIRST + 0x02000000, ADDR_MODEM_LAST + 0x02000000,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &modem_intf, NULL);
    memory_map_add(&area->map, ADDR_AICA_RTC_FIRST + 0x02000000, ADDR_AICA_RTC_LAST + 0x02000000,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_rtc_intf, rtc);
    memory_map_add(&area->map, ADDR_GDROM_FIRST + 0x02000000, ADDR_GDROM_LAST + 0x02000000,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &gdrom_reg_intf, gdrom);
    memory_map_add(&area->map, ADDR_EXT_DEV_FIRST + 0x02000000, ADDR_EXT_DEV_LAST + 0x02000000,
                   RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                   &ext_dev_intf, NULL);
}

void area0_cleanup(struct area0 *area) {
    memory_map_cleanup(&area->map);
}

#define AREA0_READFUNC(tp, suffix)                          \
    static tp area0_read##suffix(uint32_t addr,             \
                                 void *ctxt) {              \
        struct area0 *area = ctxt;                          \
        return memory_map_read_##suffix(&area->map, addr);  \
    }

#define AREA0_TRY_READFUNC(tp, suffix)          \
    static int area0_try_read##suffix(uint32_t addr, tp *val, void *ctxt) { \
        struct area0 *area = ctxt;                                      \
        return memory_map_try_read_##suffix(&area->map, addr, val);     \
    }

#define AREA0_WRITEFUNC(tp, suffix)                                     \
    static void area0_write##suffix(uint32_t addr, tp val, void *ctxt) { \
        struct area0 *area = ctxt;                                      \
        memory_map_write_##suffix(&area->map, addr, val);               \
    }

#define AREA0_TRY_WRITEFUNC(tp, suffix)                                 \
    static int area0_try_write##suffix(uint32_t addr, tp val, void *ctxt) { \
        struct area0 *area = ctxt;                                      \
        return memory_map_try_write_##suffix(&area->map, addr, val);    \
    }


AREA0_READFUNC(double, double)
AREA0_READFUNC(float, float)
AREA0_READFUNC(uint32_t, 32)
AREA0_READFUNC(uint16_t, 16)
AREA0_READFUNC(uint8_t, 8)

AREA0_TRY_READFUNC(double, double)
AREA0_TRY_READFUNC(float, float)
AREA0_TRY_READFUNC(uint32_t, 32)
AREA0_TRY_READFUNC(uint16_t, 16)
AREA0_TRY_READFUNC(uint8_t, 8)

AREA0_WRITEFUNC(double, double)
AREA0_WRITEFUNC(float, float)
AREA0_WRITEFUNC(uint32_t, 32)
AREA0_WRITEFUNC(uint16_t, 16)
AREA0_WRITEFUNC(uint8_t, 8)

AREA0_TRY_WRITEFUNC(double, double)
AREA0_TRY_WRITEFUNC(float, float)
AREA0_TRY_WRITEFUNC(uint32_t, 32)
AREA0_TRY_WRITEFUNC(uint16_t, 16)
AREA0_TRY_WRITEFUNC(uint8_t, 8)

struct memory_interface area0_intf = {
    .readfloat = area0_readfloat,
    .readdouble = area0_readdouble,
    .read32 = area0_read32,
    .read16 = area0_read16,
    .read8 = area0_read8,

    .try_readfloat = area0_try_readfloat,
    .try_readdouble = area0_try_readdouble,
    .try_read32 = area0_try_read32,
    .try_read16 = area0_try_read16,
    .try_read8 = area0_try_read8,

    .writefloat = area0_writefloat,
    .writedouble = area0_writedouble,
    .write32 = area0_write32,
    .write16 = area0_write16,
    .write8 = area0_write8,

    .try_writefloat = area0_try_writefloat,
    .try_writedouble = area0_try_writedouble,
    .try_write32 = area0_try_write32,
    .try_write16 = area0_try_write16,
    .try_write8 = area0_try_write8
};
