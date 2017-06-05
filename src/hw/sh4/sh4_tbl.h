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

#ifndef SH4_TBL_H_
#define SH4_TBL_H_

#include <stdint.h>

/*
 * lookup tables of the sine and cosine functions used by the fsca instruction
 *
 * in theory these are actually the same table with a 90-degree offset, but I'd
 * like to go through and verify that before I make any assumptions, so for now
 * these are two separate tables
 */

#define FSCA_TBL_LEN 32768

extern uint32_t const sh4_fsca_sin_tbl[FSCA_TBL_LEN];
extern uint32_t const sh4_fsca_cos_tbl[FSCA_TBL_LEN];

#endif
