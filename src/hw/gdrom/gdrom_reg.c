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

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "mount.h"
#include "error.h"
#include "mem_code.h"
#include "types.h"
#include "MemoryMap.h"
#include "hw/sys/holly_intc.h"
#include "hw/sh4/sh4.h"
#include "hw/sh4/sh4_dmac.h"
#include "cdrom.h"
#include "dreamcast.h"
#include "gdrom_response.h"
#include "gdrom.h"

#include "gdrom_reg.h"

static reg32_t
gdrom_get_status_reg(struct gdrom_status const *stat_in);
static reg32_t
gdrom_get_error_reg(struct gdrom_error const *error_in);
static reg32_t
gdrom_get_int_reason_reg(struct gdrom_int_reason *int_reason_in);

static void
gdrom_set_features_reg(struct gdrom_features *features_out, reg32_t feat_reg);
static void
gdrom_set_sect_cnt_reg(struct gdrom_sector_count *sect_cnt_out,
                       reg32_t sect_cnt_reg);
static void
gdrom_set_dev_ctrl_reg(struct gdrom_dev_ctrl *dev_ctrl_out,
                       reg32_t dev_ctrl_reg);

////////////////////////////////////////////////////////////////////////////////
//
// status flags (for REQ_STAT and the sector-number register)
//
////////////////////////////////////////////////////////////////////////////////

#define SEC_NUM_STATUS_SHIFT 0
#define SEC_NUM_STATUS_MASK (0xf << SEC_NUM_STATUS_SHIFT)

#define SEC_NUM_DISC_TYPE_SHIFT 4
#define SEC_NUM_DISC_TYPE_MASK (0xf << SEC_NUM_DISC_TYPE_SHIFT)

#define SEC_NUM_FMT_SHIFT 4
#define SEC_NUM_FMT_MASK (0xf << SEC_NUM_FMT_SHIFT)

#define N_GDROM_REGS (ADDR_GDROM_LAST - ADDR_GDROM_FIRST + 1)
static reg32_t gdrom_regs[N_GDROM_REGS];

struct gdrom_mem_mapped_reg;

typedef int(*gdrom_reg_read_handler_t)(
    struct gdrom_mem_mapped_reg const *reg_info,
    void *buf, addr32_t addr, unsigned len);
typedef int(*gdrom_reg_write_handler_t)(
    struct gdrom_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len);

static int
default_gdrom_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len);
static int
default_gdrom_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len);
static int
warn_gdrom_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len);
static int
warn_gdrom_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len);
static int
ignore_gdrom_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                               void const *buf, addr32_t addr, unsigned len)
    __attribute__((unused));

static int
gdrom_alt_status_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len);
static int
gdrom_status_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                          void *buf, addr32_t addr, unsigned len);

static int
gdrom_error_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                             void *buf, addr32_t addr, unsigned len);

static int
gdrom_cmd_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);

static int
gdrom_data_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len);
static int
gdrom_data_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len);

static int
gdrom_features_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len);

static int
gdrom_sect_cnt_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len);

static int
gdrom_dev_ctrl_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len);

static int
gdrom_int_reason_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len);


static int
gdrom_sector_num_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len);


static int
gdrom_byte_count_low_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                      void *buf, addr32_t addr, unsigned len);
static int
gdrom_byte_count_low_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                       void const *buf, addr32_t addr, unsigned len);
static int
gdrom_byte_count_high_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                       void *buf, addr32_t addr, unsigned len);
static int
gdrom_byte_count_high_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                        void const *buf, addr32_t addr, unsigned len);

static struct gdrom_mem_mapped_reg {
    char const *reg_name;

    addr32_t addr;

    unsigned len;

