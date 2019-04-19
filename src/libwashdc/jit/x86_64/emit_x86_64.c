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

#ifndef ENABLE_JIT_X86_64
#error this file should not be built when the x86_64 JIT backend is disabled
#endif

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>

#include "log.h"
#include "washdc/error.h"
#include "exec_mem.h"

#include "emit_x86_64.h"

#define X86_64_GROW_SIZE 32

static void *alloc_start;
static unsigned alloc_len;

static uint8_t *outp;
static unsigned outp_len;

static void try_grow(void) {
    if (!alloc_start)
        RAISE_ERROR(ERROR_INTEGRITY);
    if (exec_mem_grow(alloc_start, alloc_len + X86_64_GROW_SIZE) != 0) {
        LOG_ERROR("Unable to grow allocation to %u bytes\n",
                  alloc_len + X86_64_GROW_SIZE);
        struct exec_mem_stats stats;
        exec_mem_get_stats(&stats);
        exec_mem_print_stats(&stats);
        RAISE_ERROR(ERROR_OVERFLOW);
    }

    alloc_len += X86_64_GROW_SIZE;
    outp_len += X86_64_GROW_SIZE;
}

__attribute__((unused))
static void put8(uint8_t val) {
    if (outp_len >= 1) {
        *outp++ = val;
        outp_len--;
    } else {
        try_grow();
        put8(val);
    }
}

__attribute__((unused))
static void put16(uint16_t val) {
    if (outp_len >= 2) {
        memcpy(outp, &val, sizeof(val));
        outp += 2;
        outp_len -= 2;
    } else {
        try_grow();
        put16(val);
    }
}

__attribute__((unused))
static void put32(uint32_t val) {
    if (outp_len >= 4) {
        memcpy(outp, &val, sizeof(val));
        outp += 4;
        outp_len -= 4;
    } else {
        try_grow();
        put32(val);
    }
}

__attribute__((unused))
static void put64(uint64_t val) {
    if (outp_len >= 8) {
        memcpy(outp, &val, sizeof(val));
        outp += 8;
        outp_len -= 8;
    } else {
        try_grow();
        put64(val);
    }
}

void* x86asm_get_outp(void) {
    return outp;
}

#define REX_W (1 << 3) // 64-bit operand size
#define REX_R (1 << 2) // register extension
#define REX_X (1 << 1) // sib index extension
#define REX_B (1 << 0) // rm extension (or sib base)

static void emit_mod_reg_rm(unsigned rex, unsigned opcode, unsigned mod,
                            unsigned reg, unsigned rm) {
    if (reg >= R8) {
        rex |= REX_R;
        reg -= R8;
    }
    if (rm >= R8) {
        rex |= REX_B;
        rm -= R8;
    }

    bool sib_rsp = (rm == RSP) && (mod != 3);
    bool sib_rbp = (rm == RBP) && (mod == 0);

    if (rex)
        put8(rex | 0x40);
    put8(opcode);

    if (sib_rbp) {
        /*
         * Special case - having a mod of 0 and an R/M of 5 replaces R/M with a
         * 32-bit displacement (from RIP, i think), so we need to set the mod
         * to 1 or 2, which adds an 8 or 32 bit displacement relative to the
         * given register.  Then set that displacement to 0.
         */
        unsigned mod_reg_rm = (1 << 6) | (reg << 3) | rm;
        put8(mod_reg_rm);
        put8(0);
    } else {
        unsigned mod_reg_rm = (mod << 6) | (reg << 3) | rm;
        put8(mod_reg_rm);
        if (sib_rsp) {
            /*
             * Special case - using RSP for the R/M puts the x86 in SIB mode,
             * so we need to craft a SIB byte for (%RSP).
             */
            put8((RSP << 3) | RSP); // using RSP as an index counts as 0
        }
    }
}

/*
 * Special version of emit_mod_reg_rm which assumes the next byte will be a
 * SIB.  You have to emit the SIB in yourself after calling this function.
 */
static void emit_mod_reg_rm_sib(unsigned rex, unsigned opcode, unsigned mod,
                                unsigned reg, unsigned rm) {
    if (reg >= R8) {
        rex |= REX_R;
        reg -= R8;
    }
    if (rm >= R8) {
        rex |= REX_B;
        rm -= R8;
    }

    if (rex)
        put8(rex | 0x40);
    put8(opcode);

    unsigned mod_reg_rm = (mod << 6) | (reg << 3) | rm;
    put8(mod_reg_rm);
}

