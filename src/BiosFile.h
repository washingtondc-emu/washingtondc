/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016, 2017 snickerbockers
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

#ifndef BIOSFILE_H_
#define BIOSFILE_H_

#include <string.h>
#include <stdint.h>

#include "types.h"
#include "error.h"

#define BIOS_SZ_EXPECT (0x1fffff + 1)

struct BiosFile {
    size_t dat_len;
    uint8_t *dat;
};
typedef struct BiosFile BiosFile;

void bios_file_init_empty(struct BiosFile *bios_file);
void bios_file_init(struct BiosFile *bios_file, char const *path);
void bios_file_cleanup(struct BiosFile *bios_file);

void bios_file_clear(struct BiosFile *bios_file);

uint8_t *bios_file_begin(struct BiosFile *bios_file);
uint8_t *bios_file_end(struct BiosFile *bios_file);

int bios_file_read(struct BiosFile *bios_file, void *buf,
                   size_t addr, size_t len);

#endif
