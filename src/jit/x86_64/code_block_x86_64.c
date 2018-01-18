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

#include <errno.h>
#include <stddef.h>
#include <sys/mman.h>

#include "hw/sh4/sh4.h"
#include "error.h"
#include "jit/code_block.h"
#include "code_block_x86_64.h"
#include "emit_x86_64.h"
#include "dreamcast.h"

/*
 * TODO: pick a smaller default allocation size and dynamically expand blocks
 * as needed during compilation.
 *
 * 32 kilobytes is probably larger than most blocks need to be, and in certain
 * situations it might not be large enough.  A "one-size-fits-all" allocation is
 * suboptimal in this case.
 */
#define X86_64_ALLOC_SIZE (32*1024)

void code_block_x86_64_init(struct code_block_x86_64 *blk) {
    void *native = mmap(NULL, X86_64_ALLOC_SIZE, PROT_WRITE | PROT_EXEC,
                        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    blk->cycle_count = 0;
    blk->bytes_used = 0;

    if (!native || native == MAP_FAILED) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    }

    blk->native = native;
}

void code_block_x86_64_cleanup(struct code_block_x86_64 *blk) {
    munmap(blk->native, X86_64_ALLOC_SIZE);
}

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

#define JMP_ADDR_REG RBX
#define ALT_JMP_ADDR_REG R12
#define COND_JMP_FLAG_REG R13

/*
 * after emitting this:
 * original %rsp is in %rbp
 * (%rbp) is original %rbp
 * original value of JMP_ADDR_REG is at (%rbp-4)
 * original value of ALT_JMP_ADDR_REG is at (%rbp-8)
 */
static void emit_stack_frame_open(void) {
    x86asm_pushq_reg64(RBP);
    x86asm_mov_reg64_reg64(RSP, RBP);
    x86asm_pushq_reg64(JMP_ADDR_REG);
    x86asm_pushq_reg64(ALT_JMP_ADDR_REG);
}

static void emit_stack_frame_close(void) {
    x86asm_popq_reg64(ALT_JMP_ADDR_REG);
    x86asm_popq_reg64(JMP_ADDR_REG);
    x86asm_mov_reg64_reg64(RBP, RSP);
    x86asm_popq_reg64(RBP);
}

// JIT_OP_FALLBACK implementation
void emit_fallback(Sh4 *sh4, struct jit_inst const *inst) {
    uint16_t inst_bin = inst->immed.fallback.inst.inst;

    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)sh4, RDI);
    x86asm_mov_imm16_reg(inst_bin, RSI);
    x86asm_call_ptr(inst->immed.fallback.fallback_fn);
}

// JIT_OP_PREPARE_JUMP implementation
void emit_prepare_jump(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned reg_idx = inst->immed.prepare_jump.reg_idx;
    unsigned offs = inst->immed.prepare_jump.offs;

    x86asm_mov_imm64_reg64((uintptr_t)(sh4->reg + reg_idx), R9);
    x86asm_mov_indreg32_reg32(R9, EAX);
    x86asm_add_imm32_eax(offs);
    x86asm_mov_reg32_reg32(EAX, JMP_ADDR_REG);
}

// JIT_OP_PREPARE_JUMP_CONST implementation
void emit_prepare_jump_const(Sh4 *sh4, struct jit_inst const *inst) {
    x86asm_mov_imm32_reg32(inst->immed.prepare_jump_const.new_pc, JMP_ADDR_REG);
}

// JIT_OP_PREPARE_ALT_JUMP implementation
void emit_prepare_alt_jump(Sh4 *sh4, struct jit_inst const *inst) {
    x86asm_mov_imm32_reg32(inst->immed.prepare_alt_jump.new_pc,
                           ALT_JMP_ADDR_REG);
}

// JIT_OP_JUMP implementation
void emit_jump(Sh4 *sh4, struct jit_inst const *inst) {
    void *ptr = sh4->reg + SH4_REG_PC;
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)ptr, RCX);
    x86asm_mov_reg32_indreg32(JMP_ADDR_REG, RCX);

    emit_stack_frame_close();
    x86asm_ret();
}

// JIT_SET_COND_JUMP_BASED_ON_T implementation
void emit_set_cond_jump_based_on_t(Sh4 *sh4, struct jit_inst const *inst) {
    // this il instruction will configure a jump if t_flag == the sh4's t flag
    unsigned t_flag = inst->immed.set_cond_jump_based_on_t.t_flag ? 1 : 0;

    // read the SR into RAX
    void *sr_ptr = sh4->reg + SH4_REG_SR;
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)sr_ptr, RAX);
    x86asm_mov_indreg32_reg32(RAX, EAX);

    /*
     * now compare that to t_flag.  We want to set COND_JMP_FLAG_REG if
     * %eax == t_flag, else clear it.
     */
    x86asm_xor_imm32_rax(t_flag);
    x86asm_not_reg64(RAX);
    x86asm_and_imm32_rax(1);

    // now store the final result
    x86asm_mov_reg64_reg64(RAX, COND_JMP_FLAG_REG);
}

/*
 * JIT_JUMP_COND implementation
 *
 * I don't have labels implemented in my emitter yet, so I actually implement
 * this il instruction in x86 by having it call a C-function to make the
 * decision.
 */
static uint32_t pick_cond_jump(unsigned cond_flag,
                               uint32_t jump_addr_true,
                               uint32_t jump_addr_false) {
    return cond_flag ? jump_addr_true : jump_addr_false;
}

