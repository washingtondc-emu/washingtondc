/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016, 2017, 2019 snickerbockers
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

#ifndef TYPES_H_
#define TYPES_H_

#include <assert.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t reg32_t;
typedef uint32_t addr32_t;

static_assert(sizeof(reg32_t) == sizeof(float),
              "cannot fit a float into a reg32_t");

#ifdef __cplusplus
}
#endif

#endif
