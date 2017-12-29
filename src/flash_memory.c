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
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "mem_code.h"
#include "error.h"
#include "types.h"
#include "log.h"

#include "flash_memory.h"

/*
 * 128KB flash memory emulation
 *
 * According to MAME, this device is a macronix 29lv160tmc
 */

/*
 * all flash commands are prefaced by 0xaa written to 0x00205555 and then 0x55
 * written to 0x00202aaa.  After that, the command code is input and then its
 * parameter.
 */
#define FLASH_ADDR_AA 0x00205555
#define FLASH_ADDR_55 0x00202aaa

/*
 * prior to a FLASH_CMD_ERASE (0x30) byte, the firmware always sends a
 * FLASH_CMD_PRE_ERASE (0x80) byte.  Both bytes are preceded by the usual AA55
 * pattern.
 */
#define FLASH_CMD_ERASE 0x30
#define FLASH_CMD_PRE_ERASE 0x80
#define FLASH_CMD_WRITE 0xa0

// when you send it an erase command, it erases an entire sector
#define FLASH_SECTOR_SIZE (16 * 1024)
#define FLASH_SECTOR_MASK (~(FLASH_SECTOR_SIZE - 1))

enum flash_state {
    FLASH_STATE_AA,
    FLASH_STATE_55,
    FLASH_STATE_CMD,
    FLASH_STATE_WRITE,

    FLASH_STATE_ERASE
};

enum flash_state state = FLASH_STATE_AA;

/*
 * this is set to true when we receive a FLASH_CMD_PRE_ERASE command.
 * It is cleared upon receiving the FLASH_CMD_ERASE
 */
static bool erase_unlocked;

#define FLASH_MEM_TRACE(msg, ...) flash_mem_do_trace(msg, ##__VA_ARGS__)

// uncomment this to log all flash memory read/write operations
// #define FLASH_MEM_VERBOSE

// don't call this directly, use the FLASH_MEM_TRACE macro instead
static void flash_mem_do_trace(char const *msg, ...);

/*
 * this gets called from the write function to input data into the system one
 * byte at a time, including state transitions and command processing.
 */
static void flash_mem_input_byte(addr32_t addr, uint8_t val);

/*
 * this gets called from flash_mem_input_byte when it detects that the current
 * byte is a new command byte.  This function is responsible for deciding what
 * state to transfer to.
 */
static void flash_mem_input_cmd(addr32_t addr, uint8_t val);

static void flash_mem_do_erase(addr32_t addr);

static void flash_mem_do_write_cmd(addr32_t addr, uint8_t val);

static uint8_t flash_mem[FLASH_MEM_SZ];

void flash_mem_load(char const *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
        return;
    }

    if (fseek(fp, 0, SEEK_END) < 0) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
        fclose(fp);
        return;
    }

    long file_len = ftell(fp);
    if (file_len <= 0) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
        fclose(fp);
        return;
    }

    if (fseek(fp, 0, SEEK_SET) < 0) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
        fclose(fp);
        return;
    }

    if (file_len != FLASH_MEM_SZ) {
        FLASH_MEM_TRACE("WARNING - unexpected flash memory size (expected %d "
                        "bytes, got %d bytes)).  This will still be loaded "
                        "even though it\'s incorrect\n",
                        FLASH_MEM_SZ, (int)file_len);
    }

    if (file_len > FLASH_MEM_SZ || file_len < 0)
        file_len = FLASH_MEM_SZ;

    if (fread(flash_mem, sizeof(uint8_t), file_len, fp) != file_len) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
        fclose(fp);
        return;
    }

    fclose(fp);
}