/*
 * This doesn't actually output the riprel value, you have to do that yourself
 * with put32 after calling this.
 *
 * What this does do is emit the rex prefix, opcode and mod/reg/rm byte for a
 * riprel instruction.
 */
static void emit_mod_reg_rm_riprel(unsigned rex, unsigned opcode, unsigned mod,
                                   unsigned reg) {
    if (reg >= R8) {
        rex |= REX_R;
        reg -= R8;
    }

    if (rex)
        put8(rex | 0x40);
    put8(opcode);

    unsigned mod_reg_rm = (mod << 6) | (reg << 3) | RIPREL;
    put8(mod_reg_rm);
}

static void emit_mod_reg_rm_2(unsigned rex, unsigned opcode1,
                              unsigned opcode2, unsigned mod,
                              unsigned reg, unsigned rm) {
    if (reg >= R8) {
        rex |= REX_R;
        reg -= R8;
    }
    if (rm >= R8) {
        rex |= REX_B;
        rm -= R8;
    }

    bool sib_rsp = (rm == RSP) && (mod != 3);
    bool sib_rbp = (rm == RBP) && (mod != 3);

    if (rex)
        put8(rex | 0x40);
    put8(opcode1);
    put8(opcode2);
    if (sib_rbp) {
        /*
         * Special case - having a mod of 0 and an R/M of 5 replaces R/M with a
         * 32-bit displacement (from RIP, i think), so we need to set the mod
         * to 1 or 2, which adds an 8 or 32 bit displacement relative to the
         * given register.  Then set that displacement to 0.
         */
        unsigned mod_reg_rm = (1 << 6) | (reg << 3) | rm;
        put8(mod_reg_rm);
        put8(0);
    } else {
        unsigned mod_reg_rm = (mod << 6) | (reg << 3) | rm;
        put8(mod_reg_rm);
        if (sib_rsp) {
            /*
             * Special case - using RSP for the R/M puts the x86 in SIB mode,
             * so we need to craft a SIB byte for (%RSP).
             */
            put8((RSP << 3) | RSP); // using RSP as an index counts as 0
        }
    }
}

void x86asm_set_dst(void *out_ptr, unsigned n_bytes) {
    alloc_start = out_ptr;
    alloc_len = n_bytes;
    outp = (uint8_t*)out_ptr;
    outp_len = n_bytes;
}

void x86asm_call_reg(unsigned reg_no) {
    /*
     * OPCODE: 0xff
     * MOD: 0
     * REG: 2
     * R/M: <reg_no>
     */
    emit_mod_reg_rm(0, 0xff, 3, 2, reg_no);
    /* put8(0xff); */
    /* put8(0xd0 | reg_no); */
}

void x86asm_jmpq_reg64(unsigned reg_no) {
    emit_mod_reg_rm(0, 0xff, 3, 4, reg_no);
}

void x86asm_mov_imm16_reg(unsigned imm16, unsigned reg_no) {
    /*
     * PREFIX: 0x66 (16-bit operand)
     * OPCODE: c7
     * MOD: 3
     * REG: 0
     * R/M: register
     */
    put8(0x66);
    emit_mod_reg_rm(0, 0xc7, 3, 0, reg_no);
    put16(imm16);
}

void x86asm_mov_imm32_reg32(unsigned imm32, unsigned reg_no) {
    /*
     * OPCODE: c7
     * MOD: 3
     * REG: 0
     * R/M: register
     */

    // literally just the 16-bit version without the prefix
    emit_mod_reg_rm(0, 0xc7, 3, 0, reg_no);
    put32(imm32);
}

void x86asm_sal_imm8_reg64(unsigned imm8, unsigned reg_no) {
    /*
     * PREFIX: 0x48 (64-bit operand REX)
     * OPCODE: c1
     * MOD: 3
     * REG: 4
     * R/M: register
     */
    emit_mod_reg_rm(REX_W, 0xc1, 3, 4, reg_no);
    put8(imm8);
}

void x86asm_xor_reg64_reg64(unsigned reg_src, unsigned reg_dst) {
    /*
     * PREFIX: 0x48 (64-bit operand REX)
     * OPCODE: 33
     * MOD: 3
     * REG: destination register
     * R/M: source register
     */
    emit_mod_reg_rm(REX_W, 0x33, 3, reg_dst, reg_src);
}

