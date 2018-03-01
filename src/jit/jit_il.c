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

#include "code_block.h"

#include "jit_il.h"

// Mark every bit of every slot as being unkown.
static void invalidate_slots(struct il_code_block *block) {
    unsigned slot_no;
    for (slot_no = 0; slot_no < block->n_slots; slot_no++) {
        block->slots[slot_no].known_bits = 0;
        block->slots[slot_no].known_val = 0;
    }
}

void jit_fallback(struct il_code_block *block,
                  void(*fallback_fn)(Sh4*,Sh4OpArgs), inst_t inst) {
    struct jit_inst op;

    op.op = JIT_OP_FALLBACK;
    op.immed.fallback.fallback_fn = fallback_fn;
    op.immed.fallback.inst.inst = inst;

    il_code_block_push_inst(block, &op);

    invalidate_slots(block);
}

void jit_jump(struct il_code_block *block, unsigned slot_no) {
    struct jit_inst op;

    op.op = JIT_OP_JUMP;
    op.immed.jump.slot_no = slot_no;

    il_code_block_push_inst(block, &op);
}

void jit_jump_cond(struct il_code_block *block,
                   unsigned slot_no, unsigned jmp_addr_slot,
                   unsigned alt_jmp_addr_slot, unsigned t_val) {
    struct jit_inst op;

    op.op = JIT_JUMP_COND;
    op.immed.jump_cond.slot_no = slot_no;
    op.immed.jump_cond.jmp_addr_slot = jmp_addr_slot;
    op.immed.jump_cond.alt_jmp_addr_slot = alt_jmp_addr_slot;
    op.immed.jump_cond.t_flag = t_val;

    il_code_block_push_inst(block, &op);
}

void jit_set_slot(struct il_code_block *block, unsigned slot_idx,
                  uint32_t new_val) {
    struct jit_inst op;

    op.op = JIT_SET_SLOT;
    op.immed.set_slot.new_val = new_val;
    op.immed.set_slot.slot_idx = slot_idx;

    il_code_block_push_inst(block, &op);

    struct il_slot *slotp = block->slots + slot_idx;
    slotp->known_bits = 0xffffffff;
    slotp->known_val = new_val;
}

void jit_restore_sr(struct il_code_block *block, unsigned slot_no) {
    struct jit_inst op;

    op.op = JIT_OP_RESTORE_SR;
    op.immed.restore_sr.slot_no = slot_no;

    il_code_block_push_inst(block, &op);

    /*
     * invalidating *every* slot might be overkill, but in general fucking with
     * the SR can do a lot of things so I play it safe here.
     */
    invalidate_slots(block);
}

void jit_read_16_constaddr(struct il_code_block *block, addr32_t addr,
                      unsigned slot_no) {
    struct jit_inst op;

    op.op = JIT_OP_READ_16_CONSTADDR;
    op.immed.read_16_constaddr.addr = addr;
    op.immed.read_16_constaddr.slot_no = slot_no;

    il_code_block_push_inst(block, &op);

    struct il_slot *slotp = block->slots + slot_no;
    slotp->known_bits = 0xffff0000;
    slotp->known_val = 0;
}

void jit_sign_extend_16(struct il_code_block *block, unsigned slot_no) {
    struct jit_inst op;

    op.op = JIT_OP_SIGN_EXTEND_16;
    op.immed.sign_extend_16.slot_no = slot_no;

    il_code_block_push_inst(block, &op);

    struct il_slot *slotp = block->slots + slot_no;
    if (slotp->known_bits & (1 << 16)) {
        slotp->known_bits |= 0xffff0000;
        if (slotp->known_val & (1 << 16))
            slotp->known_val |= 0xffff0000;
        else
            slotp->known_val &= 0xffff;
    } else {
        slotp->known_val &= 0xffff;
        slotp->known_bits &= 0xffff;
    }
}

void jit_read_32_constaddr(struct il_code_block *block, addr32_t addr,
                      unsigned slot_no) {
    struct jit_inst op;

    op.op = JIT_OP_READ_32_CONSTADDR;
    op.immed.read_32_constaddr.addr = addr;
    op.immed.read_32_constaddr.slot_no = slot_no;

    il_code_block_push_inst(block, &op);

    struct il_slot *slotp = block->slots + slot_no;
    slotp->known_val = 0;
    slotp->known_bits = 0;
}

void jit_read_32_slot(struct il_code_block *block, unsigned addr_slot,
                      unsigned dst_slot) {
    struct jit_inst op;

    op.op = JIT_OP_READ_32_SLOT;
    op.immed.read_32_slot.addr_slot = addr_slot;
    op.immed.read_32_slot.dst_slot = dst_slot;

    il_code_block_push_inst(block, &op);

    struct il_slot *dstp = block->slots + dst_slot;
    dstp->known_val = 0;
    dstp->known_bits = 0;
}

