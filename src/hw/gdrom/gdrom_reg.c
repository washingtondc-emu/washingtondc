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
#include <stdbool.h>
#include <stdlib.h>

#include "mount.h"
#include "error.h"
#include "mem_code.h"
#include "types.h"
#include "MemoryMap.h"
#include "hw/sys/holly_intc.h"
#include "cdrom.h"

#include "gdrom_reg.h"

////////////////////////////////////////////////////////////////////////////////
//
// ATA commands
//
////////////////////////////////////////////////////////////////////////////////

#define GDROM_CMD_RESET    0x08
#define GDROM_CMD_DIAG     0x90
#define GDROM_CMD_NOP      0x00
#define GDROM_CMD_PKT      0xa0
#define GDROM_CMD_IDENTIFY 0xa1
#define GDROM_CMD_SET_FEAT 0xef

////////////////////////////////////////////////////////////////////////////////
//
// Packet Commands
//
////////////////////////////////////////////////////////////////////////////////

#define GDROM_PKT_TEST_UNIT  0x00
#define GDROM_PKT_REQ_STAT   0x10
#define GDROM_PKT_REQ_MODE   0x11
#define GDROM_PKT_READ_TOC   0x14
#define GDROM_PKT_READ       0x30
#define GDROM_PKT_START_DISK 0x70
#define GDROM_PKT_UNKNOWN_71 0x71

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


////////////////////////////////////////////////////////////////////////////////
//
// Status register flags
//
////////////////////////////////////////////////////////////////////////////////

// the drive is processing a command
#define STAT_BSY_SHIFT 7
#define STAT_BSY_MASK (1 << STAT_BSY_SHIFT)

// response to ATA command is possible
#define STAT_DRDY_SHIFT 6
#define STAT_DRDY_MASK (1 << STAT_DRDY_SHIFT)

// drive fault
#define STAT_DF_SHIFT 5
#define STAT_DF_MASK (1 << STAT_DF_SHIFT)

// seek processing is complete
#define STAT_DSC_SHIFT 4
#define STAT_DSC_MASK (1 << STAT_DSC_SHIFT)

// data transfer possible
#define STAT_DRQ_SHIFT 3
#define STAT_DRQ_MASK (1 << STAT_DRQ_SHIFT)

// correctable error flag
#define STAT_CORR_SHIFT 2
#define STAT_CORR_MASK (1 << STAT_CORR_SHIFT)

// error flag
#define STAT_CHECK_SHIFT 0
#define STAT_CHECK_MASK (1 << STAT_CHECK_SHIFT)

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

////////////////////////////////////////////////////////////////////////////////
//
// Device control register flags
//
////////////////////////////////////////////////////////////////////////////////

#define DEV_CTRL_NIEN_SHIFT 1
#define DEV_CTRL_NIEN_MASK (1 << DEV_CTRL_NIEN_SHIFT)

#define DEV_CTRL_SRST_SHIFT 2
#define DEV_CTRL_SRST_MASK (1 << DEV_CTRL_SRST_SHIFT)

////////////////////////////////////////////////////////////////////////////////
//
// status flags (for REQ_STAT and the sector-number register)
//
////////////////////////////////////////////////////////////////////////////////

enum gdrom_disc_state {
    GDROM_STATE_BUSY  = 0x0,
    GDROM_STATE_PAUSE = 0x1,
    GDROM_STATE_STANDBY = 0x2,
    GDROM_STATE_PLAY = 0x3,
    GDROM_STATE_SEEK = 0x4,
    GDROM_STATE_SCAN = 0x5,
    GDROM_STATE_OPEN = 0x6,
    GDROM_STATE_NODISC = 0x7,
    GDROM_STATE_RETRY = 0x8,
    GDROM_STATE_ERROR = 0x9
};

enum gdrom_fmt {
    GDROM_FMT_CDDA = 0,
    GDROM_FMT_CDROM = 1,
    GDROM_FMT_XA = 2,
    GDROM_FMT_CDI = 3,
    GDROM_FMT_GDROM = 8
};

#define SEC_NUM_STATUS_SHIFT 0
#define SEC_NUM_STATUS_MASK (0xf << SEC_NUM_STATUS_SHIFT)

#define SEC_NUM_FMT_SHIFT 4
#define SEC_NUM_FMT_MASK (0xf << SEC_NUM_FMT_SHIFT)

////////////////////////////////////////////////////////////////////////////////
//
// GD-ROM drive state
//
////////////////////////////////////////////////////////////////////////////////

static uint32_t stat_reg;       // status register
static uint32_t feat_reg;       // features register
static uint32_t sect_cnt_reg;   // sector count register
static uint32_t int_reason_reg; // interrupt reason register
static uint32_t dev_ctrl_reg;   // device control register
static unsigned data_byte_count;     // byte-count low/high registers

enum {
    TRANS_MODE_PIO_DFLT,
    TRANS_MODE_PIO_FLOW_CTRL,
    TRANS_MODE_SINGLE_WORD_DMA,
    TRANS_MODE_MULTI_WORD_DMA,
    TRANS_MODE_PSEUDO_DMA,

    TRANS_MODE_COUNT
};
static uint32_t trans_mode_vals[TRANS_MODE_COUNT];

enum gdrom_state {
    GDROM_STATE_NORM,
    GDROM_STATE_INPUT_PKT
};

static enum gdrom_state state;

#define PKT_LEN 12
static uint8_t pkt_buf[PKT_LEN];

// Empty out the bufq and free resources.
static void bufq_clear(void);

/*
 * grab one byte from the queue, pop/clear a node (if necessary) and return 0.
 * this returns non-zero if the queue is empty.
 */
static int bufq_consume_byte(unsigned *byte);

