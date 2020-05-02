/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018-2020 snickerbockers
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

// for native_dispatch
static unsigned const cachep_reg = REG_NONVOL0;
static unsigned const tmp_reg_1 = REG_NONVOL1;
static unsigned const native_reg = REG_NONVOL2;
static unsigned const code_cache_tbl_ptr_reg = REG_NONVOL3;
static unsigned const code_hash_reg = REG_NONVOL4;

// for native_check_cycles
static unsigned const sched_tgt_reg = REG_NONVOL0;
static unsigned const countdown_reg = REG_NONVOL0;
static unsigned const new_pc_reg = NATIVE_DISPATCH_PC_REG;
static unsigned const hash_reg = NATIVE_DISPATCH_HASH_REG;
static unsigned const cycle_stamp_reg = NATIVE_DISPATCH_CYCLE_COUNT_REG;
static unsigned const ret_reg = REG_RET;

static void native_dispatch_emit(struct native_dispatch_meta const *meta);

static void load_quad_into_reg(void *qptr, unsigned reg_no);
static void store_quad_from_reg(void *qptr, unsigned reg_no,
                                unsigned clobber_reg);
static void create_return_fn(struct native_dispatch_meta *meta);

static void jmp_to_addr(void *addr, unsigned clobber_reg);

static void jmp_to_addr_jbe(void *addr, unsigned clobber_reg);

void native_dispatch_entry_create(struct native_dispatch_meta *meta);

#ifdef JIT_PROFILE
static void create_profile_code(struct native_dispatch_meta *meta);
#endif

static void
native_dispatch_create_slow_path_entry(struct native_dispatch_meta *meta);

static void
native_dispatch_trampoline_create(struct native_dispatch_meta *meta);

#ifdef ABI_MICROSOFT
static void native_dispatch_ms_shadow_open(void) {
    x86asm_addq_imm8_reg(-32, RSP);
}

static void native_dispatch_ms_shadow_close(void) {
    x86asm_addq_imm8_reg(32, RSP);
}
#endif

void native_dispatch_init(struct native_dispatch_meta *meta, void *ctx_ptr) {
    meta->ctx_ptr = ctx_ptr;

    meta->clock_vals =
        exec_mem_alloc(sizeof(meta->clock_vals[0]) * WASHDC_CLOCK_IDX_COUNT);

    clock_set_ptrs_priv(meta->clk, meta->clock_vals);

    native_dispatch_create_slow_path_entry(meta);
    create_return_fn(meta);
#ifdef JIT_PROFILE
    create_profile_code(meta);
#endif
    native_dispatch_entry_create(meta);

    native_dispatch_trampoline_create(meta);
}

void native_dispatch_cleanup(struct native_dispatch_meta *meta) {
    // TODO: free all executable memory pointers
    exec_mem_free(meta->entry);
    exec_mem_free(meta->return_fn);
#ifdef JIT_PROFILE
    exec_mem_free(meta->profile_code);
#endif
    meta->return_fn = NULL;

    clock_set_ptrs_priv(meta->clk, NULL);

    exec_mem_free(meta->clock_vals);

    meta->clock_vals = NULL;

    exec_mem_free(meta->trampoline);
    code_cache_set_default(NULL);
}

