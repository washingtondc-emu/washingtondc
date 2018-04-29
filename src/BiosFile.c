/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2018 snickerbockers
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

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "mem_code.h"
#include "error.h"
#include "log.h"

#include "BiosFile.h"

struct BiosFile {
    size_t dat_len;
    uint8_t *dat;
};
typedef struct BiosFile BiosFile;

static struct BiosFile bios_file;

void bios_file_init(char const *path) {
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

    bios_file.dat = (uint8_t*)malloc(file_len * sizeof(uint8_t));
    if (!bios_file.dat) {
        RAISE_ERROR(ERROR_FAILED_ALLOC);
        return;
    }

    if (fread(bios_file.dat, sizeof(uint8_t), file_len, fp) != file_len) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
        free(bios_file.dat);
        fclose(fp);
        return;
    }

    fclose(fp);

    bios_file.dat_len = file_len;

    if (file_len != BIOS_SZ_EXPECT) {
        LOG_WARN("WARNING - unexpected bios size (expected %d, got %d).  This "
                 "BIOS will still be loaded but it could cause issues.\n",
                 BIOS_SZ_EXPECT, (int)file_len);
    }
}

void bios_file_cleanup(void) {
    if (bios_file.dat)
        free(bios_file.dat);

    memset(&bios_file, 0, sizeof(bios_file));
}

int bios_file_read(void *buf, size_t addr, size_t len) {
    memcpy(buf, bios_file.dat + addr, len);
    return MEM_ACCESS_SUCCESS;
}

uint8_t bios_file_read_8(addr32_t addr) {
    return bios_file.dat[addr];
}

uint16_t bios_file_read_16(addr32_t addr) {
    uint16_t const *bios16 = (uint16_t const*)bios_file.dat;
    return bios16[addr / 2];
}

uint32_t bios_file_read_32(addr32_t addr) {
    uint32_t const *bios32 = (uint32_t const*)bios_file.dat;
    return bios32[addr / 4];
}

float bios_file_read_float(addr32_t addr) {
    uint32_t tmp = bios_file_read_32(addr);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

double bios_file_read_double(addr32_t addr) {
    error_set_address(addr);
    error_set_length(8);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void bios_file_write_8(addr32_t addr, uint8_t val) {
    /*
     * I'm not sure what the correct response is when guest software tries to
     * write to the boot rom...
     */
    error_set_feature("proper response for when the guest "
                      "tries to write to the bios");
    error_set_address(addr);
    error_set_length(1);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void bios_file_write_16(addr32_t addr, uint16_t val) {
    /*
     * I'm not sure what the correct response is when guest software tries to
     * write to the boot rom...
     */
    error_set_feature("proper response for when the guest "
                      "tries to write to the bios");
    error_set_address(addr);
    error_set_length(2);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void bios_file_write_32(addr32_t addr, uint32_t val) {
    /*
     * I'm not sure what the correct response is when guest software tries to
     * write to the boot rom...
     */
    error_set_feature("proper response for when the guest "
                      "tries to write to the bios");
    error_set_address(addr);
    error_set_length(4);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void bios_file_write_float(addr32_t addr, float val) {
    /*
     * I'm not sure what the correct response is when guest software tries to
     * write to the boot rom...
     */
    error_set_feature("proper response for when the guest "
                      "tries to write to the bios");
    error_set_address(addr);
    error_set_length(4);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void bios_file_write_double(addr32_t addr, double val) {
    /*
     * I'm not sure what the correct response is when guest software tries to
     * write to the boot rom...
     */
    error_set_feature("proper response for when the guest "
                      "tries to write to the bios");
    error_set_address(addr);
    error_set_length(8);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}
