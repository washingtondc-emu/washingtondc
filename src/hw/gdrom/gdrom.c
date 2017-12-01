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
#include <stdlib.h>

#include "cdrom.h"
#include "dreamcast.h"
#include "hw/sh4/sh4.h"
#include "mount.h"
#include "hw/sys/holly_intc.h"
#include "error.h"
#include "gdrom_response.h"

#include "gdrom.h"

DEF_ERROR_INT_ATTR(gdrom_command);

#define GDROM_GDAPRO_DEFAULT 0x00007f00
#define GDROM_G1GDRC_DEFAULT 0x0000ffff
#define GDROM_GDSTAR_DEFAULT 0x00000000 // undefined
#define GDROM_GDLEN_DEFAULT  0x00000000 // undefined
#define GDROM_GDDIR_DEFAULT  0x00000000
#define GDROM_GDEN_DEFAULT   0x00000000
#define GDROM_GDST_DEFAULT   0x00000000
#define GDROM_GDLEND_DEFAULT 0x00000000 // undefined
#define GDROM_DATA_BYTE_COUNT_DEFAULT 0xeb14

enum sense_key {
    // no sense key (command execution successful)
    SENSE_KEY_NONE = 0,

    // successful error recovery
    SENSE_KEY_RECOVERED = 1,

    // drive not ready
    SENSE_KEY_NOT_READY = 2,

    // defective disc
    SENSE_KEY_MEDIUM_ERROR = 3,

    // drive failure
    SENSE_KEY_HW_ERROR = 4,

    // invalid parameter/request
    SENSE_KEY_ILLEGAL_REQ = 5,

    // disc removed/drive reset
    SENSE_KEY_UNIT_ATTN = 6,

    // writing to a read-only area
    SENSE_KEY_DATA_PROT = 7,

    // command was aborted
    SENSE_KEY_CMD_ABORT = 11
};

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
#define GDROM_PKT_SET_MODE   0x12
#define GDROM_PKT_REQ_ERROR  0x13
#define GDROM_PKT_READ_TOC   0x14
#define GDROM_PKT_READ       0x30
#define GDROM_PKT_SUBCODE    0x40
#define GDROM_PKT_START_DISK 0x70
#define GDROM_PKT_UNKNOWN_71 0x71

// Empty out the bufq and free resources.
static void bufq_clear(void);

/*
 * grab one byte from the queue, pop/clear a node (if necessary) and return 0.
 * this returns non-zero if the queue is empty.
 */
static int bufq_consume_byte(unsigned *byte);

static void gdrom_clear_error(void);

static void gdrom_input_read_packet(void);

// called when all 12 bytes of a packet have been written to data
static void gdrom_input_packet(void);

static void gdrom_input_req_mode_packet(void);

static void gdrom_input_set_mode_packet(void);

static void gdrom_input_req_error_packet(void);

static void gdrom_input_test_unit_packet(void);

static void gdrom_input_start_disk_packet(void);

static void gdrom_input_read_toc_packet(void);

static void gdrom_input_read_packet(void);

static void gdrom_input_packet_71(void);

static void gdrom_input_read_subcode_packet(void);

struct gdrom_ctxt gdrom;

/*
 * do a DMA transfer from GD-ROM to host using whatever's in the buffer queue.
 *
 * This function gets all the relevant parameters from the registers,
 * performs the transfer and sets the final value of all relevant registers
 * except the ones that have flags or pertain to interrupts
 */
static void gdrom_complete_dma(void);

