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

#ifndef ENABLE_JIT_X86_64
#error this file should not be built when the x86_64 JIT backend is disabled
#endif

#include <errno.h>
#include <stddef.h>

#include "hw/sh4/sh4.h"
#include "error.h"
#include "jit/code_block.h"
#include "exec_mem.h"
#include "emit_x86_64.h"
#include "dreamcast.h"

#include "code_block_x86_64.h"

#define N_REGS 16

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

static struct reg_stat {
    // if true this reg can never ever be allocated under any circumstance.
    
    bool const locked;

    /*
     * Decide how likely the allocator is to pick this register.
     * higher numbers are higher priority.
     */
    int const prio;

    // if this is false, nothing is in this register and it is free at any time
    bool in_use;

    /*
     * if this is true, the register is currently in use right now, and no
     * other slots should be allowed in here.  native il implementations should
     * grab any registers they are using, then use those registers then ungrab
     * them.
     *
     * When a register is not grabbed, the value contained within it is still
     * valid.  Being grabbed only prevents the register from going away.
     */
    bool grabbed;

    unsigned slot_no;
} regs[N_REGS] = {
    [RAX] = {
        .locked = false,
        .prio = 0
    },
    [RCX] = {
        .locked = false,
        .prio = 3
    },
    [RDX] = {
        // RDX is a lower priority because mul will clobber it
        .locked = false,
        .prio = 1
    },
    [RBX] = {
        .locked = false,
        .prio = 6
    },
    [RSP] = {
        // stack pointer
        .locked = true
    },
    [RBP] = {
        // base pointer
        .locked = true
    },
    [RSI] = {
        .locked = false,
        .prio = 3
    },
    [RDI] = {
        .locked = false,
        .prio = 3
    },
    [R8] = {
        .locked = false,
        .prio = 2
    },
    [R9] = {
        .locked = false,
        .prio = 2
    },
    [R10] = {
        .locked = false,
        .prio = 2
    },
    [R11] = {
        .locked = false,
        .prio = 2
    },
    /*
     * R12 and R13 have a lower priority than R14 and R15 because they require
     * extra displacement or SIB bytes to go after the mod/reg/rm due to the
     * way that they overlap with RSP and RBP.
     */
    [R12] = {
        .locked = false,
        .prio = 4
    },
    [R13] = {
        .locked = false,
        .prio = 4
    },
    [R14] = {
        .locked = false,
        .prio = 5
    },
    [R15] = {
        .locked = false,
        .prio = 5
    }
};

#define MAX_SLOTS 512

struct slot {
    union {
        // offset from rbp (if this slot resides on the stack)
        int rbp_offs;

        // x86 register index (if this slot resides in a native host register)
        unsigned reg_no;
    };

    // if false, the slot is not in use and all other fields are invalid
    bool in_use;

    // if true, reg_no is valid and the slot resides in an x86 register
    // if false, rbp_offs is valid and the slot resides on the call-stack
    bool in_reg;
} slots[MAX_SLOTS];

/*
 * offset of the next push onto the stack.
 *
 * This value is always negative (or zero) because the stack grows downwards.
 *
 * This value only ever increases towards zero when a discarded or popped slot
 * has an rbp_offs of (base_ptr_offs_next + 8).  Otherwise, the space formerly 
 *occupied by that slot ends up getting wasted until the end of the frame.
 */
static int rsp_offs; // offset from base pointer to stack pointer

static void grab_register(unsigned reg_no);
static void ungrab_register(unsigned reg_no);

static void evict_register(unsigned reg_no);

/*
 * if the stack is not 16-byte aligned, make it 16-byte aligned.
 * This way, when the CALL instruction is issued the stack will be off from
 * 16-byte alignment by 8 bytes; this is what GCC's calling convention requires.
 */
static void align_stack(void);

static void reset_slots(void) {
    memset(slots, 0, sizeof(slots));
    unsigned reg_no;
    for (reg_no = 0; reg_no < N_REGS; reg_no++) {
        regs[reg_no].in_use = false;
        regs[reg_no].grabbed = false;
        regs[reg_no].slot_no = 0xdeadbeef;
    }

    rsp_offs = 0;
}

/*
 * mark a given slot (as well as the register it resides in, if any) as no
 * longer being in use.
 */
static void discard_slot(unsigned slot_no) {
    if (slot_no >= MAX_SLOTS)
        RAISE_ERROR(ERROR_TOO_BIG);
    struct slot *slot = slots + slot_no;
    if (!slot->in_use)
        RAISE_ERROR(ERROR_INTEGRITY);
    slot->in_use = false;
    if (slot->in_reg) {
        regs[slot->reg_no].in_use = false;
    } else {
        if (rsp_offs == slot->rbp_offs) {
            // TODO: add 8 to RSP and base_ptr_offs_next
        }
    }
}

