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

#include "trace_proxy.h"
#include "hw/pvr2/pvr2_tex_mem.h"
#include "washdc/error.h"

#include "area1.h"

void area1_init(struct area1 *area1, struct pvr2 *pvr2,
                washdc_hostfile pvr2_trace_file) {
    area1->pvr2 = pvr2;

    if (pvr2_trace_file != WASHDC_HOSTFILE_INVALID) {
        static struct trace_proxy pvr2_mem_32bit_traceproxy,
            pvr2_mem_64bit_traceproxy;
        trace_proxy_create(&pvr2_mem_32bit_traceproxy, pvr2_trace_file,
                           TRACE_SOURCE_SH4, &pvr2_tex_mem_area32_intf, pvr2);
        trace_proxy_create(&pvr2_mem_64bit_traceproxy, pvr2_trace_file,
                           TRACE_SOURCE_SH4, &pvr2_tex_mem_area64_intf, pvr2);

        area1->tex_mem_32bit_intf = &trace_proxy_memory_interface;
        area1->tex_mem_32bit_argp = &pvr2_mem_32bit_traceproxy;
        area1->tex_mem_64bit_intf = &trace_proxy_memory_interface;
        area1->tex_mem_64bit_argp = &pvr2_mem_64bit_traceproxy;
    } else {
        area1->tex_mem_32bit_intf = &pvr2_tex_mem_area32_intf;
        area1->tex_mem_32bit_argp = pvr2;
        area1->tex_mem_64bit_intf = &pvr2_tex_mem_area64_intf;
        area1->tex_mem_64bit_argp = pvr2;
    }
}

void area1_cleanup(struct area1 *area1) {
}

#define AREA1_READFUNC(tp, suffix)                                      \
    static tp area1_read##suffix(uint32_t addr,                         \
                                 void *ctxt) {                          \
        struct area1 *area = ctxt;                                      \
        uint32_t addr_ext = addr & RANGE_MASK_EXT;                      \
        if ((addr_ext >= 0x04000000 && addr_ext <= 0x047fffff) ||       \
            (addr_ext >= 0x06000000 && addr_ext <= 0x067fffff)) {       \
            return area->tex_mem_64bit_intf->read##suffix(addr,         \
                                                          area->tex_mem_64bit_argp); \
        } else if ((addr_ext >= 0x05000000 && addr_ext <= 0x057fffff) || \
                   (addr_ext >= 0x07000000 && addr_ext <= 0x077fffff)) { \
            return area->tex_mem_32bit_intf->read##suffix(addr,         \
                                                          area->tex_mem_32bit_argp); \
        } else if ((addr_ext >= 0x04800000 && addr_ext <= 0x04ffffff) || \
                   (addr_ext >= 0x05800000 && addr_ext <= 0x05ffffff) ||   \
                   (addr_ext >= 0x06800000 && addr_ext <= 0x06ffffff) || \
                   (addr_ext >= 0x07800000 && addr_ext <= 0x07ffffff)) { \
            return pvr2_tex_mem_unused_intf.read##suffix(addr,NULL);   \
        } else {                                                        \
            error_set_address(addr);                                    \
            error_set_length(sizeof(tp));                               \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
    }

#define AREA1_TRY_READFUNC(tp, suffix)                                  \
    static int area1_try_read##suffix(uint32_t addr, tp *val, void *ctxt) { \
        struct area1 *area = ctxt;                                      \
        uint32_t addr_ext = addr & RANGE_MASK_EXT;                      \
        if ((addr_ext >= 0x04000000 && addr_ext <= 0x047fffff) ||       \
            (addr_ext >= 0x06000000 && addr_ext <= 0x067fffff)) {       \
            *val = area->tex_mem_64bit_intf->read##suffix(addr,         \
                                                          area->tex_mem_64bit_argp); \
            return 0;                                                   \
        } else if ((addr_ext >= 0x05000000 && addr_ext <= 0x057fffff) || \
                   (addr_ext >= 0x07000000 && addr_ext <= 0x077fffff)) { \
            *val = area->tex_mem_32bit_intf->read##suffix(addr,         \
                                                          area->tex_mem_32bit_argp); \
            return 0;                                                   \
        } else if ((addr_ext >= 0x04800000 && addr_ext <= 0x04ffffff) || \
                   (addr_ext >= 0x05800000 && addr_ext <= 0x05ffffff) || \
                   (addr_ext >= 0x06800000 && addr_ext <= 0x06ffffff) || \
                   (addr_ext >= 0x07800000 && addr_ext <= 0x07ffffff)) { \
            *val = pvr2_tex_mem_unused_intf.read##suffix(addr,NULL);    \
            return 0;                                                   \
        } else {                                                        \
            return -1;                                                  \
        }                                                               \
    }

