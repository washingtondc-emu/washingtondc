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

#include <string.h>

#include "code_block.h"

#include "jit_determ.h"

#ifndef JIT_OPTIMIZE
#error this file should not be built without -DJIT_OPTIMIZE
#endif

static void update_state(struct jit_determ_state *state,
                         struct jit_inst const *op);

void jit_determ_default(struct jit_determ_state *new_state) {
    memset(new_state, 0, sizeof(*new_state));
}

void jit_determ_pass(struct il_code_block *block) {
    unsigned inst_no;
    unsigned inst_count = block->inst_count;

    block->determ = calloc(inst_count, sizeof(struct jit_determ_state));

    struct jit_determ_state *state, *state_prev = NULL;
    for (inst_no = 0; inst_no < inst_count; inst_no++) {
        state = block->determ + inst_no;

        if (state_prev)
            *state = *state_prev;

        update_state(state, block->inst_list + inst_no);

        state = state_prev;
    }
}

static void update_state(struct jit_determ_state *state,
                         struct jit_inst const *op) {
    struct jit_determ_slot const *srcp;
    struct jit_determ_slot const *lhsp;
    struct jit_determ_slot const *rhsp;
    struct jit_determ_slot *dstp;
    unsigned slot_src, slot_dst;
    uint32_t const32;
    uint32_t zero_bits, one_bits;
    unsigned shift_amt;

    switch (op->op) {
    case JIT_SET_SLOT:
        dstp = state->slots + op->immed.set_slot.slot_idx;
        dstp->known_bits = 0xffffffff;
        dstp->known_val = op->immed.set_slot.new_val;
        break;
    case JIT_OP_READ_16_CONSTADDR:
        dstp = state->slots + op->immed.read_16_constaddr.slot_no;
        dstp->known_bits = 0xffff0000;
        dstp->known_val = 0;
        break;
    case JIT_OP_SIGN_EXTEND_16:
        dstp = state->slots + op->immed.sign_extend_16.slot_no;
        if (dstp->known_bits & (1 << 16)) {
            dstp->known_bits |= 0xffff0000;
            if (dstp->known_val & (1 << 16))
                dstp->known_val |= 0xffff0000;
            else
                dstp->known_val &= 0xffff;
        } else {
            dstp->known_val &= 0xffff;
            dstp->known_bits &= 0xffff;
        }
        break;
    case JIT_OP_READ_32_CONSTADDR:
        dstp = state->slots + op->immed.read_32_constaddr.slot_no;
        dstp->known_val = 0;
        dstp->known_bits = 0;
        break;
    case JIT_OP_READ_32_SLOT:
        dstp = state->slots + op->immed.read_32_slot.dst_slot;
        dstp->known_val = 0;
        dstp->known_bits = 0;
        break;
    case JIT_OP_WRITE_32_SLOT:
        // read-only op
        break;
    case JIT_OP_LOAD_SLOT16:
        // the IL will zero-extend
        dstp = state->slots + op->immed.load_slot16.slot_no;
        dstp->known_val = 0;
        dstp->known_bits = 0xffff0000;
        break;
    case JIT_OP_STORE_SLOT:
        // read-only op
        break;
    case JIT_OP_ADD:
        slot_src = op->immed.add.slot_src;
        slot_dst = op->immed.add.slot_dst;
        srcp = state->slots + op->immed.add.slot_src;
        dstp = state->slots + op->immed.add.slot_dst;
        if (srcp->known_bits == 0xffffffff && dstp->known_bits == 0xffffffff) {
            dstp->known_val = dstp->known_val + srcp->known_val;
            dstp->known_bits = 0xffffffff;
        } else if (slot_src == slot_dst) {
            /*
             * adding a slot into itself.
             *
             * The new value will be double the slot.
             * The new least-significant-bit will be 0.
             */
            dstp->known_val <<= 1;
            dstp->known_bits = (dstp->known_bits << 1) | 1;
        } else {
            /*
             * TODO: it should be possible to know the lower-order bits in dstp if
             * the original lower-order bits of srcp and dstp are known.
             */
            dstp->known_val = 0;
            dstp->known_bits = 0;
        }
        break;
    case JIT_OP_SUB:
        slot_src = op->immed.sub.slot_src;
        slot_dst = op->immed.sub.slot_dst;
        srcp = state->slots + op->immed.sub.slot_src;
        dstp = state->slots + op->immed.sub.slot_dst;
        if (srcp->known_bits == 0xffffffff && dstp->known_bits == 0xffffffff) {
            dstp->known_val = dstp->known_val - srcp->known_val;
            dstp->known_bits = 0xffffffff;
        } else if (slot_src == slot_dst) {
            dstp->known_bits = 0xffffffff;
            dstp->known_val = 0;
            /*
             * TODO: really there's no reason to emit a subtract in this case,
             * might as well XOR reg_dst with itself instead.
             */
        } else {
            /*
             * TODO: it should be possible to know the lower-order bits in dstp if
             * the original lower-order bits of srcp and dstp are known.
             */
            dstp->known_bits = 0;
            dstp->known_val = 0;
        }
        /*
         * TODO: there are a couple other "idiot-cases" we could cover here, such
         * as dst-0, 0-src, etc.
         */
        break;
    case JIT_OP_ADD_CONST32:
        dstp = state->slots + op->immed.add_const32.slot_dst;
        const32 = op->immed.add_const32.const32;
        if (dstp->known_bits == 0xffffffff) {
            dstp->known_val += const32;
        } else {
            /*
             * TODO: it should be possible to know the lower-order bits in dstp
             * if the original lower-order bits of srcp and dstp are known.
             */
            dstp->known_bits = 0;
            dstp->known_val = 0;
        }
        break;
    case JIT_OP_DISCARD_SLOT:
        dstp = state->slots + op->immed.discard_slot.slot_no;
        dstp->known_bits = 0;
        dstp->known_val = 0;
        break;
    case JIT_OP_XOR:
        slot_src = op->immed.xor.slot_src;
        slot_dst = op->immed.xor.slot_dst;
        srcp = state->slots + slot_src;
        dstp = state->slots + slot_dst;
        if (slot_src == slot_dst) {
            dstp->known_bits = 0xffffffff;
            dstp->known_val = 0;
        } else {
            dstp->known_bits &= srcp->known_bits;
            dstp->known_val ^= srcp->known_val;
        }
        break;
    case JIT_OP_XOR_CONST32:
        dstp = state->slots + op->immed.xor_const32.slot_no;
        const32 = op->immed.xor_const32.const32;
        dstp->known_val ^=const32;
        /*
         * slotp->known_bits is unchanged because for XOR operations, we can only
         * know the correct value of an output bit if we know both of its input
         * bits.
         */
        break;
    case JIT_OP_MOV:
        srcp = state->slots + op->immed.mov.slot_src;
        dstp = state->slots + op->immed.mov.slot_dst;
        dstp->known_bits = srcp->known_bits;
        dstp->known_val = dstp->known_val;
        break;
    case JIT_OP_AND:
        srcp = state->slots + op->immed.and.slot_src;
        dstp = state->slots + op->immed.and.slot_dst;
        zero_bits = ((~srcp->known_val) & srcp->known_bits) |
            ((~srcp->known_val) & srcp->known_bits);
        one_bits = (srcp->known_val & srcp->known_bits) &
            (srcp->known_val & srcp->known_bits);

        dstp->known_bits = zero_bits | one_bits;
        dstp->known_val = ((~zero_bits) | one_bits) & dstp->known_bits;
        break;
    case JIT_OP_AND_CONST32:
        dstp = state->slots + op->immed.and_const32.slot_no;
        const32 = op->immed.and_const32.const32;
        zero_bits = (~const32) | ((~dstp->known_val) & dstp->known_bits);
        one_bits = const32 & dstp->known_val & dstp->known_bits;

        dstp->known_bits = zero_bits | one_bits;
        dstp->known_val = ((~zero_bits) | one_bits) & dstp->known_bits;
        break;
    case JIT_OP_OR:
        srcp = state->slots + op->immed.or.slot_src;
        dstp = state->slots + op->immed.or.slot_dst;
        /*
         * we know the value of all dst-bits in which one of the two src-bits is 1
         * (in which case the dst-bit is 1) or both of the two src-bits is 0 (in
         * which case the dst-bit is 0).  We do not know the value of a dst-bit if
         * only one of the two input-bits is known to be 0.
         */
        zero_bits = ((~srcp->known_val) & srcp->known_bits) &
            ((~dstp->known_val) & dstp->known_bits);
        one_bits = (srcp->known_val & srcp->known_bits) |
            (dstp->known_val & dstp->known_bits);

        dstp->known_bits = zero_bits | one_bits;
        dstp->known_val = ((~zero_bits) | one_bits) & dstp->known_bits;
        break;
    case JIT_OP_OR_CONST32:
        dstp = state->slots + op->immed.or_const32.slot_no;
        const32 = op->immed.or_const32.const32;
        /*
         * we know the value of all dst-bits in which one of the two src-bits
         * is 1 (in which case the dst-bit is 1) or both of the two src-bits is
         * 0 (in which case the dst-bit is 0).  We do not know the value of a
         * dst-bit if only one of the two input-bits is known to be 0.
         */
        zero_bits = (~const32) & ((~dstp->known_val) & dstp->known_bits);
        one_bits = const32 | (dstp->known_val & dstp->known_bits);
        dstp->known_bits = zero_bits | one_bits;
        dstp->known_val = ((~zero_bits) | one_bits) & dstp->known_bits;
        break;
    case JIT_OP_SLOT_TO_BOOL:
        dstp = state->slots + op->immed.slot_to_bool.slot_no;
        // cache known values
        if (dstp->known_bits == 0xffffffff)
            dstp->known_val = dstp->known_val ? 1 : 0;
        else
            dstp->known_bits = 0;
        break;
    case JIT_OP_NOT:
        dstp = state->slots + op->immed.not.slot_no;
        dstp->known_val = ~dstp->known_val;
        break;
    case JIT_OP_SHLL:
        dstp = state->slots + op->immed.shll.slot_no;
        shift_amt = op->immed.shll.shift_amt;
        // cache known values
        dstp->known_val <<= shift_amt;
        if (shift_amt >= 32)
            dstp->known_bits = 0xffffffff; // all are zero
        else
            dstp->known_bits |= (1 << shift_amt) - 1;
        break;
    case JIT_OP_SHAR:
        dstp = state->slots + op->immed.shar.slot_no;
        shift_amt = op->immed.shar.shift_amt;
        dstp->known_val = ((int32_t)dstp->known_val) >> shift_amt;
        if (shift_amt >= 32)
            dstp->known_bits = 0xffffffff; // all are one
        else
            dstp->known_bits |= ~((1 << (31 - shift_amt)) - 1);
        break;
    case JIT_OP_SHLR:
        dstp = state->slots + op->immed.shlr.slot_no;
        shift_amt = op->immed.shlr.shift_amt;
        dstp->known_val >>= shift_amt;
        if (shift_amt >= 32)
            dstp->known_bits = 0xffffffff; // all are zero
        else
            dstp->known_bits |= ~((1 << (31 - shift_amt)) - 1);
        break;
    case JIT_OP_SET_GT_UNSIGNED:
        lhsp = state->slots + op->immed.set_gt_unsigned.slot_lhs;
        rhsp = state->slots + op->immed.set_gt_unsigned.slot_rhs;
        dstp = state->slots + op->immed.set_gt_unsigned.slot_dst;
        /*
         * TODO: if the upper N bits of both lhs and rhs are known and those upper
         * N bits differ then it doesn't matter that you don't know the lower
         * (32-N) bits.
         */
        if (lhsp->known_bits == 0xffffffff && rhsp->known_bits == 0xffffffff &&
            lhsp->known_val > rhsp->known_val) {
            dstp->known_bits |= 1;
            dstp->known_val |= 1;
        } else {
            dstp->known_bits &= ~1;
        }
        break;
    case JIT_OP_SET_GT_SIGNED:
        lhsp = state->slots + op->immed.set_gt_signed.slot_lhs;
        rhsp = state->slots + op->immed.set_gt_signed.slot_rhs;
        dstp = state->slots + op->immed.set_gt_signed.slot_dst;

        /*
         * TODO: if the upper N bits of both lhs and rhs are known and those upper
         * N bits differ then it doesn't matter that you don't know the lower
         * (32-N) bits.
         */
        if (lhsp->known_bits == 0xffffffff && rhsp->known_bits == 0xffffffff &&
            (int32_t)lhsp->known_val > (int32_t)rhsp->known_val) {
            dstp->known_bits |= 1;
            dstp->known_val |= 1;
        } else {
            dstp->known_bits &= ~1;
        }
        break;
    case JIT_OP_SET_GT_SIGNED_CONST:
        lhsp = state->slots + op->immed.set_gt_signed_const.slot_lhs;
        const32 = op->immed.set_gt_signed_const.imm_rhs;
        dstp = state->slots + op->immed.set_gt_signed_const.slot_dst;
        if (lhsp->known_bits == 0xffffffff &&
            (int32_t)lhsp->known_val > (int32_t)const32) {
            dstp->known_bits |= 1;
            dstp->known_val |= 1;
        } else {
            dstp->known_bits &= ~1;
        }
        break;
    case JIT_OP_SET_EQ:
        lhsp = state->slots + op->immed.set_eq.slot_lhs;
        rhsp = state->slots + op->immed.set_eq.slot_rhs;
        dstp = state->slots + op->immed.set_eq.slot_dst;

        /*
         * TODO: if the upper N bits of both lhs and rhs are known and those upper
         * N bits differ then it doesn't matter that you don't know the lower
         * (32-N) bits.
         */
        if (lhsp->known_bits == 0xffffffff && rhsp->known_bits == 0xffffffff &&
            lhsp->known_val == rhsp->known_val) {
            dstp->known_bits |= 1;
            dstp->known_val |= 1;
        } else {
            dstp->known_bits &= ~1;
        }
        break;
    case JIT_OP_SET_GE_UNSIGNED:
        lhsp = state->slots + op->immed.set_ge_unsigned.slot_lhs;
        rhsp = state->slots + op->immed.set_ge_unsigned.slot_rhs;
        dstp = state->slots + op->immed.set_ge_unsigned.slot_dst;

        /*
         * TODO: if the upper N bits of both lhs and rhs are known and those upper
         * N bits differ then it doesn't matter that you don't know the lower
         * (32-N) bits.
         */
        if (lhsp->known_bits == 0xffffffff && rhsp->known_bits == 0xffffffff &&
            lhsp->known_val >= rhsp->known_val) {
            dstp->known_bits |= 1;
            dstp->known_val |= 1;
        } else {
            dstp->known_bits &= ~1;
        }
        break;
    case JIT_OP_SET_GE_SIGNED:
        lhsp = state->slots + op->immed.set_ge_signed.slot_lhs;
        rhsp = state->slots + op->immed.set_ge_signed.slot_rhs;
        dstp = state->slots + op->immed.set_ge_signed.slot_dst;

        /*
         * TODO: if the upper N bits of both lhs and rhs are known and those upper
         * N bits differ then it doesn't matter that you don't know the lower
         * (32-N) bits.
         */
        if (lhsp->known_bits == 0xffffffff && rhsp->known_bits == 0xffffffff &&
            (int32_t)lhsp->known_val >= (int32_t)rhsp->known_val) {
            dstp->known_bits |= 1;
            dstp->known_val |= 1;
        } else {
            dstp->known_bits &= ~1;
        }
        break;
    case JIT_OP_SET_GE_SIGNED_CONST:
        lhsp = state->slots + op->immed.set_ge_signed_const.slot_lhs;
        const32 = op->immed.set_ge_signed_const.imm_rhs;
        dstp = state->slots + op->immed.set_ge_signed_const.slot_dst;

        if (lhsp->known_bits == 0xffffffff &&
            (int32_t)lhsp->known_val >= (int32_t)const32) {
            dstp->known_bits |= 1;
            dstp->known_val |= 1;
        } else {
            dstp->known_bits &= ~1;
        }
        break;
    case JIT_OP_MUL_U32:
        lhsp = state->slots + op->immed.mul_u32.slot_lhs;
        rhsp = state->slots + op->immed.mul_u32.slot_rhs;
        dstp = state->slots + op->immed.mul_u32.slot_dst;

        /*
         * TODO: this should be possible if the lower N bits of both src and
         * dst are known, but it seems complicated...
         */
        dstp->known_bits = 0;
        dstp->known_val = 0;
        break;
    case JIT_OP_CALL_FUNC:
        // touching the SR can do wild things to registers
    case JIT_OP_FALLBACK:
        // literally ANYTHING could have happened during the fallback function
    case JIT_OP_JUMP:
    case JIT_JUMP_COND:
    case JIT_OP_LOAD_SLOT:
    default:
        jit_determ_default(state);
    }
}
