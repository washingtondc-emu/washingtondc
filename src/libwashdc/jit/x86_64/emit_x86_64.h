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

#ifndef EMIT_X86_64_H_
#define EMIT_X86_64_H_

#ifndef ENABLE_JIT_X86_64
#error this file should not be built when the x86_64 JIT backend is disabled
#endif

#include <stdint.h>

#define RAX 0
#define RCX 1
#define RDX 2
#define RBX 3
#define RSP 4
#define RBP 5
#define RSI 6
#define RDI 7
#define R8  8
#define R9  9
#define R10 10
#define R11 11
#define R12 12
#define R13 13
#define R14 14
#define R15 15

#define EAX RAX
#define ECX RCX
#define EDX RDX
#define EBX RBX
#define ESP RSP
#define EBP RBP
#define ESI RSI
#define EDI RDI
#define R8D  R8
#define R9D  R9
#define R10D R10
#define R11D R11
#define R12D R12
#define R13D R13
#define R14D R14
#define R15D R15

#define AX RAX
#define CX RCX
#define DX RDX
#define BX RBX
#define SP RSP
#define BP RBP
#define SI RSI
#define DI RDI
#define R8W  R8
#define R9W  R9
#define R10W R10
#define R11W R11
#define R12W R12
#define R13W R13
#define R14W R14
#define R15W R15

#define XMM0  0
#define XMM1  1
#define XMM2  2
#define XMM3  3
#define XMM4  4
#define XMM5  5
#define XMM6  6
#define XMM7  7
#define XMM8  8
#define XMM9  9
#define XMM10 10
#define XMM11 11
#define XMM12 12
#define XMM13 13
#define XMM14 14
#define XMM15 15

#define SIB 4
#define RIPREL 5

/*
 * This is the maximum number of jump-points that can jump to a given label.
 * This value is entirely arbitrary and may be increased as needed.
 */
#define MAX_LABEL_JUMPS 32

struct lbl_jmp_pt {
    int8_t *offs;
    uint8_t *rel_pos;
};

// label for 8-bit offset jumps
struct x86asm_lbl8 {
    // This is the pointer that the label points at
    uint8_t *ptr;

    /*
     * Labels are often referenced before they are defined (forward-jumps).
     * When this happens, the label-offsets will need to be redefined
     * retroactively after the label is defined.  jump_points points to every
     * jump that needs to be updated.
     */
    struct lbl_jmp_pt jump_points[MAX_LABEL_JUMPS];
    unsigned n_jump_points;
};

void x86asm_lbl8_init(struct x86asm_lbl8 *lbl);
void x86asm_lbl8_cleanup(struct x86asm_lbl8 *lbl);

// This will define the label to point to outp.
void x86asm_lbl8_define(struct x86asm_lbl8 *lbl);

void x86asm_lbl8_push_jmp_pt(struct x86asm_lbl8 *lbl,
                             struct lbl_jmp_pt const *jmp_pt);

void x86asm_set_dst(void *out_ptr, unsigned *out_n_bytes, unsigned n_bytes);
void *x86asm_get_out_ptr(void);

// call a function pointer contained in a general-purpose register
void x86asm_call_reg(unsigned reg_no);

// call a given function.  dst must be within 2^32 bytes from the PC
void x86asm_call(void *dst);

// move a 16-bit immediate into a general-purpose register
void x86asm_mov_imm16_reg(unsigned imm16, unsigned reg_no);

// move a 32-bit immediate into a general-purpose register
void x86asm_mov_imm32_reg32(unsigned imm32, unsigned reg_no);

void x86asm_xor_reg64_reg64(unsigned reg_src, unsigned reg_dst);

void x86asm_xorl_reg32_reg32(unsigned reg_src, unsigned reg_dst);

void x86asm_sal_imm8_reg64(unsigned imm8, unsigned reg_no);

void x86asm_or_reg64_reg64(unsigned reg_src, unsigned reg_dst);

// orl %<reg32>, %<reg32>
void x86asm_orl_reg32_reg32(unsigned reg_src, unsigned reg_dst);

void x86asm_ret(void);

// movl $<imm32>, <reg32>
void x86asm_mov_imm32_reg32(unsigned imm32, unsigned reg_no);

// movq $<imm64>, <reg64>
void x86asm_mov_imm64_reg64(uint64_t imm64, unsigned reg_no);

/*
 * this pseudo-instruction emits instructions to load the given pointer into
 * R10 and then and then CALL R10.  Obviously this means R10 will be clobbered,
 * but the System V AMD64 ABI used on Unix systems allows functions to clobber
 * that register anyways.
 */
void x86asm_call_ptr(void *ptr);

// mov $<imm32>, (<reg_no>)
void x86asm_mov_imm32_indreg32(unsigned imm32, unsigned reg_no);