////////////////////////////////////////////////////////////////////////////////
//
// GD-ROM data queueing
//
////////////////////////////////////////////////////////////////////////////////

/*
 * 2352 was chosen as the size because that's the most that can be used at a
 * time on a CD (frame size)
 *
 * Most disc accesses will only use 2048 bytes, and some will use far
 * less than that (such as GDROM_PKT_REQ_MODE)
 */
#define GDROM_BUFQ_LEN CDROM_FRAME_SIZE

struct gdrom_bufq_node {
    struct fifo_node fifo_node;

    // idx is the index of the next valid access
    // len is the number of bytes which are valid
    // when idx == len, this buffer is empty and should be removed
    unsigned idx, len;
    uint8_t dat[GDROM_BUFQ_LEN];
};

static struct fifo_head bufq = FIFO_HEAD_INITIALIZER(bufq);

////////////////////////////////////////////////////////////////////////////////
//
// GD-ROM drive function implementation
//
////////////////////////////////////////////////////////////////////////////////

/* get off the phone! */
__attribute__((unused)) static bool bsy_signal() {
    return (bool)(stat_reg & STAT_BSY_MASK);
}

__attribute__((unused)) static bool drq_signal() {
    return (bool)(stat_reg & STAT_DRQ_MASK);
}

unsigned n_bytes_received;

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
      gdrom_int_reason_reg_read_handler, warn_gdrom_reg_write_handler },
    { "Sector number", 0x5f708c, 4,
      gdrom_sector_num_reg_read_handler, warn_gdrom_reg_write_handler },
    { "Byte Count (low)", 0x5f7090, 4,
      gdrom_byte_count_low_reg_read_handler, gdrom_byte_count_low_reg_write_handler },
    { "Byte Count (high)", 0x5f7094, 4,
      gdrom_byte_count_high_reg_read_handler, gdrom_byte_count_high_reg_write_handler },
    { NULL }
};

static void gdrom_cmd_set_features(void);

// called when the packet command (0xa0) is written to the cmd register
static void gdrom_cmd_begin_packet(void);

static void gdrom_cmd_identify(void);

// called when all 12 bytes of a packet have been written to data
static void gdrom_input_packet(void);

static void gdrom_input_req_mode_packet(void);

static void gdrom_input_test_unit_packet(void);

static void gdrom_input_start_disk_packet(void);

static void gdrom_input_read_toc_packet(void);

static void gdrom_input_read_packet(void);

static void gdrom_input_packet_71(void);

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
        fprintf(stderr, "WARNING: read from gdrom register %s\n",
                reg_info->reg_name);
    } else {
        switch (len) {
        case 1:
            memcpy(&val8, buf, sizeof(val8));
            fprintf(stderr, "WARNING: read 0x%02x from gdrom register %s\n",
                    (unsigned)val8, reg_info->reg_name);
            break;
        case 2:
            memcpy(&val16, buf, sizeof(val16));
            fprintf(stderr, "WARNING: read 0x%04x from gdrom register %s\n",
                    (unsigned)val16, reg_info->reg_name);
            break;
        case 4:
            memcpy(&val32, buf, sizeof(val32));
            fprintf(stderr, "WARNING: read 0x%08x from gdrom register %s\n",
                    (unsigned)val32, reg_info->reg_name);
            break;
        default:
            fprintf(stderr, "WARNING: read from gdrom register %s\n",
                    reg_info->reg_name);
        }
    }

    return 0;
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
        fprintf(stderr, "WARNING: writing 0x%02x to gdrom register %s\n",
                (unsigned)val8, reg_info->reg_name);
        break;
    case 2:
        memcpy(&val16, buf, sizeof(val16));
        fprintf(stderr, "WARNING: writing 0x%04x to gdrom register %s\n",
                (unsigned)val16, reg_info->reg_name);
        break;
    case 4:
        memcpy(&val32, buf, sizeof(val32));
        fprintf(stderr, "WARNING: writing 0x%08x to gdrom register %s\n",
                (unsigned)val32, reg_info->reg_name);
        break;
    default:
        fprintf(stderr, "WARNING: writing to gdrom register %s\n",
                reg_info->reg_name);
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
    // immediately acknowledge receipt of everything for now
    // uint32_t val =  0 /*int(!pending_gdrom_irq)*//*1*/ << 7; // BSY
    // val |= 1 << 6; // DRDY
    // val |= 1 << 3; // DRQ

    printf("WARNING: read 0x%02x from alternate GDROM status register\n",
           (unsigned)stat_reg);

    memcpy(buf, &stat_reg, len > 4 ? 4 : len);

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_status_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                          void *buf, addr32_t addr, unsigned len) {
    // immediately acknowledge receipt of everything for now
    // uint32_t val =  0 /*int(!pending_gdrom_irq)*//*1*/ << 7; // BSY
    // val |= 1 << 6; // DRDY
    // val |= 1 << 3; // DRQ

    holly_clear_ext_int(HOLLY_EXT_INT_GDROM);

    printf("WARNING: read 0x%02x from GDROM status register\n",
           (unsigned)stat_reg);

    memcpy(buf, &stat_reg, len > 4 ? 4 : len);

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_error_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                             void *buf, addr32_t addr, unsigned len) {
    uint32_t val = 0;

    memcpy(buf, &val, len > 4 ? 4 : len);

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_cmd_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    reg32_t cmd = 0;
    size_t n_bytes = len < sizeof(cmd) ? len : sizeof(cmd);

    memcpy(&cmd, buf, n_bytes);

    printf("WARNING: write 0x%x to gd-rom command register (%u bytes)\n",
           (unsigned)cmd, (unsigned)n_bytes);

    switch (cmd) {
    case GDROM_CMD_PKT:
        // TODO: implement packets instead of pretending to receive them
        gdrom_cmd_begin_packet();
        return MEM_ACCESS_SUCCESS;
        break;
    case GDROM_CMD_SET_FEAT:
        gdrom_cmd_set_features();
        break;
    case GDROM_CMD_IDENTIFY:
        gdrom_cmd_identify();
        return MEM_ACCESS_SUCCESS;
        break;
    default:
        fprintf(stderr, "WARNING: unknown command 0x%2x input to gdrom "
                "command register\n", cmd);
    }

    int_reason_reg |= INT_REASON_COD_MASK; // is this correct ?

    if (!(dev_ctrl_reg & DEV_CTRL_NIEN_MASK))
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_data_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len) {
    uint8_t *ptr = buf;

    printf("WARNING: reading %u values from GD-ROM data register:\n", len);

    while (len--) {
        unsigned dat;
        if (bufq_consume_byte(&dat) == 0)
            *ptr++ = dat;
        else
            *ptr++ = 0;
    }

    if (fifo_empty(&bufq)) {
        // done transmitting data from gdrom to host - notify host
        stat_reg &= ~(STAT_DRQ_MASK | STAT_BSY_MASK);
        stat_reg |= STAT_DRDY_MASK;
        if (!(dev_ctrl_reg & DEV_CTRL_NIEN_MASK))
            holly_raise_ext_int(HOLLY_EXT_INT_GDROM);
    }

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_data_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len) {
    uint32_t dat = 0;
    size_t n_bytes = len < sizeof(dat) ? len : sizeof(dat);

    memcpy(&dat, buf, n_bytes);

    printf("WARNING: write 0x%04x to gdrom data register (%u bytes)\n",
           (unsigned)dat, (unsigned)len);

    if (state == GDROM_STATE_INPUT_PKT) {
        pkt_buf[n_bytes_received] = dat & 0xff;
        pkt_buf[n_bytes_received + 1] = (dat >> 8) & 0xff;
        n_bytes_received += 2;

        if (n_bytes_received >= 12) {
            n_bytes_received = 0;
            gdrom_input_packet();
        }
    }

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_features_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len) {
    size_t n_bytes = len < sizeof(feat_reg) ? len : sizeof(feat_reg);

    memcpy(&feat_reg, buf, n_bytes);

    fprintf(stderr, "WARNING: writing 0x%08x to the GD-ROM features register\n",
            feat_reg);

    return MEM_ACCESS_SUCCESS;
}

