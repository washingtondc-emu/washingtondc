/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016 snickerbockers
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

#include "BiosFile.h"

void bios_file_init_empty(struct BiosFile *bios_file) {
    bios_file->dat_len = BIOS_SZ_EXPECT;
    bios_file->dat = (uint8_t*)malloc(sizeof(uint8_t) * bios_file->dat_len);

    if (!bios_file->dat) {
        RAISE_ERROR(ERROR_FAILED_ALLOC);
        return;
    }
}

void bios_file_init(struct BiosFile *bios_file, char const *path) {
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

    bios_file->dat = (uint8_t*)malloc(file_len * sizeof(uint8_t));
    if (!bios_file->dat) {
        RAISE_ERROR(ERROR_FAILED_ALLOC);
        return;
    }

    if (fread(bios_file->dat, sizeof(uint8_t), file_len, fp) != file_len) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
        free(bios_file->dat);
        fclose(fp);
        return;
    }

    fclose(fp);

    bios_file->dat_len = file_len;

    if (file_len != BIOS_SZ_EXPECT) {
        printf("WARNING - unexpected bios size (expected %d, got %d).  This "
               "BIOS will still be loaded but it could cause issues.\n",
               BIOS_SZ_EXPECT, (int)file_len);
    }
}

void bios_file_cleanup(struct BiosFile *bios_file) {
    if (bios_file->dat)
        free(bios_file->dat);

    memset(bios_file, 0, sizeof(*bios_file));
}

void bios_file_clear(struct BiosFile *bios_file) {
    memset(bios_file->dat, 0, bios_file->dat_len);
}

uint8_t *bios_file_begin(struct BiosFile *bios_file) {
    return bios_file->dat;
}

uint8_t *bios_file_end(struct BiosFile *bios_file) {
    return bios_file->dat + bios_file->dat_len;
}

int bios_file_read(struct BiosFile *bios_file, void *buf,
                   size_t addr, size_t len) {
    memcpy(buf, bios_file->dat + addr, len);
    return MEM_ACCESS_SUCCESS;
}