    gdrom_reg_read_handler_t on_read;
    gdrom_reg_write_handler_t on_write;
} gdrom_reg_info[] = {
    { "Drive Select", 0x5f7098, 4,
      warn_gdrom_reg_read_handler, warn_gdrom_reg_write_handler },
    { "Alt status/device control", 0x5f7018, 4,
      gdrom_alt_status_read_handler, gdrom_dev_ctrl_reg_write_handler },
    { "status/command", 0x5f709c, 4,
      gdrom_status_read_handler, gdrom_cmd_reg_write_handler },
    { "GD-ROM Data", 0x5f7080, 4,
      gdrom_data_reg_read_handler, gdrom_data_reg_write_handler },
    { "Error/features", 0x5f7084, 4,
      gdrom_error_reg_read_handler, gdrom_features_reg_write_handler },
    { "Interrupt reason/sector count", 0x5f7088, 4,
      gdrom_int_reason_reg_read_handler, gdrom_sect_cnt_reg_write_handler },
    { "Sector number", 0x5f708c, 4,
      gdrom_sector_num_reg_read_handler, warn_gdrom_reg_write_handler },
    { "Byte Count (low)", 0x5f7090, 4,
      gdrom_byte_count_low_reg_read_handler, gdrom_byte_count_low_reg_write_handler },
    { "Byte Count (high)", 0x5f7094, 4,
      gdrom_byte_count_high_reg_read_handler, gdrom_byte_count_high_reg_write_handler },
    { NULL }
};

int gdrom_reg_read(void *buf, size_t addr, size_t len) {
    struct gdrom_mem_mapped_reg *curs = gdrom_reg_info;

    while (curs->reg_name) {
        if (curs->addr == addr) {
            if (curs->len >= len) {
                return curs->on_read(curs, buf, addr, len);
            } else {
                error_set_feature("Whatever happens when you use an "
                                  "inappropriate length while reading from a "
                                  "gdrom register");
                error_set_address(addr);
                error_set_length(len);
                PENDING_ERROR(ERROR_UNIMPLEMENTED);
                return MEM_ACCESS_FAILURE;
            }
        }
        curs++;
    }

    error_set_feature("reading from one of the gdrom registers");
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

int gdrom_reg_write(void const *buf, size_t addr, size_t len) {
    struct gdrom_mem_mapped_reg *curs = gdrom_reg_info;

    while (curs->reg_name) {
        if (curs->addr == addr) {
            if (curs->len >= len) {
                return curs->on_write(curs, buf, addr, len);
            } else {
                error_set_feature("Whatever happens when you use an "
                                  "inappropriate length while writing to a "
                                  "gdrom register");
                error_set_address(addr);
                error_set_length(len);
                PENDING_ERROR(ERROR_UNIMPLEMENTED);
                return MEM_ACCESS_FAILURE;
            }
        }
        curs++;
    }

    error_set_feature("writing to one of the gdrom registers");
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static int
default_gdrom_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_GDROM_FIRST) >> 2;
    memcpy(buf, idx + gdrom_regs, len);
    return MEM_ACCESS_SUCCESS;
}

static int
default_gdrom_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_GDROM_FIRST) >> 2;
    memcpy(idx + gdrom_regs, buf, len);
    return MEM_ACCESS_SUCCESS;
}

static int
warn_gdrom_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                         void *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    int ret_code = default_gdrom_reg_read_handler(reg_info, buf, addr, len);

    if (ret_code) {
        GDROM_TRACE("read from register %s\n", reg_info->reg_name);
    } else {
        switch (len) {
        case 1:
            memcpy(&val8, buf, sizeof(val8));
            GDROM_TRACE("read 0x%02x from register %s\n",
                        (unsigned)val8, reg_info->reg_name);
            break;
        case 2:
            memcpy(&val16, buf, sizeof(val16));
            GDROM_TRACE("read 0x%04x from register %s\n",
                        (unsigned)val16, reg_info->reg_name);
            break;
        case 4:
            memcpy(&val32, buf, sizeof(val32));
            GDROM_TRACE("read 0x%08x from register %s\n",
                        (unsigned)val32, reg_info->reg_name);
            break;
        default:
            GDROM_TRACE("read from register %s\n", reg_info->reg_name);
        }
    }

    return ret_code;
}