void gdrom_init(void) {
    memset(&gdrom, 0, sizeof(gdrom));

    gdrom.gdapro_reg = GDROM_GDAPRO_DEFAULT;
    gdrom.g1gdrc_reg = GDROM_G1GDRC_DEFAULT;
    gdrom.dma_start_addr_reg = GDROM_GDSTAR_DEFAULT;
    gdrom.dma_len_reg = GDROM_GDLEN_DEFAULT;
    gdrom.dma_dir_reg = GDROM_GDDIR_DEFAULT;
    gdrom.dma_en_reg = GDROM_GDEN_DEFAULT;
    gdrom.dma_start_reg = GDROM_GDST_DEFAULT;
    gdrom.gdlend_reg = GDROM_GDLEND_DEFAULT;

    gdrom.additional_sense = ADDITIONAL_SENSE_NO_ERROR;

    gdrom.error_reg.ili = true;
    gdrom.sect_cnt_reg.trans_mode = TRANS_MODE_PIO_DFLT;
    gdrom.sect_cnt_reg.mode_val = 1;
    gdrom.data_byte_count = GDROM_DATA_BYTE_COUNT_DEFAULT;

    fifo_init(&gdrom.bufq);
}

static void bufq_clear(void) {
    while (!fifo_empty(&gdrom.bufq)) {
        free(&FIFO_DEREF(fifo_pop(&gdrom.bufq),
                         struct gdrom_bufq_node, fifo_node));
    }
}

static int bufq_consume_byte(unsigned *byte) {
    struct fifo_node *node = fifo_peek(&gdrom.bufq);

    if (node) {
        struct gdrom_bufq_node *bufq_node =
            &FIFO_DEREF(node, struct gdrom_bufq_node, fifo_node);

        *byte = (unsigned)bufq_node->dat[bufq_node->idx++];

        if (bufq_node->idx >= bufq_node->len) {
            fifo_pop(&gdrom.bufq);
            free(bufq_node);
        }

        return 0;
    }

    return -1;
}

static void gdrom_clear_error(void) {
    memset(&gdrom.error_reg, 0, sizeof(gdrom.error_reg));
}

static void gdrom_complete_dma(void) {
    unsigned bytes_transmitted = 0;
    unsigned bytes_to_transmit = gdrom.dma_len_reg;
    unsigned addr = gdrom.dma_start_addr_reg;

    while (bytes_transmitted < bytes_to_transmit) {
        struct fifo_node *fifo_node = fifo_pop(&gdrom.bufq);

        if (!fifo_node)
            goto done;

        struct gdrom_bufq_node *bufq_node =
            &FIFO_DEREF(fifo_node, struct gdrom_bufq_node, fifo_node);

        unsigned chunk_sz = bufq_node->len;

        if ((chunk_sz + bytes_transmitted) > bytes_to_transmit)
            chunk_sz = bytes_to_transmit - bytes_transmitted;

        bytes_transmitted += chunk_sz;

        /*
         * enforce the gdapro register
         * bytes_transmitted will still count the full length of chunk_sz
         * because that seems like the logical behavior here.  I have not run
         * any hardware tests to confirm that this is correct.
         */
        if (addr < gdrom_dma_prot_top()) {
            // don't do this chunk if the end is below gdrom_dma_prot_top
            if ((chunk_sz + addr) < gdrom_dma_prot_top())
                goto chunk_finished;

            chunk_sz -= (gdrom_dma_prot_top() - addr);
            addr = gdrom_dma_prot_top();
        }

        if ((addr + chunk_sz - 1) > gdrom_dma_prot_bot()) {
            chunk_sz = gdrom_dma_prot_bot() - addr + 1;
        }

        sh4_dmac_transfer_to_mem(addr, chunk_sz, 1, bufq_node->dat);

    chunk_finished:
        addr += chunk_sz;
        free(bufq_node);
    }

done:
    // set GD_LEND, etc here
    gdrom.gdlend_reg = bytes_transmitted;
    gdrom.dma_start_reg = 0;
}