/*
 * whenever you emit a function call, call this function first.
 * This function will grab all volatile registers and emit code to make sure
 * they all get saved.
 */
static void prefunc(void) {
    grab_register(RAX);
    grab_register(RCX);
    grab_register(RDX);
    grab_register(RSI);
    grab_register(RDI);
    grab_register(R8);
    grab_register(R9);
    grab_register(R10);
    grab_register(R11);

    evict_register(RAX);
    evict_register(RCX);
    evict_register(RDX);
    evict_register(RSI);
    evict_register(RDI);
    evict_register(R8);
    evict_register(R9);
    evict_register(R10);
    evict_register(R11);
}

/*
 * whenever you emit a function call, call this function after
 * really all it does is ungrab all the registers earlier grabbed by prefunc.
 *
 * It does not ungrab RAX even though that register is grabbed by prefunc.  The
 * reason for this is that RAX holds the return value (if any) and you probably
 * want to do something with that.  Functions that call postfunc will also need
 * to call ungrab_register(RAX) afterwards when they no longer need that
 * register.
 */
static void postfunc(void) {
    ungrab_register(R11);
    ungrab_register(R10);
    ungrab_register(R9);
    ungrab_register(R8);
    ungrab_register(RDI);
    ungrab_register(RSI);
    ungrab_register(RDX);
    ungrab_register(RCX);
}

/*
 * move the given slot from a register to the stack.  As a precondition, the
 * slot must be in a register and the register it is in must not be grabbed.
 */
static void move_slot_to_stack(unsigned slot_no) {
    if (slot_no >= MAX_SLOTS)
        RAISE_ERROR(ERROR_TOO_BIG);
    struct slot *slot = slots + slot_no;
    if (!slot->in_use || !slot->in_reg)
        RAISE_ERROR(ERROR_INTEGRITY);
    struct reg_stat *reg = regs + slot->reg_no;
    if (!reg->in_use || reg->slot_no != slot_no ||
        reg->locked /* || reg->grabbed */)
        RAISE_ERROR(ERROR_INTEGRITY);

    x86asm_pushq_reg64(slot->reg_no);

    slot->in_reg = false;
    rsp_offs -= 8;
    slot->rbp_offs = rsp_offs;
    reg->in_use = false;
}

/*
 * move the given slot into the given register.
 *
 * this function assumes that the register has already been allocated.
 * it will safely move any slots already in the register to the stack.
 */
static void move_slot_to_reg(unsigned slot_no, unsigned reg_no) {
    if (slot_no >= MAX_SLOTS)
        RAISE_ERROR(ERROR_TOO_BIG);
    struct slot *slot = slots + slot_no;
    if (!slot->in_use)
        RAISE_ERROR(ERROR_INTEGRITY);

    if (slot->in_reg) {
        unsigned src_reg = slot->reg_no;

        if (src_reg == reg_no)
            return; // nothing to do here

        struct reg_stat *reg_dst = regs + reg_no;
        if (reg_dst->in_use)
            move_slot_to_stack(reg_dst->slot_no);

        struct reg_stat *reg_src = regs + src_reg;

        x86asm_mov_reg32_reg32(src_reg, reg_no);

        reg_src->in_use = false;
        reg_dst->in_use = true;
        reg_dst->slot_no = slot_no;
        slots[slot_no].reg_no = reg_no;
        return;
    }

    struct reg_stat *reg_dst = regs + reg_no;
    if (reg_dst->in_use)
        move_slot_to_stack(reg_dst->slot_no);

    /*
     * Don't allow writes to anywhere >= %rbp-40 because that is where the
     * saved variables are stored on the stack (see emit_frame_open).
     */
    if (slot->rbp_offs >= -40)
        RAISE_ERROR(ERROR_INTEGRITY);

    /* if (slot->rbp_offs == rsp_offs) { */
    /*     rsp_offs += 8; */
    /*     x86asm_popq_reg64(reg_no); */
    /* } else  */{
        // move the slot from the stack to the reg based on offset from rbp.
        if (slot->rbp_offs > 127 || slot->rbp_offs < -128)
            x86asm_movq_disp32_reg_reg(slot->rbp_offs, RBP, reg_no);
        else
            x86asm_movq_disp8_reg_reg(slot->rbp_offs, RBP, reg_no);
    }

    reg_dst->in_use = true;
    reg_dst->slot_no = slot_no;
    slot->reg_no = reg_no;
    slot->in_reg = true;
}

