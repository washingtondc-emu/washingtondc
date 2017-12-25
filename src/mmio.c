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

#include <stdlib.h>
#include <string.h>

#include "error.h"

#include "mmio.h"

void init_mmio_region(struct mmio_region *region,
                      addr32_t first, addr32_t last) {
    region->beg = first;
    region->len = (last - first + 1) / sizeof(uint32_t);

    region->backing = calloc(region->len, sizeof(uint32_t));
    region->names = calloc(region->len, sizeof(char const*));
    region->on_read = calloc(region->len, sizeof(mmio_read_handler));
    region->on_write = calloc(region->len, sizeof(mmio_write_handler));

    if (!region->backing)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    size_t cell_no;
    for (cell_no = 0; cell_no < region->len; cell_no++) {
        region->names[cell_no] = "UNKNOWN_REGISTER";
        region->on_read[cell_no] = mmio_read_error;
        region->on_write[cell_no] = mmio_write_error;
    }
}

void cleanup_mmio_region(struct mmio_region *region) {
    free(region->on_write);
    free(region->on_read);
    free(region->names);
    free(region->backing);
    memset(region, 0, sizeof(*region));
}

void init_mmio_cell(struct mmio_region *region, char const *name,
                    addr32_t addr, mmio_read_handler on_read,
                    mmio_write_handler on_write) {
    unsigned idx = (addr - region->beg) / 4;
    region->names[idx] = name;
    region->on_read[idx] = on_read;
    region->on_write[idx] = on_write;
}

uint32_t mmio_warn_read_handler(struct mmio_region *region, unsigned idx) {
    uint32_t ret = region->backing[idx];
    LOG_DBG("Read from \"%s\": 0x%08x\n", region->names[idx], (unsigned)ret);
    return ret;
}

void mmio_warn_write_handler(struct mmio_region *region,
                             unsigned idx, uint32_t val) {
    LOG_DBG("Write to \"%s\": 0x%08x\n", region->names[idx], (unsigned)val);
    region->backing[idx] = val;
}

uint32_t mmio_read_error(struct mmio_region *region, unsigned idx) {
    error_set_length(4);
    error_set_address(idx * 4);
    error_set_feature("reading from some mmio register");
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void mmio_write_error(struct mmio_region *region, unsigned idx, uint32_t val) {
    error_set_length(4);
    error_set_address(idx * 4);
    error_set_feature("writing to some mmio register");
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void mmio_readonly_write_error(struct mmio_region *region,
                               unsigned idx, uint32_t val) {
    error_set_length(4);
    error_set_address(idx * 4);
    error_set_feature("proper response for writing to a read-only register");
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint32_t mmio_writeonly_read_handler(struct mmio_region *region,
                                     unsigned idx) {
    error_set_length(4);
    error_set_address(idx * 4);
    error_set_feature("proper response for reading from a write-only register");
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}
