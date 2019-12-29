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

#include <limits.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>

#include "log.h"
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
#include "register_set.h"

#include "code_block_x86_64.h"

#define N_REGS 16
#define N_XMM_REGS 16

static struct reg_stat const gen_regs_template[N_REGS] = {
    [RAX] = {
        .locked = false,
        .prio = 0,
        .flags = REGISTER_FLAG_RETURN
    },
    [RCX] = {
        .locked = false,
        .prio = 10,
#ifdef ABI_UNIX
        .flags = REGISTER_FLAG_NATIVE_DISPATCH_PC
#else
        .flags = REGISTER_FLAG_NONE
#endif
    },
    [RDX] = {
        // RDX is a lower priority because mul will clobber it
        .locked = false,
        .prio = 7,
#ifdef ABI_UNIX
        .flags = REGISTER_FLAG_NATIVE_DISPATCH_HASH
#else
        .flags = REGISTER_FLAG_NONE
#endif
    },
    [RBX] = {
        .locked = false,
        .prio = 10,
        .flags = REGISTER_FLAG_PRESERVED
    },
    [RSP] = {
        // stack pointer
        .locked = true,
        .flags = REGISTER_FLAG_PRESERVED

    },
    [RBP] = {
        // base pointer
        .locked = true,
        .flags = REGISTER_FLAG_PRESERVED
    },
    [RSI] = {
        .locked = false,
        .prio = 8,
#ifdef ABI_UNIX
        .flags = REGISTER_FLAG_NATIVE_DISPATCH_HASH
#elif defined(ABI_MICROSOFT)
        .flags = REGISTER_FLAG_PRESERVED
#else
#error unknown ABI
#endif
    },
    [RDI] = {
        .locked = false,
        .prio = 9,
#ifdef ABI_UNIX
        .flags = REGISTER_FLAG_NATIVE_DISPATCH_PC
#elif defined(ABI_MICROSOFT)
        .flags = REGISTER_FLAG_PRESERVED
#else
#error unknown ABI
#endif
    },
    [R8] = {
        .locked = false,
        .prio = 6,
        .flags = REGISTER_FLAG_REX
    },
    [R9] = {
        .locked = false,
        .prio = 5,
        .flags = REGISTER_FLAG_REX
    },
    [R10] = {
        .locked = false,
        .prio = 4,
        .flags = REGISTER_FLAG_REX
    },
    [R11] = {
        .locked = false,
        .prio = 3,
        .flags = REGISTER_FLAG_REX
    },
    /*
     * R12 and R13 have a lower priority than R14 and R15 because they require
     * extra displacement or SIB bytes to go after the mod/reg/rm due to the
     * way that they overlap with RSP and RBP.
     */
    [R12] = {
        .locked = false,
        .prio = 7,
        .flags = REGISTER_FLAG_PRESERVED | REGISTER_FLAG_REX
    },
    [R13] = {
        .locked = false,
        .prio = 6,
        .flags = REGISTER_FLAG_PRESERVED | REGISTER_FLAG_REX
    },
    [R14] = {
        /*
         * pointer to code_cache_tbl.  This is the same on both Unix and
         * Microsoft ABI.
         */
        .locked = true,
        .prio = 9,
        .flags = REGISTER_FLAG_PRESERVED | REGISTER_FLAG_REX
    },
    [R15] = {
        .locked = false,
        .prio = 8,
        .flags = REGISTER_FLAG_PRESERVED | REGISTER_FLAG_REX
    }
};

static struct reg_stat const xmm_regs_template[N_XMM_REGS] = {
    [XMM0] = {
        .locked = false,
        .flags = REGISTER_FLAG_RETURN
    },
    [XMM1] = {
        .locked = false,
        .flags = REGISTER_FLAG_NONE
    },
    [XMM2] = {
        .locked = false,
        .flags = REGISTER_FLAG_NONE
    },
    [XMM3] = {
        .locked = false,
        .flags = REGISTER_FLAG_NONE
    },
    [XMM4] = {
        .locked = false,
        .flags = REGISTER_FLAG_NONE
    },
    [XMM5] = {
        .locked = false,
        .flags = REGISTER_FLAG_NONE
    },
    [XMM6] = {
        .locked = false,
#if defined(ABI_MICROSOFT)
        .flags = REGISTER_FLAG_PRESERVED
#elif defined(ABI_UNIX)
        .flags = REGISTER_FLAG_NONE
#else
#error unknown ABI
#endif
    },
    [XMM7] = {
        .locked = false,
#if defined(ABI_MICROSOFT)
        .flags = REGISTER_FLAG_PRESERVED
#elif defined(ABI_UNIX)
        .flags = REGISTER_FLAG_NONE
#else
#error unknown ABI
#endif
    },
    [XMM8] = {
        .locked = false,
#if defined(ABI_MICROSOFT)
        .flags = REGISTER_FLAG_PRESERVED
#elif defined(ABI_UNIX)
        .flags = REGISTER_FLAG_NONE
#else
#error unknown ABI
#endif
    },
    [XMM9] = {
        .locked = false,
#if defined(ABI_MICROSOFT)
        .flags = REGISTER_FLAG_PRESERVED
#elif defined(ABI_UNIX)
        .flags = REGISTER_FLAG_NONE
#else
#error unknown ABI
#endif
    },
    [XMM10] = {
        .locked = false,
#if defined(ABI_MICROSOFT)
        .flags = REGISTER_FLAG_PRESERVED
#elif defined(ABI_UNIX)
        .flags = REGISTER_FLAG_NONE
#else
#error unknown ABI
#endif
    },
    [XMM11] = {
        .locked = false,
#if defined(ABI_MICROSOFT)
        .flags = REGISTER_FLAG_PRESERVED
#elif defined(ABI_UNIX)
        .flags = REGISTER_FLAG_NONE
#else
#error unknown ABI
#endif
    },
    [XMM12] = {
        .locked = false,
#if defined(ABI_MICROSOFT)
        .flags = REGISTER_FLAG_PRESERVED
#elif defined(ABI_UNIX)
        .flags = REGISTER_FLAG_NONE
#else
#error unknown ABI
#endif
    },
    [XMM13] = {
        .locked = false,
#if defined(ABI_MICROSOFT)
        .flags = REGISTER_FLAG_PRESERVED
#elif defined(ABI_UNIX)
        .flags = REGISTER_FLAG_NONE
#else
#error unknown ABI
#endif
    },
    [XMM14] = {
        .locked = false,
#if defined(ABI_MICROSOFT)
        .flags = REGISTER_FLAG_PRESERVED
#elif defined(ABI_UNIX)
        .flags = REGISTER_FLAG_NONE
#else
#error unknown ABI
#endif
    },
    [XMM15] = {
        .locked = false,
#if defined(ABI_MICROSOFT)
        .flags = REGISTER_FLAG_PRESERVED
#elif defined(ABI_UNIX)
        .flags = REGISTER_FLAG_NONE
#else
#error unknown ABI
#endif
    },
};

static struct register_state {
    struct register_set set;

    // maps registers to slot indices
    int *reg_slots;
    int n_regs;
} gen_reg_state, xmm_reg_state;

struct slot {
    union {
        // offset from rbp (if this slot resides on the stack)
        int rbp_offs;

        // x86 register index (if this slot resides in a native host register)
        unsigned reg_no;
    };

    // for now, this is only valid for general-purpose slots.
    // floating point slots ignore this, but they might not in the future
    unsigned n_bytes;

