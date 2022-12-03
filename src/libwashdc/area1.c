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

#include "area1.h"

void area1_init(struct area1 *area1, struct pvr2 *pvr2,
                washdc_hostfile pvr2_trace_file) {
    area1->pvr2 = pvr2;

    memory_map_init(&area1->map);

    if (pvr2_trace_file != WASHDC_HOSTFILE_INVALID) {
        static struct trace_proxy pvr2_mem_32bit_traceproxy,
            pvr2_mem_64bit_traceproxy;
        trace_proxy_create(&pvr2_mem_32bit_traceproxy, pvr2_trace_file,
                           TRACE_SOURCE_SH4, &pvr2_tex_mem_area32_intf, pvr2);
        trace_proxy_create(&pvr2_mem_64bit_traceproxy, pvr2_trace_file,
                           TRACE_SOURCE_SH4, &pvr2_tex_mem_area64_intf, pvr2);
        memory_map_add(&area1->map, 0x04000000, 0x047fffff,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &trace_proxy_memory_interface, &pvr2_mem_64bit_traceproxy);
        memory_map_add(&area1->map, 0x05000000, 0x057fffff,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &trace_proxy_memory_interface, &pvr2_mem_32bit_traceproxy);
        memory_map_add(&area1->map, 0x06000000, 0x067fffff,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &trace_proxy_memory_interface, &pvr2_mem_64bit_traceproxy);
        memory_map_add(&area1->map, 0x07000000, 0x077fffff,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &trace_proxy_memory_interface, &pvr2_mem_32bit_traceproxy);
    } else {
        memory_map_add(&area1->map, 0x04000000, 0x047fffff,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &pvr2_tex_mem_area64_intf, pvr2);
        memory_map_add(&area1->map, 0x05000000, 0x057fffff,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &pvr2_tex_mem_area32_intf, pvr2);
        memory_map_add(&area1->map, 0x06000000, 0x067fffff,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &pvr2_tex_mem_area64_intf, pvr2);
        memory_map_add(&area1->map, 0x07000000, 0x077fffff,
                       RANGE_MASK_EXT, MEMORY_MAP_REGION_UNKNOWN,
                       &pvr2_tex_mem_area32_intf, pvr2);
    }

    /*
     * More PowerVR2 texture regions - these don't get used much which is why
     * they're at the end (for performance reasons).
     */
    memory_map_add(&area1->map, 0x04800000, 0x04ffffff, RANGE_MASK_EXT,
                   MEMORY_MAP_REGION_UNKNOWN, &pvr2_tex_mem_unused_intf, NULL);
    memory_map_add(&area1->map, 0x05800000, 0x05ffffff, RANGE_MASK_EXT,
                   MEMORY_MAP_REGION_UNKNOWN, &pvr2_tex_mem_unused_intf, NULL);
    memory_map_add(&area1->map, 0x06800000, 0x06ffffff, RANGE_MASK_EXT,
                   MEMORY_MAP_REGION_UNKNOWN, &pvr2_tex_mem_unused_intf, NULL);
    memory_map_add(&area1->map, 0x07800000, 0x07ffffff, RANGE_MASK_EXT,
                   MEMORY_MAP_REGION_UNKNOWN, &pvr2_tex_mem_unused_intf, NULL);
}

void area1_cleanup(struct area1 *area1) {
    memory_map_cleanup(&area1->map);
}

#define AREA1_READFUNC(tp, suffix)                          \
    static tp area1_read##suffix(uint32_t addr,             \
                                 void *ctxt) {              \
        struct area1 *area = ctxt;                          \
        return memory_map_read_##suffix(&area->map, addr);  \
    }

#define AREA1_TRY_READFUNC(tp, suffix)          \
    static int area1_try_read##suffix(uint32_t addr, tp *val, void *ctxt) { \
        struct area1 *area = ctxt;                                      \
        return memory_map_try_read_##suffix(&area->map, addr, val);     \
    }

#define AREA1_WRITEFUNC(tp, suffix)                                     \
    static void area1_write##suffix(uint32_t addr, tp val, void *ctxt) { \
        struct area1 *area = ctxt;                                      \
        memory_map_write_##suffix(&area->map, addr, val);               \
    }

#define AREA1_TRY_WRITEFUNC(tp, suffix)                                 \
    static int area1_try_write##suffix(uint32_t addr, tp val, void *ctxt) { \
        struct area1 *area = ctxt;                                      \
        return memory_map_try_write_##suffix(&area->map, addr, val);    \
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
