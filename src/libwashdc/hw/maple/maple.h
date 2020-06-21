/*******************************************************************************
 *
 * Copyright 2017, 2018, 2020 snickerbockers
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

#ifndef MAPLE_H_
#define MAPLE_H_

#include <stdint.h>
#include <stdbool.h>

#include "dc_sched.h"
#include "washdc/types.h"
#include "mem_areas.h"
#include "mmio.h"
#include "maple_device.h"

#define N_MAPLE_REGS (ADDR_MAPLE_LAST - ADDR_MAPLE_FIRST + 1)

DECL_MMIO_REGION(maple_reg, N_MAPLE_REGS, ADDR_MAPLE_FIRST, uint32_t)

enum maple_cmd {
    // maplebus response codes
    MAPLE_RESP_NONE = -1,
    MAPLE_RESP_DEVINFO = 5,
    MAPLE_RESP_DATATRF = 8,

    // maplebus command codes
    MAPLE_CMD_FUCK = 0,
    MAPLE_CMD_DEVINFO = 1,
    MAPLE_CMD_NOP = 7,
    MAPLE_CMD_GETCOND = 9
};

#define MAPLE_FRAME_OUTPUT_DATA_LEN 1024
#define MAPLE_FRAME_INPUT_DATA_LEN 1024

struct maple_frame {
    /* enum maple_cmd cmd; */
    unsigned port;
    unsigned ptrn;
    uint32_t recv_addr;

    bool last_frame;

    enum maple_cmd cmd;
    unsigned maple_addr;
    unsigned pack_len;

    unsigned input_len; // length of input in bytes (not words)
    uint8_t input_data[MAPLE_FRAME_INPUT_DATA_LEN];

    unsigned output_len; // length of input in bytes (not words)
    uint8_t output_data[MAPLE_FRAME_OUTPUT_DATA_LEN];
};

#define MAPLE_PORT_COUNT 4
#define MAPLE_UNIT_COUNT 6

enum maple_dma_init_mode {
    MAPLE_DMA_INIT_MANUAL = 0,
    MAPLE_DMA_INIT_VBLANK = 1
};

struct maple {
    struct dc_clock *maple_clk;
    struct SchedEvent dma_complete_int_event;
    bool dma_complete_int_event_scheduled;

    enum maple_dma_init_mode dma_init_mode;
    bool vblank_init_unlocked;
    bool vblank_autoinit;
    bool dma_en;

    addr32_t maple_dma_prot_bot, maple_dma_prot_top, maple_dma_cmd_start;

    struct maple_device devs[MAPLE_PORT_COUNT][MAPLE_UNIT_COUNT];

    struct mmio_region_maple_reg mmio_region_maple_reg;
    uint8_t reg_backing[N_MAPLE_REGS];
    uint32_t reg_msys;
};

void maple_write_frame_resp(struct maple_frame *frame, unsigned resp_code);

#define MAPLE_TRACE(msg, ...) LOG_DBG("MAPLE: "msg, ##__VA_ARGS__)

// don't call this directly, use the MAPLE_TRACE macro instead
void maple_do_trace(char const *msg, ...);

unsigned maple_addr_pack(unsigned port, unsigned unit);
void maple_addr_unpack(unsigned addr, unsigned *port_out, unsigned *unit_out);

void maple_init(struct maple *ctxt, struct dc_clock *clk);
void maple_cleanup(struct maple *ctxt);

void maple_process_dma(struct maple *ctxt, uint32_t src_addr);

/*
 * maple has a DMA mode that's automatically triggered one line before a vblank
 * interrupt.  This function gets called by the SPG to let it know that it's
 * time for that.
 */
void maple_notify_pre_vblank(struct maple *ctxt);

#endif
