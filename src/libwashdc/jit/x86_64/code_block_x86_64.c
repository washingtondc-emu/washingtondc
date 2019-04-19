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

#ifndef ENABLE_JIT_X86_64
#error this file should not be built when the x86_64 JIT backend is disabled
#endif

#include <errno.h>
#include <stddef.h>

#include "washdc/error.h"
#include "jit/code_block.h"
#include "jit/jit_il.h"
#include "exec_mem.h"
#include "emit_x86_64.h"
#include "dreamcast.h"
#include "native_dispatch.h"
#include "native_mem.h"
#include "abi.h"
#include "config.h"
#include "washdc/cpu.h"

#include "code_block_x86_64.h"

#define N_REGS 16

static DEF_ERROR_INT_ATTR(x86_64_reg);

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
#if defined(ABI_UNIX)
    evict_register(RAX);
    grab_register(RAX);
    evict_register(RCX);
    grab_register(RCX);
    evict_register(RDX);
    grab_register(RDX);
    evict_register(RSI);
    grab_register(RSI);
    evict_register(RDI);
    grab_register(RDI);
    evict_register(R8);
    grab_register(R8);
    evict_register(R9);
    grab_register(R9);
    evict_register(R10);
    grab_register(R10);
    evict_register(R11);
    grab_register(R11);
#elif defined(ABI_MICROSOFT)
    evict_register(RAX);
    grab_register(RAX);
    evict_register(RCX);
    grab_register(RCX);
    evict_register(RDX);
    grab_register(RDX);
    evict_register(R8);
    grab_register(R8);
    evict_register(R9);
    grab_register(R9);
    evict_register(R10);
    grab_register(R10);
    evict_register(R11);
    grab_register(R11);
#endif
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
#if defined(ABI_UNIX)
    ungrab_register(R11);
    ungrab_register(R10);
    ungrab_register(R9);
    ungrab_register(R8);
    ungrab_register(RDI);
    ungrab_register(RSI);
    ungrab_register(RDX);
    ungrab_register(RCX);
#elif defined(ABI_MICROSOFT)
    ungrab_register(R11);
    ungrab_register(R10);
    ungrab_register(R9);
    ungrab_register(R8);
    ungrab_register(RDX);
    ungrab_register(RCX);
#endif
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
     * Don't allow writes to anywhere >= %rbp-0 because that is where the
     * saved variables are stored on the stack (see emit_frame_open).
     */
    if (slot->rbp_offs >= 0)
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
 * stack.  Immediately after calling this, grab the register to prevent it from
 * being allocated, and subsequently ungrab it when finished.  The register's
 * contents are unchanged by this function.
 */