    struct register_state *reg_state;

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

static void evict_register(struct code_block_x86_64 *blk,
                           struct register_state *reg_state, unsigned reg_no);

void jit_x86_64_backend_init(void) {
    memset(&gen_reg_state, 0, sizeof(gen_reg_state));
    register_set_init(&gen_reg_state.set, N_REGS, gen_regs_template);
    gen_reg_state.n_regs = N_REGS;
    gen_reg_state.reg_slots = (int*)malloc(sizeof(int) * N_REGS);

    memset(&xmm_reg_state, 0, sizeof(xmm_reg_state));
    register_set_init(&xmm_reg_state.set, N_XMM_REGS, xmm_regs_template);
    xmm_reg_state.n_regs = N_XMM_REGS;
    xmm_reg_state.reg_slots = (int*)malloc(sizeof(int) * N_XMM_REGS);
}

void jit_x86_64_backend_cleanup(void) {
    free(gen_reg_state.reg_slots);
    gen_reg_state.reg_slots = NULL;
    gen_reg_state.n_regs = 0;
    register_set_cleanup(&gen_reg_state.set);

    free(xmm_reg_state.reg_slots);
    xmm_reg_state.reg_slots = NULL;
    xmm_reg_state.n_regs = 0;
    register_set_cleanup(&xmm_reg_state.set);
}

static void reset_slots(void) {
    int reg_no;

    memset(slots, 0, sizeof(slots));

    register_set_reset(&gen_reg_state.set);
    for (reg_no = 0; reg_no < gen_reg_state.n_regs; reg_no++)
        gen_reg_state.reg_slots[reg_no] = 0xdeadbeef;

    register_set_reset(&xmm_reg_state.set);
    for (reg_no = 0; reg_no < xmm_reg_state.n_regs; reg_no++)
        xmm_reg_state.reg_slots[reg_no] = 0xdeadbeef;

    rsp_offs = 0;
}

/*
 * mark a given slot (as well as the register it resides in, if any) as no
 * longer being in use.
 */
static void discard_slot(struct code_block_x86_64 *blk, unsigned slot_no) {
    if (slot_no >= MAX_SLOTS)
        RAISE_ERROR(ERROR_TOO_BIG);
    struct slot *slot = slots + slot_no;
    if (!slot->in_use)
        RAISE_ERROR(ERROR_INTEGRITY);
    if (slot->in_reg) {
        register_discard(&slot->reg_state->set, slot->reg_no);
    } else {
        if (rsp_offs == slot->rbp_offs) {
            // TODO: add 8 to RSP and base_ptr_offs_next
        }
    }
    slot->in_use = false;
    slot->reg_state = NULL;
}

/*
 * whenever you emit a function call, call this function first.
 * This function will grab all volatile registers and emit code to make sure
 * they all get saved.
 */
static void prefunc(struct code_block_x86_64 *blk) {
    grab_register(&xmm_reg_state.set, XMM0);
    grab_register(&xmm_reg_state.set, XMM1);
    grab_register(&xmm_reg_state.set, XMM2);
    grab_register(&xmm_reg_state.set, XMM3);
    grab_register(&xmm_reg_state.set, XMM4);
    grab_register(&xmm_reg_state.set, XMM5);

#if defined(ABI_UNIX)
    // these regs are volatile on unix but not on microsoft
    grab_register(&xmm_reg_state.set, XMM6);
    grab_register(&xmm_reg_state.set, XMM7);
    grab_register(&xmm_reg_state.set, XMM8);
    grab_register(&xmm_reg_state.set, XMM9);
    grab_register(&xmm_reg_state.set, XMM10);
    grab_register(&xmm_reg_state.set, XMM11);
    grab_register(&xmm_reg_state.set, XMM12);
    grab_register(&xmm_reg_state.set, XMM13);
    grab_register(&xmm_reg_state.set, XMM14);
    grab_register(&xmm_reg_state.set, XMM15);
#endif

    evict_register(blk, &xmm_reg_state, XMM0);
    evict_register(blk, &xmm_reg_state, XMM1);
    evict_register(blk, &xmm_reg_state, XMM2);
    evict_register(blk, &xmm_reg_state, XMM3);
    evict_register(blk, &xmm_reg_state, XMM4);
    evict_register(blk, &xmm_reg_state, XMM5);

#if defined(ABI_UNIX)
    // these regs are volatile on unix but not on microsoft
    evict_register(blk, &xmm_reg_state, XMM6);
    evict_register(blk, &xmm_reg_state, XMM7);
    evict_register(blk, &xmm_reg_state, XMM8);
    evict_register(blk, &xmm_reg_state, XMM9);
    evict_register(blk, &xmm_reg_state, XMM10);
    evict_register(blk, &xmm_reg_state, XMM11);
    evict_register(blk, &xmm_reg_state, XMM12);
    evict_register(blk, &xmm_reg_state, XMM13);
    evict_register(blk, &xmm_reg_state, XMM14);
    evict_register(blk, &xmm_reg_state, XMM15);

    grab_register(&gen_reg_state.set, RAX);
    grab_register(&gen_reg_state.set, RCX);
    grab_register(&gen_reg_state.set, RDX);
    grab_register(&gen_reg_state.set, RSI);
    grab_register(&gen_reg_state.set, RDI);
    grab_register(&gen_reg_state.set, R8);
    grab_register(&gen_reg_state.set, R9);
    grab_register(&gen_reg_state.set, R10);
    grab_register(&gen_reg_state.set, R11);

    evict_register(blk, &gen_reg_state, RAX);
    evict_register(blk, &gen_reg_state, RCX);
    evict_register(blk, &gen_reg_state, RDX);
    evict_register(blk, &gen_reg_state, RSI);
    evict_register(blk, &gen_reg_state, RDI);
    evict_register(blk, &gen_reg_state, R8);
    evict_register(blk, &gen_reg_state, R9);
    evict_register(blk, &gen_reg_state, R10);
    evict_register(blk, &gen_reg_state, R11);
#elif defined(ABI_MICROSOFT)
    grab_register(&gen_reg_state.set, RAX);
    grab_register(&gen_reg_state.set, RCX);
    grab_register(&gen_reg_state.set, RDX);
    grab_register(&gen_reg_state.set, R8);
    grab_register(&gen_reg_state.set, R9);
    grab_register(&gen_reg_state.set, R10);
    grab_register(&gen_reg_state.set, R11);

    evict_register(blk, &gen_reg_state, RAX);
    evict_register(blk, &gen_reg_state, RCX);
    evict_register(blk, &gen_reg_state, RDX);
    evict_register(blk, &gen_reg_state, R8);
    evict_register(blk, &gen_reg_state, R9);
    evict_register(blk, &gen_reg_state, R10);
    evict_register(blk, &gen_reg_state, R11);
#else
    #error unknown ABI
#endif
}

/*
 * whenever you emit a function call, call this function after
 * really all it does is ungrab all the registers earlier grabbed by prefunc.
 *
 * Unless the function was returning a float, then call postfunc_float instead.
 *
 * It does not ungrab RAX even though that register is grabbed by prefunc.  The
 * reason for this is that RAX holds the return value (if any) and you probably
 * want to do something with that.  Functions that call postfunc will also need
 * to call ungrab_register(&gen_reg_state.set, RAX) afterwards when they no longer need that
 * register.
 */
static void postfunc(void) {
#if defined(ABI_UNIX)
    ungrab_register(&gen_reg_state.set, R11);
    ungrab_register(&gen_reg_state.set, R10);
    ungrab_register(&gen_reg_state.set, R9);
    ungrab_register(&gen_reg_state.set, R8);
    ungrab_register(&gen_reg_state.set, RDI);
    ungrab_register(&gen_reg_state.set, RSI);
    ungrab_register(&gen_reg_state.set, RDX);
    ungrab_register(&gen_reg_state.set, RCX);

    // these regs are volatile on unix but not on microsoft
    ungrab_register(&xmm_reg_state.set, XMM15);
    ungrab_register(&xmm_reg_state.set, XMM14);
    ungrab_register(&xmm_reg_state.set, XMM13);
    ungrab_register(&xmm_reg_state.set, XMM12);
    ungrab_register(&xmm_reg_state.set, XMM11);
    ungrab_register(&xmm_reg_state.set, XMM10);
    ungrab_register(&xmm_reg_state.set, XMM9);
    ungrab_register(&xmm_reg_state.set, XMM8);
    ungrab_register(&xmm_reg_state.set, XMM7);
    ungrab_register(&xmm_reg_state.set, XMM6);

#elif defined(ABI_MICROSOFT)
    ungrab_register(&gen_reg_state.set, R11);
    ungrab_register(&gen_reg_state.set, R10);
    ungrab_register(&gen_reg_state.set, R9);
    ungrab_register(&gen_reg_state.set, R8);
    ungrab_register(&gen_reg_state.set, RDX);
    ungrab_register(&gen_reg_state.set, RCX);

#else
    #error unknown ABI
#endif

    ungrab_register(&xmm_reg_state.set, XMM5);
    ungrab_register(&xmm_reg_state.set, XMM4);
    ungrab_register(&xmm_reg_state.set, XMM3);
    ungrab_register(&xmm_reg_state.set, XMM2);
    ungrab_register(&xmm_reg_state.set, XMM1);
    ungrab_register(&xmm_reg_state.set, XMM0);
}

/*
 * This is like postfunc, except it does ungrab RAX, and it doesn't ungrab XMM0.
 * This is intended to be called after functions that return floating points in
 * the XMM0 register.
 */
static void postfunc_float(void) {
#if defined(ABI_UNIX)
    ungrab_register(&gen_reg_state.set, R11);
    ungrab_register(&gen_reg_state.set, R10);
    ungrab_register(&gen_reg_state.set, R9);
    ungrab_register(&gen_reg_state.set, R8);
    ungrab_register(&gen_reg_state.set, RDI);
    ungrab_register(&gen_reg_state.set, RSI);
    ungrab_register(&gen_reg_state.set, RDX);
    ungrab_register(&gen_reg_state.set, RCX);
    ungrab_register(&gen_reg_state.set, RAX);

    // these regs are volatile on unix but not on microsoft
    ungrab_register(&xmm_reg_state.set, XMM15);
    ungrab_register(&xmm_reg_state.set, XMM14);
    ungrab_register(&xmm_reg_state.set, XMM13);
    ungrab_register(&xmm_reg_state.set, XMM12);
    ungrab_register(&xmm_reg_state.set, XMM11);
    ungrab_register(&xmm_reg_state.set, XMM10);
    ungrab_register(&xmm_reg_state.set, XMM9);
    ungrab_register(&xmm_reg_state.set, XMM8);
    ungrab_register(&xmm_reg_state.set, XMM7);
    ungrab_register(&xmm_reg_state.set, XMM6);

#elif defined(ABI_MICROSOFT)
    ungrab_register(&gen_reg_state.set, R11);
    ungrab_register(&gen_reg_state.set, R10);
    ungrab_register(&gen_reg_state.set, R9);
    ungrab_register(&gen_reg_state.set, R8);
    ungrab_register(&gen_reg_state.set, RDX);
    ungrab_register(&gen_reg_state.set, RCX);
    ungrab_register(&gen_reg_state.set, RAX);

#else
    #error unknown ABI
#endif

    ungrab_register(&xmm_reg_state.set, XMM5);
    ungrab_register(&xmm_reg_state.set, XMM4);
    ungrab_register(&xmm_reg_state.set, XMM3);
    ungrab_register(&xmm_reg_state.set, XMM2);
    ungrab_register(&xmm_reg_state.set, XMM1);
}

/*
 * move the given slot from a register to the stack.  As a precondition, the
 * slot must be in a register and the register it is in must not be grabbed.
 */
static void move_slot_to_stack(struct code_block_x86_64 *blk, unsigned slot_no) {
    if (slot_no >= MAX_SLOTS)
        RAISE_ERROR(ERROR_TOO_BIG);
    struct slot *slot = slots + slot_no;
    if (!slot->in_use || !slot->in_reg)
        RAISE_ERROR(ERROR_INTEGRITY);
    if (slot->reg_state->reg_slots[slot->reg_no] != slot_no)
        RAISE_ERROR(ERROR_INTEGRITY);

    int n_bytes;
    if (slot->reg_state == &gen_reg_state) {
        // handle general-purpose register
        n_bytes = 8;
        x86asm_pushq_reg64(slot->reg_no);
    } else if (slot->reg_state == &xmm_reg_state) {
        // handle SSE
        /*
         * TODO: if I ever use vector ops, then I need to save the whole
         * register
         */
        n_bytes = 4;
        x86asm_addq_imm8_reg(-4, RSP);
        x86asm_movss_xmm_indreg(slot->reg_no, RSP);
    } else {
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    register_discard(&slot->reg_state->set, slot->reg_no);

    slot->in_reg = false;
    rsp_offs -= n_bytes;
    slot->rbp_offs = rsp_offs;

    blk->dirty_stack = true;
}

/*
 * move the given slot into the given register.
 *
 * this function assumes that the register has already been allocated.
 * it will safely move any slots already in the register to the stack.
 */
static void
move_slot_to_reg(struct code_block_x86_64 *blk, unsigned slot_no, unsigned reg_no) {
    if (slot_no >= MAX_SLOTS)
        RAISE_ERROR(ERROR_TOO_BIG);
    struct slot *slot = slots + slot_no;
    if (!slot->in_use)
        RAISE_ERROR(ERROR_INTEGRITY);

    if (slot->in_reg) {
        unsigned src_reg = slot->reg_no;

        if (src_reg == reg_no)
            return; // nothing to do here

        if (register_in_use(&slot->reg_state->set, reg_no))
            move_slot_to_stack(blk, slot->reg_state->reg_slots[reg_no]);

        if (slot->reg_state == &gen_reg_state) {
            switch (slot->n_bytes) {
            case 4:
                x86asm_mov_reg32_reg32(src_reg, reg_no);
                break;
            case 8:
                x86asm_mov_reg64_reg64(src_reg, reg_no);
                break;
            default:
                error_set_length(slot->n_bytes);
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
            }
        } else if (slot->reg_state == &xmm_reg_state) {
            if (slot->n_bytes != 4)
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
            x86asm_movss_xmm_xmm(src_reg, reg_no);
        } else {
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        register_discard(&slot->reg_state->set, src_reg);
        register_acquire(&slot->reg_state->set, reg_no);
        slot->reg_state->reg_slots[reg_no] = slot_no;
        slots[slot_no].reg_no = reg_no;
        return;
    }

    if (register_in_use(&slot->reg_state->set, reg_no))
        move_slot_to_stack(blk, slot->reg_state->reg_slots[reg_no]);

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
        if (slot->reg_state == &gen_reg_state) {
            if (slot->rbp_offs > 127 || slot->rbp_offs < -128)
                x86asm_movq_disp32_reg_reg(slot->rbp_offs, RBP, reg_no);
            else
                x86asm_movq_disp8_reg_reg(slot->rbp_offs, RBP, reg_no);
        } else if (slot->reg_state == &xmm_reg_state) {
            if (slot->rbp_offs > 127 || slot->rbp_offs < -128)
                x86asm_movss_disp32_reg_xmm(slot->rbp_offs, RBP, reg_no);
            else
                x86asm_movss_disp8_reg_xmm(slot->rbp_offs, RBP, reg_no);
        } else {
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
    }

    register_acquire(&slot->reg_state->set, reg_no);
    slot->reg_state->reg_slots[reg_no] = slot_no;
    slot->reg_no = reg_no;
    slot->in_reg = true;
}

/*
 * returns true if the given jit_inst would emit a function call.
 * this is used only for optimisation purposes.
 */
static bool does_inst_emit_call(struct jit_inst const *inst) {
    return inst->op == JIT_OP_FALLBACK || inst->op == JIT_OP_CALL_FUNC ||
        (inst->op == JIT_OP_READ_16_CONSTADDR ||
         inst->op == JIT_OP_READ_32_CONSTADDR ||
         inst->op == JIT_OP_READ_8_SLOT ||
         inst->op == JIT_OP_READ_16_SLOT ||
         inst->op == JIT_OP_READ_32_SLOT ||
         inst->op == JIT_OP_READ_FLOAT_SLOT ||
         inst->op == JIT_OP_WRITE_8_SLOT ||
         inst->op == JIT_OP_WRITE_32_SLOT ||
         inst->op == JIT_OP_WRITE_FLOAT_SLOT);
}

static enum register_hint suggested_register_hints(struct il_code_block const *blk,
                                         unsigned slot_no,
                                         struct jit_inst const *inst) {
    unsigned beg = inst - blk->inst_list;
    unsigned end = jit_code_block_slot_lifespan(blk, slot_no, beg);

    enum register_hint hint = REGISTER_HINT_NONE;

    while (beg <= end) {
        struct jit_inst *cur_inst = blk->inst_list + beg;

        if (does_inst_emit_call(cur_inst))
            hint |= REGISTER_HINT_FUNCTION;

        if (cur_inst->op == JIT_OP_JUMP) {
            if (cur_inst->immed.jump.jmp_addr_slot == slot_no)
                hint |= REGISTER_HINT_JUMP_ADDR;
            if (cur_inst->immed.jump.jmp_hash_slot == slot_no)
                hint |= REGISTER_HINT_JUMP_HASH;
        }

        beg++;
    }
    return hint;
}

/*
 * call this function to send the given register's contents (if any) to the
 * stack.  Immediately after calling this, grab the register to prevent it from
 * being allocated, and subsequently ungrab it when finished.  The register's
 * contents are unchanged by this function.
 */
static void evict_register(struct code_block_x86_64 *blk,
                           struct register_state *reg_state, unsigned reg_no) {
    if (register_in_use(&reg_state->set, reg_no)) {
        // REGISTER_HINT_FUNCTION not necessary, just being used as a "default"
        int reg_dst = register_pick_unused(&reg_state->set, REGISTER_HINT_FUNCTION);
        if (reg_dst == reg_no)
            RAISE_ERROR(ERROR_INTEGRITY);

        if (reg_dst >= 0)
            move_slot_to_reg(blk, reg_state->reg_slots[reg_no], reg_dst);
        else
            move_slot_to_stack(blk, reg_state->reg_slots[reg_no]);

        /*
         * move_slot_to_stack and move_slot_to_reg will both discard the
         * register
         */
    }
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
static void grab_slot(struct code_block_x86_64 *blk,
                      struct il_code_block const *il_blk,
                      struct jit_inst const *inst,
                      struct register_state *reg_state,
                      unsigned slot_no, unsigned n_bytes) {
    if (slot_no >= MAX_SLOTS)
        RAISE_ERROR(ERROR_TOO_BIG);
    struct slot *slot = slots + slot_no;

    if (slot->in_use) {
        if (slot->reg_state != reg_state)
            RAISE_ERROR(ERROR_INTEGRITY);

        if (slot->n_bytes != n_bytes)
            RAISE_ERROR(ERROR_INTEGRITY);

        if (slot->in_reg) {
            if (register_grabbed(&slot->reg_state->set, slot->reg_no))
                return;
            else
                goto mark_grabbed;
        }

        unsigned reg_no = register_pick(&slot->reg_state->set, suggested_register_hints(il_blk, slot_no, inst));
        move_slot_to_reg(blk, slot_no, reg_no);
        goto mark_grabbed;
    } else {
        unsigned reg_no = register_pick(&reg_state->set, suggested_register_hints(il_blk, slot_no, inst));
        if (register_in_use(&reg_state->set, reg_no))
            move_slot_to_stack(blk, reg_state->reg_slots[reg_no]);
        register_acquire(&reg_state->set, reg_no);
        reg_state->reg_slots[reg_no] = slot_no;
        slot->in_use = true;
        slot->reg_no = reg_no;
        slot->reg_state = reg_state;
        slot->in_reg = true;
        slot->n_bytes = n_bytes;
        goto mark_grabbed;
    }

mark_grabbed:
    grab_register(&slot->reg_state->set, slot->reg_no);
}

static void ungrab_slot(unsigned slot_no) {
    if (slot_no >= MAX_SLOTS)
        RAISE_ERROR(ERROR_TOO_BIG);
    struct slot *slot = slots + slot_no;
    if (slot->in_reg)
        ungrab_register(&slot->reg_state->set, slot->reg_no);
    else
        RAISE_ERROR(ERROR_INTEGRITY);
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
    blk->exec_mem_alloc_start = native;
}

void code_block_x86_64_cleanup(struct code_block_x86_64 *blk) {
    exec_mem_free(blk->exec_mem_alloc_start);
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
static void emit_fallback(struct code_block_x86_64 *blk,
                          struct il_code_block const *il_blk,
                          void *cpu, struct jit_inst const *inst) {
    cpu_inst_param inst_bin = inst->immed.fallback.inst;

    prefunc(blk);

    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)cpu, REG_ARG0);
    x86asm_mov_imm32_reg32(inst_bin, REG_ARG1);

    ms_shadow_open(blk);
    x86_64_align_stack(blk);
    x86asm_call_ptr(inst->immed.fallback.fallback_fn);
    ms_shadow_close();

    postfunc();
    ungrab_register(&gen_reg_state.set, REG_RET);
}

// JIT_OP_JUMP implementation
static void emit_jump(struct code_block_x86_64 *blk,
                      struct il_code_block const *il_blk,
                      void *cpu, struct jit_inst const *inst) {
    unsigned jmp_addr_slot = inst->immed.jump.jmp_addr_slot;
    unsigned jmp_hash_slot = inst->immed.jump.jmp_hash_slot;

    grab_register(&gen_reg_state.set, NATIVE_DISPATCH_PC_REG);
    grab_register(&gen_reg_state.set, NATIVE_DISPATCH_HASH_REG);

    move_slot_to_reg(blk, jmp_addr_slot, NATIVE_DISPATCH_PC_REG);
    move_slot_to_reg(blk, jmp_hash_slot, NATIVE_DISPATCH_HASH_REG);
}

static void emit_cset(struct code_block_x86_64 *blk,
                      struct il_code_block const *il_blk,
                      void *cpu, struct jit_inst const *inst) {
    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);
    uint32_t src_val = inst->immed.cset.src_val;
    unsigned dst_slot = inst->immed.cset.dst_slot;
    unsigned flag_slot = inst->immed.cset.flag_slot;

    grab_slot(blk, il_blk, inst, &gen_reg_state, flag_slot, 4);

    unsigned flag_reg = slots[flag_slot].reg_no;

    x86asm_testl_imm32_reg32(1, flag_reg);
    if (inst->immed.cset.t_flag)
        x86asm_jz_lbl8(&lbl);
    else
        x86asm_jnz_lbl8(&lbl);

    grab_slot(blk, il_blk, inst, &gen_reg_state, dst_slot, 4);
    unsigned dst_reg = slots[dst_slot].reg_no;
    x86asm_mov_imm32_reg32(src_val, dst_reg);
    ungrab_slot(dst_slot);

    x86asm_lbl8_define(&lbl);

    ungrab_slot(flag_slot);
    x86asm_lbl8_cleanup(&lbl);
}

// JIT_SET_SLOT implementation
static void emit_set_slot(struct code_block_x86_64 *blk,
                          struct il_code_block const *il_blk,
                          void *cpu, struct jit_inst const *inst) {
    unsigned slot_idx = inst->immed.set_slot.slot_idx;
    uint32_t new_val = inst->immed.set_slot.new_val;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_idx, 4);
    x86asm_mov_imm32_reg32(new_val, slots[slot_idx].reg_no);
    ungrab_slot(slot_idx);
}

// JIT_SET_SLOT_HOST_PTR implementation
static void emit_set_slot_host_ptr(struct code_block_x86_64 *blk,
                                   struct il_code_block const *il_blk,
                                   void *cpu, struct jit_inst const *inst) {
    unsigned slot_idx = inst->immed.set_slot_host_ptr.slot_idx;
    uintptr_t new_val = (uintptr_t)inst->immed.set_slot_host_ptr.ptr;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_idx, 8);
    x86asm_mov_imm64_reg64(new_val, slots[slot_idx].reg_no);
    ungrab_slot(slot_idx);
}

