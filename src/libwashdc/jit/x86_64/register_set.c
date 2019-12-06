/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
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

#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "washdc/error.h"
#include "register_set.h"
#include "log.h"

void register_set_init(struct register_set *set, int n_regs,
                       struct reg_stat const *regs) {
    set->n_regs = n_regs;
    size_t n_bytes = sizeof(struct reg_stat) * n_regs;
    set->regs = (struct reg_stat*)malloc(n_bytes);
    if (!set->regs) {
        error_set_length(n_bytes);
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    }
    memcpy(set->regs, regs, n_bytes);
}

void register_set_cleanup(struct register_set *set) {
    free(set->regs);
    memset(set, 0, sizeof(*set));
}

void register_set_reset(struct register_set *set) {
    unsigned reg_no;
    for (reg_no = 0; reg_no < set->n_regs; reg_no++) {
        set->regs[reg_no].in_use = false;
        set->regs[reg_no].grabbed = false;
    }
}

void register_acquire(struct register_set *set, unsigned reg_no) {
    if (reg_no >= set->n_regs)
        RAISE_ERROR(ERROR_INTEGRITY);

    struct reg_stat *reg = set->regs + reg_no;

    if (reg->in_use || reg->locked)
        RAISE_ERROR(ERROR_INTEGRITY);

    reg->in_use = true;
}

void register_discard(struct register_set *set, unsigned reg_no) {
    if (reg_no >= set->n_regs)
        RAISE_ERROR(ERROR_INTEGRITY);

    struct reg_stat *reg = set->regs + reg_no;

    if (!reg->in_use || reg->locked)
        RAISE_ERROR(ERROR_INTEGRITY);

    reg->in_use = false;
}

static bool register_available(struct register_set *set, unsigned reg_no) {
    if (reg_no >= set->n_regs)
        RAISE_ERROR(ERROR_INTEGRITY);

    struct reg_stat *reg = set->regs + reg_no;
    return !(reg->in_use || reg->locked || reg->grabbed);
}

bool register_in_use(struct register_set *set, unsigned reg_no) {
    if (reg_no >= set->n_regs)
        RAISE_ERROR(ERROR_INTEGRITY);

    return set->regs[reg_no].in_use;
}

static bool register_locked(struct register_set *set, unsigned reg_no) {
    if (reg_no >= set->n_regs)
        RAISE_ERROR(ERROR_INTEGRITY);

    return set->regs[reg_no].locked;
}

static DEF_ERROR_INT_ATTR(native_reg);

