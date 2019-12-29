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

#include "washdc/error.h"
#include "washdc/cpu.h"
#include "code_block.h"

#include "jit_il.h"

void jit_fallback(struct il_code_block *block,
                  void(*fallback_fn)(void*,cpu_inst_param), cpu_inst_param inst) {
    struct jit_inst op;

    op.op = JIT_OP_FALLBACK;
    op.immed.fallback.fallback_fn = fallback_fn;
    op.immed.fallback.inst = inst;

    il_code_block_push_inst(block, &op);
}

void jit_jump(struct il_code_block *block, unsigned jmp_addr_slot,
              unsigned jmp_hash_slot) {
    struct jit_inst op;

    check_slot(block, jmp_addr_slot, WASHDC_JIT_SLOT_GEN);
    check_slot(block, jmp_hash_slot, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_JUMP;
    op.immed.jump.jmp_addr_slot = jmp_addr_slot;
    op.immed.jump.jmp_hash_slot = jmp_hash_slot;

    il_code_block_push_inst(block, &op);
}

void jit_cset(struct il_code_block *block, unsigned flag_slot,
              unsigned t_flag, uint32_t src_val, unsigned dst_slot) {
    struct jit_inst op;

    check_slot(block, flag_slot, WASHDC_JIT_SLOT_GEN);
    check_slot(block, dst_slot, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_CSET;
    op.immed.cset.flag_slot = flag_slot;
    op.immed.cset.t_flag = t_flag;
    op.immed.cset.src_val = src_val;
    op.immed.cset.dst_slot = dst_slot;

    il_code_block_push_inst(block, &op);
}

void jit_set_slot(struct il_code_block *block, unsigned slot_idx,
                  uint32_t new_val) {
    struct jit_inst op;

    check_slot(block, slot_idx, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_SET_SLOT;
    op.immed.set_slot.new_val = new_val;
    op.immed.set_slot.slot_idx = slot_idx;

    il_code_block_push_inst(block, &op);
}

void jit_set_slot_host_ptr(struct il_code_block *block, unsigned slot_idx,
                  void *ptr) {
    struct jit_inst op;

    check_slot(block, slot_idx, WASHDC_JIT_SLOT_HOST_PTR);

    op.op = JIT_SET_SLOT_HOST_PTR;
    op.immed.set_slot_host_ptr.ptr = ptr;
    op.immed.set_slot_host_ptr.slot_idx = slot_idx;

    il_code_block_push_inst(block, &op);
}

void jit_call_func(struct il_code_block *block,
                   void(*func)(void*,uint32_t), unsigned slot_no) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_CALL_FUNC;
    op.immed.call_func.slot_no = slot_no;
    op.immed.call_func.func = func;

    il_code_block_push_inst(block, &op);
}

void jit_read_16_constaddr(struct il_code_block *block, struct memory_map *map,
                           addr32_t addr, unsigned slot_no) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_READ_16_CONSTADDR;
    op.immed.read_16_constaddr.map = map;
    op.immed.read_16_constaddr.addr = addr;
    op.immed.read_16_constaddr.slot_no = slot_no;

    il_code_block_push_inst(block, &op);
}

void jit_sign_extend_8(struct il_code_block *block, unsigned slot_no) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_SIGN_EXTEND_8;
    op.immed.sign_extend_8.slot_no = slot_no;

    il_code_block_push_inst(block, &op);
}

void jit_sign_extend_16(struct il_code_block *block, unsigned slot_no) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_SIGN_EXTEND_16;
    op.immed.sign_extend_16.slot_no = slot_no;

    il_code_block_push_inst(block, &op);
}

void jit_read_32_constaddr(struct il_code_block *block, struct memory_map *map,
                           addr32_t addr, unsigned slot_no) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_READ_32_CONSTADDR;
    op.immed.read_32_constaddr.map = map;
    op.immed.read_32_constaddr.addr = addr;
    op.immed.read_32_constaddr.slot_no = slot_no;

    il_code_block_push_inst(block, &op);
}

