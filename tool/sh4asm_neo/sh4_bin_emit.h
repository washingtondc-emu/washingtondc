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

#ifndef SH4_BIN_EMIT_H_
#define SH4_BIN_EMIT_H_

#include <stdint.h>

#include "sh4_opcodes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*emit_bin_handler_func)(uint16_t);

uint16_t assemble_bin_noarg(uint16_t opcode);
uint16_t assemble_bin_rn(uint16_t opcode, unsigned rn);
uint16_t assemble_bin_imm8(uint16_t opcode, unsigned imm8);
uint16_t assemble_bin_imm12(uint16_t opcode, unsigned imm12);
uint16_t assemble_bin_rn_imm8(uint16_t opcode, unsigned rn, unsigned imm8);
uint16_t assemble_bin_rm_rn(uint16_t opcode, unsigned rm, unsigned rn);
uint16_t assemble_bin_rm_rn_bank(uint16_t opcode, unsigned rm,
                                 unsigned rn_bank);
uint16_t assemble_bin_rn_imm4(uint16_t opcode, unsigned rn, unsigned imm4);
uint16_t assemble_bin_rm_rn_imm4(uint16_t opcode, unsigned rm,
                                 unsigned imm4, unsigned rn);
uint16_t assemble_bin_drm_drn(uint16_t opcode, unsigned drm, unsigned drn);
uint16_t assemble_bin_rm_drn(uint16_t opcode, unsigned rm, unsigned drn);
uint16_t assemble_bin_drm_rn(uint16_t opcode, unsigned drm, unsigned rn);
uint16_t assemble_bin_drn(uint16_t opcode, unsigned drn);
uint16_t assemble_bin_fvm_fvn(uint16_t opcode, unsigned fvm, unsigned fvn);
uint16_t assemble_bin_fvn(uint16_t opcode, unsigned fvn);

void emit_bin_inst(emit_bin_handler_func emit, uint16_t inst);

// OP
// xxxxxxxxxxxxxxxx
#define DEF_BIN_NOARG(op, val)                                  \
    static inline void sh4_bin_##op(emit_bin_handler_func em) { \
        emit_bin_inst((em), assemble_bin_noarg(val));           \
    }

// OP Rn
// xxxxnnnnxxxxxxxx
#define DEF_BIN_RN(op, val)                                             \
    static inline void sh4_bin_##op##_rn(emit_bin_handler_func em,      \
                                         unsigned rn) {                 \
        emit_bin_inst((em), assemble_bin_rn(val, rn));                  \
    }

// OP @Rn
// xxxxnnnnxxxxxxxx
#define DEF_BIN_ARN(op, val)                                            \
    static inline void sh4_bin_##op##_arn(emit_bin_handler_func em,     \
                                          unsigned rn) {                \
        emit_bin_inst((em), assemble_bin_rn(val, rn));                  \
    }

// OP Rm, REG
// xxxxmmmmxxxxxxxx
#define DEF_BIN_RM_REG(op, val, reg)                                    \
    static inline void sh4_bin_##op##_rm_##reg(emit_bin_handler_func em, \
                                               unsigned rm) {           \
        emit_bin_inst((em), assemble_bin_rn(val, rm));                  \
    }

// OP REG, Rn
// xxxxnnnnxxxxxxxx
#define DEF_BIN_REG_RN(op, val, reg)                                    \
    static inline void sh4_bin_##op##_##reg##_rn(emit_bin_handler_func em, \
                                                 unsigned rn) {         \
        emit_bin_inst((em), assemble_bin_rn(val, rn));                  \
    }

// OP @Rm+, REG
// xxxxnnnnxxxxxxxx
#define DEF_BIN_ARMP_REG(op, val, reg)                                  \
    static inline void sh4_bin_##op##_armp_##reg(emit_bin_handler_func em, \
                                                 unsigned rm) {         \
        emit_bin_inst((em), assemble_bin_rn(val, rm));                  \
    }

// OP REG, @-Rn
// xxxxnnnnxxxxxxxx
#define DEF_BIN_REG_AMRN(op, val, reg)                                  \
    static inline void sh4_bin_##op##_##reg##_amrn(emit_bin_handler_func em, \
                                                   unsigned rn) {       \
        emit_bin_inst((em), assemble_bin_rn(val, rn));                  \
    }

// OP REG, @Rn
// xxxxnnnnxxxxxxxx
#define DEF_BIN_REG_ARN(op, val, reg)                                   \
    static inline void sh4_bin_##op##_##reg##_arn(emit_bin_handler_func em, \
                                                  unsigned rn) {        \
        emit_bin_inst((em), assemble_bin_rn(val, rn));                  \
    }

// OP FRn
#define DEF_BIN_FRN(op, val)                                            \
    static inline void sh4_bin_##op##_frn(emit_bin_handler_func em,     \
                                          unsigned frn) {               \
        emit_bin_inst((em), assemble_bin_rn(val, frn));                 \
    }

// OP FRm, REG
// xxxxmmmmxxxxxxxx
#define DEF_BIN_FRM_REG(op, val, reg)                                   \
    static inline void sh4_bin_##op##_frm_##reg(emit_bin_handler_func em, \
                                                unsigned frm) {         \
        emit_bin_inst((em), assemble_bin_rn(val, frm));                 \
    }

// OP REG, FRn
// xxxxnnnnxxxxxxxx
#define DEF_BIN_REG_FRN(op, val, reg)                                   \
    static inline void sh4_bin_##op##_##reg##_frn(emit_bin_handler_func em, \
                                                  unsigned frn) {       \
        emit_bin_inst((em), assemble_bin_rn(val, frn));                \
    }

// OP #imm8, REG
// xxxxxxxxiiiiiiii
#define DEF_BIN_IMM8_REG(op, val, reg)                                  \
    static inline void sh4_bin_##op##_imm8_##reg(emit_bin_handler_func em, \
                                                 unsigned imm8) {       \
        emit_bin_inst((em), assemble_bin_imm8(val, imm8));              \
    }

// OP #imm8, @(REG1, REG2)
// xxxxxxxxiiiiiiii
#define DEF_BIN_IMM8_A_REG_REG(op, val, reg1, reg2)                     \
    static inline void                                                  \
    sh4_bin_##op##_imm8_a_##reg1##_##reg2(emit_bin_handler_func em,     \
                                          unsigned imm8) {              \
        emit_bin_inst((em), assemble_bin_imm8(val, imm8));              \
    }

