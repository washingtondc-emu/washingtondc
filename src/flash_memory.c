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

#include "flash_memory.h"

#define FLASH_MEM_TRACE(msg, ...) flash_mem_do_trace(msg, ##__VA_ARGS__)

// don't call this directly, use the FLASH_MEM_TRACE macro instead
void flash_mem_do_trace(char const *msg, ...);

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

int flash_mem_read(void *buf, size_t addr, size_t len) {
    if ((addr + len - 1 > ADDR_FLASH_LAST) || (addr < ADDR_FLASH_FIRST)) {
        error_set_address(addr);
        error_set_length(len);
        PENDING_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
        return MEM_ACCESS_FAILURE;
    }

    if (len == 1) {
        uint8_t val;
        memcpy(&val, flash_mem + (addr - ADDR_FLASH_FIRST), sizeof(val));
        FLASH_MEM_TRACE("read %02x from %08x\n", (unsigned)val, (unsigned)addr);
    } else if (len == 2) {
        uint16_t val;
        memcpy(&val, flash_mem + (addr - ADDR_FLASH_FIRST), sizeof(val));
        FLASH_MEM_TRACE("read %04x from %08x\n", (unsigned)val, (unsigned)addr);
    } else if (len == 4) {
        uint32_t val;
        memcpy(&val, flash_mem + (addr - ADDR_FLASH_FIRST), sizeof(val));
        FLASH_MEM_TRACE("read %08x from %08x\n", (unsigned)val, (unsigned)addr);
    } else {
        FLASH_MEM_TRACE("read %08x bytes from %08x\n",
                        (unsigned)len, (unsigned)addr);
    }

    memcpy(buf, flash_mem + (addr - ADDR_FLASH_FIRST), len);

    return MEM_ACCESS_SUCCESS;
}

int flash_mem_write(void const *buf, size_t addr, size_t len) {
    if ((addr + len - 1 > ADDR_FLASH_LAST) || (addr < ADDR_FLASH_FIRST)) {
        error_set_address(addr);
        error_set_length(len);
        PENDING_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
        return MEM_ACCESS_FAILURE;
    }

    if (len == 1) {
        uint8_t val;
        memcpy(&val, buf, sizeof(val));
        FLASH_MEM_TRACE("write %02x to %08x\n", (unsigned)val, (unsigned)addr);
    } else if (len == 2) {
        uint16_t val;
        memcpy(&val, buf, sizeof(val));
        FLASH_MEM_TRACE("write %04x to %08x\n", (unsigned)val, (unsigned)addr);
    } else if (len == 4) {
        uint32_t val;
        memcpy(&val, buf, sizeof(val));
        FLASH_MEM_TRACE("write %08x to %08x\n", (unsigned)val, (unsigned)addr);
    } else {
        FLASH_MEM_TRACE("write %08x bytes to %08x\n",
                        (unsigned)len, (unsigned)addr);
    }

    memcpy(flash_mem + (addr - ADDR_FLASH_FIRST), buf, len);

    return MEM_ACCESS_SUCCESS;
}

void flash_mem_do_trace(char const *msg, ...) {
    va_list var_args;
    va_start(var_args, msg);

    printf("FLASH_MEM: ");

    vprintf(msg, var_args);

    va_end(var_args);
}
