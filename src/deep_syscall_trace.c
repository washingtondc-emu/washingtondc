/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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
 *
 * Names and indices of these system calls were obtained from Marcus Comstedt's
 * page at http://mc.pp.se/dc/syscalls.html .
 */

#ifndef DEEP_SYSCALL_TRACE
#error rebuild with DEEP_SYSCALL_TRACE enabled
#endif

#include "deep_syscall_trace.h"

#define GDROM_SYSCALL_ADDR 0x8c001000

#define SYSCALL_TRACE(msg, ...)                                         \
    do {                                                                \
        printf("SYSCALL: ");                                            \
        printf(msg, ##__VA_ARGS__);                                     \
    } while (0)

static char const* cmd_name(reg32_t r4) {
#define CMD_NAME_BUF_LEN 32
    static char cmd_name_buf[CMD_NAME_BUF_LEN];

    switch (r4) {
    case 16:
        strncpy(cmd_name_buf, "READ_PIO", CMD_NAME_BUF_LEN);
        break;
    case 17:
        strncpy(cmd_name_buf, "READ_DMA", CMD_NAME_BUF_LEN);
        break;
    case 18:
        strncpy(cmd_name_buf, "GET_TOC", CMD_NAME_BUF_LEN);
        break;
    case 19:
        strncpy(cmd_name_buf, "GET_TOC_2", CMD_NAME_BUF_LEN);
        break;
    case 20:
        strncpy(cmd_name_buf, "PLAY", CMD_NAME_BUF_LEN);
        break;
    case 21:
        strncpy(cmd_name_buf, "PLAY_2", CMD_NAME_BUF_LEN);
        break;
    case 22:
        strncpy(cmd_name_buf, "PAUSE", CMD_NAME_BUF_LEN);
        break;
    case 23:
        strncpy(cmd_name_buf, "RELEASE", CMD_NAME_BUF_LEN);
        break;
    case 24:
        strncpy(cmd_name_buf, "INIT", CMD_NAME_BUF_LEN);
        break;
    case 27:
        strncpy(cmd_name_buf, "SEEK", CMD_NAME_BUF_LEN);
        break;
    case 28:
        strncpy(cmd_name_buf, "READ", CMD_NAME_BUF_LEN);
        break;
    case 33:
        strncpy(cmd_name_buf, "STOP", CMD_NAME_BUF_LEN);
        break;
    case 34:
        strncpy(cmd_name_buf, "GET_SCD", CMD_NAME_BUF_LEN);
        break;
    case 35:
        strncpy(cmd_name_buf, "GET_SESSION", CMD_NAME_BUF_LEN);
        break;
    default:
        snprintf(cmd_name_buf, CMD_NAME_BUF_LEN,
                 "UNKNOWN <0x%02x>", (unsigned)r4);
        break;
    }
    cmd_name_buf[CMD_NAME_BUF_LEN - 1] = '\0';
    return cmd_name_buf;
}

void deep_syscall_notify_jump(addr32_t pc) {
    Sh4 *sh4 = dreamcast_get_cpu();
    if (pc == GDROM_SYSCALL_ADDR) {
        reg32_t r4 = *sh4_gen_reg(sh4, 4);
        reg32_t r6 = *sh4_gen_reg(sh4, 6);
        reg32_t r7 = *sh4_gen_reg(sh4, 7);

        if (r6 == -1) {
            if (r7 == 0) {
                SYSCALL_TRACE("MISC_INIT\n");
            } else if (r7 == 1) {
                SYSCALL_TRACE("MISC_SETVECTOR\n");
            } else {
                SYSCALL_TRACE("unknown system call (r6=0x%02x, r7=0x%02x)\n",
                              (unsigned)r6, (unsigned)r7);
            }
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
            case 4:
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
            default:
                SYSCALL_TRACE("unknown system call (r6=0x%02x, r7=0x%02x)\n",
                              (unsigned)r6, (unsigned)r7);
            }
        } else {
            SYSCALL_TRACE("unknown system call (r6=0x%02x, r7=0x%02x)\n",
                          (unsigned)r6, (unsigned)r7);
        }
    }
}
