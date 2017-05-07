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

#include "types.h"
#include "MemoryMap.h"
#include "hw/sys/holly_intc.h"

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
// GD-ROM drive state
//
////////////////////////////////////////////////////////////////////////////////

static uint32_t stat_reg;       // status register
static uint32_t feat_reg;       // features register
static uint32_t sect_cnt_reg;   // sector count register
static uint32_t int_reason_reg; // interrupt reason register
static uint32_t dev_ctrl_reg;   // device control register

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

//this is used to buffer PIO data going from the GD-ROM to the host
#define DATA_BUF_MAX 64
unsigned data_buf_len;
unsigned data_buf_idx;
unsigned data_buf[DATA_BUF_MAX];

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
      gdrom_data_reg_read_handler/* warn_gdrom_reg_read_handler */, gdrom_data_reg_write_handler },
    { "Error/features", 0x5f7084, 4,
      gdrom_error_reg_read_handler, gdrom_features_reg_write_handler },
    { "Interrupt reason/sector count", 0x5f7088, 4,
      gdrom_int_reason_reg_read_handler, warn_gdrom_reg_write_handler },
    { "Sector number", 0x5f708c, 4,
      warn_gdrom_reg_read_handler, warn_gdrom_reg_write_handler },
    { "Byte Count (low)", 0x5f7090, 4,
      warn_gdrom_reg_read_handler, warn_gdrom_reg_write_handler },
    { "Byte Count (high)", 0x5f7094, 4,
      warn_gdrom_reg_read_handler, warn_gdrom_reg_write_handler },
    { NULL }
};

static void gdrom_cmd_set_features(void);

// called when the packet command (0xa0) is written to the cmd register
static void gdrom_cmd_begin_packet(void);

// called when all 12 bytes of a packet have been written to data
static void gdrom_input_packet(void);

static void gdrom_input_req_mode_packet(void);

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
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
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
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
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
    return 0;
}

static int
default_gdrom_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_GDROM_FIRST) >> 2;
    memcpy(idx + gdrom_regs, buf, len);
    return 0;
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
    return 0;
}

static int
gdrom_alt_status_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len) {
    // immediately acknowledge receipt of everything for now
    // uint32_t val =  0 /*int(!pending_gdrom_irq)*//*1*/ << 7; // BSY
    // val |= 1 << 6; // DRDY
    // val |= 1 << 3; // DRQ

    printf("WARNING: read %u from alternate GDROM status register\n",
           (unsigned)stat_reg);

    memcpy(buf, &stat_reg, len > 4 ? 4 : len);

    return 0;
}

static int
gdrom_status_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                          void *buf, addr32_t addr, unsigned len) {
    // immediately acknowledge receipt of everything for now
    // uint32_t val =  0 /*int(!pending_gdrom_irq)*//*1*/ << 7; // BSY
    // val |= 1 << 6; // DRDY
    // val |= 1 << 3; // DRQ

    holly_clear_ext_int(HOLLY_EXT_INT_GDROM);

    printf("WARNING: read %u from GDROM status register\n",
           (unsigned)stat_reg);

    memcpy(buf, &stat_reg, len > 4 ? 4 : len);

    return 0;
}

static int
gdrom_error_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                             void *buf, addr32_t addr, unsigned len) {
    uint32_t val = 0;

    memcpy(buf, &val, len > 4 ? 4 : len);

    return 0;
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
        return 0;
        break;
    case GDROM_CMD_SET_FEAT:
        gdrom_cmd_set_features();
        break;
    default:
        fprintf(stderr, "WARNING: unknown command 0x%2x input to gdrom "
                "command register\n", cmd);
    }

    int_reason_reg |= INT_REASON_COD_MASK; // is this correct ?

    if (!(dev_ctrl_reg & DEV_CTRL_NIEN_MASK))
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);

    return 0;
}

static int
gdrom_data_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len) {
    uint8_t *ptr = buf;

    printf("WARNING: reading %u values from GD-ROM data register:\n", len);

    while (len--) {
        unsigned old_idx = data_buf_idx;
        *ptr++ = data_buf_idx < data_buf_len ? data_buf[data_buf_idx++] : 0;
        printf("\t[%u] = 0x%02x\n", old_idx, (unsigned)*(ptr - 1));
    }

    if (data_buf_idx >= data_buf_len) {
        // done transmitting data from gdrom to host - notify host
        stat_reg &= ~(STAT_DRQ_MASK | STAT_BSY_MASK);
        stat_reg |= STAT_DRDY_MASK;
    if (!(dev_ctrl_reg & DEV_CTRL_NIEN_MASK))
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);
    }

    return 0;
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

    return 0;
}

static int
gdrom_features_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len) {
    size_t n_bytes = len < sizeof(feat_reg) ? len : sizeof(feat_reg);

    memcpy(&feat_reg, buf, n_bytes);

    return 0;
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

    if (pkt_buf[0] == 0x10) {
        printf("REQ_STAT command received!\n");
        state = GDROM_STATE_NORM;
    } else if (pkt_buf[0] == 0x11) {
        gdrom_input_req_mode_packet();
    } else {
        printf("unknown packet 0x%02x received\n", (unsigned)pkt_buf[0]);
        state = GDROM_STATE_NORM;
    }
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

    if (len != 0) {
        unsigned first_idx = starting_addr;
        unsigned last_idx = starting_addr + (len - 1);

        if (first_idx > 31)
            first_idx = 31;
        if (last_idx > 31)
            last_idx = 31;

        unsigned idx;
        for (idx = first_idx; idx <= last_idx; idx++) {
            data_buf[idx - first_idx] = info[idx];
            printf("GD-ROM: data_buf[%u] = 0x%02x\n",
                   idx - first_idx, data_buf[idx - first_idx]);
        }

        data_buf_len = last_idx - first_idx + 1;
    } else {
        data_buf_len = 0;
    }
    data_buf_idx = 0;

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

    return 0;
}

static int
gdrom_dev_ctrl_reg_write_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                 void const *buf, addr32_t addr, unsigned len) {
    size_t n_bytes = len < sizeof(dev_ctrl_reg) ? len : sizeof(dev_ctrl_reg);

    memcpy(&dev_ctrl_reg, buf, n_bytes);

    return 0;
}

static int
gdrom_int_reason_reg_read_handler(struct gdrom_mem_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len) {
    size_t n_bytes = len < sizeof(int_reason_reg) ? len : sizeof(int_reason_reg);

    printf("GD-ROM: int_reason is 0x%08x\n", int_reason_reg);

    memcpy(buf, &int_reason_reg, n_bytes);

    return 0;
}