static int
warn_gdrom_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                          void const *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    switch (len) {
    case 1:
        memcpy(&val8, buf, sizeof(val8));
        GDROM_TRACE("write 0x%02x to register %s\n",
                    (unsigned)val8, reg_info->reg_name);
        break;
    case 2:
        memcpy(&val16, buf, sizeof(val16));
        GDROM_TRACE("write 0x%04x to register %s\n",
                    (unsigned)val16, reg_info->reg_name);
        break;
    case 4:
        memcpy(&val32, buf, sizeof(val32));
        GDROM_TRACE("write 0x%08x to register %s\n",
                (unsigned)val32, reg_info->reg_name);
        break;
    default:
        GDROM_TRACE("write to register %s\n", reg_info->reg_name);
    }

    return default_gdrom_reg_write_handler(reg_info, buf, addr, len);
}

static int
ignore_gdrom_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                               void const *buf, addr32_t addr, unsigned len) {
    /* do nothing */
    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_alt_status_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len) {
    reg32_t stat_bin = gdrom_get_status_reg(&gdrom.stat_reg);
    GDROM_TRACE("read 0x%02x from alternate status register\n",
                (unsigned)stat_bin);
    memcpy(buf, &stat_bin, len > 4 ? 4 : len);

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_status_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                          void *buf, addr32_t addr, unsigned len) {
    /*
     * XXX
     * For the most part, I try to keep all the logic in gdrom.c and all the
     * encoding/decoding here in gdrom_reg.c (ie, gdrom.c manages the system
     * state and gdrom_reg.c translates data into/from the format the guest
     * software expects it to be in).
     *
     * This part here where I clear the interrupt flag is an exception to that
     * rule because I didn't it was worth it to add a layer of indirection to
     * this single function call.  If this function did more than just read from
     * a register and clear the interrupt flag, then I would have some
     * infrastructure in place to do that on its behalf in gdrom.c
     */
    holly_clear_ext_int(HOLLY_EXT_INT_GDROM);

    reg32_t stat_bin = gdrom_get_status_reg(&gdrom.stat_reg);
    GDROM_TRACE("read 0x%02x from status register\n", (unsigned)stat_bin);

    memcpy(buf, &stat_bin, len > 4 ? 4 : len);

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_error_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                             void *buf, addr32_t addr, unsigned len) {
    reg32_t tmp = gdrom_get_error_reg(&gdrom.error_reg);
    GDROM_TRACE("read 0x%02x from error register\n", (unsigned)tmp);

    memcpy(buf, &tmp, len > 4 ? 4 : len);

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_cmd_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    reg32_t cmd = 0;
    size_t n_bytes = len < sizeof(cmd) ? len : sizeof(cmd);

    memcpy(&cmd, buf, n_bytes);

    GDROM_TRACE("write 0x%x to command register (%u bytes)\n",
           (unsigned)cmd, (unsigned)n_bytes);

    gdrom_input_cmd(cmd);

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_data_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len) {
    gdrom_read_data((uint8_t*)buf, len);
    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_data_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len) {
    gdrom_write_data((uint8_t*)buf, len);
    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_features_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len) {
    reg32_t tmp;
    size_t n_bytes = len < sizeof(tmp) ? len : sizeof(tmp);

    memcpy(&tmp, buf, n_bytes);

    GDROM_TRACE("write 0x%08x to the features register\n", (unsigned)tmp);

    gdrom_set_features_reg(&gdrom.feat_reg, tmp);

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_sect_cnt_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len) {
    reg32_t tmp;
    size_t n_bytes = len < sizeof(tmp) ? len : sizeof(tmp);

    memcpy(&tmp, buf, n_bytes);

    GDROM_TRACE("Write %08x to sec_cnt_reg\n", (unsigned)tmp);

    gdrom_set_sect_cnt_reg(&gdrom.sect_cnt_reg, tmp);

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_dev_ctrl_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len) {
    reg32_t tmp;
    size_t n_bytes = len < sizeof(tmp) ? len : sizeof(tmp);

    memcpy(&tmp, buf, n_bytes);

    gdrom_set_dev_ctrl_reg(&gdrom.dev_ctrl_reg, tmp);

    GDROM_TRACE("Write %08x to dev_ctrl_reg\n", (unsigned)tmp);

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_int_reason_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len) {
    reg32_t tmp = gdrom_get_int_reason_reg(&gdrom.int_reason_reg);
    size_t n_bytes = len < sizeof(tmp) ? len : sizeof(tmp);

    GDROM_TRACE("int_reason is 0x%08x\n", (unsigned)tmp);

    memcpy(buf, &tmp, n_bytes);

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_sector_num_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len) {
    uint32_t status;

    status = ((uint32_t)gdrom_get_drive_state() << SEC_NUM_STATUS_SHIFT) |
        ((uint32_t)gdrom_get_disc_type() << SEC_NUM_DISC_TYPE_SHIFT);

    GDROM_TRACE("read 0x%02x from the sector number\n", (unsigned)status);

    memcpy(buf, &status, len < sizeof(status) ? len : sizeof(status));

    return 0;
}

