/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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

#ifndef ABI_H_
#define ABI_H_

#include "emit_x86_64.h"

#if defined(__CYGWIN__) || defined(_WIN32)
#define ABI_MICROSOFT
#endif

#ifdef __linux__
#define ABI_UNIX
#endif

/*
 * x86_64 System V ABI (for Unix systems).
 * Source:
 *     https://en.wikipedia.org/wiki/X86_calling_conventions#System_V_AMD64_ABI
 *
 * non-float args go into RDI, RSI, RDX, RCX, R8, R9
 * Subsequent args get pushed on to the stack, just like in x86 stdcall.
 * If calling a variadic function, number of floats in SSE/AvX regs needs to be
 * passed in RAX
 * non-float return values go into RAX
 * If returning a 128-bit value, RDX is used too (I am not sure which register
 * is high and which register is low).
 * values in RBX, RBP, R12-R15 will be saved by the callee (and also I think
 * RSP, but the wiki page doesn't say that).
 * All other values should be considered clobberred by the function call.
 */

#if defined(ABI_UNIX)

#define REG_ARG0 RDI
#define REG_ARG1 RSI
#define REG_ARG2 RDX
#define REG_ARG3 RCX

#elif defined(ABI_MICROSOFT)

#define REG_ARG0 RCX
#define REG_ARG1 RDX
#define REG_ARG2 R8
#define REG_ARG3 R9

#endif

#define REG_RET RAX
#define REG_RET_XMM XMM0

/*
 * volatile registers: registers whose values are not preserved across function
 * calls.
 *
 * The REG_ARG and REG_RET registers defined above are also considered to be
 * volatile general-purpose registers, and can be safely be used as such.
 */

#define REG_VOL0 R10
#define REG_VOL1 R11

/*
 * non-volatile registers: registers whose values are preserved across function
 * calls.
 *
 * Note that the ones listed here are just the ones common across Microsoft and
 * Unix ABIs.  There are other nonvolatile registers you have to save when
 * opening a stack frame.
 */

#define REG_NONVOL0 RBX
#define REG_NONVOL1 R12
#define REG_NONVOL2 R13
#define REG_NONVOL3 R14
#define REG_NONVOL4 R15

#endif
