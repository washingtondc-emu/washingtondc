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

#include <stdio.h>
#include <string.h>

#include "error.h"
#include "mem_areas.h"
#include "types.h"

#include "maple_reg.h"

#define N_MAPLE_REGS (ADDR_MAPLE_LAST - ADDR_MAPLE_FIRST + 1)
static reg32_t maple_regs[N_MAPLE_REGS];

struct maple_mapped_reg;

typedef int(*maple_reg_read_handler_t)(struct maple_mapped_reg const *reg_info,
                                       void *buf, addr32_t addr, unsigned len);
typedef int(*maple_reg_write_handler_t)(struct maple_mapped_reg const *reg_info,
                                        void const *buf, addr32_t addr,
                                        unsigned len);
static int
default_maple_reg_read_handler(struct maple_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len);
static int
default_maple_reg_write_handler(struct maple_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len);

static int
warn_maple_reg_read_handler(struct maple_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len);
static int
warn_maple_reg_write_handler(struct maple_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len);

static struct maple_mapped_reg {
    char const *reg_name;

    addr32_t addr;

    unsigned len;

    maple_reg_read_handler_t on_read;
    maple_reg_write_handler_t on_write;
} maple_reg_info[] = {
    { "SB_MDSTAR", 0x5f6c04, 4,
      warn_maple_reg_read_handler, warn_maple_reg_write_handler },
    { "SB_MDTSEL", 0x5f6c10, 4,
      warn_maple_reg_read_handler, warn_maple_reg_write_handler },
    { "SB_MDEN", 0x5f6c14, 4,
      warn_maple_reg_read_handler, warn_maple_reg_write_handler },
    { "SB_MDST", 0x5f6c18, 4,
      warn_maple_reg_read_handler, warn_maple_reg_write_handler },
    { "SB_MSYS", 0x5f6c80, 4,
      warn_maple_reg_read_handler, warn_maple_reg_write_handler },
    { "SB_MDAPRO", 0x5f6c8c, 4,
      warn_maple_reg_read_handler, warn_maple_reg_write_handler },
    { "SB_MMSEL", 0x5f6ce8, 4,
      warn_maple_reg_read_handler, warn_maple_reg_write_handler },
    { NULL }
};

int maple_reg_read(void *buf, size_t addr, size_t len) {
    struct maple_mapped_reg *curs = maple_reg_info;

    while (curs->reg_name) {
        if (curs->addr == addr) {
            if (curs->len == len) {
                return curs->on_read(curs, buf, addr, len);
            } else {
                error_set_address(addr);
                error_set_length(len);
                error_set_feature("Whatever happens when you use an "
                                  "inapproriate length while reading from a "
                                  "maple register");
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
            }
        }
        curs++;
    }

    error_set_address(addr);
    error_set_length(len);
    error_set_feature("reading from one of the maple registers");
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

int maple_reg_write(void const *buf, size_t addr, size_t len) {
    struct maple_mapped_reg *curs = maple_reg_info;

    while (curs->reg_name) {
        if (curs->addr == addr) {
            if (curs->len == len) {
                return curs->on_write(curs, buf, addr, len);
            } else {
                error_set_address(addr);
                error_set_length(len);
                error_set_feature("Whatever happens when you use an "
                                  "inapproriate length while writing to a "
                                  "maple register");
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
            }
        }
        curs++;
    }

    error_set_address(addr);
    error_set_length(len);
    error_set_feature("writing to one of the maple registers");
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static int
default_maple_reg_read_handler(struct maple_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_MAPLE_FIRST) >> 2;
    memcpy(buf, idx + maple_regs, len);
    return 0;
}

static int
default_maple_reg_write_handler(struct maple_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_MAPLE_FIRST) >> 2;
    memcpy(idx + maple_regs, buf, len);
    return 0;
}

static int
warn_maple_reg_read_handler(struct maple_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    int ret_code = default_maple_reg_read_handler(reg_info, buf, addr, len);

    if (ret_code) {
        fprintf(stderr, "WARNING: read from maple register %s\n",
                reg_info->reg_name);
    } else {
        switch (reg_info->len) {
        case 1:
            memcpy(&val8, buf, sizeof(val8));
            fprintf(stderr, "WARNING: read 0x%02x from maple register %s\n",
                    (unsigned)val8, reg_info->reg_name);
            break;
        case 2:
            memcpy(&val16, buf, sizeof(val16));
            fprintf(stderr, "WARNING: read 0x%04x from maple register %s\n",
                    (unsigned)val16, reg_info->reg_name);
            break;
        case 4:
            memcpy(&val32, buf, sizeof(val32));
            fprintf(stderr, "WARNING: read 0x%08x from maple register %s\n",
                    (unsigned)val32, reg_info->reg_name);
            break;
        default:
            fprintf(stderr, "WARNING: read from maple register %s\n",
                    reg_info->reg_name);
        }
    }

    return 0;
}

static int
warn_maple_reg_write_handler(struct maple_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    switch (reg_info->len) {
    case 1:
        memcpy(&val8, buf, sizeof(val8));
        fprintf(stderr, "WARNING: writing 0x%02x to maple register %s\n",
                (unsigned)val8, reg_info->reg_name);
        break;
    case 2:
        memcpy(&val16, buf, sizeof(val16));
        fprintf(stderr, "WARNING: writing 0x%04x to maple register %s\n",
                (unsigned)val16, reg_info->reg_name);
        break;
    case 4:
        memcpy(&val32, buf, sizeof(val32));
        fprintf(stderr, "WARNING: writing 0x%08x to maple register %s\n",
                (unsigned)val32, reg_info->reg_name);
        break;
    default:
        fprintf(stderr, "WARNING: writing to maple register %s\n",
                reg_info->reg_name);
    }

    return default_maple_reg_write_handler(reg_info, buf, addr, len);
}