#define AREA1_WRITEFUNC(tp, suffix)                                     \
    static void area1_write##suffix(uint32_t addr, tp val, void *ctxt) { \
        struct area1 *area = ctxt;                                      \
        uint32_t addr_ext = addr & RANGE_MASK_EXT;                      \
        if ((addr_ext >= 0x04000000 && addr_ext <= 0x047fffff) ||       \
            (addr_ext >= 0x06000000 && addr_ext <= 0x067fffff)) {       \
            area->tex_mem_64bit_intf->write##suffix(addr, val,          \
                                                    area->tex_mem_64bit_argp); \
        } else if ((addr_ext >= 0x05000000 && addr_ext <= 0x057fffff) || \
                   (addr_ext >= 0x07000000 && addr_ext <= 0x077fffff)) { \
            area->tex_mem_32bit_intf->write##suffix(addr, val,          \
                                                    area->tex_mem_32bit_argp); \
        } else if ((addr_ext >= 0x04800000 && addr_ext <= 0x04ffffff) || \
                   (addr_ext >= 0x05800000 && addr_ext <= 0x05ffffff) || \
                   (addr_ext >= 0x06800000 && addr_ext <= 0x06ffffff) || \
                   (addr_ext >= 0x07800000 && addr_ext <= 0x07ffffff)) { \
            pvr2_tex_mem_unused_intf.write##suffix(addr,val,NULL);      \
        } else {                                                        \
            error_set_address(addr);                                    \
            error_set_length(sizeof(tp));                               \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
    }

#define AREA1_TRY_WRITEFUNC(tp, suffix)                                 \
    static int area1_try_write##suffix(uint32_t addr, tp val, void *ctxt) { \
        struct area1 *area = ctxt;                                      \
        uint32_t addr_ext = addr & RANGE_MASK_EXT;                      \
        if ((addr_ext >= 0x04000000 && addr_ext <= 0x047fffff) ||       \
            (addr_ext >= 0x06000000 && addr_ext <= 0x067fffff)) {       \
            area->tex_mem_64bit_intf->write##suffix(addr, val,          \
                                                    area->tex_mem_64bit_argp); \
            return 0;                                                   \
        } else if ((addr_ext >= 0x05000000 && addr_ext <= 0x057fffff) || \
                   (addr_ext >= 0x07000000 && addr_ext <= 0x077fffff)) { \
            area->tex_mem_32bit_intf->write##suffix(addr, val,          \
                                                    area->tex_mem_32bit_argp); \
            return 0;                                                   \
        } else if ((addr_ext >= 0x04800000 && addr_ext <= 0x04ffffff) || \
                   (addr_ext >= 0x05800000 && addr_ext <= 0x05ffffff) || \
                   (addr_ext >= 0x06800000 && addr_ext <= 0x06ffffff) || \
                   (addr_ext >= 0x07800000 && addr_ext <= 0x07ffffff)) { \
            pvr2_tex_mem_unused_intf.write##suffix(addr,val,NULL);      \
            return 0;                                                   \
        } else {                                                        \
            return -1;                                                  \
        }                                                               \
    }

AREA1_READFUNC(double, double)
AREA1_READFUNC(float, float)
AREA1_READFUNC(uint32_t, 32)
AREA1_READFUNC(uint16_t, 16)
AREA1_READFUNC(uint8_t, 8)

AREA1_TRY_READFUNC(double, double)
AREA1_TRY_READFUNC(float, float)
AREA1_TRY_READFUNC(uint32_t, 32)
AREA1_TRY_READFUNC(uint16_t, 16)
AREA1_TRY_READFUNC(uint8_t, 8)

AREA1_WRITEFUNC(double, double)
AREA1_WRITEFUNC(float, float)
AREA1_WRITEFUNC(uint32_t, 32)
AREA1_WRITEFUNC(uint16_t, 16)
AREA1_WRITEFUNC(uint8_t, 8)

AREA1_TRY_WRITEFUNC(double, double)
AREA1_TRY_WRITEFUNC(float, float)
AREA1_TRY_WRITEFUNC(uint32_t, 32)
AREA1_TRY_WRITEFUNC(uint16_t, 16)
AREA1_TRY_WRITEFUNC(uint8_t, 8)

struct memory_interface area1_intf = {
    .readfloat = area1_readfloat,
    .readdouble = area1_readdouble,
    .read32 = area1_read32,
    .read16 = area1_read16,
    .read8 = area1_read8,

    .try_readfloat = area1_try_readfloat,
    .try_readdouble = area1_try_readdouble,
    .try_read32 = area1_try_read32,
    .try_read16 = area1_try_read16,
    .try_read8 = area1_try_read8,

    .writefloat = area1_writefloat,
    .writedouble = area1_writedouble,
    .write32 = area1_write32,
    .write16 = area1_write16,
    .write8 = area1_write8,

    .try_writefloat = area1_try_writefloat,
    .try_writedouble = area1_try_writedouble,
    .try_write32 = area1_try_write32,
    .try_write16 = area1_try_write16,
    .try_write8 = area1_try_write8
};