void jit_read_8_slot(struct il_code_block *block, struct memory_map *map,
                     unsigned addr_slot, unsigned dst_slot) {
    struct jit_inst op;

    check_slot(block, addr_slot, WASHDC_JIT_SLOT_GEN);
    check_slot(block, dst_slot, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_READ_8_SLOT;
    op.immed.read_8_slot.map = map;
    op.immed.read_8_slot.addr_slot = addr_slot;
    op.immed.read_8_slot.dst_slot = dst_slot;

    il_code_block_push_inst(block, &op);
}

void jit_read_16_slot(struct il_code_block *block, struct memory_map *map,
                      unsigned addr_slot, unsigned dst_slot) {
    struct jit_inst op;

    check_slot(block, addr_slot, WASHDC_JIT_SLOT_GEN);
    check_slot(block, dst_slot, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_READ_16_SLOT;
    op.immed.read_16_slot.map = map;
    op.immed.read_16_slot.addr_slot = addr_slot;
    op.immed.read_16_slot.dst_slot = dst_slot;

    il_code_block_push_inst(block, &op);
}

void jit_read_32_slot(struct il_code_block *block, struct memory_map *map,
                      unsigned addr_slot, unsigned dst_slot) {
    struct jit_inst op;

    check_slot(block, addr_slot, WASHDC_JIT_SLOT_GEN);
    check_slot(block, dst_slot, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_READ_32_SLOT;
    op.immed.read_32_slot.map = map;
    op.immed.read_32_slot.addr_slot = addr_slot;
    op.immed.read_32_slot.dst_slot = dst_slot;

    il_code_block_push_inst(block, &op);
}

void jit_read_float_slot(struct il_code_block *block, struct memory_map *map,
                         unsigned addr_slot, unsigned dst_slot) {
    struct jit_inst op;

    check_slot(block, addr_slot, WASHDC_JIT_SLOT_GEN);
    check_slot(block, dst_slot, WASHDC_JIT_SLOT_FLOAT);

    op.op = JIT_OP_READ_FLOAT_SLOT;
    op.immed.read_float_slot.map = map;
    op.immed.read_float_slot.addr_slot = addr_slot;
    op.immed.read_float_slot.dst_slot = dst_slot;

    il_code_block_push_inst(block, &op);
}

void jit_write_8_slot(struct il_code_block *block, struct memory_map *map,
                      unsigned src_slot, unsigned addr_slot) {
    struct jit_inst op;

    check_slot(block, src_slot, WASHDC_JIT_SLOT_GEN);
    check_slot(block, addr_slot, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_WRITE_8_SLOT;
    op.immed.write_8_slot.map = map;
    op.immed.write_8_slot.addr_slot = addr_slot;
    op.immed.write_8_slot.src_slot = src_slot;

    il_code_block_push_inst(block, &op);
}

void jit_write_32_slot(struct il_code_block *block, struct memory_map *map,
                       unsigned src_slot, unsigned addr_slot) {
    struct jit_inst op;

    check_slot(block, src_slot, WASHDC_JIT_SLOT_GEN);
    check_slot(block, addr_slot, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_WRITE_32_SLOT;
    op.immed.write_32_slot.map = map;
    op.immed.write_32_slot.addr_slot = addr_slot;
    op.immed.write_32_slot.src_slot = src_slot;

    il_code_block_push_inst(block, &op);
}

void jit_write_float_slot(struct il_code_block *block, struct memory_map *map,
                          unsigned src_slot, unsigned addr_slot) {
    struct jit_inst op;

    check_slot(block, src_slot, WASHDC_JIT_SLOT_FLOAT);
    check_slot(block, addr_slot, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_WRITE_FLOAT_SLOT;
    op.immed.write_float_slot.map = map;
    op.immed.write_float_slot.addr_slot = addr_slot;
    op.immed.write_float_slot.src_slot = src_slot;

    il_code_block_push_inst(block, &op);
}

void jit_load_slot16(struct il_code_block *block, unsigned slot_no,
                     uint16_t const *src) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_LOAD_SLOT16;
    op.immed.load_slot16.src = src;
    op.immed.load_slot16.slot_no = slot_no;

    il_code_block_push_inst(block, &op);
}

void jit_load_slot(struct il_code_block *block, unsigned slot_no,
                   uint32_t const *src) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_LOAD_SLOT;
    op.immed.load_slot.src = src;
    op.immed.load_slot.slot_no = slot_no;

    il_code_block_push_inst(block, &op);
}

void jit_load_slot_indexed(struct il_code_block *block, unsigned slot_base,
                           unsigned index, unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_base, WASHDC_JIT_SLOT_HOST_PTR);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_LOAD_SLOT_INDEXED;
    op.immed.load_slot_indexed.slot_base = slot_base;
    op.immed.load_slot_indexed.index = index;
    op.immed.load_slot_indexed.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_load_float_slot(struct il_code_block *block, unsigned slot_no,
                         float const *src) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_FLOAT);

    op.op = JIT_OP_LOAD_FLOAT_SLOT;
    op.immed.load_float_slot.src = src;
    op.immed.load_float_slot.slot_no = slot_no;

    il_code_block_push_inst(block, &op);
}

void jit_load_float_slot_indexed(struct il_code_block *block,
                                 unsigned slot_base,
                                 unsigned index, unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_base, WASHDC_JIT_SLOT_HOST_PTR);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_FLOAT);

    op.op = JIT_OP_LOAD_FLOAT_SLOT_INDEXED;
    op.immed.load_float_slot_indexed.slot_base = slot_base;
    op.immed.load_float_slot_indexed.index = index;
    op.immed.load_float_slot_indexed.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_store_slot(struct il_code_block *block, unsigned slot_no,
                    uint32_t *dst) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_STORE_SLOT;
    op.immed.store_slot.dst = dst;
    op.immed.store_slot.slot_no = slot_no;

    il_code_block_push_inst(block, &op);
}

