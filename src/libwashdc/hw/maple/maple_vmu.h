/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2020 snickerbockers
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

#ifndef MAPLE_VMU_H_
#define MAPLE_VMU_H_

#include <stdint.h>

extern struct maple_switch_table maple_vmu_switch_table;

#define MAPLE_VMU_BLOCK_SZ 512
#define MAPLE_VMU_N_BLOCKS 256
#define MAPLE_VMU_DAT_LEN (MAPLE_VMU_BLOCK_SZ * MAPLE_VMU_N_BLOCKS)

struct maple_vmu {
    unsigned char *datp;
    char *backing_path;
};

struct maple_device;
int maple_vmu_init(struct maple_device *dev, char const *backing_path);

#endif