// JIT_OP_CALL_FUNC implementation
static void emit_call_func(struct code_block_x86_64 *blk,
                           struct il_code_block const *il_blk,
                           void *cpu, struct jit_inst const *inst) {
    prefunc(blk);

    // now call sh4_on_sr_change(cpu, old_sr)
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)cpu, REG_ARG0);
    move_slot_to_reg(blk, inst->immed.call_func.slot_no, REG_ARG1);

    evict_register(blk, &gen_reg_state, REG_ARG1); // TODO: is this necessary ?

    ms_shadow_open(blk);
    x86_64_align_stack(blk);
    x86asm_call_ptr(inst->immed.call_func.func);
    ms_shadow_close();

    postfunc();
    ungrab_register(&gen_reg_state.set, REG_RET);
}

// JIT_OP_READ_16_CONSTADDR implementation
static void emit_read_16_constaddr(struct code_block_x86_64 *blk,
                                   struct il_code_block const *il_blk,
                                   void *cpu, struct jit_inst const *inst) {
    addr32_t vaddr = inst->immed.read_16_constaddr.addr;
    unsigned slot_no = inst->immed.read_16_constaddr.slot_no;
    struct memory_map const *map = inst->immed.read_16_constaddr.map;

    // call memory_map_read_16(vaddr)
    prefunc(blk);

    if (config_get_inline_mem()) {
        x86asm_mov_imm32_reg32(vaddr, REG_ARG0);
        native_mem_read_16(blk, map);
    } else {
        x86asm_mov_imm64_reg64((uint64_t)map, REG_ARG0);
        x86asm_mov_imm32_reg32(vaddr, REG_ARG1);
        ms_shadow_open(blk);
        x86_64_align_stack(blk);
        x86asm_call_ptr(memory_map_read_16);
        ms_shadow_close();
    }

    postfunc();

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_no, 4);
    x86asm_mov_reg32_reg32(REG_RET, slots[slot_no].reg_no);

    ungrab_register(&gen_reg_state.set, REG_RET);
    ungrab_slot(slot_no);
}