/*
 * This function will pick an unused register to use.  This doesn't change the
 * state of the register.  If there are no unused registers available, this
 * function will return -1.
 */
static int pick_unused_reg(void) {
    unsigned reg_no;
    unsigned best_reg = 0;
    int best_prio;
    bool found_one = false;

    for (reg_no = 0; reg_no < N_REGS; reg_no++) {
        struct reg_stat const *reg = regs + reg_no;
        if (!reg->locked && !reg->grabbed && !reg->in_use) {
            if (!found_one || reg->prio > best_prio) {
                found_one = true;
                best_prio = reg->prio;
                best_reg = reg_no;
            }
        }
    }

    if (found_one)
        return best_reg;
    return -1;
}

/*
 * The allocator calls this to find a register it can use.  This doesn't change
 * the state of the register or do anything to save the value in that register.
 * All it does is find a register which is not locked and not grabbed.
 */
static unsigned pick_reg(void) {
    unsigned reg_no;
    unsigned best_reg = 0;
    int best_prio;
    bool found_one = false;

    // first pass: try to find one that's not in use
    int unused_reg = pick_unused_reg();
    if (unused_reg >= 0)
        return (unsigned)unused_reg;

    /*
     * second pass: they're all in use so just pick one that is not locked or
     * grabbed.
     */
    for (reg_no = 0; reg_no < N_REGS; reg_no++) {
        struct reg_stat const *reg = regs + reg_no;
        if (!reg->locked && !reg->grabbed) {
            if (!found_one || reg->prio > best_prio) {
                found_one = true;
                best_prio = reg->prio;
                best_reg = reg_no;
            }
        }
    }

    if (found_one)
        return best_reg;

    LOG_ERROR("x86_64: no more registers!\n");
    RAISE_ERROR(ERROR_INTEGRITY);
}

/*
 * call this function to send the given register's contents (if any) to the
 * stack.  You should first grab the register to prevent it from being
 * allocated, and subsequently ungrab it when finished.  The register's contents
 * are unchanged by this function.
 */
static void evict_register(unsigned reg_no) {
    struct reg_stat *reg = regs + reg_no;
    if (reg->in_use) {
        int reg_dst = pick_unused_reg();

        if (reg_dst >= 0)
            move_slot_to_reg(reg->slot_no, reg_dst);
        else
            move_slot_to_stack(reg->slot_no);
    }
    reg->in_use = false;
}

/*
 * If the slot is in a register, then mark that register as grabbed.
 *
 * If the slot is not in use, then find a register, move that register's slot
 * to the stack (if there's something already in it), and mark that register as
 * grabbed.  The value in the register is undefined.
 *
 * If the slot is on the stack, then find a register, move that register's slot
 * to the stack (if there's something already in it), move this slot to that
 * register, and mark that register as grabbed.
 */
static void grab_slot(unsigned slot_no) {
    if (slot_no >= MAX_SLOTS)
        RAISE_ERROR(ERROR_TOO_BIG);
    struct slot *slot = slots + slot_no;

    if (slot->in_use) {
        if (slot->in_reg)
            goto mark_grabbed;

        unsigned reg_no = pick_reg();
        move_slot_to_reg(slot_no, reg_no);
        goto mark_grabbed;
    } else {
        unsigned reg_no = pick_reg();
        struct reg_stat *reg = regs + reg_no;
        if (reg->in_use)
            move_slot_to_stack(reg->slot_no);
        reg->in_use = true;
        reg->slot_no = slot_no;
        slot->in_use = true;
        slot->reg_no = reg_no;
        slot->in_reg = true;
        goto mark_grabbed;
    }

mark_grabbed:
    grab_register(slot->reg_no);
}

static void ungrab_slot(unsigned slot_no) {
    if (slot_no >= MAX_SLOTS)
        RAISE_ERROR(ERROR_TOO_BIG);
    struct slot *slot = slots + slot_no;
    if (slot->in_reg)
        ungrab_register(slot->reg_no);
    else
        RAISE_ERROR(ERROR_INTEGRITY);
}

/*
 * unlike grab_slot, this does not preserve the slot that is currently in the
 * register.  To do that, call evict_register first.
 */
static void grab_register(unsigned reg_no) {
    if (regs[reg_no].grabbed)
        RAISE_ERROR(ERROR_INTEGRITY);
    regs[reg_no].grabbed = true;
}

static void ungrab_register(unsigned reg_no) {
    if (!regs[reg_no].grabbed)
        RAISE_ERROR(ERROR_INTEGRITY);
    regs[reg_no].grabbed = false;
}

#define X86_64_ALLOC_SIZE 32