static void create_return_fn(struct native_dispatch_meta *meta) {
    meta->return_fn = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(meta->return_fn, NULL, BASIC_ALLOC);

    // return PC
    x86asm_mov_reg32_reg32(new_pc_reg, REG_RET);

    // store sched_tgt into cycle_stamp
    load_quad_into_reg(meta->clock_vals + WASHDC_CLOCK_IDX_TARGET,
                       sched_tgt_reg);
    store_quad_from_reg(meta->clock_vals + WASHDC_CLOCK_IDX_STAMP,
                        sched_tgt_reg, REG_VOL1);
    x86asm_xorl_reg32_reg32(sched_tgt_reg, sched_tgt_reg);
    store_quad_from_reg(meta->clock_vals + WASHDC_CLOCK_IDX_COUNTDOWN,
                        sched_tgt_reg, REG_VOL1);

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

    // restore non-volatile xmm registers
    // TODO: align stack pointer and use movaps instead of movups
    x86asm_movups_disp32_reg_xmm(16 * 9, RSP, XMM15);
    x86asm_movups_disp32_reg_xmm(16 * 8, RSP, XMM14);
    x86asm_movups_disp32_reg_xmm(16 * 7, RSP, XMM13);
    x86asm_movups_disp32_reg_xmm(16 * 6, RSP, XMM12);
    x86asm_movups_disp32_reg_xmm(16 * 5, RSP, XMM11);
    x86asm_movups_disp32_reg_xmm(16 * 4, RSP, XMM10);
    x86asm_movups_disp32_reg_xmm(16 * 3, RSP, XMM9);
    x86asm_movups_disp32_reg_xmm(16 * 2, RSP, XMM8);
    x86asm_movups_disp32_reg_xmm(16 * 1, RSP, XMM7);
    x86asm_movups_disp32_reg_xmm(16 * 0, RSP, XMM6);
    x86asm_addq_imm32_reg64(16 * (15 - 6 + 1), RSP);

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
}

#ifdef JIT_PROFILE
static void create_profile_code(struct native_dispatch_meta *meta) {
    struct x86asm_lbl8 skipit;
    x86asm_lbl8_init(&skipit);

    meta->profile_code = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(meta->profile_code, NULL, BASIC_ALLOC);

    size_t const jit_profile_offs = offsetof(struct cache_entry, blk.profile);
    x86asm_movq_disp8_reg_reg(jit_profile_offs, cachep_reg, REG_ARG1);

    /*
     * ignore NULL pointers.
     *
     * the trampoline block has a null pointer here because it's not a real
     * code block.
     */
    x86asm_testq_reg64_reg64(REG_ARG1, REG_ARG1);
    x86asm_jz_lbl8(&skipit);

    // call jit_profile_notify
    x86asm_mov_imm64_reg64((uintptr_t)meta->ctx_ptr, REG_ARG0);
    x86asm_movq_disp8_reg_reg(jit_profile_offs, cachep_reg, REG_ARG1);
    x86asm_mov_imm64_reg64((uintptr_t)(void*)meta->profile_notify, REG_RET);
    x86asm_jmpq_reg64(REG_RET); // tail-call elimination

    x86asm_lbl8_define(&skipit);
    x86asm_ret();

    x86asm_lbl8_cleanup(&skipit);
}
#endif

void native_dispatch_entry_create(struct native_dispatch_meta *meta) {
    void *entry = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(entry, NULL, BASIC_ALLOC);

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

    // save non-volatile xmm registers
    // TODO: align stack pointer and use movaps instead of movups
    x86asm_addq_imm32_reg64(-16 * (15 - 6 + 1), RSP);
    x86asm_movups_xmm_disp32_reg(XMM6,  16 * 0, RSP);
    x86asm_movups_xmm_disp32_reg(XMM7,  16 * 1, RSP);
    x86asm_movups_xmm_disp32_reg(XMM8,  16 * 2, RSP);
    x86asm_movups_xmm_disp32_reg(XMM9,  16 * 3, RSP);
    x86asm_movups_xmm_disp32_reg(XMM10, 16 * 4, RSP);
    x86asm_movups_xmm_disp32_reg(XMM11, 16 * 5, RSP);
    x86asm_movups_xmm_disp32_reg(XMM12, 16 * 6, RSP);
    x86asm_movups_xmm_disp32_reg(XMM13, 16 * 7, RSP);
    x86asm_movups_xmm_disp32_reg(XMM14, 16 * 8, RSP);
    x86asm_movups_xmm_disp32_reg(XMM15, 16 * 9, RSP);

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

    x86asm_mov_imm64_reg64((uintptr_t)(void*)code_cache_tbl,
                           code_cache_tbl_ptr_reg);

    /*
     * JIT code is only expected to preserve the base pointer, and to leave the
     * new value of the PC in RAX.  Other than that, it may do as it pleases.
     */
    native_dispatch_emit(meta);

    meta->entry = (native_dispatch_entry_func)entry;
}