static void gdrom_input_read_packet(void) {
    GDROM_TRACE("READ_PACKET command received\n");

    unsigned start_addr = (gdrom.pkt_buf[2] << 16) | (gdrom.pkt_buf[3] << 8) | gdrom.pkt_buf[4];
    unsigned trans_len = (gdrom.pkt_buf[8] << 16) | (gdrom.pkt_buf[9] << 8) | gdrom.pkt_buf[10];
    unsigned data_sel = gdrom.pkt_buf[1] >> 4;
    unsigned param_tp = gdrom.pkt_buf[1] & 1;

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

    GDROM_TRACE("request to read %u sectors from FAD %u\n",
                trans_len, start_addr);

    if (gdrom.feat_reg.dma_enable) {
        GDROM_TRACE("DMA READ ACCESS\n");
        /* error_set_feature("GD-ROM DMA access"); */
        /* RAISE_ERROR(ERROR_UNIMPLEMENTED); */
    }

    bufq_clear();

    gdrom.data_byte_count = CDROM_FRAME_DATA_SIZE * trans_len;

    unsigned fad_offs = 0;
    while (trans_len--) {
        struct gdrom_bufq_node *node =
            (struct gdrom_bufq_node*)malloc(sizeof(struct gdrom_bufq_node));

        if (!node)
            RAISE_ERROR(ERROR_FAILED_ALLOC);

        if (mount_read_sectors(node->dat, start_addr + fad_offs++, 1) < 0) {
            free(node);

            gdrom.error_reg.sense_key = SENSE_KEY_ILLEGAL_REQ;
            gdrom.stat_reg.check = true;
            gdrom.state = GDROM_STATE_NORM;
            return;
        }

        node->idx = 0;
        node->len = CDROM_FRAME_DATA_SIZE;

        fifo_push(&gdrom.bufq, &node->fifo_node);
    }

    if (gdrom.feat_reg.dma_enable) {
        return; // wait for them to write 1 to GDST before doing something
    } else {
        gdrom.int_reason_reg.io = true;
        gdrom.int_reason_reg.cod = false;
        gdrom.stat_reg.drq = true;
    }

    if (!gdrom.dev_ctrl_reg.nien)
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);

    gdrom.state = GDROM_STATE_NORM;
    gdrom.stat_reg.check = false;
    gdrom_clear_error();
}

/*
 * this function is called after 12 bytes have been written to the data
 * register after the drive has received GDROM_CMD_PKT (which puts it in
 * GDROM_STATE_INPUT_PKT
 */
static void gdrom_input_packet(void) {
    gdrom.stat_reg.drq = false;
    gdrom.stat_reg.bsy = false;

    if (!gdrom.dev_ctrl_reg.nien)
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);

    switch (gdrom.pkt_buf[0]) {
    case GDROM_PKT_TEST_UNIT:
        gdrom_input_test_unit_packet();
        break;
    case GDROM_PKT_REQ_STAT:
        GDROM_TRACE("REQ_STAT command received!\n");
        gdrom.state = GDROM_STATE_NORM; // TODO: implement
        break;
    case GDROM_PKT_REQ_MODE:
        gdrom_input_req_mode_packet();
        break;
    case GDROM_PKT_SET_MODE:
        gdrom_input_set_mode_packet();
        break;
    case GDROM_PKT_REQ_ERROR:
        gdrom_input_req_error_packet();
        break;
    case GDROM_PKT_START_DISK:
        gdrom_input_start_disk_packet();
        break;
    case GDROM_PKT_READ_TOC:
        gdrom_input_read_toc_packet();
        break;
    case GDROM_PKT_SUBCODE:
        gdrom_input_read_subcode_packet();
        break;
    case GDROM_PKT_READ:
        gdrom_input_read_packet();
        break;
    case GDROM_PKT_UNKNOWN_71:
        gdrom_input_packet_71();
        break;
    default:
        error_set_feature("unknown GD-ROM packet command");
        error_set_gdrom_command((unsigned)gdrom.pkt_buf[0]);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
        /* state = GDROM_STATE_NORM; */
    }
}