void code_block_x86_64_init(struct code_block_x86_64 *blk) {
    void *native = exec_mem_alloc(X86_64_ALLOC_SIZE);
    blk->cycle_count = 0;
    blk->bytes_used = 0;

    if (!native) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    }

    blk->native = native;
}

void code_block_x86_64_cleanup(struct code_block_x86_64 *blk) {
    exec_mem_free(blk->native);
    memset(blk, 0, sizeof(*blk));
}

/*
 * after emitting this:
 * original %rsp is in %rbp
 * (%rbp) is original %rbp
 */
static void emit_stack_frame_open(void) {
    x86asm_pushq_reg64(RBP);
    x86asm_mov_reg64_reg64(RSP, RBP);
    x86asm_pushq_reg64(RBX);
    x86asm_pushq_reg64(R12);
    x86asm_pushq_reg64(R13);
    x86asm_pushq_reg64(R14);
    x86asm_pushq_reg64(R15);

    rsp_offs = -40;
}

static void emit_stack_frame_close(void) {
    x86asm_movq_disp8_reg_reg(-8, RBP, RBX);
    x86asm_movq_disp8_reg_reg(-16, RBP, R12);
    x86asm_movq_disp8_reg_reg(-24, RBP, R13);
    x86asm_movq_disp8_reg_reg(-32, RBP, R14);
    x86asm_movq_disp8_reg_reg(-40, RBP, R15);
    x86asm_mov_reg64_reg64(RBP, RSP);
    x86asm_popq_reg64(RBP);
}

// JIT_OP_FALLBACK implementation
void emit_fallback(Sh4 *sh4, struct jit_inst const *inst) {
    uint16_t inst_bin = inst->immed.fallback.inst.inst;

    prefunc();

    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)sh4, RDI);
    x86asm_mov_imm16_reg(inst_bin, RSI);
    align_stack();
    x86asm_call_ptr(inst->immed.fallback.fallback_fn);

    postfunc();
    ungrab_register(RAX);
}

// JIT_OP_JUMP implementation
void emit_jump(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.jump.slot_no;

    grab_register(RAX);
    evict_register(RAX);

    grab_slot(slot_no);

    x86asm_mov_reg32_reg32(slots[slot_no].reg_no, EAX);
    emit_stack_frame_close();
    x86asm_ret();

    ungrab_slot(slot_no);
}

// JIT_JUMP_COND implementation
void emit_jump_cond(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned t_flag = inst->immed.jump_cond.t_flag ? 1 : 0;
    unsigned flag_slot = inst->immed.jump_cond.slot_no;
    unsigned jmp_addr_slot = inst->immed.jump_cond.jmp_addr_slot;
    unsigned alt_jmp_addr_slot = inst->immed.jump_cond.alt_jmp_addr_slot;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_register(RAX);
    evict_register(RAX);
    grab_register(RCX);
    evict_register(RCX);

    grab_slot(flag_slot);
    x86asm_mov_reg32_reg32(slots[flag_slot].reg_no, EAX);
    x86asm_and_imm32_rax(1);
    x86asm_mov_reg32_reg32(EAX, ECX);
    ungrab_slot(flag_slot);

    grab_slot(jmp_addr_slot);
    grab_slot(alt_jmp_addr_slot);

    /*
     * move the alt-jmp addr into the return register, then replace that with
     * the normal jmp addr if the flag is set.
     *
     * TODO: the gcc output for the (now defunct) pick_cond_jump function is as
     * follows:
     * testl %edi, %edi
     * movl %edx, %eax
     * cmovne %esi, %eax
     *
     * Based on this, it would seem that gcc thinks a conditional-move is
     * faster than a conditional jump-forward.  I should experiment and
     * benchmark...
     */
    x86asm_mov_reg64_reg64(slots[alt_jmp_addr_slot].reg_no, RAX);
    x86asm_cmpl_reg32_imm8(ECX, !t_flag);
    x86asm_jz_lbl8(&lbl);    // JUMP IF EQUAL
    x86asm_mov_reg64_reg64(slots[jmp_addr_slot].reg_no, RAX);
    x86asm_lbl8_define(&lbl);

    // the chosen address is now in %rax, so we're ready to return

    emit_stack_frame_close();
    x86asm_ret();

    ungrab_slot(alt_jmp_addr_slot);
    ungrab_slot(jmp_addr_slot);

    ungrab_register(RCX);
    ungrab_register(RAX); // not that it matters at this point...

    x86asm_lbl8_cleanup(&lbl);
}

// JIT_SET_REG implementation
void emit_set_slot(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_idx = inst->immed.set_slot.slot_idx;
    uint32_t new_val = inst->immed.set_slot.new_val;

    grab_slot(slot_idx);
    x86asm_mov_imm32_reg32(new_val, slots[slot_idx].reg_no);
    ungrab_slot(slot_idx);
}

