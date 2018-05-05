/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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

#include <string.h>

#include "MemoryMap.h"

#include "aica_dsp.h"

static float aica_dsp_read_float(uint32_t addr, void *ctxt);
static double aica_dsp_read_double(uint32_t addr, void *ctxt);
static uint32_t aica_dsp_read_32(uint32_t addr, void *ctxt);
static uint16_t aica_dsp_read_16(uint32_t addr, void *ctxt);
static uint8_t aica_dsp_read_8(uint32_t addr, void *ctxt);
static void aica_dsp_write_float(uint32_t addr, float val, void *ctxt);
static void aica_dsp_write_double(uint32_t addr, double val, void *ctxt);
static void aica_dsp_write_32(uint32_t addr, uint32_t val, void *ctxt);
static void aica_dsp_write_16(uint32_t addr, uint16_t val, void *ctxt);
static void aica_dsp_write_8(uint32_t addr, uint8_t val, void *ctxt);

struct memory_interface aica_dsp_intf = {
    .readfloat = aica_dsp_read_float,
    .readdouble = aica_dsp_read_double,
    .read32 = aica_dsp_read_32,
    .read16 = aica_dsp_read_16,
    .read8 = aica_dsp_read_8,

    .writefloat = aica_dsp_write_float,
    .writedouble = aica_dsp_write_double,
    .write32 = aica_dsp_write_32,
    .write16 = aica_dsp_write_16,
    .write8 = aica_dsp_write_8
};

void aica_dsp_init(struct aica_dsp *data) {
    memset(data->backing, 0, sizeof(data->backing));
}

void aica_dsp_cleanup(struct aica_dsp *data) {
}

static float aica_dsp_read_float(uint32_t addr, void *ctxt) {
    struct aica_dsp *dsp = (struct aica_dsp*)ctxt;
    return ((float*)dsp->backing)[addr / sizeof(float)];
}

static double aica_dsp_read_double(uint32_t addr, void *ctxt) {
    struct aica_dsp *dsp = (struct aica_dsp*)ctxt;
    return ((double*)dsp->backing)[addr / sizeof(double)];
}

static uint32_t aica_dsp_read_32(uint32_t addr, void *ctxt) {
    struct aica_dsp *dsp = (struct aica_dsp*)ctxt;
    return ((uint32_t*)dsp->backing)[addr / sizeof(uint32_t)];
}

static uint16_t aica_dsp_read_16(uint32_t addr, void *ctxt) {
    struct aica_dsp *dsp = (struct aica_dsp*)ctxt;
    return ((uint16_t*)dsp->backing)[addr / sizeof(uint16_t)];
}

static uint8_t aica_dsp_read_8(uint32_t addr, void *ctxt) {
    struct aica_dsp *dsp = (struct aica_dsp*)ctxt;
    return ((uint8_t*)dsp->backing)[addr / sizeof(uint8_t)];
}

static void aica_dsp_write_float(uint32_t addr, float val, void *ctxt) {
    struct aica_dsp *dsp = (struct aica_dsp*)ctxt;
    ((float*)dsp->backing)[addr / sizeof(float)] = val;
}

static void aica_dsp_write_double(uint32_t addr, double val, void *ctxt) {
    struct aica_dsp *dsp = (struct aica_dsp*)ctxt;
    ((double*)dsp->backing)[addr / sizeof(double)] = val;
}

static void aica_dsp_write_32(uint32_t addr, uint32_t val, void *ctxt) {
    struct aica_dsp *dsp = (struct aica_dsp*)ctxt;
    ((uint32_t*)dsp->backing)[addr / sizeof(uint32_t)] = val;
}

static void aica_dsp_write_16(uint32_t addr, uint16_t val, void *ctxt) {
    struct aica_dsp *dsp = (struct aica_dsp*)ctxt;
    ((uint16_t*)dsp->backing)[addr / sizeof(uint16_t)] = val;
}

static void aica_dsp_write_8(uint32_t addr, uint8_t val, void *ctxt) {
    struct aica_dsp *dsp = (struct aica_dsp*)ctxt;
    ((uint8_t*)dsp->backing)[addr / sizeof(uint8_t)] = val;
}
