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

#include "types.h"
#include "mem_code.h"
#include "mem_areas.h"
#include "error.h"
#include "log.h"

#include "g2_reg.h"

#define N_G2_REGS (ADDR_G2_LAST - ADDR_G2_FIRST + 1)
static reg32_t g2_regs[N_G2_REGS];

struct g2_mem_mapped_reg;

typedef int(*g2_reg_read_handler_t)(struct g2_mem_mapped_reg const *reg_info,
                                    void *buf, addr32_t addr, unsigned len);
typedef int(*g2_reg_write_handler_t)(struct g2_mem_mapped_reg const *reg_info,
                                     void const *buf, addr32_t addr,
                                     unsigned len);

static int
default_g2_reg_read_handler(struct g2_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len);
static int
default_g2_reg_write_handler(struct g2_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len);
static int
warn_g2_reg_read_handler(struct g2_mem_mapped_reg const *reg_info,
                         void *buf, addr32_t addr, unsigned len);
static int
warn_g2_reg_write_handler(struct g2_mem_mapped_reg const *reg_info,
                          void const *buf, addr32_t addr, unsigned len);

static int sb_adst_reg_write_handler(struct g2_mem_mapped_reg const *reg_info,
                                     void const *buf, addr32_t addr, unsigned len);

static struct g2_mem_mapped_reg {
    char const *reg_name;

    addr32_t addr;

    unsigned len;

