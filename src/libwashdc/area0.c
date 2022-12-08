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
        area->pvr2_memory_interface = &trace_proxy_memory_interface;
        area->argp = &pvr2_reg_traceproxy;
    } else {
        area->pvr2_memory_interface = &pvr2_reg_intf;
        area->argp = pvr2;
    }
}

void area0_cleanup(struct area0 *area) {
}

#define AREA0_READFUNC(tp, suffix)                                      \
    static tp area0_read##suffix(uint32_t addr,                         \
                                 void *ctxt) {                          \
        struct area0 *area = ctxt;                                      \
        uint32_t addr_bus = addr & 0x01ffffff;                          \
        if (addr_bus >= ADDR_PVR2_FIRST && addr_bus <= ADDR_PVR2_LAST) { \
            return area->pvr2_memory_interface->read##suffix(addr, area->argp); \
        } else if (addr_bus >= ADDR_BIOS_FIRST && addr_bus <= ADDR_BIOS_LAST) { \
            return boot_rom_intf.read##suffix(addr, area->bios);        \
        } else if (addr_bus >= ADDR_FLASH_FIRST && addr_bus <= ADDR_FLASH_LAST) { \
            return flash_mem_intf.read##suffix(addr, area->flash);      \
        } else if (addr_bus >= ADDR_G1_FIRST && addr_bus <= ADDR_G1_LAST) { \
            return g1_intf.read##suffix(addr, NULL);                    \
        } else if (addr_bus >= ADDR_SYS_FIRST && addr_bus <= ADDR_SYS_LAST) { \
            return sys_block_intf.read##suffix(addr, area->sys_block);  \
        } else if (addr_bus >= ADDR_MAPLE_FIRST && addr_bus <= ADDR_MAPLE_LAST) { \
            return maple_intf.read##suffix(addr, area->maple);          \
        } else if (addr_bus >= ADDR_G2_FIRST && addr_bus <= ADDR_G2_LAST) { \
            return g2_intf.read##suffix(addr, NULL);                    \
        } else if (addr_bus >= ADDR_MODEM_FIRST && addr_bus <= ADDR_MODEM_LAST) { \
            return modem_intf.read##suffix(addr, NULL);                 \
        } else if (addr_bus >= ADDR_AICA_WAVE_FIRST && addr_bus <= ADDR_AICA_WAVE_LAST) { \
            return aica_wave_mem_intf.read##suffix(addr, &area->aica->mem); \
        } else if (addr_bus >= 0x00700000 && addr_bus <= 0x00707fff) {  \
            return aica_sys_intf.read##suffix(addr, area->aica);        \
        } else if (addr_bus >= ADDR_AICA_RTC_FIRST && addr_bus <= ADDR_AICA_RTC_LAST) { \
            return aica_rtc_intf.read##suffix(addr, area->rtc);         \
        } else if (addr_bus >= ADDR_GDROM_FIRST && addr_bus <= ADDR_GDROM_LAST) { \
            return gdrom_reg_intf.read##suffix(addr, area->gdrom);      \
        } else if (addr_bus >= ADDR_EXT_DEV_FIRST && addr_bus <= ADDR_EXT_DEV_LAST) { \
            return ext_dev_intf.read##suffix(addr, NULL);               \
        } else {                                                        \
            error_set_address(addr);                                    \
            error_set_length(sizeof(tp));                               \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
    }

#define AREA0_TRY_READFUNC(tp, suffix)          \
    static int area0_try_read##suffix(uint32_t addr, tp *val, void *ctxt) { \
        struct area0 *area = ctxt;                                      \
        uint32_t addr_bus = addr & 0x01ffffff;                          \
        if (addr_bus >= ADDR_PVR2_FIRST && addr_bus <= ADDR_PVR2_LAST) { \
            return area->pvr2_memory_interface->try_read##suffix(addr, val, area->argp); \
        } else if (addr_bus >= ADDR_BIOS_FIRST && addr_bus <= ADDR_BIOS_LAST) { \
            return boot_rom_intf.try_read##suffix(addr, val, area->bios);        \
        } else if (addr_bus >= ADDR_FLASH_FIRST && addr_bus <= ADDR_FLASH_LAST) { \
            return flash_mem_intf.try_read##suffix(addr, val, area->flash);      \
        } else if (addr_bus >= ADDR_G1_FIRST && addr_bus <= ADDR_G1_LAST) { \
            return g1_intf.try_read##suffix(addr, val, NULL);                    \
        } else if (addr_bus >= ADDR_SYS_FIRST && addr_bus <= ADDR_SYS_LAST) { \
            return sys_block_intf.try_read##suffix(addr, val, area->sys_block);  \
        } else if (addr_bus >= ADDR_MAPLE_FIRST && addr_bus <= ADDR_MAPLE_LAST) { \
            return maple_intf.try_read##suffix(addr, val, area->maple);          \
        } else if (addr_bus >= ADDR_G2_FIRST && addr_bus <= ADDR_G2_LAST) { \
            return g2_intf.try_read##suffix(addr, val, NULL);                    \
        } else if (addr_bus >= ADDR_MODEM_FIRST && addr_bus <= ADDR_MODEM_LAST) { \
            return modem_intf.try_read##suffix(addr, val, NULL);                 \
        } else if (addr_bus >= ADDR_AICA_WAVE_FIRST && addr_bus <= ADDR_AICA_WAVE_LAST) { \
            return aica_wave_mem_intf.try_read##suffix(addr, val, &area->aica->mem); \
        } else if (addr_bus >= 0x00700000 && addr_bus <= 0x00707fff) {  \
            return aica_sys_intf.try_read##suffix(addr, val, area->aica);        \
        } else if (addr_bus >= ADDR_AICA_RTC_FIRST && addr_bus <= ADDR_AICA_RTC_LAST) { \
            return aica_rtc_intf.try_read##suffix(addr, val, area->rtc);         \
        } else if (addr_bus >= ADDR_GDROM_FIRST && addr_bus <= ADDR_GDROM_LAST) { \
            return gdrom_reg_intf.try_read##suffix(addr, val, area->gdrom);      \
        } else if (addr_bus >= ADDR_EXT_DEV_FIRST && addr_bus <= ADDR_EXT_DEV_LAST) { \
            return ext_dev_intf.try_read##suffix(addr, val, NULL);               \
        } else {                                                        \
            return -1;                                                  \
        }                                                               \
    }

