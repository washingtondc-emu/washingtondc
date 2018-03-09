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

#include <stddef.h>
#include <stdint.h>

#include "error.h"
#include "dreamcast.h"
#include "dc_sched.h"
#include "exec_mem.h"
#include "emit_x86_64.h"
#include "jit/code_cache.h"
#include "jit/jit.h"

#include "native_dispatch.h"

#define BASIC_ALLOC 32

static dc_cycle_stamp_t *sched_tgt;

uint32_t (*native_dispatch_entry)(uint32_t pc);
void *native_dispatch;
void *native_check_cycles;

static void native_dispatch_entry_create(void);
static void native_dispatch_create(void);
static void native_check_cycles_create(void);

void native_dispatch_init(void) {
    sched_tgt = exec_mem_alloc(sizeof(sched_tgt));
    sched_set_target_pointer(sched_tgt);

    native_dispatch_create();
    native_dispatch_entry_create();
    native_check_cycles_create();
}

void native_dispatch_cleanup(void) {
    sched_set_target_pointer(NULL);

    // TODO: free all executable memory pointers
    exec_mem_free(sched_tgt);
}

static void native_dispatch_entry_create(void) {
    native_dispatch_entry = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(native_dispatch_entry, BASIC_ALLOC);

    /*
     * When native_dispatch_entry is called, the stack is 8 bytes after a
     * 16-byte boundary; this is mandated by the calling convention on x86_64
     * GCC/Linux.  Here we open a stack frame by pushing 40 bytes onto the
     * stack.  This puts the stack on a 16-byte boundary.  The code emitted by
     * code_block_x86_64.c expects to be perfectly aligned on a 16-byte
     * boundary, and it will restore RSP and RBP to their initial values
     * whenever it calls native_check_cycles.  This means that as long as
     * native_dispatch_entry, native_dispatch and native_check_cycles don't
     * push anything onto the stack that they don't pop off of the stack before
     * jumping to a code_block (other than this stack-frame opener) then it
     * will always be safe to jump into a code_block withing checking the stack
     * alignment.
     */
    x86asm_pushq_reg64(RBP);
    x86asm_mov_reg64_reg64(RSP, RBP);
    x86asm_pushq_reg64(RBX);
    x86asm_pushq_reg64(R12);
    x86asm_pushq_reg64(R13);
    x86asm_pushq_reg64(R14);
    x86asm_pushq_reg64(R15);

    /*
     * JIT code is only expected to preserve the base pointer, and to leave the
     * new value of the PC in RAX.  Other than that, it may do as it pleases.
     */
    x86asm_mov_imm64_reg64((uintptr_t)(void*)native_dispatch, RAX);
    x86asm_jmpq_reg64(RAX);
}

static struct code_block_x86_64 *get_block(uint32_t pc) {
    struct cache_entry *ent = code_cache_find(pc);
    if (!ent->valid) {
        jit_compile_native(&ent->blk.x86_64, pc);
        ent->valid = true;
    }
    return &ent->blk.x86_64;
}

static void native_dispatch_create(void) {
    native_dispatch = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(native_dispatch, BASIC_ALLOC);

    // the PC should still be in EDI.
    x86asm_mov_imm64_reg64((uintptr_t)(void*)get_block, RAX);
    x86asm_call_reg(RAX);

    // TODO: maybe always assume native_offs is 0 ?
    size_t native_offs = offsetof(struct code_block_x86_64, native);
    if (native_offs >= 256)
        RAISE_ERROR(ERROR_INTEGRITY); // this will never happen
    x86asm_movq_disp8_reg_reg(native_offs, RAX, RDX);

    // the native pointer now resides in RDX
    x86asm_jmpq_reg64(RDX); // tail-call elimination
}

// returns 1 if the jit should return
static int check_cycles(uint32_t n_cycles) {
    dc_cycle_stamp_t new_stamp = dc_cycle_stamp() + n_cycles;
    if (new_stamp >= *sched_tgt) {
        dc_cycle_stamp_set(*sched_tgt);
        return 1;
    } else {
        dc_cycle_stamp_set(new_stamp);
        return 0;
    }
}

static void native_check_cycles_create(void) {
    native_check_cycles = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(native_check_cycles, BASIC_ALLOC);

    struct x86asm_lbl8 dont_return;
    x86asm_lbl8_init(&dont_return);

    // save argument
    x86asm_mov_reg32_reg32(ESI, EBX);

    // the cycle-count should still be in EDI.
    x86asm_mov_imm64_reg64((uintptr_t)(void*)check_cycles, RAX);
    x86asm_call_reg(RAX);

    x86asm_testl_reg32_reg32(EAX, EAX);
    x86asm_jz_lbl8(&dont_return);
    x86asm_mov_reg32_reg32(EBX, EAX);

    x86asm_movq_disp8_reg_reg(-8, RBP, RBX);
    x86asm_movq_disp8_reg_reg(-16, RBP, R12);
    x86asm_movq_disp8_reg_reg(-24, RBP, R13);
    x86asm_movq_disp8_reg_reg(-32, RBP, R14);
    x86asm_movq_disp8_reg_reg(-40, RBP, R15);
    x86asm_mov_reg64_reg64(RBP, RSP);
    x86asm_popq_reg64(RBP);
    x86asm_ret();

    x86asm_lbl8_define(&dont_return);

    // call native_dispatch
    x86asm_mov_reg32_reg32(EBX, EDI);
    x86asm_mov_imm64_reg64((uintptr_t)native_dispatch, RAX);
    x86asm_jmpq_reg64(RAX);

    x86asm_lbl8_cleanup(&dont_return);
}