static int
gdrom_byte_count_low_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                      void *buf, addr32_t addr, unsigned len) {
    uint32_t low = gdrom.data_byte_count & 0xff;
    memcpy(buf, &low, len < sizeof(low) ? len : sizeof(low));

    GDROM_TRACE("read 0x%02x from byte_count_low\n", (unsigned)low);

    return 0;
}

static int
gdrom_byte_count_low_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                       void const *buf, addr32_t addr, unsigned len) {
    uint32_t tmp = 0;
    memcpy(&tmp, buf, len < sizeof(tmp) ? len : sizeof(tmp));

    gdrom.data_byte_count = (gdrom.data_byte_count & ~0xff) | (tmp & 0xff);
    GDROM_TRACE("write 0x%02x to byte_count_low\n", (unsigned)(tmp & 0xff));

    return 0;
}

static int
gdrom_byte_count_high_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                       void *buf, addr32_t addr, unsigned len) {
    uint32_t high = (gdrom.data_byte_count & 0xff00) >> 8;
    memcpy(buf, &high, len < sizeof(high) ? len : sizeof(high));

    GDROM_TRACE("read 0x%02x from byte_count_high\n", (unsigned)high);

    return 0;
}

static int
gdrom_byte_count_high_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                        void const *buf, addr32_t addr, unsigned len) {
    uint32_t tmp = 0;
    memcpy(&tmp, buf, len < sizeof(tmp) ? len : sizeof(tmp));

    gdrom.data_byte_count =
        (gdrom.data_byte_count & ~0xff00) | ((tmp & 0xff) << 8);
    GDROM_TRACE("write 0x%02x to byte_count_high\n",
                (unsigned)((tmp & 0xff) << 8));
    return 0;
}

int
gdrom_gdapro_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &gdrom.gdapro_reg, len);
    GDROM_TRACE("read %08x from GDAPRO\n", gdrom.gdapro_reg);

    return 0;
}

int
gdrom_gdapro_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                               void const *buf, addr32_t addr, unsigned len) {
    // the g1 bus code will make sure len is equal to 4
    uint32_t val = *(uint32_t*)buf;

    // check security code
    if ((val & 0xffff0000) != 0x88430000)
        return 0;

    gdrom.gdapro_reg = val;

    GDROM_TRACE("GDAPRO (0x%08x) - allowing writes from 0x%08x through "
                "0x%08x\n",
                gdrom.gdapro_reg, gdrom_dma_prot_top(), gdrom_dma_prot_bot());

    return 0;
}

int
gdrom_g1gdrc_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &gdrom.g1gdrc_reg, len);
    GDROM_TRACE("read %08x from G1GDRC\n", gdrom.g1gdrc_reg);
    return 0;
}

int
gdrom_g1gdrc_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                               void const *buf, addr32_t addr, unsigned len) {
    memcpy(&gdrom.g1gdrc_reg, buf, sizeof(gdrom.g1gdrc_reg));
    GDROM_TRACE("write %08x to G1GDRC\n", gdrom.g1gdrc_reg);
    return 0;
}

int
gdrom_gdstar_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &gdrom.dma_start_addr_reg, len);
    GDROM_TRACE("read %08x from GDSTAR\n", gdrom.dma_start_addr_reg);
    return 0;
}

