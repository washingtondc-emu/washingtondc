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

#include "code_cache.h"

#ifdef ENABLE_JIT_X86_64
#include "x86_64/exec_mem.h"
#include "x86_64/native_dispatch.h"
#include "x86_64/native_mem.h"
#endif

#include "jit.h"

void jit_init(struct dc_clock *clk) {
#ifdef ENABLE_JIT_X86_64
    exec_mem_init();
    native_dispatch_init(clk);
    native_mem_init();
#endif
    code_cache_init();
}

void jit_cleanup(void) {
    code_cache_cleanup();
#ifdef ENABLE_JIT_X86_64
    native_mem_cleanup();
    native_dispatch_cleanup();
    exec_mem_cleanup();
#endif
}
