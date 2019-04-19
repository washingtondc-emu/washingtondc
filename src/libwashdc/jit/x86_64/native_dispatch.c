/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "washdc/error.h"
#include "dc_sched.h"
#include "exec_mem.h"
#include "emit_x86_64.h"
#include "jit/code_cache.h"
#include "jit/jit.h"
#include "abi.h"

#include "native_dispatch.h"

#define BASIC_ALLOC 32

static dc_cycle_stamp_t *sched_tgt;
static dc_cycle_stamp_t *cycle_stamp;
static struct dc_clock *native_dispatch_clk;

static void native_dispatch_emit(void *ctx_ptr,
                                 native_dispatch_compile_func compile_handler);

static void load_quad_into_reg(void *qptr, unsigned reg_no);
static void store_quad_from_reg(void *qptr, unsigned reg_no,
                                unsigned clobber_reg);

void native_dispatch_init(struct dc_clock *clk) {
    native_dispatch_clk = clk;
    sched_tgt = exec_mem_alloc(sizeof(*sched_tgt));
    cycle_stamp = exec_mem_alloc(sizeof(*cycle_stamp));

    clock_set_target_pointer(clk, sched_tgt);
    clock_set_cycle_stamp_pointer(clk, cycle_stamp);
}

void native_dispatch_cleanup(void) {
    // TODO: free all executable memory pointers
    clock_set_target_pointer(native_dispatch_clk, NULL);
    exec_mem_free(cycle_stamp);
    exec_mem_free(sched_tgt);
}

native_dispatch_entry_func
native_dispatch_entry_create(void *ctx_ptr,
                             native_dispatch_compile_func compile_handler) {
    void *entry = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(entry, BASIC_ALLOC);

#if defined(ABI_UNIX)
    x86asm_pushq_reg64(RBP);
    x86asm_mov_reg64_reg64(RSP, RBP);
    x86asm_pushq_reg64(RBX);
    x86asm_pushq_reg64(R12);
    x86asm_pushq_reg64(R13);
    x86asm_pushq_reg64(R14);
    x86asm_pushq_reg64(R15);
#elif defined(ABI_MICROSOFT)
    x86asm_pushq_reg64(RBP);
    x86asm_mov_reg64_reg64(RSP, RBP);
    x86asm_pushq_reg64(RBX);
    x86asm_pushq_reg64(RDI);
    x86asm_pushq_reg64(RSI);
    x86asm_pushq_reg64(R12);
    x86asm_pushq_reg64(R13);
    x86asm_pushq_reg64(R14);
    x86asm_pushq_reg64(R15);

    /*
     * The native-dispatch code uses its own calling convention which is mostly
     * identical to the UNIX convention, so we have to move arg0 into rdi
     */
    /* x86asm_mov_reg64_reg64(REG_ARG0, RDI); */
#else
#error unknown abi
#endif

    /*
     * When entry is called, the stack is 8 bytes after a
     * 16-byte boundary; this is mandated by the calling convention on x86_64
     * GCC/Linux.  After pushing 48 bytes above, the stack remains 8 bytes off
     * from a 16-byte boundary.  The code emitted by code_block_x86_64.c
     * expects to be perfectly aligned on a 16-byte boundary, and it will
     * restore RSP and RBP to their initial values whenever it calls
     * native_check_cycles.  This means that as long as entry,
     * native_dispatch and native_check_cycles don't push anything else onto the
     * stack that they don't pop off of the stack before jumping to a code_block
     * then it will always be safe to jump into a code_block withing checking
     * the stack alignment.
     */
    x86asm_addq_imm8_reg(-8, RSP);

    /*
     * JIT code is only expected to preserve the base pointer, and to leave the
     * new value of the PC in RAX.  Other than that, it may do as it pleases.
     */
    native_dispatch_emit(ctx_ptr, compile_handler);

    return entry;
}