// JIT_OP_SIGN_EXTEND_8 implementation
static void emit_sign_extend_8(struct code_block_x86_64 *blk,
                                struct il_code_block const *il_blk,
                                void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.sign_extend_8.slot_no;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_no, 4);

    unsigned reg_no = slots[slot_no].reg_no;
    x86asm_movsx_reg8_reg32(reg_no, reg_no);

    ungrab_slot(slot_no);
}

// JIT_OP_SIGN_EXTEND_16 implementation
static void emit_sign_extend_16(struct code_block_x86_64 *blk,
                                struct il_code_block const *il_blk,
                                void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.sign_extend_16.slot_no;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_no, 4);

    unsigned reg_no = slots[slot_no].reg_no;
    x86asm_movsx_reg16_reg32(reg_no, reg_no);

    ungrab_slot(slot_no);
}

// JIT_OP_READ_32_CONSTADDR implementation
static void emit_read_32_constaddr(struct code_block_x86_64 *blk,
                                   struct il_code_block const *il_blk,
                                   void *cpu, struct jit_inst const *inst) {
    addr32_t vaddr = inst->immed.read_32_constaddr.addr;
    unsigned slot_no = inst->immed.read_32_constaddr.slot_no;
    struct memory_map const *map = inst->immed.read_32_constaddr.map;

    // call memory_map_read_32(vaddr)

    prefunc(blk);

    if (config_get_inline_mem()) {
        x86asm_mov_imm32_reg32(vaddr, REG_ARG0);
        native_mem_read_32(blk, map);
    } else {
        x86asm_mov_imm64_reg64((uint64_t)map, REG_ARG0);
        x86asm_mov_imm32_reg32(vaddr, REG_ARG1);
        ms_shadow_open(blk);
        x86_64_align_stack(blk);
        x86asm_call_ptr(memory_map_read_32);
        ms_shadow_close();
    }

    postfunc();

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_no, 4);
    x86asm_mov_reg32_reg32(REG_RET, slots[slot_no].reg_no);

    ungrab_slot(slot_no);
    ungrab_register(&gen_reg_state.set, REG_RET);
}

// JIT_OP_READ_8_SLOT implementation
static void emit_read_8_slot(struct code_block_x86_64 *blk,
                             struct il_code_block const *il_blk,
                             void *cpu, struct jit_inst const *inst) {
    unsigned dst_slot = inst->immed.read_8_slot.dst_slot;
    unsigned addr_slot = inst->immed.read_8_slot.addr_slot;
    struct memory_map const *map = inst->immed.read_8_slot.map;

    // call memory_map_read_8(*addr_slot)
    prefunc(blk);

    if (config_get_inline_mem()) {
        move_slot_to_reg(blk, addr_slot, REG_ARG0);
        evict_register(blk, &gen_reg_state, REG_ARG0);
        native_mem_read_8(blk, map);
    } else {
        x86asm_mov_imm64_reg64((uint64_t)map, REG_ARG0);
        move_slot_to_reg(blk, addr_slot, REG_ARG1);
        evict_register(blk, &gen_reg_state, REG_ARG1);
        ms_shadow_open(blk);
        x86_64_align_stack(blk);
        x86asm_call_ptr(memory_map_read_8);
        ms_shadow_close();
    }

    postfunc();

    grab_slot(blk, il_blk, inst, &gen_reg_state, dst_slot, 4);
    x86asm_mov_reg32_reg32(REG_RET, slots[dst_slot].reg_no);

    ungrab_slot(dst_slot);
    ungrab_register(&gen_reg_state.set, REG_RET);
}

// JIT_OP_READ_16_SLOT implementation
static void emit_read_16_slot(struct code_block_x86_64 *blk,
                              struct il_code_block const *il_blk,
                              void *cpu, struct jit_inst const *inst) {
    unsigned dst_slot = inst->immed.read_16_slot.dst_slot;
    unsigned addr_slot = inst->immed.read_16_slot.addr_slot;
    struct memory_map const *map = inst->immed.read_16_slot.map;

    // call memory_map_read_16(*addr_slot)
    prefunc(blk);

    if (config_get_inline_mem()) {
        move_slot_to_reg(blk, addr_slot, REG_ARG0);
        evict_register(blk, &gen_reg_state, REG_ARG0);
        native_mem_read_16(blk, map);
    } else {
        x86asm_mov_imm64_reg64((uint64_t)map, REG_ARG0);
        move_slot_to_reg(blk, addr_slot, REG_ARG1);
        evict_register(blk, &gen_reg_state, REG_ARG1);
        ms_shadow_open(blk);
        x86_64_align_stack(blk);
        x86asm_call_ptr(memory_map_read_16);
        ms_shadow_close();
    }

    postfunc();

    grab_slot(blk, il_blk, inst, &gen_reg_state, dst_slot, 4);
    x86asm_mov_reg32_reg32(REG_RET, slots[dst_slot].reg_no);

    ungrab_slot(dst_slot);
    ungrab_register(&gen_reg_state.set, REG_RET);
}

// JIT_OP_READ_32_SLOT implementation
static void emit_read_32_slot(struct code_block_x86_64 *blk,
                              struct il_code_block const *il_blk,
                              void *cpu, struct jit_inst const *inst) {
    unsigned dst_slot = inst->immed.read_32_slot.dst_slot;
    unsigned addr_slot = inst->immed.read_32_slot.addr_slot;
    struct memory_map const *map = inst->immed.read_32_slot.map;

    // call memory_map_read_32(*addr_slot)
    prefunc(blk);

    if (config_get_inline_mem()) {
        move_slot_to_reg(blk, addr_slot, REG_ARG0);
        evict_register(blk, &gen_reg_state, REG_ARG0);
        native_mem_read_32(blk, map);
    } else {
        x86asm_mov_imm64_reg64((uint64_t)map, REG_ARG0);
        move_slot_to_reg(blk, addr_slot, REG_ARG1);
        evict_register(blk, &gen_reg_state, REG_ARG1);
        ms_shadow_open(blk);
        x86_64_align_stack(blk);
        x86asm_call_ptr(memory_map_read_32);
        ms_shadow_close();
    }

    postfunc();

    grab_slot(blk, il_blk, inst, &gen_reg_state, dst_slot, 4);
    x86asm_mov_reg32_reg32(REG_RET, slots[dst_slot].reg_no);

    ungrab_slot(dst_slot);
    ungrab_register(&gen_reg_state.set, REG_RET);
}

