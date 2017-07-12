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

#include "sh4.h"
#include "sh4_dmac.h"
#include "error.h"
#include "mem_code.h"
#include "mem_areas.h"
#include "types.h"
#include "MemoryMap.h"

#include "maple_reg.h"

#define N_MAPLE_REGS (ADDR_MAPLE_LAST - ADDR_MAPLE_FIRST + 1)
static reg32_t maple_regs[N_MAPLE_REGS];

static addr32_t maple_dma_prot_bot = 0;
static addr32_t maple_dma_prot_top = (0x1 << 27) | (0x7f << 20);
static addr32_t maple_dma_cmd_start;

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

static int
write_only_maple_reg_read_handler(struct maple_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len);

static int
mden_reg_read_handler(struct maple_mapped_reg const *reg_info,
                      void *buf, addr32_t addr, unsigned len);
static int
mden_reg_write_handler(struct maple_mapped_reg const *reg_info,
                       void const *buf, addr32_t addr, unsigned len);
static int
mdstar_reg_read_handler(struct maple_mapped_reg const *reg_info,
                        void *buf, addr32_t addr, unsigned len);
static int
mdstar_reg_write_handler(struct maple_mapped_reg const *reg_info,
                         void const *buf, addr32_t addr, unsigned len);
static int
mdtsel_reg_read_handler(struct maple_mapped_reg const *reg_info,
                        void *buf, addr32_t addr, unsigned len);
static int
mdtsel_reg_write_handler(struct maple_mapped_reg const *reg_info,
                         void const *buf, addr32_t addr, unsigned len);
static int
mdst_reg_read_handler(struct maple_mapped_reg const *reg_info,
                      void *buf, addr32_t addr, unsigned len);
static int
mdst_reg_write_handler(struct maple_mapped_reg const *reg_info,
                       void const *buf, addr32_t addr, unsigned len);

static struct maple_mapped_reg {
    char const *reg_name;

    addr32_t addr;

    unsigned len;

    maple_reg_read_handler_t on_read;
    maple_reg_write_handler_t on_write;
} maple_reg_info[] = {
    { "SB_MDSTAR", 0x5f6c04, 4,
      mdstar_reg_read_handler, mdstar_reg_write_handler },
    { "SB_MDTSEL", 0x5f6c10, 4,
      mdtsel_reg_read_handler, mdtsel_reg_write_handler },
    { "SB_MDEN", 0x5f6c14, 4,
      mden_reg_read_handler, mden_reg_write_handler },
    { "SB_MDST", 0x5f6c18, 4,
      mdst_reg_read_handler, mdst_reg_write_handler },
    { "SB_MSYS", 0x5f6c80, 4,
      warn_maple_reg_read_handler, warn_maple_reg_write_handler },
    { "SB_MDAPRO", 0x5f6c8c, 4,
      write_only_maple_reg_read_handler, warn_maple_reg_write_handler },
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
                PENDING_ERROR(ERROR_UNIMPLEMENTED);
                return MEM_ACCESS_FAILURE;
            }
        }
        curs++;
    }

    error_set_address(addr);
    error_set_length(len);
    error_set_feature("reading from one of the maple registers");
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
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
                PENDING_ERROR(ERROR_UNIMPLEMENTED);
                return MEM_ACCESS_FAILURE;
            }
        }
        curs++;
    }

    error_set_address(addr);
    error_set_length(len);
    error_set_feature("writing to one of the maple registers");
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static int
default_maple_reg_read_handler(struct maple_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_MAPLE_FIRST) >> 2;
    memcpy(buf, idx + maple_regs, len);
    return MEM_ACCESS_SUCCESS;
}

static int
default_maple_reg_write_handler(struct maple_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_MAPLE_FIRST) >> 2;
    memcpy(idx + maple_regs, buf, len);
    return MEM_ACCESS_SUCCESS;
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

    return MEM_ACCESS_SUCCESS;
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

static int
write_only_maple_reg_read_handler(struct maple_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len) {
    error_set_feature("whatever happens when you read from "
                      "a write-only register");
    error_set_address(addr);
    error_set_length(len);
    PENDING_ERROR(ERROR_UNIMPLEMENTED);
    return MEM_ACCESS_FAILURE;
}

static int
mden_reg_read_handler(struct maple_mapped_reg const *reg_info,
                      void *buf, addr32_t addr, unsigned len) {
    uint32_t val_out = 0;

    memcpy(buf, &val_out, len);

    fprintf(stderr, "WARNING: reading 0 from maple\'s SB_MDEN register\n");

    return 0;
}