static void evict_register(unsigned reg_no) {
    struct reg_stat *reg = regs + reg_no;
    if (reg->in_use) {
        int reg_dst = pick_unused_reg();
        if (reg_dst == reg_no)
            RAISE_ERROR(ERROR_INTEGRITY);

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
        if (slot->in_reg) {
            if (regs[slot->reg_no].grabbed)
                return;
            else
                goto mark_grabbed;
        }

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
    if (regs[reg_no].grabbed) {
        error_set_x86_64_reg(reg_no);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
    regs[reg_no].grabbed = true;
}

static void ungrab_register(unsigned reg_no) {
    if (!regs[reg_no].grabbed) {
        error_set_x86_64_reg(reg_no);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
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

    rsp_offs = 0;
}

static void emit_stack_frame_close(void) {
    x86asm_mov_reg64_reg64(RBP, RSP);
    x86asm_popq_reg64(RBP);
}

// JIT_OP_FALLBACK implementation
static void emit_fallback(void *cpu, struct jit_inst const *inst) {
    cpu_inst_param inst_bin = inst->immed.fallback.inst;

    prefunc();

    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)cpu, REG_ARG0);
    x86asm_mov_imm32_reg32(inst_bin, REG_ARG1);

    ms_shadow_open();
    x86_64_align_stack();
    x86asm_call_ptr(inst->immed.fallback.fallback_fn);
    ms_shadow_close();

    postfunc();
    ungrab_register(REG_RET);
}

// JIT_OP_JUMP implementation
static void emit_jump(void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.jump.slot_no;

    evict_register(REG_RET);
    grab_register(REG_RET);

    grab_slot(slot_no);

    x86asm_mov_reg32_reg32(slots[slot_no].reg_no, REG_RET);

    ungrab_slot(slot_no);
}

// JIT_JUMP_COND implementation
static void emit_jump_cond(void *cpu, struct jit_inst const *inst) {
    unsigned t_flag = inst->immed.jump_cond.t_flag ? 1 : 0;
    unsigned flag_slot = inst->immed.jump_cond.slot_no;
    unsigned jmp_addr_slot = inst->immed.jump_cond.jmp_addr_slot;
    unsigned alt_jmp_addr_slot = inst->immed.jump_cond.alt_jmp_addr_slot;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    evict_register(REG_RET);
    grab_register(REG_RET);

    grab_slot(flag_slot);
    x86asm_mov_reg32_reg32(slots[flag_slot].reg_no, REG_RET);
    ungrab_slot(flag_slot);

    grab_slot(jmp_addr_slot);
    grab_slot(alt_jmp_addr_slot);

    /*
     * move the alt-jmp addr into the return register, then replace that with
     * the normal jmp addr if the flag is set.
     */
    x86asm_and_imm32_rax(1);
    x86asm_mov_reg32_reg32(slots[alt_jmp_addr_slot].reg_no, REG_RET);
    if (t_flag)
        x86asm_jz_lbl8(&lbl);
    else
        x86asm_jnz_lbl8(&lbl);
    x86asm_mov_reg32_reg32(slots[jmp_addr_slot].reg_no, REG_RET);
    x86asm_lbl8_define(&lbl);

    // the chosen address is now in %rax, so we're ready to return

    ungrab_slot(alt_jmp_addr_slot);
    ungrab_slot(jmp_addr_slot);

    ungrab_register(REG_RET); // not that it matters at this point...

    x86asm_lbl8_cleanup(&lbl);
}

// JIT_SET_REG implementation
static void emit_set_slot(void *cpu, struct jit_inst const *inst) {
    unsigned slot_idx = inst->immed.set_slot.slot_idx;
    uint32_t new_val = inst->immed.set_slot.new_val;

    grab_slot(slot_idx);
    x86asm_mov_imm32_reg32(new_val, slots[slot_idx].reg_no);
    ungrab_slot(slot_idx);
}

// JIT_OP_CALL_FUNC implementation
static void emit_call_func(void *cpu, struct jit_inst const *inst) {
    prefunc();

    // now call sh4_on_sr_change(cpu, old_sr)
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)cpu, REG_ARG0);
    move_slot_to_reg(inst->immed.call_func.slot_no, REG_ARG1);

    evict_register(REG_ARG1); // TODO: is this necessary ?

    ms_shadow_open();
    x86_64_align_stack();
    x86asm_call_ptr(inst->immed.call_func.func);
    ms_shadow_close();

    postfunc();
    ungrab_register(REG_RET);
}

// JIT_OP_READ_16_CONSTADDR implementation
static void emit_read_16_constaddr(void *cpu, struct jit_inst const *inst) {
    addr32_t vaddr = inst->immed.read_16_constaddr.addr;
    unsigned slot_no = inst->immed.read_16_constaddr.slot_no;
    struct memory_map const *map = inst->immed.read_16_constaddr.map;

    // call memory_map_read_16(vaddr)
    prefunc();

    if (config_get_inline_mem()) {
        x86asm_mov_imm32_reg32(vaddr, REG_ARG0);
        native_mem_read_16(map);
    } else {
        x86asm_mov_imm64_reg64((uint64_t)map, REG_ARG0);
        x86asm_mov_imm32_reg32(vaddr, REG_ARG1);
        ms_shadow_open();
        x86_64_align_stack();
        x86asm_call_ptr(memory_map_read_16);
        ms_shadow_close();
    }

    postfunc();

    grab_slot(slot_no);
    x86asm_mov_reg32_reg32(REG_RET, slots[slot_no].reg_no);

    ungrab_register(REG_RET);
    ungrab_slot(slot_no);
}

