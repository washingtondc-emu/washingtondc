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

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "error.h"
#include "MemoryMap.h"

#include "aica_channel.h"

static float aica_channel_read_float(uint32_t addr, void *ctxt);
static void aica_channel_write_float(uint32_t addr, float val, void *ctxt);
static double aica_channel_read_double(uint32_t addr, void *ctxt);
static void aica_channel_write_double(uint32_t addr, double val, void *ctxt);
static uint32_t aica_channel_read_32(uint32_t addr, void *ctxt);
static void aica_channel_write_32(uint32_t addr, uint32_t val, void *ctxt);
static uint16_t aica_channel_read_16(uint32_t addr, void *ctxt);
static void aica_channel_write_16(uint32_t addr, uint16_t val, void *ctxt);
static uint8_t aica_channel_read_8(uint32_t addr, void *ctxt);
static void aica_channel_write_8(uint32_t addr, uint8_t val, void *ctxt);

struct memory_interface aica_channel_intf = {
    .readfloat = aica_channel_read_float,
    .readdouble = aica_channel_read_double,
    .read32 = aica_channel_read_32,
    .read16 = aica_channel_read_16,
    .read8 = aica_channel_read_8,

    .writefloat = aica_channel_write_float,
    .writedouble = aica_channel_write_double,
    .write32 = aica_channel_write_32,
    .write16 = aica_channel_write_16,
    .write8 = aica_channel_write_8
};

void aica_channel_init(struct aica_channel *data) {
    memset(data->backing, 0, sizeof(data->backing));
}

void aica_channel_cleanup(struct aica_channel *data) {
}

static float aica_channel_read_float(uint32_t addr, void *ctxt) {
    struct aica_channel *ch = (struct aica_channel*)ctxt;

    if (addr >= AICA_CHANNEL_LEN) {
        error_set_address(addr);
        error_set_length(sizeof(float));
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

    return ((float*)ch->backing)[addr / sizeof(float)];
}

static double aica_channel_read_double(uint32_t addr, void *ctxt) {
    struct aica_channel *ch = (struct aica_channel*)ctxt;

    if (addr >= AICA_CHANNEL_LEN) {
        error_set_address(addr);
        error_set_length(sizeof(double));
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

    return ((double*)ch->backing)[addr / sizeof(double)];
}

static uint32_t aica_channel_read_32(uint32_t addr, void *ctxt) {
    struct aica_channel *ch = (struct aica_channel*)ctxt;

    if (addr >= AICA_CHANNEL_LEN) {
        error_set_address(addr);
        error_set_length(sizeof(uint32_t));
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

    return ((uint32_t*)ch->backing)[addr / sizeof(uint32_t)];
}

static uint16_t aica_channel_read_16(uint32_t addr, void *ctxt) {
    struct aica_channel *ch = (struct aica_channel*)ctxt;

    if (addr >= AICA_CHANNEL_LEN) {
        error_set_address(addr);
        error_set_length(sizeof(uint16_t));
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

    return ((uint16_t*)ch->backing)[addr / sizeof(uint16_t)];
}

static uint8_t aica_channel_read_8(uint32_t addr, void *ctxt) {
    struct aica_channel *ch = (struct aica_channel*)ctxt;

    if (addr >= AICA_CHANNEL_LEN) {
        error_set_address(addr);
        error_set_length(sizeof(uint8_t));
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

    return ((uint8_t*)ch->backing)[addr / sizeof(uint8_t)];
}

static void aica_channel_write_float(uint32_t addr, float val, void *ctxt) {
    struct aica_channel *ch = (struct aica_channel*)ctxt;

    if (addr >= AICA_CHANNEL_LEN) {
        error_set_address(addr);
        error_set_length(sizeof(float));
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

    ((float*)ch->backing)[addr / sizeof(float)] = val;
}

static void aica_channel_write_double(uint32_t addr, double val, void *ctxt) {
    struct aica_channel *ch = (struct aica_channel*)ctxt;

    if (addr >= AICA_CHANNEL_LEN) {
        error_set_address(addr);
        error_set_length(sizeof(double));
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

    ((double*)ch->backing)[addr / sizeof(double)] = val;
}

static void aica_channel_write_32(uint32_t addr, uint32_t val, void *ctxt) {
    struct aica_channel *ch = (struct aica_channel*)ctxt;

    if (addr >= AICA_CHANNEL_LEN) {
        error_set_address(addr);
        error_set_length(sizeof(uint32_t));
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

    ((uint32_t*)ch->backing)[addr / sizeof(uint32_t)] = val;
}

static void aica_channel_write_16(uint32_t addr, uint16_t val, void *ctxt) {
    struct aica_channel *ch = (struct aica_channel*)ctxt;

    if (addr >= AICA_CHANNEL_LEN) {
        error_set_address(addr);
        error_set_length(sizeof(uint16_t));
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

    ((uint16_t*)ch->backing)[addr / sizeof(uint16_t)] = val;
}

static void aica_channel_write_8(uint32_t addr, uint8_t val, void *ctxt) {
    struct aica_channel *ch = (struct aica_channel*)ctxt;

    if (addr >= AICA_CHANNEL_LEN) {
        error_set_address(addr);
        error_set_length(sizeof(uint8_t));
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

    ((uint8_t*)ch->backing)[addr / sizeof(uint8_t)] = val;
}