#define AREA0_WRITEFUNC(tp, suffix)                                     \
    static void area0_write##suffix(uint32_t addr, tp val, void *ctxt) { \
        struct area0 *area = ctxt;                                      \
        uint32_t addr_bus = addr & 0x01ffffff;                          \
        if (addr_bus >= ADDR_PVR2_FIRST && addr_bus <= ADDR_PVR2_LAST) { \
            area->pvr2_memory_interface->write##suffix(addr, val, area->argp); \
        } else if (addr_bus >= ADDR_BIOS_FIRST && addr_bus <= ADDR_BIOS_LAST) { \
            boot_rom_intf.write##suffix(addr, val, area->bios);         \
        } else if (addr_bus >= ADDR_FLASH_FIRST && addr_bus <= ADDR_FLASH_LAST) { \
            flash_mem_intf.write##suffix(addr, val, area->flash);       \
        } else if (addr_bus >= ADDR_G1_FIRST && addr_bus <= ADDR_G1_LAST) { \
            g1_intf.write##suffix(addr, val, NULL);                     \
        } else if (addr_bus >= ADDR_SYS_FIRST && addr_bus <= ADDR_SYS_LAST) { \
            sys_block_intf.write##suffix(addr, val, area->sys_block);   \
        } else if (addr_bus >= ADDR_MAPLE_FIRST && addr_bus <= ADDR_MAPLE_LAST) { \
            maple_intf.write##suffix(addr, val, area->maple);           \
        } else if (addr_bus >= ADDR_G2_FIRST && addr_bus <= ADDR_G2_LAST) { \
            g2_intf.write##suffix(addr, val, NULL);                     \
        } else if (addr_bus >= ADDR_MODEM_FIRST && addr_bus <= ADDR_MODEM_LAST) { \
            modem_intf.write##suffix(addr, val, NULL);                  \
        } else if (addr_bus >= ADDR_AICA_WAVE_FIRST && addr_bus <= ADDR_AICA_WAVE_LAST) { \
            aica_wave_mem_intf.write##suffix(addr, val, &area->aica->mem); \
        } else if (addr_bus >= 0x00700000 && addr_bus <= 0x00707fff) {  \
            aica_sys_intf.write##suffix(addr, val, area->aica);         \
        } else if (addr_bus >= ADDR_AICA_RTC_FIRST && addr_bus <= ADDR_AICA_RTC_LAST) { \
            aica_rtc_intf.write##suffix(addr, val, area->rtc);          \
        } else if (addr_bus >= ADDR_GDROM_FIRST && addr_bus <= ADDR_GDROM_LAST) { \
            gdrom_reg_intf.write##suffix(addr, val, area->gdrom);       \
        } else if (addr_bus >= ADDR_EXT_DEV_FIRST && addr_bus <= ADDR_EXT_DEV_LAST) { \
            ext_dev_intf.write##suffix(addr, val, NULL);                \
        } else {                                                        \
            error_set_address(addr);                                    \
            error_set_length(sizeof(tp));                               \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
    }