// JIT_OP_SIGN_EXTEND_16 implementation
static void emit_sign_extend_16(void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.sign_extend_16.slot_no;

    grab_slot(slot_no);

    unsigned reg_no = slots[slot_no].reg_no;
    x86asm_movsx_reg16_reg32(reg_no, reg_no);

    ungrab_slot(slot_no);
}

// JIT_OP_READ_32_CONSTADDR implementation
static void emit_read_32_constaddr(void *cpu, struct jit_inst const *inst) {
    addr32_t vaddr = inst->immed.read_32_constaddr.addr;
    unsigned slot_no = inst->immed.read_32_constaddr.slot_no;
    struct memory_map const *map = inst->immed.read_32_constaddr.map;

    // call memory_map_read_32(vaddr)

    prefunc();

    if (config_get_inline_mem()) {
        x86asm_mov_imm32_reg32(vaddr, REG_ARG0);
        native_mem_read_32(map);
    } else {
        x86asm_mov_imm64_reg64((uint64_t)map, REG_ARG0);
        x86asm_mov_imm32_reg32(vaddr, REG_ARG1);
        ms_shadow_open();
        x86_64_align_stack();
        x86asm_call_ptr(memory_map_read_32);
        ms_shadow_close();
    }

    postfunc();

    grab_slot(slot_no);
    x86asm_mov_reg32_reg32(REG_RET, slots[slot_no].reg_no);

    ungrab_slot(slot_no);
    ungrab_register(REG_RET);
}

// JIT_OP_READ_32_SLOT implementation
static void emit_read_32_slot(void *cpu, struct jit_inst const *inst) {
    unsigned dst_slot = inst->immed.read_32_slot.dst_slot;
    unsigned addr_slot = inst->immed.read_32_slot.addr_slot;
    struct memory_map const *map = inst->immed.read_32_slot.map;

    // call memory_map_read_32(*addr_slot)
    prefunc();

    if (config_get_inline_mem()) {
        move_slot_to_reg(addr_slot, REG_ARG0);
        evict_register(REG_ARG0);
        native_mem_read_32(map);
    } else {
        x86asm_mov_imm64_reg64((uint64_t)map, REG_ARG0);
        move_slot_to_reg(addr_slot, REG_ARG1);
        evict_register(REG_ARG1);
        ms_shadow_open();
        x86_64_align_stack();
        x86asm_call_ptr(memory_map_read_32);
        ms_shadow_close();
    }

    postfunc();

    grab_slot(dst_slot);
    x86asm_mov_reg32_reg32(REG_RET, slots[dst_slot].reg_no);

    ungrab_slot(dst_slot);
    ungrab_register(REG_RET);
}

// JIT_OP_WRITE_32_SLOT implementation
static void emit_write_32_slot(void *cpu, struct jit_inst const *inst) {
    unsigned src_slot = inst->immed.write_32_slot.src_slot;
    unsigned addr_slot = inst->immed.write_32_slot.addr_slot;
    struct memory_map const *map = inst->immed.write_32_slot.map;

    prefunc();

    if (config_get_inline_mem()) {
        move_slot_to_reg(addr_slot, REG_ARG0);
        move_slot_to_reg(src_slot, REG_ARG1);

        evict_register(REG_ARG0);
        evict_register(REG_ARG1);

        native_mem_write_32(map);
    } else {
        move_slot_to_reg(addr_slot, REG_ARG1);
        move_slot_to_reg(src_slot, REG_ARG2);

        evict_register(REG_ARG1);
        evict_register(REG_ARG2);

        x86asm_mov_imm64_reg64((uint64_t)map, REG_ARG0);
        ms_shadow_open();
        x86_64_align_stack();
        x86asm_call_ptr(memory_map_write_32);
        ms_shadow_close();
    }

    postfunc();

    ungrab_register(REG_RET);
}

