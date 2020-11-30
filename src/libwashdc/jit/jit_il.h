/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018-2020 snickerbockers
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

#ifndef JIT_IL_H_
#define JIT_IL_H_

#include <stdbool.h>

#include "washdc/cpu.h"
#include "washdc/types.h"
#include "washdc/MemoryMap.h"

/*
 * Defines the number of slots available to IL programs.
 *
 * this limit is arbitrary; it is safe to make MAX_SLOTS bigger if necessary
 */
#define MAX_SLOTS (8 * 1024)

enum jit_opcode {
    // this opcode calls an interpreter function
    JIT_OP_FALLBACK,

    // This jumps to the jump destination address previously stored
    JIT_OP_JUMP,

    // conditionally set based on flag
    JIT_CSET,

    // this will set a register to the given constant value
    JIT_SET_SLOT,

    // this will set a register to point to something on te host machine
    JIT_SET_SLOT_HOST_PTR,

    /*
     * this will call a function which accepts a 32-bit int as its sole argument.
     *
     * the argument will be read from a slot.
     */
    JIT_OP_CALL_FUNC,

    /*
     * this will call a function which accepts a 32-bit int as its sole argument.
     *
     * the argument will be a constant immediate value
     */
    JIT_OP_CALL_FUNC_IMM32,

    // read 16 bits from a constant address and store them in a given slot
    JIT_OP_READ_16_CONSTADDR,

    // sign-extend an 8-bit int in a slot into a 32-bit int
    JIT_OP_SIGN_EXTEND_8,

    // sign-extend a 16-bit int in a slot into a 32-bit int
    JIT_OP_SIGN_EXTEND_16,

    // read a 32-bit int at a constant address into a slot
    JIT_OP_READ_32_CONSTADDR,

    // read a 8-bit int at an address contained in a slot into another slot
    JIT_OP_READ_8_SLOT,

    // read a 16-bit int at an address contained in a slot into another slot
    JIT_OP_READ_16_SLOT,

    // read a 32-bit int at an address contained in a slot into another slot
    JIT_OP_READ_32_SLOT,

    /*
     * read a 32-bit float at an address contained in a slot into a
     * floating-point slot
     */
    JIT_OP_READ_FLOAT_SLOT,

    /*
     * write an 8-bit int contained in a slot to memory at an address contained
     * in a slot
     */
    JIT_OP_WRITE_8_SLOT,

    /*
     * write a 16-bit int contained in a slot to memory at an address contained
     * in a slot
     */
    JIT_OP_WRITE_16_SLOT,

    /*
     * write a 32-bit int contained in a slot to memory at an address contained
     * in a slot
     */
    JIT_OP_WRITE_32_SLOT,

    /*
     * write a 32-bit float contained in a slot to memory at an address contained
     * in a slot
     */
    JIT_OP_WRITE_FLOAT_SLOT,

    /*
     * load 16-bits from a host memory address into a jit register
     * upper 16-bits should be zero-extended.
     */
    JIT_OP_LOAD_SLOT16,

    // load 32-bits from a host memory address into a jit register
    JIT_OP_LOAD_SLOT,

    /*
     * load 32-bits from a host memory address (addressed via address in a
     * slot + index) into another slot
     */
    JIT_OP_LOAD_SLOT_OFFSET,

    // load 32-bit float from a host memory address into a jit register
    JIT_OP_LOAD_FLOAT_SLOT,

    JIT_OP_LOAD_FLOAT_SLOT_OFFSET,

    // store 32-bits from a jit register address into a host memory address
    JIT_OP_STORE_SLOT,

    /*
     * store 32-bits to a host memory address (addressed via address in a
     * slot + index) from another slot
     */
    JIT_OP_STORE_SLOT_OFFSET,

    // store float from a jit register address into a host memory address
    JIT_OP_STORE_FLOAT_SLOT,

    JIT_OP_STORE_FLOAT_SLOT_OFFSET,

    // add one slot into another
    JIT_OP_ADD,

    // subtract one slot from another
    JIT_OP_SUB,

    // subtract one 32-bit floating point slot from another
    JIT_OP_SUB_FLOAT,

    // add one 32-bit floating point slot to another
    JIT_OP_ADD_FLOAT,

    // add a 32-bit constant value into a slot
    JIT_OP_ADD_CONST32,

    // xor one slot into another
    JIT_OP_XOR,

    // XOR one slot with a 32-bit constant
    JIT_OP_XOR_CONST32,

    // move one slot into another
    JIT_OP_MOV,

    // move one fpu slot into anoter
    JIT_OP_MOV_FLOAT,