#define AREA0_TRY_WRITEFUNC(tp, suffix)                                 \
    static int area0_try_write##suffix(uint32_t addr, tp val, void *ctxt) { \
        struct area0 *area = ctxt;                                      \
        uint32_t addr_bus = addr & 0x01ffffff;                          \
        if (addr_bus >= ADDR_PVR2_FIRST && addr_bus <= ADDR_PVR2_LAST) { \
            return area->pvr2_memory_interface->try_write##suffix(addr, val, area->argp); \
        } else if (addr_bus >= ADDR_BIOS_FIRST && addr_bus <= ADDR_BIOS_LAST) { \
            return boot_rom_intf.try_write##suffix(addr, val, area->bios);        \
        } else if (addr_bus >= ADDR_FLASH_FIRST && addr_bus <= ADDR_FLASH_LAST) { \
            return flash_mem_intf.try_write##suffix(addr, val, area->flash);      \
        } else if (addr_bus >= ADDR_G1_FIRST && addr_bus <= ADDR_G1_LAST) { \
            return g1_intf.try_write##suffix(addr, val, NULL);                    \
        } else if (addr_bus >= ADDR_SYS_FIRST && addr_bus <= ADDR_SYS_LAST) { \
            return sys_block_intf.try_write##suffix(addr, val, area->sys_block);  \
        } else if (addr_bus >= ADDR_MAPLE_FIRST && addr_bus <= ADDR_MAPLE_LAST) { \
            return maple_intf.try_write##suffix(addr, val, area->maple);          \
        } else if (addr_bus >= ADDR_G2_FIRST && addr_bus <= ADDR_G2_LAST) { \
            return g2_intf.try_write##suffix(addr, val, NULL);                    \
        } else if (addr_bus >= ADDR_MODEM_FIRST && addr_bus <= ADDR_MODEM_LAST) { \
            return modem_intf.try_write##suffix(addr, val, NULL);                 \
        } else if (addr_bus >= ADDR_AICA_WAVE_FIRST && addr_bus <= ADDR_AICA_WAVE_LAST) { \
            return aica_wave_mem_intf.try_write##suffix(addr, val, &area->aica->mem); \
        } else if (addr_bus >= 0x00700000 && addr_bus <= 0x00707fff) {  \
            return aica_sys_intf.try_write##suffix(addr, val, area->aica);        \
        } else if (addr_bus >= ADDR_AICA_RTC_FIRST && addr_bus <= ADDR_AICA_RTC_LAST) { \
            return aica_rtc_intf.try_write##suffix(addr, val, area->rtc);         \
        } else if (addr_bus >= ADDR_GDROM_FIRST && addr_bus <= ADDR_GDROM_LAST) { \
            return gdrom_reg_intf.try_write##suffix(addr, val, area->gdrom);      \
        } else if (addr_bus >= ADDR_EXT_DEV_FIRST && addr_bus <= ADDR_EXT_DEV_LAST) { \
            return ext_dev_intf.try_write##suffix(addr, val, NULL);               \
        } else {                                                        \
            return -1;                                                  \
        }                                                               \
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