// JIT_OP_WRITE_8_SLOT implementation
static void emit_write_8_slot(struct code_block_x86_64 *blk,
                               struct il_code_block const *il_blk,
                               void *cpu, struct jit_inst const *inst) {
    unsigned src_slot = inst->immed.write_8_slot.src_slot;
    unsigned addr_slot = inst->immed.write_8_slot.addr_slot;
    struct memory_map const *map = inst->immed.write_8_slot.map;

    prefunc(blk);

    if (config_get_inline_mem()) {
        move_slot_to_reg(blk, addr_slot, REG_ARG0);
        move_slot_to_reg(blk, src_slot, REG_ARG1);

        evict_register(blk, &gen_reg_state, REG_ARG0);
        evict_register(blk, &gen_reg_state, REG_ARG1);

        native_mem_write_8(blk, map);
    } else {
        move_slot_to_reg(blk, addr_slot, REG_ARG1);
        move_slot_to_reg(blk, src_slot, REG_ARG2);

        evict_register(blk, &gen_reg_state, REG_ARG1);
        evict_register(blk, &gen_reg_state, REG_ARG2);

        x86asm_mov_imm64_reg64((uint64_t)map, REG_ARG0);
        ms_shadow_open(blk);
        x86_64_align_stack(blk);
        x86asm_call_ptr(memory_map_write_8);
        ms_shadow_close();
    }

    postfunc();

    ungrab_register(&gen_reg_state.set, REG_RET);
}

// JIT_OP_WRITE_32_SLOT implementation
static void emit_write_32_slot(struct code_block_x86_64 *blk,
                               struct il_code_block const *il_blk,
                               void *cpu, struct jit_inst const *inst) {
    unsigned src_slot = inst->immed.write_32_slot.src_slot;
    unsigned addr_slot = inst->immed.write_32_slot.addr_slot;
    struct memory_map const *map = inst->immed.write_32_slot.map;

    prefunc(blk);

    if (config_get_inline_mem()) {
        move_slot_to_reg(blk, addr_slot, REG_ARG0);
        move_slot_to_reg(blk, src_slot, REG_ARG1);

        evict_register(blk, &gen_reg_state, REG_ARG0);
        evict_register(blk, &gen_reg_state, REG_ARG1);

        native_mem_write_32(blk, map);
    } else {
        move_slot_to_reg(blk, addr_slot, REG_ARG1);
        move_slot_to_reg(blk, src_slot, REG_ARG2);

        evict_register(blk, &gen_reg_state, REG_ARG1);
        evict_register(blk, &gen_reg_state, REG_ARG2);

        x86asm_mov_imm64_reg64((uint64_t)map, REG_ARG0);
        ms_shadow_open(blk);
        x86_64_align_stack(blk);
        x86asm_call_ptr(memory_map_write_32);
        ms_shadow_close();
    }

    postfunc();

    ungrab_register(&gen_reg_state.set, REG_RET);
}

// JIT_OP_WRITE_FLOAT_SLOT implementation
static void emit_write_float_slot(struct code_block_x86_64 *blk,
                                  struct il_code_block const *il_blk,
                                  void *cpu, struct jit_inst const *inst) {
    unsigned src_slot = inst->immed.write_float_slot.src_slot;
    unsigned addr_slot = inst->immed.write_float_slot.addr_slot;
    struct memory_map const *map = inst->immed.write_float_slot.map;

    prefunc(blk);

    if (config_get_inline_mem()) {
        move_slot_to_reg(blk, addr_slot, REG_ARG0);
        move_slot_to_reg(blk, src_slot, REG_ARG0_XMM);

        evict_register(blk, &gen_reg_state, REG_ARG0);
        evict_register(blk, &gen_reg_state, REG_ARG0_XMM);

        native_mem_write_float(blk, map);
    } else {
        move_slot_to_reg(blk, addr_slot, REG_ARG1);
        move_slot_to_reg(blk, src_slot, REG_ARG0_XMM);

        evict_register(blk, &gen_reg_state, REG_ARG1);
        evict_register(blk, &xmm_reg_state, REG_ARG0_XMM);

        x86asm_mov_imm64_reg64((uint64_t)map, REG_ARG0);
        ms_shadow_open(blk);
        x86_64_align_stack(blk);
        x86asm_call_ptr(memory_map_write_float);
        ms_shadow_close();
    }

    postfunc();
    ungrab_register(&gen_reg_state.set, REG_RET);
}

static void
emit_load_slot16(struct code_block_x86_64 *blk,
                 struct il_code_block const *il_blk,
                 void *cpu, struct jit_inst const* inst) {
    unsigned slot_no = inst->immed.load_slot16.slot_no;
    void const *src_ptr = inst->immed.load_slot16.src;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_no, 4);

    unsigned reg_no = slots[slot_no].reg_no;

    x86asm_mov_imm64_reg64((uintptr_t)src_ptr, reg_no);
    x86asm_movzxw_indreg_reg(reg_no, reg_no);

    ungrab_slot(slot_no);
}

static void
emit_load_slot(struct code_block_x86_64 *blk,
               struct il_code_block const *il_blk,
               void *cpu, struct jit_inst const* inst) {
    unsigned slot_no = inst->immed.load_slot.slot_no;
    void const *src_ptr = inst->immed.load_slot.src;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_no, 4);

    unsigned reg_no = slots[slot_no].reg_no;

    x86asm_mov_imm64_reg64((uintptr_t)src_ptr, reg_no);
    x86asm_mov_indreg32_reg32(reg_no, reg_no);

    ungrab_slot(slot_no);
}

static void emit_load_slot_indexed(struct code_block_x86_64 *blk,
                                   struct il_code_block const *il_blk,
                                   void *cpu, struct jit_inst const* inst) {
    unsigned slot_base = inst->immed.load_slot_indexed.slot_base;
    unsigned slot_dst = inst->immed.load_slot_indexed.slot_dst;
    unsigned index = inst->immed.load_slot_indexed.index;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_base, 8);
    if (slot_base != slot_dst)
        grab_slot(blk, il_blk, inst, &gen_reg_state, slot_dst, 4);

    unsigned reg_base = slots[slot_base].reg_no;
    unsigned reg_dst = slots[slot_dst].reg_no;
    int disp_bytes = 4 * index;

    if (disp_bytes <= 127 && disp_bytes >= - 128)
        x86asm_movl_disp8_reg_reg(disp_bytes, reg_base, reg_dst);
    else
        x86asm_movl_disp32_reg_reg(disp_bytes, reg_base, reg_dst);

    if (slot_base == slot_dst)
        slots[slot_base].n_bytes = 4;
    else
        ungrab_slot(slot_dst);
    ungrab_slot(slot_base);
}

static void
emit_store_slot(struct code_block_x86_64 *blk,
                struct il_code_block const *il_blk,
                void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.store_slot.slot_no;
    void const *dst_ptr = inst->immed.store_slot.dst;

    evict_register(blk, &gen_reg_state, REG_RET);
    grab_register(&gen_reg_state.set, REG_RET);
    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_no, 4);

    unsigned reg_no = slots[slot_no].reg_no;
    x86asm_mov_imm64_reg64((uintptr_t)dst_ptr, REG_RET);
    x86asm_mov_reg32_indreg32(reg_no, REG_RET);

    ungrab_slot(slot_no);
    ungrab_register(&gen_reg_state.set, REG_RET);
}

static void
emit_store_slot_indexed(struct code_block_x86_64 *blk,
                        struct il_code_block const *il_blk,
                        void *cpu, struct jit_inst const *inst) {
    unsigned slot_base = inst->immed.store_slot_indexed.slot_base;
    unsigned slot_src = inst->immed.store_slot_indexed.slot_src;
    unsigned index = inst->immed.store_slot_indexed.index;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_base, 8);
    if (slot_base != slot_src)
        grab_slot(blk, il_blk, inst, &gen_reg_state, slot_src, 4);

    unsigned reg_base = slots[slot_base].reg_no;
    unsigned reg_src = slots[slot_src].reg_no;
    int disp_bytes = 4 * index;

    if (disp_bytes <= 127 && disp_bytes >= - 128)
        x86asm_movl_reg_disp8_reg(reg_src, disp_bytes, reg_base);
    else
        x86asm_movl_reg_disp32_reg(reg_src, disp_bytes, reg_base);

    if (slot_base != slot_src)
        ungrab_slot(slot_src);
    ungrab_slot(slot_base);
}

static void
emit_add(struct code_block_x86_64 *blk,
         struct il_code_block const *il_blk,
         void *cpu, struct jit_inst const *inst) {
    unsigned slot_src = inst->immed.add.slot_src;
    unsigned slot_dst = inst->immed.add.slot_dst;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_src, 4);
    if (slot_src != slot_dst)
        grab_slot(blk, il_blk, inst, &gen_reg_state, slot_dst, 4);

    x86asm_addl_reg32_reg32(slots[slot_src].reg_no, slots[slot_dst].reg_no);

    if (slot_src != slot_dst)
        ungrab_slot(slot_dst);
    ungrab_slot(slot_src);
}

