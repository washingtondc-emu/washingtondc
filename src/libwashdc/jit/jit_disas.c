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

#include "jit_disas.h"

void jit_disas_il(FILE *out, struct jit_inst const *inst, int idx) {
    enum jit_opcode op = inst->op;
    union jit_immed const *immed = &inst->immed;

    switch (op) {
    case JIT_OP_FALLBACK:
        fprintf(out, "%02X: FALLBACK %p(0x%04x)\n",
                idx, immed->fallback.fallback_fn, (int)immed->fallback.inst);
        break;
    case JIT_OP_JUMP:
        fprintf(out, "%02X: JUMP <SLOT %02X, SLOT %02X>\n",
                idx, immed->jump.jmp_addr_slot, immed->jump.jmp_hash_slot);
        break;
    case JIT_JUMP_COND:
        fprintf(out,
                "%02X: JUMP_COND <SLOT %02X, SLOT %02X> IF "
                "(<SLOT %02X> & 1) == %u ELSE <SLOT %02X, SLOT %02X>\n", idx,
                immed->jump_cond.jmp_addr_slot, immed->jump_cond.jmp_hash_slot,
                immed->jump_cond.flag_slot, immed->jump_cond.t_flag,
                immed->jump_cond.alt_jmp_addr_slot,
                immed->jump_cond.alt_jmp_hash_slot);
        break;
    case JIT_SET_SLOT:
        fprintf(out, "%02X: SET %08X, <SLOT %02X>\n", idx,
                (unsigned)immed->set_slot.new_val, immed->set_slot.slot_idx);
        break;
    case JIT_OP_CALL_FUNC:
        fprintf(out, "%02X: CALL %p(<CPU CTXT>, <SLOT %02X>)\n", idx,
                immed->call_func.func, immed->call_func.slot_no);
        break;
    case JIT_OP_READ_16_CONSTADDR:
        fprintf(out, "%02X: READ_16_CONSTADDR *(U16*)%08X, *<SLOT %02X>\n",
                idx, (unsigned)immed->read_16_constaddr.addr,
                immed->read_16_constaddr.slot_no);
        break;
    case JIT_OP_SIGN_EXTEND_16:
        fprintf(out, "%02X: SIGN_EXTEND_16 <SLOT %02X>\n", idx,
                immed->sign_extend_16.slot_no);
        break;
    case JIT_OP_READ_32_CONSTADDR:
        fprintf(out, "%02X: READ_32_CONSTADDR *(U32*)%08X, *<SLOT %02X>\n",
                idx, (unsigned)immed->read_32_constaddr.addr,
                immed->read_32_constaddr.slot_no);
        break;
    case JIT_OP_READ_16_SLOT:
        fprintf(out, "%02X: READ_16_SLOT *(U16*)<SLOT %02X>, <SLOT %02X>\n",
                idx, immed->read_16_slot.addr_slot,
                immed->read_16_slot.dst_slot);
        break;
    case JIT_OP_READ_32_SLOT:
        fprintf(out, "%02X: READ_32_SLOT *(U32*)<SLOT %02X>, <SLOT %02X>\n",
                idx, immed->read_32_slot.addr_slot,
                immed->read_32_slot.dst_slot);
        break;
    case JIT_OP_WRITE_32_SLOT:
        fprintf(out, "%02X: WRITE_32_SLOT <SLOT %02X>, *(U32*)<SLOT %02X>\n",
                idx, immed->write_32_slot.src_slot,
                immed->write_32_slot.addr_slot);
        break;
    case JIT_OP_LOAD_SLOT16:
        fprintf(out, "%02X: LOAD_SLOT16 *(U16*)%p <SLOT %02X>\n",
                idx, immed->load_slot16.src, immed->load_slot16.slot_no);
        break;
    case JIT_OP_LOAD_SLOT:
        fprintf(out, "%02X: LOAD_SLOT *(U32*)%p, <SLOT %02X>\n",
                idx, immed->load_slot.src, immed->load_slot.slot_no);
        break;
    case JIT_OP_STORE_SLOT:
        fprintf(out, "%02X: STORE_SLOT <SLOT %02X>, *(U32*)%p\n", idx,
                immed->store_slot.slot_no, immed->store_slot.dst);
        break;
    case JIT_OP_ADD:
        fprintf(out, "%02X: ADD <SLOT %02X>, <SLOT %02X>\n",
                idx, immed->add.slot_src, immed->add.slot_dst);
        break;
    case JIT_OP_SUB:
        fprintf(out, "%02X: SUB <SLOT %02X>, <SLOT %02X>\n",
                idx, immed->sub.slot_src, immed->sub.slot_dst);
        break;
    case JIT_OP_ADD_CONST32:
        fprintf(out, "%02X: ADD_CONST32 %08X, <SLOT %02X>\n", idx,
                (unsigned)immed->add_const32.const32,
                immed->add_const32.slot_dst);
        break;
    case JIT_OP_XOR:
        fprintf(out, "%02X: XOR <SLOT %02X>, <SLOT %02X>\n", idx,
                immed->xor.slot_src, immed->xor.slot_dst);
        break;
    case JIT_OP_XOR_CONST32:
        fprintf(out, "%02X: XOR_CONST32 %08X, <SLOT %02X>\n", idx,
                (unsigned)immed->xor_const32.const32,
                immed->xor_const32.slot_no);
        break;
    case JIT_OP_MOV:
        fprintf(out, "%02X: MOV <SLOT %02X>, <SLOT %02X>\n", idx,
                immed->mov.slot_src, immed->mov.slot_dst);
        break;
    case JIT_OP_AND:
        fprintf(out, "%02X: AND <SLOT %02X>, <SLOT %02X>\n", idx,
                immed->and.slot_src, immed->and.slot_dst);
        break;
    case JIT_OP_AND_CONST32:
        fprintf(out, "%02X: AND %08X, <SLOT %02X>\n", idx,
                (unsigned)immed->and_const32.const32,
                immed->and_const32.slot_no);
        break;
    case JIT_OP_OR:
        fprintf(out, "%02X: OR <SLOT %02X>, <SLOT %02X>\n", idx,
                immed->or.slot_src, immed->or.slot_dst);
        break;
    case JIT_OP_OR_CONST32:
        fprintf(out, "%02X: OR_CONST32 %08X, <SLOT %02X>\n", idx,
                (unsigned)immed->or_const32.const32,
                immed->or_const32.slot_no);
        break;
    case JIT_OP_SLOT_TO_BOOL:
        fprintf(out, "%02X: SLOT_TO_BOOL <SLOT %02X>\n",
                idx, immed->slot_to_bool.slot_no);
        break;
    case JIT_OP_NOT:
        fprintf(out, "%02X: NOT <SLOT %02X>\n", idx, immed->not.slot_no);
        break;
    case JIT_OP_SHLL:
        fprintf(out, "%02X: SHLL %08X, <SLOT %02X>\n", idx,
                immed->shll.shift_amt, immed->shll.slot_no);
        break;
    case JIT_OP_SHAR:
        fprintf(out, "%02X: SHAR %08X, <SLOT %02X>\n", idx,
                immed->shar.shift_amt, immed->shar.slot_no);
        break;
    case JIT_OP_SHLR:
        fprintf(out, "%02X: SHLR %08X, <SLOT %02X>\n", idx,
                immed->shlr.shift_amt, immed->shlr.slot_no);
        break;
    case JIT_OP_SHAD:
        fprintf(out, "%02X: SHAD <SLOT %02X>, <SLOT %02X>\n", idx,
                immed->shad.slot_shift_amt, immed->shad.slot_val);
        break;
    case JIT_OP_SET_GT_UNSIGNED:
        fprintf(out, "%02X: SET_GT_UNSIGNED <SLOT %02X>, <SLOT %02X>, "
                "<SLOT %02X>\n", idx,
                immed->set_gt_unsigned.slot_lhs,
                immed->set_gt_unsigned.slot_rhs,
                immed->set_gt_unsigned.slot_dst);
        break;
    case JIT_OP_SET_GT_SIGNED:
        fprintf(out, "%02X: SET_GT_SIGNED <SLOT %02X>, <SLOT %02X>, "
                "<SLOT %02X>\n", idx,
                immed->set_gt_signed.slot_lhs,
                immed->set_gt_signed.slot_rhs, immed->set_gt_signed.slot_dst);
        break;
    case JIT_OP_SET_GT_SIGNED_CONST:
        fprintf(out, "%02X: SET_GT_SIGNED_CONST <SLOT %02X>, %08X, "
                "<SLOT %02X>\n", idx,
                immed->set_gt_signed_const.slot_lhs,
                (unsigned)immed->set_gt_signed_const.imm_rhs,
                immed->set_gt_signed_const.slot_dst);
        break;
    case JIT_OP_SET_EQ:
        fprintf(out, "%02X: SET_EQ <SLOT %02X>, <SLOT %02X>, "
                "<SLOT %02X>\n", idx,
                immed->set_eq.slot_lhs, immed->set_eq.slot_rhs,
                immed->set_eq.slot_dst);
        break;
    case JIT_OP_SET_GE_UNSIGNED:
        fprintf(out, "%02X: SET_GE_UNSIGNED <SLOT %02X>, <SLOT %02X>, "
                "<SLOT %02X>\n", idx,
                immed->set_ge_unsigned.slot_lhs,
                immed->set_ge_unsigned.slot_rhs,
                immed->set_ge_unsigned.slot_dst);
        break;
    case JIT_OP_SET_GE_SIGNED:
        fprintf(out, "%02X: SET_GE_SIGNED_CONST <SLOT %02X>, %08X, "
                "<SLOT %02X>\n", idx,
                immed->set_ge_signed_const.slot_lhs,
                (unsigned)immed->set_ge_signed_const.imm_rhs,
                immed->set_ge_signed_const.slot_dst);
        break;
    case JIT_OP_SET_GE_SIGNED_CONST:
        fprintf(out, "%02X: SET_GE_SIGNED_CONST <SLOT %02X>, %08X, "
                "<SLOT %02X>\n", idx,
                immed->set_ge_signed_const.slot_lhs,
                (unsigned)immed->set_ge_signed_const.imm_rhs,
                immed->set_ge_signed_const.slot_dst);
        break;
    case JIT_OP_MUL_U32:
        fprintf(out, "%02X: MUL_U32 <SLOT %02X>, <SLOT %02X>, <SLOT %02X>\n",
                idx, immed->mul_u32.slot_lhs, immed->mul_u32.slot_rhs,
                immed->mul_u32.slot_dst);
        break;
    case JIT_OP_DISCARD_SLOT:
        fprintf(out, "%02X: DISCARD_SLOT <SLOT %02X>\n", idx,
                immed->discard_slot.slot_no);
        break;
    default:
        fprintf(out, "%02X: <unknown opcode %02X>\n", idx, (int)op);
    }
}
