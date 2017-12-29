/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#include "error.h"
#include "modem.h"
#include "mem_code.h"

float modem_read_float(addr32_t addr) {
    return 0.0f;
}

void modem_write_float(addr32_t addr, float val) {
    error_set_feature("sending data to the modem unit");
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

double modem_read_double(addr32_t addr) {
    return 0.0;
}

void modem_write_double(addr32_t addr, double val) {
    error_set_feature("sending data to the modem unit");
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint8_t modem_read_8(addr32_t addr) {
    return 0;
}

void modem_write_8(addr32_t addr, uint8_t val) {
    error_set_feature("sending data to the modem unit");
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint16_t modem_read_16(addr32_t addr) {
    return 0;
}

void modem_write_16(addr32_t addr, uint16_t val) {
    error_set_feature("sending data to the modem unit");
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint32_t modem_read_32(addr32_t addr) {
    return 0;
}

void modem_write_32(addr32_t addr, uint32_t val) {
    error_set_feature("sending data to the modem unit");
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}