static struct cache_entry *
dispatch_slow_path(uint32_t pc, struct native_dispatch_meta const *meta) {
    void *ctx_ptr = meta->ctx_ptr;
    struct cache_entry *entry = code_cache_find_slow(meta->hash_func(ctx_ptr, pc));

    code_cache_tbl[pc & CODE_CACHE_HASH_TBL_MASK] = entry;

    if (!entry->valid) {
        meta->on_compile(ctx_ptr, meta, &entry->blk, pc);
        entry->valid = 1;
    }

    return entry;
}

static void native_dispatch_emit(struct native_dispatch_meta const *meta) {
    struct x86asm_lbl8 code_cache_slow_path, have_valid_ent;

    /*
     * BEFORE CALLING THIS FUNCTION, REG_ARG0 MUST HOLD THE 32-BIT CODE HASH
     * VALUE.
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

    x86asm_lbl8_init(&code_cache_slow_path);
    x86asm_lbl8_init(&have_valid_ent);

    x86asm_mov_reg32_reg32(hash_reg, code_hash_reg);
    x86asm_andl_imm32_reg32(CODE_CACHE_HASH_TBL_MASK, code_hash_reg);

    x86asm_movq_sib_reg(code_cache_tbl_ptr_reg, 8, code_hash_reg, cachep_reg);

    // now check the address against the one that's still in hash_reg
    size_t const addr_offs = offsetof(struct cache_entry, node.key);
    if (addr_offs >= 256)
        RAISE_ERROR(ERROR_INTEGRITY); // this will never happen
    x86asm_movl_disp8_reg_reg(addr_offs, cachep_reg, tmp_reg_1);

    size_t const native_offs = offsetof(struct cache_entry, blk.x86_64.native);
    if (native_offs >= 256)
        RAISE_ERROR(ERROR_INTEGRITY); // this will never happen
    x86asm_movq_disp8_reg_reg(native_offs, cachep_reg, native_reg);

    x86asm_cmpl_reg32_reg32(tmp_reg_1, hash_reg);
    x86asm_jnz_lbl8(&code_cache_slow_path);// not equal

    x86asm_lbl8_define(&have_valid_ent);
    // cachep_reg points to a valid struct cache_entry which we want to jump to.

#ifdef JIT_PROFILE
    x86asm_pushq_reg64(native_reg);
    jmp_to_addr(meta->profile_code, REG_RET);
#else
    x86asm_jmpq_reg64(native_reg); // tail-call elimination
#endif

    // after this point no code is executed

    x86asm_lbl8_define(&code_cache_slow_path);

    x86asm_mov_imm64_reg64((uintptr_t)have_valid_ent.ptr, REG_RET);
    x86asm_pushq_reg64(REG_RET);
    jmp_to_addr(meta->dispatch_slow_path, REG_RET);

    x86asm_lbl8_cleanup(&have_valid_ent);
    x86asm_lbl8_cleanup(&code_cache_slow_path);
}

void native_check_cycles_emit(struct native_dispatch_meta const *meta) {
    static_assert(sizeof(dc_cycle_stamp_t) == 8,
                  "dc_cycle_stamp_t is not a quadword!");

    load_quad_into_reg(meta->clock_vals + WASHDC_CLOCK_IDX_COUNTDOWN,
                       countdown_reg);
    x86asm_subq_reg64_reg64(cycle_stamp_reg, countdown_reg);

    jmp_to_addr_jbe(meta->return_fn, REG_VOL0);

    store_quad_from_reg(meta->clock_vals + WASHDC_CLOCK_IDX_COUNTDOWN,
                        countdown_reg, REG_VOL1);

    // call native_dispatch
    native_dispatch_emit(meta);

    /*
     * the code created by native_dispatch_emit does not return, so execution
     * does not continue past this point.
     */
}