void grab_register(struct register_set *set, unsigned reg_no) {
    if (reg_no >= set->n_regs)
        RAISE_ERROR(ERROR_INTEGRITY);

    if (set->regs[reg_no].grabbed) {
        error_set_native_reg(reg_no);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
    set->regs[reg_no].grabbed = true;
}

void ungrab_register(struct register_set *set, unsigned reg_no) {
    if (reg_no >= set->n_regs)
        RAISE_ERROR(ERROR_INTEGRITY);

    if (!set->regs[reg_no].grabbed) {
        error_set_native_reg(reg_no);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
    set->regs[reg_no].grabbed = false;
}

bool register_grabbed(struct register_set *set, unsigned reg_no) {
    if (reg_no >= set->n_regs)
        RAISE_ERROR(ERROR_INTEGRITY);

    return set->regs[reg_no].grabbed;
}

static int register_priority(struct register_set *set, unsigned reg_no) {
    if (reg_no >= set->n_regs)
        RAISE_ERROR(ERROR_INTEGRITY);

    return set->regs[reg_no].prio;
}

static int
pick_unused_reg_with_flags(struct register_set *set,
                           enum register_flag flags, enum register_flag mask) {
    int best_prio = INT_MIN;
    bool found_one = false;
    unsigned reg_no, best_reg = 0xdeadbeef;
    for (reg_no = 0; reg_no < set->n_regs; reg_no++) {
        enum register_flag reg_flags = set->regs[reg_no].flags;
        int prio = register_priority(set, reg_no);
        if ((reg_flags & mask) == flags &&
            register_available(set, reg_no) &&
            ((prio > best_prio) || !found_one)) {
            found_one = true;
            best_prio = prio;
            best_reg = reg_no;
        }
    }

    if (found_one)
        return best_reg;
    return -1;
}

/*
 * This function will pick an unused register to use.  This doesn't change the
 * state of the register.  If there are no unused registers available, this
 * function will return -1.
 */
int register_pick_unused(struct register_set *set, enum register_hint hints) {
    int reg_no;

    if (hints & REGISTER_HINT_JUMP_ADDR) {
        reg_no =
            pick_unused_reg_with_flags(set, REGISTER_FLAG_NATIVE_DISPATCH_PC,
                                       REGISTER_FLAG_NATIVE_DISPATCH_PC);
        if (reg_no >= 0)
            return reg_no;
    }

    if (hints & REGISTER_HINT_JUMP_HASH) {
        reg_no =
            pick_unused_reg_with_flags(set, REGISTER_FLAG_NATIVE_DISPATCH_HASH,
                                       REGISTER_FLAG_NATIVE_DISPATCH_HASH);
        if (reg_no >= 0)
            return reg_no;
    }

    if (hints & REGISTER_HINT_FUNCTION) {
        /*
         * first consider registers which will be preserved across function
         * calls.
         */
        reg_no =
            pick_unused_reg_with_flags(set, REGISTER_FLAG_PRESERVED,
                                       REGISTER_FLAG_PRESERVED |
                                       REGISTER_FLAG_REX |
                                       REGISTER_FLAG_RETURN);
        if (reg_no >= 0)
            return reg_no;
        reg_no =
            pick_unused_reg_with_flags(set, REGISTER_FLAG_PRESERVED,
                                       REGISTER_FLAG_PRESERVED |
                                       REGISTER_FLAG_RETURN);
        if (reg_no >= 0)
            return reg_no;

        // pick one of the ones that will get clobbered by function calls
        reg_no =
            pick_unused_reg_with_flags(set, REGISTER_FLAG_NONE,
                                       REGISTER_FLAG_REX |
                                       REGISTER_FLAG_RETURN);
        if (reg_no >= 0)
            return reg_no;
        reg_no =
            pick_unused_reg_with_flags(set, REGISTER_FLAG_NONE,
                                       REGISTER_FLAG_RETURN);
        if (reg_no >= 0)
            return reg_no;
    } else {
        /*
         * first look at registers that don't need a rex.
         * IDK why RAX gets top priority but this code
         * has been that way for a while.
         */
        reg_no =
            pick_unused_reg_with_flags(set, REGISTER_FLAG_RETURN,
                                       REGISTER_FLAG_RETURN |
                                       REGISTER_FLAG_REX);
        if (reg_no >= 0)
            return reg_no;

        // consider RBX even though it's nonvolatile since it doesn't need REX
        reg_no =
            pick_unused_reg_with_flags(set, REGISTER_FLAG_PRESERVED,
                                       REGISTER_FLAG_REX);
        if (reg_no >= 0)
            return reg_no;

        // volatile registers that need REX
        reg_no =
            pick_unused_reg_with_flags(set, REGISTER_FLAG_NONE, REGISTER_FLAG_PRESERVED);
        if (reg_no >= 0)
            return reg_no;

        // nonvolatile registers that need REX
        reg_no =
            pick_unused_reg_with_flags(set, REGISTER_FLAG_NONE,
                                       REGISTER_FLAG_NONE);
        if (reg_no >= 0)
            return reg_no;
    }

    return -1;
}

int register_pick(struct register_set *set, enum register_hint hints) {
    unsigned reg_no;
    unsigned best_reg = 0;
    int best_prio = INT_MIN;
    bool found_one = false;

    // first pass: try to find one that's not in use
    int unused_reg = register_pick_unused(set, hints);
    if (unused_reg >= 0)
        return (unsigned)unused_reg;

    /*
     * second pass: they're all in use so just pick one that is not locked or
     * grabbed.
     */
    for (reg_no = 0; reg_no < set->n_regs; reg_no++) {
        if (!register_locked(set, reg_no) &&
            !register_grabbed(set, reg_no)) {
            int prio = register_priority(set, reg_no);
            if (!found_one || prio > best_prio) {
                found_one = true;
                best_prio = prio;
                best_reg = reg_no;
            }
        }
    }

    if (found_one)
        return best_reg;

    LOG_ERROR("x86_64: no more registers!\n");
    RAISE_ERROR(ERROR_INTEGRITY);
}
