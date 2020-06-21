/*******************************************************************************
 *
 * Copyright 2019 snickerbockers
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

#ifndef REGISTER_SET_H_
#define REGISTER_SET_H_

#include <stdbool.h>

enum register_flag {
    REGISTER_FLAG_NONE = 0,

    // value of register is preserved across function calls
    REGISTER_FLAG_PRESERVED = 1,

    // register is used to host the native_dispatch PC register
    REGISTER_FLAG_NATIVE_DISPATCH_PC = 2,

    // register is used to host the native_dispatch jump hash register
    REGISTER_FLAG_NATIVE_DISPATCH_HASH = 4,

    // register stores function return values
    REGISTER_FLAG_RETURN = 8,

    // register introduces a REX prefix
    REGISTER_FLAG_REX = 16
};

enum register_hint {
    REGISTER_HINT_NONE = 0,

    /*
     * this hint tells the allocator to favor registers that will be preserved
     * across function calls.
     */
    REGISTER_HINT_FUNCTION = 1,

    /*
     * Tells the allocator that this slot will be used to store the hash for
     * the jump instruction
     */
    REGISTER_HINT_JUMP_HASH = 2,

    /*
     * Tells the allocator that this slot will be used to store the address for
     * the jump instruction
     */
    REGISTER_HINT_JUMP_ADDR = 4
};

struct reg_stat {
    // if true this reg can never ever be allocated under any circumstance.
    bool locked;

    /*
     * Decide how likely the allocator is to pick this register.
     * higher numbers are higher priority.
     */
    int prio;

    enum register_flag flags;

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
};

struct register_set {
    int n_regs;
    struct reg_stat *regs;
};

void register_set_init(struct register_set *set, int n_regs,
                       struct reg_stat const *regs);
void register_set_cleanup(struct register_set *set);

/*
 * call this function at the beginning of each code block to put all the
 * registers back into their default states.
 */
void register_set_reset(struct register_set *set);

void register_acquire(struct register_set *set, unsigned reg_no);
void register_discard(struct register_set *set, unsigned reg_no);

bool register_in_use(struct register_set *set, unsigned reg_no);

/*
 * unlike grab_slot, this does not preserve the slot that is currently in the
 * register.  To do that, call evict_register first.
 */
void grab_register(struct register_set *set, unsigned reg_no);
void ungrab_register(struct register_set *set, unsigned reg_no);

bool register_grabbed(struct register_set *set, unsigned reg_no);

int register_pick_unused(struct register_set *set, enum register_hint hints);

/*
 * The allocator calls this to find a register it can use.  This doesn't change
 * the state of the register or do anything to save the value in that register.
 * All it does is find a register which is not locked and not grabbed.
 */
int register_pick(struct register_set *set, enum register_hint hints);

#endif