int
gdrom_gdstar_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                               void const *buf, addr32_t addr, unsigned len) {
    memcpy(&gdrom.dma_start_addr_reg, buf, sizeof(gdrom.dma_start_addr_reg));
    gdrom.dma_start_addr_reg &= ~0xe0000000;
    GDROM_TRACE("write %08x to GDSTAR\n", gdrom.dma_start_addr_reg);
    return 0;
}

int
gdrom_gdlen_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                             void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &gdrom.dma_len_reg, len);
    GDROM_TRACE("read %08x from GDLEN\n", gdrom.dma_len_reg);
    return 0;
}

int
gdrom_gdlen_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                              void const *buf, addr32_t addr, unsigned len) {
    memcpy(&gdrom.dma_len_reg, buf, sizeof(gdrom.dma_len_reg));
    GDROM_TRACE("write %08x to GDLEN\n", gdrom.dma_len_reg);
    return 0;
}

int
gdrom_gddir_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                             void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &gdrom.dma_dir_reg, len);
    GDROM_TRACE("read %08x from GDDIR\n", gdrom.dma_dir_reg);
    return 0;
}

int
gdrom_gddir_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                              void const *buf, addr32_t addr, unsigned len) {
    memcpy(&gdrom.dma_dir_reg, buf, sizeof(gdrom.dma_dir_reg));
    GDROM_TRACE("write %08x to GDDIR\n", gdrom.dma_dir_reg);
    return 0;
}

int
gdrom_gden_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &gdrom.dma_en_reg, len);
    GDROM_TRACE("read %08x from GDEN\n", gdrom.dma_en_reg);
    return 0;
}

int
gdrom_gden_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len) {
    memcpy(&gdrom.dma_en_reg, buf, sizeof(gdrom.dma_en_reg));
    GDROM_TRACE("write %08x to GDEN\n", gdrom.dma_en_reg);
    return 0;
}

int
gdrom_gdst_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &gdrom.dma_start_reg, len);
    GDROM_TRACE("read %08x from GDST\n", gdrom.dma_start_reg);
    return 0;
}

int
gdrom_gdst_reg_write_handler(struct g1_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len) {
    memcpy(&gdrom.dma_start_reg, buf, sizeof(gdrom.dma_start_reg));
    GDROM_TRACE("write %08x to GDST\n", gdrom.dma_start_reg);

    gdrom_start_dma();

    return 0;
}