static int
mden_reg_write_handler(struct maple_mapped_reg const *reg_info,
                       void const *buf, addr32_t addr, unsigned len) {
    uint32_t val;
    memcpy(&val, buf, sizeof(val));

    if (val)
        fprintf(stderr, "WARNING: enabling maplebus DMA\n");
    else
        fprintf(stderr, "WARNING: aborting maplebus DMA\n");

    return 0;
}

static int
mdstar_reg_read_handler(struct maple_mapped_reg const *reg_info,
                        void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &maple_dma_cmd_start, len);
    fprintf(stderr, "reading %08x from MDSTAR\n",
            (unsigned)maple_dma_cmd_start);
    return 0;
}

static int
mdstar_reg_write_handler(struct maple_mapped_reg const *reg_info,
                         void const *buf, addr32_t addr, unsigned len) {
    memcpy(&maple_dma_cmd_start, buf, sizeof(maple_dma_cmd_start));
    fprintf(stderr, "writing %08x to MDSTAR\n",
            (unsigned)maple_dma_cmd_start);
    return 0;
}

static int
mdtsel_reg_read_handler(struct maple_mapped_reg const *reg_info,
                        void *buf, addr32_t addr, unsigned len) {
    uint32_t val = 0;
    memcpy(buf, &val, len);

    fprintf(stderr, "reading 0 from MDTSEL\n");

    return 0;
}

static int
mdtsel_reg_write_handler(struct maple_mapped_reg const *reg_info,
                         void const *buf, addr32_t addr, unsigned len) {
    uint32_t val;
    memcpy(&val, buf, sizeof(val));

    if (val) {
        error_set_feature("vblank Maple-DMA initialization");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    return 0;
}

static int
mdst_reg_read_handler(struct maple_mapped_reg const *reg_info,
                      void *buf, addr32_t addr, unsigned len) {
    uint32_t val = 0;
    memcpy(buf, &val, len);

    fprintf(stderr, "WARNING: reading 0 from MDST\n");

    return 0;
}

static int
mdst_reg_write_handler(struct maple_mapped_reg const *reg_info,
                       void const *buf, addr32_t addr, unsigned len) {
    uint32_t val;
    memcpy(&val, buf, sizeof(val));

    if (val) {
        fprintf(stderr, "WARNING: starting maple DMA operation\n");
        fprintf(stderr, "\tstarting address is %08x\n",
                (unsigned)maple_dma_cmd_start);
        addr32_t addr = maple_dma_cmd_start;
        bool last_packet;
        unsigned packet_no = 0;
        do {
            fprintf(stderr, "packet %u\n", packet_no++);
            uint32_t msg_length_port;

            sh4_dmac_transfer_from_mem(addr, sizeof(msg_length_port),
                                       1, &msg_length_port);
            addr += 4;

            uint32_t len = msg_length_port & 0xff;
            uint32_t port = (msg_length_port & (0x3 << 16)) >> 16;
            uint32_t ptrn = (msg_length_port & (0x7 << 8)) >> 8;
            last_packet = (bool)(msg_length_port & (1 << 31));

            fprintf(stderr, "\treading %u bytes (port is %u)\n",
                    (unsigned)len, (unsigned)port);
            fprintf(stderr, "\tthe pattern is %02x\n", (unsigned)ptrn);

            uint32_t recv_addr;
            sh4_dmac_transfer_from_mem(addr, sizeof(recv_addr),
                                       1, &recv_addr);
            addr += 4;

            printf("\tthe receive address is 0x%08x\n", (unsigned)recv_addr);

            uint32_t cmd_addr_pack_len;
            sh4_dmac_transfer_from_mem(addr, sizeof(cmd_addr_pack_len),
                                       1, &cmd_addr_pack_len);
            addr += 4;

            fprintf(stderr, "\tthe command is %02x\n",
                    cmd_addr_pack_len & 0xff);
            fprintf(stderr, "\tthe addr is %02x\n",
                    (cmd_addr_pack_len >> 8) & 0xff);
            fprintf(stderr, "\tthe packet length is %02x\n",
                    (cmd_addr_pack_len >> 24) & 0xff);

            if (last_packet)
                fprintf(stderr, "\tthis was the last packet\n");
            else
                fprintf(stderr, "\tthis was not the last packet\n");

            addr += len * 4;
        } while(!last_packet);
    }

    return 0;
}

addr32_t maple_get_dma_prot_bot(void) {
    return maple_dma_prot_bot;
}

addr32_t maple_get_dma_prot_top(void) {
    return maple_dma_prot_top;
}