// OP #imm8
// xxxxxxxxiiiiiiii
#define DEF_BIN_IMM8(op, val)                                           \
    static inline void sh4_bin_##op##_imm8(emit_bin_handler_func em,    \
                                           unsigned imm8) {             \
        emit_bin_inst((em), assemble_bin_imm8(val, imm8));              \
    }

/*
 * OP offset (relative to PC)
 * xxxxxxxxiiiiiiii
 *
 * the input to the generated function is the offset from the program-counter
 * register.  The target-address is this offset plus the PC.
 */
#define DEF_BIN_OFFS8(op, val, disp_shift, trans)                       \
    static inline void sh4_bin_##op##_offs8(emit_bin_handler_func em,   \
                                            unsigned offset) {          \
        unsigned disp8 = ((offset - (trans)) >> (disp_shift)) & 0xff;   \
        emit_bin_inst((em), assemble_bin_imm8(val, disp8));             \
    }

// OP REG, @(disp8, REG)
// xxxxxxxxiiiiiiii
#define DEF_BIN_REG_A_DISP8_REG(op, val, reg1, reg2, disp_shift)        \
    static inline void                                                  \
    sh4_bin_##op##_##reg1##_a_disp8_##reg2(emit_bin_handler_func em,    \
                                           unsigned disp8) {            \
        emit_bin_inst((em), assemble_bin_imm8(val, (disp8) >> disp_shift)); \
    }

// OP @(disp8, REG), REG
// xxxxxxxxiiiiiiii
#define DEF_BIN_A_DISP8_REG_REG(op, val, reg1, reg2, disp_shift)        \
    static inline void                                                  \
    sh4_bin_##op##_a_disp8_##reg1##_##reg2(emit_bin_handler_func em,    \
                                           unsigned disp8) {            \
        emit_bin_inst((em), assemble_bin_imm8(val, (disp8) >> disp_shift)); \
    }

/*
 * OP @(offset, REG), REG
 * xxxxxxxxiiiiiiii
 *
 * the input to the generated function is the offset from the program-counter
 * register.  The target-address is this offset plus the PC.
 */
#define DEF_BIN_A_OFFS8_REG_REG(op, val, reg1, reg2, disp_shift, trans) \
    static inline void                                                  \
    sh4_bin_##op##_a_offs8_##reg1##_##reg2(emit_bin_handler_func em,    \
                                           unsigned offset) {           \
        unsigned disp8 = ((offset - (trans)) >> (disp_shift)) & 0xff;   \
        emit_bin_inst((em), assemble_bin_imm8(val, disp8)); \
    }

// OP offs12
// xxxxiiiiiiiiiiii
#define DEF_BIN_OFFS12(op, val, disp_shift, trans)                      \
    static inline void                                                  \
    sh4_bin_##op##_offs12(emit_bin_handler_func em, unsigned offset) {  \
        unsigned disp12 = ((offset - (trans)) >> (disp_shift)) & 0xfff; \
        emit_bin_inst((em), assemble_bin_imm12(val, disp12));           \
    }

// OP #imm8, Rn
// xxxxnnnniiiiiiii
#define DEF_BIN_IMM8_RN(op, val)                                        \
    static inline void                                                  \
    sh4_bin_##op##_imm8_rn(emit_bin_handler_func em,                    \
                           unsigned imm8, unsigned rn) {                \
        emit_bin_inst((em), assemble_bin_rn_imm8((val), (rn), (imm8))); \
    }

// OP @(disp8, REG), Rn
// xxxxnnnniiiiiiii
#define DEF_BIN_A_OFFS8_REG_RN(op, val, reg, disp_shift, trans)         \
    static inline void                                                  \
    sh4_bin_##op##_a_offs8_##reg##_rn(emit_bin_handler_func em,         \
                                      unsigned offset, unsigned rn) {   \
        unsigned disp8 = ((offset - (trans)) >> (disp_shift)) & 0xff;   \
        emit_bin_inst((em), assemble_bin_rn_imm8((val), rn, disp8)); \
    }

// OP Rm, Rn
// xxxxnnnnmmmmxxxx
#define DEF_BIN_RM_RN(op, val)                  \
    static inline void sh4_bin_##op##_rm_rn(emit_bin_handler_func em,   \
                                            unsigned rm, unsigned rn) { \
        emit_bin_inst((em), assemble_bin_rm_rn((val), rm, rn));         \
    }

// OP Rm, @(REG, Rn)
// xxxxnnnnmmmmxxxx
#define DEF_BIN_RM_A_REG_RN(op, val, reg)                               \
    static inline void                                                  \
    sh4_bin_##op##_rm_a_##reg##_rn(emit_bin_handler_func em,            \
                                   unsigned rm, unsigned rn) {          \
        emit_bin_inst((em), assemble_bin_rm_rn((val), rm, rn));         \
    }

// OP @(REG, Rm), Rn
// xxxxnnnnmmmmxxxx
#define DEF_BIN_A_REG_RM_RN(op, val, reg)                               \
    static inline void                                                  \
    sh4_bin_##op##_a_##reg##_rm_rn(emit_bin_handler_func em,            \
                                   unsigned rm, unsigned rn) {          \
        emit_bin_inst((em), assemble_bin_rm_rn((val), rm, rn));         \
    }

// OP Rm, @Rn
// xxxxnnnnmmmmxxxx
#define DEF_BIN_RM_ARN(op, val)                                         \
    static inline void sh4_bin_##op##_rm_arn(emit_bin_handler_func em,  \
                                             unsigned rm, unsigned rn) { \
        emit_bin_inst((em), assemble_bin_rm_rn((val), rm, rn));         \
    }

// OP @Rm, Rn
// xxxxnnnnmmmmxxxx
#define DEF_BIN_ARM_RN(op, val)                                         \
    static inline void sh4_bin_##op##_arm_rn(emit_bin_handler_func em, \
                                             unsigned rm, unsigned rn) { \
        emit_bin_inst((em), assemble_bin_rm_rn((val), rm, rn));         \
    }

// OP Rm, @-Rn
// xxxxnnnnmmmmxxxx
#define DEF_BIN_RM_AMRN(op, val)                \
    static inline void sh4_bin_##op##_rm_amrn(emit_bin_handler_func em, \
                                              unsigned rm, unsigned rn) { \
        emit_bin_inst((em), assemble_bin_rm_rn((val), rm, rn));         \
    }

