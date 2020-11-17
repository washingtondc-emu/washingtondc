/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018, 2020 snickerbockers
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
    MAPLE_RESP_OK = 7,
    MAPLE_RESP_DATATRF = 8,

    // maplebus command codes
    MAPLE_CMD_FUCK = 0,
    MAPLE_CMD_DEVINFO = 1,
    MAPLE_CMD_NOP = 7,
    MAPLE_CMD_GETCOND = 9,
    MAPLE_CMD_MEMINFO = 10,
    MAPLE_CMD_BREAD = 11,
    MAPLE_CMD_BWRITE = 12,
    MAPLE_CMD_BSYNC = 13,
    MAPLE_CMD_SETCOND = 14
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

static inline uint32_t maple_convert_endian(uint32_t val) {
    /*
     * I've been working on this emulator for more than four years now
     * and this is the first time I've ever written something that won't
     * break on big-endian platforms.
     *
     * If I'm ever able to fulfill my dreams of porting WashingtonDC to
     * the Nintendo Wii U then I will probably regret ignoring
     * endian-ness for so long.
     */
    uint8_t rawdat[4];
    memcpy(rawdat, &val, sizeof(rawdat));
    return ((uint32_t)rawdat[0] << 24) |
        ((uint32_t)rawdat[1] << 16) |
        ((uint32_t)rawdat[2] << 8) |
        (uint32_t)rawdat[3];
}

#endif