static void
emit_sub(struct code_block_x86_64 *blk,
         struct il_code_block const *il_blk,
         void *cpu, struct jit_inst const *inst) {
    unsigned slot_src = inst->immed.add.slot_src;
    unsigned slot_dst = inst->immed.add.slot_dst;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_src, 4);
    if (slot_src != slot_dst)
        grab_slot(blk, il_blk, inst, &gen_reg_state, slot_dst, 4);

    x86asm_subl_reg32_reg32(slots[slot_src].reg_no, slots[slot_dst].reg_no);

    if (slot_src != slot_dst)
        ungrab_slot(slot_dst);
    ungrab_slot(slot_src);
}

static void
emit_add_const32(struct code_block_x86_64 *blk,
                 struct il_code_block const *il_blk,
                 void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.add_const32.slot_dst;
    uint32_t const_val = inst->immed.add_const32.const32;

    evict_register(blk, &gen_reg_state, REG_RET);
    grab_register(&gen_reg_state.set, REG_RET);

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_no, 4);

    unsigned reg_no = slots[slot_no].reg_no;

    x86asm_mov_reg32_reg32(reg_no, REG_RET);
    x86asm_add_imm32_eax(const_val);
    x86asm_mov_reg32_reg32(REG_RET, reg_no);

    ungrab_slot(slot_no);
    ungrab_register(&gen_reg_state.set, REG_RET);
}

static void
emit_xor(struct code_block_x86_64 *blk,
         struct il_code_block const *il_blk,
         void *cpu, struct jit_inst const *inst) {
    unsigned slot_src = inst->immed.xor.slot_src;
    unsigned slot_dst = inst->immed.xor.slot_dst;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_src, 4);
    if (slot_src != slot_dst)
        grab_slot(blk, il_blk, inst, &gen_reg_state, slot_dst, 4);

    x86asm_xorl_reg32_reg32(slots[slot_src].reg_no, slots[slot_dst].reg_no);

    if (slot_src != slot_dst)
        ungrab_slot(slot_dst);
    ungrab_slot(slot_src);
}

static void
emit_mov(struct code_block_x86_64 *blk,
         struct il_code_block const *il_blk,
         void *cpu, struct jit_inst const *inst) {
    unsigned slot_src = inst->immed.mov.slot_src;
    unsigned slot_dst = inst->immed.mov.slot_dst;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_src, 4);
    if (slot_src != slot_dst)
        grab_slot(blk, il_blk, inst, &gen_reg_state, slot_dst, 4);

    x86asm_mov_reg32_reg32(slots[slot_src].reg_no, slots[slot_dst].reg_no);

    if (slot_src != slot_dst)
        ungrab_slot(slot_dst);
    ungrab_slot(slot_src);
}

static void
emit_mov_float(struct code_block_x86_64 *blk,
               struct il_code_block const *il_blk,
               void *cpu, struct jit_inst const *inst) {
    unsigned slot_src = inst->immed.mov_float.slot_src;
    unsigned slot_dst = inst->immed.mov_float.slot_dst;

    grab_slot(blk, il_blk, inst, &xmm_reg_state, slot_src, 4);
    if (slot_src != slot_dst)
        grab_slot(blk, il_blk, inst, &xmm_reg_state, slot_dst, 4);

    x86asm_movss_xmm_xmm(slots[slot_src].reg_no, slots[slot_dst].reg_no);

    if (slot_src != slot_dst)
        ungrab_slot(slot_dst);
    ungrab_slot(slot_src);
}

static void
emit_and(struct code_block_x86_64 *blk,
         struct il_code_block const *il_blk,
         void *cpu, struct jit_inst const *inst) {
    unsigned slot_src = inst->immed.and.slot_src;
    unsigned slot_dst = inst->immed.and.slot_dst;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_src, 4);
    if (slot_src != slot_dst)
        grab_slot(blk, il_blk, inst, &gen_reg_state, slot_dst, 4);

    x86asm_andl_reg32_reg32(slots[slot_src].reg_no, slots[slot_dst].reg_no);

    if (slot_src != slot_dst)
        ungrab_slot(slot_dst);
    ungrab_slot(slot_src);
}

static void
emit_and_const32(struct code_block_x86_64 *blk,
                 struct il_code_block const *il_blk,
                 void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.and_const32.slot_no;
    unsigned const32 = inst->immed.and_const32.const32;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_no, 4);

    x86asm_andl_imm32_reg32(const32, slots[slot_no].reg_no);

    ungrab_slot(slot_no);
}

static void
emit_or(struct code_block_x86_64 *blk,
        struct il_code_block const *il_blk,
        void *cpu, struct jit_inst const *inst) {
    unsigned slot_src = inst->immed.or.slot_src;
    unsigned slot_dst = inst->immed.or.slot_dst;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_src, 4);
    if (slot_src != slot_dst)
        grab_slot(blk, il_blk, inst, &gen_reg_state, slot_dst, 4);

    x86asm_orl_reg32_reg32(slots[slot_src].reg_no, slots[slot_dst].reg_no);

    if (slot_src != slot_dst)
        ungrab_slot(slot_dst);
    ungrab_slot(slot_src);
}

static void
emit_or_const32(struct code_block_x86_64 *blk,
                struct il_code_block const *il_blk,
                void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.or_const32.slot_no;
    unsigned const32 = inst->immed.or_const32.const32;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_no, 4);

    x86asm_orl_imm32_reg32(const32, slots[slot_no].reg_no);

    ungrab_slot(slot_no);
}

static void
emit_xor_const32(struct code_block_x86_64 *blk,
                 struct il_code_block const *il_blk,
                 void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.xor_const32.slot_no;
    unsigned const32 = inst->immed.xor_const32.const32;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_no, 4);

    x86asm_xorl_imm32_reg32(const32, slots[slot_no].reg_no);

    ungrab_slot(slot_no);
}

static void
emit_slot_to_bool(struct code_block_x86_64 *blk,
                  struct il_code_block const *il_blk,
                  void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.slot_to_bool.slot_no;

    evict_register(blk, &gen_reg_state, REG_RET);
    grab_register(&gen_reg_state.set, REG_RET);
    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_no, 4);

    x86asm_xorl_reg32_reg32(REG_RET, REG_RET);
    x86asm_testl_reg32_reg32(slots[slot_no].reg_no, slots[slot_no].reg_no);
    x86asm_setnzl_reg32(REG_RET);

    x86asm_mov_reg32_reg32(REG_RET, slots[slot_no].reg_no);

    ungrab_slot(slot_no);
    ungrab_register(&gen_reg_state.set, REG_RET);
}

static void
emit_not(struct code_block_x86_64 *blk,
         struct il_code_block const *il_blk,
         void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.not.slot_no;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_no, 4);

    x86asm_notl_reg32(slots[slot_no].reg_no);

    ungrab_slot(slot_no);
}

static void
emit_shll(struct code_block_x86_64 *blk,
          struct il_code_block const *il_blk,
          void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.shll.slot_no;
    unsigned shift_amt = inst->immed.shll.shift_amt;

    if (shift_amt >= 32)
        shift_amt = 32;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_no, 4);
    x86asm_shll_imm8_reg32(shift_amt, slots[slot_no].reg_no);
    ungrab_slot(slot_no);
}

static void
emit_shar(struct code_block_x86_64 *blk,
          struct il_code_block const *il_blk,
          void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.shar.slot_no;
    unsigned shift_amt = inst->immed.shar.shift_amt;

    if (shift_amt >= 32)
        shift_amt = 32;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_no, 4);
    x86asm_sarl_imm8_reg32(shift_amt, slots[slot_no].reg_no);
    ungrab_slot(slot_no);
}

static void
emit_shlr(struct code_block_x86_64 *blk,
          struct il_code_block const *il_blk,
          void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.shlr.slot_no;
    unsigned shift_amt = inst->immed.shlr.shift_amt;

    if (shift_amt >= 32)
        shift_amt = 32;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_no, 4);
    x86asm_shrl_imm8_reg32(shift_amt, slots[slot_no].reg_no);
    ungrab_slot(slot_no);
}

static void emit_set_gt_unsigned(struct code_block_x86_64 *blk,
                                 struct il_code_block const *il_blk,
                                 void *cpu, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.set_gt_unsigned.slot_lhs;
    unsigned slot_rhs = inst->immed.set_gt_unsigned.slot_rhs;
    unsigned slot_dst = inst->immed.set_gt_unsigned.slot_dst;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_lhs, 4);
    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_rhs, 4);
    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_dst, 4);

    x86asm_cmpl_reg32_reg32(slots[slot_rhs].reg_no, slots[slot_lhs].reg_no);
    x86asm_jbe_lbl8(&lbl);
    x86asm_orl_imm32_reg32(1, slots[slot_dst].reg_no);
    x86asm_lbl8_define(&lbl);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_rhs);
    ungrab_slot(slot_lhs);

    x86asm_lbl8_cleanup(&lbl);
}

static void emit_set_gt_signed(struct code_block_x86_64 *blk,
                               struct il_code_block const *il_blk,
                               void *cpu, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.set_gt_signed.slot_lhs;
    unsigned slot_rhs = inst->immed.set_gt_signed.slot_rhs;
    unsigned slot_dst = inst->immed.set_gt_signed.slot_dst;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_lhs, 4);
    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_rhs, 4);
    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_dst, 4);

    x86asm_cmpl_reg32_reg32(slots[slot_rhs].reg_no, slots[slot_lhs].reg_no);
    x86asm_jle_lbl8(&lbl);
    x86asm_orl_imm32_reg32(1, slots[slot_dst].reg_no);
    x86asm_lbl8_define(&lbl);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_rhs);
    ungrab_slot(slot_lhs);

    x86asm_lbl8_cleanup(&lbl);
}

