/*******************************************************************************
 *
 * Copyright 2016-2019 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#ifndef GDBSTUB_H_
#define GDBSTUB_H_

#ifndef USE_LIBEVENT
#error this file should not be built with USE_LIBEVENT disabled!
#endif
#ifndef ENABLE_DEBUGGER
#error this file whould not be built with ENABLE_DEBUGGER disabled!
#endif

#include <cstdint>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/buffer.h>

#include "washdc/debugger.h"
#include "washdc/types.h"

// it's 'cause 1999 is the year the Dreamcast came out in America
#define GDB_PORT_NO 1999

// see sh_sh4_register_name in gdb/sh-tdep.c in the gdb source code
enum gdb_reg_order {
    R0, R1, R2, R3, R4, R5, R6, R7,
    R8, R9, R10, R11, R12, R13, R14, R15,

    PC, PR, GBR, VBR, MACH, MACL, SR, FPUL, FPSCR,

    FR0, FR1, FR2, FR3, FR4, FR5, FR6, FR7,
    FR8, FR9, FR10, FR11, FR12, FR13, FR14, FR15,

    SSR, SPC,

    R0B0, R1B0, R2B0, R3B0, R4B0, R5B0, R6B0, R7B0,
    R0B1, R1B1, R2B1, R3B1, R4B1, R5B1, R6B1, R7B1,

    N_REGS
};

void gdb_init(void);
void gdb_cleanup(void);

extern struct debug_frontend gdb_frontend;

#endif