void jit_write_32_slot(struct il_code_block *block, unsigned src_slot,
                       unsigned addr_slot) {
    struct jit_inst op;

    op.op = JIT_OP_WRITE_32_SLOT;
    op.immed.write_32_slot.addr_slot = addr_slot;
    op.immed.write_32_slot.src_slot = src_slot;

    il_code_block_push_inst(block, &op);
}

void jit_load_slot16(struct il_code_block *block, unsigned slot_no,
                     uint16_t const *src) {
    struct jit_inst op;

    op.op = JIT_OP_LOAD_SLOT16;
    op.immed.load_slot16.src = src;
    op.immed.load_slot16.slot_no = slot_no;

    il_code_block_push_inst(block, &op);

    // the IL will zero-extend
    struct il_slot *slotp = block->slots + slot_no;
    slotp->known_val = 0;
    slotp->known_bits = 0xffff0000;
}

void jit_load_slot(struct il_code_block *block, unsigned slot_no,
                   uint32_t const *src) {
    struct jit_inst op;

    op.op = JIT_OP_LOAD_SLOT;
    op.immed.load_slot.src = src;
    op.immed.load_slot.slot_no = slot_no;

    il_code_block_push_inst(block, &op);

    struct il_slot *slotp = block->slots + slot_no;
    slotp->known_val = 0;
    slotp->known_bits = 0;
}

void jit_store_slot(struct il_code_block *block, unsigned slot_no,
                    uint32_t *dst) {
    struct jit_inst op;

    op.op = JIT_OP_STORE_SLOT;
    op.immed.store_slot.dst = dst;
    op.immed.store_slot.slot_no = slot_no;

    il_code_block_push_inst(block, &op);
}

void jit_add(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst) {
    struct jit_inst op;

    op.op = JIT_OP_ADD;
    op.immed.add.slot_src = slot_src;
    op.immed.add.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);

    // cache known values.
    struct il_slot const *srcp = block->slots + slot_src;
    struct il_slot *dstp = block->slots + slot_dst;
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
}

void jit_sub(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst) {
    struct jit_inst op;

    op.op = JIT_OP_SUB;
    op.immed.sub.slot_src = slot_src;
    op.immed.sub.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);

    // cache known values.
    struct il_slot const *srcp = block->slots + slot_src;
    struct il_slot *dstp = block->slots + slot_dst;
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
}

void jit_add_const32(struct il_code_block *block, unsigned slot_dst,
                     uint32_t const32) {
    struct jit_inst op;

    op.op = JIT_OP_ADD_CONST32;
    op.immed.add_const32.slot_dst = slot_dst;
    op.immed.add_const32.const32 = const32;

    il_code_block_push_inst(block, &op);

    struct il_slot *dstp = block->slots + slot_dst;
    if (dstp->known_bits == 0xffffffff) {
        dstp->known_val += const32;
    } else {
        /*
         * TODO: it should be possible to know the lower-order bits in dstp if
         * the original lower-order bits of srcp and dstp are known.
         */
        dstp->known_bits = 0;
        dstp->known_val = 0;
    }
}

void jit_discard_slot(struct il_code_block *block, unsigned slot_no) {
    struct jit_inst op;

    op.op = JIT_OP_DISCARD_SLOT;
    op.immed.discard_slot.slot_no = slot_no;

    il_code_block_push_inst(block, &op);

    struct il_slot *dstp = block->slots + slot_no;
    dstp->known_bits = 0;
    dstp->known_val = 0;
}

void jit_xor(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst) {
    struct jit_inst op;

    op.op = JIT_OP_XOR;
    op.immed.xor.slot_src = slot_src;
    op.immed.xor.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);

    // cache known values.
    struct il_slot const *srcp = block->slots + slot_src;
    struct il_slot *dstp = block->slots + slot_dst;
    if (slot_src == slot_dst) {
        dstp->known_bits = 0xffffffff;
        dstp->known_val = 0;
    } else {
        dstp->known_bits &= srcp->known_bits;
        dstp->known_val ^= srcp->known_val;
    }
}

void jit_xor_const32(struct il_code_block *block, unsigned slot_no,
                     uint32_t const32) {
    struct jit_inst op;

    op.op = JIT_OP_XOR_CONST32;

    op.immed.xor_const32.slot_no = slot_no;
    op.immed.xor_const32.const32 = const32;

    il_code_block_push_inst(block, &op);

    // cache known values
    struct il_slot *slotp = block->slots + slot_no;
    slotp->known_val ^= const32;
    /*
     * slotp->known_bits is unchanged because for XOR operations, we can only
     * know the correct value of an output bit if we know both of its input
     * bits.
     */
}