static void
emit_load_slot16(void *cpu, struct jit_inst const* inst) {
    unsigned slot_no = inst->immed.load_slot16.slot_no;
    void const *src_ptr = inst->immed.load_slot16.src;

    grab_slot(slot_no);

    unsigned reg_no = slots[slot_no].reg_no;

    x86asm_mov_imm64_reg64((uintptr_t)src_ptr, reg_no);
    x86asm_movzxw_indreg_reg(reg_no, reg_no);

    ungrab_slot(slot_no);
}

static void
emit_load_slot(void *cpu, struct jit_inst const* inst) {
    unsigned slot_no = inst->immed.load_slot.slot_no;
    void const *src_ptr = inst->immed.load_slot.src;

    grab_slot(slot_no);

    unsigned reg_no = slots[slot_no].reg_no;

    x86asm_mov_imm64_reg64((uintptr_t)src_ptr, reg_no);
    x86asm_mov_indreg32_reg32(reg_no, reg_no);

    ungrab_slot(slot_no);
}

static void
emit_store_slot(void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.store_slot.slot_no;
    void const *dst_ptr = inst->immed.store_slot.dst;

    evict_register(REG_RET);
    grab_register(REG_RET);
    grab_slot(slot_no);

    unsigned reg_no = slots[slot_no].reg_no;
    x86asm_mov_imm64_reg64((uintptr_t)dst_ptr, REG_RET);
    x86asm_mov_reg32_indreg32(reg_no, REG_RET);

    ungrab_slot(slot_no);
    ungrab_register(REG_RET);
}

