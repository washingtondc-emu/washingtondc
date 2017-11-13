/*******************************************************************************
 *
 * Copyright (c) 2017, snickerbockers <chimerasaurusrex@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
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

#include "sh4_bin_emit.h"

void emit_bin_inst(emit_bin_handler_func emit, uint16_t inst) {
    emit(inst);
}

uint16_t assemble_bin_noarg(uint16_t opcode) {
    return opcode;
}

uint16_t assemble_bin_rn(uint16_t opcode, unsigned rn) {
    return opcode | (uint16_t)((rn & 15) << 8);
}

uint16_t assemble_bin_imm8(uint16_t opcode, unsigned imm8) {
    return opcode | (uint16_t)(imm8 & 0xff);
}

uint16_t assemble_bin_imm12(uint16_t opcode, unsigned imm12) {
    return opcode | (uint16_t)(imm12 & 0xfff);
}

uint16_t assemble_bin_rn_imm8(uint16_t opcode, unsigned rn, unsigned imm8) {
    return opcode | (uint16_t)((rn & 15) << 8) | (uint16_t)(imm8 & 0xff);
}

uint16_t assemble_bin_rm_rn(uint16_t opcode, unsigned rm, unsigned rn) {
    return opcode | (uint16_t)((rm & 15) << 4) | (uint16_t)((rn & 15) << 8);
}

uint16_t assemble_bin_rm_rn_bank(uint16_t opcode,
                                unsigned rm, unsigned rn_bank) {
    return opcode | (uint16_t)((rm & 15) << 8) | (uint16_t)((rn_bank & 7) << 4);
}

uint16_t assemble_bin_rn_imm4(uint16_t opcode, unsigned rn, unsigned imm4) {
    return opcode | (uint16_t)((rn & 15) << 4) | (uint16_t)(imm4 & 15);
}

uint16_t assemble_bin_rm_rn_imm4(uint16_t opcode, unsigned rm,
                                 unsigned rn, unsigned imm4) {
    return opcode | (uint16_t)(imm4 & 15) | (uint16_t)((rm & 15) << 4) |
        (uint16_t)((rn & 15) << 8);
}

uint16_t assemble_bin_drm_drn(uint16_t opcode, unsigned drm, unsigned drn) {
    drm = (drm >> 1) & 7;
    drn = (drn >> 1) & 7;
    return opcode | (uint16_t)(drm << 5) | (uint16_t)(drn << 9);
}

uint16_t assemble_bin_rm_drn(uint16_t opcode, unsigned rm, unsigned drn) {
    drn = (drn >> 1) & 7;
    return opcode | (uint16_t)((rm & 15) << 4) | (unsigned)(drn << 9);
}

uint16_t assemble_bin_drm_rn(uint16_t opcode, unsigned drm, unsigned rn) {
    drm = (drm >> 1) & 7;
    return opcode | (uint16_t)((rn & 15) << 8) | (uint16_t)(drm << 5);
}

uint16_t assemble_bin_drn(uint16_t opcode, unsigned drn) {
    drn = (drn >> 1) & 7;
    return opcode | (uint16_t)(drn << 9);
}

uint16_t assemble_bin_fvm_fvn(uint16_t opcode, unsigned fvm, unsigned fvn) {
    fvm = (fvm >> 2) & 3;
    fvn = (fvn >> 2) & 3;
    return opcode | (uint16_t)(fvm << 8) | (uint16_t)(fvn << 10);
}

uint16_t assemble_bin_fvn(uint16_t opcode, unsigned fvn) {
    fvn = (fvn >> 2) & 3;
    return opcode | (uint16_t)(fvn << 10);
}