void gdrom_cmd_set_features(void) {
    /* bool set; */

    GDROM_TRACE("SET_FEATURES command received\n");

    if (gdrom.feat_reg.set_feat_enable) {
        /* set = (bool)(feat_reg >> 7); */
    } else {
        GDROM_TRACE("software executed \"Set Features\" command without "
                    "writing 3 to the features register\n");
        return;
    }

    switch (gdrom.sect_cnt_reg.trans_mode) {
    case TRANS_MODE_PIO_DFLT:
        gdrom.trans_mode_vals[TRANS_MODE_PIO_DFLT] =
            gdrom.sect_cnt_reg.mode_val;
        GDROM_TRACE("default PIO transfer mode set to 0x%02x\n",
                    (unsigned)gdrom.trans_mode_vals[TRANS_MODE_PIO_DFLT]);
        break;
    case TRANS_MODE_PIO_FLOW_CTRL:
        gdrom.trans_mode_vals[TRANS_MODE_PIO_FLOW_CTRL] =
            gdrom.sect_cnt_reg.mode_val;
        GDROM_TRACE("flow-control PIO transfer mode set to 0x%02x\n",
                    (unsigned)gdrom.trans_mode_vals[TRANS_MODE_PIO_FLOW_CTRL]);
        break;
    case TRANS_MODE_SINGLE_WORD_DMA:
        gdrom.trans_mode_vals[TRANS_MODE_SINGLE_WORD_DMA] =
            gdrom.sect_cnt_reg.mode_val;
        GDROM_TRACE("single-word DMA transfer mode set to 0x%02x\n",
                    (unsigned)gdrom.trans_mode_vals[TRANS_MODE_SINGLE_WORD_DMA]);
        break;
    case TRANS_MODE_MULTI_WORD_DMA:
        gdrom.trans_mode_vals[TRANS_MODE_MULTI_WORD_DMA] =
            gdrom.sect_cnt_reg.mode_val;
        GDROM_TRACE("multi-word DMA transfer mode set to 0x%02x\n",
                    (unsigned)gdrom.trans_mode_vals[TRANS_MODE_MULTI_WORD_DMA]);
        break;
    case TRANS_MODE_PSEUDO_DMA:
        gdrom.trans_mode_vals[TRANS_MODE_PSEUDO_DMA] =
            gdrom.sect_cnt_reg.mode_val;
        GDROM_TRACE("pseudo-DMA transfer mode set to 0x%02x\n",
                    (unsigned)gdrom.trans_mode_vals[TRANS_MODE_PSEUDO_DMA]);
        break;
    default:
        /*
         * I'm pretty sure this can never happen due to the
         * 'unrecognized transfer mode' ERROR_UNIMPLEMENTED in
         * gdrom_set_sect_cnt_reg.  If that ever gets changed from an error to
         * a warning, then we're going to have to set the trans_mode to some
         * special constant value to show that it's invalid.
         *
         * One other problem is that of the default value; currently it
         * defaults to TRANS_MODE_PIO_DFLT (because that's zero), but I'm not
         * sure if this is the correct default value for the sector count
         * register.
         */
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    gdrom.stat_reg.check = false;
    gdrom_clear_error();
    gdrom.int_reason_reg.cod = true; // is this correct ?

    if (!gdrom.dev_ctrl_reg.nien)
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);
}

void gdrom_cmd_identify(void) {
    GDROM_TRACE("IDENTIFY command received\n");

    gdrom.state = GDROM_STATE_NORM;

    gdrom.stat_reg.bsy = false;
    gdrom.stat_reg.drq = true;

    if (!gdrom.dev_ctrl_reg.nien)
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);

    bufq_clear();

    struct gdrom_bufq_node *node =
        (struct gdrom_bufq_node*)malloc(sizeof(struct gdrom_bufq_node));

    if (!node)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    node->idx = 0;
    node->len = GDROM_IDENT_RESP_LEN;
    memcpy(node->dat, gdrom_ident_resp, sizeof(gdrom_ident_resp));

    gdrom.data_byte_count = GDROM_IDENT_RESP_LEN;

    fifo_push(&gdrom.bufq, &node->fifo_node);

    gdrom.stat_reg.check = false;
    gdrom_clear_error();
}