static void gdrom_cmd_set_features(void) {
    bool set;

    if ((feat_reg & 0x7f) == 3) {
        set = (bool)(feat_reg >> 7);
    } else {
        fprintf(stderr, "WARNING: software executed GDROM Set Features "
                "command without writing 3 to the features register\n");
        return;
    }

    if ((sect_cnt_reg & TRANS_MODE_PIO_DFLT_MASK) ==
        TRANS_MODE_PIO_DFLT_VAL) {
        trans_mode_vals[TRANS_MODE_PIO_DFLT] = sect_cnt_reg;
        fprintf(stderr, "WARNING: default PIO transfer mode set to 0x%02x\n",
                (unsigned)trans_mode_vals[TRANS_MODE_PIO_DFLT]);
    } else if ((sect_cnt_reg & TRANS_MODE_PIO_FLOW_CTRL_MASK) ==
               TRANS_MODE_PIO_FLOW_CTRL_VAL) {
        trans_mode_vals[TRANS_MODE_PIO_FLOW_CTRL] =
            sect_cnt_reg & ~TRANS_MODE_PIO_FLOW_CTRL_MASK;
        fprintf(stderr, "WARNING: flow-control PIO transfer mode set to "
                "0x%02x\n",
                (unsigned)trans_mode_vals[TRANS_MODE_PIO_FLOW_CTRL]);
    } else if ((sect_cnt_reg & TRANS_MODE_SINGLE_WORD_DMA_MASK) ==
               TRANS_MODE_SINGLE_WORD_DMA_VAL) {
        trans_mode_vals[TRANS_MODE_SINGLE_WORD_DMA] =
            sect_cnt_reg & ~TRANS_MODE_SINGLE_WORD_DMA_MASK;
        fprintf(stderr, "WARNING: single-word DMA transfer mode set to "
                "0x%02x\n",
                (unsigned)trans_mode_vals[TRANS_MODE_SINGLE_WORD_DMA]);
    } else if ((sect_cnt_reg & TRANS_MODE_MULTI_WORD_DMA_MASK) ==
               TRANS_MODE_MULTI_WORD_DMA_MASK) {
        trans_mode_vals[TRANS_MODE_MULTI_WORD_DMA] =
            sect_cnt_reg & ~TRANS_MODE_MULTI_WORD_DMA_MASK;
        fprintf(stderr, "WARNING: multi-word DMA transfer mode set to 0x%02x\n",
                (unsigned)trans_mode_vals[TRANS_MODE_MULTI_WORD_DMA]);
    } else if ((sect_cnt_reg & TRANS_MODE_PSEUDO_DMA_MASK) ==
               TRANS_MODE_PSEUDO_DMA_VAL) {
        trans_mode_vals[TRANS_MODE_PSEUDO_DMA] =
            sect_cnt_reg & ~TRANS_MODE_PSEUDO_DMA_MASK;
        fprintf(stderr, "WARNING: pseudo-DMA transfer mode set to 0x%02x\n",
                (unsigned)trans_mode_vals[TRANS_MODE_PSEUDO_DMA]);
    } else {
        fprintf(stderr, "WARNING: unrecognized GD-ROM transfer mode\n"
                "sect_cnt_reg is 0x%08x\n", sect_cnt_reg);
    }
}