void x86asm_xorl_reg32_reg32(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(0, 0x33, 3, reg_dst, reg_src);
}

void x86asm_or_reg64_reg64(unsigned reg_src, unsigned reg_dst) {
    /*
     * PREFIX: 0x48 (64-bit operand REX)
     * OPCODE: 09
     * MOD: 3
     * REG: destination register
     * R/M: source register
     */
    emit_mod_reg_rm(REX_W, 0x09, 3, reg_src, reg_dst);
}

// orl %<reg32>, %<reg32>
void x86asm_orl_reg32_reg32(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(0, 0x09, 3, reg_src, reg_dst);
}

void x86asm_ret(void) {
    /*
     * OPCODE: C3
     */
    put8(0xc3);
}

void x86asm_mov_imm64_reg64(uint64_t imm64, unsigned reg_no) {
    unsigned rex = 0x40 | REX_W;
    if (reg_no >= R8) {
        reg_no -= R8;
        rex |= REX_B;
    }
    put8(rex);
    put8(0xb8 | reg_no);
    put64(imm64);
}

void x86asm_call(void *dst) {
    size_t offs = (uint8_t*)dst - outp;

    if (offs > INT_MAX)
        abort();

    put8(0xe8);
    put32(offs);
}

void x86asm_call_ptr(void *ptr) {
    x86asm_mov_imm64_reg64((uint64_t)(uintptr_t)ptr, R10);
    x86asm_call_reg(R10);
}

// mov $<imm32>, (<reg_no>)
void x86asm_mov_imm32_indreg32(unsigned imm32, unsigned reg_no) {
    /*
     * OPCODE: C7
     * MOD: 0
     * R/M: <reg-ptr>
     */
    emit_mod_reg_rm(0, 0xc7, 0, 0, reg_no);
}

// movl %<reg_src>, (%<reg_dst>)
void x86asm_mov_reg32_indreg32(unsigned reg_src, unsigned reg_dst) {
    /*
     * OPCODE: 0x89
     * MOD: 0
     * REG: <reg_src>
     * R/M: <reg_dst>
     */
    /* put8(0x67); // TODO: do i need prefix? */
    emit_mod_reg_rm(0, 0x89, 0, reg_src, reg_dst);
}

// movq %<reg_src>, (%<reg_dst>)
void x86asm_movq_reg64_indreg64(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(REX_W, 0x89, 0, reg_src, reg_dst);
}

// movq <reg_src>, <reg_dst>
void x86asm_mov_reg64_reg64(unsigned reg_src, unsigned reg_dst) {
    /*
     * PREFIX: 0x48
     * OPCODE: 0x89
     * MOD: 0x3
     * REG: <reg_src>
     * R/M: <reg_dst>
     */
    emit_mod_reg_rm(REX_W, 0x89, 3, reg_src, reg_dst);
}

// movq (%<reg_src>), %<reg_dst>
void x86asm_movq_indreg_reg(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(REX_W, 0x8b, 0, reg_dst, reg_src);
}

/*
 * movq (%rip+disp32), %<reg_dst>
 */
void x86asm_movq_riprel_reg(uint32_t relptr, unsigned reg_dst) {
    emit_mod_reg_rm_riprel(REX_W, 0x8b, 0, reg_dst);
    put32(relptr);
}

/*
 * movq %<reg_src>, (%rip+disp32)
 *
 * relptr is relative to the address *after* this instruction.  This
 * instruction will always be 7 bytes long, so that's the position that relptr
 * is relative to.
 */
void x86asm_movq_reg_riprel(unsigned reg_dst, uint32_t relptr) {
    emit_mod_reg_rm_riprel(REX_W, 0x89, 0, reg_dst);
    put32(relptr);
}