static void
emit_add(void *cpu, struct jit_inst const *inst) {
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
emit_sub(void *cpu, struct jit_inst const *inst) {
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
emit_add_const32(void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.add_const32.slot_dst;
    uint32_t const_val = inst->immed.add_const32.const32;

    evict_register(REG_RET);
    grab_register(REG_RET);

    grab_slot(slot_no);

    unsigned reg_no = slots[slot_no].reg_no;

    x86asm_mov_reg32_reg32(reg_no, REG_RET);
    x86asm_add_imm32_eax(const_val);
    x86asm_mov_reg32_reg32(REG_RET, reg_no);

    ungrab_slot(slot_no);
    ungrab_register(REG_RET);
}

static void
emit_xor(void *cpu, struct jit_inst const *inst) {
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
emit_mov(void *cpu, struct jit_inst const *inst) {
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
emit_and(void *cpu, struct jit_inst const *inst) {
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
emit_and_const32(void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.and_const32.slot_no;
    unsigned const32 = inst->immed.and_const32.const32;

    grab_slot(slot_no);

    x86asm_andl_imm32_reg32(const32, slots[slot_no].reg_no);

    ungrab_slot(slot_no);
}

static void
emit_or(void *cpu, struct jit_inst const *inst) {
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
emit_or_const32(void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.or_const32.slot_no;
    unsigned const32 = inst->immed.or_const32.const32;

    grab_slot(slot_no);

    x86asm_orl_imm32_reg32(const32, slots[slot_no].reg_no);

    ungrab_slot(slot_no);
}

static void
emit_xor_const32(void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.xor_const32.slot_no;
    unsigned const32 = inst->immed.xor_const32.const32;

    grab_slot(slot_no);

    x86asm_xorl_imm32_reg32(const32, slots[slot_no].reg_no);

    ungrab_slot(slot_no);
}

static void
emit_slot_to_bool(void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.slot_to_bool.slot_no;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    evict_register(REG_RET);
    grab_register(REG_RET);
    grab_slot(slot_no);

    x86asm_xorl_reg32_reg32(REG_RET, REG_RET);
    x86asm_testl_reg32_reg32(slots[slot_no].reg_no, slots[slot_no].reg_no);
    x86asm_jz_lbl8(&lbl);
    x86asm_incl_reg32(REG_RET);

    x86asm_lbl8_define(&lbl);
    x86asm_mov_reg32_reg32(REG_RET, slots[slot_no].reg_no);

    ungrab_slot(slot_no);
    ungrab_register(REG_RET);

    x86asm_lbl8_cleanup(&lbl);
}

static void
emit_not(void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.not.slot_no;

    grab_slot(slot_no);

    x86asm_notl_reg32(slots[slot_no].reg_no);

    ungrab_slot(slot_no);
}

static void
emit_shll(void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.shll.slot_no;
    unsigned shift_amt = inst->immed.shll.shift_amt;

    if (shift_amt >= 32)
        shift_amt = 32;

    grab_slot(slot_no);
    x86asm_shll_imm8_reg32(shift_amt, slots[slot_no].reg_no);
    ungrab_slot(slot_no);
}

static void
emit_shar(void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.shar.slot_no;
    unsigned shift_amt = inst->immed.shar.shift_amt;

    if (shift_amt >= 32)
        shift_amt = 32;

    grab_slot(slot_no);
    x86asm_sarl_imm8_reg32(shift_amt, slots[slot_no].reg_no);
    ungrab_slot(slot_no);
}

static void
emit_shlr(void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.shlr.slot_no;
    unsigned shift_amt = inst->immed.shlr.shift_amt;

    if (shift_amt >= 32)
        shift_amt = 32;

    grab_slot(slot_no);
    x86asm_shrl_imm8_reg32(shift_amt, slots[slot_no].reg_no);
    ungrab_slot(slot_no);
}

static void emit_set_gt_unsigned(void *cpu, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.set_gt_unsigned.slot_lhs;
    unsigned slot_rhs = inst->immed.set_gt_unsigned.slot_rhs;
    unsigned slot_dst = inst->immed.set_gt_unsigned.slot_dst;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_slot(slot_lhs);
    grab_slot(slot_rhs);
    grab_slot(slot_dst);

    x86asm_cmpl_reg32_reg32(slots[slot_rhs].reg_no, slots[slot_lhs].reg_no);
    x86asm_jbe_lbl8(&lbl);
    x86asm_orl_imm32_reg32(1, slots[slot_dst].reg_no);
    x86asm_lbl8_define(&lbl);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_rhs);
    ungrab_slot(slot_lhs);

    x86asm_lbl8_cleanup(&lbl);
}

static void emit_set_gt_signed(void *cpu, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.set_gt_signed.slot_lhs;
    unsigned slot_rhs = inst->immed.set_gt_signed.slot_rhs;
    unsigned slot_dst = inst->immed.set_gt_signed.slot_dst;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_slot(slot_lhs);
    grab_slot(slot_rhs);
    grab_slot(slot_dst);

    x86asm_cmpl_reg32_reg32(slots[slot_rhs].reg_no, slots[slot_lhs].reg_no);
    x86asm_jle_lbl8(&lbl);
    x86asm_orl_imm32_reg32(1, slots[slot_dst].reg_no);
    x86asm_lbl8_define(&lbl);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_rhs);
    ungrab_slot(slot_lhs);

    x86asm_lbl8_cleanup(&lbl);
}

static void emit_set_gt_signed_const(void *cpu, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.set_gt_signed_const.slot_lhs;
    unsigned imm_rhs = inst->immed.set_gt_signed_const.imm_rhs;
    unsigned slot_dst = inst->immed.set_gt_signed_const.slot_dst;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_slot(slot_lhs);
    grab_slot(slot_dst);

    x86asm_cmpl_imm8_reg32(imm_rhs, slots[slot_lhs].reg_no);
    x86asm_jle_lbl8(&lbl);
    x86asm_orl_imm32_reg32(1, slots[slot_dst].reg_no);
    x86asm_lbl8_define(&lbl);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_lhs);

    x86asm_lbl8_cleanup(&lbl);
}

static void emit_set_ge_signed_const(void *cpu, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.set_ge_signed_const.slot_lhs;
    unsigned imm_rhs = inst->immed.set_ge_signed_const.imm_rhs;
    unsigned slot_dst = inst->immed.set_ge_signed_const.slot_dst;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_slot(slot_lhs);
    grab_slot(slot_dst);

    x86asm_cmpl_imm8_reg32(imm_rhs, slots[slot_lhs].reg_no);
    x86asm_jl_lbl8(&lbl);
    x86asm_orl_imm32_reg32(1, slots[slot_dst].reg_no);
    x86asm_lbl8_define(&lbl);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_lhs);

    x86asm_lbl8_cleanup(&lbl);
}

static void emit_set_eq(void *cpu, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.set_eq.slot_lhs;
    unsigned slot_rhs = inst->immed.set_eq.slot_rhs;
    unsigned slot_dst = inst->immed.set_eq.slot_dst;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_slot(slot_lhs);
    grab_slot(slot_rhs);
    grab_slot(slot_dst);

    x86asm_cmpl_reg32_reg32(slots[slot_rhs].reg_no, slots[slot_lhs].reg_no);
    x86asm_jnz_lbl8(&lbl);

    x86asm_orl_imm32_reg32(1, slots[slot_dst].reg_no);
    x86asm_lbl8_define(&lbl);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_rhs);
    ungrab_slot(slot_lhs);

    x86asm_lbl8_cleanup(&lbl);
}

static void emit_set_ge_unsigned(void *cpu, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.set_ge_unsigned.slot_lhs;
    unsigned slot_rhs = inst->immed.set_ge_unsigned.slot_rhs;
    unsigned slot_dst = inst->immed.set_ge_unsigned.slot_dst;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_slot(slot_lhs);
    grab_slot(slot_rhs);
    grab_slot(slot_dst);

    x86asm_cmpl_reg32_reg32(slots[slot_rhs].reg_no, slots[slot_lhs].reg_no);
    x86asm_jb_lbl8(&lbl);
    x86asm_orl_imm32_reg32(1, slots[slot_dst].reg_no);
    x86asm_lbl8_define(&lbl);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_rhs);
    ungrab_slot(slot_lhs);

    x86asm_lbl8_cleanup(&lbl);
}

