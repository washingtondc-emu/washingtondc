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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sh4.h"
#include "sh4_dmac.h"
#include "hw/sys/holly_intc.h"

#include "maple.h"

#define MAPLE_LENGTH_SHIFT 0
#define MAPLE_LENGTH_MASK (0xff << MAPLE_LENGTH_SHIFT)

#define MAPLE_PORT_SHIFT 16
#define MAPLE_PORT_MASK (0x3 << MAPLE_PORT_SHIFT)

#define MAPLE_PTRN_SHIFT 8
#define MAPLE_PTRN_MASK (0x7 << MAPLE_PTRN_SHIFT)

#define MAPLE_LAST_SHIFT 31
#define MAPLE_LAST_MASK (1 << MAPLE_LAST_SHIFT)

#define MAPLE_CMD_SHIFT 0
#define MAPLE_CMD_MASK (0xff << MAPLE_CMD_SHIFT)

#define MAPLE_ADDR_SHIFT 8
#define MAPLE_ADDR_MASK (0xff << MAPLE_ADDR_SHIFT)

#define MAPLE_PACK_LEN_SHIFT 24
#define MAPLE_PACK_LEN_MASK (0xff << MAPLE_PACK_LEN_SHIFT)

static void maple_handle_devinfo(struct maple_frame *frame);

void maple_handle_frame(struct maple_frame *frame) {
    MAPLE_TRACE("frame received!\n");
    MAPLE_TRACE("\tlength: %u\n", frame->len);
    MAPLE_TRACE("\tport: %u\n", frame->port);
    MAPLE_TRACE("\tpattern: %u\n", frame->ptrn);
    MAPLE_TRACE("\treceive address: 0x%08x\n", (unsigned)frame->recv_addr);
    MAPLE_TRACE("\tcommand: %02x\n", (unsigned)frame->cmd);
    MAPLE_TRACE("\tmaple address: %02x\n", frame->maple_addr);
    MAPLE_TRACE("\tpacket length: %u\n", frame->pack_len);

    if (frame->last_frame)
        MAPLE_TRACE("\tthis was the last frame\n");
    else
        MAPLE_TRACE("\tthis was not the last frame\n");

    switch (frame->cmd) {
    case MAPLE_CMD_DEVINFO:
        maple_handle_devinfo(frame);
        break;
    default:
        MAPLE_TRACE("ERROR: no handler for maplebus command-frame %02x\n",
                    (unsigned)frame->cmd);
    }
}

static void maple_handle_devinfo(struct maple_frame *frame) {
    MAPLE_TRACE("DEVINFO maplebus frame received\n");

    struct maple_frame resp;
    memset(&resp, 0, sizeof(resp));

    resp.port = frame->port;
    resp.ptrn = frame->ptrn;
    resp.recv_addr = frame->recv_addr;
    resp.last_frame = frame->last_frame;
    resp.maple_addr = frame->maple_addr;

    // for now, hardcode all controller ports as being unplugged
    resp.cmd = MAPLE_RESP_NONE;

    maple_write_frame(&resp, frame->recv_addr);
    holly_raise_nrm_int(HOLLY_MAPLE_ISTNRM_DMA_COMPLETE);
}

uint32_t maple_write_frame(struct maple_frame const *frame, uint32_t addr) {
    uint32_t cmd = (((unsigned)frame->cmd) << MAPLE_CMD_SHIFT) & MAPLE_CMD_MASK;
    uint32_t maple_addr = (frame->maple_addr << MAPLE_ADDR_SHIFT) &
        MAPLE_ADDR_MASK;
    uint32_t pack_len = (frame->pack_len << MAPLE_PACK_LEN_SHIFT) &
        MAPLE_PACK_LEN_MASK;
    uint32_t cmd_addr_pack_len = cmd | maple_addr | pack_len;

    MAPLE_TRACE("\t%08x\n", (unsigned)cmd_addr_pack_len);
    sh4_dmac_transfer_to_mem(addr, sizeof(cmd_addr_pack_len), 1,
                             &cmd_addr_pack_len);
    addr += 4;

    MAPLE_TRACE("\tlength is %u bytes\n", frame->len);

    /* if (frame->len / 4) { */
    /*     sh4_dmac_transfer_to_mem(addr, 4, frame->len / 4, frame->data); */
    /*     addr += frame->len; */
    /* } */

    return addr;
}

uint32_t maple_read_frame(struct maple_frame *frame_out, uint32_t addr) {
    uint32_t msg_length_port;

    sh4_dmac_transfer_from_mem(addr, sizeof(msg_length_port),
                               1, &msg_length_port);
    addr += 4;

    frame_out->len = (msg_length_port & MAPLE_LENGTH_MASK) >>
        MAPLE_LENGTH_SHIFT;
    frame_out->len *= 4;
    frame_out->port = (msg_length_port & MAPLE_PORT_MASK) >> MAPLE_PORT_SHIFT;
    frame_out->ptrn = (msg_length_port & MAPLE_PTRN_MASK) >> MAPLE_PTRN_SHIFT;
    frame_out->last_frame = (bool)(msg_length_port & MAPLE_LAST_MASK);

    uint32_t recv_addr;
    sh4_dmac_transfer_from_mem(addr, sizeof(recv_addr),
                               1, &recv_addr);
    addr += 4;
    frame_out->recv_addr = recv_addr;

    uint32_t cmd_addr_pack_len;
    sh4_dmac_transfer_from_mem(addr, sizeof(cmd_addr_pack_len),
                               1, &cmd_addr_pack_len);
    addr += 4;

    frame_out->cmd = (enum maple_cmd)((cmd_addr_pack_len & MAPLE_CMD_MASK) >>
                                      MAPLE_CMD_SHIFT);
    frame_out->maple_addr = (cmd_addr_pack_len & MAPLE_ADDR_MASK) >>
        MAPLE_ADDR_SHIFT;
    frame_out->pack_len = (cmd_addr_pack_len & MAPLE_PACK_LEN_MASK) >>
        MAPLE_PACK_LEN_SHIFT;

    if (frame_out->len) {
        frame_out->data = malloc(frame_out->len);
        sh4_dmac_transfer_from_mem(addr, 4, frame_out->len / 4,
                                   frame_out->data);
    } else {
        frame_out->data = NULL;
    }

    addr += frame_out->len;

    return addr;
}

void maple_do_trace(char const *msg, ...) {
    va_list var_args;
    va_start(var_args, msg);

    printf("MAPLE: ");

    vprintf(msg, var_args);

    va_end(var_args);
}
