/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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

#include "log.h"
#include "washdc/error.h"
#include "modem.h"
#include "mem_code.h"

float modem_read_float(addr32_t addr, void *ctxt) {
    return 0.0f;
}

void modem_write_float(addr32_t addr, float val, void *ctxt) {
    LOG_DBG("%s - Writing %f to the modem\n", __func__, (double)val);
}

double modem_read_double(addr32_t addr, void *ctxt) {
    return 0.0;
}

void modem_write_double(addr32_t addr, double val, void *ctxt) {
    LOG_DBG("%s - Writing %f to the modem\n", __func__, val);
}

uint8_t modem_read_8(addr32_t addr, void *ctxt) {
    return 0;
}

void modem_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    LOG_DBG("%s - Writing 0x%02x to the modem\n", __func__, (unsigned)val);
}

uint16_t modem_read_16(addr32_t addr, void *ctxt) {
    return 0;
}

void modem_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    LOG_DBG("%s - Writing 0x%04x to the modem\n", __func__, (unsigned)val);
}

uint32_t modem_read_32(addr32_t addr, void *ctxt) {
    return 0;
}

void modem_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    LOG_DBG("%s - Writing 0x%08x to the modem\n", __func__, (unsigned)val);
}

struct memory_interface modem_intf = {
    .read32 = modem_read_32,
    .read16 = modem_read_16,
    .read8 = modem_read_8,
    .readfloat = modem_read_float,
    .readdouble = modem_read_double,

    .write32 = modem_write_32,
    .write16 = modem_write_16,
    .write8 = modem_write_8,
    .writefloat = modem_write_float,
    .writedouble = modem_write_double
};
