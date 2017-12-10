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

#ifndef G1_REG_H_
#define G1_REG_H_

#include "types.h"

struct g1_mem_mapped_reg;

typedef int(*g1_reg_read_handler_t)(struct g1_mem_mapped_reg const *reg_info,
                                    void *buf, addr32_t addr, unsigned len);
typedef int(*g1_reg_write_handler_t)(struct g1_mem_mapped_reg const *reg_info,
                                     void const *buf, addr32_t addr,
                                     unsigned len);

struct g1_mem_mapped_reg {
    char const *reg_name;

    addr32_t addr;

    unsigned len;

    g1_reg_read_handler_t on_read;
    g1_reg_write_handler_t on_write;
};

int g1_reg_read(void *buf, size_t addr, size_t len);
int g1_reg_write(void const *buf, size_t addr, size_t len);

#endif