    // AND one slot with another
    JIT_OP_AND,

    // AND one slot with a 32-bit constant
    JIT_OP_AND_CONST32,

    // OR one slot with another
    JIT_OP_OR,

    // OR one slot with a 32-bit constant
    JIT_OP_OR_CONST32,

    // set the given slot to 0 if any bits are set, else 1
    JIT_OP_SLOT_TO_BOOL_INV,

    // place one's-compliment of given slot into another slot
    JIT_OP_NOT,

    // left-shift the given slot by the given amount
    JIT_OP_SHLL,

    // arithmetic right-shift the given slot by the given amount
    JIT_OP_SHAR,

    // logical right-shift the given slot by the given amount
    JIT_OP_SHLR,

    /*
     * left-shift a slot (if the argument is positive) or right-shift the slot
     * (if the argument is negative) by the absolute value of the argument.
     */
    JIT_OP_SHAD,

    /*
     * takes three regs as input.  If the second reg is greater than the first
     * reg, then the third reg will be ORed with 1.  Else, the third reg
     * remains unchanged.
     */
    JIT_OP_SET_GT_UNSIGNED,
    JIT_OP_SET_GT_SIGNED,
    JIT_OP_SET_GT_SIGNED_CONST,

    JIT_OP_SET_EQ,

    JIT_OP_SET_GE_UNSIGNED,
    JIT_OP_SET_GE_SIGNED,
    JIT_OP_SET_GE_SIGNED_CONST,

    JIT_OP_SET_GT_FLOAT,

    /*
     * multiply two unsigned 32-bit slots together and place the result in a
     * third slot.
     */
    JIT_OP_MUL_U32,

    // it's like JIT_OP_MUL_U32, but for 32-bit floating points
    JIT_OP_MUL_FLOAT,

    // set a floating-point slot to 0.0f
    JIT_OP_CLEAR_FLOAT,

    /*
     * This tells the backend that a given slot is no longer needed and its
     * value does not need to be preserved.
     */
    JIT_OP_DISCARD_SLOT
};

struct jit_fallback_immed {
    void(*fallback_fn)(void*,cpu_inst_param);
    cpu_inst_param inst;
};

struct jump_immed {
    // this should point to the slot where the jump address is stored
    unsigned jmp_addr_slot;

    // this should point to the slot where the jump hash is stored
    unsigned jmp_hash_slot;
};

struct cset_immed {
    unsigned flag_slot, t_flag;

    uint32_t src_val;
    unsigned dst_slot;
};

struct set_slot_immed {
    unsigned slot_idx;
    uint32_t new_val;
};

struct set_slot_host_ptr_immed {
    unsigned slot_idx;
    void *ptr;
};

struct call_func_immed {
    void(*func)(void*,uint32_t);
    unsigned slot_no;
};

struct call_func_imm32_immed {
    void(*func)(void*,uint32_t);
    uint32_t imm32;
};

struct read_16_constaddr_immed {
    struct memory_map *map;
    addr32_t addr;
    unsigned slot_no;
};

struct sign_extend_8_immed {
    unsigned slot_no;
};

struct sign_extend_16_immed {
    unsigned slot_no;
};

struct read_32_constaddr_immed {
    struct memory_map *map;
    addr32_t addr;
    unsigned slot_no;
};

struct read_8_slot_immed {
    struct memory_map *map;
    unsigned addr_slot;
    unsigned dst_slot;
};

struct read_16_slot_immed {
    struct memory_map *map;
    unsigned addr_slot;
    unsigned dst_slot;
};

struct read_32_slot_immed {
    struct memory_map *map;
    unsigned addr_slot;
    unsigned dst_slot;
};

struct read_float_slot_immed {
    struct memory_map *map;
    unsigned addr_slot;
    unsigned dst_slot;
};

struct write_8_slot_immed {
    struct memory_map *map;
    unsigned src_slot;
    unsigned addr_slot;
};

struct write_16_slot_immed {
    struct memory_map *map;
    unsigned src_slot;
    unsigned addr_slot;
};

struct write_32_slot_immed {
    struct memory_map *map;
    unsigned src_slot;
    unsigned addr_slot;
};

struct write_float_slot_immed {
    struct memory_map *map;
    unsigned src_slot;
    unsigned addr_slot;
};

struct load_slot16_immed {
    uint16_t const *src;
    unsigned slot_no;
};

struct load_slot_immed {
    uint32_t const *src;
    unsigned slot_no;
};

struct load_slot_offset_immed {
    unsigned slot_base;
    unsigned index;
    unsigned slot_dst;
};

