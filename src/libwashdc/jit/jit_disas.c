/*******************************************************************************
 *
 * Copyright 2019, 2020 snickerbockers
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

#include "jit_disas.h"

void jit_disas_il(washdc_hostfile out, struct jit_inst const *inst, int idx) {
    enum jit_opcode op = inst->op;
    union jit_immed const *immed = &inst->immed;

    switch (op) {
    case JIT_OP_FALLBACK:
        washdc_hostfile_printf(out, "%02X: FALLBACK %p(0x%04x)\n",
                               idx, immed->fallback.fallback_fn,
                               (int)immed->fallback.inst);
        break;
    case JIT_OP_JUMP:
        washdc_hostfile_printf(out, "%02X: JUMP <SLOT %02X, SLOT %02X>\n",
                               idx, immed->jump.jmp_addr_slot,
                               immed->jump.jmp_hash_slot);
        break;
    case JIT_CSET:
        washdc_hostfile_printf(out,
                               "%02X: CSET %08X, <SLOT %02X> IF "
                               "(<SLOT %02X> & 1) == %u\n", idx,
                               (unsigned)immed->cset.src_val,
                               immed->cset.dst_slot, immed->cset.flag_slot,
                               immed->cset.t_flag);
        break;
    case JIT_SET_SLOT:
        washdc_hostfile_printf(out, "%02X: SET %08X, <SLOT %02X>\n", idx,
                               (unsigned)immed->set_slot.new_val,
                               immed->set_slot.slot_idx);
        break;
    case JIT_SET_SLOT_HOST_PTR:
        washdc_hostfile_printf(out, "%02X: SET_HOST_PTR %p, <SLOT %02X>\n", idx,
                               immed->set_slot_host_ptr.ptr,
                               immed->set_slot_host_ptr.slot_idx);
        break;
    case JIT_OP_CALL_FUNC:
        washdc_hostfile_printf(out, "%02X: CALL %p(<CPU CTXT>, <SLOT %02X>)\n",
                               idx, immed->call_func.func,
                               immed->call_func.slot_no);
        break;
    case JIT_OP_CALL_FUNC_IMM32:
        washdc_hostfile_printf(out, "%02X: CALL %p(<CPU CTXT>, %08X)\n",
                               idx, immed->call_func_imm32.func,
                               (unsigned)immed->call_func_imm32.imm32);
        break;
    case JIT_OP_READ_16_CONSTADDR:
        washdc_hostfile_printf(out,
                               "%02X: READ_16_CONSTADDR *(U16*)%08X, "
                               "*<SLOT %02X>\n", idx,
                               (unsigned)immed->read_16_constaddr.addr,
                               immed->read_16_constaddr.slot_no);
        break;
    case JIT_OP_SIGN_EXTEND_8:
        washdc_hostfile_printf(out, "%02X: SIGN_EXTEND_8 <SLOT %02X>\n", idx,
                               immed->sign_extend_8.slot_no);
        break;
    case JIT_OP_SIGN_EXTEND_16:
        washdc_hostfile_printf(out, "%02X: SIGN_EXTEND_16 <SLOT %02X>\n", idx,
                               immed->sign_extend_16.slot_no);
        break;
    case JIT_OP_READ_32_CONSTADDR:
        washdc_hostfile_printf(out,
                               "%02X: READ_32_CONSTADDR *(U32*)%08X, "
                               "*<SLOT %02X>\n", idx,
                               (unsigned)immed->read_32_constaddr.addr,
                               immed->read_32_constaddr.slot_no);
        break;
    case JIT_OP_READ_8_SLOT:
        washdc_hostfile_printf(out, "%02X: READ_8_SLOT *(U8*)<SLOT %02X>, "
                               "<SLOT %02X>\n", idx,
                               immed->read_8_slot.addr_slot,
                               immed->read_8_slot.dst_slot);
        break;
    case JIT_OP_READ_16_SLOT:
        washdc_hostfile_printf(out, "%02X: READ_16_SLOT *(U16*)<SLOT %02X>, "
                               "<SLOT %02X>\n", idx,
                               immed->read_16_slot.addr_slot,
                               immed->read_16_slot.dst_slot);
        break;
    case JIT_OP_READ_32_SLOT:
        washdc_hostfile_printf(out, "%02X: READ_32_SLOT *(U32*)<SLOT %02X>, "
                               "<SLOT %02X>\n", idx,
                               immed->read_32_slot.addr_slot,
                               immed->read_32_slot.dst_slot);
        break;
    case JIT_OP_READ_FLOAT_SLOT:
        washdc_hostfile_printf(out, "%02X: READ_FLOAT_SLOT *(FLOAT*)<SLOT %02X>, "
                               "<SLOT %02X>\n", idx,
                               immed->read_float_slot.addr_slot,
                               immed->read_float_slot.dst_slot);
        break;
    case JIT_OP_WRITE_8_SLOT:
        washdc_hostfile_printf(out, "%02X: WRITE_8_SLOT <SLOT %02X>, "
                               "*(U8*)<SLOT %02X>\n", idx,
                               immed->write_8_slot.src_slot,
                               immed->write_8_slot.addr_slot);
        break;
    case JIT_OP_WRITE_16_SLOT:
        washdc_hostfile_printf(out, "%02X: WRITE_16_SLOT <SLOT %02X>, "
                               "*(U16*)<SLOT %02X>\n", idx,
                               immed->write_16_slot.src_slot,
                               immed->write_16_slot.addr_slot);
        break;
    case JIT_OP_WRITE_32_SLOT:
        washdc_hostfile_printf(out, "%02X: WRITE_32_SLOT <SLOT %02X>, "
                               "*(U32*)<SLOT %02X>\n",
                               idx, immed->write_32_slot.src_slot,
                               immed->write_32_slot.addr_slot);
        break;
    case JIT_OP_WRITE_FLOAT_SLOT:
        washdc_hostfile_printf(out, "%02X: WRITE_FLOAT_SLOT <SLOT %02X>, "
                               "*(U32*)<SLOT %02X>\n",
                               idx, immed->write_float_slot.src_slot,
                               immed->write_float_slot.addr_slot);
        break;
    case JIT_OP_LOAD_SLOT16:
        washdc_hostfile_printf(out, "%02X: LOAD_SLOT16 *(U16*)%p <SLOT %02X>\n",
                               idx, immed->load_slot16.src,
                               immed->load_slot16.slot_no);
        break;
    case JIT_OP_LOAD_SLOT:
        washdc_hostfile_printf(out, "%02X: LOAD_SLOT *(U32*)%p, <SLOT %02X>\n",
                               idx, immed->load_slot.src,
                               immed->load_slot.slot_no);
        break;
    case JIT_OP_LOAD_SLOT_OFFSET:
        washdc_hostfile_printf(out, "%02X: LOAD_SLOT_OFFSET *(U32*)(<SLOT %02X> + %u * 4), <SLOT %02X>\n",
                               idx, immed->load_slot_offset.slot_base,
                               immed->load_slot_offset.index,
                               immed->load_slot_offset.slot_dst);
        break;
    case JIT_OP_LOAD_FLOAT_SLOT:
        washdc_hostfile_printf(out, "%02X: LOAD_FLOAT_SLOT *(FLOAT*)%p, <SLOT %02X>\n",
                               idx, immed->load_float_slot.src,
                               immed->load_float_slot.slot_no);
        break;
    case JIT_OP_LOAD_FLOAT_SLOT_OFFSET:
        washdc_hostfile_printf(out, "%02X: LOAD_FLOAT_SLOT_OFFSET *(FLOAT*)(<SLOT %02X> + %u * 4), <SLOT %02X>\n",
                               idx, immed->load_float_slot_offset.slot_base,
                               immed->load_float_slot_offset.index,
                               immed->load_float_slot_offset.slot_dst);
        break;
    case JIT_OP_STORE_SLOT:
        washdc_hostfile_printf(out, "%02X: STORE_SLOT <SLOT %02X>, *(U32*)%p\n",
                               idx, immed->store_slot.slot_no,
                               immed->store_slot.dst);
        break;
    case JIT_OP_STORE_SLOT_OFFSET:
        washdc_hostfile_printf(out, "%02X: STORE_SLOT_OFFSET <SLOT %02X>, (<SLOT %02X> + %u * 4)\n",
                               idx, immed->store_slot_offset.slot_src,
                               immed->store_slot_offset.slot_base,
                               immed->store_slot_offset.index);
        break;
    case JIT_OP_STORE_FLOAT_SLOT:
        washdc_hostfile_printf(out, "%02X: STORE_FLOAT_SLOT <SLOT %02X>, *(FLOAT*)%p\n",
                               idx, immed->store_float_slot.slot_no,
                               immed->store_float_slot.dst);
        break;
    case JIT_OP_STORE_FLOAT_SLOT_OFFSET:
        washdc_hostfile_printf(out, "%02X: STORE_FLOAT_SLOT_OFFSET <SLOT %02X>, *(FLOAT*)(<SLOT %02X> + %u * 4)\n",
                               idx, immed->store_float_slot_offset.slot_src,
                               immed->store_float_slot_offset.slot_base,
                               immed->store_float_slot_offset.index);
        break;
    case JIT_OP_ADD:
        washdc_hostfile_printf(out, "%02X: ADD <SLOT %02X>, <SLOT %02X>\n",
                               idx, immed->add.slot_src, immed->add.slot_dst);
        break;
    case JIT_OP_SUB:
        washdc_hostfile_printf(out, "%02X: SUB <SLOT %02X>, <SLOT %02X>\n",
                               idx, immed->sub.slot_src, immed->sub.slot_dst);
        break;
    case JIT_OP_SUB_FLOAT:
        washdc_hostfile_printf(out, "%02X: SUB_FLOAT <SLOT %02X>, <SLOT %02X>\n",
                               idx, immed->sub_float.slot_src, immed->sub_float.slot_dst);
        break;
    case JIT_OP_ADD_FLOAT:
        washdc_hostfile_printf(out, "%02X: ADD_FLOAT <SLOT %02X>, <SLOT %02X>\n",
                               idx, immed->add_float.slot_src, immed->add_float.slot_dst);
        break;
    case JIT_OP_ADD_CONST32:
        washdc_hostfile_printf(out, "%02X: ADD_CONST32 %08X, <SLOT %02X>\n",
                               idx,
                               (unsigned)immed->add_const32.const32,
                               immed->add_const32.slot_dst);
        break;
    case JIT_OP_XOR:
        washdc_hostfile_printf(out, "%02X: XOR <SLOT %02X>, <SLOT %02X>\n", idx,
                               immed->xor.slot_src, immed->xor.slot_dst);
        break;
    case JIT_OP_XOR_CONST32:
        washdc_hostfile_printf(out, "%02X: XOR_CONST32 %08X, <SLOT %02X>\n",
                               idx, (unsigned)immed->xor_const32.const32,
                               immed->xor_const32.slot_no);
        break;
    case JIT_OP_MOV:
        washdc_hostfile_printf(out, "%02X: MOV <SLOT %02X>, <SLOT %02X>\n", idx,
                               immed->mov.slot_src, immed->mov.slot_dst);
        break;
    case JIT_OP_MOV_FLOAT:
        washdc_hostfile_printf(out, "%02X: MOV_FLOAT <SLOT %02X>, <SLOT %02X>\n", idx,
                               immed->mov_float.slot_src, immed->mov_float.slot_dst);
        break;
    case JIT_OP_AND:
        washdc_hostfile_printf(out, "%02X: AND <SLOT %02X>, <SLOT %02X>\n", idx,
                               immed->and.slot_src, immed->and.slot_dst);
        break;
    case JIT_OP_AND_CONST32:
        washdc_hostfile_printf(out, "%02X: AND %08X, <SLOT %02X>\n", idx,
                               (unsigned)immed->and_const32.const32,
                               immed->and_const32.slot_no);
        break;
    case JIT_OP_OR:
        washdc_hostfile_printf(out, "%02X: OR <SLOT %02X>, <SLOT %02X>\n", idx,
                               immed->or.slot_src, immed->or.slot_dst);
        break;
    case JIT_OP_OR_CONST32:
        washdc_hostfile_printf(out, "%02X: OR_CONST32 %08X, <SLOT %02X>\n", idx,
                               (unsigned)immed->or_const32.const32,
                               immed->or_const32.slot_no);
        break;
    case JIT_OP_SLOT_TO_BOOL:
        washdc_hostfile_printf(out, "%02X: SLOT_TO_BOOL <SLOT %02X>\n",
                               idx, immed->slot_to_bool.slot_no);
        break;
    case JIT_OP_NOT:
        washdc_hostfile_printf(out, "%02X: NOT <SLOT %02X>\n",
                               idx, immed->not.slot_no);
        break;
    case JIT_OP_SHLL:
        washdc_hostfile_printf(out, "%02X: SHLL %08X, <SLOT %02X>\n", idx,
                               immed->shll.shift_amt, immed->shll.slot_no);
        break;
    case JIT_OP_SHAR:
        washdc_hostfile_printf(out, "%02X: SHAR %08X, <SLOT %02X>\n", idx,
                               immed->shar.shift_amt, immed->shar.slot_no);
        break;
    case JIT_OP_SHLR:
        washdc_hostfile_printf(out, "%02X: SHLR %08X, <SLOT %02X>\n", idx,
                               immed->shlr.shift_amt, immed->shlr.slot_no);
        break;
    case JIT_OP_SHAD:
        washdc_hostfile_printf(out, "%02X: SHAD <SLOT %02X>, <SLOT %02X>\n",
                               idx, immed->shad.slot_shift_amt,
                               immed->shad.slot_val);
        break;
    case JIT_OP_SET_GT_UNSIGNED:
        washdc_hostfile_printf(out, "%02X: SET_GT_UNSIGNED <SLOT %02X>, "
                               "<SLOT %02X>, <SLOT %02X>\n", idx,
                               immed->set_gt_unsigned.slot_lhs,
                               immed->set_gt_unsigned.slot_rhs,
                               immed->set_gt_unsigned.slot_dst);
        break;
    case JIT_OP_SET_GT_SIGNED:
        washdc_hostfile_printf(out, "%02X: SET_GT_SIGNED <SLOT %02X>, "
                               "<SLOT %02X>, <SLOT %02X>\n", idx,
                               immed->set_gt_signed.slot_lhs,
                               immed->set_gt_signed.slot_rhs,
                               immed->set_gt_signed.slot_dst);
        break;
    case JIT_OP_SET_GT_SIGNED_CONST:
        washdc_hostfile_printf(out, "%02X: SET_GT_SIGNED_CONST <SLOT %02X>, "
                               "%08X, <SLOT %02X>\n", idx,
                               immed->set_gt_signed_const.slot_lhs,
                               (unsigned)immed->set_gt_signed_const.imm_rhs,
                               immed->set_gt_signed_const.slot_dst);
        break;
    case JIT_OP_SET_EQ:
        washdc_hostfile_printf(out, "%02X: SET_EQ <SLOT %02X>, <SLOT %02X>, "
                               "<SLOT %02X>\n", idx,
                               immed->set_eq.slot_lhs, immed->set_eq.slot_rhs,
                               immed->set_eq.slot_dst);
        break;
    case JIT_OP_SET_GE_UNSIGNED:
        washdc_hostfile_printf(out, "%02X: SET_GE_UNSIGNED <SLOT %02X>, "
                               "<SLOT %02X>, <SLOT %02X>\n", idx,
                               immed->set_ge_unsigned.slot_lhs,
                               immed->set_ge_unsigned.slot_rhs,
                               immed->set_ge_unsigned.slot_dst);
        break;
    case JIT_OP_SET_GE_SIGNED:
        washdc_hostfile_printf(out, "%02X: SET_GE_SIGNED_CONST <SLOT %02X>, "
                               "%08X, <SLOT %02X>\n", idx,
                               immed->set_ge_signed_const.slot_lhs,
                               (unsigned)immed->set_ge_signed_const.imm_rhs,
                               immed->set_ge_signed_const.slot_dst);
        break;
    case JIT_OP_SET_GE_SIGNED_CONST:
        washdc_hostfile_printf(out, "%02X: SET_GE_SIGNED_CONST <SLOT %02X>, "
                               "%08X, <SLOT %02X>\n", idx,
                               immed->set_ge_signed_const.slot_lhs,
                               (unsigned)immed->set_ge_signed_const.imm_rhs,
                               immed->set_ge_signed_const.slot_dst);
        break;
    case JIT_OP_MUL_U32:
        washdc_hostfile_printf(out, "%02X: MUL_U32 <SLOT %02X>, <SLOT %02X>, "
                               "<SLOT %02X>\n",
                               idx, immed->mul_u32.slot_lhs,
                               immed->mul_u32.slot_rhs,
                               immed->mul_u32.slot_dst);
        break;
    case JIT_OP_MUL_FLOAT:
        washdc_hostfile_printf(out,
                               "%02X: MUL_FLOAT <SLOT %02X>, <SLOT %02X>\n",
                               idx, immed->mul_float.slot_lhs,
                               immed->mul_float.slot_dst);
        break;
    case JIT_OP_DISCARD_SLOT:
        washdc_hostfile_printf(out, "%02X: DISCARD_SLOT <SLOT %02X>\n", idx,
                               immed->discard_slot.slot_no);
        break;
    default:
        washdc_hostfile_printf(out, "%02X: <unknown opcode %02X>\n",
                               idx, (int)op);
    }
}
