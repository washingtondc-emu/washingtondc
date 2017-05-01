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

#ifndef ARCH_FPU_H_
#define ARCH_FPU_H_

/*
 * TODO: if/when I add support for other architectures, come up with a
 * way to choose which version of arch_fenv.hpp to include instead of always
 * including the x86_64 one.
 */
#include "x86_64/arch_fenv.h"

#ifdef __cplusplus
extern "C" {
#endif

// replacement for the C99 isfinite macro
#define arch_isfinite(val) __builtin_isfinite((val))

// replacement for the C99 fesetround function
int arch_fesetround (int round);

// replacement for the C99 fegetround function
int arch_fegetround (void);

#ifdef __cplusplus
}
#endif

#endif