struct load_float_slot_immed {
    float const *src;
    unsigned slot_no;
};

struct load_float_slot_offset_immed {
    unsigned slot_base;
    unsigned index;
    unsigned slot_dst;
};

struct store_slot_immed {
    uint32_t *dst;
    unsigned slot_no;
};

struct store_slot_offset_immed {
    unsigned slot_base;
    unsigned index;
    unsigned slot_src;
};

struct store_float_slot_immed {
    float *dst;
    unsigned slot_no;
};

struct store_float_slot_offset_immed {
    unsigned slot_base;
    unsigned index;
    unsigned slot_src;
};

struct add_immed {
    unsigned slot_src, slot_dst;
};

struct sub_immed {
    unsigned slot_src, slot_dst;
};

struct sub_float_immed {
    unsigned slot_src, slot_dst;
};

struct add_float_immed {
    unsigned slot_src, slot_dst;
};

struct add_const32_immed {
    unsigned slot_dst;
    uint32_t const32;
};

struct discard_slot_immed {
    unsigned slot_no;
};

struct xor_immed {
    unsigned slot_src, slot_dst;
};

struct xor_const32_immed {
    unsigned slot_no;
    unsigned const32;
};

struct mov_immed {
    unsigned slot_src, slot_dst;
};

struct mov_float_immed {
    unsigned slot_src, slot_dst;
};

struct and_immed {
    unsigned slot_src, slot_dst;
};

struct and_const32_immed {
    unsigned slot_no;
    unsigned const32;
};

struct or_immed {
    unsigned slot_src, slot_dst;
};

struct or_const32_immed {
    unsigned slot_no;
    unsigned const32;
};

struct slot_to_bool_inv_immed {
    unsigned slot_no;
};

struct not_immed {
    unsigned slot_no;
};

struct shll_immed {
    unsigned slot_no;
    unsigned shift_amt;
};

struct shar_immed {
    unsigned slot_no;
    unsigned shift_amt;
};

struct shlr_immed {
    unsigned slot_no;
    unsigned shift_amt;
};

struct shad_immed {
    unsigned slot_val;
    unsigned slot_shift_amt;
};

struct set_gt_unsigned_immed {
    // dst |= 1 if lhs > rhs
    unsigned slot_lhs, slot_rhs;
    unsigned slot_dst;
};

struct set_gt_signed_immed {
    // dst |= 1 if lhs > rhs
    unsigned slot_lhs, slot_rhs;
    unsigned slot_dst;
};

struct set_gt_signed_const_immed {
    // dst |= 1 if lhs > rhs
    unsigned slot_lhs;
    unsigned slot_dst;
    int32_t imm_rhs;
};

struct set_eq_immed {
    // dst |= 1 if lhs == rhs
    unsigned slot_lhs, slot_rhs;
    unsigned slot_dst;
};

struct set_ge_unsigned_immed {
    // dst |= 1 if lhs >= rhs
    unsigned slot_lhs, slot_rhs;
    unsigned slot_dst;
};

struct set_ge_signed_immed {
    // dst |= 1 if lhs >= rhs
    unsigned slot_lhs, slot_rhs;
    unsigned slot_dst;
};

struct set_ge_signed_const_immed {
    // dst |= 1 if lhs >= rhs
    unsigned slot_lhs;
    unsigned slot_dst;
    int32_t imm_rhs;
};

struct set_gt_float_immed {
    /*
     * dst |= 1 if lhs > rhs
     *
     * lhs and rhs must be floating-point slots
     * dst must be a general-purpose slot.
     */
    unsigned slot_lhs, slot_rhs;
    unsigned slot_dst;
};

struct mul_u32_immed {
    unsigned slot_lhs, slot_rhs;
    unsigned slot_dst;
};

struct mul_float_immed {
    unsigned slot_lhs, slot_dst;
};

struct clear_float_immed {
    unsigned slot_dst;
};

