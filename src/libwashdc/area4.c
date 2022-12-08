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

#include "washdc/error.h"
#include "hw/pvr2/pvr2_ta.h"
#include "hw/pvr2/pvr2_yuv.h"
#include "trace_proxy.h"

#include "area4.h"

void area4_init(struct area4 *area4, struct pvr2 *pvr2,
                washdc_hostfile pvr2_trace_file) {
    area4->pvr2 = pvr2;

    if (pvr2_trace_file != WASHDC_HOSTFILE_INVALID) {
        static struct trace_proxy ta_fifo_traceproxy, ta_yuv_fifo_traceproxy;

        trace_proxy_create(&ta_fifo_traceproxy, pvr2_trace_file,
                           TRACE_SOURCE_SH4, &pvr2_ta_fifo_intf, pvr2);
        trace_proxy_create(&ta_yuv_fifo_traceproxy, pvr2_trace_file,
                           TRACE_SOURCE_SH4, &pvr2_ta_yuv_fifo_intf, pvr2);

        area4->ta_fifo_intf = &ta_fifo_traceproxy;
        area4->ta_yuv_intf = &ta_yuv_fifo_traceproxy;
    } else {
        area4->ta_fifo_intf = &pvr2_ta_fifo_intf;
        area4->ta_yuv_intf = &pvr2_ta_yuv_fifo_intf;
    }
}

void area4_cleanup(struct area4 *area4) {
}

#define AREA4_READFUNC(tp, suffix)                                      \
    static tp area4_read##suffix(uint32_t addr,                         \
                                 void *ctxt) {                          \
        struct area4 *area4 = ctxt;                                     \
        uint32_t addr_ext = addr & RANGE_MASK_EXT;                      \
        if ((addr_ext >= 0x10000000 && addr_ext <= 0x107fffff) ||       \
            (addr_ext >= 0x11000000 && addr_ext <= 0x117fffff)) {       \
            return area4->ta_fifo_intf->read##suffix(addr, area4->pvr2); \
        } else if (addr_ext >= 0x10800000 && addr_ext <= 0x10ffffff) {  \
            return area4->ta_yuv_intf->read##suffix(addr, area4->pvr2); \
        } else {                                                        \
            error_set_address(addr);                                    \
            error_set_length(sizeof(tp));                               \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
    }

#define AREA4_TRY_READFUNC(tp, suffix)                                  \
    static int area4_try_read##suffix(uint32_t addr, tp *val, void *ctxt) { \
        struct area4 *area = ctxt;                                      \
        uint32_t addr_ext = addr & RANGE_MASK_EXT;                      \
        if ((addr_ext >= 0x10000000 && addr_ext <= 0x107fffff) ||       \
            (addr_ext >= 0x11000000 && addr_ext <= 0x117fffff)) {       \
            return area->ta_fifo_intf->try_read##suffix(addr, val, area->pvr2); \
        } else if (addr_ext >= 0x10800000 && addr_ext <= 0x10ffffff) {  \
            return area->ta_yuv_intf->try_read##suffix(addr, val, area->pvr2); \
        } else {                                                        \
            return -1;                                                  \
        }                                                               \
    }

#define AREA4_WRITEFUNC(tp, suffix)                                     \
    static void area4_write##suffix(uint32_t addr, tp val, void *ctxt) { \
        struct area4 *area4 = ctxt;                                     \
        uint32_t addr_ext = addr & RANGE_MASK_EXT;                      \
        if ((addr_ext >= 0x10000000 && addr_ext <= 0x107fffff) ||       \
            (addr_ext >= 0x11000000 && addr_ext <= 0x117fffff)) {       \
            area4->ta_fifo_intf->write##suffix(addr, val, area4->pvr2); \
        } else if (addr_ext >= 0x10800000 && addr_ext <= 0x10ffffff) {  \
            area4->ta_yuv_intf->write##suffix(addr, val, area4->pvr2);  \
        } else {                                                        \
            error_set_address(addr);                                    \
            error_set_length(sizeof(tp));                               \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
    }

#define AREA4_TRY_WRITEFUNC(tp, suffix)                                 \
    static int area4_try_write##suffix(uint32_t addr, tp val, void *ctxt) { \
        struct area4 *area = ctxt;                                      \
        uint32_t addr_ext = addr & RANGE_MASK_EXT;                      \
        if ((addr_ext >= 0x10000000 && addr_ext <= 0x107fffff) ||       \
            (addr_ext >= 0x11000000 && addr_ext <= 0x117fffff)) {       \
            return area->ta_fifo_intf->try_write##suffix(addr, val, area->pvr2); \
        } else if (addr_ext >= 0x10800000 && addr_ext <= 0x10ffffff) {  \
            return area->ta_yuv_intf->try_write##suffix(addr, val, area->pvr2); \
        } else {                                                        \
            return -1;                                                  \
        }                                                               \
    }

AREA4_READFUNC(double, double)
AREA4_READFUNC(float, float)
AREA4_READFUNC(uint32_t, 32)
AREA4_READFUNC(uint16_t, 16)
AREA4_READFUNC(uint8_t, 8)

AREA4_TRY_READFUNC(double, double)
AREA4_TRY_READFUNC(float, float)
AREA4_TRY_READFUNC(uint32_t, 32)
AREA4_TRY_READFUNC(uint16_t, 16)
AREA4_TRY_READFUNC(uint8_t, 8)

AREA4_WRITEFUNC(double, double)
AREA4_WRITEFUNC(float, float)
AREA4_WRITEFUNC(uint32_t, 32)
AREA4_WRITEFUNC(uint16_t, 16)
AREA4_WRITEFUNC(uint8_t, 8)

AREA4_TRY_WRITEFUNC(double, double)
AREA4_TRY_WRITEFUNC(float, float)
AREA4_TRY_WRITEFUNC(uint32_t, 32)
AREA4_TRY_WRITEFUNC(uint16_t, 16)
AREA4_TRY_WRITEFUNC(uint8_t, 8)

struct memory_interface area4_intf = {
    .readfloat = area4_readfloat,
    .readdouble = area4_readdouble,
    .read32 = area4_read32,
    .read16 = area4_read16,
    .read8 = area4_read8,

    .try_readfloat = area4_try_readfloat,
    .try_readdouble = area4_try_readdouble,
    .try_read32 = area4_try_read32,
    .try_read16 = area4_try_read16,
    .try_read8 = area4_try_read8,

    .writefloat = area4_writefloat,
    .writedouble = area4_writedouble,
    .write32 = area4_write32,
    .write16 = area4_write16,
    .write8 = area4_write8,

    .try_writefloat = area4_try_writefloat,
    .try_writedouble = area4_try_writedouble,
    .try_write32 = area4_try_write32,
    .try_write16 = area4_try_write16,
    .try_write8 = area4_try_write8
};