// JIT_OP_RESTORE_SR implementation
void emit_restore_sr(Sh4 *sh4, struct jit_inst const *inst) {
    void *sr_ptr = sh4->reg + SH4_REG_SR;
    void *ssr_ptr = sh4->reg + SH4_REG_SSR;

    prefunc();

    // move old_sr into ESI for the function call
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)sr_ptr, RCX);
    x86asm_mov_indreg32_reg32(RCX, ESI);

    // update SR from SSR
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)ssr_ptr, RDX);
    x86asm_mov_indreg32_reg32(RDX, EDX);
    x86asm_mov_reg32_indreg32(EDX, RCX);

    // now call sh4_on_sr_change(cpu, old_sr)
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)sh4, RDI);
    align_stack();
    x86asm_call_ptr(sh4_on_sr_change);

    postfunc();
    ungrab_register(RAX);
}

// JIT_OP_READ_16_CONSTADDR implementation
void emit_read_16_constaddr(Sh4 *sh4, struct jit_inst const *inst) {
    addr32_t vaddr = inst->immed.read_16_constaddr.addr;
    unsigned slot_no = inst->immed.read_16_constaddr.slot_no;

    // call sh4_read_mem_16(sh4, vaddr)
    prefunc();

    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)sh4, RDI);
    x86asm_mov_imm32_reg32(vaddr, ESI);
    align_stack();
    x86asm_call_ptr(sh4_read_mem_16);
    x86asm_and_imm32_rax(0x0000ffff);

    postfunc();

    grab_slot(slot_no);
    x86asm_mov_reg32_reg32(EAX, slots[slot_no].reg_no);

    ungrab_register(RAX);
    ungrab_slot(slot_no);
}

// JIT_OP_SIGN_EXTEND_16 implementation
void emit_sign_extend_16(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.sign_extend_16.slot_no;

    grab_slot(slot_no);

    unsigned reg_no = slots[slot_no].reg_no;
    x86asm_movsx_reg16_reg32(reg_no, reg_no);

    ungrab_slot(slot_no);
}

// JIT_OP_READ_32_CONSTADDR implementation
void emit_read_32_constaddr(Sh4 *sh4, struct jit_inst const *inst) {
    addr32_t vaddr = inst->immed.read_32_constaddr.addr;
    unsigned slot_no = inst->immed.read_32_constaddr.slot_no;

    // call sh4_read_mem_32(sh4, vaddr)

    prefunc();

    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)sh4, RDI);
    x86asm_mov_imm32_reg32(vaddr, ESI);
    align_stack();
    x86asm_call_ptr(sh4_read_mem_32);

    postfunc();

    grab_slot(slot_no);
    x86asm_mov_reg32_reg32(EAX, slots[slot_no].reg_no);

    ungrab_slot(slot_no);
    ungrab_register(RAX);
}

// JIT_OP_READ_32_SLOT implementation
void emit_read_32_slot(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned dst_slot = inst->immed.read_32_slot.dst_slot;
    unsigned addr_slot = inst->immed.read_32_slot.addr_slot;

    // call sh4_read_mem_32(sh4, *addr_slot)
    prefunc();

    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)sh4, RDI);
    move_slot_to_reg(addr_slot, ESI);
    evict_register(ESI);

    align_stack();
    x86asm_call_ptr(sh4_read_mem_32);

    postfunc();

    grab_slot(dst_slot);
    x86asm_mov_reg32_reg32(EAX, slots[dst_slot].reg_no);

    ungrab_slot(dst_slot);
    ungrab_register(RAX);
}

// JIT_OP_WRITE_32_SLOT implementation
void emit_write_32_slot(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned src_slot = inst->immed.write_32_slot.src_slot;
    unsigned addr_slot = inst->immed.write_32_slot.addr_slot;

    prefunc();

    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)sh4, RDI);
    move_slot_to_reg(src_slot, ESI);
    move_slot_to_reg(addr_slot, EDX);

    evict_register(ESI);
    evict_register(EDX);

    align_stack();
    x86asm_call_ptr(sh4_write_mem_32);

    postfunc();

    ungrab_register(RAX);
}

static void
emit_load_slot16(Sh4 *sh4, struct jit_inst const* inst) {
    unsigned slot_no = inst->immed.load_slot16.slot_no;
    void const *src_ptr = inst->immed.load_slot16.src;

    grab_slot(slot_no);

    unsigned reg_no = slots[slot_no].reg_no;

    x86asm_mov_imm64_reg64((uintptr_t)src_ptr, reg_no);
    x86asm_movzxw_indreg_reg(reg_no, reg_no);

    ungrab_slot(slot_no);
}