void gdrom_cmd_begin_packet(void) {
    GDROM_TRACE("PACKET command received\n");

    // clear errors
    // TODO: I'm not sure if this should be done for all commands, or just packet commands
    gdrom.stat_reg.check = false;
    /* memset(&error_reg, 0, sizeof(error_reg)); */

    gdrom.int_reason_reg.io = false;
    gdrom.int_reason_reg.cod = true;
    gdrom.stat_reg.drq = true;
    gdrom.n_bytes_received = 0;
    gdrom.state = GDROM_STATE_INPUT_PKT;
}

static void gdrom_input_test_unit_packet(void) {
    GDROM_TRACE("TEST_UNIT packet received\n");

    // is this correct?
    gdrom.int_reason_reg.cod = true;
    gdrom.int_reason_reg.io = true;
    gdrom.stat_reg.drdy = true;
    gdrom.stat_reg.bsy = false;
    gdrom.stat_reg.drq = false;

    // raise interrupt if it is enabled - this is already done from
    // gdrom_input_packet
    /* if (!(dev_ctrl_reg & DEV_CTRL_NIEN_MASK)) */
    /*     holly_raise_ext_int(HOLLY_EXT_INT_GDROM); */

    gdrom.state = GDROM_STATE_NORM;

    gdrom_clear_error();
    if (mount_check()) {
        gdrom.stat_reg.check = false;
    } else {
        gdrom.stat_reg.check = true;
        gdrom.error_reg.sense_key = SENSE_KEY_NOT_READY;
        gdrom.additional_sense = ADDITIONAL_SENSE_NO_DISC;
    }
}

static void gdrom_input_req_error_packet(void) {
    GDROM_TRACE("REQ_ERROR packet received\n");

    uint8_t len = gdrom.pkt_buf[4];

    uint8_t dat_out[10] = {
        0xf0,
        0,
        gdrom.error_reg.sense_key & 0xf,
        0,
        0,
        0,
        0,
        0,
        (uint8_t)(gdrom.additional_sense),
        0
    };

    if (len > 10)
        len = 10;

    bufq_clear();

    if (len != 0) {
        struct gdrom_bufq_node *node =
            (struct gdrom_bufq_node*)malloc(sizeof(struct gdrom_bufq_node));
        node->idx = 0;
        node->len = len;
        memcpy(&node->dat, dat_out, len);
        fifo_push(&gdrom.bufq, &node->fifo_node);
        gdrom.data_byte_count = node->len;
    }

    gdrom.int_reason_reg.io = true;
    gdrom.int_reason_reg.cod = false;
    gdrom.stat_reg.drq = true;
    gdrom.stat_reg.bsy = false;
    if (!gdrom.dev_ctrl_reg.nien)
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);

    gdrom.state = GDROM_STATE_NORM;
}

/*
 * Exactly what this command does is a mystery to me.  It doesn't appear to
 * convey any data because the bios does not check for any.  What little
 * information I can find would seem to convey that this is some sort of a
 * disk initialization function?
 */
static void gdrom_input_start_disk_packet(void) {
    GDROM_TRACE("START_DISK(=0x70) packet received\n");

    // is this correct?
    gdrom.int_reason_reg.cod = true;
    gdrom.int_reason_reg.io = true;
    gdrom.stat_reg.drdy = true;
    gdrom.stat_reg.bsy = false;
    gdrom.stat_reg.drq = false;

    // raise interrupt if it is enabled - this is already done from
    // gdrom_input_packet
    /* if (!(dev_ctrl_reg & DEV_CTRL_NIEN_MASK)) */
    /*     holly_raise_ext_int(HOLLY_EXT_INT_GDROM); */

    gdrom.state = GDROM_STATE_NORM;

    gdrom.stat_reg.check = false;
    gdrom_clear_error();
}