static void emit_set_ge_signed(void *cpu, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.set_ge_signed.slot_lhs;
    unsigned slot_rhs = inst->immed.set_ge_signed.slot_rhs;
    unsigned slot_dst = inst->immed.set_ge_signed.slot_dst;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_slot(slot_lhs);
    grab_slot(slot_rhs);
    grab_slot(slot_dst);

    x86asm_cmpl_reg32_reg32(slots[slot_rhs].reg_no, slots[slot_lhs].reg_no);
    x86asm_jl_lbl8(&lbl);
    x86asm_orl_imm32_reg32(1, slots[slot_dst].reg_no);
    x86asm_lbl8_define(&lbl);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_rhs);
    ungrab_slot(slot_lhs);

    x86asm_lbl8_cleanup(&lbl);
}

static void emit_mul_u32(void *cpu, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.mul_u32.slot_lhs;
    unsigned slot_rhs = inst->immed.mul_u32.slot_rhs;
    unsigned slot_dst = inst->immed.mul_u32.slot_dst;

    evict_register(REG_RET);
    grab_register(REG_RET);
    evict_register(EDX);
    grab_register(EDX);

    grab_slot(slot_lhs);
    grab_slot(slot_rhs);
    grab_slot(slot_dst);

#ifdef INVARIANTS
    if (slots[slot_lhs].reg_no == REG_RET || slots[slot_lhs].reg_no == EDX ||
        slots[slot_rhs].reg_no == REG_RET || slots[slot_rhs].reg_no == EDX ||
        slots[slot_dst].reg_no == REG_RET || slots[slot_dst].reg_no == EDX)
        RAISE_ERROR(ERROR_INTEGRITY);
#endif

    x86asm_mov_reg32_reg32(slots[slot_lhs].reg_no, REG_RET);
    x86asm_mull_reg32(slots[slot_rhs].reg_no);
    x86asm_mov_reg32_reg32(REG_RET, slots[slot_dst].reg_no);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_rhs);
    ungrab_slot(slot_lhs);
    ungrab_register(EDX);
    ungrab_register(REG_RET);
}