void jit_mov(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst) {
    struct jit_inst op;

    op.op = JIT_OP_MOV;
    op.immed.mov.slot_src = slot_src;
    op.immed.mov.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);

    // cache known values.
    struct il_slot const *srcp = block->slots + slot_src;
    struct il_slot *dstp = block->slots + slot_dst;
    dstp->known_bits = srcp->known_bits;
    dstp->known_val = dstp->known_val;
}

void jit_and(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst) {
    struct jit_inst op;

    op.op = JIT_OP_AND;
    op.immed.and.slot_src = slot_src;
    op.immed.and.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);

    // cache known values.
    struct il_slot const *srcp = block->slots + slot_src;
    struct il_slot *dstp = block->slots + slot_dst;

    /*
     * we know the value of all dst-bits in which one of the two src-bits is 0
     * (in which case the dst-bit is 0) or both of the two src-bits is 1 (in
     * which the dst-bit is 1).  We do not know the value of a dst-bit if only
     * one of the input bits is known to be 1.
     */
    uint32_t zero_bits = ((~srcp->known_val) & srcp->known_bits) |
        ((~srcp->known_val) & srcp->known_bits);
    uint32_t one_bits = (srcp->known_val & srcp->known_bits) &
        (srcp->known_val & srcp->known_bits);

    dstp->known_bits = zero_bits | one_bits;
    dstp->known_val = ((~zero_bits) | one_bits) & dstp->known_bits;
}

void jit_and_const32(struct il_code_block *block, unsigned slot_no,
                     unsigned const32) {
    struct jit_inst op;

    op.op = JIT_OP_AND_CONST32;

    op.immed.and_const32.slot_no = slot_no;
    op.immed.and_const32.const32 = const32;

    il_code_block_push_inst(block, &op);

    // cache known values.
    struct il_slot *slotp = block->slots + slot_no;

    uint32_t zero_bits = (~const32) | ((~slotp->known_val) & slotp->known_bits);
    uint32_t one_bits = const32 & slotp->known_val & slotp->known_bits;

    slotp->known_bits = zero_bits | one_bits;
    slotp->known_val = ((~zero_bits) | one_bits) & slotp->known_bits;
}

void jit_or(struct il_code_block *block, unsigned slot_src,
            unsigned slot_dst) {
    struct jit_inst op;

    op.op = JIT_OP_OR;
    op.immed.or.slot_src = slot_src;
    op.immed.or.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);

    // cache known values.
    struct il_slot const *srcp = block->slots + slot_src;
    struct il_slot *dstp = block->slots + slot_dst;

    /*
     * we know the value of all dst-bits in which one of the two src-bits is 1
     * (in which case the dst-bit is 1) or both of the two src-bits is 0 (in
     * which case the dst-bit is 0).  We do not know the value of a dst-bit if
     * only one of the two input-bits is known to be 0.
     */
    uint32_t zero_bits = ((~srcp->known_val) & srcp->known_bits) &
        ((~dstp->known_val) & dstp->known_bits);
    uint32_t one_bits = (srcp->known_val & srcp->known_bits) |
        (dstp->known_val & dstp->known_bits);

    dstp->known_bits = zero_bits | one_bits;
    dstp->known_val = ((~zero_bits) | one_bits) & dstp->known_bits;
}

void jit_or_const32(struct il_code_block *block, unsigned slot_no,
                    unsigned const32) {
    struct jit_inst op;

    op.op = JIT_OP_OR_CONST32;
    op.immed.or_const32.slot_no = slot_no;
    op.immed.or_const32.const32 = const32;

    il_code_block_push_inst(block, &op);

    // cache known values
    struct il_slot *slotp = block->slots + slot_no;

    /*
     * we know the value of all dst-bits in which one of the two src-bits is 1
     * (in which case the dst-bit is 1) or both of the two src-bits is 0 (in
     * which case the dst-bit is 0).  We do not know the value of a dst-bit if
     * only one of the two input-bits is known to be 0.
     */
    uint32_t zero_bits = (~const32) & ((~slotp->known_val) & slotp->known_bits);
    uint32_t one_bits = const32 | (slotp->known_val & slotp->known_bits);
    slotp->known_bits = zero_bits | one_bits;
    slotp->known_val = ((~zero_bits) | one_bits) & slotp->known_bits;
}

void jit_slot_to_bool(struct il_code_block *block, unsigned slot_no) {
    struct jit_inst op;

    op.op = JIT_OP_SLOT_TO_BOOL;
    op.immed.slot_to_bool.slot_no = slot_no;

    il_code_block_push_inst(block, &op);

    // cache known values
    struct il_slot *slotp = block->slots + slot_no;
    if (slotp->known_bits == 0xffffffff)
        slotp->known_val = slotp->known_val ? 1 : 0;
    else
        slotp->known_bits = 0;
}

