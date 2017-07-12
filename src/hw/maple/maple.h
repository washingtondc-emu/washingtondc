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

#ifndef MAPLE_H_
#define MAPLE_H_

#include <stdint.h>
#include <stdbool.h>

enum maple_cmd {
    // maplebus response codes
    MAPLE_RESP_NONE = -1,

    // maplebus command codes
    MAPLE_CMD_DEVINFO = 1
};

struct maple_frame {
    /* enum maple_cmd cmd; */
    unsigned port;
    unsigned ptrn;
    uint32_t recv_addr;

    bool last_frame;

    enum maple_cmd cmd;
    unsigned maple_addr;
    unsigned pack_len;

    unsigned len;
    void *data;
};

void maple_handle_frame(struct maple_frame *frame);

uint32_t maple_read_frame(struct maple_frame *frame_out, uint32_t addr);
uint32_t maple_write_frame(struct maple_frame const *frame, uint32_t addr);

#endif