static void emit_set_gt_signed_const(struct code_block_x86_64 *blk,
                                     struct il_code_block const *il_blk,
                                     void *cpu, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.set_gt_signed_const.slot_lhs;
    unsigned imm_rhs = inst->immed.set_gt_signed_const.imm_rhs;
    unsigned slot_dst = inst->immed.set_gt_signed_const.slot_dst;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_lhs, 4);
    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_dst, 4);

    x86asm_cmpl_imm8_reg32(imm_rhs, slots[slot_lhs].reg_no);
    x86asm_jle_lbl8(&lbl);
    x86asm_orl_imm32_reg32(1, slots[slot_dst].reg_no);
    x86asm_lbl8_define(&lbl);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_lhs);

    x86asm_lbl8_cleanup(&lbl);
}

static void emit_set_ge_signed_const(struct code_block_x86_64 *blk,
                                     struct il_code_block const *il_blk,
                                     void *cpu, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.set_ge_signed_const.slot_lhs;
    unsigned imm_rhs = inst->immed.set_ge_signed_const.imm_rhs;
    unsigned slot_dst = inst->immed.set_ge_signed_const.slot_dst;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_lhs, 4);
    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_dst, 4);

    x86asm_cmpl_imm8_reg32(imm_rhs, slots[slot_lhs].reg_no);
    x86asm_jl_lbl8(&lbl);
    x86asm_orl_imm32_reg32(1, slots[slot_dst].reg_no);
    x86asm_lbl8_define(&lbl);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_lhs);

    x86asm_lbl8_cleanup(&lbl);
}

static void emit_set_eq(struct code_block_x86_64 *blk,
                        struct il_code_block const *il_blk,
                        void *cpu, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.set_eq.slot_lhs;
    unsigned slot_rhs = inst->immed.set_eq.slot_rhs;
    unsigned slot_dst = inst->immed.set_eq.slot_dst;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_lhs, 4);
    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_rhs, 4);
    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_dst, 4);

    x86asm_cmpl_reg32_reg32(slots[slot_rhs].reg_no, slots[slot_lhs].reg_no);
    x86asm_jnz_lbl8(&lbl);

    x86asm_orl_imm32_reg32(1, slots[slot_dst].reg_no);
    x86asm_lbl8_define(&lbl);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_rhs);
    ungrab_slot(slot_lhs);

    x86asm_lbl8_cleanup(&lbl);
}

static void emit_set_ge_unsigned(struct code_block_x86_64 *blk,
                                 struct il_code_block const *il_blk,
                                 void *cpu, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.set_ge_unsigned.slot_lhs;
    unsigned slot_rhs = inst->immed.set_ge_unsigned.slot_rhs;
    unsigned slot_dst = inst->immed.set_ge_unsigned.slot_dst;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_lhs, 4);
    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_rhs, 4);
    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_dst, 4);

    x86asm_cmpl_reg32_reg32(slots[slot_rhs].reg_no, slots[slot_lhs].reg_no);
    x86asm_jb_lbl8(&lbl);
    x86asm_orl_imm32_reg32(1, slots[slot_dst].reg_no);
    x86asm_lbl8_define(&lbl);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_rhs);
    ungrab_slot(slot_lhs);

    x86asm_lbl8_cleanup(&lbl);
}

static void emit_set_ge_signed(struct code_block_x86_64 *blk,
                               struct il_code_block const *il_blk,
                               void *cpu, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.set_ge_signed.slot_lhs;
    unsigned slot_rhs = inst->immed.set_ge_signed.slot_rhs;
    unsigned slot_dst = inst->immed.set_ge_signed.slot_dst;

    struct x86asm_lbl8 lbl;
    x86asm_lbl8_init(&lbl);

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_lhs, 4);
    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_rhs, 4);
    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_dst, 4);

    x86asm_cmpl_reg32_reg32(slots[slot_rhs].reg_no, slots[slot_lhs].reg_no);
    x86asm_jl_lbl8(&lbl);
    x86asm_orl_imm32_reg32(1, slots[slot_dst].reg_no);
    x86asm_lbl8_define(&lbl);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_rhs);
    ungrab_slot(slot_lhs);

    x86asm_lbl8_cleanup(&lbl);
}

static void emit_mul_u32(struct code_block_x86_64 *blk,
                         struct il_code_block const *il_blk,
                         void *cpu, struct jit_inst const *inst) {
    unsigned slot_lhs = inst->immed.mul_u32.slot_lhs;
    unsigned slot_rhs = inst->immed.mul_u32.slot_rhs;
    unsigned slot_dst = inst->immed.mul_u32.slot_dst;

    evict_register(blk, &gen_reg_state, REG_RET);
    grab_register(&gen_reg_state.set, REG_RET);
    evict_register(blk, &gen_reg_state, EDX);
    grab_register(&gen_reg_state.set, EDX);

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_lhs, 4);
    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_rhs, 4);
    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_dst, 4);

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
    ungrab_register(&gen_reg_state.set, EDX);
    ungrab_register(&gen_reg_state.set, REG_RET);
}

static void emit_mul_float(struct code_block_x86_64 *blk,
                           struct il_code_block const *il_blk,
                           void *cpu, struct jit_inst const *inst) {
    unsigned slot_src = inst->immed.mul_float.slot_lhs;
    unsigned slot_dst = inst->immed.mul_float.slot_dst;

    grab_slot(blk, il_blk, inst, &xmm_reg_state, slot_src, 4);
    if (slot_src != slot_dst)
        grab_slot(blk, il_blk, inst, &xmm_reg_state, slot_dst, 4);

    x86asm_mulss_xmm_xmm(slots[slot_src].reg_no, slots[slot_dst].reg_no);

    if (slot_src != slot_dst)
        ungrab_slot(slot_dst);
    ungrab_slot(slot_src);
}

static void emit_sub_float(struct code_block_x86_64 *blk,
                           struct il_code_block const *il_blk,
                           void *cpu, struct jit_inst const *inst) {
    unsigned slot_src = inst->immed.sub_float.slot_src;
    unsigned slot_dst = inst->immed.sub_float.slot_dst;

    grab_slot(blk, il_blk, inst, &xmm_reg_state, slot_src, 4);
    if (slot_src != slot_dst)
        grab_slot(blk, il_blk, inst, &xmm_reg_state, slot_dst, 4);

    x86asm_subss_xmm_xmm(slots[slot_src].reg_no, slots[slot_dst].reg_no);

    if (slot_src != slot_dst)
        ungrab_slot(slot_dst);
    ungrab_slot(slot_src);
}

static void emit_shad(struct code_block_x86_64 *blk,
                      struct il_code_block const *il_blk,
                      void *cpu, struct jit_inst const *inst) {
    unsigned slot_val = inst->immed.shad.slot_val;
    unsigned slot_shift_amt = inst->immed.shad.slot_shift_amt;

    // shift_amt register must be CL
    evict_register(blk, &gen_reg_state, RCX);
    grab_register(&gen_reg_state.set, RCX);

    // REGISTER_HINT_FUNCTION not necessary, just being used as a "default"
    int reg_tmp = register_pick(&gen_reg_state.set, REGISTER_HINT_FUNCTION);
    evict_register(blk, &gen_reg_state, reg_tmp);
    grab_register(&gen_reg_state.set, reg_tmp);

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_shift_amt, 4);
    x86asm_mov_reg32_reg32(slots[slot_shift_amt].reg_no, RCX);
    ungrab_slot(slot_shift_amt);

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_val, 4);

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
    ungrab_register(&gen_reg_state.set, reg_tmp);
    ungrab_register(&gen_reg_state.set, RCX);
}

static void emit_read_float_slot(struct code_block_x86_64 *blk,
                                 struct il_code_block const *il_blk,
                                 void *cpu, struct jit_inst const *inst) {
    unsigned dst_slot = inst->immed.read_float_slot.dst_slot;
    unsigned addr_slot = inst->immed.read_float_slot.addr_slot;
    struct memory_map const *map = inst->immed.read_float_slot.map;

    // call memory_map_read_float(*addr_slot)
    prefunc(blk);

    if (config_get_inline_mem()) {
        move_slot_to_reg(blk, addr_slot, REG_ARG0);
        evict_register(blk, &gen_reg_state, REG_ARG0);
        native_mem_read_float(blk, map);
    } else {
        x86asm_mov_imm64_reg64((uint64_t)map, REG_ARG0);
        move_slot_to_reg(blk, addr_slot, REG_ARG1);
        evict_register(blk, &gen_reg_state, REG_ARG1);
        ms_shadow_open(blk);
        x86_64_align_stack(blk);
        x86asm_call_ptr(memory_map_read_float);
        ms_shadow_close();
    }

    postfunc_float();

    grab_slot(blk, il_blk, inst, &xmm_reg_state, dst_slot, 4);

    // move XMM0 into slots[dst_slot].reg_no
    x86asm_movss_xmm_xmm(REG_RET_XMM, slots[dst_slot].reg_no);

    ungrab_slot(dst_slot);
    ungrab_register(&xmm_reg_state.set, REG_RET_XMM);
}

static void emit_load_float_slot(struct code_block_x86_64 *blk,
                                 struct il_code_block const *il_blk,
                                 void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.load_float_slot.slot_no;
    void const *src_ptr = inst->immed.load_float_slot.src;

    grab_slot(blk, il_blk, inst, &xmm_reg_state, slot_no, 4);

    unsigned reg_dst = slots[slot_no].reg_no;

    int reg_addr = register_pick(&gen_reg_state.set, REGISTER_HINT_NONE);
    evict_register(blk, &gen_reg_state, reg_addr);
    grab_register(&gen_reg_state.set, reg_addr);

    x86asm_mov_imm64_reg64((uintptr_t)src_ptr, reg_addr);

    // move *reg_addr into the XMM reg
    x86asm_movss_indreg_xmm(reg_addr, reg_dst);

    ungrab_register(&gen_reg_state.set, reg_addr);
    ungrab_slot(slot_no);
}