union jit_immed {
    struct jit_fallback_immed fallback;
    struct jump_immed jump;
    struct cset_immed cset;
    struct set_slot_immed set_slot;
    struct set_slot_host_ptr_immed set_slot_host_ptr;
    struct call_func_immed call_func;
    struct call_func_imm32_immed call_func_imm32;
    struct read_16_constaddr_immed read_16_constaddr;
    struct sign_extend_16_immed sign_extend_8;
    struct sign_extend_16_immed sign_extend_16;
    struct read_32_constaddr_immed read_32_constaddr;
    struct read_8_slot_immed read_8_slot;
    struct read_16_slot_immed read_16_slot;
    struct read_32_slot_immed read_32_slot;
    struct read_float_slot_immed read_float_slot;
    struct write_8_slot_immed write_8_slot;
    struct write_16_slot_immed write_16_slot;
    struct write_32_slot_immed write_32_slot;
    struct write_float_slot_immed write_float_slot;
    struct load_slot16_immed load_slot16;
    struct load_slot_immed load_slot;
    struct load_slot_offset_immed load_slot_offset;
    struct load_float_slot_immed load_float_slot;
    struct load_float_slot_offset_immed load_float_slot_offset;
    struct store_slot_immed store_slot;
    struct store_slot_offset_immed store_slot_offset;
    struct store_float_slot_immed store_float_slot;
    struct store_float_slot_offset_immed store_float_slot_offset;
    struct add_immed add;
    struct sub_immed sub;
    struct sub_float_immed sub_float;
    struct add_float_immed add_float;
    struct add_const32_immed add_const32;
    struct discard_slot_immed discard_slot;
    struct xor_immed xor;
    struct xor_const32_immed xor_const32;
    struct mov_immed mov;
    struct mov_float_immed mov_float;
    struct and_immed and;
    struct and_const32_immed and_const32;
    struct or_immed or;
    struct or_const32_immed or_const32;
    struct slot_to_bool_inv_immed slot_to_bool_inv;
    struct not_immed not;
    struct shll_immed shll;
    struct shar_immed shar;
    struct shlr_immed shlr;
    struct shad_immed shad;
    struct set_gt_unsigned_immed set_gt_unsigned;
    struct set_gt_signed_immed set_gt_signed;
    struct set_gt_signed_const_immed set_gt_signed_const;
    struct set_eq_immed set_eq;
    struct set_ge_unsigned_immed set_ge_unsigned;
    struct set_ge_signed_immed set_ge_signed;
    struct set_ge_signed_const_immed set_ge_signed_const;
    struct set_gt_float_immed set_gt_float;
    struct mul_u32_immed mul_u32;
    struct mul_float_immed mul_float;
    struct clear_float_immed clear_float;
};

struct jit_inst {
    enum jit_opcode op;
    union jit_immed immed;
};

struct il_code_block;

// return true if the instruction reads from the given slot, else return false
bool jit_inst_is_read_slot(struct jit_inst const *inst, unsigned slot_no);

// return true if the instruction writes to the given slot, else return false
#define JIT_IL_MAX_WRITE_SLOTS 2
void jit_inst_get_write_slots(struct jit_inst const *inst,
                              int write_slots[JIT_IL_MAX_WRITE_SLOTS]);
bool jit_inst_is_write_slot(struct jit_inst const *inst, unsigned slot_no);

void jit_fallback(struct il_code_block *block,
                  void(*fallback_fn)(void*,cpu_inst_param), cpu_inst_param inst);
void jit_jump(struct il_code_block *block, unsigned jmp_addr_slot, unsigned jmp_hash_slot);
void jit_cset(struct il_code_block *block, unsigned flag_slot,
              unsigned t_flag, uint32_t src_val, unsigned dst_slot);
void jit_set_slot(struct il_code_block *block, unsigned slot_idx,
                  uint32_t new_val);
void jit_set_slot_host_ptr(struct il_code_block *block, unsigned slot_idx,
                           void *ptr);
void jit_call_func(struct il_code_block *block,
                   void(*func)(void*,uint32_t), unsigned slot_no);
void jit_call_func_imm32(struct il_code_block *block,
                         void(*func)(void*,uint32_t), uint32_t imm32);
void jit_read_16_constaddr(struct il_code_block *block, struct memory_map *map,
                           addr32_t addr, unsigned slot_no);
void jit_sign_extend_8(struct il_code_block *block, unsigned slot_no);
void jit_sign_extend_16(struct il_code_block *block, unsigned slot_no);
void jit_read_32_constaddr(struct il_code_block *block, struct memory_map *map,
                           addr32_t addr, unsigned slot_no);
void jit_read_8_slot(struct il_code_block *block, struct memory_map *map,
                     unsigned addr_slot, unsigned dst_slot);
void jit_read_16_slot(struct il_code_block *block, struct memory_map *map,
                      unsigned addr_slot, unsigned dst_slot);
void jit_read_32_slot(struct il_code_block *block, struct memory_map *map,
                      unsigned addr_slot, unsigned dst_slot);
void jit_read_float_slot(struct il_code_block *block, struct memory_map *map,
                         unsigned addr_slot, unsigned dst_slot);
void jit_write_8_slot(struct il_code_block *block, struct memory_map *map,
                      unsigned src_slot, unsigned addr_slot);
