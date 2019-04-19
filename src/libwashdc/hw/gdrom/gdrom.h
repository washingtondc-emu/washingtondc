/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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

#ifndef GDROM_H_
#define GDROM_H_

#include <stdint.h>
#include <stdbool.h>

#include "washdc/fifo.h"
#include "log.h"
#include "dc_sched.h"

#define GDROM_TRACE(msg, ...)                                           \
    do {                                                                \
        LOG_DBG("GD-ROM (PC=%08x): ",                                   \
               (unsigned)dreamcast_get_cpu()->reg[SH4_REG_PC]);         \
        LOG_DBG(msg, ##__VA_ARGS__);                                    \
    } while (0)

struct gdrom_status {
    // get off the phone!
    bool bsy;

    // response to an ata command is possible
    bool drdy;

    // drive fault
    bool df;

    // seek processing complete
    bool dsc;

    // data transfer possible
    bool drq;

    //correctable error flag
    bool corr;

    // error flag
    bool check;
};

struct gdrom_error {
    uint32_t ili : 1;
    uint32_t eomf : 1;
    uint32_t abrt : 1;
    uint32_t mcr : 1;
    uint32_t sense_key : 4;
};

struct gdrom_features {
    bool dma_enable;
    bool set_feat_enable;// this is true if the lower 7 bits are 3
};

enum gdrom_trans_mode{
    TRANS_MODE_PIO_DFLT,
    TRANS_MODE_PIO_FLOW_CTRL,
    TRANS_MODE_SINGLE_WORD_DMA,
    TRANS_MODE_MULTI_WORD_DMA,
    TRANS_MODE_PSEUDO_DMA,

    TRANS_MODE_COUNT
};

struct gdrom_sector_count {
    enum gdrom_trans_mode trans_mode;
    unsigned mode_val;
};

struct gdrom_int_reason {
    bool cod;
    bool io;
};

struct gdrom_dev_ctrl {
    bool nien;
    bool srst;
};

enum gdrom_state {
    GDROM_STATE_NORM,
    GDROM_STATE_INPUT_PKT,
    GDROM_STATE_SET_MODE, // waiting for PIO input for the SET_MODE packet

    /*
     * currently executing a PIO command that needs to spend a little
     * time fetching data
     */
    GDROM_STATE_PIO_READ_DELAY,

    GDROM_STATE_PIO_READING,

    GDROM_STATE_DMA_READING
};

enum additional_sense {
    ADDITIONAL_SENSE_NO_ERROR = 0,
    ADDITIONAL_SENSE_NO_DISC = 0x3a
};

#define GDROM_MMIO_LEN (ADDR_GDROM_LAST - ADDR_GDROM_FIRST + 1)
#define GDROM_REG_COUNT (GDROM_MMIO_LEN / 4)

struct gdrom_read_meta {
    // number of bytes to transfer
    unsigned byte_count;

    unsigned bytes_read;
};

union state_meta {
    struct gdrom_read_meta read;
};

struct gdrom_ctxt {
    struct dc_clock *clk;

    uint32_t regs[GDROM_REG_COUNT];

    bool gdrom_int_scheduled;
    struct SchedEvent gdrom_int_raise_event;

    struct gdrom_status stat_reg;
    struct gdrom_error error_reg;
    struct gdrom_features feat_reg;       // features register
    struct gdrom_sector_count sect_cnt_reg;   // sector count register
    struct gdrom_int_reason int_reason_reg; // interrupt reason register
    struct gdrom_dev_ctrl dev_ctrl_reg;   // device control register
    unsigned data_byte_count;// byte-count low/high registers

    // GD-ROM DMA memory protecion
    uint32_t gdapro_reg;

    // ???
    uint32_t g1gdrc_reg;

    // GD-ROM DMA start address
    uint32_t dma_start_addr_reg;

    // GD-ROM DMA transfer length (in bytes)
    uint32_t dma_len_reg;

    // GD-ROM DMA transfer direction
    uint32_t dma_dir_reg;

    // GD-ROM DMA enable
    uint32_t dma_en_reg;

    // GD-ROM DMA start
    uint32_t dma_start_reg;

    // length of DMA result
    uint32_t gdlend_reg;

    uint32_t drive_sel_reg;

    enum additional_sense additional_sense;

    uint32_t trans_mode_vals[TRANS_MODE_COUNT];

    enum gdrom_state state;
    union state_meta meta;

    /*
     * number of bytes we're waiting for.  This only holds meaning when
     * state == GDROM_STATE_SET_MODE.
     */
    int set_mode_bytes_remaining;

#define PKT_LEN 12
    uint8_t pkt_buf[PKT_LEN];
    unsigned n_bytes_received;

    struct fifo_head bufq;
};

/*
 * reset the gdrom system to its default state.
 * This is effectively a hard-reset.
 */
void gdrom_init(struct gdrom_ctxt *gdrom, struct dc_clock *gdrom_clk);

void gdrom_cleanup(struct gdrom_ctxt *gdrom);

// ideally this will never be access from outside of the GD-ROM code.
/* extern struct gdrom_ctxt gdrom; */

void gdrom_cmd_set_features(struct gdrom_ctxt *gdrom);

// called when the packet command (0xa0) is written to the cmd register
void gdrom_cmd_begin_packet(struct gdrom_ctxt *gdrom);

void gdrom_cmd_identify(struct gdrom_ctxt *gdrom);

void gdrom_read_data(struct gdrom_ctxt *gdrom, uint8_t *buf, unsigned n_bytes);

void gdrom_write_data(struct gdrom_ctxt *gdrom, uint8_t const *buf, unsigned n_bytes);

enum gdrom_disc_type {
    DISC_TYPE_CDDA = 0,
    DISC_TYPE_CDROM = 1,
    DISC_TYPE_CDROM_XA = 2,
    DISC_TYPE_CDI = 3, // i think this refers to phillips CD-I, not .cdi images
    DISC_TYPE_GDROM = 8
};

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

/*
 * should return the type of disc in the drive (which will usually be
 * DISC_TYPE_GDROM)
 */
enum gdrom_disc_type gdrom_get_disc_type(void);

/*
 * return the state the physical drive is in (GDROM_STATE_NODISC,
 * GDROM_STATE_PAUSE, etc).
 */
enum gdrom_disc_state gdrom_get_drive_state(void);

struct memory_interface gdrom_reg_intf;

void gdrom_start_dma(struct gdrom_ctxt *gdrom);

void gdrom_input_cmd(struct gdrom_ctxt *ctxt, unsigned cmd);

unsigned gdrom_dma_prot_top(struct gdrom_ctxt *gdrom);
unsigned gdrom_dma_prot_bot(struct gdrom_ctxt *gdrom);

#endif
