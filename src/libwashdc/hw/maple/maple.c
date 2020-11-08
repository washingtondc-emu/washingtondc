/*******************************************************************************
 *
 * Copyright 2017-2020 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "sh4.h"
#include "sh4_dmac.h"
#include "hw/sys/holly_intc.h"
#include "maple_device.h"
#include "washdc/error.h"
#include "dc_sched.h"
#include "dreamcast.h"
#include "maple_reg.h"

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
#define MAPLE_CMD_MASK (0xf << MAPLE_CMD_SHIFT)

#define MAPLE_ADDR_SHIFT 8
#define MAPLE_ADDR_MASK (0xff << MAPLE_ADDR_SHIFT)

#define MAPLE_PACK_LEN_SHIFT 24
#define MAPLE_PACK_LEN_MASK (0xff << MAPLE_PACK_LEN_SHIFT)

/*
 * delay that specifies how long a DMA transaction should take.
 *
 * TODO: I ran a test that seemingly confirmed this value on real hardware to
 * be approximately 1ms.  Using that value broke several games (Namco Museum and
 * Sonic Adventure 2 at the least and probably others too) so I'm going with 0
 * for now.
 *
 * The test only tested DEVINFO packets, so it's possible that GETCOND is
 * actually faster.  The test was also built on a KallistiOS kernel that I
 * hacked up with my own timing code, so that may have been interfering with it
 * as well.  A new test should be written which doesn't rely on any
 * pre-existing SDK so that we can be sure nothing is interfering with the
 * timing.
 *
 * I'm pretty sure that Star Wars Episode I Racer needs there to be *some*
 * latency here, so this does need to be figured out eventually.
 */
#define MAPLE_DMA_COMPLETE_DELAY 0

static void maple_dma_complete_int_event_handler(struct SchedEvent *event);

static void maple_dma_complete(struct maple *ctxt);

static void maple_handle_devinfo(struct maple *ctxt, struct maple_frame *frame);
static void maple_handle_getcond(struct maple *ctxt, struct maple_frame *frame);
static void maple_handle_bwrite(struct maple *ctxt, struct maple_frame *frame);
static void maple_handle_setcond(struct maple *ctxt, struct maple_frame *frame);

static void maple_decode_frame(struct maple_frame *frame_out,
                               uint32_t const dat[3]);

static void
maple_write_frame_resp(struct maple *ctxt,
                       struct maple_frame *frame, unsigned resp_code);

static DEF_ERROR_INT_ATTR(maple_command);