// OP @Rm+, Rn
// xxxxnnnnmmmmxxxx
#define DEF_BIN_ARMP_RN(op, val)                \
    static inline void sh4_bin_##op##_armp_rn(emit_bin_handler_func em, \
                                              unsigned rm, unsigned rn) { \
        emit_bin_inst((em), assemble_bin_rm_rn((val), rm, rn));         \
    }

// OP @Rm+, @Rn+
// xxxxnnnnmmmmxxxx
#define DEF_BIN_ARMP_ARNP(op, val)                                      \
    static inline void sh4_bin_##op##_armp_arnp(emit_bin_handler_func em, \
                                                unsigned rm, unsigned rn) { \
        emit_bin_inst((em), assemble_bin_rm_rn((val), rm, rn));         \
    }

// OP FRm, FRn
// xxxxnnnnmmmmxxxx
#define DEF_BIN_FRM_FRN(op, val)                                        \
    static inline void sh4_bin_##op##_frm_frn(emit_bin_handler_func em, \
                                              unsigned frm, unsigned frn) { \
        emit_bin_inst((em), assemble_bin_rm_rn((val), frm, frn));       \
    }

// OP @Rm, FRn
// xxxxnnnnmmmmxxxx
#define DEF_BIN_ARM_FRN(op, val)                                        \
    static inline void sh4_bin_##op##_arm_frn(emit_bin_handler_func em, \
                                              unsigned rm, unsigned frn) { \
        emit_bin_inst((em), assemble_bin_rm_rn((val), rm, frn));        \
    }

// OP @(REG, Rm), FRn
// xxxxnnnnmmmmxxxx
#define DEF_BIN_A_REG_RM_FRN(op, val, reg)      \
    static inline void                                                  \
    sh4_bin_##op##_a_##reg##_rm_frn(emit_bin_handler_func em,           \
                                    unsigned rm, unsigned frn) {        \
        emit_bin_inst((em), assemble_bin_rm_rn((val), rm, frn));        \
    }

// OP @Rm+, Frn
// xxxxnnnnmmmmxxxx
#define DEF_BIN_ARMP_FRN(op, val)                                       \
    static inline void sh4_bin_##op##_armp_frn(emit_bin_handler_func em, \
                                               unsigned rm, unsigned frn) { \
        emit_bin_inst((em), assemble_bin_rm_rn((val), rm, frn));        \
    }

// OP FRm, @Rn
// xxxxnnnnmmmmxxxx
#define DEF_BIN_FRM_ARN(op, val)                                        \
    static inline void sh4_bin_##op##_frm_arn(emit_bin_handler_func em, \
                                              unsigned frm, unsigned rn) { \
        emit_bin_inst((em), assemble_bin_rm_rn((val), frm, rn));        \
    }

// OP FRm, @-Rn
// xxxxnnnnmmmmxxxx
#define DEF_BIN_FRM_AMRN(op, val)                                       \
    static inline void sh4_bin_##op##_frm_amrn(emit_bin_handler_func em, \
                                               unsigned frm, unsigned rn) { \
        emit_bin_inst((em), assemble_bin_rm_rn((val), frm, rn));        \
    }

// OP FRm, @(REG, Rn)
// xxxxnnnnmmmmxxxx
#define DEF_BIN_FRM_A_REG_RN(op, val, reg)                              \
    static inline void                                                  \
    sh4_bin_##op##_frm_a_##reg##_rn(emit_bin_handler_func em,           \
                                    unsigned frm, unsigned rn) {        \
        emit_bin_inst((em), assemble_bin_rm_rn((val), frm, rn));        \
    }

// OP REG, FRM, FRN
// xxxxnnnnmmmmxxxx
#define DEF_BIN_REG_FRM_FRN(op, val, reg)                               \
    static inline void                                                  \
    sh4_bin_##op##_##reg##_frm_frn(emit_bin_handler_func em,            \
                                   unsigned frm, unsigned frn) {        \
        emit_bin_inst((em), assemble_bin_rm_rn((val), frm, frn));       \
    }

// OP Rm, Rn_BANK
#define DEF_BIN_RM_RN_BANK(op, val)                                     \
    static inline void                                                  \
    sh4_bin_##op##_rm_rn_bank(emit_bin_handler_func em,                 \
                              unsigned rm, unsigned rn_bank) {          \
        emit_bin_inst((em), assemble_bin_rm_rn_bank((val), rm, rn_bank)); \
    }

// OP Rm, Rn_BANK
#define DEF_BIN_RM_BANK_RN(op, val)                                     \
    static inline void                                                  \
    sh4_bin_##op##_rm_bank_rn(emit_bin_handler_func em,                 \
                              unsigned rm_bank, unsigned rn) {          \
        emit_bin_inst((em), assemble_bin_rm_rn_bank((val), rn, rm_bank)); \
    }

// OP @Rm+, Rn_BANK
#define DEF_BIN_ARMP_RN_BANK(op, val)                                   \
    static inline void                                                  \
    sh4_bin_##op##_armp_rn_bank(emit_bin_handler_func em,               \
                                unsigned rm, unsigned rn_bank) {        \
        emit_bin_inst((em), assemble_bin_rm_rn_bank((val), rm, rn_bank)); \
    }

// OP @Rm_BANK, @-Rn
#define DEF_BIN_RM_BANK_AMRN(op, val)                                   \
    static inline void                                                  \
    sh4_bin_##op##_rm_bank_amrn(emit_bin_handler_func em,               \
                                unsigned rm_bank, unsigned rn) {        \
        emit_bin_inst((em), assemble_bin_rm_rn_bank((val), rn, rm_bank)); \
    }

// OP REG, @(disp4, Rn)
// xxxxxxxxnnnndddd
#define DEF_BIN_REG_A_DISP4_RN(op, val, reg, disp_shift)                \
    static inline void                                                  \
    sh4_bin_##op##_##reg##_a_disp4_rn(emit_bin_handler_func em,         \
                                      unsigned disp4, unsigned rn) {    \
        emit_bin_inst((em), assemble_bin_rn_imm4((val), rn,             \
                                                 disp4 >> (disp_shift))); \
    }

// OP @(disp4, Rm), REG
#define DEF_BIN_A_DISP4_RM_REG(op, val, reg, disp_shift)                \
    static inline void                                                  \
    sh4_bin_##op##_a_disp4_rm_##reg(emit_bin_handler_func em,           \
                                    unsigned disp4, unsigned rm) {      \
        emit_bin_inst((em), assemble_bin_rn_imm4((val), rm,             \
                                                 disp4 >> (disp_shift))); \
    }

