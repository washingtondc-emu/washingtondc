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
#include <stdio.h>

#include <stdint.h>

#include "g1_reg.h"

#include "mem_code.h"
#include "error.h"
#include "types.h"
#include "mem_areas.h"

#define N_G1_REGS (ADDR_G1_LAST - ADDR_G1_FIRST + 1)
static reg32_t g1_regs[N_G1_REGS];

struct g1_mem_mapped_reg;

typedef int(*g1_reg_read_handler_t)(struct g1_mem_mapped_reg const *reg_info,
                                    void *buf, addr32_t addr, unsigned len);
typedef int(*g1_reg_write_handler_t)(struct g1_mem_mapped_reg const *reg_info,
                                     void const *buf, addr32_t addr,
                                     unsigned len);

static int
default_g1_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len);
static int
default_g1_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len);
static int
warn_g1_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                         void *buf, addr32_t addr, unsigned len);
static int
warn_g1_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                          void const *buf, addr32_t addr, unsigned len);

static struct g1_mem_mapped_reg {
    char const *reg_name;

    addr32_t addr;

    unsigned len;

    g1_reg_read_handler_t on_read;
    g1_reg_write_handler_t on_write;
} g1_reg_info[] = {
    /* GD-ROM DMA registers */
    { "SB_GDSTAR", 0x5f7404, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },
    { "SB_GDLEN", 0x5f7408, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },
    { "SB_GDDIR", 0x5f740c, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },
    { "SB_GDEN", 0x5f7414, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },
    { "SB_GDST", 0x5f7418, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },

    /* system boot-rom registers */
    // XXX this is supposed to be write-only, but currently it's readable
    { "SB_G1RRC", 0x005f7480, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },
    { "SB_G1RWC", 0x5f7484, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },

    /* flash rom registers */
    { "SB_G1FRC", 0x5f7488, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },
    { "SB_G1FWC", 0x5f748c, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },

    /* GD PIO timing registers - I guess this is related to GD-ROM ? */
    { "SB_G1CRC", 0x5f7490, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },
    { "SB_G1CWC", 0x5f7494, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },

    /* GD-DMA timing registers - *probably* related to GD-ROM */
    { "SB_G1GDRC", 0x5f74a0, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },
    { "SB_G1GDWC", 0x5f74a4, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },

    // TODO: SB_G1SYSM should be read-only
    { "SB_G1SYSM", 0x5f74b0, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },
    { "SB_G1CRDYC", 0x5f74b4, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },
    { "SB_GDAPRO", 0x5f74b8, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },

    { "UNKNOWN", 0x005f74e4, 4,
      warn_g1_reg_read_handler, warn_g1_reg_write_handler },

    { NULL }
};

int g1_reg_read(void *buf, size_t addr, size_t len) {
    struct g1_mem_mapped_reg *curs = g1_reg_info;

    while (curs->reg_name) {
        if (curs->addr == addr) {
            if (curs->len >= len) {
                return curs->on_read(curs, buf, addr, len);
            } else {
                error_set_address(addr);
                error_set_length(len);
                error_set_feature("Whatever happens when you use an "
                                  "inapproriate length while reading "
                                  "from a g1 register");
                PENDING_ERROR(ERROR_UNIMPLEMENTED);
                return MEM_ACCESS_FAILURE;
            }
        }
        curs++;
    }

    error_set_address(addr);
    error_set_feature("reading from one of the g1 registers");
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

int g1_reg_write(void const *buf, size_t addr, size_t len) {
    struct g1_mem_mapped_reg *curs = g1_reg_info;

    while (curs->reg_name) {
        if (curs->addr == addr) {
            if (curs->len >= len) {
                return curs->on_write(curs, buf, addr, len);
            } else {
                error_set_address(addr);
                error_set_length(len);
                error_set_feature("Whatever happens when you use an "
                                  "inapproriate length while writing to a g1 "
                                  "register");
                PENDING_ERROR(ERROR_UNIMPLEMENTED);
                return MEM_ACCESS_FAILURE;
            }
        }
        curs++;
    }

    error_set_address(addr);
    error_set_length(len);
    error_set_feature("writing to one of the g1 registers");
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static int
default_g1_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_G1_FIRST) >> 2;
    memcpy(buf, idx + g1_regs, len);
    return MEM_ACCESS_SUCCESS;
}

static int
default_g1_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_G1_FIRST) >> 2;
    memcpy(idx + g1_regs, buf, len);
    return MEM_ACCESS_SUCCESS;
}

static int
warn_g1_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    int ret_code = default_g1_reg_read_handler(reg_info, buf, addr, len);

    if (ret_code) {
        fprintf(stderr, "WARNING: read from g1 register %s\n",
                reg_info->reg_name);
    } else {
        switch (len) {
        case 1:
            memcpy(&val8, buf, sizeof(val8));
            fprintf(stderr, "WARNING: read 0x%02x from g1 register %s\n",
                    (unsigned)val8, reg_info->reg_name);
            break;
        case 2:
            memcpy(&val16, buf, sizeof(val16));
            fprintf(stderr, "WARNING: read 0x%04x from g1 register %s\n",
                    (unsigned)val16, reg_info->reg_name);
            break;
        case 4:
            memcpy(&val32, buf, sizeof(val32));
            fprintf(stderr, "WARNING: read 0x%08x from g1 register %s\n",
                    (unsigned)val32, reg_info->reg_name);
            break;
        default:
            fprintf(stderr, "WARNING: read from g1 register %s\n",
                    reg_info->reg_name);
        }
    }

    return MEM_ACCESS_SUCCESS;
}

static int
warn_g1_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    switch (len) {
    case 1:
        memcpy(&val8, buf, sizeof(val8));
        fprintf(stderr, "WARNING: write 0x%02x to g1 register %s\n",
                (unsigned)val8, reg_info->reg_name);
        break;
    case 2:
        memcpy(&val16, buf, sizeof(val16));
        memcpy(&val16, buf, sizeof(val16));
        fprintf(stderr, "WARNING: write 0x%04x to g1 register %s\n",
                (unsigned)val16, reg_info->reg_name);
        break;
    case 4:
        memcpy(&val32, buf, sizeof(val32));
        fprintf(stderr, "WARNING: write 0x%08x to g1 register %s\n",
                (unsigned)val32, reg_info->reg_name);
        break;
    default:
        fprintf(stderr, "WARNING: write to g1 register %s\n",
                reg_info->reg_name);
    }

    return default_g1_reg_write_handler(reg_info, buf, addr, len);
}