static void emit_shad(void *cpu, struct jit_inst const *inst) {
    unsigned slot_val = inst->immed.shad.slot_val;
    unsigned slot_shift_amt = inst->immed.shad.slot_shift_amt;

    // shift_amt register must be CL
    evict_register(RCX);
    grab_register(RCX);

    int reg_tmp = pick_reg();
    evict_register(reg_tmp);
    grab_register(reg_tmp);

    grab_slot(slot_shift_amt);
    x86asm_mov_reg32_reg32(slots[slot_shift_amt].reg_no, RCX);
    ungrab_slot(slot_shift_amt);

    grab_slot(slot_val);

    x86asm_mov_reg32_reg32(slots[slot_val].reg_no, reg_tmp);
    x86asm_shll_cl_reg32(slots[slot_val].reg_no);

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    x86asm_testl_reg32_reg32(slots[slot_shift_amt].reg_no,
                             slots[slot_shift_amt].reg_no);
    x86asm_jns_lbl8(&lbl);

    x86asm_negl_reg32(ECX);
    x86asm_sarl_cl_reg32(reg_tmp);
    x86asm_mov_reg32_reg32(reg_tmp, slots[slot_val].reg_no);

    x86asm_lbl8_define(&lbl);
    x86asm_lbl8_cleanup(&lbl);

    ungrab_slot(slot_val);
    ungrab_register(reg_tmp);
    ungrab_register(RCX);
}

/*
 * pad the stack so that it is properly aligned for a function call.
 * At the beginning of the stack frame, the stack was aligned to a 16-byte
 * boundary.  emit_stack_frame_open pushed 8 bytes onto the stack; this
 * means that the stack alignment was 16 modulo 8 after the stack frame open.
 * The rsp_offs at that point was 0, which is not 16 modulo 8.  Ergo, when
 * rsp_offs is 16-modulo-8, then the stack is 16-byte aligned.  Likewise, when
 * rsp_offs is aligned to 16-bytes then so the actual stack pointer is not.
 *
 * Prior to issuing a call instruction, the stack-pointer needs to be aligned
 * on a 16-byte boundary so that the alignment is 16-modulo-8 after the CALL
 * instruction pushes the return address.  This function pads the stack so that
 * it is aligned on a 16-byte boundary and the CALL instruction can be safely
 * issued.
 */
void x86_64_align_stack(void) {
    unsigned mod = (rsp_offs - 8) % 16;

    if (mod) {
        x86asm_addq_imm8_reg(-(16 - mod), RSP);
        rsp_offs -= (16 - mod);
    }
}

/*
 * Microsoft's ABI requires 32 bytes to be allocated on the stack when calling
 * a function.
 */
void ms_shadow_open(void) {
#ifdef ABI_MICROSOFT
    x86asm_addq_imm8_reg(-32, RSP);
    rsp_offs -= 32;
#endif
}

void ms_shadow_close(void) {
#ifdef ABI_MICROSOFT
    x86asm_addq_imm8_reg(32, RSP);
    rsp_offs += 32;
#endif
}