// OP Rm, @(disp4, Rn)
// xxxxnnnnmmmmdddd
#define DEF_BIN_RM_A_DISP4_RN(op, val, disp_shift)              \
    static inline void                                          \
    sh4_bin_##op##_rm_a_disp4_rn(emit_bin_handler_func em,      \
                                 unsigned rm, unsigned disp4, unsigned rn) { \
        emit_bin_inst((em), assemble_bin_rm_rn_imm4((val), rm, rn,      \
                                                    disp4 >> (disp_shift))); \
    }

// OP @(disp4, Rm), Rn
// xxxxnnnnmmmmdddd
#define DEF_BIN_A_DISP4_RM_RN(op, val, disp_shift)                      \
    static inline void                                                  \
    sh4_bin_##op##_a_disp4_rm_rn(emit_bin_handler_func em,              \
                                 unsigned disp4, unsigned rm, unsigned rn) { \
        emit_bin_inst((em), assemble_bin_rm_rn_imm4((val), rm, rn,      \
                                                    disp4 >> (disp_shift))); \
    }

// OP DRm, DRn
// xxxxnnnxmmmxxxxx
#define DEF_BIN_DRM_DRN(op, val)                                        \
    static inline void                                                  \
    sh4_bin_##op##_drm_drn(emit_bin_handler_func em,                    \
                           unsigned drm, unsigned drn) {                \
        emit_bin_inst((em), assemble_bin_drm_drn((val), drm, drn));     \
    }

// OP DRm, XDn
// xxxxnnnxmmmxxxxx
#define DEF_BIN_DRM_XDN(op, val)                                        \
    static inline void                                                  \
    sh4_bin_##op##_drm_xdn(emit_bin_handler_func em,                    \
                           unsigned drm, unsigned xdn) {                \
        emit_bin_inst((em), assemble_bin_drm_drn((val), drm, xdn));     \
    }

// OP XDm, DRn
// xxxxnnnxmmmxxxxx
#define DEF_BIN_XDM_DRN(op, val)                                        \
    static inline void                                                  \
    sh4_bin_##op##_xdm_drn(emit_bin_handler_func em,                    \
                           unsigned xdm, unsigned drn) {                \
        emit_bin_inst((em), assemble_bin_drm_drn((val), xdm, drn));     \
    }

// OP XDm, XDn
// xxxxnnnxmmmxxxxx
#define DEF_BIN_XDM_XDN(op, val)                                        \
    static inline void                                                  \
    sh4_bin_##op##_xdm_xdn(emit_bin_handler_func em,                    \
                           unsigned xdm, unsigned xdn) {                \
        emit_bin_inst((em), assemble_bin_drm_drn((val), xdm, xdn));     \
    }

// OP @Rm, DRn
#define DEF_BIN_ARM_DRN(op, val)                                        \
    static inline void                                                  \
    sh4_bin_##op##_arm_drn(emit_bin_handler_func em,                    \
                           unsigned rm, unsigned drn) {                 \
        emit_bin_inst((em), assemble_bin_rm_drn((val), rm, drn));       \
    }

// OP @(REG, Rm), DRn
#define DEF_BIN_A_REG_RM_DRN(op, val, reg)                              \
    static inline void                                                  \
    sh4_bin_##op##_a_##reg##_rm_drn(emit_bin_handler_func em,           \
                                    unsigned rm, unsigned drn) {        \
        emit_bin_inst((em), assemble_bin_rm_drn((val), rm, drn));       \
    }

// OP @Rm+, DRn
#define DEF_BIN_ARMP_DRN(op, val)                                       \
    static inline void                                                  \
    sh4_bin_##op##_armp_drn(emit_bin_handler_func em,                   \
                            unsigned rm, unsigned drn) {                \
        emit_bin_inst((em), assemble_bin_rm_drn((val), rm, drn));       \
    }

// OP @Rm, XDn
#define DEF_BIN_ARM_XDN(op, val)                                        \
    static inline void                                                  \
    sh4_bin_##op##_arm_xdn(emit_bin_handler_func em,                    \
                           unsigned rm, unsigned xdn) {                 \
        emit_bin_inst((em), assemble_bin_rm_drn((val), rm, xdn));       \
    }

// OP @Rm+, XDn
#define DEF_BIN_ARMP_XDN(op, val)                                       \
    static inline void                                                  \
    sh4_bin_##op##_armp_xdn(emit_bin_handler_func em,                   \
                            unsigned rm, unsigned xdn) {                \
        emit_bin_inst((em), assemble_bin_rm_drn((val), rm, xdn));       \
    }

// OP @(REG, Rm), XDn
#define DEF_BIN_A_REG_RM_XDN(op, val, reg)                              \
    static inline void                                                  \
    sh4_bin_##op##_a_##reg##_rm_xdn(emit_bin_handler_func em,           \
                                    unsigned rm, unsigned xdn) {        \
        emit_bin_inst((em), assemble_bin_rm_drn((val), rm, xdn));       \
    }

// OP DRm, @Rn
#define DEF_BIN_DRM_ARN(op, val)                                        \
    static inline void                                                  \
    sh4_bin_##op##_drm_arn(emit_bin_handler_func em,                    \
                           unsigned drm, unsigned rn) {                 \
        emit_bin_inst((em), assemble_bin_drm_rn((val), drm, rn));       \
    }

// OP DRm, @-Rn
#define DEF_BIN_DRM_AMRN(op, val)                                       \
    static inline void                                                  \
    sh4_bin_##op##_drm_amrn(emit_bin_handler_func em,                   \
                        unsigned drm, unsigned rn) {                    \
        emit_bin_inst((em), assemble_bin_drm_rn((val), drm, rn));       \
    }

// OP DRm, @(REG, Rn)
#define DEF_BIN_DRM_A_REG_RN(op, val, reg)                              \
    static inline void                                                  \
    sh4_bin_##op##_drm_a_##reg##_rn(emit_bin_handler_func em,           \
                                unsigned drm, unsigned rn) {            \
        emit_bin_inst((em), assemble_bin_drm_rn((val), drm, rn));       \
    }

// OP XDm, @Rn
#define DEF_BIN_XDM_ARN(op, val)                                        \
    static inline void                                                  \
    sh4_bin_##op##_xdm_arn(emit_bin_handler_func em,                    \
                       unsigned xdm, unsigned rn) {                     \
        emit_bin_inst((em), assemble_bin_drm_rn((val), xdm, rn));       \
    }