static void
emit_load_float_slot_indexed(struct code_block_x86_64 *blk,
                             struct il_code_block const *il_blk,
                             void *cpu, struct jit_inst const *inst) {
    unsigned slot_base = inst->immed.load_float_slot_indexed.slot_base;
    unsigned slot_dst = inst->immed.load_float_slot_indexed.slot_dst;
    unsigned index = inst->immed.load_float_slot_indexed.index;

    grab_slot(blk, il_blk, inst, &gen_reg_state, slot_base, 8);
    grab_slot(blk, il_blk, inst, &xmm_reg_state, slot_dst, 4);

    unsigned reg_base = slots[slot_base].reg_no;
    unsigned reg_dst = slots[slot_dst].reg_no;
    int disp_bytes = 4 * index;

    if (disp_bytes <= 127 && disp_bytes >= - 128)
        x86asm_movss_disp8_reg_xmm(disp_bytes, reg_base, reg_dst);
    else
        x86asm_movss_disp32_reg_xmm(disp_bytes, reg_base, reg_dst);

    ungrab_slot(slot_dst);
    ungrab_slot(slot_base);
}

static void emit_store_float_slot(struct code_block_x86_64 *blk,
                                  struct il_code_block const *il_blk,
                                  void *cpu, struct jit_inst const *inst) {
    unsigned slot_no = inst->immed.store_float_slot.slot_no;
    void const *dst_ptr = inst->immed.store_float_slot.dst;

    evict_register(blk, &gen_reg_state, REG_RET);
    grab_register(&gen_reg_state.set, REG_RET);
    grab_slot(blk, il_blk, inst, &xmm_reg_state, slot_no, 4);

    unsigned reg_no = slots[slot_no].reg_no;
    x86asm_mov_imm64_reg64((uintptr_t)dst_ptr, REG_RET);
    x86asm_movss_xmm_indreg(reg_no, REG_RET);

    ungrab_slot(slot_no);
    ungrab_register(&gen_reg_state.set, REG_RET);
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
void x86_64_align_stack(struct code_block_x86_64 *blk) {
    int cur = -(rsp_offs + 8);
    int roundup = ((cur + 15) / 16) * 16;
    int newdisp = roundup - cur;

    if (newdisp) {
        x86asm_addq_imm8_reg(-newdisp, RSP);
        rsp_offs -= newdisp;
    }

    blk->dirty_stack = true;
}

/*
 * Microsoft's ABI requires 32 bytes to be allocated on the stack when calling
 * a function.
 */
void ms_shadow_open(struct code_block_x86_64 *blk) {
#ifdef ABI_MICROSOFT
    x86asm_addq_imm8_reg(-32, RSP);
    rsp_offs -= 32;
    blk->dirty_stack = true;
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
                               struct native_dispatch_meta const *dispatch_meta,
                               unsigned cycle_count) {
    struct jit_inst const* inst = il_blk->inst_list;
    unsigned inst_count = il_blk->inst_count;
    out->cycle_count = cycle_count;
    out->dirty_stack = false;

    x86asm_set_dst(out->exec_mem_alloc_start, &out->bytes_used,
                   X86_64_ALLOC_SIZE);

    reset_slots();

    emit_stack_frame_open();

    void *skip_stack_frame = x86asm_get_out_ptr();

    while (inst_count--) {
        switch (inst->op) {
        case JIT_OP_FALLBACK:
            emit_fallback(out, il_blk, cpu, inst);
            break;
        case JIT_OP_JUMP:
            emit_jump(out, il_blk, cpu, inst);
            break;
        case JIT_CSET:
            emit_cset(out, il_blk, cpu, inst);
            break;
        case JIT_SET_SLOT:
            emit_set_slot(out, il_blk, cpu, inst);
            break;
        case JIT_SET_SLOT_HOST_PTR:
            emit_set_slot_host_ptr(out, il_blk, cpu, inst);
            break;
        case JIT_OP_CALL_FUNC:
            emit_call_func(out, il_blk, cpu, inst);
            break;
        case JIT_OP_READ_16_CONSTADDR:
            emit_read_16_constaddr(out, il_blk, cpu, inst);
            break;
        case JIT_OP_SIGN_EXTEND_8:
            emit_sign_extend_8(out, il_blk, cpu, inst);
            break;
        case JIT_OP_SIGN_EXTEND_16:
            emit_sign_extend_16(out, il_blk, cpu, inst);
            break;
        case JIT_OP_READ_32_CONSTADDR:
            emit_read_32_constaddr(out, il_blk, cpu, inst);
            break;
        case JIT_OP_READ_8_SLOT:
            emit_read_8_slot(out, il_blk, cpu, inst);
            break;
        case JIT_OP_READ_16_SLOT:
            emit_read_16_slot(out, il_blk, cpu, inst);
            break;
        case JIT_OP_READ_32_SLOT:
            emit_read_32_slot(out, il_blk, cpu, inst);
            break;
        case JIT_OP_WRITE_8_SLOT:
            emit_write_8_slot(out, il_blk, cpu, inst);
            break;
        case JIT_OP_WRITE_32_SLOT:
            emit_write_32_slot(out, il_blk, cpu, inst);
            break;
        case JIT_OP_WRITE_FLOAT_SLOT:
            emit_write_float_slot(out, il_blk, cpu, inst);
            break;
        case JIT_OP_LOAD_SLOT16:
            emit_load_slot16(out, il_blk, cpu, inst);
            break;
        case JIT_OP_LOAD_SLOT:
            emit_load_slot(out, il_blk, cpu, inst);
            break;
        case JIT_OP_LOAD_SLOT_INDEXED:
            emit_load_slot_indexed(out, il_blk, cpu, inst);
            break;
        case JIT_OP_STORE_SLOT:
            emit_store_slot(out, il_blk, cpu, inst);
            break;
        case JIT_OP_STORE_SLOT_INDEXED:
            emit_store_slot_indexed(out, il_blk, cpu, inst);
            break;
        case JIT_OP_ADD:
            emit_add(out, il_blk, cpu, inst);
            break;
        case JIT_OP_SUB:
            emit_sub(out, il_blk, cpu, inst);
            break;
        case JIT_OP_SUB_FLOAT:
            emit_sub_float(out, il_blk, cpu, inst);
            break;
        case JIT_OP_ADD_CONST32:
            emit_add_const32(out, il_blk, cpu, inst);
            break;
        case JIT_OP_XOR:
            emit_xor(out, il_blk, cpu, inst);
            break;
        case JIT_OP_XOR_CONST32:
            emit_xor_const32(out, il_blk, cpu, inst);
            break;
        case JIT_OP_MOV:
            emit_mov(out, il_blk, cpu, inst);
            break;
        case JIT_OP_MOV_FLOAT:
            emit_mov_float(out, il_blk, cpu, inst);
            break;
        case JIT_OP_AND:
            emit_and(out, il_blk, cpu, inst);
            break;
        case JIT_OP_AND_CONST32:
            emit_and_const32(out, il_blk, cpu, inst);
            break;
        case JIT_OP_OR:
            emit_or(out, il_blk, cpu, inst);
            break;
        case JIT_OP_OR_CONST32:
            emit_or_const32(out, il_blk, cpu, inst);
            break;
        case JIT_OP_DISCARD_SLOT:
            discard_slot(out, inst->immed.discard_slot.slot_no);
            break;
        case JIT_OP_SLOT_TO_BOOL:
            emit_slot_to_bool(out, il_blk, cpu, inst);
            break;
        case JIT_OP_NOT:
            emit_not(out, il_blk, cpu, inst);
            break;
        case JIT_OP_SHLL:
            emit_shll(out, il_blk, cpu, inst);
            break;
        case JIT_OP_SHAR:
            emit_shar(out, il_blk, cpu, inst);
            break;
        case JIT_OP_SHLR:
            emit_shlr(out, il_blk, cpu, inst);
            break;
        case JIT_OP_SET_GT_UNSIGNED:
            emit_set_gt_unsigned(out, il_blk, cpu, inst);
            break;
        case JIT_OP_SET_GT_SIGNED:
            emit_set_gt_signed(out, il_blk, cpu, inst);
            break;
        case JIT_OP_SET_GT_SIGNED_CONST:
            emit_set_gt_signed_const(out, il_blk, cpu, inst);
            break;
        case JIT_OP_SET_EQ:
            emit_set_eq(out, il_blk, cpu, inst);
            break;
        case JIT_OP_SET_GE_UNSIGNED:
            emit_set_ge_unsigned(out, il_blk, cpu, inst);
            break;
        case JIT_OP_SET_GE_SIGNED:
            emit_set_ge_signed(out, il_blk, cpu, inst);
            break;
        case JIT_OP_SET_GE_SIGNED_CONST:
            emit_set_ge_signed_const(out, il_blk, cpu, inst);
            break;
        case JIT_OP_MUL_U32:
            emit_mul_u32(out, il_blk, cpu, inst);
            break;
        case JIT_OP_MUL_FLOAT:
            emit_mul_float(out, il_blk, cpu, inst);
            break;
        case JIT_OP_SHAD:
            emit_shad(out, il_blk, cpu, inst);
            break;
        case JIT_OP_READ_FLOAT_SLOT:
            emit_read_float_slot(out, il_blk, cpu, inst);
            break;
        case JIT_OP_LOAD_FLOAT_SLOT:
            emit_load_float_slot(out, il_blk, cpu, inst);
            break;
        case JIT_OP_LOAD_FLOAT_SLOT_INDEXED:
            emit_load_float_slot_indexed(out, il_blk, cpu, inst);
            break;
        case JIT_OP_STORE_FLOAT_SLOT:
            emit_store_float_slot(out, il_blk, cpu, inst);
            break;
        default:
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
        inst++;
    }

    x86asm_mov_imm32_reg32(out->cycle_count,
                           NATIVE_DISPATCH_CYCLE_COUNT_REG);

    if (out->dirty_stack) {
        emit_stack_frame_close();
    } else {
        out->native = skip_stack_frame;
    }

    native_check_cycles_emit(dispatch_meta);
}