// mov $<reg_src>, (<reg_dst>)
void x86asm_mov_reg32_indreg32(unsigned reg_src, unsigned reg_dst);

// movq %<reg_src>, (%<reg_dst>)
void x86asm_movq_reg64_indreg64(unsigned reg_src, unsigned reg_dst);

// movq <reg_src>, <reg_dst>
void x86asm_mov_reg64_reg64(unsigned reg_src, unsigned reg_dst);

/*
 * movq (%rip+disp32), %<reg_dst>
 *
 * relptr is relative to the address *after* this instruction.  This
 * instruction will always be 7 bytes long, so that's the position that relptr
 * is relative to.
 */
void x86asm_movq_riprel_reg(uint32_t relptr, unsigned reg_dst);

/*
 * movq %<reg_src>, (%rip+disp32)
 *
 * relptr is relative to the address *after* this instruction.  This
 * instruction will always be 7 bytes long, so that's the position that relptr
 * is relative to.
 */
void x86asm_movq_reg_riprel(unsigned reg_dst, uint32_t relptr);

// movl (<reg_src>), <reg_dst>
void x86asm_mov_indreg32_reg32(unsigned reg_src, unsigned reg_dst);

// movq (%<reg_src>), %<reg_dst>
void x86asm_movq_indreg_reg(unsigned reg_src, unsigned reg_dst);

// movq (%<reg_base>, <scale>, %<reg_index>), %<reg_dst>
void x86asm_movq_sib_reg(unsigned reg_base, unsigned scale,
                         unsigned reg_index, unsigned reg_dst);

// movl (%<reg_base>, <scale>, %<reg_index>), %<reg_dst>
void x86asm_movl_sib_reg(unsigned reg_base, unsigned scale,
                         unsigned reg_index, unsigned reg_dst);

// movw (%<reg_base>, <scale>, %<reg_index>), %<reg_dst>
void x86asm_movw_sib_reg(unsigned reg_base, unsigned scale,
                         unsigned reg_index, unsigned reg_dst);

// movb (%<reg_base>, <scale>, %<reg_index>), %<reg_dst>
void x86asm_movb_sib_reg(unsigned reg_base, unsigned scale,
                         unsigned reg_index, unsigned reg_dst);

// movq %<reg_src>, (%<reg_base>, <scale>, %<reg_index>)
void x86asm_movq_reg_sib(unsigned reg_src, unsigned reg_base,
                         unsigned scale, unsigned reg_index);

// movl %<reg_src>, (%<reg_base>, <scale>, %<reg_index>)
void x86asm_movl_reg_sib(unsigned reg_src, unsigned reg_base,
                         unsigned scale, unsigned reg_index);

// movb %<reg_src>, (%<reg_base>, <scale>, %<reg_index>)
void x86asm_movb_reg_sib(unsigned reg_src, unsigned reg_base,
                         unsigned scale, unsigned reg_index);

// movw (<reg_src>), <reg_dst>
void x86asm_mov_indreg16_reg16(unsigned reg_src, unsigned reg_dst);

// movl <disp8>(<reg_src>), <reg_dst>
void x86asm_movl_disp8_reg_reg(int disp8, unsigned reg_src, unsigned reg_dst);

// movl <disp32>(<reg_src>), <reg_dst>
void x86asm_movl_disp32_reg_reg(int disp32, unsigned reg_src, unsigned reg_dst);

// movq <disp8>(<reg_src>), <reg_dst>
void x86asm_movq_disp8_reg_reg(int disp8, unsigned reg_src, unsigned reg_dst);

// movq <disp32>(<reg_src>), <reg_dst>
void x86asm_movq_disp32_reg_reg(int disp32, unsigned reg_src, unsigned reg_dst);

// add $imm32, %eax
void x86asm_add_imm32_eax(unsigned imm32);

// addq $imm8, %<reg>
void x86asm_addq_imm8_reg(uint8_t imm8, unsigned reg);

void x86asm_addq_reg64_reg64(unsigned reg_src, unsigned reg_dst);

// addl %<reg_src>, %<reg_dst>
void x86asm_addl_reg32_reg32(unsigned reg_src, unsigned reg_dst);

// subl %<reg_src>, %<reg_dst>
void x86asm_subl_reg32_reg32(unsigned reg_src, unsigned reg_dst);

// subl %<reg_src>, %<reg_dst>
void x86asm_subq_reg64_reg64(unsigned reg_src, unsigned reg_dst);

// movl %<reg_src>, %<reg_dst>
void x86asm_mov_reg32_reg32(unsigned reg_src, unsigned reg_dst);

// pushq %<reg64>
void x86asm_pushq_reg64(unsigned reg);

