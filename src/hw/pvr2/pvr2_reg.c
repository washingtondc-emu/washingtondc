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

#include "mem_code.h"
#include "types.h"
#include "MemoryMap.h"

#include "pvr2_reg.h"

#define N_PVR2_REGS (ADDR_PVR2_LAST - ADDR_PVR2_FIRST + 1)
static reg32_t pvr2_regs[N_PVR2_REGS];

struct pvr2_mem_mapped_reg;

typedef int(*pvr2_reg_read_handler_t)(
    struct pvr2_mem_mapped_reg const *reg_info,
    void *buf, addr32_t addr, unsigned len);
typedef int(*pvr2_reg_write_handler_t)(
    struct pvr2_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len);

static int
default_pvr2_reg_read_handler(struct pvr2_mem_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len);
static int
default_pvr2_reg_write_handler(struct pvr2_mem_mapped_reg const *reg_info,
                               void const *buf, addr32_t addr, unsigned len);
static int
warn_pvr2_reg_read_handler(struct pvr2_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
warn_pvr2_reg_write_handler(struct pvr2_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);

static struct pvr2_mem_mapped_reg {
    char const *reg_name;

    addr32_t addr;

    unsigned len;

    pvr2_reg_read_handler_t on_read;
    pvr2_reg_write_handler_t on_write;
} pvr2_reg_info[] = {
    { "SB_PDSTAP", 0x5f7c00, 4,
      warn_pvr2_reg_read_handler, warn_pvr2_reg_write_handler },
    { "SB_PDSTAR", 0x5f7c04, 4,
      warn_pvr2_reg_read_handler, warn_pvr2_reg_write_handler },
    { "SB_PDLEN", 0x5f7c08, 4,
      warn_pvr2_reg_read_handler, warn_pvr2_reg_write_handler },
    { "SB_PDDIR", 0x5f7c0c, 4,
      warn_pvr2_reg_read_handler, warn_pvr2_reg_write_handler },
    { "SB_PDTSEL", 0x5f7c10, 4,
      warn_pvr2_reg_read_handler, warn_pvr2_reg_write_handler },
    { "SB_PDEN", 0x5f7c14, 4,
      warn_pvr2_reg_read_handler, warn_pvr2_reg_write_handler },
    { "SB_PDST", 0x5f7c18, 4,
      warn_pvr2_reg_read_handler, warn_pvr2_reg_write_handler },
    { "SB_PDAPRO", 0x5f7c80, 4,
      warn_pvr2_reg_read_handler, warn_pvr2_reg_write_handler },

    { NULL }
};

int pvr2_reg_read(void *buf, size_t addr, size_t len) {
    struct pvr2_mem_mapped_reg *curs = pvr2_reg_info;

    while (curs->reg_name) {
        if (curs->addr == addr) {
            if (curs->len >= len) {
                return curs->on_read(curs, buf, addr, len);
            } else {
                error_set_feature("Whatever happens when you use an "
                                  "inappropriate length while reading from a "
                                  "pvr2 register");
                error_set_address(addr);
                error_set_length(len);
                PENDING_ERROR(ERROR_UNIMPLEMENTED);
                return MEM_ACCESS_FAILURE;
            }
        }
        curs++;
    }

    error_set_feature("reading from one of the pvr2 registers");
    error_set_address(addr);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

int pvr2_reg_write(void const *buf, size_t addr, size_t len) {
    struct pvr2_mem_mapped_reg *curs = pvr2_reg_info;

    while (curs->reg_name) {
        if (curs->addr == addr) {
            if (curs->len >= len) {
                return curs->on_write(curs, buf, addr, len);
            } else {
                error_set_feature("Whatever happens when you use an "
                                  "inappropriate length while writing to a "
                                  "pvr2 register");
                error_set_address(addr);
                error_set_length(len);
                PENDING_ERROR(ERROR_UNIMPLEMENTED);
                return MEM_ACCESS_FAILURE;
            }
        }
        curs++;
    }

    error_set_feature("writing to one of the pvr2 registers");
    error_set_address(addr);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static int
default_pvr2_reg_read_handler(struct pvr2_mem_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_PVR2_FIRST) >> 2;
    memcpy(buf, idx + pvr2_regs, len);
    return MEM_ACCESS_SUCCESS;
}

static int
default_pvr2_reg_write_handler(struct pvr2_mem_mapped_reg const *reg_info,
                               void const *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_PVR2_FIRST) >> 2;
    memcpy(idx + pvr2_regs, buf, len);
    return MEM_ACCESS_SUCCESS;
}

static int
warn_pvr2_reg_read_handler(struct pvr2_mem_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    int ret_code = default_pvr2_reg_read_handler(reg_info, buf, addr, len);

    if (ret_code) {
        fprintf(stderr, "WARNING: read from pvr2 register %s\n",
                reg_info->reg_name);
    } else {
        switch (len) {
        case 1:
            memcpy(&val8, buf, sizeof(val8));
            fprintf(stderr, "WARNING: read 0x%02x from pvr2 register %s\n",
                    (unsigned)val8, reg_info->reg_name);
            break;
        case 2:
            memcpy(&val16, buf, sizeof(val16));
            fprintf(stderr, "WARNING: read 0x%04x from pvr2 register %s\n",
                    (unsigned)val16, reg_info->reg_name);
            break;
        case 4:
            memcpy(&val32, buf, sizeof(val32));
            fprintf(stderr, "WARNING: read 0x%08x from pvr2 register %s\n",
                    (unsigned)val32, reg_info->reg_name);
            break;
        default: 
            fprintf(stderr, "WARNING: read from pvr2 register %s\n",
                    reg_info->reg_name);
        }
    }

    return MEM_ACCESS_SUCCESS;
}

static int
warn_pvr2_reg_write_handler(struct pvr2_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    switch (len) {
    case 1:
        memcpy(&val8, buf, sizeof(val8));
        fprintf(stderr, "WARNING: writing 0x%02x to pvr2 register %s\n",
                    (unsigned)val8, reg_info->reg_name);
        break;
    case 2:
        memcpy(&val16, buf, sizeof(val16));
        fprintf(stderr, "WARNING: writing 0x%04x to pvr2 register %s\n",
                (unsigned)val16, reg_info->reg_name);
        break;
    case 4:
        memcpy(&val32, buf, sizeof(val32));
        fprintf(stderr, "WARNING: writing 0x%08x to pvr2 register %s\n",
                (unsigned)val32, reg_info->reg_name);
        break;
    default:
        fprintf(stderr, "WARNING: writing to pvr2 register %s\n",
                reg_info->reg_name);
    }

    return default_pvr2_reg_write_handler(reg_info, buf, addr, len);
}