// string of bytes returned by the GDROM_CMD_IDENTIFY command
static uint8_t gdrom_ident_str[80] = {
    0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x53, 0x45, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x43, 0x44, 0x2d, 0x52, 0x4f, 0x4d, 0x20, 0x44,
    0x52, 0x49, 0x56, 0x45, 0x20, 0x20, 0x20, 0x20, 0x36, 0x2e,
    0x34, 0x32, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static void gdrom_cmd_identify(void) {
    state = GDROM_STATE_NORM;

    stat_reg &= ~STAT_BSY_MASK;
    stat_reg |= STAT_DRQ_MASK;

    if (!(dev_ctrl_reg & DEV_CTRL_NIEN_MASK))
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);

    bufq_clear();

    struct gdrom_bufq_node *node =
        (struct gdrom_bufq_node*)malloc(sizeof(struct gdrom_bufq_node));

    if (!node)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    node->idx = 0;
    node->len = 80;
    memcpy(node->dat, gdrom_ident_str, sizeof(gdrom_ident_str));

    data_byte_count = 80;

    fifo_push(&bufq, &node->fifo_node);
}

static void gdrom_cmd_begin_packet(void) {
    int_reason_reg &= ~INT_REASON_IO_MASK;
    int_reason_reg |= INT_REASON_COD_MASK;
    stat_reg |= STAT_DRQ_MASK;
    n_bytes_received = 0;
    state = GDROM_STATE_INPUT_PKT;
}

/*
 * this function is called after 12 bytes have been written to the data
 * register after the drive has received GDROM_CMD_PKT (which puts it in
 * GDROM_STATE_INPUT_PKT
 */
static void gdrom_input_packet(void) {
    stat_reg &= ~(STAT_DRQ_MASK | STAT_BSY_MASK);

    if (!(dev_ctrl_reg & DEV_CTRL_NIEN_MASK))
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);


    switch (pkt_buf[0]) {
    case GDROM_PKT_TEST_UNIT:
        gdrom_input_test_unit_packet();
        break;
    case GDROM_PKT_REQ_STAT:
        printf("REQ_STAT command received!\n");
        state = GDROM_STATE_NORM; // TODO: implement
        break;
    case GDROM_PKT_REQ_MODE:
        gdrom_input_req_mode_packet();
        break;
    case GDROM_PKT_START_DISK:
        gdrom_input_start_disk_packet();
        break;
    case GDROM_PKT_READ_TOC:
        gdrom_input_read_toc_packet();
        break;
    case GDROM_PKT_READ:
        gdrom_input_read_packet();
        break;
    case GDROM_PKT_UNKNOWN_71:
        gdrom_input_packet_71();
        break;
    default:
        printf("unknown packet 0x%02x received\n", (unsigned)pkt_buf[0]);
        state = GDROM_STATE_NORM;
    }
}

static void gdrom_input_test_unit_packet(void) {
    printf("TEST_UNIT packet received\n");

    // is this correct?
    int_reason_reg |= (INT_REASON_COD_MASK | INT_REASON_IO_MASK);
    stat_reg |= STAT_DRDY_MASK;
    stat_reg &= ~(STAT_BSY_MASK | STAT_DRQ_MASK);

    // raise interrupt if it is enabled - this is already done from
    // gdrom_input_packet
    /* if (!(dev_ctrl_reg & DEV_CTRL_NIEN_MASK)) */
    /*     holly_raise_ext_int(HOLLY_EXT_INT_GDROM); */

    state = GDROM_STATE_NORM;
}

/*
 * Exactly what this command does is a mystery to me.  It doesn't appear to
 * convey any data because the bios does not check for any.  What little
 * information I can find would seem to convey that this is some sort of a
 * disk initialization function?
 */
static void gdrom_input_start_disk_packet(void) {
    printf("START_DISK(=0x70) packet received\n");

    // is this correct?
    int_reason_reg |= (INT_REASON_COD_MASK | INT_REASON_IO_MASK);
    stat_reg |= STAT_DRDY_MASK;
    stat_reg &= ~(STAT_BSY_MASK | STAT_DRQ_MASK);

    // raise interrupt if it is enabled - this is already done from
    // gdrom_input_packet
    /* if (!(dev_ctrl_reg & DEV_CTRL_NIEN_MASK)) */
    /*     holly_raise_ext_int(HOLLY_EXT_INT_GDROM); */

    state = GDROM_STATE_NORM;
}

/*
 * Now this here is the stuff nightmares are made of.
 *
 * Packet 0x71 is not available in any of the documentation I have on hand,
 * and its purpose is not apparent.  What it does is return a huge chunk of
 * data.  The data returned is never the same (even on the same Dreamcast
 * with the same disc inserted!), and it's not even the same length.
 *
 * Whatever this is, it definitely does SOMETHING important because without
 * sending data back from this command, the GDROM_REINIT system call will fail
 * and return -1 even after it has successfully read the table-of-contents.
 *
 * I looked through some of the other open-source dreamcast emulators on the
 * net, it doesn't look like any of them have a good idea of what this is
 * either.
 *
 * The below implementation returns a string that I captured on a live Dreamcast.
 * Even though it's always the same string, this seems to work well enough.
 * Someday I'll figure this out for real.
 *
 * (in case you're wondering, the below comments about screens are referncing
 * the program I used to get these; I don't have a serial cable handy so what
 * did was print this string to the framebuffer in three groups, and then rip
 * it over to here by hand.  I beleive everything is in order, but I still
 * intend to go back and double check to make sure I didn't mistype anything).
 */