void jit_store_slot_indexed(struct il_code_block *block, unsigned slot_src,
                            unsigned slot_base, unsigned index) {
    struct jit_inst op;

    check_slot(block, slot_src, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_base, WASHDC_JIT_SLOT_HOST_PTR);

    op.op = JIT_OP_STORE_SLOT_INDEXED;
    op.immed.store_slot_indexed.slot_src = slot_src;
    op.immed.store_slot_indexed.index = index;
    op.immed.store_slot_indexed.slot_base = slot_base;

    il_code_block_push_inst(block, &op);
}

void jit_store_float_slot(struct il_code_block *block, unsigned slot_no,
                          float *dst) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_FLOAT);

    op.op = JIT_OP_STORE_FLOAT_SLOT;
    op.immed.store_float_slot.dst = dst;
    op.immed.store_float_slot.slot_no = slot_no;

    il_code_block_push_inst(block, &op);
}

void
jit_store_float_slot_indexed(struct il_code_block *block, unsigned slot_src,
                             unsigned slot_base, unsigned index) {
    struct jit_inst op;

    check_slot(block, slot_src, WASHDC_JIT_SLOT_FLOAT);
    check_slot(block, slot_base, WASHDC_JIT_SLOT_HOST_PTR);

    op.op = JIT_OP_STORE_FLOAT_SLOT_INDEXED;
    op.immed.store_float_slot_indexed.slot_src = slot_src;
    op.immed.store_float_slot_indexed.index = index;
    op.immed.store_float_slot_indexed.slot_base = slot_base;

    il_code_block_push_inst(block, &op);
}