static void native_dispatch_emit(void *ctx_ptr,
                                 native_dispatch_compile_func compile_handler) {
    struct x86asm_lbl8 check_valid_bit, code_cache_slow_path, have_valid_ent,
        compile;

    /*
     * BEFORE CALLING THIS FUNCTION, EDI MUST HOLD THE 32-BIT SH4 PC ADDRESS
     * THIS IS THE ONLY PARAMETER EXPECTED BY THIS FUNCTION.
     * THE CODE EMITTED BY THIS FUNCTION WILL NOT RETURN.
     *
     * REGISTER ALLOCATION:
     *    RBX points to the struct cache_entry
     *    EDI holds the 32-bit SH4 PC address
     *    ECX holds the index into the code_cache_tbl
     *
     *    All other registers are considered to be "temporary" registers whose
     *    values change often.
     */

    // 32-bit SH4 PC address
    static unsigned const pc_reg = REG_ARG0;
    static unsigned const cachep_reg = REG_NONVOL0;
    static unsigned const tmp_reg_1 = REG_NONVOL1;
    static unsigned const tmp_reg_2 = REG_NONVOL2;
    static unsigned const code_cache_tbl_ptr_reg = REG_NONVOL3;
    static unsigned const code_hash_reg = REG_NONVOL4;
    static unsigned const func_reg = REG_RET;
    static unsigned const ret_reg = REG_RET;

    x86asm_lbl8_init(&check_valid_bit);
    x86asm_lbl8_init(&code_cache_slow_path);
    x86asm_lbl8_init(&have_valid_ent);
    x86asm_lbl8_init(&compile);

    x86asm_mov_imm64_reg64((uintptr_t)(void*)code_cache_tbl, code_cache_tbl_ptr_reg);

    x86asm_mov_reg32_reg32(pc_reg, code_hash_reg);
    x86asm_andl_imm32_reg32(CODE_CACHE_HASH_TBL_MASK, code_hash_reg);

    x86asm_movq_sib_reg(code_cache_tbl_ptr_reg, 8, code_hash_reg, cachep_reg);

    // make sure the pointer isn't null; if so, jump to the slow-path
    x86asm_testq_reg64_reg64(cachep_reg, cachep_reg);
    x86asm_jz_lbl8(&code_cache_slow_path);

    // now check the address against the one that's still in pc_reg
    size_t const addr_offs = offsetof(struct cache_entry, node.key);
    if (addr_offs >= 256)
        RAISE_ERROR(ERROR_INTEGRITY); // this will never happen
    x86asm_movl_disp8_reg_reg(addr_offs, cachep_reg, tmp_reg_1);
    x86asm_cmpl_reg32_reg32(tmp_reg_1, pc_reg);
    x86asm_jnz_lbl8(&code_cache_slow_path);// not equal

    x86asm_lbl8_define(&check_valid_bit);
    // cachep_reg now points to the struct cache_entry
    size_t const valid_offs = offsetof(struct cache_entry, valid);
    x86asm_movb_disp8_reg_reg(valid_offs, cachep_reg, tmp_reg_1);// EMITTING WRONG INST
    x86asm_testl_imm32_reg32(1, tmp_reg_1);
    x86asm_jnz_lbl8(&have_valid_ent);

    x86asm_lbl8_define(&compile);

    /*
     * the PC should still be in pc_reg.
     * this is the last time we'll need it so there's no need to store it
     * anywhere
     */
    x86asm_mov_reg32_reg32(pc_reg, REG_ARG2);
    x86asm_mov_reg64_reg64(cachep_reg, REG_ARG1);
    x86asm_addq_imm8_reg(offsetof(struct cache_entry, blk.x86_64), REG_ARG1);
    x86asm_mov_imm64_reg64((uintptr_t)ctx_ptr, pc_reg);
    x86asm_mov_imm64_reg64((uintptr_t)(void*)compile_handler, func_reg);
    x86asm_addq_imm8_reg(-32, RSP);
    x86asm_call_reg(func_reg);
    x86asm_addq_imm8_reg(32, RSP);

    // now set the valid bit
    x86asm_xorl_reg32_reg32(ret_reg, ret_reg);
    x86asm_incl_reg32(ret_reg);
    x86asm_movb_reg_disp8_reg(ret_reg, valid_offs, cachep_reg);

    x86asm_lbl8_define(&have_valid_ent);
    // cachep_reg points to a valid struct cache_entry which we want to jump to.

    size_t const native_offs = offsetof(struct cache_entry, blk.x86_64.native);
    if (native_offs >= 256)
        RAISE_ERROR(ERROR_INTEGRITY); // this will never happen
    x86asm_movq_disp8_reg_reg(native_offs, cachep_reg, func_reg);

    // the native pointer now resides in RDX
    x86asm_jmpq_reg64(func_reg); // tail-call elimination
    // after this point no code is executed

    x86asm_lbl8_define(&code_cache_slow_path);

    // call code_cache_find_slow
    x86asm_mov_imm64_reg64((uintptr_t)(void*)&code_cache_find_slow, func_reg);
    x86asm_mov_reg32_reg32(pc_reg, tmp_reg_1);
    x86asm_mov_reg64_reg64(code_hash_reg, tmp_reg_2);
    x86asm_addq_imm8_reg(-32, RSP);
    x86asm_call_reg(func_reg);
    x86asm_addq_imm8_reg(32, RSP);
    x86asm_mov_reg32_reg32(tmp_reg_1, pc_reg);
    x86asm_mov_reg64_reg64(tmp_reg_2, code_hash_reg);
    x86asm_mov_reg64_reg64(ret_reg, cachep_reg);

    // now write the pointer into the table
    x86asm_movq_reg_sib(ret_reg, code_cache_tbl_ptr_reg, 8, code_hash_reg);

    // now jump up to the compile-point
    x86asm_jmp_lbl8(&check_valid_bit);

    x86asm_lbl8_cleanup(&compile);
    x86asm_lbl8_cleanup(&have_valid_ent);
    x86asm_lbl8_cleanup(&code_cache_slow_path);
    x86asm_lbl8_cleanup(&check_valid_bit);
}