void jit_not(struct il_code_block *block, unsigned slot_no) {
    struct jit_inst op;

    op.op = JIT_OP_NOT;
    op.immed.not.slot_no = slot_no;

    il_code_block_push_inst(block, &op);

    // cache known values
    struct il_slot *slotp = block->slots + slot_no;
    slotp->known_val = ~slotp->known_val;
}

void jit_shll(struct il_code_block *block, unsigned slot_no,
              unsigned shift_amt) {
    struct jit_inst op;

    op.op = JIT_OP_SHLL;
    op.immed.shll.slot_no = slot_no;
    op.immed.shll.shift_amt = shift_amt;

    il_code_block_push_inst(block, &op);

    // cache known values
    struct il_slot *slotp = block->slots + slot_no;
    slotp->known_val <<= shift_amt;
    if (shift_amt >= 32)
        slotp->known_bits = 0xffffffff; // all are zero
    else
        slotp->known_bits |= (1 << shift_amt) - 1;
}

void jit_shar(struct il_code_block *block, unsigned slot_no,
              unsigned shift_amt) {
    struct jit_inst op;

    op.op = JIT_OP_SHAR;
    op.immed.shar.slot_no = slot_no;
    op.immed.shar.shift_amt = shift_amt;

    il_code_block_push_inst(block, &op);

    // cache known values
    struct il_slot *slotp = block->slots + slot_no;
    slotp->known_val = ((int32_t)slotp->known_val) >> shift_amt;
    if (shift_amt >= 32)
        slotp->known_bits = 0xffffffff; // all are one
    else
        slotp->known_bits |= ~((1 << (31 - shift_amt)) - 1);
}

void jit_shlr(struct il_code_block *block, unsigned slot_no,
              unsigned shift_amt) {
    struct jit_inst op;

    op.op = JIT_OP_SHLR;
    op.immed.shlr.slot_no = slot_no;
    op.immed.shlr.shift_amt = shift_amt;

    il_code_block_push_inst(block, &op);

    // cache known values
    struct il_slot *slotp = block->slots + slot_no;
    slotp->known_val >>= shift_amt;
    if (shift_amt >= 32)
        slotp->known_bits = 0xffffffff; // all are zero
    else
        slotp->known_bits |= ~((1 << (31 - shift_amt)) - 1);
}

void jit_set_gt(struct il_code_block *block, unsigned slot_lhs,
                unsigned slot_rhs, unsigned slot_dst) {
    struct jit_inst op;

    op.op = JIT_OP_SET_GT;
    op.immed.set_gt.slot_lhs = slot_lhs;
    op.immed.set_gt.slot_rhs = slot_rhs;
    op.immed.set_gt.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);

    // cache known values
    struct il_slot *lhsp = block->slots + slot_lhs;
    struct il_slot *rhsp = block->slots + slot_rhs;
    struct il_slot *dstp = block->slots + slot_dst;

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
}

void jit_set_eq(struct il_code_block *block, unsigned slot_lhs,
                unsigned slot_rhs, unsigned slot_dst) {
    struct jit_inst op;

    op.op = JIT_OP_SET_EQ;
    op.immed.set_eq.slot_lhs = slot_lhs;
    op.immed.set_eq.slot_rhs = slot_rhs;
    op.immed.set_eq.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);

    // cache known values
    struct il_slot *lhsp = block->slots + slot_lhs;
    struct il_slot *rhsp = block->slots + slot_rhs;
    struct il_slot *dstp = block->slots + slot_dst;

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
}

void jit_set_ge(struct il_code_block *block, unsigned slot_lhs,
                unsigned slot_rhs, unsigned slot_dst) {
    struct jit_inst op;

    op.op = JIT_OP_SET_GE;
    op.immed.set_ge.slot_lhs = slot_lhs;
    op.immed.set_ge.slot_rhs = slot_rhs;
    op.immed.set_ge.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);

    // cache known values
    struct il_slot *lhsp = block->slots + slot_lhs;
    struct il_slot *rhsp = block->slots + slot_rhs;
    struct il_slot *dstp = block->slots + slot_dst;

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
}

void jit_mul_u32(struct il_code_block *block, unsigned slot_lhs,
                 unsigned slot_rhs, unsigned slot_dst) {
    struct jit_inst op;

    op.op = JIT_OP_MUL_U32;
    op.immed.mul_u32.slot_lhs = slot_lhs;
    op.immed.mul_u32.slot_rhs = slot_rhs;
    op.immed.mul_u32.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);

    /*
     * TODO: cache known values
     * this should be possible if the lower N bits of both src and dst are
     * known, but it seems complicated...
     */
    struct il_slot *dstp = block->slots + slot_dst;
    dstp->known_bits = 0;
    dstp->known_val = 0;
}