// OP XDm, @-Rn
#define DEF_BIN_XDM_AMRN(op, val)                                       \
    static inline void                                                  \
    sh4_bin_##op##_xdm_amrn(emit_bin_handler_func em,                   \
                        unsigned xdm, unsigned rn) {                    \
        emit_bin_inst((em), assemble_bin_drm_rn((val), xdm, rn));       \
    }

// OP XDm, @(REG, Rn)
#define DEF_BIN_XDM_A_REG_RN(op, val, reg)                              \
    static inline void                                                  \
    sh4_bin_##op##_xdm_a_##reg##_rn(emit_bin_handler_func em,           \
                                unsigned xdm, unsigned rn) {            \
        emit_bin_inst((em), assemble_bin_drm_rn((val), xdm, rn));       \
    }

// OP DRn
#define DEF_BIN_DRN(op, val)                                            \
    static inline void                                                  \
    sh4_bin_##op##_drn(emit_bin_handler_func em, unsigned drn) {        \
        emit_bin_inst((em), assemble_bin_drn((val), drn));              \
    }

// OP DRm, REG
#define DEF_BIN_DRM_REG(op, val, reg)                                   \
    static inline void                                                  \
    sh4_bin_##op##_drm_##reg(emit_bin_handler_func em, unsigned drm) {  \
        emit_bin_inst((em), assemble_bin_drn((val), drm));              \
    }

// OP REG, DRn
#define DEF_BIN_REG_DRN(op, val, reg)                                   \
    static inline void                                                  \
    sh4_bin_##op##_##reg##_drn(emit_bin_handler_func em, unsigned drn) { \
        emit_bin_inst((em), assemble_bin_drn((val), drn));              \
    }

// OP FVm, FVn
#define DEF_BIN_FVM_FVN(op, val)                                        \
    static inline void                                                  \
    sh4_bin_##op##_fvm_fvn(emit_bin_handler_func em,                    \
                           unsigned fvm, unsigned fvn) {                \
        emit_bin_inst((em), assemble_bin_fvm_fvn((val), fvm, fvn));     \
    }

// OP REG, FVn
// xxxxnnxxxxxxxxxx
#define DEF_BIN_REG_FVN(op, val, reg)                                   \
    static inline void                                                  \
    sh4_bin_##op##_##reg##_fvn(emit_bin_handler_func em, unsigned fvn) { \
        emit_bin_inst((em), assemble_bin_fvn((val), fvn));              \
    }

// opcodes which take no arguments (noarg)
DEF_BIN_NOARG(div0u, OPCODE_DIV0U)
DEF_BIN_NOARG(rts, OPCODE_RTS)
DEF_BIN_NOARG(clrmac, OPCODE_CLRMAC)
DEF_BIN_NOARG(clrs, OPCODE_CLRS)
DEF_BIN_NOARG(clrt, OPCODE_CLRT)
DEF_BIN_NOARG(ldtlb, OPCODE_LDTLB)
DEF_BIN_NOARG(nop, OPCODE_NOP)
DEF_BIN_NOARG(rte, OPCODE_RTE)
DEF_BIN_NOARG(sets, OPCODE_SETS)
DEF_BIN_NOARG(sett, OPCODE_SETT)
DEF_BIN_NOARG(sleep, OPCODE_SLEEP)
DEF_BIN_NOARG(frchg, OPCODE_FRCHG)
DEF_BIN_NOARG(fschg, OPCODE_FSCHG)

DEF_BIN_RN(movt, OPCODE_MOVT_RN)
DEF_BIN_RN(cmppz, OPCODE_CMPPZ_RN)
DEF_BIN_RN(cmppl, OPCODE_CMPPL_RN)
DEF_BIN_RN(dt, OPCODE_DT_RN)
DEF_BIN_RN(rotl, OPCODE_ROTL_RN)
DEF_BIN_RN(rotr, OPCODE_ROTR_RN)
DEF_BIN_RN(rotcl, OPCODE_ROTCL_RN)
DEF_BIN_RN(rotcr, OPCODE_ROTCR_RN)
DEF_BIN_RN(shal, OPCODE_SHAL_RN)
DEF_BIN_RN(shar, OPCODE_SHAR_RN)
DEF_BIN_RN(shll, OPCODE_SHLL_RN)
DEF_BIN_RN(shlr, OPCODE_SHLR_RN)
DEF_BIN_RN(shll2, OPCODE_SHLL2_RN)
DEF_BIN_RN(shlr2, OPCODE_SHLR2_RN)
DEF_BIN_RN(shll8, OPCODE_SHLL8_RN)
DEF_BIN_RN(shlr8, OPCODE_SHLR8_RN)
DEF_BIN_RN(shll16, OPCODE_SHLL16_RN)
DEF_BIN_RN(shlr16, OPCODE_SHLR16_RN)
DEF_BIN_RN(braf, OPCODE_BRAF_RN)
DEF_BIN_RN(bsrf, OPCODE_BSRF_RN)

DEF_BIN_ARN(tasb, OPCODE_TASB_ARN)
DEF_BIN_ARN(ocbi, OPCODE_OCBI_ARN)
DEF_BIN_ARN(ocbp, OPCODE_OCBP_ARN)
DEF_BIN_ARN(ocbwb, OPCODE_OCBWB_ARN)
DEF_BIN_ARN(pref, OPCODE_PREF_ARN)
DEF_BIN_ARN(jmp, OPCODE_JMP_ARN)
DEF_BIN_ARN(jsr, OPCODE_JSR_ARN)

DEF_BIN_RM_REG(ldc, OPCODE_LDC_RM_SR, sr)
DEF_BIN_RM_REG(ldc, OPCODE_LDC_RM_GBR, gbr)
DEF_BIN_RM_REG(ldc, OPCODE_LDC_RM_VBR, vbr)
DEF_BIN_RM_REG(ldc, OPCODE_LDC_RM_SSR, ssr)
DEF_BIN_RM_REG(ldc, OPCODE_LDC_RM_SPC, spc)
DEF_BIN_RM_REG(ldc, OPCODE_LDC_RM_DBR, dbr)
DEF_BIN_RM_REG(lds, OPCODE_LDS_RM_MACH, mach)
DEF_BIN_RM_REG(lds, OPCODE_LDS_RM_MACL, macl)
DEF_BIN_RM_REG(lds, OPCODE_LDS_RM_PR, pr)
DEF_BIN_RM_REG(lds, OPCODE_LDS_RM_FPSCR, fpscr)
DEF_BIN_RM_REG(lds, OPCODE_LDS_RM_FPUL, fpul)