void native_check_cycles_emit(void *ctx_ptr,
                              native_dispatch_compile_func compile_handler) {
    struct x86asm_lbl8 dont_return;
    x86asm_lbl8_init(&dont_return);

    static_assert(sizeof(dc_cycle_stamp_t) == 8,
                  "dc_cycle_stamp_t is not a quadword!");

    static unsigned const sched_tgt_reg = REG_NONVOL0;
    static unsigned const cycle_count_reg = REG_ARG0;
    static unsigned const jump_reg = REG_ARG1;
    static unsigned const cycle_stamp_reg = REG_RET;
    static unsigned const ret_reg = REG_RET;

    load_quad_into_reg(sched_tgt, sched_tgt_reg);
    load_quad_into_reg(cycle_stamp, cycle_stamp_reg);
    x86asm_addq_reg64_reg64(cycle_stamp_reg, cycle_count_reg);
    x86asm_cmpq_reg64_reg64(sched_tgt_reg, cycle_count_reg);
    x86asm_jb_lbl8(&dont_return);

    // return PC
    x86asm_mov_reg32_reg32(jump_reg, ret_reg);

    // store sched_tgt into cycle_stamp
    store_quad_from_reg(cycle_stamp, sched_tgt_reg, REG_VOL1);

    // close the stack frame
    x86asm_addq_imm8_reg(8, RSP);

#if defined(ABI_UNIX)
    x86asm_popq_reg64(R15);
    x86asm_popq_reg64(R14);
    x86asm_popq_reg64(R13);
    x86asm_popq_reg64(R12);
    x86asm_popq_reg64(RBX);
    x86asm_popq_reg64(RBP);
#elif defined(ABI_MICROSOFT)
    x86asm_popq_reg64(R15);
    x86asm_popq_reg64(R14);
    x86asm_popq_reg64(R13);
    x86asm_popq_reg64(R12);
    x86asm_popq_reg64(RSI);
    x86asm_popq_reg64(RDI);
    x86asm_popq_reg64(RBX);
    x86asm_popq_reg64(RBP);
#else
#error unknown abi
#endif

    x86asm_ret();

    // continue
    x86asm_lbl8_define(&dont_return);

    store_quad_from_reg(cycle_stamp, cycle_count_reg, REG_VOL1);

    // call native_dispatch
    x86asm_mov_reg32_reg32(jump_reg, REG_ARG0);
    native_dispatch_emit(ctx_ptr, compile_handler);

    x86asm_lbl8_cleanup(&dont_return);
}

static void load_quad_into_reg(void *qptr, unsigned reg_no) {
    intptr_t qaddr = (uintptr_t)qptr;
    intptr_t rip = (uintptr_t)x86asm_get_outp() + 7;

    intptr_t disp = qaddr - rip;
    if (disp >= INT32_MIN && disp <= INT32_MAX) {
        x86asm_movq_riprel_reg(disp, reg_no);
    } else {
        x86asm_mov_imm64_reg64(qaddr, reg_no);
        x86asm_movq_indreg_reg(reg_no, reg_no);
    }
}

static void store_quad_from_reg(void *qptr, unsigned reg_no,
                                unsigned clobber_reg) {
    intptr_t qaddr = (uintptr_t)qptr;
    intptr_t rip = (uintptr_t)x86asm_get_outp() + 7;

    intptr_t disp = qaddr - rip;
    if (disp >= INT32_MIN && disp <= INT32_MAX) {
        x86asm_movq_reg_riprel(reg_no, disp);
    } else {
        x86asm_mov_imm64_reg64(qaddr, clobber_reg);
        x86asm_movq_reg64_indreg64(reg_no, clobber_reg);
    }
}