static void
emit_load_slot(Sh4 *sh4, struct jit_inst const* inst) {
    unsigned slot_no = inst->immed.load_slot.slot_no;
    void const *src_ptr = inst->immed.load_slot.src;

    grab_slot(slot_no);

    unsigned reg_no = slots[slot_no].reg_no;

    x86asm_mov_imm64_reg64((uintptr_t)src_ptr, reg_no);
    x86asm_mov_indreg32_reg32(reg_no, reg_no);

    ungrab_slot(slot_no);
}

static void
emit_store_slot(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.store_slot.slot_no;
    void const *dst_ptr = inst->immed.store_slot.dst;

    grab_register(RAX);
    evict_register(RAX);
    grab_slot(slot_no);

    unsigned reg_no = slots[slot_no].reg_no;
    x86asm_mov_imm64_reg64((uintptr_t)dst_ptr, RAX);
    x86asm_mov_reg32_indreg32(reg_no, RAX);

    ungrab_slot(slot_no);
    ungrab_register(RAX);
}

static void
emit_add(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_src = inst->immed.add.slot_src;
    unsigned slot_dst = inst->immed.add.slot_dst;

    grab_slot(slot_src);
    if (slot_src != slot_dst)
        grab_slot(slot_dst);

    x86asm_addl_reg32_reg32(slots[slot_src].reg_no, slots[slot_dst].reg_no);

    if (slot_src != slot_dst)
        ungrab_slot(slot_dst);
    ungrab_slot(slot_src);
}

static void
emit_sub(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_src = inst->immed.add.slot_src;
    unsigned slot_dst = inst->immed.add.slot_dst;

    grab_slot(slot_src);
    if (slot_src != slot_dst)
        grab_slot(slot_dst);

    x86asm_subl_reg32_reg32(slots[slot_src].reg_no, slots[slot_dst].reg_no);

    if (slot_src != slot_dst)
        ungrab_slot(slot_dst);
    ungrab_slot(slot_src);
}

static void
emit_add_const32(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.add_const32.slot_dst;
    uint32_t const_val = inst->immed.add_const32.const32;

    grab_register(RAX);
    evict_register(RAX);

    grab_slot(slot_no);

    unsigned reg_no = slots[slot_no].reg_no;

    x86asm_mov_reg32_reg32(reg_no, EAX);
    x86asm_add_imm32_eax(const_val);
    x86asm_mov_reg32_reg32(EAX, reg_no);

    ungrab_slot(slot_no);
    ungrab_register(RAX);
}

static void
emit_xor(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_src = inst->immed.xor.slot_src;
    unsigned slot_dst = inst->immed.xor.slot_dst;

    grab_slot(slot_src);
    if (slot_src != slot_dst)
        grab_slot(slot_dst);

    x86asm_xorl_reg32_reg32(slots[slot_src].reg_no, slots[slot_dst].reg_no);

    if (slot_src != slot_dst)
        ungrab_slot(slot_dst);
    ungrab_slot(slot_src);
}

static void
emit_mov(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_src = inst->immed.mov.slot_src;
    unsigned slot_dst = inst->immed.mov.slot_dst;

    grab_slot(slot_src);
    if (slot_src != slot_dst)
        grab_slot(slot_dst);

    x86asm_mov_reg32_reg32(slots[slot_src].reg_no, slots[slot_dst].reg_no);

    if (slot_src != slot_dst)
        ungrab_slot(slot_dst);
    ungrab_slot(slot_src);
}

static void
emit_and(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_src = inst->immed.and.slot_src;
    unsigned slot_dst = inst->immed.and.slot_dst;

    grab_slot(slot_src);
    if (slot_src != slot_dst)
        grab_slot(slot_dst);

    x86asm_andl_reg32_reg32(slots[slot_src].reg_no, slots[slot_dst].reg_no);

    if (slot_src != slot_dst)
        ungrab_slot(slot_dst);
    ungrab_slot(slot_src);
}

static void
emit_and_const32(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.and_const32.slot_no;
    unsigned const32 = inst->immed.and_const32.const32;

    grab_slot(slot_no);

    x86asm_andl_imm32_reg32(const32, slots[slot_no].reg_no);

    ungrab_slot(slot_no);
}

static void
emit_or(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_src = inst->immed.or.slot_src;
    unsigned slot_dst = inst->immed.or.slot_dst;

    grab_slot(slot_src);
    if (slot_src != slot_dst)
        grab_slot(slot_dst);

    x86asm_orl_reg32_reg32(slots[slot_src].reg_no, slots[slot_dst].reg_no);

    if (slot_src != slot_dst)
        ungrab_slot(slot_dst);
    ungrab_slot(slot_src);
}