void code_block_x86_64_compile(void *cpu, struct code_block_x86_64 *out,
                               struct il_code_block const *il_blk,
                               native_dispatch_compile_func compile_func,
                               unsigned cycle_count) {
    struct jit_inst const* inst = il_blk->inst_list;
    unsigned inst_count = il_blk->inst_count;
    out->cycle_count = cycle_count;

    x86asm_set_dst(out->native, X86_64_ALLOC_SIZE);

    reset_slots();

    emit_stack_frame_open();

    while (inst_count--) {
        switch (inst->op) {
        case JIT_OP_FALLBACK:
            emit_fallback(cpu, inst);
            break;
        case JIT_OP_JUMP:
            emit_jump(cpu, inst);
            break;
        case JIT_JUMP_COND:
            emit_jump_cond(cpu, inst);
            break;
        case JIT_SET_SLOT:
            emit_set_slot(cpu, inst);
            break;
        case JIT_OP_CALL_FUNC:
            emit_call_func(cpu, inst);
            break;
        case JIT_OP_READ_16_CONSTADDR:
            emit_read_16_constaddr(cpu, inst);
            break;
        case JIT_OP_SIGN_EXTEND_16:
            emit_sign_extend_16(cpu, inst);
            break;
        case JIT_OP_READ_32_CONSTADDR:
            emit_read_32_constaddr(cpu, inst);
            break;
        case JIT_OP_READ_32_SLOT:
            emit_read_32_slot(cpu, inst);
            break;
        case JIT_OP_WRITE_32_SLOT:
            emit_write_32_slot(cpu, inst);
            break;
        case JIT_OP_LOAD_SLOT16:
            emit_load_slot16(cpu, inst);
            break;
        case JIT_OP_LOAD_SLOT:
            emit_load_slot(cpu, inst);
            break;
        case JIT_OP_STORE_SLOT:
            emit_store_slot(cpu, inst);
            break;
        case JIT_OP_ADD:
            emit_add(cpu, inst);
            break;
        case JIT_OP_SUB:
            emit_sub(cpu, inst);
            break;
        case JIT_OP_ADD_CONST32:
            emit_add_const32(cpu, inst);
            break;
        case JIT_OP_XOR:
            emit_xor(cpu, inst);
            break;
        case JIT_OP_XOR_CONST32:
            emit_xor_const32(cpu, inst);
            break;
        case JIT_OP_MOV:
            emit_mov(cpu, inst);
            break;
        case JIT_OP_AND:
            emit_and(cpu, inst);
            break;
        case JIT_OP_AND_CONST32:
            emit_and_const32(cpu, inst);
            break;
        case JIT_OP_OR:
            emit_or(cpu, inst);
            break;
        case JIT_OP_OR_CONST32:
            emit_or_const32(cpu, inst);
            break;
        case JIT_OP_DISCARD_SLOT:
            discard_slot(inst->immed.discard_slot.slot_no);
            break;
        case JIT_OP_SLOT_TO_BOOL:
            emit_slot_to_bool(cpu, inst);
            break;
        case JIT_OP_NOT:
            emit_not(cpu, inst);
            break;
        case JIT_OP_SHLL:
            emit_shll(cpu, inst);
            break;
        case JIT_OP_SHAR:
            emit_shar(cpu, inst);
            break;
        case JIT_OP_SHLR:
            emit_shlr(cpu, inst);
            break;
        case JIT_OP_SET_GT_UNSIGNED:
            emit_set_gt_unsigned(cpu, inst);
            break;
        case JIT_OP_SET_GT_SIGNED:
            emit_set_gt_signed(cpu, inst);
            break;
        case JIT_OP_SET_GT_SIGNED_CONST:
            emit_set_gt_signed_const(cpu, inst);
            break;
        case JIT_OP_SET_EQ:
            emit_set_eq(cpu, inst);
            break;
        case JIT_OP_SET_GE_UNSIGNED:
            emit_set_ge_unsigned(cpu, inst);
            break;
        case JIT_OP_SET_GE_SIGNED:
            emit_set_ge_signed(cpu, inst);
            break;
        case JIT_OP_SET_GE_SIGNED_CONST:
            emit_set_ge_signed_const(cpu, inst);
            break;
        case JIT_OP_MUL_U32:
            emit_mul_u32(cpu, inst);
            break;
        case JIT_OP_SHAD:
            emit_shad(cpu, inst);
            break;
        }
        inst++;
    }

    x86asm_mov_imm32_reg32(out->cycle_count, REG_ARG0);
    x86asm_mov_reg32_reg32(REG_RET, REG_ARG1);
    emit_stack_frame_close();
    native_check_cycles_emit(cpu, compile_func);
}