void emit_jump_cond(Sh4 *sh4, struct jit_inst const *inst) {
    x86asm_mov_reg64_reg64(COND_JMP_FLAG_REG, RDI);
    x86asm_mov_reg64_reg64(JMP_ADDR_REG, RSI);
    x86asm_mov_reg64_reg64(ALT_JMP_ADDR_REG, RDX);

    x86asm_call_ptr(pick_cond_jump);

    // the chosen address is now in %eax.  Move it to sh4->reg[SH4_REG_PC].
    void *pc_ptr = sh4->reg + SH4_REG_PC;
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)pc_ptr, RDI);
    x86asm_mov_reg32_indreg32(EAX, RDI);

    emit_stack_frame_close();
    x86asm_ret();
}

// JIT_SET_REG implementation
void emit_set_reg(Sh4 *sh4, struct jit_inst const *inst) {
    void *reg_ptr = sh4->reg + inst->immed.set_reg.reg_idx;
    uint32_t new_val = inst->immed.set_reg.new_val;

    x86asm_mov_imm32_reg32(new_val, EAX);
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)reg_ptr, RCX);
    x86asm_mov_reg32_indreg32(EAX, RCX);
}

// JIT_OP_RESTORE_SR implementation
void emit_restore_sr(Sh4 *sh4, struct jit_inst const *inst) {
    void *sr_ptr = sh4->reg + SH4_REG_SR;
    void *ssr_ptr = sh4->reg + SH4_REG_SSR;

    // move old_sr into ESI for the function call
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)sr_ptr, RCX);
    x86asm_mov_indreg32_reg32(RCX, ESI);

    // update SR from SSR
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)ssr_ptr, RDX);
    x86asm_mov_indreg32_reg32(RDX, EDX);
    x86asm_mov_reg32_indreg32(EDX, RCX);

    // now call sh4_on_sr_change(cpu, old_sr)
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)sh4, RDI);
    x86asm_call_ptr(sh4_on_sr_change);
}

// JIT_OP_READ_16_REG implementation
void emit_read_16_reg(Sh4 *sh4, struct jit_inst const *inst) {
    uint32_t vaddr = inst->immed.read_16_reg.addr;
    void *reg_ptr = sh4->reg + inst->immed.read_16_reg.reg_no;

    // call sh4_read_mem_16(sh4, vaddr)
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)sh4, RDI);
    x86asm_mov_imm32_reg32(vaddr, ESI);
    x86asm_call_ptr(sh4_read_mem_16);

    // zero-extend the 16-bit value (maybe this isn't necessary?  IDK...)
    x86asm_and_imm32_rax(0x0000ffff);

    // move the return value into *reg_ptr
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)reg_ptr, RDI);
    x86asm_mov_reg32_indreg32(EAX, RDI);
}

// JIT_OP_SIGN_EXTEND_16 implementation
void emit_sign_extend_16(Sh4 *sh4, struct jit_inst const *inst) {
    // void x86asm_movsx_indreg16_reg32(unsigned reg_src, unsigned reg_dst);
    void *reg_ptr = sh4->reg + inst->immed.sign_extend_16.reg_no;
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)reg_ptr, RAX);
    x86asm_movsx_indreg16_reg32(RAX, ECX);
    x86asm_mov_reg32_indreg32(ECX, RAX);
}

// JIT_OP_READ_32_REG implementation
void emit_read_32_reg(Sh4 *sh4, struct jit_inst const *inst) {
    uint32_t vaddr = inst->immed.read_32_reg.addr;
    void *reg_ptr = sh4->reg + inst->immed.read_32_reg.reg_no;

    // call sh4_read_mem_32(sh4, vaddr)
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)sh4, RDI);
    x86asm_mov_imm32_reg32(vaddr, ESI);
    x86asm_call_ptr(sh4_read_mem_32);

    // move the return value into *reg_ptr
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)reg_ptr, RDI);
    x86asm_mov_reg32_indreg32(EAX, RDI);
}

void code_block_x86_64_compile(struct code_block_x86_64 *out,
                               struct il_code_block const *il_blk) {
    struct jit_inst const* inst = il_blk->inst_list;
    unsigned inst_count = il_blk->inst_count;
    Sh4 *sh4 = dreamcast_get_cpu();
    out->cycle_count = il_blk->cycle_count;

    x86asm_set_dst(out->native, X86_64_ALLOC_SIZE);

    emit_stack_frame_open();

    while (inst_count--) {
        switch (inst->op) {
        case JIT_OP_FALLBACK:
            emit_fallback(sh4, inst);
            break;
        case JIT_OP_PREPARE_JUMP:
            emit_prepare_jump(sh4, inst);
            break;
        case JIT_OP_PREPARE_JUMP_CONST:
            emit_prepare_jump_const(sh4, inst);
            break;
        case JIT_OP_PREPARE_ALT_JUMP:
            emit_prepare_alt_jump(sh4, inst);
            break;
        case JIT_OP_JUMP:
            emit_jump(sh4, inst);
            return;
        case JIT_SET_COND_JUMP_BASED_ON_T:
            emit_set_cond_jump_based_on_t(sh4, inst);
            break;
        case JIT_JUMP_COND:
            emit_jump_cond(sh4, inst);
            return;
        case JIT_SET_REG:
            emit_set_reg(sh4, inst);
            break;
        case JIT_OP_RESTORE_SR:
            emit_restore_sr(sh4, inst);
            break;
        case JIT_OP_READ_16_REG:
            emit_read_16_reg(sh4, inst);
            break;
        case JIT_OP_SIGN_EXTEND_16:
            emit_sign_extend_16(sh4, inst);
            break;
        case JIT_OP_READ_32_REG:
            emit_read_32_reg(sh4, inst);
            break;
        }
        inst++;
    }

    // all blocks should end by jumping out
    LOG_ERROR("ERROR: %u-len block does not jump out\n", il_blk->inst_count);
    RAISE_ERROR(ERROR_INTEGRITY);
}