static void
emit_or_const32(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.or_const32.slot_no;
    unsigned const32 = inst->immed.or_const32.const32;

    grab_slot(slot_no);

    x86asm_orl_imm32_reg32(const32, slots[slot_no].reg_no);

    ungrab_slot(slot_no);
}

static void
emit_xor_const32(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.xor_const32.slot_no;
    unsigned const32 = inst->immed.xor_const32.const32;

    grab_slot(slot_no);

    x86asm_xorl_imm32_reg32(const32, slots[slot_no].reg_no);

    ungrab_slot(slot_no);
}

static void
emit_slot_to_bool(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.slot_to_bool.slot_no;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_register(RAX);
    evict_register(RAX);
    grab_slot(slot_no);

    x86asm_mov_reg32_reg32(slots[slot_no].reg_no, EAX);
    x86asm_xorl_reg32_reg32(EAX, EAX);
    x86asm_cmpl_reg32_imm8(slots[slot_no].reg_no, 0);
    x86asm_jz_lbl8(&lbl);
    x86asm_incl_reg32(EAX);

    x86asm_lbl8_define(&lbl);
    x86asm_mov_reg32_reg32(EAX, slots[slot_no].reg_no);

    ungrab_slot(slot_no);
    ungrab_register(RAX);

    x86asm_lbl8_cleanup(&lbl);
}

static void
emit_not(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.not.slot_no;

    grab_slot(slot_no);

    x86asm_notl_reg32(slots[slot_no].reg_no);

    ungrab_slot(slot_no);
}

static void
emit_shll(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.shll.slot_no;
    unsigned shift_amt = inst->immed.shll.shift_amt;

    if (shift_amt >= 32)
        shift_amt = 32;

    grab_slot(slot_no);
    x86asm_shll_imm8_reg32(shift_amt, slots[slot_no].reg_no);
    ungrab_slot(slot_no);
}

static void
emit_shar(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.shar.slot_no;
    unsigned shift_amt = inst->immed.shar.shift_amt;

    if (shift_amt >= 32)
        shift_amt = 32;

    grab_slot(slot_no);
    x86asm_sarl_imm8_reg32(shift_amt, slots[slot_no].reg_no);
    ungrab_slot(slot_no);
}

static void
emit_shlr(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.shlr.slot_no;
    unsigned shift_amt = inst->immed.shlr.shift_amt;

    if (shift_amt >= 32)
        shift_amt = 32;

    grab_slot(slot_no);
    x86asm_shrl_imm8_reg32(shift_amt, slots[slot_no].reg_no);
    ungrab_slot(slot_no);
}

static void emit_set_gt(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.set_gt.slot_lhs;
    unsigned slot_rhs = inst->immed.set_gt.slot_rhs;
    unsigned slot_dst = inst->immed.set_gt.slot_dst;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_slot(slot_lhs);
    grab_slot(slot_rhs);
    grab_slot(slot_dst);

    x86asm_cmpl_reg32_reg32(slots[slot_lhs].reg_no, slots[slot_rhs].reg_no);
    x86asm_jbe_lbl8(&lbl);
    x86asm_orl_imm32_reg32(1, slots[slot_dst].reg_no);
    x86asm_lbl8_define(&lbl);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_rhs);
    ungrab_slot(slot_lhs);

    x86asm_lbl8_cleanup(&lbl);
}

static void emit_mul_u32(Sh4 *sh4, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.mul_u32.slot_lhs;
    unsigned slot_rhs = inst->immed.mul_u32.slot_rhs;
    unsigned slot_dst = inst->immed.mul_u32.slot_dst;

    evict_register(EAX);
    evict_register(EDX);
    grab_register(EAX);
    grab_register(EDX);

    grab_slot(slot_lhs);
    grab_slot(slot_rhs);
    grab_slot(slot_dst);

    x86asm_mov_reg32_reg32(slots[slot_lhs].reg_no, EAX);
    x86asm_mull_reg32(slots[slot_rhs].reg_no);
    x86asm_mov_reg32_reg32(EAX, slots[slot_dst].reg_no);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_rhs);
    ungrab_slot(slot_lhs);
    ungrab_register(EDX);
    ungrab_register(EAX);
}