static void gdrom_input_packet_71(void) {
    printf("GDROM_PKT_UNKNOWN_71 packet received; sending recorded response\n");
#define PKT_71_RESP_LEN 960
    static uint8_t pkt71_resp[PKT_71_RESP_LEN] = {
        0x42, 0x07, 0xf0, 0x47, 0x07, 0x1b, 0x10, 0xed, 0x05, 0xbc,
        0xc0, 0x00, 0x06, 0x08, 0x94, 0x78, 0x6a, 0x0a, 0x4b, 0x00,
        0x46, 0x0e, 0xdd, 0xc4, 0xa8, 0x38, 0x82, 0xb1, 0xfc, 0xd5,
        0x5e, 0x2a, 0xb7, 0x00, 0x9a, 0x0b, 0xec, 0x1d, 0xbe, 0x85,
        0x5c, 0x96, 0xf4, 0x82, 0x00, 0x69, 0x0a, 0x9e, 0x34, 0xe7,
        0x3d, 0x9f, 0x22, 0x83, 0x00, 0x09, 0x0f, 0x3f, 0x7c, 0x26,
        0x83, 0x28, 0xbf, 0x6d, 0xb3, 0x23, 0x0f, 0x07, 0x42, 0x00,
        0xe1, 0x07, 0xd8, 0xfe, 0x23, 0xfc, 0x00, 0x00, 0x04, 0x67,
        0x00, 0x30, 0x03, 0x00, 0xca, 0x08, 0x55, 0x75, 0xe0, 0xbf,
        0x92, 0x00, 0xed, 0x05, 0x36, 0x7b, 0x00, 0x9a, 0x0c, 0x08,
        0xb1, 0x79, 0x88, 0x13, 0xce, 0xec, 0x96, 0x00, 0x00, 0xcd,
        0x04, 0x68, 0x00, 0xa6, 0x03, 0x00, 0x89, 0x0e, 0x22, 0xd2,
        0x32, 0x4c, 0x8f, 0x97, 0xd2, 0x7d, 0xc0, 0x41, 0x7e, 0x00,
        0x51, 0x0e, 0x63, 0xc0, 0xd0, 0x82, 0x12, 0xe6, 0x93, 0x90,
        0xc2, 0xa0, 0xa3, 0x00, 0x15, 0x06, 0xc3, 0x97, 0x5c, 0x00,
        0xf1, 0x03, 0x00, 0x0b, 0x03, 0x00, 0x55, 0x04, 0xc3, 0x00,
        0x88, 0x0b, 0x9e, 0x41, 0x2b, 0x4b, 0x2d, 0x5a, 0xce, 0x2b,
        0x00, 0x67, 0x0c, 0xb8, 0x0d, 0xc4, 0x53, 0x63, 0xdc, 0x65,
        0xa2, 0xa6, 0x00, 0x61, 0x0c, 0x96, 0x18, 0xd6, 0xeb, 0x66,
        0xa9, 0x05, 0x29, 0xf0, 0x00, 0xdd, 0x0b, 0x79, 0x9d, 0xd9,
        0x9e, 0xdb, 0x35, 0x03, 0x14, 0x00, 0xcb, 0x04, 0xe0, 0x00,
        0x51, 0x08, 0x5f, 0xfc, 0x14, 0x6a, 0x6c, 0x00, 0x60, 0x10,
        0xa6, 0xde, 0x2c, 0x8f, 0x38, 0x6d, 0x28, 0xd6, 0xc4, 0x8a,
        0xe8, 0xcb, 0xec, 0x00, 0x3c, 0x11, 0x01, 0xe6, 0x1d, 0x80,
        0xaa, 0x1b, 0x22, 0x2e, 0x0e, 0x0d, 0xf5, 0x29, 0x51, 0xe3,
        0x00, 0x60, 0x07, 0xb6, 0x4d, 0x4e, 0x3c, 0x00, 0x22, 0x05,
        0x5a, 0xb9, 0x00, 0x9b, 0x08, 0x9b, 0x70, 0xb6, 0xbd, 0x29,
        0x00, 0x1a, 0x06, 0xfe, 0x3a, 0xbe, 0x00, 0x0a, 0x09, 0xf6,
        0x38, 0xb1, 0xb5, 0x1d, 0xa1, 0x00, 0x45, 0x0e, 0xe5, 0xc0,
        0xd2, 0x62, 0x62, 0xe1, 0x98, 0x70, 0x01, 0x1b, 0x11, 0x00,
        0x4a, 0x05, 0xbe, 0xbd, 0x00, 0xcb, 0x0c, 0xf5, 0xa0, 0xa4,
        0x4c, 0x2e, 0xea, 0x17, 0x9e, 0x37, 0x00, 0x6c, 0x0e, 0xb0,
        0xbf, 0xca, 0x1f, 0xe9, 0x22, 0xc5, 0x49, 0xb7, 0x7e, 0x6f,
        0x00, 0x4f, 0x10, 0xec, 0xea, 0x92, 0xc5, 0x5c, 0x72, 0x6a,
        0x6f, 0x5d, 0x95, 0xac, 0x8f, 0xcb, 0x00, 0x81, 0x0f, 0x12,
        0xba, 0x67, 0x89, 0x0f, 0xb2, 0x97, 0xbf, 0x93, 0x59, 0xc4,
        0xba, 0x00, 0x7b, 0x09, 0x4c, 0xc3, 0xdd, 0xd8, 0xd8, 0x5e,
        0x00, 0xd3, 0x11, 0x06, 0x2d, 0xff, 0x81, 0x8f, 0x49, 0x3b,
        0xe0, 0x93, 0x41, 0xec, 0x01, 0x5d, 0x7f, 0x00, 0xff, 0x0c,
        0x6e, 0xde, 0xf0, 0x89, 0x50, 0x92, 0x6b, 0xca, 0xd9, 0x00,
        0x8f, 0x0a, 0x10, 0xe3, 0xc0, 0x20, 0x69, 0xbe, 0x5b, 0xf9,
        0x5f, 0xb5, 0x81, 0x0f, 0x89, 0xc9, 0x96, 0xc8, 0x8b, 0x6e,
        0x6c, 0x41, 0xa0, 0xe7, 0x83, 0xc0, 0x6b, 0x9c, 0xdb, 0x43,
        0xbd, 0x34, 0x2a, 0x40, 0xf9, 0x6b, 0xf2, 0xc9, 0xa5, 0xdd,
        0xce, 0x58, 0xbf, 0xa7, 0xf9, 0xcb, 0xce, 0xf7, 0x6d, 0xc9,
        0xe5, 0xf6, 0x90, 0x87, 0x1c, 0x5b, 0xe0, 0xd0, 0xea, 0x55,
        0x2e, 0xa0, 0x19, 0x7e, 0x2b, 0x07, 0xf6, 0xda, 0x16, 0xbe,
        0xfb, 0x9f, 0x5b, 0xc2, 0x89, 0x76, 0xb1,

        // END OF FIRST SCREEN
        // BEGINNONG OF SECOND SCREEN
                                                  0x19, 0xcb, 0x28,
        0xe0, 0x9b, 0xb2, 0x86, 0x15, 0x1d, 0x1b, 0xac, 0x15, 0xf2,
        0x75, 0x18, 0xb3, 0xe7, 0xe2, 0x2f, 0xfd, 0xfd, 0x1e, 0x83,
        0x1a, 0xb5, 0x2c, 0x45, 0xe9, 0x4a, 0x44, 0x0a, 0x72, 0xff,
        0x40, 0x34, 0xbb, 0xf8, 0xe0, 0x16, 0xed, 0x32, 0x9c, 0xfc,
        0x0a, 0xf9, 0xbc, 0x93, 0xa4, 0x46, 0x73, 0x16, 0xd9, 0x3f,
        0x32, 0xa7, 0xa7, 0xcc, 0xe8, 0xac, 0xe0, 0xba, 0xa6, 0xff,
        0x30, 0xe9, 0x10, 0x19, 0xe8, 0x7c, 0x5c, 0x20, 0x43, 0x9c,
        0xbc, 0xe7, 0x24, 0xdf, 0x51, 0x73, 0x0e, 0x60, 0x5d, 0x3d,
        0x6a, 0x8a, 0x64, 0x73, 0xcf, 0x57, 0x24, 0xa2, 0x10, 0x64,
        0xba, 0xcd, 0xef, 0x00, 0x40, 0x23, 0xd5, 0xdd, 0x42, 0x31,
        0x38, 0x13, 0xb9, 0x9c, 0xd5, 0xa8, 0x43, 0x34, 0xfd, 0x91,
        0x88, 0x3e, 0xaa, 0x42, 0x45, 0x63, 0xfa, 0x10, 0xd9, 0xc5,
        0xa7, 0x7f, 0xde, 0x12, 0x64, 0xa9, 0x2e, 0x88, 0x0a, 0xa9,
        0x30, 0xda, 0x63, 0xcc, 0xb5, 0x2c, 0xe0, 0x22, 0x2e, 0x6c,
        0x5d, 0x14, 0xc9, 0x4b, 0xbd, 0x3b, 0xef, 0xdd, 0x9c, 0x97,
        0xf5, 0x4d, 0xd3, 0x99, 0x01, 0x5e, 0xb0, 0xed, 0xe7, 0xa9,
        0xc8, 0x21, 0x06, 0x91, 0x34, 0x74, 0x75, 0x5f, 0x85, 0x0a,
        0x74, 0x1d, 0x74, 0x42, 0xd0, 0x5a, 0xac, 0x45, 0x11, 0x06,
        0xf9, 0x41, 0xa1, 0x61, 0xe1, 0x0f, 0xd4, 0xd2, 0x98, 0x41,
        0xb7, 0x81, 0x9d, 0x24, 0xf8, 0x40, 0xa0, 0x87, 0x7b, 0x63,
        0x8f, 0xaa, 0xcc, 0x96, 0x9e, 0xb4, 0xac, 0x72, 0xb8, 0xf2,
        0x8a, 0xde, 0x55, 0xa6, 0xf2, 0x47, 0x84, 0xe2, 0x99, 0xf4,
        0xd4, 0xee, 0xe4, 0x22, 0x7b, 0xca, 0x51, 0xde, 0xa4, 0x1c,
        0xae, 0x3a, 0x8d, 0x1f, 0x7c, 0x1b, 0xec, 0xfb, 0x6f, 0x9a,
        0x8b, 0xf2, 0x39, 0xa4, 0x6d, 0xc9, 0x9d, 0x7b, 0xa9, 0x2e,
        0xef, 0xc2, 0xe3, 0x5f, 0xb5, 0x55, 0x1d, 0xad, 0x1a, 0x83,
        0xc8, 0xb1, 0x03, 0x1e, 0x67, 0x98, 0xa5, 0xb1, 0x39, 0x29,
        0xaa, 0x09, 0x39, 0x6b, 0x05, 0x12, 0x0b, 0x85, 0x6d, 0x26,
        0xad, 0xa4, 0xc4, 0x21, 0x54, 0xfd, 0x37, 0x6f, 0xb8, 0xe4,
        0x76, 0x3b, 0xc2, 0x46, 0xf0, 0x82, 0xb9, 0xce, 0x9f, 0x5b,
        0xbe, 0x18, 0x0e, 0x83, 0x81, 0x4b, 0xfe, 0xc2, 0x0b, 0xb8,
        0x1e, 0xae, 0xf5, 0x36, 0x0d, 0x8d, 0xef, 0xbd, 0x2e, 0xe5,
        0xdf, 0xd7, 0x12, 0xde, 0x6b, 0x92, 0xfb, 0xb6, 0xea, 0xba,
        0x14, 0x49, 0x7a, 0xb5, 0x08, 0xff, 0xaf, 0x8e, 0x14, 0xc7,
        0x0e, 0x7a, 0x41, 0xd3, 0x05, 0x9f, 0x29, 0xe4, 0xdb, 0x5e,
        0x87, 0x03, 0xe4, 0xe5, 0x47, 0x75, 0x9e, 0x59, 0x7b, 0x82,
        0x9c, 0x70, 0xae, 0x44, 0xaa, 0xa4, 0x6d, 0x22, 0x31, 0x6e,
        0x64, 0x0b, 0xe2, 0xd0, 0x5e, 0xdc, 0xf3, 0x2d, 0x97, 0xf6,
        0xf4, 0xa2, 0x3b, 0x68, 0x1e, 0xa4, 0x9b, 0x36, 0x0b, 0x64,
        0x92, 0x8d, 0x5d, 0xa6, 0x63, 0x03, 0x98, 0x71, 0x75, 0x29,
        0xac, 0xfc, 0x4d, 0xe5, 0x6a, 0xe5, 0xc8, 0x7c, 0xc7, 0xa3,
        0xda, 0x68, 0x4e, 0x17, 0xf0, 0x0f, 0x13, 0x08, 0xe1, 0xa7,
        0x0c, 0xfc, 0xad, 0x08, 0xc2, 0x68, 0xbe, 0xfd, 0x66, 0x79,
        0x14, 0x77, 0x66, 0xf4, 0x1c, 0x8c, 0x54, 0x24, 0x78, 0x0f,
        0x91, 0x93, 0x55, 0x64, 0x58, 0x96, 0xe0, 0x55, 0xe4, 0xfb,
        0x45, 0x9b, 0x0b, 0xfc, 0x15, 0x35, 0x96, 0x59, 0xc9, 0xde,
        0x91, 0xa4, 0x04, 0xb8, 0x12, 0x47, 0x13, 0xea, 0xbf, 0x5a,
        0x2f, 0xbe, 0x24, 0xe6,

        // END SECOND SCREEN
        // BEGIN FINAL SCREEN

                                0x62, 0x71, 0xf1, 0xb1, 0xf3, 0x8d
    };

    bufq_clear();

    struct gdrom_bufq_node *node =
        (struct gdrom_bufq_node*)malloc(sizeof(struct gdrom_bufq_node));
    node->idx = 0;
    node->len = PKT_71_RESP_LEN;

    /*
     * XXX this works because PKT_71_RESP_LEN is less than GDROM_BUFQ_LEN.
     * if that ever changes, so must this code
     */
    memcpy(node->dat, pkt71_resp, PKT_71_RESP_LEN);

    data_byte_count = PKT_71_RESP_LEN;

    fifo_push(&bufq, &node->fifo_node);

    int_reason_reg |= INT_REASON_IO_MASK;
    int_reason_reg &= ~INT_REASON_COD_MASK;
    stat_reg |= STAT_DRQ_MASK;
    if (!(dev_ctrl_reg & DEV_CTRL_NIEN_MASK))
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);

    state = GDROM_STATE_NORM;
}

