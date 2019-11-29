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

#include "washdc/error.h"
#include "register_set.h"

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

    set->regs[reg_no].in_use = true;
}

void register_discard(struct register_set *set, unsigned reg_no) {
    if (reg_no >= set->n_regs)
        RAISE_ERROR(ERROR_INTEGRITY);

    set->regs[reg_no].in_use = false;
}

bool register_available(struct register_set *set, unsigned reg_no) {
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

bool register_locked(struct register_set *set, unsigned reg_no) {
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

int register_priority(struct register_set *set, unsigned reg_no) {
    if (reg_no >= set->n_regs)
        RAISE_ERROR(ERROR_INTEGRITY);

    return set->regs[reg_no].prio;
}