/*
 * pad the stack so that it is properly aligned for a function call.
 * At the beginning of the stack frame, the stack was aligned by
 * 16 modulo 8.  emit_stack_frame_open pushed 6*8 bytes onto the stack; this
 * means that the stack alignment was still 16 modulo 8 after the stack frame
 * open.  The rsp_offs at that point was -40, which is 16 modulo 8.  Ergo, when
 * rsp_offs is 16-modulo-8, then the stack is 16-modulo-8.  Likewise, when
 * rsp_offs is divisible by 16 then so is the actual stack pointer.
 *
 * Prior to issuing a call instruction, the stack-pointer needs to be aligned
 * on a 16-byte boundary so that the alignment is 16-modulo-8 after the CALL
 * instruction pushes the return address.  This function pads the stack so that
 * it is aligned on a 16-byte boundary and the CALL instruction can be safely
 * issued.
 */
static void align_stack(void) {
    unsigned mod = rsp_offs % 16;

    if (mod) {
        x86asm_addq_imm8_reg(-(16 - mod), RSP);
        rsp_offs -= (16 - mod);
    }
}

void code_block_x86_64_compile(struct code_block_x86_64 *out,
                               struct il_code_block const *il_blk) {
    struct jit_inst const* inst = il_blk->inst_list;
    unsigned inst_count = il_blk->inst_count;
    Sh4 *sh4 = dreamcast_get_cpu();
    out->cycle_count = il_blk->cycle_count * SH4_CLOCK_SCALE;

    x86asm_set_dst(out->native, X86_64_ALLOC_SIZE);

    reset_slots();

    emit_stack_frame_open();

    while (inst_count--) {
        switch (inst->op) {
        case JIT_OP_FALLBACK:
            emit_fallback(sh4, inst);
            break;
        case JIT_OP_JUMP:
            emit_jump(sh4, inst);
            return;
        case JIT_JUMP_COND:
            emit_jump_cond(sh4, inst);
            return;
        case JIT_SET_SLOT:
            emit_set_slot(sh4, inst);
            break;
        case JIT_OP_RESTORE_SR:
            emit_restore_sr(sh4, inst);
            break;
        case JIT_OP_READ_16_CONSTADDR:
            emit_read_16_constaddr(sh4, inst);
            break;
        case JIT_OP_SIGN_EXTEND_16:
            emit_sign_extend_16(sh4, inst);
            break;
        case JIT_OP_READ_32_CONSTADDR:
            emit_read_32_constaddr(sh4, inst);
            break;
        case JIT_OP_READ_32_SLOT:
            emit_read_32_slot(sh4, inst);
            break;
        case JIT_OP_WRITE_32_SLOT:
            emit_write_32_slot(sh4, inst);
            break;
        case JIT_OP_LOAD_SLOT16:
            emit_load_slot16(sh4, inst);
            break;
        case JIT_OP_LOAD_SLOT:
            emit_load_slot(sh4, inst);
            break;
        case JIT_OP_STORE_SLOT:
            emit_store_slot(sh4, inst);
            break;
        case JIT_OP_ADD:
            emit_add(sh4, inst);
            break;
        case JIT_OP_SUB:
            emit_sub(sh4, inst);
            break;
        case JIT_OP_ADD_CONST32:
            emit_add_const32(sh4, inst);
            break;
        case JIT_OP_XOR:
            emit_xor(sh4, inst);
            break;
        case JIT_OP_XOR_CONST32:
            emit_xor_const32(sh4, inst);
            break;
        case JIT_OP_MOV:
            emit_mov(sh4, inst);
            break;
        case JIT_OP_AND:
            emit_and(sh4, inst);
            break;
        case JIT_OP_AND_CONST32:
            emit_and_const32(sh4, inst);
            break;
        case JIT_OP_OR:
            emit_or(sh4, inst);
            break;
        case JIT_OP_OR_CONST32:
            emit_or_const32(sh4, inst);
            break;
        case JIT_OP_DISCARD_SLOT:
            discard_slot(inst->immed.discard_slot.slot_no);
            break;
        case JIT_OP_SLOT_TO_BOOL:
            emit_slot_to_bool(sh4, inst);
            break;
        case JIT_OP_NOT:
            emit_not(sh4, inst);
            break;
        case JIT_OP_SHLL:
            emit_shll(sh4, inst);
            break;
        case JIT_OP_SHAR:
            emit_shar(sh4, inst);
            break;
        case JIT_OP_SHLR:
            emit_shlr(sh4, inst);
            break;
        case JIT_OP_SET_GT:
            emit_set_gt(sh4, inst);
            break;
        case JIT_OP_MUL_U32:
            emit_mul_u32(sh4, inst);
            break;
        }
        inst++;
    }

    // all blocks should end by jumping out
    LOG_ERROR("ERROR: %u-len block does not jump out\n", il_blk->inst_count);
    RAISE_ERROR(ERROR_INTEGRITY);
}