static void gdrom_input_req_mode_packet(void) {
    unsigned starting_addr = pkt_buf[2];
    unsigned len = pkt_buf[4];

    printf("REQ_MODE command received\n");
    printf("read %u bytes starting at %u\n", len, starting_addr);

    /*
     * response to command packet 0x11 (REQ_MODE).  A couple of these fields
     * are supposed to be user-editable via the 0x12 (SET_MODE) packet.  Mostly
     * it's just irrelevant text used to get the drive's firmware version.  For
     * now none of these fields can be changed because I haven't implemented
     * that yet.
     */
    static uint8_t info[32] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0xb4, 0x19, 0x00,
        0x00, 0x08,  'S',  'E',  ' ',  ' ',  ' ',  ' ',
         ' ',  ' ',  'R',  'e',  'v',  ' ',  '6',  '.',
         '4',  '2',  '9',  '9',  '0',  '3',  '1',  '6'
    };

    bufq_clear();
    if (len != 0) {
        unsigned first_idx = starting_addr;
        unsigned last_idx = starting_addr + (len - 1);

        if (first_idx > 31)
            first_idx = 31;
        if (last_idx > 31)
            last_idx = 31;

        struct gdrom_bufq_node *node =
            (struct gdrom_bufq_node*)malloc(sizeof(struct gdrom_bufq_node));

        node->idx = 0;
        node->len = last_idx - first_idx + 1;
        memcpy(&node->dat, info + first_idx, node->len * sizeof(uint8_t));

        bufq_clear();
        fifo_push(&bufq, &node->fifo_node);
    }

    int_reason_reg |= INT_REASON_IO_MASK;
    int_reason_reg &= ~INT_REASON_COD_MASK;
    stat_reg |= STAT_DRQ_MASK;
    if (!(dev_ctrl_reg & DEV_CTRL_NIEN_MASK))
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);

    state = GDROM_STATE_NORM;
}