static void maple_handle_frame(struct maple *ctxt, struct maple_frame *frame) {
    MAPLE_TRACE("frame received!\n");
    MAPLE_TRACE("\tlength: %u\n", frame->input_len);
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

    switch (frame->ptrn) {
    case 0:
        break;
    case 7:
        if (frame->pack_len)
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        return;
    default:RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    switch (frame->cmd) {
    case MAPLE_CMD_DEVINFO:
        maple_handle_devinfo(ctxt, frame);
        break;
    case MAPLE_CMD_GETCOND:
        maple_handle_getcond(ctxt, frame);
        break;
    case MAPLE_CMD_BWRITE:
        maple_handle_bwrite(ctxt, frame);
        break;
    case MAPLE_CMD_SETCOND:
        maple_handle_setcond(ctxt, frame);
        break;
        /* case MAPLE_CMD_NOP: */
    /*     frame->output_len = 0; */
    /*     maple_write_frame_resp(frame, MAPLE_RESP_NONE); */
    /*     break; */
    /* case MAPLE_CMD_FUCK: */
        /* break; */
    default:
        error_set_feature("ERROR: no handler for maplebus command frame");
        error_set_maple_command(frame->cmd);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

static void
maple_handle_devinfo(struct maple *ctxt, struct maple_frame *frame) {
    MAPLE_TRACE("DEVINFO maplebus frame received\n");

    struct maple_device *dev = maple_device_get(ctxt, frame->maple_addr);

    if (dev->enable) {
        struct maple_devinfo devinfo;
        maple_device_info(dev, &devinfo);

        maple_compile_devinfo(&devinfo, frame->output_data);
        frame->output_len = MAPLE_DEVINFO_SIZE;
        maple_write_frame_resp(ctxt, frame, MAPLE_RESP_DEVINFO);
    } else {
        // this port/unit combo is not plugged in
        frame->output_len = 0;
        maple_write_frame_resp(ctxt, frame, MAPLE_RESP_NONE);
    }

    maple_dma_complete(ctxt);
}

static void
maple_handle_getcond(struct maple *ctxt, struct maple_frame *frame) {
    MAPLE_TRACE("GETCOND maplebus frame received\n");

    struct maple_device *dev = maple_device_get(ctxt, frame->maple_addr);

    if (dev->enable) {
        struct maple_cond cond;

        maple_device_cond(dev, &cond);
        maple_compile_cond(&cond, frame->output_data);
        switch (cond.tp) {
        case MAPLE_COND_TYPE_CONTROLLER:
            frame->output_len = MAPLE_CONTROLLER_COND_SIZE;
            break;
        case MAPLE_COND_TYPE_KEYBOARD:
            frame->output_len = MAPLE_KEYBOARD_COND_SIZE;
            break;
        }
        maple_write_frame_resp(ctxt, frame, MAPLE_RESP_DATATRF);
    } else {
        error_set_feature("proper response for when the guest tries to send "
                          "the GETCOND command to an empty maple port");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    maple_dma_complete(ctxt);
}

/*
 * Write data to a maple device
 *
 * PuruPuru uses this, VMU apparently does too
 *
 * First DWORD should be the function of the device that receives the command
 * (like PuruPuru, LCD, memory, etc).
 *
 * Other data appears to be specific to each function, although the second
 * DWORD is always 0?
 */
static void maple_handle_bwrite(struct maple *ctxt, struct maple_frame *frame) {
    MAPLE_TRACE("BWRITE maplebus frame received\n");

    struct maple_device *dev = maple_device_get(ctxt, frame->maple_addr);

    if (dev->enable) {
        struct maple_bwrite bwrite;

        bwrite.n_dwords = frame->input_len / sizeof(uint32_t);
        size_t n_bytes = sizeof(uint32_t) * bwrite.n_dwords;
        if (n_bytes > MAPLE_FRAME_INPUT_DATA_LEN) {
            /*
             * too much data, don't want to over-read the buffer in struct
             * maple_frame!
             */
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        bwrite.dat = malloc(n_bytes);
        if (!bwrite.dat)
            RAISE_ERROR(ERROR_FAILED_ALLOC);
        memcpy(bwrite.dat, frame->input_data, n_bytes);

        maple_device_bwrite(dev, &bwrite);

        frame->output_len = 0;
        maple_write_frame_resp(ctxt, frame, MAPLE_RESP_DATATRF);

        free(bwrite.dat);
    } else {
        error_set_feature("proper response for when the guest tries to send "
                          "the BWRITE command to an empty maple port");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    maple_dma_complete(ctxt);
}

/*
 * AFAIK VMU uses this to beep and PuruPuru uses this to vibrate.
 *
 * TBH i feel like this and bwrite are redundandt but that's the way things are.
 */
static void
maple_handle_setcond(struct maple *ctxt, struct maple_frame *frame) {
    MAPLE_TRACE("SETCOND maplebus frame received\n");

    struct maple_device *dev = maple_device_get(ctxt, frame->maple_addr);

    if (dev->enable) {
        struct maple_setcond setcond;

        setcond.n_dwords = frame->input_len / sizeof(uint32_t);
        size_t n_bytes = sizeof(uint32_t) * setcond.n_dwords;
        if (n_bytes > MAPLE_FRAME_INPUT_DATA_LEN) {
            /*
             * too much data, don't want to over-read the buffer in struct
             * maple_frame!
             */
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        setcond.dat = malloc(n_bytes);
        if (!setcond.dat)
            RAISE_ERROR(ERROR_FAILED_ALLOC);
        memcpy(setcond.dat, frame->input_data, n_bytes);

        maple_device_setcond(dev, &setcond);

        frame->output_len = 0;
        maple_write_frame_resp(ctxt, frame, MAPLE_RESP_DATATRF);

        free(setcond.dat);
    } else {
        error_set_feature("proper response for when the guest tries to send "
                          "the BWRITE command to an empty maple port");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    maple_dma_complete(ctxt);
}

static void maple_write_frame_resp(struct maple *ctxt,
                            struct maple_frame *frame, unsigned resp_code) {
    unsigned subdevs = 0;
    unsigned port, unit;
    maple_addr_unpack(frame->maple_addr, &port, &unit);
    if (unit == 0) {
        if (ctxt->devs[port][1].enable)
            subdevs |= 1;
        if (ctxt->devs[port][2].enable)
            subdevs |= 2;
    }

    unsigned len = frame->output_len / 4;
    uint32_t pkt_hdr = ((resp_code << MAPLE_CMD_SHIFT) & MAPLE_CMD_MASK) |
        ((frame->maple_addr << MAPLE_ADDR_SHIFT) & MAPLE_ADDR_MASK) |
        ((len << MAPLE_PACK_LEN_SHIFT) & MAPLE_PACK_LEN_MASK) |
        (subdevs << MAPLE_PORT_SHIFT);

    sh4_dmac_transfer_to_mem(dreamcast_get_cpu(), frame->recv_addr,
                             sizeof(pkt_hdr), 1, &pkt_hdr);
    if (len) {
        sh4_dmac_transfer_to_mem(dreamcast_get_cpu(), frame->recv_addr + 4, 1,
                                 frame->output_len, frame->output_data);
    }
}

static void maple_decode_frame(struct maple_frame *frame_out,
                               uint32_t const dat[3]) {
    uint32_t msg_length_port = dat[0];
    uint32_t recv_addr = dat[1];
    uint32_t cmd_addr_pack_len = dat[2];

    for (unsigned idx = 0; idx < 3; idx++)
        MAPLE_TRACE("%08x\n", dat[idx]);

    frame_out->input_len = (msg_length_port & MAPLE_LENGTH_MASK) >>
        MAPLE_LENGTH_SHIFT;
    frame_out->input_len *= 4;
    frame_out->port = (msg_length_port & MAPLE_PORT_MASK) >> MAPLE_PORT_SHIFT;
    frame_out->ptrn = (msg_length_port & MAPLE_PTRN_MASK) >> MAPLE_PTRN_SHIFT;
    frame_out->last_frame = (bool)(msg_length_port & MAPLE_LAST_MASK);

    frame_out->cmd = (enum maple_cmd)((cmd_addr_pack_len & MAPLE_CMD_MASK) >>
                                      MAPLE_CMD_SHIFT);
    frame_out->maple_addr = (cmd_addr_pack_len & MAPLE_ADDR_MASK) >>
        MAPLE_ADDR_SHIFT;
    frame_out->pack_len = (cmd_addr_pack_len & MAPLE_PACK_LEN_MASK) >>
        MAPLE_PACK_LEN_SHIFT;

    frame_out->recv_addr = recv_addr;

    if (frame_out->input_len != (4 * frame_out->pack_len)) {
        // IDK if these two values are supposed to always be the same or not
        error_set_feature("maple frames with differing lengths");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

void maple_process_dma(struct maple *ctxt, uint32_t src_addr) {
    bool xfer_complete;
    unsigned ptrn;
    struct maple_frame frame;
    uint32_t frame_meta[3];

#ifdef INVARIANTS
    if (!ctxt->dma_en)
        RAISE_ERROR(ERROR_INTEGRITY);
#endif

    do {
        sh4_dmac_transfer_from_mem(dreamcast_get_cpu(), src_addr,
                                   sizeof(frame_meta[0]), 1, frame_meta);
        xfer_complete = (bool)(frame_meta[0] >> 31);
        ptrn = (frame_meta[0] >> 8) & 7;

        src_addr += 4;

        switch (ptrn) {
        case 0:
            break;
        case 7:
            continue;
        default:
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        sh4_dmac_transfer_from_mem(dreamcast_get_cpu(), src_addr,
                                   sizeof(frame_meta[1]), 2, frame_meta + 1);
        maple_decode_frame(&frame, frame_meta);

        src_addr += 8;

        if (frame.input_len) {
            sh4_dmac_transfer_from_mem(dreamcast_get_cpu(), src_addr, 4,
                                       frame.input_len / 4,
                                       frame.input_data);
        }

        src_addr += frame.input_len;

        maple_handle_frame(ctxt, &frame);

    } while (!xfer_complete);
}

void maple_addr_unpack(unsigned addr, unsigned *port_out, unsigned *unit_out) {
    unsigned unit, port;

    if ((addr & 0x3f) == 0x20)
        unit = 0;
    else if ((addr & 0x1f) == 1)
        unit = 1;
    else if ((addr & 0x1f) == 2)
        unit = 2;
    else if ((addr & 0x1f) == 4)
        unit = 3;
    else if ((addr & 0x1f) == 8)
        unit = 4;
    else if ((addr & 0x1f) == 16)
        unit = 5;
    else
        RAISE_ERROR(ERROR_INTEGRITY);

    port = (addr >> 6) & 0x3;

    *port_out = port;
    *unit_out = unit;
}

unsigned maple_addr_pack(unsigned port, unsigned unit) {
#ifdef INVARIANTS
    if (port >= MAPLE_PORT_COUNT || unit >= MAPLE_UNIT_COUNT)
        RAISE_ERROR(ERROR_INTEGRITY);
#endif

    unsigned addr;

    if (unit > 0)
        addr =  (1 << (unit - 1)) & 0x1f;
    else
        addr = 0x20;

    addr |= port << 6;

    return addr;
}

static void maple_dma_complete(struct maple *ctxt) {
    if (!ctxt->dma_complete_int_event_scheduled) {
        ctxt->dma_complete_int_event_scheduled = true;
        ctxt->dma_complete_int_event.arg_ptr = ctxt;
        ctxt->dma_complete_int_event.handler =
            maple_dma_complete_int_event_handler;
        ctxt->dma_complete_int_event.when =
            clock_cycle_stamp(ctxt->maple_clk) + MAPLE_DMA_COMPLETE_DELAY;
        sched_event(ctxt->maple_clk, &ctxt->dma_complete_int_event);
    }
}

static void maple_dma_complete_int_event_handler(struct SchedEvent *event) {
    struct maple *ctxt = (struct maple*)event->arg_ptr;
    ctxt->dma_complete_int_event_scheduled = false;
    holly_raise_nrm_int(HOLLY_MAPLE_ISTNRM_DMA_COMPLETE);
}

void maple_init(struct maple *maple_ctxt, struct dc_clock *clk) {
    memset(maple_ctxt, 0, sizeof(*maple_ctxt));

    maple_ctxt->maple_dma_prot_bot = 0;
    maple_ctxt->maple_dma_prot_top = (0x1 << 27) | (0x7f << 20);
    maple_ctxt->maple_clk = clk;

    maple_reg_init(maple_ctxt);
}

void maple_cleanup(struct maple *maple_ctxt) {
    unsigned port, unit;
    for (port = 0; port < MAPLE_PORT_COUNT; port++)
        for (unit = 0; unit < MAPLE_UNIT_COUNT; unit++)
            if (maple_ctxt->devs[port][unit].enable)
                maple_device_cleanup(maple_ctxt, maple_addr_pack(port, unit));

    maple_reg_cleanup(maple_ctxt);
}

void maple_notify_pre_vblank(struct maple *ctxt) {
    if ((ctxt->vblank_init_unlocked || ctxt->vblank_autoinit) && ctxt->dma_en) {
        MAPLE_TRACE("Initiating Maple DMA transfer automatically due to "
                    "incoming VBLANK.\n");
        maple_process_dma(ctxt, ctxt->maple_dma_cmd_start);
        if (!ctxt->vblank_autoinit)
            ctxt->vblank_init_unlocked = false;
    }
}
