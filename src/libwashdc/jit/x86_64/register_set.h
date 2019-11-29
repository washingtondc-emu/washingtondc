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

#ifndef REGISTER_SET_H_
#define REGISTER_SET_H_

#include <stdbool.h>

struct reg_stat {
    // if true this reg can never ever be allocated under any circumstance.
    bool locked;

    /*
     * Decide how likely the allocator is to pick this register.
     * higher numbers are higher priority.
     */
    int prio;

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

bool register_available(struct register_set *set, unsigned reg_no);
bool register_in_use(struct register_set *set, unsigned reg_no);
bool register_locked(struct register_set *set, unsigned reg_no);

/*
 * unlike grab_slot, this does not preserve the slot that is currently in the
 * register.  To do that, call evict_register first.
 */
void grab_register(struct register_set *set, unsigned reg_no);
void ungrab_register(struct register_set *set, unsigned reg_no);

bool register_grabbed(struct register_set *set, unsigned reg_no);

int register_priority(struct register_set *set, unsigned reg_no);

#endif