/*
 * Packet 0x71 is not available in any of the documentation I have on hand,
 * and its purpose is not apparent.  What it does is return a huge chunk of
 * data.  The data returned is never the same (even on the same Dreamcast
 * with the same disc inserted!), and it's not even the same length.
 *
 * TODO: This is some sort of security check.  See p1pkin's comments on
 * https://github.com/washingtondc-emu/washingtondc/commit/3d282f10a22a9e15de6fa5731834ca0a8ed4437a
 * for an explanation.
 *
 * For now, the below implementation returns a string that I captured on a live
 * Dreamcast.  Even though it's always the same string, this seems to work well
 * enough.
 */
static_assert(GDROM_PKT_71_RESP_LEN < GDROM_BUFQ_LEN,
              "GDROM_BUFQ_LEN is too small for the response to packet 0x71");
static void gdrom_input_packet_71(void) {
    GDROM_TRACE("GDROM_PKT_UNKNOWN_71 packet received; sending pre-recorded "
                "response\n");

    bufq_clear();

    struct gdrom_bufq_node *node =
        (struct gdrom_bufq_node*)malloc(sizeof(struct gdrom_bufq_node));
    node->idx = 0;
    node->len = GDROM_PKT_71_RESP_LEN;

    /*
     * XXX this works because GDROM_PKT_71_RESP_LEN is less than GDROM_BUFQ_LEN.
     * if that ever changes, so must this code
     */
    memcpy(node->dat, pkt71_resp, GDROM_PKT_71_RESP_LEN);

    gdrom.data_byte_count = GDROM_PKT_71_RESP_LEN;

    fifo_push(&gdrom.bufq, &node->fifo_node);

    gdrom.int_reason_reg.io = true;
    gdrom.int_reason_reg.cod = false;
    gdrom.stat_reg.drq = true;
    if (!gdrom.dev_ctrl_reg.nien)
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);

    gdrom.state = GDROM_STATE_NORM;

    gdrom.stat_reg.check = false;
    gdrom_clear_error();
}

static void gdrom_input_set_mode_packet(void) {
    // TODO: actually implement this for real instead of ignoring the data

    __attribute__((unused)) unsigned starting_addr = gdrom.pkt_buf[2];
    __attribute__((unused)) unsigned len = gdrom.pkt_buf[4];

    GDROM_TRACE("SET_MODE command received\n");
    GDROM_TRACE("read %u bytes starting at %u\n", len, starting_addr);

    // read features, byte count here
    gdrom.set_mode_bytes_remaining = gdrom.data_byte_count;
    GDROM_TRACE("data_byte_count is %u\n", (unsigned)gdrom.data_byte_count);

    if (gdrom.feat_reg.dma_enable) {
        error_set_feature("GD-ROM SET_MODE command DMA support");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    gdrom.int_reason_reg.io = true;
    gdrom.int_reason_reg.cod = false;
    gdrom.stat_reg.drq = true;

    gdrom.state = GDROM_STATE_SET_MODE;
}

static void gdrom_input_req_mode_packet(void) {
    unsigned starting_addr = gdrom.pkt_buf[2];
    unsigned len = gdrom.pkt_buf[4];

    GDROM_TRACE("REQ_MODE command received\n");
    GDROM_TRACE("read %u bytes starting at %u\n", len, starting_addr);

    bufq_clear();
    if (len != 0) {
        unsigned first_idx = starting_addr;
        unsigned last_idx = starting_addr + (len - 1);

        if (first_idx > (GDROM_REQ_MODE_RESP_LEN - 1))
            first_idx = GDROM_REQ_MODE_RESP_LEN - 1;
        if (last_idx > (GDROM_REQ_MODE_RESP_LEN - 1))
            last_idx = GDROM_REQ_MODE_RESP_LEN - 1;

        struct gdrom_bufq_node *node =
            (struct gdrom_bufq_node*)malloc(sizeof(struct gdrom_bufq_node));

        node->idx = 0;
        node->len = last_idx - first_idx + 1;
        memcpy(&node->dat, gdrom_req_mode_resp + first_idx,
               node->len * sizeof(uint8_t));

        bufq_clear();
        fifo_push(&gdrom.bufq, &node->fifo_node);
        gdrom.data_byte_count = node->len;
    }

    gdrom.int_reason_reg.io = true;
    gdrom.int_reason_reg.cod = false;
    gdrom.stat_reg.drq = true;
    if (!gdrom.dev_ctrl_reg.nien)
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);

    gdrom.state = GDROM_STATE_NORM;

    gdrom.stat_reg.check = false;
    gdrom_clear_error();
}