    g2_reg_read_handler_t on_read;
    g2_reg_write_handler_t on_write;
} g2_reg_info[] = {
    { "SB_ADSTAG", 0x5f7800, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_ADSTAR", 0x5f7804, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_ADLEN", 0x5f7808, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_ADDIR", 0x5f780c, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_ADTSEL", 0x5f7810, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_ADEN", 0x5f7814, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_ADST", 0x5f7818, 4,
      warn_g2_reg_read_handler, sb_adst_reg_write_handler },
    { "SB_ADSUSP", 0x5f781c, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_E1STAG", 0x5f7820, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_E1STAR", 0x5f7824, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_E1LEN", 0x5f7828, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_E1DIR", 0x5f782c, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_E1TSEL", 0x5f7830, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_E1EN", 0x5f7834, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_E1ST", 0x5f7838, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_E1SUSP", 0x5f783c, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_E2STAG", 0x5f7840, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_E2STAR", 0x5f7844, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_E2LEN", 0x5f7848, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_E2DIR", 0x5f784c, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_E2TSEL", 0x5f7850, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_E2EN", 0x5f7854, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_E2ST", 0x5f7858, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_E2SUSP", 0x5f785c, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_DDSTAG", 0x5f7860, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_DDSTAR", 0x5f7864, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_DDLEN", 0x5f7868, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_DDIR", 0x5f786c, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_DDTSEL", 0x5f7870, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_DDEN", 0x5f7874, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_DDST", 0x5f7878, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_DDSUSP", 0x5f787c, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },

    /* some debugging bullshit, hopefully I never need these... */
    { "SB_G2DSTO", 0x5f7890, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_G2TRTO", 0x5f7894, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },

    /* the modem, it will be a long time before I get around to this */
    { "SB_G2MDMTO", 0x5f7898, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "SB_G2MDMW", 0x5f789c, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },

    /* ??? */
    { "UNKNOWN_G2_REG_0x5f78a0", 0x5f78a0, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "UNKNOWN_G2_REG_0x5f78a4", 0x5f78a4, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "UNKNOWN_G2_REG_0x5f78a8", 0x5f78a8, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "UNKNOWN_G2_REG_0x5f78ac", 0x5f78ac, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "UNKNOWN_G2_REG_0x5f78b0", 0x5f78b0, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "UNKNOWN_G2_REG_0x5f78b4", 0x5f78b4, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },
    { "UNKNOWN_G2_REG_0x5f78b8", 0x5f78b8, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },

    { "SB_G2APRO", 0x5f78bc, 4,
      warn_g2_reg_read_handler, warn_g2_reg_write_handler },

    { NULL }
};

int g2_reg_read(void *buf, size_t addr, size_t len) {
    struct g2_mem_mapped_reg *curs = g2_reg_info;

    while (curs->reg_name) {
        if (curs->addr == addr) {
            if (curs->len >= len) {
                return curs->on_read(curs, buf, addr, len);
            } else {
                error_set_address(addr);
                error_set_length(len);
                error_set_feature("Whatever happens when you use an "
                                  "inapproriate length while reading "
                                  "from a g2 register");
                PENDING_ERROR(ERROR_UNIMPLEMENTED);
                return MEM_ACCESS_FAILURE;
            }
        }
        curs++;
    }

    error_set_address(addr);
    error_set_feature("reading from one of the g2 registers");
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

int g2_reg_write(void const *buf, size_t addr, size_t len) {
    struct g2_mem_mapped_reg *curs = g2_reg_info;

    while (curs->reg_name) {
        if (curs->addr == addr) {
            if (curs->len >= len) {
                return curs->on_write(curs, buf, addr, len);
            } else {
                error_set_address(addr);
                error_set_length(len);
                error_set_feature("Whatever happens when you use an "
                                  "inapproriate length while writing to a g2 "
                                  "register");
                PENDING_ERROR(ERROR_UNIMPLEMENTED);
                return MEM_ACCESS_FAILURE;
            }
        }
        curs++;
    }

    error_set_address(addr);
    error_set_length(len);
    error_set_feature("writing to one of the g2 registers");
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static int
default_g2_reg_read_handler(struct g2_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_G2_FIRST) >> 2;
    memcpy(buf, idx + g2_regs, len);
    return MEM_ACCESS_SUCCESS;
}

static int
default_g2_reg_write_handler(struct g2_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_G2_FIRST) >> 2;
    memcpy(idx + g2_regs, buf, len);
    return MEM_ACCESS_SUCCESS;
}

static int
warn_g2_reg_read_handler(struct g2_mem_mapped_reg const *reg_info,
                         void *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    int ret_code = default_g2_reg_read_handler(reg_info, buf, addr, len);

    if (ret_code) {
        LOG_DBG("read from g2 register %s\n",
                reg_info->reg_name);
    } else {
        switch (len) {
        case 1:
            memcpy(&val8, buf, sizeof(val8));
            LOG_DBG("read 0x%02x from g2 register %s\n",
                    (unsigned)val8, reg_info->reg_name);
            break;
        case 2:
            memcpy(&val16, buf, sizeof(val16));
            LOG_DBG("read 0x%04x from g2 register %s\n",
                    (unsigned)val16, reg_info->reg_name);
            break;
        case 4:
            memcpy(&val32, buf, sizeof(val32));
            LOG_DBG("read 0x%08x from g2 register %s\n",
                    (unsigned)val32, reg_info->reg_name);
            break;
        default:
            LOG_DBG("read from g2 register %s\n", reg_info->reg_name);
        }
    }

    return MEM_ACCESS_SUCCESS;
}

static int
warn_g2_reg_write_handler(struct g2_mem_mapped_reg const *reg_info,
                          void const *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    switch (len) {
    case 1:
        memcpy(&val8, buf, sizeof(val8));
        LOG_DBG("write 0x%02x to g2 register %s\n",
                (unsigned)val8, reg_info->reg_name);
        break;
    case 2:
        memcpy(&val16, buf, sizeof(val16));
        LOG_DBG("write 0x%04x to g2 register %s\n",
                (unsigned)val16, reg_info->reg_name);
        break;
    case 4:
        memcpy(&val32, buf, sizeof(val32));
        LOG_DBG("write 0x%08x to g2 register %s\n",
                (unsigned)val32, reg_info->reg_name);
        break;
    default:
        LOG_DBG("write to g2 register %s\n",
                reg_info->reg_name);
    }

    return default_g2_reg_write_handler(reg_info, buf, addr, len);
}

static int sb_adst_reg_write_handler(struct g2_mem_mapped_reg const *reg_info,
                                     void const *buf, addr32_t addr, unsigned len) {
    uint32_t val = 0;

    if (len != sizeof(val)) {
        error_set_feature("weird ADST len");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    memcpy(&val, buf, sizeof(val));

    if (val) {
        error_set_feature("AICA DMA");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    return 0;
}