DEF_BIN_REG_RN(stc, OPCODE_STC_SR_RN, sr)
DEF_BIN_REG_RN(stc, OPCODE_STC_GBR_RN, gbr)
DEF_BIN_REG_RN(stc, OPCODE_STC_VBR_RN, vbr)
DEF_BIN_REG_RN(stc, OPCODE_STC_SSR_RN, ssr)
DEF_BIN_REG_RN(stc, OPCODE_STC_SPC_RN, spc)
DEF_BIN_REG_RN(stc, OPCODE_STC_SGR_RN, sgr)
DEF_BIN_REG_RN(stc, OPCODE_STC_DBR_RN, dbr)
DEF_BIN_REG_RN(sts, OPCODE_STS_MACH_RN, mach)
DEF_BIN_REG_RN(sts, OPCODE_STS_MACL_RN, macl)
DEF_BIN_REG_RN(sts, OPCODE_STS_PR_RN, pr)
DEF_BIN_REG_RN(sts, OPCODE_STS_FPSCR_RN, fpscr)
DEF_BIN_REG_RN(sts, OPCODE_STS_FPUL_RN, fpul)

DEF_BIN_ARMP_REG(ldcl, OPCODE_LDCL_ARMP_SR, sr)
DEF_BIN_ARMP_REG(ldcl, OPCODE_LDCL_ARMP_GBR, gbr)
DEF_BIN_ARMP_REG(ldcl, OPCODE_LDCL_ARMP_VBR, vbr)
DEF_BIN_ARMP_REG(ldcl, OPCODE_LDCL_ARMP_SSR, ssr)
DEF_BIN_ARMP_REG(ldcl, OPCODE_LDCL_ARMP_SPC, spc)
DEF_BIN_ARMP_REG(ldcl, OPCODE_LDCL_ARMP_DBR, dbr)
DEF_BIN_ARMP_REG(ldsl, OPCODE_LDSL_ARMP_MACH, mach)
DEF_BIN_ARMP_REG(ldsl, OPCODE_LDSL_ARMP_MACL, macl)
DEF_BIN_ARMP_REG(ldsl, OPCODE_LDSL_ARMP_PR, pr)
DEF_BIN_ARMP_REG(ldsl, OPCODE_LDSL_ARMP_FPSCR, fpscr)
DEF_BIN_ARMP_REG(ldsl, OPCODE_LDSL_ARMP_FPUL, fpul)

DEF_BIN_REG_AMRN(stcl, OPCODE_STCL_SR_AMRN, sr)
DEF_BIN_REG_AMRN(stcl, OPCODE_STCL_GBR_AMRN, gbr)
DEF_BIN_REG_AMRN(stcl, OPCODE_STCL_VBR_AMRN, vbr)
DEF_BIN_REG_AMRN(stcl, OPCODE_STCL_SSR_AMRN, ssr)
DEF_BIN_REG_AMRN(stcl, OPCODE_STCL_SPC_AMRN, spc)
DEF_BIN_REG_AMRN(stcl, OPCODE_STCL_SGR_AMRN, sgr)
DEF_BIN_REG_AMRN(stcl, OPCODE_STCL_DBR_AMRN, dbr)
DEF_BIN_REG_AMRN(stsl, OPCODE_STSL_MACH_AMRN, mach)
DEF_BIN_REG_AMRN(stsl, OPCODE_STSL_MACL_AMRN, macl)
DEF_BIN_REG_AMRN(stsl, OPCODE_STSL_PR_AMRN, pr)
DEF_BIN_REG_AMRN(stsl, OPCODE_STSL_FPSCR_AMRN, fpscr)
DEF_BIN_REG_AMRN(stsl, OPCODE_STSL_FPUL_AMRN, fpul)

DEF_BIN_REG_ARN(movcal, OPCODE_MOVCAL_R0_ARN, r0)

DEF_BIN_FRN(fldi0, OPCODE_FLDI0_FRN)
DEF_BIN_FRN(fldi1, OPCODE_FLDI1_FRN)
DEF_BIN_FRN(fabs, OPCODE_FABS_FRN)
DEF_BIN_FRN(fneg, OPCODE_FNEG_FRN)
DEF_BIN_FRN(fsqrt, OPCODE_FSQRT_FRN)
DEF_BIN_FRN(fsrra, OPCODE_FSRRA_FRN)

DEF_BIN_FRM_REG(flds, OPCODE_FLDS_FRM_FPUL, fpul)
DEF_BIN_FRM_REG(ftrc, OPCODE_FTRC_FRM_FPUL, fpul)

DEF_BIN_REG_FRN(fsts, OPCODE_FSTS_FPUL_FRN, fpul)
DEF_BIN_REG_FRN(float, OPCODE_FLOAT_FPUL_FRN, fpul)

DEF_BIN_IMM8_REG(cmpeq, OPCODE_CMPEQ_IMM8_R0, r0)
DEF_BIN_IMM8_REG(and, OPCODE_AND_IMM8_R0, r0)
DEF_BIN_IMM8_REG(or, OPCODE_OR_IMM8_R0, r0)
DEF_BIN_IMM8_REG(tst, OPCODE_TST_IMM8_R0, r0)
DEF_BIN_IMM8_REG(xor, OPCODE_XOR_IMM8_R0, r0)

DEF_BIN_IMM8_A_REG_REG(andb, OPCODE_ANDB_IMM8_A_R0_GBR, r0, gbr)
DEF_BIN_IMM8_A_REG_REG(orb, OPCODE_ORB_IMM8_A_R0_GBR, r0, gbr)
DEF_BIN_IMM8_A_REG_REG(tstb, OPCODE_TSTB_IMM8_A_R0_GBR, r0, gbr)
DEF_BIN_IMM8_A_REG_REG(xorb, OPCODE_XORB_IMM8_A_R0_GBR, r0, gbr)

DEF_BIN_OFFS8(bf, OPCODE_BF_DISP8, 1, 4);
DEF_BIN_OFFS8(bfs, OPCODE_BFS_DISP8, 1, 4)
DEF_BIN_OFFS8(bt, OPCODE_BT_DISP8, 1, 4)
DEF_BIN_OFFS8(bts, OPCODE_BTS_DISP8, 1, 4)

DEF_BIN_IMM8(trapa, OPCODE_TRAPA_IMM8)

DEF_BIN_REG_A_DISP8_REG(movb, OPCODE_MOVB_R0_A_DISP8_GBR, r0, gbr, 0)
DEF_BIN_REG_A_DISP8_REG(movw, OPCODE_MOVW_R0_A_DISP8_GBR, r0, gbr, 1)
DEF_BIN_REG_A_DISP8_REG(movl, OPCODE_MOVL_R0_A_DISP8_GBR, r0, gbr, 2)

