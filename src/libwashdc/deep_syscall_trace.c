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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "log.h"
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

static uint32_t ret_addr;
static bool in_syscall;

#define SYSCALL_TRACE(msg, ...)                                         \
    do {                                                                \
        printf("SYSCALL: ");                                            \
        printf(msg, ##__VA_ARGS__);                                     \
        LOG_DBG("SYSCALL: "msg, ##__VA_ARGS__);                         \
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

static struct syscall_stat {
    int id;
    uint32_t initial_state;
    union {
        uint32_t n_bytes_addr;
        uint32_t check_cmd_out_addr;
        uint32_t check_drive_out_addr;
    };
} syscall_stat;

void deep_syscall_notify_jump(addr32_t pc) {
    Sh4 *sh4 = dreamcast_get_cpu();
    if (pc == GDROM_SYSCALL_ADDR) {
        if (in_syscall) {
            SYSCALL_TRACE("recursive syscall detected.  "
                          "Trace will be unreliable!\n");
        }

        reg32_t r4 = *sh4_gen_reg(sh4, 4);
        reg32_t r5 = *sh4_gen_reg(sh4, 5);
        reg32_t r6 = *sh4_gen_reg(sh4, 6);
        reg32_t r7 = *sh4_gen_reg(sh4, 7);
        ret_addr = sh4->reg[SH4_REG_PR];
        in_syscall = true;
        syscall_stat.id = -1;

        if (dc_try_read32(0x8c0012e8 + 20, &syscall_stat.initial_state) != 0)
            SYSCALL_TRACE("FAILURE TO READ GDROM SYSCALL STATE\n");

        if (r6 == -1) {
            if (r7 == 0) {
                SYSCALL_TRACE("MISC_INIT\n");
            } else if (r7 == 1) {
                SYSCALL_TRACE("MISC_SETVECTOR\n");
            } else {
                SYSCALL_TRACE("unknown system call (r4=%08X, r5=%08X, r6=%02X, r7=%02X)\n",
                              (unsigned)r4, (unsigned)r5, (unsigned)r6, (unsigned)r7);
            }
        } else if (r6 == 0) {
            syscall_stat.id = r7;
            switch (r7) {
            case 0: {
                SYSCALL_TRACE("GDROM_SEND_COMMAND <0x%02x> %s\n",
                              (unsigned)r4, cmd_name(r4));
                SYSCALL_TRACE("\treturn_addr %08X\n", (unsigned)ret_addr);
                SYSCALL_TRACE("\tCOMMAND %02X\n", (unsigned)r4);
                uint32_t n_dwords_addr = 0x8c0012e8 + 0x4e8 + r4 * 4;
                uint32_t n_dwords;
                if (dc_try_read32(n_dwords_addr, &n_dwords) == 0) {
                    SYSCALL_TRACE("\tparams %08X\n", (unsigned)r5);
                    unsigned idx;
                    for (idx = 0; idx < n_dwords; idx++) {
                        uint32_t val;
                        if (dc_try_read32(r5 + idx * 4, &val) == 0)
                            SYSCALL_TRACE("\t\tparams[%u] %08X\n", idx, (unsigned)val);
                        else
                            SYSCALL_TRACE("\t\tparams[%u] <ERROR>\n", idx);
                    }
                } else {
                    SYSCALL_TRACE("\t<unable to determine parameter length>\n");
                }
            }
                break;
            case 1:
                SYSCALL_TRACE("GDROM_CHECK_COMMAND\n");
                SYSCALL_TRACE("\treturn_addr %08X\n", (unsigned)ret_addr);
                SYSCALL_TRACE("\treq_id %08X\n", (unsigned)r4);
                SYSCALL_TRACE("\tparams %08X\n", (unsigned)r5);
                break;
            case 2:
                SYSCALL_TRACE("GDROM_MAINLOOP\n");
                SYSCALL_TRACE("\treturn_addr %08X\n", (unsigned)ret_addr);
                break;
            case 3:
                SYSCALL_TRACE("GDROM_INIT\n");
                SYSCALL_TRACE("\treturn_addr %08X\n", (unsigned)ret_addr);
                break;
            case 4:
                SYSCALL_TRACE("GDROM_CHECK_DRIVE\n");
                SYSCALL_TRACE("\treturn_addr %08X\n", (unsigned)ret_addr);
                SYSCALL_TRACE("\tparams %08X\n", (unsigned)r4);
                syscall_stat.check_drive_out_addr = r4;
                break;
            case 6: {
                SYSCALL_TRACE("GDROM_DMA_BEGIN\n");
                SYSCALL_TRACE("\treturn_addr %08X\n", (unsigned)ret_addr);
                SYSCALL_TRACE("\treq_id %08X\n", (unsigned)r4);
                SYSCALL_TRACE("\tparams %08X\n", (unsigned)r5);
                uint32_t addr_dst;
                if (dc_try_read32(r5, &addr_dst) == 0) {
                    SYSCALL_TRACE("\t\tdst %08X\n", (unsigned)addr_dst);
                } else {
                    SYSCALL_TRACE("\t\tdst <unable to read from %08X>\n",
                                  (unsigned)r5);
                }
                uint32_t n_bytes;
                if (dc_try_read32(r5+4, &n_bytes) == 0) {
                    SYSCALL_TRACE("\t\tn_bytes %08X\n", (unsigned)n_bytes);
                } else {
                    SYSCALL_TRACE("\t\tdst <unable to read from %08X>\n",
                                  (unsigned)(r5+4));
                }
            }
                break;
            case 7:
                SYSCALL_TRACE("GDROM_DMA_CHECK\n");
                SYSCALL_TRACE("\treturn_addr %08X\n", (unsigned)ret_addr);
                SYSCALL_TRACE("\treq_id %08X\n", (unsigned)r4);
                SYSCALL_TRACE("\tparams %08X\n", (unsigned)r5);
                syscall_stat.n_bytes_addr = r5;
                break;
            case 8:
                SYSCALL_TRACE("GDROM_ABORT_COMMAND\n");
                SYSCALL_TRACE("\treturn_addr %08X\n", (unsigned)ret_addr);
                SYSCALL_TRACE("\treq_id = 0x%02x\n",
                              (unsigned)r4);
                break;
            case 9:
                SYSCALL_TRACE("GDROM_RESET\n");
                SYSCALL_TRACE("\treturn_addr %08X\n", (unsigned)ret_addr);
                break;
            case 10:
                SYSCALL_TRACE("GDROM_SECTOR_MODE\n");
                SYSCALL_TRACE("\treturn_addr %08X\n", (unsigned)ret_addr);
                break;
            default:
                SYSCALL_TRACE("unknown system call (r4=%08X, r5=%08X, r6=%02X, r7=%02X)\n",
                              (unsigned)r4, (unsigned)r5, (unsigned)r6, (unsigned)r7);
            }
        } else {
            SYSCALL_TRACE("unknown system call (r4=%08X, r5=%08X, r6=%02X, r7=%02X)\n",
                          (unsigned)r4, (unsigned)r5, (unsigned)r6, (unsigned)r7);
        }
    } else if (in_syscall && pc == ret_addr) {
        switch (syscall_stat.id) {
        case 1: {
            // GDROM_CHECK_COMMAND
            uint32_t param;
            unsigned idx;
            for (idx = 0; idx < 4; idx++) {
                if (dc_try_read32(syscall_stat.check_cmd_out_addr + 4 * idx, &param) == 0) {
                    SYSCALL_TRACE("\tparams[%u] <returned> %08X\n", idx,
                                  (unsigned)param);
                } else {
                    SYSCALL_TRACE("\t\tparams[%u] <returned> <unable to read "
                                  "from %08X>\n", idx,
                                  (unsigned)syscall_stat.check_cmd_out_addr + 4 * idx);
                }
            }
        }
            break;
        case 4: {
            uint32_t drive_stat, disc_fmt;
            if (dc_try_read32(syscall_stat.check_drive_out_addr,
                              &drive_stat) == 0) {
                char const *drive_stat_str;
                switch (drive_stat) {
                case 0:
                    drive_stat_str = "BUSY";
                    break;
                case 1:
                    drive_stat_str = "PAUSE";
                    break;
                case 2:
                    drive_stat_str = "STANDBY";
                    break;
                case 3:
                    drive_stat_str = "PLAY";
                    break;
                case 4:
                    drive_stat_str = "SEEK";
                    break;
                case 5:
                    drive_stat_str = "SCAN";
                    break;
                case 6:
                    drive_stat_str = "OPEN";
                    break;
                case 7:
                    drive_stat_str = "NO_DISC";
                    break;
                case 8:
                    drive_stat_str = "RETRY";
                    break;
                case 9:
                    drive_stat_str = "ERROR";
                    break;
                default:
                    drive_stat_str = "UNKNOWN (EMULATOR OR FIRMWARE ERROR)";
                }
                SYSCALL_TRACE("\tdrive_status <returned> %08X <%s>\n",
                              (unsigned)drive_stat, drive_stat_str);
            } else {
                SYSCALL_TRACE("\tdrive_status <returned> <unable to read "
                              "from %08X>\n",
                              (unsigned)syscall_stat.check_drive_out_addr);
            }
            if (dc_try_read32(syscall_stat.check_drive_out_addr + 4,
                              &disc_fmt) == 0) {
                char const *disc_fmt_str;
                switch(disc_fmt) {
                case 0x00:
                    disc_fmt_str = "CD DIGITAL AUDIO";
                    break;
                case 0x10:
                    disc_fmt_str = "CD-ROM";
                    break;
                case 0x20:
                    disc_fmt_str = "CD-ROM XA";
                    break;
                case 0x30:
                    disc_fmt_str = "CD-I";
                    break;
                case 0x80:
                    disc_fmt_str = "GD-ROM";
                    break;
                default:
                    disc_fmt_str = "UNKNOWN (EMULATOR OR FIRMWARE ERROR)";
                }
                SYSCALL_TRACE("\tdisc_format <returned> %08X <%s>\n",
                              (unsigned)disc_fmt, disc_fmt_str);
            } else {
                SYSCALL_TRACE("\tdisc_format <returned> <unable to read "
                              "from %08X>\n",
                              (unsigned)syscall_stat.check_drive_out_addr + 4);
            }
        }
            break;
        case 7: {
            // GDROM DMA CHECK
            uint32_t n_bytes;
            if (dc_try_read32(syscall_stat.n_bytes_addr, &n_bytes) == 0) {
                SYSCALL_TRACE("\tn_bytes <returned> %08X\n",
                              (unsigned)n_bytes);
            } else {
                SYSCALL_TRACE("\t\tn_bytes <returned> <unable to read "
                              "from %08X>\n",
                              (unsigned)syscall_stat.n_bytes_addr);
            }
        }
            break;
        }
        uint32_t gdrom_syscall_state;
        if (dc_try_read32(0x8c0012e8 + 20, &gdrom_syscall_state) == 0) {
            if (syscall_stat.initial_state != gdrom_syscall_state) {
                SYSCALL_TRACE("gd-rom syscall state changed from %d to %d "
                              "during system call\n",
                              (int)syscall_stat.initial_state,
                              (int)gdrom_syscall_state);
            }
        } else {
            SYSCALL_TRACE("FAILURE TO READ GDROM SYSCALL STATE\n");
        }

        SYSCALL_TRACE("Returining 0x%08x to 0x%08x\n",
                      (unsigned)*sh4_gen_reg(sh4, 0), ret_addr);
        in_syscall = false;
    }
}