void jit_add(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_src, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_ADD;
    op.immed.add.slot_src = slot_src;
    op.immed.add.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_sub(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_src, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_SUB;
    op.immed.sub.slot_src = slot_src;
    op.immed.sub.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_sub_float(struct il_code_block *block, unsigned slot_src,
                   unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_src, WASHDC_JIT_SLOT_FLOAT);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_FLOAT);

    op.op = JIT_OP_SUB_FLOAT;
    op.immed.sub_float.slot_src = slot_src;
    op.immed.sub_float.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_add_const32(struct il_code_block *block, unsigned slot_dst,
                     uint32_t const32) {
    struct jit_inst op;

    check_slot(block, slot_dst, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_ADD_CONST32;
    op.immed.add_const32.slot_dst = slot_dst;
    op.immed.add_const32.const32 = const32;

    il_code_block_push_inst(block, &op);
}

void jit_discard_slot(struct il_code_block *block, unsigned slot_no) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_DISCARD_SLOT;
    op.immed.discard_slot.slot_no = slot_no;

    il_code_block_push_inst(block, &op);
}

void jit_xor(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_src, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_XOR;
    op.immed.xor.slot_src = slot_src;
    op.immed.xor.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_xor_const32(struct il_code_block *block, unsigned slot_no,
                     uint32_t const32) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_XOR_CONST32;

    op.immed.xor_const32.slot_no = slot_no;
    op.immed.xor_const32.const32 = const32;

    il_code_block_push_inst(block, &op);
}

void jit_mov(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_src, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_MOV;
    op.immed.mov.slot_src = slot_src;
    op.immed.mov.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_mov_float(struct il_code_block *block, unsigned slot_src,
                   unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_src, WASHDC_JIT_SLOT_FLOAT);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_FLOAT);

    op.op = JIT_OP_MOV_FLOAT;
    op.immed.mov_float.slot_src = slot_src;
    op.immed.mov_float.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_and(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_src, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_AND;
    op.immed.and.slot_src = slot_src;
    op.immed.and.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_and_const32(struct il_code_block *block, unsigned slot_no,
                     unsigned const32) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_AND_CONST32;

    op.immed.and_const32.slot_no = slot_no;
    op.immed.and_const32.const32 = const32;

    il_code_block_push_inst(block, &op);
}

void jit_or(struct il_code_block *block, unsigned slot_src,
            unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_src, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_OR;
    op.immed.or.slot_src = slot_src;
    op.immed.or.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_or_const32(struct il_code_block *block, unsigned slot_no,
                    unsigned const32) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_OR_CONST32;
    op.immed.or_const32.slot_no = slot_no;
    op.immed.or_const32.const32 = const32;

    il_code_block_push_inst(block, &op);
}

void jit_slot_to_bool(struct il_code_block *block, unsigned slot_no) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_SLOT_TO_BOOL;
    op.immed.slot_to_bool.slot_no = slot_no;

    il_code_block_push_inst(block, &op);
}

void jit_not(struct il_code_block *block, unsigned slot_no) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_NOT;
    op.immed.not.slot_no = slot_no;

    il_code_block_push_inst(block, &op);
}

void jit_shll(struct il_code_block *block, unsigned slot_no,
              unsigned shift_amt) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_SHLL;
    op.immed.shll.slot_no = slot_no;
    op.immed.shll.shift_amt = shift_amt;

    il_code_block_push_inst(block, &op);
}

void jit_shar(struct il_code_block *block, unsigned slot_no,
              unsigned shift_amt) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_SHAR;
    op.immed.shar.slot_no = slot_no;
    op.immed.shar.shift_amt = shift_amt;

    il_code_block_push_inst(block, &op);
}