DEF_BIN_A_DISP8_REG_REG(movb, OPCODE_MOVB_A_DISP8_GBR_R0, gbr, r0, 0)
DEF_BIN_A_DISP8_REG_REG(movw, OPCODE_MOVW_A_DISP8_GBR_R0, gbr, r0, 1)
DEF_BIN_A_DISP8_REG_REG(movl, OPCODE_MOVL_A_DISP8_GBR_R0, gbr, r0, 2)

DEF_BIN_A_OFFS8_REG_REG(mova, OPCODE_MOVA_A_DISP8_PC_R0, pc, r0, 2, 4)

DEF_BIN_OFFS12(bra, OPCODE_BRA_DISP12, 1, 4)
DEF_BIN_OFFS12(bsr, OPCODE_BSR_DISP12, 1, 4)

DEF_BIN_IMM8_RN(mov, OPCODE_MOV_IMM8_RN)
DEF_BIN_IMM8_RN(add, OPCODE_ADD_IMM8_RN)

DEF_BIN_A_OFFS8_REG_RN(movw, OPCODE_MOVW_A_DISP8_PC_RN, pc, 1, 4)
DEF_BIN_A_OFFS8_REG_RN(movl, OPCODE_MOVL_A_DISP8_PC_RN, pc, 2, 4)

DEF_BIN_RM_RN(mov, OPCODE_MOV_RM_RN)
DEF_BIN_RM_RN(swapb, OPCODE_SWAPB_RM_RN)
DEF_BIN_RM_RN(swapw, OPCODE_SWAPW_RM_RN)
DEF_BIN_RM_RN(xtrct, OPCODE_XTRCT_RM_RN)
DEF_BIN_RM_RN(add, OPCODE_ADD_RM_RN)
DEF_BIN_RM_RN(addc, OPCODE_ADDC_RM_RN)
DEF_BIN_RM_RN(addv, OPCODE_ADDV_RM_RN)
DEF_BIN_RM_RN(cmpeq, OPCODE_CMPEQ_RM_RN)
DEF_BIN_RM_RN(cmphs, OPCODE_CMPHS_RM_RN)
DEF_BIN_RM_RN(cmpge, OPCODE_CMPGE_RM_RN)
DEF_BIN_RM_RN(cmphi, OPCODE_CMPHI_RM_RN)
DEF_BIN_RM_RN(cmpgt, OPCODE_CMPGT_RM_RN)
DEF_BIN_RM_RN(cmpstr, OPCODE_CMPSTR_RM_RN)
DEF_BIN_RM_RN(div1, OPCODE_DIV1_RM_RN)
DEF_BIN_RM_RN(div0s, OPCODE_DIV0S_RM_RN)
DEF_BIN_RM_RN(dmulsl, OPCODE_DMULSL_RM_RN)
DEF_BIN_RM_RN(dmulul, OPCODE_DMULUL_RM_RN)
DEF_BIN_RM_RN(extsb, OPCODE_EXTSB_RM_RN)
DEF_BIN_RM_RN(extsw, OPCODE_EXTSW_RM_RN)
DEF_BIN_RM_RN(extub, OPCODE_EXTUB_RM_RN)
DEF_BIN_RM_RN(extuw, OPCODE_EXTUW_RM_RN)
DEF_BIN_RM_RN(mull, OPCODE_MULL_RM_RN)
DEF_BIN_RM_RN(mulsw, OPCODE_MULSW_RM_RN)
DEF_BIN_RM_RN(muluw, OPCODE_MULUW_RM_RN)
DEF_BIN_RM_RN(neg, OPCODE_NEG_RM_RN)
DEF_BIN_RM_RN(negc, OPCODE_NEGC_RM_RN)
DEF_BIN_RM_RN(sub, OPCODE_SUB_RM_RN)
DEF_BIN_RM_RN(subc, OPCODE_SUBC_RM_RN)
DEF_BIN_RM_RN(subv, OPCODE_SUBV_RM_RN)
DEF_BIN_RM_RN(and, OPCODE_AND_RM_RN)
DEF_BIN_RM_RN(not, OPCODE_NOT_RM_RN)
DEF_BIN_RM_RN(or, OPCODE_OR_RM_RN)
DEF_BIN_RM_RN(tst, OPCODE_TST_RM_RN)
DEF_BIN_RM_RN(xor, OPCODE_XOR_RM_RN)
DEF_BIN_RM_RN(shad, OPCODE_SHAD_RM_RN)
DEF_BIN_RM_RN(shld, OPCODE_SHLD_RM_RN)

DEF_BIN_RM_A_REG_RN(movb, OPCODE_MOVB_RM_A_R0_RN, r0)
DEF_BIN_RM_A_REG_RN(movw, OPCODE_MOVW_RM_A_R0_RN, r0)
DEF_BIN_RM_A_REG_RN(movl, OPCODE_MOVL_RM_A_R0_RN, r0)

DEF_BIN_A_REG_RM_RN(movb, OPCODE_MOVB_A_R0_RM_RN, r0)
DEF_BIN_A_REG_RM_RN(movw, OPCODE_MOVW_A_R0_RM_RN, r0)
DEF_BIN_A_REG_RM_RN(movl, OPCODE_MOVL_A_R0_RM_RN, r0)

DEF_BIN_RM_ARN(movb, OPCODE_MOVB_RM_ARN)
DEF_BIN_RM_ARN(movw, OPCODE_MOVW_RM_ARN)
DEF_BIN_RM_ARN(movl, OPCODE_MOVL_RM_ARN)

DEF_BIN_ARM_RN(movb, OPCODE_MOVB_ARM_RN)
DEF_BIN_ARM_RN(movw, OPCODE_MOVW_ARM_RN)
DEF_BIN_ARM_RN(movl, OPCODE_MOVL_ARM_RN)

DEF_BIN_RM_AMRN(movb, OPCODE_MOVB_RM_AMRN)
DEF_BIN_RM_AMRN(movw, OPCODE_MOVW_RM_AMRN)
DEF_BIN_RM_AMRN(movl, OPCODE_MOVL_RM_AMRN)

DEF_BIN_ARMP_RN(movb, OPCODE_MOVB_ARMP_RN)
DEF_BIN_ARMP_RN(movw, OPCODE_MOVW_ARMP_RN)
DEF_BIN_ARMP_RN(movl, OPCODE_MOVL_ARMP_RN)