// movq (%<reg_base>, <scale>, %<reg_index>), %<reg_dst>
void x86asm_movq_sib_reg(unsigned reg_base, unsigned scale,
                         unsigned reg_index, unsigned reg_dst) {
    unsigned log2;
    switch (scale) {
    case 1:
        log2 = 0;
        break;
    case 2:
        log2 = 1;
        break;
    case 4:
        log2 = 2;
        break;
    case 8:
        log2 = 3;
        break;
    default:
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned rex = REX_W;
    if (reg_base >= R8) {
        rex |= REX_B;
        reg_base -= R8;
    }
    if (reg_index >= R8) {
        rex |= REX_X;
        reg_index -= R8;
    }

    emit_mod_reg_rm_sib(rex, 0x8b, 0, reg_dst, SIB);

    unsigned sib = reg_base | (reg_index << 3) | (log2 << 6);
    put8(sib);
}

// movl (%<reg_base>, <scale>, %<reg_index>), %<reg_dst>
void x86asm_movl_sib_reg(unsigned reg_base, unsigned scale,
                         unsigned reg_index, unsigned reg_dst) {
    unsigned log2;
    switch (scale) {
    case 1:
        log2 = 0;
        break;
    case 2:
        log2 = 1;
        break;
    case 4:
        log2 = 2;
        break;
    case 8:
        log2 = 3;
        break;
    default:
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned rex = 0;
    if (reg_base >= R8) {
        rex |= REX_B;
        reg_base -= R8;
    }
    if (reg_index >= R8) {
        rex |= REX_X;
        reg_index -= R8;
    }

    emit_mod_reg_rm_sib(rex, 0x8b, 0, reg_dst, SIB);

    unsigned sib = reg_base | (reg_index << 3) | (log2 << 6);
    put8(sib);
}

// movw (%<reg_base>, <scale>, %<reg_index>), %<reg_dst>
void x86asm_movw_sib_reg(unsigned reg_base, unsigned scale,
                         unsigned reg_index, unsigned reg_dst) {
    unsigned log2;
    switch (scale) {
    case 1:
        log2 = 0;
        break;
    case 2:
        log2 = 1;
        break;
    case 4:
        log2 = 2;
        break;
    case 8:
        log2 = 3;
        break;
    default:
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned rex = 0;
    if (reg_base >= R8) {
        rex |= REX_B;
        reg_base -= R8;
    }
    if (reg_index >= R8) {
        rex |= REX_X;
        reg_index -= R8;
    }

    put8(0x66);

    emit_mod_reg_rm_sib(rex, 0x8b, 0, reg_dst, SIB);

    unsigned sib = reg_base | (reg_index << 3) | (log2 << 6);
    put8(sib);
}

// movq %<reg_src>, (%<reg_base>, <scale>, %<reg_index>)
void x86asm_movq_reg_sib(unsigned reg_src, unsigned reg_base,
                         unsigned scale, unsigned reg_index) {
    unsigned log2;
    switch (scale) {
    case 1:
        log2 = 0;
        break;
    case 2:
        log2 = 1;
        break;
    case 4:
        log2 = 2;
        break;
    case 8:
        log2 = 3;
        break;
    default:
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned rex = REX_W;
    if (reg_base >= R8) {
        rex |= REX_B;
        reg_base -= R8;
    }
    if (reg_index >= R8) {
        rex |= REX_X;
        reg_index -= R8;
    }

    emit_mod_reg_rm_sib(rex, 0x89, 0, reg_src, SIB);

    unsigned sib = reg_base | (reg_index << 3) | (log2 << 6);
    put8(sib);
}

// movl %<reg_src>, (%<reg_base>, <scale>, %<reg_index>)
void x86asm_movl_reg_sib(unsigned reg_src, unsigned reg_base,
                         unsigned scale, unsigned reg_index) {
    unsigned log2;
    switch (scale) {
    case 1:
        log2 = 0;
        break;
    case 2:
        log2 = 1;
        break;
    case 4:
        log2 = 2;
        break;
    case 8:
        log2 = 3;
        break;
    default:
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    unsigned rex = 0;
    if (reg_base >= R8) {
        rex |= REX_B;
        reg_base -= R8;
    }
    if (reg_index >= R8) {
        rex |= REX_X;
        reg_index -= R8;
    }

    emit_mod_reg_rm_sib(rex, 0x89, 0, reg_src, SIB);

    unsigned sib = reg_base | (reg_index << 3) | (log2 << 6);
    put8(sib);
}

// movl (<reg_src>), <reg_dst>
void x86asm_mov_indreg32_reg32(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(0, 0x8b, 0, reg_dst, reg_src);
}

// movw (<reg_src>), <reg_dst>
void x86asm_mov_indreg16_reg16(unsigned reg_src, unsigned reg_dst) {
    put8(0x66);
    emit_mod_reg_rm(0, 0x8b, 0, reg_dst, reg_src);
}

// movb <disp8>(%<reg_src>), <reg_dst>
void x86asm_movb_disp8_reg_reg(int disp8, unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(0, 0x8a, 1, reg_dst, reg_src);
    put8(disp8);
}

// movb %<reg_src>, <disp8>(%<reg_dst>)
void x86asm_movb_reg_disp8_reg(unsigned reg_src, int disp8, unsigned reg_dst) {
    emit_mod_reg_rm(0, 0x88, 1, reg_src, reg_dst);
    put8(disp8);
}

// movl <disp8>(<reg_src>), <reg_dst>
void x86asm_movl_disp8_reg_reg(int disp8, unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(0, 0x8b, 1, reg_dst, reg_src);
    put8(disp8);
}

// movl <disp32>(<reg_src>), <reg_dst>
void x86asm_movl_disp32_reg_reg(int disp32, unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(0, 0x8b, 2, reg_dst, reg_src);
    put32(disp32);
}

// movq <disp8>(<reg_src>), <reg_dst>
void x86asm_movq_disp8_reg_reg(int disp8, unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(REX_W, 0x8b, 1, reg_dst, reg_src);
    put8(disp8);
}

// movq <disp32>(<reg_src>), <reg_dst>
void x86asm_movq_disp32_reg_reg(int disp32, unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(REX_W, 0x8b, 2, reg_dst, reg_src);
    put32(disp32);
}

void x86asm_add_imm32_eax(unsigned imm32) {
    put8(0x05);
    put32(imm32);
}

// addl %<reg_src>, %<reg_dst>
void x86asm_addl_reg32_reg32(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(0, 3, 3, reg_dst, reg_src);
}

void x86asm_addq_reg64_reg64(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(REX_W, 3, 3, reg_dst, reg_src);
}

// subl %<reg_src>, %<reg_dst>
void x86asm_subl_reg32_reg32(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(0, 0x29, 3, reg_src, reg_dst);
}

// movl %<reg_src>, %<reg_dst>
void x86asm_mov_reg32_reg32(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(0, 0x89, 3, reg_src, reg_dst);
}

// pushq %<reg64>
void x86asm_pushq_reg64(unsigned reg) {
    // no need to set the W bit, PUSH defaults to 64-bit
    unsigned rex = 0;
    if (reg >= R8) {
        rex |= REX_B;
        reg -= R8;
    }
    if (rex)
        put8(0x40 | rex);
    put8(0x50 | reg);
}

// popq %<reg64>
void x86asm_popq_reg64(unsigned reg) {
    // no need to set the W bit, POP defaults to 64-bit
    unsigned rex = 0;
    if (reg >= R8) {
        rex |= REX_B;
        reg -= R8;
    }
    if (rex)
        put8(0x40 | rex);
    put8(0x58 | reg);
}

// and $<imm32>, %rax
// the imm32 is sign-extended
void x86asm_and_imm32_rax(unsigned imm32) {
    put8(0x40 | REX_W);
    put8(0x25);
    put32(imm32);
}

// andl $<imm32>, %<reg32>
void x86asm_andl_imm32_reg32(uint32_t imm32, unsigned reg_no) {
    emit_mod_reg_rm(0, 0x81, 3, 4, reg_no);
    put32(imm32);
}

// andl %<reg32>, %<reg32>
void x86asm_andl_reg32_reg32(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(0, 0x21, 3, reg_src, reg_dst);
}

// cmp $<imm32>, %<reg64>
// the imm32 is sign-extended
void x86asm_cmp_imm32_reg64(unsigned imm32, unsigned reg64) {
    emit_mod_reg_rm(REX_W, 0x81, 3, 0x07, reg64);
    put32(imm32);
}

// xorl $<imm32>, %eax
void x86asm_xorl_imm32_eax(unsigned imm32) {
    put8(0x35);
    put32(imm32);
}

// xor $<imm32>, %rax
// the imm32 is sign-extended
void x86asm_xor_imm32_rax(unsigned imm32) {
    put8(0x40 | REX_W);
    put8(0x35);
    put32(imm32);
}

// notl %(reg)
void x86asm_notl_reg32(unsigned reg) {
    emit_mod_reg_rm(0, 0xf7, 3, 0x02, reg);
}

// notq %(reg)
void x86asm_not_reg64(unsigned reg) {
    emit_mod_reg_rm(REX_W, 0xf7, 3, 0x02, reg);
}

// movsx (<%reg16>), %<reg32>
// reg16 is a 64-bit pointer to a 16-bit integer
void x86asm_movsx_indreg16_reg32(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm_2(0, 0x0f, 0xbf, 0, reg_dst, reg_src);
}

/*
 * cmpl #imm8, %<reg_no>
 * compare the given register with the (sign-extended) imm8
 */
void x86asm_cmpl_imm8_reg32(unsigned imm8, unsigned reg_no) {
    emit_mod_reg_rm(0, 0x83, 3, 7, reg_no);
    put8(imm8);
}

/*
 * cmpl #imm8, %<reg_no>
 * compare the given register with imm32
 */
void x86asm_cmpl_imm32_reg32(unsigned imm32, unsigned reg_no) {
    emit_mod_reg_rm(0, 0x81, 3, 7, reg_no);
    put32(imm32);
}

/*
 * cmpl %<reg_rhs>, %<reg_lhs>
 *
 * compare the two given registers by subtracting.  Keep in mind that the lhs
 * goes on the right and the rhs goes on the left for this function call
 * because I'm trying to vaguely mimic the syntax of GNU as.
 */
void x86asm_cmpl_reg32_reg32(unsigned reg_lhs, unsigned reg_rhs) {
    emit_mod_reg_rm(0, 0x39, 3, reg_lhs, reg_rhs);
}

void x86asm_cmpq_reg64_reg64(unsigned reg_lhs, unsigned reg_rhs) {
    emit_mod_reg_rm(REX_W, 0x39, 3, reg_lhs, reg_rhs);
}

void x86asm_jmp_disp8(int disp8) {
    put8(0xeb);
    put8(disp8);
}

void x86asm_jmp_lbl8(struct x86asm_lbl8 *lbl) {
    struct lbl_jmp_pt pt;
    put8(0xeb);

    pt.offs = (int8_t*)outp;
    pt.rel_pos = outp + 1;

    put8(0); // temporary placeholder for the offset value
    x86asm_lbl8_push_jmp_pt(lbl, &pt);
}

/*
 * jz (pc+disp8)
 *
 * jump if the zero-flag is set (meaning a cmp was equal).
 *
 * The disp8 is a signed value relative to what the PC would otherwise be
 * *after* this instruction (which is always two bytes long) has executed.
 */
void x86asm_jz_disp8(int disp8) {
    put8(0x74);
    put8(disp8);
}

void x86asm_jz_lbl8(struct x86asm_lbl8 *lbl) {
    struct lbl_jmp_pt pt;
    put8(0x74);

    pt.offs = (int8_t*)outp;
    pt.rel_pos = outp + 1;

    put8(0); // temporary placeholder for the offset value
    x86asm_lbl8_push_jmp_pt(lbl, &pt);
}

void x86asm_jnz_disp8(int disp8) {
    put8(0x75);
    put8(disp8);
}

void x86asm_jnz_lbl8(struct x86asm_lbl8 *lbl) {
    struct lbl_jmp_pt pt;
    put8(0x75);

    pt.offs = (int8_t*)outp;
    pt.rel_pos = outp + 1;

    put8(0); // temporary placeholder for the offset value
    x86asm_lbl8_push_jmp_pt(lbl, &pt);
}

/*
 * ja (pc+disp8)
 *
 * jump if the carry flag and zero flag are both not set (meaning a cmp was
 * greater-than).
 */
void x86asm_ja_disp8(int disp8) {
    put8(0x77);
    put8(disp8);
}

void x86asm_ja_lbl8(struct x86asm_lbl8 *lbl) {
    struct lbl_jmp_pt pt;
    put8(0x77);

    pt.offs = (int8_t*)outp;
    pt.rel_pos = outp + 1;

    put8(0); // temporary placeholder for the offset value
    x86asm_lbl8_push_jmp_pt(lbl, &pt);
}

void x86asm_jbe_disp8(int disp8) {
    put8(0x76);
    put8(disp8);
}

void x86asm_jbe_lbl8(struct x86asm_lbl8 *lbl) {
    struct lbl_jmp_pt pt;
    put8(0x76);

    pt.offs = (int8_t*)outp;
    pt.rel_pos = outp + 1;

    put8(0); // temporary placeholder for the offset value
    x86asm_lbl8_push_jmp_pt(lbl, &pt);
}

void x86asm_jb_disp8(int disp8) {
    put8(0x72);
    put8(disp8);
}

void x86asm_jb_lbl8(struct x86asm_lbl8 *lbl) {
    struct lbl_jmp_pt pt;
    put8(0x72);

    pt.offs = (int8_t*)outp;
    pt.rel_pos = outp + 1;

    put8(0); // temporary placeholder for the offset value
    x86asm_lbl8_push_jmp_pt(lbl, &pt);
}

void x86asm_jl_disp8(int disp8) {
    put8(0x7c);
    put8(disp8);
}

void x86asm_jl_lbl8(struct x86asm_lbl8 *lbl) {
    struct lbl_jmp_pt pt;
    put8(0x7c);

    pt.offs = (int8_t*)outp;
    pt.rel_pos = outp + 1;

    put8(0); // temporary placeholder for the offset value
    x86asm_lbl8_push_jmp_pt(lbl, &pt);
}

void x86asm_jle_disp8(int disp8) {
    put8(0x7e);
    put8(disp8);
}

void x86asm_jle_lbl8(struct x86asm_lbl8 *lbl) {
    struct lbl_jmp_pt pt;
    put8(0x7e);

    pt.offs = (int8_t*)outp;
    pt.rel_pos = outp + 1;

    put8(0); // temporary placeholder for the offset value
    x86asm_lbl8_push_jmp_pt(lbl, &pt);
}

void x86asm_jnge_disp8(int disp8) {
    put8(0x7c);
    put8(disp8);
}

void x86asm_jnge_lbl8(struct x86asm_lbl8 *lbl) {
    struct lbl_jmp_pt pt;
    put8(0x7c);

    pt.offs = (int8_t*)outp;
    pt.rel_pos = outp + 1;

    put8(0); // temporary placeholder for the offset value
    x86asm_lbl8_push_jmp_pt(lbl, &pt);
}

void x86asm_jns_disp8(int disp8) {
    put8(0x79);
    put8(disp8);
}

void x86asm_jns_lbl8(struct x86asm_lbl8 *lbl) {
    struct lbl_jmp_pt pt;
    put8(0x79);

    pt.offs = (int8_t*)outp;
    pt.rel_pos = outp + 1;

    put8(0); // temporary placeholder for the offset value
    x86asm_lbl8_push_jmp_pt(lbl, &pt);
}

// movsx %<reg16>, %<reg32>
void x86asm_movsx_reg16_reg32(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm_2(0, 0x0f, 0xbf, 3, reg_dst, reg_src);
}

// addq $imm8, %<reg>
void x86asm_addq_imm8_reg(uint8_t imm8, unsigned reg) {
    emit_mod_reg_rm(REX_W, 0x83, 3, 0, reg);
    put8(imm8);
}

// movzxw (%<reg_src>), %<reg_dst>
void x86asm_movzxw_indreg_reg(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm_2(0, 0x0f, 0xb7, 0, reg_dst, reg_src);
}

// orl $<imm32>, %eax
void x86asm_orl_imm32_reg32(unsigned imm32, unsigned reg_no) {
    emit_mod_reg_rm(0, 0x81, 3, 1, reg_no);
    put32(imm32);
}

// xorl $<imm32>, %<reg_no>
void x86asm_xorl_imm32_reg32(unsigned imm32, unsigned reg_no) {
    emit_mod_reg_rm(0, 0x81, 3, 6, reg_no);
    put32(imm32);
}

// incl %<reg_no>
void x86asm_incl_reg32(unsigned reg_no) {
    emit_mod_reg_rm(0, 0xff, 3, 0, reg_no);
}

// shll $<imm8>, %reg_no
void x86asm_shll_imm8_reg32(unsigned imm8, unsigned reg_no) {
    emit_mod_reg_rm(0, 0xc1, 3, 4, reg_no);
    put8(imm8);
}

// sarl $<imm8>, %reg_no
void x86asm_sarl_imm8_reg32(unsigned imm8, unsigned reg_no) {
    emit_mod_reg_rm(0, 0xc1, 3, 7, reg_no);
    put8(imm8);
}

// shrl $<imm8>, %reg_no
void x86asm_shrl_imm8_reg32(unsigned imm8, unsigned reg_no) {
    emit_mod_reg_rm(0, 0xc1, 3, 5, reg_no);
    put8(imm8);
}

// shrl %cl, reg_no
void x86asm_shrl_cl_reg32(unsigned reg_no) {
    emit_mod_reg_rm(0, 0xd3, 3, 5, reg_no);
}

// sarl %cl, reg_no
void x86asm_sarl_cl_reg32(unsigned reg_no) {
    emit_mod_reg_rm(0, 0xd3, 3, 7, reg_no);
}

// shll %cl, reg_no
void x86asm_shll_cl_reg32(unsigned reg_no) {
    emit_mod_reg_rm(0, 0xd3, 3, 4, reg_no);
}

void x86asm_lbl8_init(struct x86asm_lbl8 *lbl) {
    memset(lbl, 0, sizeof(*lbl));
}

void x86asm_lbl8_cleanup(struct x86asm_lbl8 *lbl) {
    memset(lbl, 0, sizeof(*lbl));
}

// This will define the label to point to outp.
void x86asm_lbl8_define(struct x86asm_lbl8 *lbl) {
    lbl->ptr = outp;

    unsigned jmp_pt_idx;
    for (jmp_pt_idx = 0; jmp_pt_idx < lbl->n_jump_points; jmp_pt_idx++) {
        struct lbl_jmp_pt *pt = lbl->jump_points + jmp_pt_idx;
        ptrdiff_t offs = lbl->ptr - pt->rel_pos;
        if (offs > INT8_MAX)
            RAISE_ERROR(ERROR_TOO_BIG);
        if (offs < INT8_MIN)
            RAISE_ERROR(ERROR_TOO_SMALL);
        *pt->offs = offs;
    }
}

void x86asm_lbl8_push_jmp_pt(struct x86asm_lbl8 *lbl,
                             struct lbl_jmp_pt const *jmp_pt) {
    if (lbl->ptr) {
        // label has already been defined
        ptrdiff_t offs = lbl->ptr - jmp_pt->rel_pos;
        if (offs > INT8_MAX)
            RAISE_ERROR(ERROR_TOO_BIG);
        if (offs < INT8_MIN)
            RAISE_ERROR(ERROR_TOO_SMALL);
        *jmp_pt->offs = offs;
    } else {
        // save this jump point for when the label gets defined later
        if (lbl->n_jump_points >= MAX_LABEL_JUMPS)
            RAISE_ERROR(ERROR_OVERFLOW);
        lbl->jump_points[lbl->n_jump_points++] = *jmp_pt;
    }
}

void x86asm_mull_reg32(unsigned reg_no) {
    emit_mod_reg_rm(0, 0xf7, 3, 4, reg_no);
}

void x86asm_testl_reg32_reg32(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(0, 0x85, 3, reg_src, reg_dst);
}

void x86asm_testq_reg64_reg64(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm(REX_W, 0x85, 3, reg_src, reg_dst);
}

void x86asm_testl_imm32_reg32(uint32_t imm32, unsigned reg_no) {
    emit_mod_reg_rm(0, 0xf7, 3, 0, reg_no);
    put32(imm32);
}

// conditional-move if not-equal (ZF=0)
void x86asm_cmovnel_reg32_reg32(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm_2(0, 0x0f, 0x45, 3, reg_dst, reg_src);
}

// conditional-move if equal (ZF=1)
void x86asm_cmovel_reg32_reg32(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm_2(0, 0x0f, 0x44, 3, reg_dst, reg_src);
}

// conditional-move if greater (unsigned) (CF=0, ZF=0)
void x86asm_cmoval_reg32_reg32(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm_2(0, 0x0f, 0x47, 3, reg_dst, reg_src);
}

// conditional-move if greater (signed) (CF=0, ZF=0)
void x86asm_cmovgl_reg32_reg32(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm_2(0, 0x0f, 0x4f, 3, reg_dst, reg_src);
}

// conditional-move if greater-or-equal (unsigned)
void x86asm_cmovael_reg32_reg32(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm_2(0, 0x0f, 0x43, 3, reg_dst, reg_src);
}

// conditional-move if greater-or-equal (signed)
void x86asm_cmovgel_reg32_reg32(unsigned reg_src, unsigned reg_dst) {
    emit_mod_reg_rm_2(0, 0x0f, 0x4d, 3, reg_dst, reg_src);
}

void x86asm_setnzl_reg32(unsigned reg_no) {
    emit_mod_reg_rm_2(0, 0x0f, 0x95, 3, 0, reg_no);
}

void x86asm_negl_reg32(unsigned reg_no) {
    emit_mod_reg_rm(0, 0xf7, 3, 3, reg_no);
}