static void gdrom_input_read_toc_packet(void) {
    unsigned session = pkt_buf[1] & 1;
    unsigned len = (((unsigned)pkt_buf[3]) << 8) | pkt_buf[4];

    printf("GD-ROM: GET_TOC command received\n");
    printf("request to read %u bytes from the Table of Contents for "
           "Session %u\n", len, session);

    struct mount_toc toc;
    memset(&toc, 0, sizeof(toc));

    // TODO: call mount_check and signal an error if nothing is mounted
    mount_read_toc(&toc, session);

    bufq_clear();
    struct gdrom_bufq_node *node =
        (struct gdrom_bufq_node*)malloc(sizeof(struct gdrom_bufq_node));

    uint8_t const *ptr = mount_encode_toc(&toc);

    node->idx = 0;
    node->len = len;
    memcpy(node->dat, ptr, len);
    data_byte_count = len;

    fifo_push(&bufq, &node->fifo_node);

    int_reason_reg |= INT_REASON_IO_MASK;
    int_reason_reg &= ~INT_REASON_COD_MASK;
    stat_reg |= STAT_DRQ_MASK;
    if (!(dev_ctrl_reg & DEV_CTRL_NIEN_MASK))
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);

    state = GDROM_STATE_NORM;
}

static void gdrom_input_read_packet(void) {
    // TODO implement
    printf("GD-ROM - READ_PACKET command received\n");

    unsigned start_addr = (pkt_buf[2] << 16) | (pkt_buf[3] << 8) | pkt_buf[4];
    unsigned trans_len = (pkt_buf[8] << 16) | (pkt_buf[9] << 8) | pkt_buf[10];
    unsigned data_sel = pkt_buf[1] >> 4;
    unsigned data_tp_expect = (pkt_buf[1] >> 1) & 0x7;
    unsigned param_tp = pkt_buf[1] & 1;

    if (data_sel != 0x2) {
        error_set_feature("CD-ROM header/subheader access");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    // TODO - check the expected data type (data_tp_expect)

    if (param_tp != 0) {
        /*
         * i think this is a timecode format that maps linearly to FAD/LBA, but
         * for now I'm just not sure.
         */
        error_set_feature("MSF format CD-ROM access");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    printf("request to read %u sectors from FAD %u\n", trans_len, start_addr);

    /* if (trans_len > 20) // TODO: fix this */
    /*     RAISE_ERROR(ERROR_UNIMPLEMENTED); */

    bufq_clear();

    while (trans_len--) {
        struct gdrom_bufq_node *node =
            (struct gdrom_bufq_node*)malloc(sizeof(struct gdrom_bufq_node));

        if (!node)
            RAISE_ERROR(ERROR_FAILED_ALLOC);

        if (mount_read_sectors(node->dat, start_addr, 1) < 0) {
            // obviously this needs to get done eventually
            error_set_feature("Correct errror message for failed GD-ROM read");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        node->idx = 0;
        node->len = CDROM_FRAME_DATA_SIZE;

        fifo_push(&bufq, &node->fifo_node);
    }

    data_byte_count = 2048 * trans_len;

    int_reason_reg |= INT_REASON_IO_MASK;
    int_reason_reg &= ~INT_REASON_COD_MASK;
    stat_reg |= STAT_DRQ_MASK;
    if (!(dev_ctrl_reg & DEV_CTRL_NIEN_MASK))
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);

    state = GDROM_STATE_NORM;
}

static int
gdrom_sect_cnt_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len) {
    size_t n_bytes = len < sizeof(sect_cnt_reg) ? len : sizeof(sect_cnt_reg);

    memcpy(&sect_cnt_reg, buf, n_bytes);

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_dev_ctrl_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len) {
    size_t n_bytes = len < sizeof(dev_ctrl_reg) ? len : sizeof(dev_ctrl_reg);

    memcpy(&dev_ctrl_reg, buf, n_bytes);

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_int_reason_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len) {
    size_t n_bytes = len < sizeof(int_reason_reg) ? len : sizeof(int_reason_reg);

    printf("GD-ROM: int_reason is 0x%08x\n", int_reason_reg);

    memcpy(buf, &int_reason_reg, n_bytes);

    return MEM_ACCESS_SUCCESS;
}

static int
gdrom_sector_num_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len) {
    // for now, hard code this register so that there's never a disc inserted

    uint32_t status = mount_check() ? GDROM_STATE_PAUSE : GDROM_STATE_NODISC;
    status <<= SEC_NUM_STATUS_SHIFT;

    memcpy(buf, &status, len < sizeof(status) ? len : sizeof(status));

    return 0;
}

