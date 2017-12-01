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

#include <stdio.h>

#include "hw/sh4/sh4.h"
#include "dreamcast.h"

/*
 * The purpose of this system is to track system calls (specifically GD-ROM
 * system calls) and log them.  The eventual goal is to report on success codes,
 * but I'm putting off implementing that for now because I've realized this
 * isn't going to be useful for debugging the early boot because the firmware
 * doesn't start using its own system calls until after it gets to the RTC
 * reset screen.
 */

#ifndef DEEP_SYSCALL_TRACE
#error rebuild with DEEP_SYSCALL_TRACE enabled
#endif

#include "deep_syscall_trace.h"

#define GDROM_SYSCALL_ADDR 0x8c001000

#define SYSCALL_TRACE(msg, ...)                                         \
    do {                                                                \
        LOG_DBG("SYSCALL: ");                                           \
        LOG_DBG(msg, ##__VA_ARGS__);                                    \
    } while (0)

static char const* cmd_name(reg32_t r4) {
    switch (r4) {
    case 16:
        return "READ_PIO";
    case 17:
        return "READ_DMA";
    case 18:
        return "GET_TOC";
    case 19:
        return "GET_TOC_2";
    case 20:
        return "PLAY";
    case 21:
        return "PLAY_2";
    case 22:
        return "PAUSE";
    case 23:
        return "RELEASE";
    case 24:
        return "INIT";
    case 27:
        return "SEEK";
    case 28:
        return "READ";
    case 33:
        return "STOP";
    case 34:
        return "GET_SCD";
    case 35:
        return "GET_SESSION";
    }

    return "UNKNOWN_CMD";
}

void deep_syscall_notify_jump(addr32_t pc) {
    Sh4 *sh4 = dreamcast_get_cpu();
    if (pc == GDROM_SYSCALL_ADDR) {
        reg32_t r4 = *sh4_gen_reg(sh4, 4);
        reg32_t r6 = *sh4_gen_reg(sh4, 6);
        reg32_t r7= *sh4_gen_reg(sh4, 7);

        if (r6 == -1) {
            if (r7 == 0)
                SYSCALL_TRACE("MISC_INIT\n");
            else if (r7 == 1)
                SYSCALL_TRACE("MISC_SETVECTOR\n");
            else
                SYSCALL_TRACE("unkown system call\n");
        } else if (r6 == 0) {
            switch (r7) {
            case 0:
                SYSCALL_TRACE("GDROM_SEND_COMMAND <0x%02x> %s\n",
                              (unsigned)r4, cmd_name(r4));
                break;
            case 1:
                SYSCALL_TRACE("GDROM_CHECK_COMMAND\n");
                break;
            case 2:
                SYSCALL_TRACE("GDROM_MAINLOOP\n");
                break;
            case 3:
                SYSCALL_TRACE("GDROM_INIT\n");
                break;
            case 5:
                SYSCALL_TRACE("GDROM_CHECK_DRIVE\n");
                break;
            case 8:
                SYSCALL_TRACE("GDROM_ABORT_COMMAND\n");
                break;
            case 9:
                SYSCALL_TRACE("GDROM_RESET\n");
                break;
            case 10:
                SYSCALL_TRACE("GDROM_SECTOR_MODE\n");
                break;
            dfault:
                SYSCALL_TRACE("unkown system call\n");
            }
        } else {
            SYSCALL_TRACE("unkown system call\n");
        }
    }
}
