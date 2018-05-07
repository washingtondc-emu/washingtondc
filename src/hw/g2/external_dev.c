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

#include "external_dev.h"

static uint8_t ext_dev_read_8(uint32_t addr, void *ctxt);
static void ext_dev_write_8(uint32_t addr, uint8_t val, void *ctxt);
static uint16_t ext_dev_read_16(uint32_t addr, void *ctxt);
static void ext_dev_write_16(uint32_t addr, uint16_t val, void *ctxt);
static uint32_t ext_dev_read_32(uint32_t addr, void *ctxt);
static void ext_dev_write_32(uint32_t addr, uint32_t val, void *ctxt);
static float ext_dev_read_float(uint32_t addr, void *ctxt);
static void ext_dev_write_float(uint32_t addr, float val, void *ctxt);
static double ext_dev_read_double(uint32_t addr, void *ctxt);
static void ext_dev_write_double(uint32_t addr, double val, void *ctxt);

static uint8_t ext_dev_read_8(uint32_t addr, void *ctxt) {
    return 0;
}

static void ext_dev_write_8(uint32_t addr, uint8_t val, void *ctxt) {
}

static uint16_t ext_dev_read_16(uint32_t addr, void *ctxt) {
    return 0;
}

static void ext_dev_write_16(uint32_t addr, uint16_t val, void *ctxt) {
}

static uint32_t ext_dev_read_32(uint32_t addr, void *ctxt) {
    return 0;
}

static void ext_dev_write_32(uint32_t addr, uint32_t val, void *ctxt) {
}

static float ext_dev_read_float(uint32_t addr, void *ctxt) {
    return 0.0f;
}

static void ext_dev_write_float(uint32_t addr, float val, void *ctxt) {
}

static double ext_dev_read_double(uint32_t addr, void *ctxt) {
    return 0.0;
}

static void ext_dev_write_double(uint32_t addr, double val, void *ctxt) {
}

struct memory_interface ext_dev_intf = {
    .read8 = ext_dev_read_8,
    .read16 = ext_dev_read_16,
    .read32 = ext_dev_read_32,
    .readfloat = ext_dev_read_float,
    .readdouble = ext_dev_read_double,

    .write8 = ext_dev_write_8,
    .write16 = ext_dev_write_16,
    .write32 = ext_dev_write_32,
    .writefloat = ext_dev_write_float,
    .writedouble = ext_dev_write_double
};