void jit_shlr(struct il_code_block *block, unsigned slot_no,
              unsigned shift_amt) {
    struct jit_inst op;

    check_slot(block, slot_no, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_SHLR;
    op.immed.shlr.slot_no = slot_no;
    op.immed.shlr.shift_amt = shift_amt;

    il_code_block_push_inst(block, &op);
}

void jit_set_gt_unsigned(struct il_code_block *block, unsigned slot_lhs,
                         unsigned slot_rhs, unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_lhs, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_rhs, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_SET_GT_UNSIGNED;
    op.immed.set_gt_unsigned.slot_lhs = slot_lhs;
    op.immed.set_gt_unsigned.slot_rhs = slot_rhs;
    op.immed.set_gt_unsigned.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_set_gt_signed(struct il_code_block *block, unsigned slot_lhs,
                       unsigned slot_rhs, unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_lhs, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_rhs, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_SET_GT_SIGNED;
    op.immed.set_gt_signed.slot_lhs = slot_lhs;
    op.immed.set_gt_signed.slot_rhs = slot_rhs;
    op.immed.set_gt_signed.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_set_gt_signed_const(struct il_code_block *block, unsigned slot_lhs,
                             unsigned imm_rhs, unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_lhs, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_SET_GT_SIGNED_CONST;
    op.immed.set_gt_signed_const.slot_lhs = slot_lhs;
    op.immed.set_gt_signed_const.imm_rhs = imm_rhs;
    op.immed.set_gt_signed_const.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_set_eq(struct il_code_block *block, unsigned slot_lhs,
                unsigned slot_rhs, unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_lhs, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_rhs, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_SET_EQ;
    op.immed.set_eq.slot_lhs = slot_lhs;
    op.immed.set_eq.slot_rhs = slot_rhs;
    op.immed.set_eq.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_set_ge_unsigned(struct il_code_block *block, unsigned slot_lhs,
                         unsigned slot_rhs, unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_lhs, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_rhs, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_SET_GE_UNSIGNED;
    op.immed.set_ge_unsigned.slot_lhs = slot_lhs;
    op.immed.set_ge_unsigned.slot_rhs = slot_rhs;
    op.immed.set_ge_unsigned.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_set_ge_signed(struct il_code_block *block, unsigned slot_lhs,
                       unsigned slot_rhs, unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_lhs, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_rhs, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_SET_GE_SIGNED;
    op.immed.set_ge_signed.slot_lhs = slot_lhs;
    op.immed.set_ge_signed.slot_rhs = slot_rhs;
    op.immed.set_ge_signed.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_set_ge_signed_const(struct il_code_block *block, unsigned slot_lhs,
                             unsigned imm_rhs, unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_lhs, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_SET_GE_SIGNED_CONST;
    op.immed.set_ge_signed_const.slot_lhs = slot_lhs;
    op.immed.set_ge_signed_const.imm_rhs = imm_rhs;
    op.immed.set_ge_signed_const.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_mul_u32(struct il_code_block *block, unsigned slot_lhs,
                 unsigned slot_rhs, unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_lhs, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_rhs, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_MUL_U32;
    op.immed.mul_u32.slot_lhs = slot_lhs;
    op.immed.mul_u32.slot_rhs = slot_rhs;
    op.immed.mul_u32.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_mul_float(struct il_code_block *block, unsigned slot_lhs,
                   unsigned slot_dst) {
    struct jit_inst op;

    check_slot(block, slot_lhs, WASHDC_JIT_SLOT_FLOAT);
    check_slot(block, slot_dst, WASHDC_JIT_SLOT_FLOAT);

    op.op = JIT_OP_MUL_FLOAT;
    op.immed.mul_float.slot_lhs = slot_lhs;
    op.immed.mul_float.slot_dst = slot_dst;

    il_code_block_push_inst(block, &op);
}

void jit_shad(struct il_code_block *block, unsigned slot_val,
              unsigned slot_shift_amt) {
    struct jit_inst op;

    check_slot(block, slot_val, WASHDC_JIT_SLOT_GEN);
    check_slot(block, slot_shift_amt, WASHDC_JIT_SLOT_GEN);

    op.op = JIT_OP_SHAD;
    op.immed.shad.slot_val = slot_val;
    op.immed.shad.slot_shift_amt = slot_shift_amt;

    il_code_block_push_inst(block, &op);
}

bool jit_inst_is_read_slot(struct jit_inst const *inst, unsigned slot_no) {
    union jit_immed const *immed = &inst->immed;
    switch (inst->op) {
    case JIT_OP_FALLBACK:
        return false;
    case JIT_OP_JUMP:
        return slot_no == immed->jump.jmp_addr_slot ||
            slot_no == immed->jump.jmp_hash_slot;
    case JIT_CSET:
        return slot_no == immed->cset.flag_slot ||
            slot_no == immed->cset.dst_slot;
    case JIT_SET_SLOT:
        return false;
    case JIT_SET_SLOT_HOST_PTR:
        return false;
    case JIT_OP_CALL_FUNC:
        return slot_no == immed->call_func.slot_no;
    case JIT_OP_READ_16_CONSTADDR:
        return false;
    case JIT_OP_SIGN_EXTEND_8:
        return slot_no == immed->sign_extend_8.slot_no;
    case JIT_OP_SIGN_EXTEND_16:
        return slot_no == immed->sign_extend_16.slot_no;
    case JIT_OP_READ_32_CONSTADDR:
        return false;
    case JIT_OP_READ_8_SLOT:
        return slot_no == immed->read_8_slot.addr_slot;
    case JIT_OP_READ_16_SLOT:
        return slot_no == immed->read_16_slot.addr_slot;
    case JIT_OP_READ_32_SLOT:
        return slot_no == immed->read_32_slot.addr_slot;
    case JIT_OP_READ_FLOAT_SLOT:
        return slot_no == immed->read_float_slot.addr_slot;
    case JIT_OP_WRITE_8_SLOT:
        return slot_no == immed->write_8_slot.addr_slot ||
            slot_no == immed->write_8_slot.src_slot;
    case JIT_OP_WRITE_32_SLOT:
        return slot_no == immed->write_32_slot.addr_slot ||
            slot_no == immed->write_32_slot.src_slot;
    case JIT_OP_WRITE_FLOAT_SLOT:
        return slot_no == immed->write_float_slot.addr_slot ||
            slot_no == immed->write_float_slot.src_slot;
    case JIT_OP_LOAD_SLOT16:
        return false;
    case JIT_OP_LOAD_SLOT:
        return false;
    case JIT_OP_LOAD_SLOT_INDEXED:
        return slot_no == immed->load_slot_indexed.slot_base;
    case JIT_OP_LOAD_FLOAT_SLOT:
        return false;
    case JIT_OP_LOAD_FLOAT_SLOT_INDEXED:
        return slot_no == immed->load_float_slot_indexed.slot_base;
    case JIT_OP_STORE_SLOT:
        return slot_no == immed->store_slot.slot_no;
    case JIT_OP_STORE_SLOT_INDEXED:
        return slot_no == immed->store_slot_indexed.slot_src ||
            slot_no == immed->store_slot_indexed.slot_base;
    case JIT_OP_STORE_FLOAT_SLOT:
        return slot_no == immed->store_float_slot.slot_no;
    case JIT_OP_STORE_FLOAT_SLOT_INDEXED:
        return slot_no == immed->store_float_slot_indexed.slot_src ||
            slot_no == immed->store_float_slot_indexed.slot_base;
    case JIT_OP_ADD:
        return slot_no == immed->add.slot_src || slot_no == immed->add.slot_dst;
    case JIT_OP_SUB:
        return slot_no == immed->sub.slot_src || slot_no == immed->sub.slot_dst;
    case JIT_OP_SUB_FLOAT:
        return slot_no == immed->sub_float.slot_src || slot_no == immed->sub_float.slot_dst;
    case JIT_OP_ADD_CONST32:
        return slot_no == immed->add_const32.slot_dst;
    case JIT_OP_DISCARD_SLOT:
        return false;
    case JIT_OP_XOR:
        return slot_no == immed->xor.slot_src || slot_no == immed->xor.slot_dst;
    case JIT_OP_XOR_CONST32:
        return slot_no == immed->xor_const32.slot_no;
    case JIT_OP_MOV:
        return slot_no == immed->mov.slot_src;
    case JIT_OP_MOV_FLOAT:
        return slot_no == immed->mov_float.slot_src;
    case JIT_OP_AND:
        return slot_no == immed->and.slot_src || slot_no == immed->and.slot_dst;
    case JIT_OP_AND_CONST32:
        return slot_no == immed->and_const32.slot_no;
    case JIT_OP_OR:
        return slot_no == immed->or.slot_src || slot_no == immed->or.slot_dst;
    case JIT_OP_OR_CONST32:
        return slot_no == immed->or_const32.slot_no;
    case JIT_OP_SLOT_TO_BOOL:
        return slot_no == immed->slot_to_bool.slot_no;
    case JIT_OP_NOT:
        return slot_no == immed->not.slot_no;
    case JIT_OP_SHLL:
        return slot_no == immed->shll.slot_no;
    case JIT_OP_SHAR:
        return slot_no == immed->shar.slot_no;
    case JIT_OP_SHLR:
        return slot_no == immed->shlr.slot_no;
    case JIT_OP_SHAD:
        return slot_no == immed->shad.slot_val ||
            slot_no == immed->shad.slot_shift_amt;
    case JIT_OP_SET_GT_UNSIGNED:
        return slot_no == immed->set_gt_unsigned.slot_lhs ||
            slot_no == immed->set_gt_unsigned.slot_rhs ||
            slot_no == immed->set_gt_unsigned.slot_dst;
    case JIT_OP_SET_GT_SIGNED:
        return slot_no == immed->set_gt_signed.slot_lhs ||
            slot_no == immed->set_gt_signed.slot_rhs ||
            slot_no == immed->set_gt_signed.slot_dst;
    case JIT_OP_SET_GT_SIGNED_CONST:
        return slot_no == immed->set_gt_signed_const.slot_lhs ||
            slot_no == immed->set_gt_signed_const.slot_dst;
    case JIT_OP_SET_EQ:
        return slot_no == immed->set_eq.slot_lhs ||
            slot_no == immed->set_eq.slot_rhs ||
            slot_no == immed->set_eq.slot_dst;
    case JIT_OP_SET_GE_UNSIGNED:
        return slot_no == immed->set_ge_unsigned.slot_lhs ||
            slot_no == immed->set_ge_unsigned.slot_rhs ||
            slot_no == immed->set_ge_unsigned.slot_dst;
    case JIT_OP_SET_GE_SIGNED:
        return slot_no == immed->set_ge_signed.slot_lhs ||
            slot_no == immed->set_ge_signed.slot_rhs ||
            slot_no == immed->set_ge_signed.slot_dst;
    case JIT_OP_SET_GE_SIGNED_CONST:
        return slot_no == immed->set_ge_signed_const.slot_lhs ||
            slot_no == immed->set_ge_signed_const.slot_dst;
    case JIT_OP_MUL_U32:
        return slot_no == immed->mul_u32.slot_lhs ||
            slot_no == immed->mul_u32.slot_rhs;
    case JIT_OP_MUL_FLOAT:
        return slot_no == immed->mul_float.slot_lhs ||
            slot_no == immed->mul_float.slot_dst;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

void jit_inst_get_write_slots(struct jit_inst const *inst,
                              int write_slots[JIT_IL_MAX_WRITE_SLOTS]) {
    for (int idx = 0; idx < JIT_IL_MAX_WRITE_SLOTS; idx++)
        write_slots[idx] = -1;

    union jit_immed const *immed = &inst->immed;
    switch (inst->op) {
    case JIT_OP_FALLBACK:
        break;
    case JIT_OP_JUMP:
        break;
    case JIT_CSET:
        write_slots[0] = immed->cset.dst_slot;
        break;
    case JIT_SET_SLOT:
        write_slots[0] = immed->set_slot.slot_idx;
        break;
    case JIT_SET_SLOT_HOST_PTR:
        write_slots[0] = immed->set_slot_host_ptr.slot_idx;
        break;
    case JIT_OP_CALL_FUNC:
        break;
    case JIT_OP_READ_16_CONSTADDR:
        write_slots[0] = immed->read_16_constaddr.slot_no;
        break;
    case JIT_OP_SIGN_EXTEND_8:
        write_slots[0] = immed->sign_extend_8.slot_no;
        break;
    case JIT_OP_SIGN_EXTEND_16:
        write_slots[0] = immed->sign_extend_16.slot_no;
        break;
    case JIT_OP_READ_32_CONSTADDR:
        write_slots[0] = immed->read_32_constaddr.slot_no;
        break;
    case JIT_OP_READ_8_SLOT:
        write_slots[0] = immed->read_8_slot.dst_slot;
        break;
    case JIT_OP_READ_16_SLOT:
        write_slots[0] = immed->read_16_slot.dst_slot;
        break;
    case JIT_OP_READ_32_SLOT:
        write_slots[0] = immed->read_32_slot.dst_slot;
        break;
    case JIT_OP_READ_FLOAT_SLOT:
        write_slots[0] = immed->read_float_slot.dst_slot;
        break;
    case JIT_OP_WRITE_8_SLOT:
        break;
    case JIT_OP_WRITE_32_SLOT:
        break;
    case JIT_OP_WRITE_FLOAT_SLOT:
        break;
    case JIT_OP_LOAD_SLOT16:
        write_slots[0] = immed->load_slot16.slot_no;
        break;
    case JIT_OP_LOAD_SLOT:
        write_slots[0] = immed->load_slot.slot_no;
        break;
    case JIT_OP_LOAD_SLOT_INDEXED:
        write_slots[0] = immed->load_slot_indexed.slot_dst;
        break;
    case JIT_OP_LOAD_FLOAT_SLOT:
        write_slots[0] = immed->load_float_slot.slot_no;
        break;
    case JIT_OP_LOAD_FLOAT_SLOT_INDEXED:
        write_slots[0] = immed->load_float_slot_indexed.slot_dst;
        break;
    case JIT_OP_STORE_SLOT:
        break;
    case JIT_OP_STORE_SLOT_INDEXED:
        break;
    case JIT_OP_STORE_FLOAT_SLOT:
        break;
    case JIT_OP_STORE_FLOAT_SLOT_INDEXED:
        break;
    case JIT_OP_ADD:
        write_slots[0] = immed->add.slot_dst;
        break;
    case JIT_OP_SUB:
        write_slots[0] = immed->sub.slot_dst;
        break;
    case JIT_OP_SUB_FLOAT:
        write_slots[0] = immed->sub_float.slot_dst;
        break;
    case JIT_OP_ADD_CONST32:
        write_slots[0] = immed->add_const32.slot_dst;
        break;
    case JIT_OP_DISCARD_SLOT:
        break;
    case JIT_OP_XOR:
        write_slots[0] = immed->xor.slot_dst;
        break;
    case JIT_OP_XOR_CONST32:
        write_slots[0] = immed->xor_const32.slot_no;
        break;
    case JIT_OP_MOV:
        write_slots[0] = immed->mov.slot_dst;
        break;
    case JIT_OP_MOV_FLOAT:
        write_slots[0] = immed->mov.slot_dst;
        break;
    case JIT_OP_AND:
        write_slots[0] = immed->and.slot_dst;
        break;
    case JIT_OP_AND_CONST32:
        write_slots[0] = immed->and_const32.slot_no;
        break;
    case JIT_OP_OR:
        write_slots[0] = immed->or.slot_dst;
        break;
    case JIT_OP_OR_CONST32:
        write_slots[0] = immed->or_const32.slot_no;
        break;
    case JIT_OP_SLOT_TO_BOOL:
        write_slots[0] = immed->slot_to_bool.slot_no;
        break;
    case JIT_OP_NOT:
        write_slots[0] = immed->not.slot_no;
        break;
    case JIT_OP_SHLL:
        write_slots[0] = immed->shll.slot_no;
        break;
    case JIT_OP_SHAR:
        write_slots[0] = immed->shar.slot_no;
        break;
    case JIT_OP_SHLR:
        write_slots[0] = immed->shlr.slot_no;
        break;
    case JIT_OP_SHAD:
        write_slots[0] = immed->shad.slot_val;
        break;
    case JIT_OP_SET_GT_UNSIGNED:
        write_slots[0] = immed->set_gt_unsigned.slot_dst;
        break;
    case JIT_OP_SET_GT_SIGNED:
        write_slots[0] = immed->set_gt_signed.slot_dst;
        break;
    case JIT_OP_SET_GT_SIGNED_CONST:
        write_slots[0] = immed->set_gt_signed_const.slot_dst;
        break;
    case JIT_OP_SET_EQ:
        write_slots[0] = immed->set_eq.slot_dst;
        break;
    case JIT_OP_SET_GE_UNSIGNED:
        write_slots[0] = immed->set_ge_unsigned.slot_dst;
        break;
    case JIT_OP_SET_GE_SIGNED:
        write_slots[0] = immed->set_ge_signed.slot_dst;
        break;
    case JIT_OP_SET_GE_SIGNED_CONST:
        write_slots[0] = immed->set_ge_signed_const.slot_dst;
        break;
    case JIT_OP_MUL_U32:
        write_slots[0] = immed->mul_u32.slot_dst;
        break;
    case JIT_OP_MUL_FLOAT:
        write_slots[0] = immed->mul_float.slot_dst;
        break;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

bool jit_inst_is_write_slot(struct jit_inst const *inst, unsigned slot_no) {
    int idx;
    int write_slots[JIT_IL_MAX_WRITE_SLOTS];

    jit_inst_get_write_slots(inst, write_slots);
    for (idx = 0; idx < JIT_IL_MAX_WRITE_SLOTS; idx++)
        if (slot_no == write_slots[idx])
            return true;
    return false;
}