static void gdrom_input_read_toc_packet(void) {
    unsigned session = gdrom.pkt_buf[1] & 1;
    unsigned len = (((unsigned)gdrom.pkt_buf[3]) << 8) | gdrom.pkt_buf[4];

    GDROM_TRACE("GET_TOC command received\n");
    GDROM_TRACE("request to read %u bytes from the Table of Contents for "
                "Session %u\n", len, session);

    struct mount_toc toc;
    memset(&toc, 0, sizeof(toc));

    // TODO: call mount_check and signal an error if nothing is mounted
    mount_read_toc(&toc, session);

    bufq_clear();
    struct gdrom_bufq_node *node =
        (struct gdrom_bufq_node*)malloc(sizeof(struct gdrom_bufq_node));

    uint8_t const *ptr = mount_encode_toc(&toc);

    if (len > CDROM_TOC_SIZE)
        len = CDROM_TOC_SIZE;

    node->idx = 0;
    node->len = len;
    memcpy(node->dat, ptr, len);
    gdrom.data_byte_count = len;

    fifo_push(&gdrom.bufq, &node->fifo_node);

    gdrom.int_reason_reg.io = true;
    gdrom.int_reason_reg.cod = false;
    gdrom.stat_reg.drq = true;
    if (!gdrom.dev_ctrl_reg.nien)
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);

    gdrom.state = GDROM_STATE_NORM;

    gdrom.stat_reg.check = false;
    gdrom_clear_error();
}

static void gdrom_input_read_subcode_packet(void) {
    unsigned idx;
    unsigned len = (((unsigned)gdrom.pkt_buf[3]) << 8) | gdrom.pkt_buf[4];
    GDROM_TRACE("WARNING: semi-unimplemented CD_SCD packet received:\n");
    for (idx = 0; idx < PKT_LEN; idx++)
        GDROM_TRACE("\t%02x\n", (unsigned)gdrom.pkt_buf[idx]);


    bufq_clear();
    struct gdrom_bufq_node *node =
        (struct gdrom_bufq_node*)malloc(sizeof(struct gdrom_bufq_node));

    node->idx = 0;
    node->len = len;

    // TODO: fill in with real data instead of all zeroes
    memset(node->dat, 0, len);
    gdrom.data_byte_count = len;

    fifo_push(&gdrom.bufq, &node->fifo_node);

    gdrom.int_reason_reg.io = true;
    gdrom.int_reason_reg.cod = false;
    gdrom.stat_reg.drq = true;
    if (!gdrom.dev_ctrl_reg.nien)
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);

    gdrom.state = GDROM_STATE_NORM;

    gdrom.stat_reg.check = false;
    gdrom_clear_error();
}

unsigned gdrom_dma_prot_top(void) {
    return (((gdrom.gdapro_reg & 0x7f00) >> 8) << 20) | 0x08000000;
}

unsigned gdrom_dma_prot_bot(void) {
    return ((gdrom.gdapro_reg & 0x7f) << 20) | 0x080fffff;
}

void gdrom_read_data(uint8_t *buf, unsigned n_bytes) {
    uint8_t *ptr = buf;

    while (n_bytes--) {
        unsigned dat;
        if (bufq_consume_byte(&dat) == 0)
            *ptr++ = dat;
        else
            *ptr++ = 0;
    }

    if (fifo_empty(&gdrom.bufq)) {
        // done transmitting data from gdrom to host - notify host
        gdrom.stat_reg.drq = false;
        gdrom.stat_reg.bsy = false;
        gdrom.stat_reg.drdy = true;
        gdrom.int_reason_reg.cod = true;
        gdrom.int_reason_reg.io = true;
        if (!gdrom.dev_ctrl_reg.nien)
            holly_raise_ext_int(HOLLY_EXT_INT_GDROM);
    }
}