static int
gdrom_byte_count_low_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                      void *buf, addr32_t addr, unsigned len) {
    uint32_t low = data_byte_count & 0xff;
    memcpy(buf, &low, len < sizeof(low) ? len : sizeof(low));

    return 0;
}

static int
gdrom_byte_count_low_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                       void const *buf, addr32_t addr, unsigned len) {
    uint32_t tmp = 0;
    memcpy(&tmp, buf, len < sizeof(tmp) ? len : sizeof(tmp));

    data_byte_count = (data_byte_count & ~0xff) | (tmp & 0xff);
    return 0;
}

static int
gdrom_byte_count_high_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                       void *buf, addr32_t addr, unsigned len) {
    uint32_t high = (data_byte_count & 0xff00) >> 8;
    memcpy(buf, &high, len < sizeof(high) ? len : sizeof(high));

    return 0;
}

static int
gdrom_byte_count_high_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                        void const *buf, addr32_t addr, unsigned len) {
    uint32_t tmp = 0;
    memcpy(&tmp, buf, len < sizeof(tmp) ? len : sizeof(tmp));

    data_byte_count = (data_byte_count & ~0xff00) | ((tmp & 0xff) << 8);
    return 0;
}

static void bufq_clear(void) {
    while (!fifo_empty(&bufq))
        free(&FIFO_DEREF(fifo_pop(&bufq), struct gdrom_bufq_node, fifo_node));
}

static int bufq_consume_byte(unsigned *byte) {
    struct fifo_node *node = fifo_peek(&bufq);

    if (node) {
        struct gdrom_bufq_node *bufq_node =
            &FIFO_DEREF(node, struct gdrom_bufq_node, fifo_node);

        *byte = (unsigned)bufq_node->dat[bufq_node->idx++];

        if (bufq_node->idx >= bufq_node->len) {
            fifo_pop(&bufq);
            free(bufq_node);
        }

        return 0;
    }

    return -1;
}