static void
native_dispatch_create_slow_path_entry(struct native_dispatch_meta *meta) {
    size_t const native_offs = offsetof(struct cache_entry, blk.x86_64.native);
    if (native_offs >= 256)
        RAISE_ERROR(ERROR_INTEGRITY); // this will never happen

    meta->dispatch_slow_path = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(meta->dispatch_slow_path, NULL, BASIC_ALLOC);

    // pc is still in REG_ARG0
    x86asm_mov_imm64_reg64((uintptr_t)(void*)dispatch_slow_path, REG_RET);
    x86asm_mov_imm64_reg64((uintptr_t)(void*)meta, REG_ARG1);

    // fix stack alignment in case the C code uses SSE instructions
    x86asm_addq_imm8_reg(-8, RSP);

#ifdef ABI_MICROSOFT
    native_dispatch_ms_shadow_open();
#endif
    x86asm_call_reg(REG_RET);
#ifdef ABI_MICROSOFT
    native_dispatch_ms_shadow_close();
#endif

    x86asm_addq_imm8_reg(8, RSP);

    x86asm_mov_reg64_reg64(REG_RET, cachep_reg);

    x86asm_movq_disp8_reg_reg(native_offs, cachep_reg, native_reg);

    x86asm_ret();
}

static void native_dispatch_trampoline_create(struct native_dispatch_meta *meta) {
    meta->trampoline = exec_mem_alloc(BASIC_ALLOC);
    x86asm_set_dst(meta->trampoline, NULL, BASIC_ALLOC);

    // PC should already be in REG_ARG0
    x86asm_mov_imm64_reg64((uintptr_t)meta->dispatch_slow_path, REG_RET);

    // fix stack alignment in case the C code uses SSE instructions
    x86asm_addq_imm8_reg(-8, RSP);
    x86asm_call_reg(REG_RET);
    x86asm_addq_imm8_reg(8, RSP);

#ifdef JIT_PROFILE
    x86asm_pushq_reg64(native_reg);
    jmp_to_addr(meta->profile_code, REG_RET);
#else
    x86asm_jmpq_reg64(native_reg); // tail-call elimination
#endif

    // after this point no code is executed

    /*
     * create the actual code block.  the key is arbitrary; we use 0xa0000000
     * so that it always gets tested and we know that it works.  This means
     * that PC=0xa0000000 is the only time the trampoline ever gets invoked
     * because every other hash value will be seen as a potentially-valid block
     * which isn't the one we're looking for.
     */
    memset(&meta->fake_cache_entry, 0, sizeof(meta->fake_cache_entry));
    meta->fake_cache_entry.valid = 1;
    meta->fake_cache_entry.blk.x86_64.native = meta->trampoline;
    meta->fake_cache_entry.node.key = 0xa0000000;

    code_cache_set_default(&meta->fake_cache_entry);
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

static void jmp_to_addr(void *addr, unsigned clobber_reg) {
    char *base = x86asm_get_outp() + 5;
    intptr_t diff = ((char*)addr) - base;
    if (diff <= INT32_MAX && diff >= INT32_MIN) {
        x86asm_jmpq_offs32(diff);
    } else {
        x86asm_mov_imm64_reg64((uintptr_t)addr, clobber_reg);
        x86asm_jmpq_reg64(clobber_reg);
    }
}

static void jmp_to_addr_jbe(void *addr, unsigned clobber_reg) {
    char *base = x86asm_get_outp() + 6;
    intptr_t diff = ((char*)addr) - base;
    if (diff <= INT32_MAX && diff >= INT32_MIN)
        x86asm_jbe_disp32(diff);
    else
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
}