// popq %<reg64>
void x86asm_popq_reg64(unsigned reg);

// andq $<imm32>, %rax
// the imm32 is sign-extended
void x86asm_and_imm32_rax(unsigned imm32);

// andl $<imm32>, %<reg32>
void x86asm_andl_imm32_reg32(uint32_t imm32, unsigned reg_no);

// andl %<reg32>, %<reg32>
void x86asm_andl_reg32_reg32(unsigned reg_src, unsigned reg_dst);

// cmp $<imm32>, %<reg64>
// the imm32 is sign-extended
void x86asm_cmp_imm32_reg64(unsigned imm32, unsigned reg64);

// xorq $<imm32>, %rax
// the imm32 is sign-extended
void x86asm_xor_imm32_rax(unsigned imm32);

// xorl $<imm32>, %eax
void x86asm_xorl_imm32_eax(unsigned imm32);

// notl %(reg)
void x86asm_notl_reg32(unsigned reg);

// notq %(reg)
void x86asm_not_reg64(unsigned reg);

// movsx (<%reg16>), %<reg32>
// reg16 is a 64-bit pointer to a 16-bit integer
void x86asm_movsx_indreg16_reg32(unsigned reg_src, unsigned reg_dst);

/*
 * cmpl #imm8, %<reg_no>
 * compare the given register with the (sign-extended) imm8
 */
void x86asm_cmpl_imm8_reg32(unsigned imm8, unsigned reg_no);

/*
 * cmpl #imm8, %<reg_no>
 * compare the given register with imm32
 */
void x86asm_cmpl_imm32_reg32(unsigned imm32, unsigned reg_no);

/*
 * cmpl %<reg_rhs>, %<reg_lhs>
 *
 * compare the two given registers by subtracting.  Keep in mind that the lhs
 * goes on the right and the rhs goes on the left for this function call
 * because I'm trying to vaguely mimic the syntax of GNU as.
 */
void x86asm_cmpl_reg32_reg32(unsigned reg_lhs, unsigned reg_rhs);

/*
 * cmpq %<reg_rhs>, %<reg_lhs>
 *
 * compare the two given registers by subtracting.  Keep in mind that the lhs
 * goes on the right and the rhs goes on the left for this function call
 * because I'm trying to vaguely mimic the syntax of GNU as.
 */
void x86asm_cmpq_reg64_reg64(unsigned reg_lhs, unsigned reg_rhs);

/*
 * jz (pc+disp8)
 *
 * jump if the zero-flag is set (meaning a cmp was equal)
 */
void x86asm_jz_disp8(int disp8);

void x86asm_jz_lbl8(struct x86asm_lbl8 *lbl);

// jnz (pc + disp8)
void x86asm_jnz_disp8(int disp8);
void x86asm_jnz_lbl8(struct x86asm_lbl8 *lbl);

// jnge (pc + disp8)
void x86asm_jnge_disp8(int disp8);
void x86asm_jnge_lbl8(struct x86asm_lbl8 *lbl);

/*
 * jns (pc+disp8)
 *
 * Jump if Not Signed
 */
void x86asm_jns_disp8(int disp8);
void x86asm_jns_lbl8(struct x86asm_lbl8 *lbl);

/*
 * ja (pc+disp8)
 *
 * jump if the carry flag and zero flag are both not set (meaning a cmp was
 * greater-than).
 */
void x86asm_ja_disp8(int disp8);
void x86asm_ja_lbl8(struct x86asm_lbl8 *lbl);

/*
 * jae (pc+disp8)
 *
 * jump if above or equal (carry-flag is 0)
 */
void x86asm_jae_disp8(int disp8);
void x86asm_jae_disp32(uint32_t disp32);
void x86asm_jae_lbl8(struct x86asm_lbl8 *lbl);

/*
 * jbe (pc+disp8)
 *
 * jump if below or equal
 */
void x86asm_jbe_disp8(int disp8);
void x86asm_jbe_disp32(uint32_t disp32);
void x86asm_jbe_lbl8(struct x86asm_lbl8 *lbl);

void x86asm_jb_disp8(int disp8);
void x86asm_jb_lbl8(struct x86asm_lbl8 *lbl);

/*
 * jl (pc + disp8
 * jump if less (signed)
 */
void x86asm_jl_disp8(int disp8);
void x86asm_jl_lbl8(struct x86asm_lbl8 *lbl);

/*
 * jle (pc + disp8
 * jump if less (signed)
 */
void x86asm_jle_disp8(int disp8);
void x86asm_jle_lbl8(struct x86asm_lbl8 *lbl);

void x86asm_jmp_disp8(int disp8);
void x86asm_jmp_lbl8(struct x86asm_lbl8 *lbl);

