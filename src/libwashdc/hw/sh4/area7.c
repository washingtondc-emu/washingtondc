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

#include "sh4_mem.h"
#include "sh4_ocache.h"

#include "area7.h"

void area7_init(struct area7 *area7, struct Sh4 *sh4) {
    area7->sh4 = sh4;

    memory_map_init(&area7->map);
}

void area7_cleanup(struct area7 *area7) {
    memory_map_cleanup(&area7->map);
}

#define AREA7_READFUNC(tp, suffix)                                      \
    static tp area7_read##suffix(uint32_t addr, void *ctxt) {           \
        struct area7 *area7 = ctxt;                                     \
        if (addr >= SH4_AREA_P4_FIRST && addr <= SH4_AREA_P4_LAST) {    \
            return sh4_p4_intf.read##suffix(addr, area7->sh4);          \
        } else if (addr >= 0x7c000000 && addr <= 0x7fffffff) {          \
            return sh4_ora_intf.read##suffix(addr, area7->sh4);         \
        } else {                                                        \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
    }

#define AREA7_TRY_READFUNC(tp, suffix)                                  \
    static int area7_try_read##suffix(uint32_t addr, tp *val, void *ctxt) { \
        struct area7 *area7 = ctxt;                                     \
        if (addr >= SH4_AREA_P4_FIRST && addr <= SH4_AREA_P4_LAST) {   \
            return sh4_p4_intf.try_read##suffix(addr, val, area7->sh4); \
        } else if (addr >= 0x7c000000 && addr <= 0x7fffffff) {         \
            return sh4_ora_intf.try_read##suffix(addr, val, area7->sh4); \
        } else {                                                        \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
    }


#define AREA7_WRITEFUNC(tp, suffix)                                     \
    static void area7_write##suffix(uint32_t addr, tp val, void *ctxt) { \
        struct area7 *area7 = ctxt;                                     \
        if (addr >= SH4_AREA_P4_FIRST && addr <= SH4_AREA_P4_LAST) {   \
            sh4_p4_intf.write##suffix(addr, val, area7->sh4);           \
        } else if (addr >= 0x7c000000 && addr <= 0x7fffffff) {          \
            sh4_ora_intf.write##suffix(addr, val, area7->sh4);          \
        }                                                               \
    }

#define AREA7_TRY_WRITEFUNC(tp, suffix)                                 \
    static int area7_try_write##suffix(uint32_t addr, tp val, void *ctxt) { \
        struct area7 *area7 = ctxt;                                     \
        if (addr >= SH4_AREA_P4_FIRST && addr <= SH4_AREA_P4_LAST) {   \
            return sh4_p4_intf.try_write##suffix(addr, val, area7->sh4); \
        } else if (addr >= 0x7c000000 && addr <= 0x7fffffff) {         \
            return sh4_ora_intf.try_write##suffix(addr, val, area7->sh4); \
        } else {                                                        \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
    }

AREA7_READFUNC(double, double)
AREA7_READFUNC(float, float)
AREA7_READFUNC(uint32_t, 32)
AREA7_READFUNC(uint16_t, 16)
AREA7_READFUNC(uint8_t, 8)

AREA7_TRY_READFUNC(double, double)
AREA7_TRY_READFUNC(float, float)
AREA7_TRY_READFUNC(uint32_t, 32)
AREA7_TRY_READFUNC(uint16_t, 16)
AREA7_TRY_READFUNC(uint8_t, 8)

AREA7_WRITEFUNC(double, double)
AREA7_WRITEFUNC(float, float)
AREA7_WRITEFUNC(uint32_t, 32)
AREA7_WRITEFUNC(uint16_t, 16)
AREA7_WRITEFUNC(uint8_t, 8)

AREA7_TRY_WRITEFUNC(double, double)
AREA7_TRY_WRITEFUNC(float, float)
AREA7_TRY_WRITEFUNC(uint32_t, 32)
AREA7_TRY_WRITEFUNC(uint16_t, 16)
AREA7_TRY_WRITEFUNC(uint8_t, 8)

struct memory_interface area7_intf = {
    .readfloat = area7_readfloat,
    .readdouble = area7_readdouble,
    .read32 = area7_read32,
    .read16 = area7_read16,
    .read8 = area7_read8,

    .try_readfloat = area7_try_readfloat,
    .try_readdouble = area7_try_readdouble,
    .try_read32 = area7_try_read32,
    .try_read16 = area7_try_read16,
    .try_read8 = area7_try_read8,

    .writefloat = area7_writefloat,
    .writedouble = area7_writedouble,
    .write32 = area7_write32,
    .write16 = area7_write16,
    .write8 = area7_write8,

    .try_writefloat = area7_try_writefloat,
    .try_writedouble = area7_try_writedouble,
    .try_write32 = area7_try_write32,
    .try_write16 = area7_try_write16,
    .try_write8 = area7_try_write8
};