void gdrom_write_data(uint8_t const *buf, unsigned n_bytes) {
    uint32_t dat = 0;
    n_bytes = n_bytes < sizeof(dat) ? n_bytes : sizeof(dat);

    memcpy(&dat, buf, n_bytes);

    GDROM_TRACE("write 0x%04x to data register (%u bytes)\n",
                (unsigned)dat, (unsigned)n_bytes);

    if (gdrom.state == GDROM_STATE_INPUT_PKT) {
        gdrom.pkt_buf[gdrom.n_bytes_received] = dat & 0xff;
        gdrom.pkt_buf[gdrom.n_bytes_received + 1] = (dat >> 8) & 0xff;
        gdrom.n_bytes_received += 2;

        if (gdrom.n_bytes_received >= 12) {
            gdrom.n_bytes_received = 0;
            gdrom_input_packet();
        }
    } else if (gdrom.state == GDROM_STATE_SET_MODE) {
        gdrom.set_mode_bytes_remaining -= n_bytes;
        GDROM_TRACE("received data for SET_MODE, %u bytes remaining\n",
                    gdrom.set_mode_bytes_remaining);

        if (gdrom.set_mode_bytes_remaining <= 0) {
            gdrom.stat_reg.drq = false;
            gdrom.state = GDROM_STATE_NORM;

            if (!gdrom.dev_ctrl_reg.nien)
                holly_raise_ext_int(HOLLY_EXT_INT_GDROM);
        }
    }
}

/*
 * should return the type of disc in the drive (which will usually be
 * DISC_TYPE_GDROM)
 */
enum gdrom_disc_type  gdrom_get_disc_type(void) {
    if (mount_check())
        return DISC_TYPE_GDROM;

    /*
     * this technically evaluates to DISC_TYPE_CDDA, but it doesn't matter
     * because anything that calls this function will be smart enough to check
     * the drive state and realize that there's nothing inserted.
     */
    return (enum gdrom_disc_type)0;
}

/*
 * return the state the physical drive is in (GDROM_STATE_NODISC,
 * GDROM_STATE_PAUSE, etc).
 */
enum gdrom_disc_state gdrom_get_drive_state(void) {
    if (mount_check()) {
        return GDROM_STATE_PAUSE;
    }
    return GDROM_STATE_NODISC;
}

void gdrom_start_dma(void) {
    if (gdrom.dma_start_reg) {
        gdrom.int_reason_reg.io = true;
        gdrom.int_reason_reg.cod = true;
        gdrom.stat_reg.drdy = true;
        gdrom.stat_reg.drq = false;
        gdrom_complete_dma();
    }

    if (!gdrom.dev_ctrl_reg.nien)
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);

    gdrom.state = GDROM_STATE_NORM;
    gdrom.stat_reg.check = false;
    gdrom_clear_error();
}

void gdrom_input_cmd(unsigned cmd) {
    switch (cmd) {
    case GDROM_CMD_PKT:
        gdrom_cmd_begin_packet();
        break;
    case GDROM_CMD_SET_FEAT:
        gdrom_cmd_set_features();
        break;
    case GDROM_CMD_IDENTIFY:
        gdrom_cmd_identify();
        break;
    case GDROM_CMD_NOP:
        /*
         * TODO: I think this is supposed to be able to interrupt in-progress
         * operations, but that isn't implemented yet.
         */
        GDROM_TRACE("WARNING: GDROM_CMD_NOP is not implemented yet\n");
        break;
    default:
        error_set_feature("unknown GD-ROM command");
        error_set_gdrom_command(cmd);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}