// movsx <%reg8>, %<reg32>
void x86asm_movsx_reg8_reg32(unsigned reg_src, unsigned reg_dst);

// movsx <%reg16>, %<reg32>
void x86asm_movsx_reg16_reg32(unsigned reg_src, unsigned reg_dst);

// movzxw (%<reg_src>), %<reg_dst>
void x86asm_movzxw_indreg_reg(unsigned reg_src, unsigned reg_dst);

// orl $<imm32>, %<reg_no>
void x86asm_orl_imm32_reg32(unsigned imm32, unsigned reg_no);

// xorl $<imm32>, %<reg_no>
void x86asm_xorl_imm32_reg32(unsigned imm32, unsigned reg_no);

// incl %<reg_no>
void x86asm_incl_reg32(unsigned reg_no);

// shll $<imm8>, %reg_no
void x86asm_shll_imm8_reg32(unsigned imm8, unsigned reg_no);

// shll %cl, reg_no
void x86asm_shll_cl_reg32(unsigned reg_no);

// shrl $<imm8>, %reg_no
void x86asm_shrl_imm8_reg32(unsigned imm8, unsigned reg_no);

// shrl %cl, reg_no
void x86asm_shrl_cl_reg32(unsigned reg_no);

// sarl %cl, reg_no
void x86asm_sarl_cl_reg32(unsigned reg_no);

// sarl $<imm8>, %reg_no
void x86asm_sarl_imm8_reg32(unsigned imm8, unsigned reg_no);

void* x86asm_get_outp(void);

/*
 * mull %reg_no
 * Performs an unsigned 32-bit multiplication of the given register and %eax.
 *
 * The result of this multiplication is a 64-bit integer left in %eax and %edx
 * (with %eax being the lower four bytes).  Obviously this means both of those
 * registers will be clobbered.
 */
void x86asm_mull_reg32(unsigned reg_no);

void x86asm_testl_reg32_reg32(unsigned reg_src, unsigned reg_dst);
void x86asm_testq_reg64_reg64(unsigned reg_src, unsigned reg_dst);

void x86asm_testl_imm32_reg32(uint32_t imm32, unsigned reg_no);

// conditional-move if not-equal (ZF=0)
void x86asm_cmovnel_reg32_reg32(unsigned reg_src, unsigned reg_dst);

// conditional-move if equal (ZF=1)
void x86asm_cmovel_reg32_reg32(unsigned reg_src, unsigned reg_dst);

// conditional-move if greater (unsigned) (CF=0, ZF=0)
void x86asm_cmoval_reg32_reg32(unsigned reg_src, unsigned reg_dst);

// conditional-move if greater (signed)
void x86asm_cmovgl_reg32_reg32(unsigned reg_src, unsigned reg_dst);

// conditional-move if greater-or-equal (signed)
void x86asm_cmovgel_reg32_reg32(unsigned reg_src, unsigned reg_dst);

// conditional-move if greater-or-equal (unsigned)
void x86asm_cmovael_reg32_reg32(unsigned reg_src, unsigned reg_dst);

void x86asm_setnzl_reg32(unsigned reg_no);

void x86asm_negl_reg32(unsigned reg_no);

void x86asm_jmpq_reg64(unsigned reg_no);

void x86asm_jmpq_offs32(int32_t offs);

// movb <disp8>(%<reg_src>), <reg_dst>
void x86asm_movb_disp8_reg_reg(int disp8, unsigned reg_src, unsigned reg_dst);

// movb %<reg_src>, <disp8>(%<reg_dst>)
void x86asm_movb_reg_disp8_reg(unsigned reg_src, int disp8, unsigned reg_dst);

void x86asm_jmp_disp8(int disp8);
void x86asm_jmp_lbl8(struct x86asm_lbl8 *lbl);

/*******************************************************************************
 *
 * SSE instructions
 *
 *******************************************************************************/

// movss (%<reg_src>), %<xmm_reg_dst>
void x86asm_movss_indreg_xmm(unsigned reg_src, unsigned xmm_reg_dst);

// movss %<xmm_reg_src>, (%<reg_dst>)
void x86asm_movss_xmm_indreg(unsigned xmm_reg_src, unsigned reg_dst);

// movss %<xmm_reg_src>, %<xmm_reg_dst>
void x86asm_movss_xmm_xmm(unsigned xmm_reg_src, unsigned xmm_reg_dst);

// movss <disp8>(%reg_src), <xmm_reg_dst
void
x86asm_movss_disp8_reg_xmm(int disp8, unsigned reg_src, unsigned xmm_reg_dst);

// movss <disp32>(%reg_src), <xmm_reg_dst
void
x86asm_movss_disp32_reg_xmm(int disp32, unsigned reg_src, unsigned xmm_reg_dst);

#endif