DEF_BIN_ARMP_ARNP(macl, OPCODE_MACL_ARMP_ARNP)
DEF_BIN_ARMP_ARNP(macw, OPCODE_MACW_ARMP_ARNP)

DEF_BIN_FRM_FRN(fmov, OPCODE_FMOV_FRM_FRN)
DEF_BIN_FRM_FRN(fadd, OPCODE_FADD_FRM_FRN)
DEF_BIN_FRM_FRN(fcmpeq, OPCODE_FCMPEQ_FRM_FRN)
DEF_BIN_FRM_FRN(fcmpgt, OPCODE_FCMPGT_FRM_FRN)
DEF_BIN_FRM_FRN(fdiv, OPCODE_FDIV_FRM_FRN)
DEF_BIN_FRM_FRN(fmul, OPCODE_FMUL_FRM_FRN)
DEF_BIN_FRM_FRN(fsub, OPCODE_FSUB_FRM_FRN)

DEF_BIN_ARM_FRN(fmovs, OPCODE_FMOVS_ARM_FRN)

DEF_BIN_A_REG_RM_FRN(fmovs, OPCODE_FMOVS_A_R0_RM_FRN, r0)

DEF_BIN_ARMP_FRN(fmovs, OPCODE_FMOVS_ARMP_FRN)

DEF_BIN_FRM_ARN(fmovs, OPCODE_FMOVS_FRM_ARN)

DEF_BIN_FRM_AMRN(fmovs, OPCODE_FMOVS_FRM_AMRN)

DEF_BIN_FRM_A_REG_RN(fmovs, OPCODE_FMOVS_FRM_A_R0_RN, r0)

DEF_BIN_REG_FRM_FRN(fmac, OPCODE_FMAC_FR0_FRM_FRN, fr0)

DEF_BIN_RM_RN_BANK(ldc, OPCODE_LDC_RM_RN_BANK)

DEF_BIN_RM_BANK_RN(stc, OPCODE_STC_RM_BANK_RN)

DEF_BIN_ARMP_RN_BANK(ldcl, OPCODE_LDCL_ARMP_RN_BANK)

DEF_BIN_RM_BANK_AMRN(stcl, OPCODE_STCL_RM_BANK_AMRN)

DEF_BIN_REG_A_DISP4_RN(movb, OPCODE_MOVB_R0_A_DISP4_RN, r0, 0)
DEF_BIN_REG_A_DISP4_RN(movw, OPCODE_MOVW_R0_A_DISP4_RN, r0, 1)

DEF_BIN_A_DISP4_RM_REG(movb, OPCODE_MOVB_A_DISP4_RM_R0, r0, 0)
DEF_BIN_A_DISP4_RM_REG(movw, OPCODE_MOVW_A_DISP4_RM_R0, r0, 1)

DEF_BIN_RM_A_DISP4_RN(movl, OPCODE_MOVL_RM_A_DISP4_RN, 2)

DEF_BIN_A_DISP4_RM_RN(movl, OPCODE_MOVL_A_DISP4_RM_RN, 2)

DEF_BIN_DRM_DRN(fmov, OPCODE_FMOV_DRM_DRN)
DEF_BIN_DRM_DRN(fadd, OPCODE_FADD_DRM_DRN)
DEF_BIN_DRM_DRN(fcmpeq, OPCODE_FCMPEQ_DRM_DRN)
DEF_BIN_DRM_DRN(fcmpgt, OPCODE_FCMPGT_DRM_DRN)
DEF_BIN_DRM_DRN(fdiv, OPCODE_FDIV_DRM_DRN)
DEF_BIN_DRM_DRN(fmul, OPCODE_FMUL_DRM_DRN)
DEF_BIN_DRM_DRN(fsub, OPCODE_FSUB_DRM_DRN)

DEF_BIN_DRM_XDN(fmov, OPCODE_FMOV_DRM_XDN)

DEF_BIN_XDM_DRN(fmov, OPCODE_FMOV_XDM_DRN)

DEF_BIN_XDM_XDN(fmov, OPCODE_FMOV_XDM_XDN)

DEF_BIN_ARM_DRN(fmov, OPCODE_FMOV_ARM_DRN)

DEF_BIN_A_REG_RM_DRN(fmov, OPCODE_FMOV_A_R0_RM_DRN, r0)

DEF_BIN_ARMP_DRN(fmov, OPCODE_FMOV_ARMP_DRN)

DEF_BIN_ARM_XDN(fmov, OPCODE_FMOV_ARM_XDN)

DEF_BIN_ARMP_XDN(fmov, OPCODE_FMOV_ARMP_XDN)

DEF_BIN_A_REG_RM_XDN(fmov, OPCODE_FMOV_A_R0_RM_XDN, r0)

DEF_BIN_DRM_ARN(fmov, OPCODE_FMOV_DRM_ARN)

DEF_BIN_DRM_AMRN(fmov, OPCODE_FMOV_DRM_AMRN)

DEF_BIN_DRM_A_REG_RN(fmov, OPCODE_FMOV_DRM_A_R0_RN, r0)

DEF_BIN_XDM_ARN(fmov, OPCODE_FMOV_XDM_ARN)

DEF_BIN_XDM_AMRN(fmov, OPCODE_FMOV_XDM_AMRN)

DEF_BIN_XDM_A_REG_RN(fmov, OPCODE_FMOV_XDM_A_R0_RN, r0)

DEF_BIN_DRN(fabs, OPCODE_FABS_DRN)
DEF_BIN_DRN(fneg, OPCODE_FNEG_DRN)
DEF_BIN_DRN(fsqrt, OPCODE_FSQRT_DRN)

DEF_BIN_DRM_REG(fcnvds, OPCODE_FCNVDS_DRM_FPUL, fpul)
DEF_BIN_DRM_REG(ftrc, OPCODE_FTRC_DRM_FPUL, fpul)

DEF_BIN_REG_DRN(fcnvsd, OPCODE_FCNVSD_FPUL_DRN, fpul)
DEF_BIN_REG_DRN(float, OPCODE_FLOAT_FPUL_DRN, fpul)
DEF_BIN_REG_DRN(fsca, OPCODE_FSCA_FPUL_DRN, fpul)

DEF_BIN_FVM_FVN(fipr, OPCODE_FIPR_FVM_FVN)

DEF_BIN_REG_FVN(ftrv, OPCODE_FTRV_XMTRX_FVN, xmtrx)

#ifdef __cplusplus
}
#endif

#endif
