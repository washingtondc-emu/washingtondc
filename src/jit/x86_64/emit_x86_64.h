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

#ifndef EMIT_X86_64_H_
#define EMIT_X86_64_H_

#ifndef ENABLE_JIT_X86_64
#error this file should not be built when the x86_64 JIT backend is disabled
#endif

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

void x86asm_set_dst(void *out_ptr, unsigned n_bytes);

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

// movq <reg_src>, <reg_dst>
void x86asm_mov_reg64_reg64(unsigned reg_src, unsigned reg_dst);

// movl (<reg_src>), <reg_dst>
void x86asm_mov_indreg32_reg32(unsigned reg_src, unsigned reg_dst);

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

// addl %<reg_src>, %<reg_dst>
void x86asm_addl_reg32_reg32(unsigned reg_src, unsigned reg_dst);

// subl %<reg_src>, %<reg_dst>
void x86asm_subl_reg32_reg32(unsigned reg_src, unsigned reg_dst);

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
void x86asm_cmpl_reg32_imm8(unsigned reg_no, unsigned imm8);

/*
 * jz (pc+disp8)
 *
 * jump if the zero-flag is set (meaning a cmp was equal)
 */
void x86asm_jz_disp8(unsigned disp8);

// movsx <%reg16>, %<reg32>
void x86asm_movsx_reg16_reg32(unsigned reg_src, unsigned reg_dst);

// movzxw (%<reg_src>), %<reg_dst>
void x86asm_movzxw_indreg_reg(unsigned reg_src, unsigned reg_dst);

// orl $<imm32>, %<reg_no>
void x86asm_orl_imm32_reg32(unsigned imm32, unsigned reg_no);

// xorl $<imm32>, %<reg_no>
void x86asm_xorl_imm32_reg32(unsigned imm32, unsigned reg_no);

#endif