void jit_write_16_slot(struct il_code_block *block, struct memory_map *map,
                      unsigned src_slot, unsigned addr_slot);
void jit_write_32_slot(struct il_code_block *block, struct memory_map *map,
                       unsigned src_slot, unsigned addr_slot);
void jit_write_float_slot(struct il_code_block *block, struct memory_map *map,
                          unsigned src_slot, unsigned addr_slot);
void jit_load_slot(struct il_code_block *block, unsigned slot_no,
                   uint32_t const *src);
void jit_load_slot_offset(struct il_code_block *block, unsigned slot_base,
                           unsigned index, unsigned slot_dst);
void jit_load_float_slot(struct il_code_block *block, unsigned slot_no,
                   float const *src);
void jit_load_float_slot_offset(struct il_code_block *block,
                                 unsigned slot_base,
                                 unsigned index, unsigned slot_dst);
void jit_load_slot16(struct il_code_block *block, unsigned slot_no,
                     uint16_t const *src);
void jit_store_slot(struct il_code_block *block, unsigned slot_no,
                    uint32_t *dst);
void jit_store_slot_offset(struct il_code_block *block, unsigned slot_src,
                            unsigned slot_base, unsigned index);
void jit_store_float_slot(struct il_code_block *block, unsigned slot_no,
                          float *dst);
void jit_store_float_slot_offset(struct il_code_block *block, unsigned slot_src,
                                  unsigned slot_base, unsigned index);
void jit_add(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst);
void jit_sub(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst);
void jit_sub_float(struct il_code_block *block, unsigned slot_src,
                   unsigned slot_dst);
void jit_add_float(struct il_code_block *block, unsigned slot_src,
                   unsigned slot_dst);
void jit_add_const32(struct il_code_block *block, unsigned slot_no,
                     uint32_t const32);
void jit_discard_slot(struct il_code_block *block, unsigned slot_no);
void jit_xor(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst);
void jit_xor_const32(struct il_code_block *block, unsigned slot_no,
                     uint32_t const32);
void jit_mov(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst);
void jit_mov_float(struct il_code_block *block, unsigned slot_src,
                   unsigned slot_dst);
void jit_and(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst);
void jit_and_const32(struct il_code_block *block, unsigned slot_src,
                     unsigned const32);
void jit_or(struct il_code_block *block, unsigned slot_src,
            unsigned slot_dst);
void jit_or_const32(struct il_code_block *block, unsigned slot_no,
                    unsigned const32);
void jit_slot_to_bool_inv(struct il_code_block *block, unsigned slot_no);
void jit_not(struct il_code_block *block, unsigned slot_no);
void jit_shll(struct il_code_block *block, unsigned slot_no,
              unsigned shift_amt);
void jit_shar(struct il_code_block *block, unsigned slot_no,
              unsigned shift_amt);
void jit_shlr(struct il_code_block *block, unsigned slot_no,
              unsigned shift_amt);
void jit_shad(struct il_code_block *block, unsigned slot_val,
              unsigned slot_shift_amt);
void jit_set_gt_unsigned(struct il_code_block *block, unsigned slot_lhs,
                         unsigned slot_rhs, unsigned slot_dst);
void jit_set_gt_signed(struct il_code_block *block, unsigned slot_lhs,
                       unsigned slot_rhs, unsigned slot_dst);
void jit_set_gt_signed_const(struct il_code_block *block, unsigned slot_lhs,
                             unsigned imm_rhs, unsigned slot_dst);
void jit_set_eq(struct il_code_block *block, unsigned slot_lhs,
                unsigned slot_rhs, unsigned slot_dst);
void jit_set_ge_unsigned(struct il_code_block *block, unsigned slot_lhs,
                         unsigned slot_rhs, unsigned slot_dst);
void jit_set_ge_signed(struct il_code_block *block, unsigned slot_lhs,
                       unsigned slot_rhs, unsigned slot_dst);
void jit_set_ge_signed_const(struct il_code_block *block, unsigned slot_lhs,
                             unsigned imm_rhs, unsigned slot_dst);
void jit_set_gt_float(struct il_code_block *block, unsigned slot_lhs,
                      unsigned slot_rhs, unsigned slot_dst);
void jit_mul_u32(struct il_code_block *block, unsigned slot_lhs,
                 unsigned slot_rhs, unsigned slot_dst);
void jit_mul_float(struct il_code_block *block, unsigned slot_lhs,
                   unsigned slot_dst);
void jit_clear_float(struct il_code_block *block, unsigned slot_dst);

#endif