float flash_mem_read_float(addr32_t addr) {
    uint32_t tmp = flash_mem_read_32(addr);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

void flash_mem_write_float(addr32_t addr, float val) {
    error_set_feature("flash memory write-lengths other than 1-byte");
    error_set_length(4);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

double flash_mem_read_double(addr32_t addr) {
    error_set_address(addr);
    error_set_length(sizeof(double));
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void flash_mem_write_double(addr32_t addr, double val) {
    error_set_feature("flash memory write-lengths other than 1-byte");
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint32_t flash_mem_read_32(addr32_t addr) {
    if ((addr + sizeof(uint32_t) - 1 > ADDR_FLASH_LAST) ||
        (addr < ADDR_FLASH_FIRST)) {
        error_set_address(addr);
        error_set_length(sizeof(uint32_t));
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

#ifdef FLASH_MEM_VERBOSE
    FLASH_MEM_TRACE("read %08x (4 bytes) from %08x\n",
                    (unsigned)val, (unsigned)addr);
#endif

    uint32_t const *in_ptr = (uint32_t const*)flash_mem;
    return in_ptr[(addr - ADDR_FLASH_FIRST) / sizeof(uint32_t)];
}

void flash_mem_write_32(addr32_t addr, uint32_t val) {
    error_set_feature("flash memory write-lengths other than 1-byte");
    error_set_length(4);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint16_t flash_mem_read_16(addr32_t addr) {
    if ((addr + sizeof(uint16_t) - 1 > ADDR_FLASH_LAST) ||
        (addr < ADDR_FLASH_FIRST)) {
        error_set_address(addr);
        error_set_length(sizeof(uint16_t));
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

#ifdef FLASH_MEM_VERBOSE
    FLASH_MEM_TRACE("read %04x (2 bytes) from %08x\n",
                    (unsigned)val, (unsigned)addr);
#endif

    uint16_t const *in_ptr = (uint16_t const*)flash_mem;
    return in_ptr[(addr - ADDR_FLASH_FIRST) / sizeof(uint16_t)];
}

void flash_mem_write_16(addr32_t addr, uint16_t val) {
    error_set_feature("flash memory write-lengths other than 1-byte");
    error_set_length(2);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

uint8_t flash_mem_read_8(addr32_t addr) {
    if ((addr + sizeof(uint8_t) - 1 > ADDR_FLASH_LAST) ||
        (addr < ADDR_FLASH_FIRST)) {
        error_set_address(addr);
        error_set_length(sizeof(uint8_t));
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

#ifdef FLASH_MEM_VERBOSE
    FLASH_MEM_TRACE("read %02x (1 byte) from %08x\n",
                    (unsigned)val, (unsigned)addr);
#endif

    uint8_t const *in_ptr = (uint8_t const*)flash_mem;
    return in_ptr[(addr - ADDR_FLASH_FIRST) / sizeof(uint8_t)];
}

void flash_mem_write_8(addr32_t addr, uint8_t val) {
    if ((addr > ADDR_FLASH_LAST) || (addr < ADDR_FLASH_FIRST)) {
        error_set_address(addr);
        error_set_length(1);
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

#ifdef FLASH_MEM_VERBOSE
    FLASH_MEM_TRACE("write %02x to %08x\n", (unsigned)val, (unsigned)addr);
#endif

    flash_mem_input_byte(addr, val);
}

static void flash_mem_do_trace(char const *msg, ...) {
    va_list var_args;
    va_start(var_args, msg);

    LOG_DBG("FLASH_MEM: ");

    vprintf(msg, var_args);

    va_end(var_args);
}

static void flash_mem_input_byte(addr32_t addr, uint8_t val) {
    switch (state) {
    case FLASH_STATE_AA:
        if (val == 0xaa && addr == FLASH_ADDR_AA) {
            state = FLASH_STATE_55;
        } else {
            FLASH_MEM_TRACE("garbage data input (was expecting AA to "
                            "0x%08x)\n", FLASH_ADDR_AA);
        }
        break;
    case FLASH_STATE_55:
        if (val == 0x55 && addr == FLASH_ADDR_55) {
            state = FLASH_STATE_CMD;
        } else {
            FLASH_MEM_TRACE("garbage data input (was expecting tt to "
                            "0x%08x)\n", FLASH_ADDR_55);
        }
        break;
    case FLASH_STATE_CMD:
        flash_mem_input_cmd(addr, val);
        break;
    case FLASH_STATE_WRITE:
        flash_mem_do_write_cmd(addr, val);
        break;
    default:
        RAISE_ERROR(ERROR_INTEGRITY);
    }
}

/*
 * TODO: need to figure out what should happen when the software sends
 * FLASH_CMD_PRE_ERASE but doesn't sned FLASH_CMD_ERASE immediately after.
 *
 * Does the device remain open for a subsequent erase, or does the erase command
 * become locked again?
 *
 * I also left in an ERROR_UNIMPLEMENTED for the case where FLASH_CMD_ERASE is
 * not immediately preceded by FLASH_CMD_PRE_ERASE, although in that case
 * FLASH_CMD_ERASE is probably just a no-op.
 */
static void flash_mem_input_cmd(addr32_t addr, uint8_t val) {
    FLASH_MEM_TRACE("input command 0x%02x\n", (unsigned)val);

    switch (val) {
    case FLASH_CMD_ERASE:
        if (erase_unlocked) {
            flash_mem_do_erase(addr);
            state = FLASH_STATE_AA;
            erase_unlocked = false;
        } else {
            error_set_feature("proper response for failure to send the "
                              "flash PRE_ERASE command");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
        break;
    case FLASH_CMD_PRE_ERASE:
        if (erase_unlocked) {
            error_set_feature("proper response for not sending "
                              "FLASH_CMD_ERASE immediately after "
                              "FLASH_CMD_PRE_ERASE");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
        state = FLASH_STATE_AA;
        erase_unlocked = true;
        break;
    case FLASH_CMD_WRITE:
        if (erase_unlocked) {
            error_set_feature("proper response for not sending "
                              "FLASH_CMD_ERASE immediately after "
                              "FLASH_CMD_PRE_ERASE");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
        state = FLASH_STATE_WRITE;
        break;
    default:
        FLASH_MEM_TRACE("command 0x%02x is unrecognized\n", (unsigned)val);
        state = FLASH_STATE_AA;
        if (erase_unlocked) {
            error_set_feature("proper response for not sending "
                              "FLASH_CMD_ERASE immediately after "
                              "FLASH_CMD_PRE_ERASE");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
    }
}

static void flash_mem_do_erase(addr32_t addr) {
    addr -= ADDR_FLASH_FIRST;
    addr &= FLASH_SECTOR_MASK;

    FLASH_MEM_TRACE("FLASH_CMD_ERASE - ERASE SECTOR 0x%08x\n", (unsigned)addr);

    memset(flash_mem + addr, 0xff, FLASH_SECTOR_SIZE);
}

static void flash_mem_do_write_cmd(addr32_t addr, uint8_t val) {
    uint8_t tmp;

    FLASH_MEM_TRACE("FLASH_CMD_WRITE - AND 0x%02x into address 0x%08x\n",
                    (unsigned)val, (unsigned)addr);

    memcpy(&tmp, flash_mem + (addr - ADDR_FLASH_FIRST), sizeof(tmp));
    tmp &= val;
    memcpy(flash_mem + (addr - ADDR_FLASH_FIRST), &tmp, sizeof(tmp));

    state = FLASH_STATE_AA;
}