int
gdrom_gdlend_reg_read_handler(struct g1_mem_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len) {
    memcpy(buf, &gdrom.gdlend_reg, len);
    GDROM_TRACE("read %08x from GDLEND\n", gdrom.gdlend_reg);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Error register flags
//
////////////////////////////////////////////////////////////////////////////////

#define GDROM_ERROR_SENSE_KEY_SHIFT 4
#define GDROM_ERROR_SENSE_KEY_MASK (0xf << GDROM_ERROR_SENSE_KEY_SHIFT)

#define GDROM_ERROR_MCR_SHIFT 3
#define GDROM_ERROR_MCR_MASK (1 << GDROM_ERROR_MCR_SHIFT)

#define GDROM_ERROR_ABRT_SHIFT 2
#define GDROM_ERROR_ABRT_MASK (1 << GDROM_ERROR_ABRT_SHIFT)

#define GDROM_ERROR_EOMF_SHIFT 1
#define GDROM_ERROR_EOMF_MASK (1 << GDROM_ERROR_EOMF_SHIFT)

#define GDROM_ERROR_ILI_SHIFT 0
#define GDROM_ERROR_ILI_MASK (1 << GDROM_ERROR_ILI_SHIFT)

static reg32_t gdrom_get_error_reg(struct gdrom_error const *error_in) {
    reg32_t error_reg =
        (((reg32_t)error_in->sense_key) << GDROM_ERROR_SENSE_KEY_SHIFT) &
        GDROM_ERROR_SENSE_KEY_MASK;

    if (error_in->ili)
        error_reg |= GDROM_ERROR_ILI_MASK;
    if (error_in->eomf)
        error_reg |= GDROM_ERROR_EOMF_MASK;
    if (error_in->abrt)
        error_reg |= GDROM_ERROR_ABRT_MASK;
    if (error_in->mcr)
        error_reg |= GDROM_ERROR_MCR_MASK;

    return error_reg;
}

////////////////////////////////////////////////////////////////////////////////
//
// Status register flags
//
////////////////////////////////////////////////////////////////////////////////

// the drive is processing a command
#define GDROM_STAT_BSY_SHIFT 7
#define GDROM_STAT_BSY_MASK (1 << GDROM_STAT_BSY_SHIFT)

// response to ATA command is possible
#define GDROM_STAT_DRDY_SHIFT 6
#define GDROM_STAT_DRDY_MASK (1 << GDROM_STAT_DRDY_SHIFT)

// drive fault
#define GDROM_STAT_DF_SHIFT 5
#define GDROM_STAT_DF_MASK (1 << GDROM_STAT_DF_SHIFT)

// seek processing is complete
#define GDROM_STAT_DSC_SHIFT 4
#define GDROM_STAT_DSC_MASK (1 << GDROM_STAT_DSC_SHIFT)

// data transfer possible
#define GDROM_STAT_DRQ_SHIFT 3
#define GDROM_STAT_DRQ_MASK (1 << GDROM_STAT_DRQ_SHIFT)

// correctable error flag
#define GDROM_STAT_CORR_SHIFT 2
#define GDROM_STAT_CORR_MASK (1 << GDROM_STAT_CORR_SHIFT)

// error flag
#define GDROM_STAT_CHECK_SHIFT 0
#define GDROM_STAT_CHECK_MASK (1 << GDROM_STAT_CHECK_SHIFT)

static reg32_t gdrom_get_status_reg(struct gdrom_status const *stat_in) {
    reg32_t stat_reg = 0;

    if (stat_in->bsy)
        stat_reg |= GDROM_STAT_BSY_MASK;
    if (stat_in->drdy)
        stat_reg |= GDROM_STAT_DRDY_MASK;
    if (stat_in->df)
        stat_reg |= GDROM_STAT_DF_MASK;
    if (stat_in->dsc)
        stat_reg |= GDROM_STAT_DSC_MASK;
    if (stat_in->drq)
        stat_reg |= GDROM_STAT_DRQ_MASK;
    if (stat_in->corr)
        stat_reg |= GDROM_STAT_CORR_MASK;
    if (stat_in->check)
        stat_reg |= GDROM_STAT_CHECK_MASK;

    return stat_reg;
}

////////////////////////////////////////////////////////////////////////////////
//
// feature register flags
//
////////////////////////////////////////////////////////////////////////////////

#define FEAT_REG_DMA_SHIFT 0
#define FEAT_REG_DMA_MASK (1 << FEAT_REG_DMA_SHIFT)

static void
gdrom_set_features_reg(struct gdrom_features *features_out, reg32_t feat_reg) {
    if (feat_reg & FEAT_REG_DMA_MASK)
        features_out->dma_enable = true;
    else
        features_out->dma_enable = false;

    if ((feat_reg & 0x7f) == 3)
        features_out->set_feat_enable = true;
    else
        features_out->set_feat_enable = false;
}

////////////////////////////////////////////////////////////////////////////////
//
// Transfer Modes (for the sector count register in GDROM_CMD_SEAT_FEAT)
//
////////////////////////////////////////////////////////////////////////////////

#define TRANS_MODE_PIO_DFLT_MASK        0xfe
#define TRANS_MODE_PIO_DFLT_VAL         0x00

#define TRANS_MODE_PIO_FLOW_CTRL_MASK   0xf8
#define TRANS_MODE_PIO_FLOW_CTRL_VAL    0x08

#define TRANS_MODE_SINGLE_WORD_DMA_MASK 0xf8
#define TRANS_MODE_SINGLE_WORD_DMA_VAL  0x10

#define TRANS_MODE_MULTI_WORD_DMA_MASK  0xf8
#define TRANS_MODE_MULTI_WORD_DMA_VAL   0x20

#define TRANS_MODE_PSEUDO_DMA_MASK      0xf8
#define TRANS_MODE_PSEUDO_DMA_VAL       0x18

#define SECT_CNT_MODE_VAL_SHIFT 0
#define SECT_CNT_MODE_VAL_MASK (0xf << SECT_CNT_MODE_VAL_SHIFT)

static void
gdrom_set_sect_cnt_reg(struct gdrom_sector_count *sect_cnt_out,
                       reg32_t sect_cnt_reg) {
    unsigned mode_val =
        (sect_cnt_reg & SECT_CNT_MODE_VAL_MASK) >> SECT_CNT_MODE_VAL_SHIFT;
    if ((sect_cnt_reg & TRANS_MODE_PIO_DFLT_MASK) ==
        TRANS_MODE_PIO_DFLT_VAL) {
        sect_cnt_out->trans_mode = TRANS_MODE_PIO_DFLT;
    } else if ((sect_cnt_reg & TRANS_MODE_PIO_FLOW_CTRL_MASK) ==
               TRANS_MODE_PIO_FLOW_CTRL_VAL) {
        sect_cnt_out->trans_mode = TRANS_MODE_PIO_FLOW_CTRL;
    } else if ((sect_cnt_reg & TRANS_MODE_SINGLE_WORD_DMA_MASK) ==
               TRANS_MODE_SINGLE_WORD_DMA_VAL) {
        sect_cnt_out->trans_mode = TRANS_MODE_SINGLE_WORD_DMA;
    } else if ((sect_cnt_reg & TRANS_MODE_MULTI_WORD_DMA_MASK) ==
               TRANS_MODE_MULTI_WORD_DMA_VAL) {
        sect_cnt_out->trans_mode = TRANS_MODE_MULTI_WORD_DMA;
    } else if ((sect_cnt_reg & TRANS_MODE_PSEUDO_DMA_MASK) ==
               TRANS_MODE_PSEUDO_DMA_VAL) {
        sect_cnt_out->trans_mode = TRANS_MODE_PSEUDO_DMA;
    } else {
        // TODO: maybe this should be a soft warning instead of an error
        GDROM_TRACE("unrecognized transfer mode (sec_cnt_reg is 0x%08x)\n",
                    sect_cnt_reg);
        error_set_feature("unrecognized transfer mode\n");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    sect_cnt_out->mode_val = mode_val;
}

////////////////////////////////////////////////////////////////////////////////
//
// Interrupt Reason register flags
//
////////////////////////////////////////////////////////////////////////////////

// ready to receive command
#define INT_REASON_COD_SHIFT 0
#define INT_REASON_COD_MASK (1 << INT_REASON_COD_SHIFT)

/*
 * ready to receive data from software to drive if set
 * ready to send data from drive to software if not set
 */
#define INT_REASON_IO_SHIFT 1
#define INT_REASON_IO_MASK (1 << INT_REASON_IO_SHIFT)

static reg32_t gdrom_get_int_reason_reg(struct gdrom_int_reason *int_reason_in) {
    reg32_t reg_out = 0;

    if (int_reason_in->cod)
        reg_out |= INT_REASON_COD_MASK;
    if (int_reason_in->io)
        reg_out |= INT_REASON_IO_MASK;

    return reg_out;
}

////////////////////////////////////////////////////////////////////////////////
//
// Device control register flags
//
////////////////////////////////////////////////////////////////////////////////

#define DEV_CTRL_NIEN_SHIFT 1
#define DEV_CTRL_NIEN_MASK (1 << DEV_CTRL_NIEN_SHIFT)

#define DEV_CTRL_SRST_SHIFT 2
#define DEV_CTRL_SRST_MASK (1 << DEV_CTRL_SRST_SHIFT)

static void
gdrom_set_dev_ctrl_reg(struct gdrom_dev_ctrl *dev_ctrl_out,
                       reg32_t dev_ctrl_reg) {
    dev_ctrl_out->nien = (bool)(dev_ctrl_reg & DEV_CTRL_NIEN_MASK);
    dev_ctrl_out->srst = (bool)(dev_ctrl_reg & DEV_CTRL_SRST_MASK);
}
