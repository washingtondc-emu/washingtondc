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

#ifndef MAPLE_REG_H_
#define MAPLE_REG_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

int maple_reg_read(void *buf, size_t addr, size_t len);
int maple_reg_write(void const *buf, size_t addr, size_t len);

addr32_t maple_get_dma_prot_bot(void);
addr32_t maple_get_dma_prot_top(void);

#ifdef __cplusplus
}
#endif

#endif
