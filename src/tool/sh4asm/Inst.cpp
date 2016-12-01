/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016 snickerbockers
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

#include "Inst.hpp"

PtrnList get_patterns() {
    PtrnList list;

    /***************************************************************************
     **
     ** operators which take no arguments
     **
     **************************************************************************/
    // DIVOU
    // 0000000000011001
    list.push_back(PtrnPtr(new NoArgOperator<INST_PTRN(divou), 0x0019>));

    // RTS
    // 0000000000001011
    list.push_back(PtrnPtr(new NoArgOperator<INST_PTRN(rts), 0x000b>));

    // CLRMAC
    // 0000000000101000
    list.push_back(PtrnPtr(new NoArgOperator<INST_PTRN(clrmac), 0x0028>));

    // CLRS
    // 0000000001001000
    list.push_back(PtrnPtr(new NoArgOperator<INST_PTRN(clrs), 0x0048>));

    // CLRT
    // 0000000000001000
    list.push_back(PtrnPtr(new NoArgOperator<INST_PTRN(clrt), 0x0008>));

    // LDTLB
    // 0000000000111000
    list.push_back(PtrnPtr(new NoArgOperator<INST_PTRN(ldtlb), 0x0038>));

    // NOP
    // 0000000000001001
    list.push_back(PtrnPtr(new NoArgOperator<INST_PTRN(nop), 0x0009>));

    // RTE
    // 0000000000101011
    list.push_back(PtrnPtr(new NoArgOperator<INST_PTRN(rte), 0x002b>));

    // SETS
    // 0000000001011000
    list.push_back(PtrnPtr(new NoArgOperator<INST_PTRN(sets), 0x0058>));

    // SETT
    // 0000000000011000
    list.push_back(PtrnPtr(new NoArgOperator<INST_PTRN(sett), 0x0018>));

    // SLEEP
    // 0000000000011011
    list.push_back(PtrnPtr(new NoArgOperator<INST_PTRN(sleep), 0x001b>));

    // FRCHG
    // 1111101111111101
    list.push_back(PtrnPtr(new NoArgOperator<INST_PTRN(frchg), 0xfbfd>));

    // FSCHG
    // 1111001111111101
    list.push_back(PtrnPtr(new NoArgOperator<INST_PTRN(fschg), 0xf3fd>));

    /***************************************************************************
     **
     ** operators which take 1 argument (general-purpose register):
     **
     **************************************************************************/

    // MOVT Rn
    // 0000nnnn00101001
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(movt), Ptrn_GenReg,
                           0x0029, 0xf0ff, 8>));

    // CMP/PZ Rn
    // 0100nnnn00010001
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(cmppz), Ptrn_GenReg,
                           0x4011, 0xf0ff, 8>));

    // CMP/PL Rn
    // 0100nnnn00010101
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(cmppl), Ptrn_GenReg,
                           0x4015, 0xf0ff, 8>));

    // DT Rn
    // 0100nnnn00010000
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(dt), Ptrn_GenReg,
                           0x4010, 0xf0ff, 8>));

    // ROTL Rn
    // 0100nnnn00000100
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(rotl), Ptrn_GenReg,
                           0x4004, 0xf0ff, 8>));

    // ROTR Rn
    // 0100nnnn00000101
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(rotr), Ptrn_GenReg,
                           0x4005, 0xf0ff, 8>));

    // ROTCL Rn
    // 0100nnnn00100100
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(rotcl), Ptrn_GenReg,
                           0x4024, 0xf0ff, 8>));

    // ROTCR Rn
    // 0100nnnn00100101
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(rotcr), Ptrn_GenReg,
                           0x4025, 0xf0ff, 8>));

    // SHAL Rn
    // 0100nnnn00200000
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(shal), Ptrn_GenReg,
                           0x4020, 0xf0ff, 8>));

    // SHAR Rn
    // 0100nnnn00100001
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(shar), Ptrn_GenReg,
                           0x4021, 0xf0ff, 8>));

    // SHLL Rn
    // 0100nnnn00000000
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(shll), Ptrn_GenReg,
                           0x4000, 0xf0ff, 8>));

    // SHLR Rn
    // 0100nnnn00000001
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(shlr), Ptrn_GenReg,
                           0x4001, 0xf0ff, 8>));

    // SHLL2 Rn
    // 0100nnnn00001000
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(shll2), Ptrn_GenReg,
                           0x4008, 0xf0ff, 8>));

    // SHLR2 Rn
    // 0100nnnn00001001
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(shlr2), Ptrn_GenReg,
                           0x4009, 0xf0ff, 8>));

    // SHLL8 Rn
    // 0100nnnn00011000
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(shll8), Ptrn_GenReg,
                           0x4018, 0xf0ff, 8>));

    // SHLR8 Rn
    // 0100nnnn00011001
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(shlr8), Ptrn_GenReg,
                           0x4019, 0xf0ff, 8>));

    // SHLL16 Rn
    // 0100nnnn00101000
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(shll16), Ptrn_GenReg,
                           0x4028, 0xf0ff, 8>));

    // SHLR16 Rn
    // 0100nnnn00101001
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(shlr16), Ptrn_GenReg,
                           0x4029, 0xf0ff, 8>));

    // BRAF Rn
    // 0000nnnn00100011
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(braf), Ptrn_GenReg,
                           0x0023, 0xf0ff, 8>));

    // BSRF Rn
    // 0000nnnn00000011
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(bsrf), Ptrn_GenReg,
                           0x0003, 0xf0ff, 8>));


    /***************************************************************************
     **
     ** opcode that only takes an immediate value as input
     **
     **************************************************************************/
    // CMP/EQ #imm, R0
    // 10001000iiiiiiii
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(cmpeq), Ptrn_immed<0xff>,
                           Ptrn_R0Reg, 0x8800, 0xff00, 0, 0>));

    // AND.B #imm, @(R0, GBR)
    // 11001101iiiiiiii
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(andb), Ptrn_immed<0xff>,
                           Ptrn_BinaryInd<Ptrn_R0Reg, Ptrn_GbrReg, 0, 0, 0>,
			   0xcd00, 0xff00, 0, 0>));

    // AND #imm, R0
    // 11001001iiiiiiii
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(and), Ptrn_immed<0xff>,
                           Ptrn_R0Reg, 0xc900, 0xff00, 0, 0>));

    // OR.B #imm, @(R0, GBR)
    // 11001111iiiiiiii
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(orb), Ptrn_immed<0xff>,
                           Ptrn_BinaryInd<Ptrn_R0Reg, Ptrn_GbrReg, 0, 0, 0>,
			   0xcf00, 0xff00, 0>));

    // OR #imm, R0
    // 11001011iiiiiiii
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(or), Ptrn_immed<0xff>,
			   Ptrn_R0Reg, 0xcb00, 0xff00, 0>));

    // TST #imm, R0
    // 11001000iiiiiiii
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(tst), Ptrn_immed<0xff>,
                           Ptrn_R0Reg, 0xc800, 0xff00, 0, 0>));

    // TST.B #imm, @(R0, GBR)
    // 11001100iiiiiiii
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(tstb), Ptrn_immed<0xff>,
                           Ptrn_BinaryInd<Ptrn_R0Reg, Ptrn_GbrReg, 0, 0, 0>,
			   0xcc00, 0xff00, 0, 0>));

    // XOR #imm, R0
    // 11001010iiiiiiii
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(xor), Ptrn_immed<0xff>,
                           Ptrn_R0Reg, 0xca00, 0xff00, 0, 0>));

    // XOR.B #imm, @(R0, GBR)
    // 11001110iiiiiiii
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(xorb), Ptrn_immed<0xff>,
                           Ptrn_BinaryInd<Ptrn_R0Reg, Ptrn_GbrReg, 0, 0, 0>,
			   0xce00, 0xff00, 0, 0>));

    // BF label
    // 10001011dddddddd
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(bf), Ptrn_Disp<0xff>,
                           0x8b00, 0xff00, 0>));

    // BF/S label
    // 10001111dddddddd
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(bfs), Ptrn_Disp<0xff>,
                           0x8f00, 0xff00, 0>));

    // BT label
    // 10001001dddddddd
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(bt), Ptrn_Disp<0xff>,
                           0x8900, 0xff00, 0>));

    // BT/S label
    // 10001101dddddddd
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(bts), Ptrn_Disp<0xff>,
                           0x8d00, 0xff00, 0>));

    // BRA label
    // 1010dddddddddddd
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(bra), Ptrn_Disp<0xfff>,
                           0xa000, 0xf000, 0>));

    // BSR label
    // 1011dddddddddddd
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(bsr), Ptrn_Disp<0xfff>,
                           0xb000, 0xf000, 0>));

    // TRAPA #immed
    // 11000011iiiiiiii
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(trapa), Ptrn_immed<0x0ff>,
                           0xc300, 0xff00, 0>));

    /***************************************************************************
     **
     ** opcode that takes a general-purpose register containing the
     ** address of its sole argument.
     **
     **************************************************************************/
    // TAS.B @Rn
    // 0100nnnn00011011
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(tasb), Ptrn_Ind<Ptrn_GenReg>,
                           0x401b, 0xf0ff, 8>));

    // OCBI @Rn
    // 0000nnnn10100011
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(ocbi), Ptrn_Ind<Ptrn_GenReg>,
                           0x00a3, 0xf0ff, 8>));

    // OCBP @Rn
    // 0000nnnn10100011
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(ocbp), Ptrn_Ind<Ptrn_GenReg>,
                           0x00b3, 0xf0ff, 8>));

    // PREF @Rn
    // 0000nnnn10000011
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(pref), Ptrn_Ind<Ptrn_GenReg>,
                           0x0083, 0xf0ff, 8>));

    // JMP @Rn
    // 0100nnnn00101011
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(jmp),
                           Ptrn_Ind<Ptrn_GenReg>, 0x402b, 0xf0ff, 8>));

    // JSR @Rn
    // 0100nnnn00001011
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(jsr),
                           Ptrn_Ind<Ptrn_GenReg>, 0x400b, 0xf0ff, 8>));

    /***************************************************************************
     **
     ** LDC/STC instructions
     **
     **************************************************************************/
    // LDC Rm, SR
    // 0100mmmm00001110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldc), Ptrn_GenReg,
                           Ptrn_SrReg, 0x400e, 0xf0ff, 8, 0>));

    // LDC Rm, GBR
    // 0100mmmm00011110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldc), Ptrn_GenReg,
                           Ptrn_GbrReg, 0x401e, 0xf0ff, 8, 0>));

    // LDC Rm, VBR
    // 0100mmmm00101110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldc), Ptrn_GenReg,
                           Ptrn_VbrReg, 0x402e, 0xf0ff, 8, 0>));

    // LDC Rm, SSR
    // 0100mmmm00111110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldc), Ptrn_GenReg,
                           Ptrn_SsrReg, 0x403e, 0xf0ff, 8, 0>));

    // LDC Rm, SPC
    // 0100mmmm01001110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldc), Ptrn_GenReg,
                           Ptrn_SpcReg, 0x404e, 0xf0ff, 8, 0>));

    // LDC Rm, DBR
    // 0100mmmm11111010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldc), Ptrn_GenReg,
                           Ptrn_DbrReg, 0x40fa, 0xf0ff, 8, 0>));

    // STC SR, Rn
    // 0000nnnn00000010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stc), Ptrn_SrReg,
                           Ptrn_GenReg, 0x0002, 0xf0ff, 0, 8>));

    // STC GBR, Rn
    // 0000nnnn00010010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stc), Ptrn_GbrReg,
                           Ptrn_GenReg, 0x0012, 0xf0ff, 0, 8>));

    // STC VBR, Rn
    // 0000nnnn00100010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stc), Ptrn_VbrReg,
                           Ptrn_GenReg, 0x0022, 0xf0ff, 0, 8>));

    // STC SSR, Rn
    // 0000nnnn00110010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stc), Ptrn_SsrReg,
                           Ptrn_GenReg, 0x0032, 0xf0ff, 0, 8>));

    // STC SPC, Rn
    // 0000nnnn01000010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stc), Ptrn_SpcReg,
                           Ptrn_GenReg, 0x0042, 0xf0ff, 0, 8>));

    // STC SGR, Rn
    // 0000nnnn00111010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stc), Ptrn_SgrReg,
                           Ptrn_GenReg, 0x003a, 0xf0ff, 0, 8>));

    // STC DBR, Rn
    // 0000nnnn11111010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stc), Ptrn_DbrReg,
                           Ptrn_GenReg, 0x00fa, 0xf0ff, 0, 8>));

    // LDC.L @Rm+, SR
    // 0100mmmm00000111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldcl),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_SrReg,
                           0x4007, 0xf0ff, 8, 0>));

    // LDC.L @Rm+, GBR
    // 0100mmmm00010111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldcl),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_GbrReg, 0x4017,
                           0xf0ff, 8, 0>));

    // LDC.L @Rm+, VBR
    // 0100mmmm00100111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldcl),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_VbrReg,
                           0x4027, 0xf0ff, 8, 0>));

    // LDC.L @Rm+, SSR
    // 0100mmmm00110111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldcl),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_SsrReg,
                           0x4037, 0xf0ff, 8, 0>));

    // LDC.L @Rm+, SPC
    // 0100mmmm01000111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldcl),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_SpcReg,
                           0x4047, 0xf0ff, 8, 0>));

    // LDC.L @Rm+, DBR
    // 0100mmmm11110110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldcl),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_DbrReg,
                           0x40f6, 0xf0ff, 8, 0>));

    // STC.L SR, @-Rn
    // 0100nnnn00000011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stcl),
                           Ptrn_SrReg, Ptrn_DecInd<Ptrn_GenReg>,
                           0x4003, 0xf0ff, 0, 8>));

    // STC.L GBR, @-Rn
    // 0100nnnn00010011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stcl),
                           Ptrn_GbrReg, Ptrn_DecInd<Ptrn_GenReg>,
                           0x4013, 0xf0ff, 0, 8>));

    // STC.L VBR, @-Rn
    // 0100nnnn00100011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stcl),
                           Ptrn_VbrReg, Ptrn_DecInd<Ptrn_GenReg>,
                           0x4023, 0xf0ff, 0, 8>));

    // STC.L SSR, @-Rn
    // 0100nnnn00110011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stcl),
                           Ptrn_SsrReg, Ptrn_DecInd<Ptrn_GenReg>,
                           0x4033, 0xf0ff, 0, 8>));

    // STC.L SPC, @-Rn
    // 0100nnnn01000011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stcl),
                           Ptrn_SpcReg, Ptrn_DecInd<Ptrn_GenReg>,
                           0x4043, 0xf0ff, 0, 8>));

    // STC.L SGR, @-Rn
    // 0100nnnn00110010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stcl),
                           Ptrn_SgrReg, Ptrn_DecInd<Ptrn_GenReg>,
                           0x4032, 0xf0ff, 0, 8>));

    // STC.L DBR, @-Rn
    // 0100nnnn11110010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stcl),
                           Ptrn_DbrReg, Ptrn_DecInd<Ptrn_GenReg>,
                           0x40f2, 0xf0ff, 0, 8>));

    /***************************************************************************
     **
     ** Opcodes that take an immediate as input and a general-purpose
     ** register as output
     **
     **************************************************************************/
    // MOV #imm, Rn
    // 1110nnnniiiiiiii
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(mov), Ptrn_immed<0x00ff>,
                           Ptrn_GenReg, 0xe000, 0xf000, 0, 8>));

    // ADD #imm, Rn
    // 0111nnnniiiiiiii
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(add), Ptrn_immed<0x00ff>,
                           Ptrn_GenReg, 0x7000, 0xf000, 0, 8>));

    /***************************************************************************
     **
     ** Opcodes that add an immediate value (scaled by either 2 or 4) to the
     ** PC and then use *that* address as the source to move a value into a
     ** given general-purpose register (the destination).
     **
     **************************************************************************/
    // MOV.W @(disp, PC), Rn
    // 1001nnnndddddddd
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movw), Ptrn_BinaryInd<
                           Ptrn_Disp<0x00ff>, Ptrn_PcReg, 0x0000, 0, 0>,
                           Ptrn_GenReg, 0x9000, 0xf000, 0, 8>));

    // MOV.L @(disp, PC), Rn
    // 1101nnnndddddddd
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movl), Ptrn_BinaryInd<
                           Ptrn_Disp<0x00ff>, Ptrn_PcReg, 0x0000, 0, 0>,
                           Ptrn_GenReg, 0xd000, 0xf000, 0, 8>));

    /***************************************************************************
     **
     ** Opcodes that take a general-purpose register as a source and a
     ** general-purpose register as a destination
     **
     **************************************************************************/
    // MOV Rm, Rn
    // 0110nnnnmmmm0011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(mov), Ptrn_GenReg,
                           Ptrn_GenReg, 0x6003, 0xf00f, 4, 8>));

    // SWAP.B Rm, Rn
    // 0110nnnnmmmm1000
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(swapb), Ptrn_GenReg,
                           Ptrn_GenReg, 0x6008, 0xf00f, 4, 8>));

    // SWAP.W Rm, Rn
    // 0110nnnnmmmm1001
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(swapw), Ptrn_GenReg,
                           Ptrn_GenReg, 0x6009, 0xf00f, 4, 8>));

    // XTRCT Rm, Rn
    // 0110nnnnmmmm1101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(xtrct), Ptrn_GenReg,
                           Ptrn_GenReg, 0x200d, 0xf00f, 4, 8>));

    // ADD Rm, Rn
    // 0111nnnnmmmm1100
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(add), Ptrn_GenReg,
                           Ptrn_GenReg, 0x300c, 0xf00f, 4, 8>));

    // ADDC Rm, Rn
    // 0111nnnnmmmm1110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(addc), Ptrn_GenReg,
                           Ptrn_GenReg, 0x300e, 0xf00f, 4, 8>));

    // ADDV Rm, Rn
    // 0111nnnnmmmm1111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(addv), Ptrn_GenReg,
                           Ptrn_GenReg, 0x300f, 0xf00f, 4, 8>));

    // CMP/EQ Rm, Rn
    // 0011nnnnmmmm0000
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(cmpeq), Ptrn_GenReg,
                           Ptrn_GenReg, 0x3000, 0xf00f, 4, 8>));

    // CMP/HS Rm, Rn
    // 0011nnnnmmmm0010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(cmphs), Ptrn_GenReg,
                           Ptrn_GenReg, 0x3002, 0xf00f, 4, 8>));

    // CMP/GE Rm, Rn
    // 0011nnnnmmmm0011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(cmpge), Ptrn_GenReg,
                           Ptrn_GenReg, 0x3003, 0xf00f, 4, 8>));

    // CMP/HI Rm, Rn
    // 0011nnnnmmmm0110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(cmphi), Ptrn_GenReg,
                           Ptrn_GenReg, 0x3006, 0xf00f, 4, 8>));

    // CMP/GT Rm, Rn
    // 0011nnnnmmmm0111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(cmpgt), Ptrn_GenReg,
                           Ptrn_GenReg, 0x3007, 0xf00f, 4, 8>));

    // CMP/STR Rm, Rn
    // 0010nnnnmmmm1100
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(cmpstr), Ptrn_GenReg,
                           Ptrn_GenReg, 0x200c, 0xf00f, 4, 8>));

    // DIV1 Rm, Rn
    // 0011nnnnmmmm0100
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(div1), Ptrn_GenReg,
                           Ptrn_GenReg, 0x3004, 0xf00f, 4, 8>));

    // DIV0S Rm, Rn
    // 0010nnnnmmmm0111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(div0s), Ptrn_GenReg,
                           Ptrn_GenReg, 0x2007, 0xf00f, 4, 8>));

    // DMULS.L Rm, Rn
    // 0011nnnnmmmm1101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(dmulsl), Ptrn_GenReg,
                           Ptrn_GenReg, 0x300d, 0xf00f, 4, 8>));

    // DMULU.L Rm, Rn
    // 0011nnnnmmmm0101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(dmulul), Ptrn_GenReg,
                           Ptrn_GenReg, 0x3005, 0xf00f, 4, 8>));

    // EXTS.B Rm, Rn
    // 0110nnnnmmmm1110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(extsb), Ptrn_GenReg,
                           Ptrn_GenReg, 0x600e, 0xf00f, 4, 8>));

    // EXTS.W Rm, Rn
    // 0110nnnnmmmm1111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(extsw), Ptrn_GenReg,
                           Ptrn_GenReg, 0x600f, 0xf00f, 4, 8>));

    // EXTU.B Rm, Rn
    // 0110nnnnmmmm1100
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(extub), Ptrn_GenReg,
                           Ptrn_GenReg, 0x600c, 0xf00f, 4, 8>));

    // EXTU.W Rm, Rn
    // 0110nnnnmmmm1101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(extuw), Ptrn_GenReg,
                           Ptrn_GenReg, 0x600d, 0xf00f, 4, 8>));

    // MUL.L Rm, Rn
    // 0000nnnnmmmm0111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(mull), Ptrn_GenReg,
                           Ptrn_GenReg, 0x0007, 0xf00f, 4, 8>));

    // MULS.W Rm, Rn
    // 0010nnnnmmmm1111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(mulsw), Ptrn_GenReg,
                           Ptrn_GenReg, 0x200f, 0xf00f, 4, 8>));

    // MULU.W Rm, Rn
    // 0010nnnnmmmm1110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(muluw), Ptrn_GenReg,
                           Ptrn_GenReg, 0x200e, 0xf00f, 4, 8>));

    // NEG Rm, Rn
    // 0110nnnnmmmm1011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(neg), Ptrn_GenReg,
                           Ptrn_GenReg, 0x600b, 0xf00f, 4, 8>));

    // NEGC Rm, Rn
    // 0110nnnnmmmm1010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(negc), Ptrn_GenReg,
                           Ptrn_GenReg, 0x600a, 0xf00f, 4, 8>));

    // SUB Rm, Rn
    // 0011nnnnmmmm1000
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(sub), Ptrn_GenReg,
                           Ptrn_GenReg, 0x3008, 0xf00f, 4, 8>));

    // SUBC Rm, Rn
    // 0011nnnnmmmm1010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(subc), Ptrn_GenReg,
                           Ptrn_GenReg, 0x300a, 0xf00f, 4, 8>));

    // SUBV Rm, Rn
    // 0011nnnnmmmm1011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(subv), Ptrn_GenReg,
                           Ptrn_GenReg, 0x300b, 0xf00f, 4, 8>));

    // AND Rm, Rn
    // 0010nnnnmmmm1001
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(and), Ptrn_GenReg,
                           Ptrn_GenReg, 0x2009, 0xf00f, 4, 8>));

    // NOT Rm, Rn
    // 0110nnnnmmmm0111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(not), Ptrn_GenReg,
                           Ptrn_GenReg, 0x6007, 0xf00f, 4, 8>));

    // OR Rm, Rn
    // 0010nnnnmmmm1011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(or), Ptrn_GenReg,
                           Ptrn_GenReg, 0x200b, 0xf00f, 4, 8>));

    // TST Rm, Rn
    // 0010nnnnmmmm1000
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(tst), Ptrn_GenReg,
                           Ptrn_GenReg, 0x2008, 0xf00f, 4, 8>));

    // XOR Rm, Rn
    // 0010nnnnmmmm1010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(xor), Ptrn_GenReg,
                           Ptrn_GenReg, 0x200a, 0xf00f, 4, 8>));

    // SHAD Rm, Rn
    // 0100nnnnmmmm1100
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(shad), Ptrn_GenReg,
                           Ptrn_GenReg, 0x400c, 0xf00f, 4, 8>));

    // SHLD Rm, Rn
    // 0100nnnnmmmm1101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(shld), Ptrn_GenReg,
                           Ptrn_GenReg, 0x400c, 0xf00f, 4, 8>));

    /***************************************************************************
     **
     ** Opcodes that use bank-switched registers as the source or destination
     **
     **************************************************************************/
    // LDC Rm, Rn_BANK
    // 0100mmmm1nnn1110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldc), Ptrn_GenReg,
                           Ptrn_BankReg, 0x408e, 0xf08f, 8, 4>));

    // LDC.L @Rm+, Rn_BANK
    // 0100mmmm1nnn0111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldcl),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_BankReg,
                           0x4087, 0xf08f, 8, 4>));

    // STC Rm_BANK, Rn
    // 0000nnnn1mmm0010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stc), Ptrn_BankReg,
                           Ptrn_GenReg, 0x0082, 0xf08f, 4, 8>));

    // STC.L Rm_BANK, @-Rn
    // 0100nnnn1mmm0011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stcl), Ptrn_BankReg,
                           Ptrn_DecInd<Ptrn_GenReg>, 0x4083, 0xf08f, 4, 8>));

    /***************************************************************************
     **
     ** Some assorted LDS/STS instructions
     **
     **************************************************************************/
    // LDS Rm,MACH
    // 0100mmmm00001010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(lds),
                           Ptrn_GenReg, Ptrn_Mach, 0x400a, 0xf0ff, 8, 0>));

    // LDS Rm, MACL
    // 0100mmmm00011010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(lds),
                           Ptrn_GenReg, Ptrn_Macl, 0x401a, 0xf0ff, 8, 0>));

    // STS MACH, Rn
    // 0000nnnn00001010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(sts),
                           Ptrn_Mach, Ptrn_GenReg, 0x000a, 0xf0ff, 0, 8>));

    // STS MACL, Rn
    // 0000nnnn00011010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(sts),
                           Ptrn_Macl, Ptrn_GenReg, 0x001a, 0xf0ff, 0, 8>));

    // LDS Rm, PR
    // 0100mmmm00101010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(lds),
                           Ptrn_GenReg, Ptrn_PrReg, 0x402a, 0xf0ff, 8, 0>));

    // STS PR, Rn
    // 0000nnnn00101010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(sts),
                           Ptrn_PrReg, Ptrn_GenReg, 0x002a, 0xf0ff, 0, 8>));

    // LDS.L @Rm+, MACH
    // 0100mmmm00000110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldsl),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_Mach,
                           0x4006, 0xf0ff, 8, 0>));

    // LDS.L @Rm+, MACL
    // 0100mmmm00010110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldsl),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_Macl,
                           0x4016, 0xf0ff, 8, 0>));

    // STS.L MACH, @-Rn
    // 0100mmmm00000010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stsl),
                           Ptrn_Mach, Ptrn_DecInd<Ptrn_GenReg>,
                           0x4002, 0xf0ff, 0, 8>));

    // STS.L MACL, @-Rn
    // 0100mmmm00010010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stsl),
                           Ptrn_Macl, Ptrn_DecInd<Ptrn_GenReg>,
                           0x4012, 0xf0ff, 0, 8>));

    // LDS.L @Rm+, PR
    // 0100mmmm00100110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldsl),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_PrReg,
                           0x4026, 0xf0ff, 8, 0>));

    // STS.L PR, @-Rn
    // 0100nnnn00100010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stsl), Ptrn_PrReg,
                           Ptrn_DecInd<Ptrn_GenReg>, 0x4022, 0xf0ff, 0, 8>));

    /***************************************************************************
     **
     ** Opcodes that move a general-purpose register into the address pointed
     ** to by another general-purpose register
     **
     **************************************************************************/
    // MOV.B Rm, @Rn
    // 0010nnnnmmmm0000
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movb), Ptrn_GenReg,
                           Ptrn_Ind<Ptrn_GenReg>, 0x2000, 0xf00f, 4, 8>));

    // MOV.W Rm, @Rn
    // 0010nnnnmmmm0001
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movw), Ptrn_GenReg,
                           Ptrn_Ind<Ptrn_GenReg>, 0x2001, 0xf00f, 4, 8>));

    // MOV.L Rm, @Rn
    // 0010nnnnmmmm0010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movl), Ptrn_GenReg,
                           Ptrn_Ind<Ptrn_GenReg>, 0x2002, 0xf00f, 4, 8>));


    /***************************************************************************
     **
     ** Opcodes that move the contents of the address pointed to by a
     ** general-purpose register into a general-purpose register
     **
     **************************************************************************/
    // MOV.B @Rm, Rn
    // 0110nnnnmmmm0000
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movb), Ptrn_Ind<Ptrn_GenReg>,
                           Ptrn_GenReg, 0x6000, 0xf00f, 4, 8>));

    // MOV.W @Rm, Rn
    // 0110nnnnmmmm0001
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movw), Ptrn_Ind<Ptrn_GenReg>,
                           Ptrn_GenReg, 0x6001, 0xf00f, 4, 8>));

    // MOV.L @Rm, Rn
    // 0110nnnnmmmm0010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movl), Ptrn_Ind<Ptrn_GenReg>,
                           Ptrn_GenReg, 0x6002, 0xf00f, 4, 8>));

    /***************************************************************************
     **
     ** Opcodes that move the contents of a general-purpose register into the
     ** memory pointed to by another general purpose register after first
     ** decrementing the destination register
     **
     **************************************************************************/
    // MOV.B Rm, @-Rn
    // 0010nnnnmmmm0100
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movb), Ptrn_GenReg,
                           Ptrn_DecInd<Ptrn_GenReg>, 0x2004, 0xf00f, 4, 8>));

    // MOV.W Rm, @-Rn
    // 0010nnnnmmmm0101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movw), Ptrn_GenReg,
                           Ptrn_DecInd<Ptrn_GenReg>, 0x2005, 0xf00f, 4, 8>));

    // MOV.L Rm, @-Rn
    // 0010nnnnmmmm0110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movl), Ptrn_GenReg,
                           Ptrn_DecInd<Ptrn_GenReg>, 0x2006, 0xf00f, 4, 8>));

    /***************************************************************************
     **
     ** Opcodes that move the contents of the memory pointed to by the source
     ** register into the destination register and then increment the source
     ** register
     **
     **************************************************************************/
    // MOV.B @Rm+, Rn
    // 0110nnnnmmmm0100
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movb),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_GenReg,
                           0x6004, 0xf00f, 4, 8>));

    // MOV.W @Rm+, Rn
    // 0110nnnnmmmm0101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movw),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_GenReg,
                           0x6005, 0xf00f, 4, 8>));

    // MOV.L @Rm+, Rn
    // 0110nnnnmmmm0110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movl),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_GenReg,
                           0x6006, 0xf00f, 4, 8>));

    /***************************************************************************
     **
     ** Opcodes that multiply the contents of the memory pointed to by the
     ** source register into the second source register and add that to MAC.
     ** Then both source registers are incremented
     **
     **************************************************************************/
    // MAC.L @Rm+, @Rn+
    // 0000nnnnmmmm1111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(macl), Ptrn_IndInc<Ptrn_GenReg>,
                           Ptrn_IndInc<Ptrn_GenReg>, 0x000f, 0xf00f, 4, 8>));

    // MAC.W @Rm+, @Rn+
    // 0100nnnnmmmm1111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(macw),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_IndInc<Ptrn_GenReg>,
                           0x400f, 0xf00f, 4, 8>));

    /***************************************************************************
     **
     ** Opcodes that move R0 into @(source reg + displacement).
     **
     **************************************************************************/
    // MOV.B R0, @(disp, Rn)
    // 10000000nnnndddd
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movb), Ptrn_R0Reg,
                           Ptrn_BinaryInd<Ptrn_Disp<0xf>, Ptrn_GenReg, 0, 0, 4>,
                           0x8000, 0xff00, 0, 0>));

    // MOV.W R0, @(disp, Rn)
    // 10000001nnnndddd
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movw), Ptrn_R0Reg,
                           Ptrn_BinaryInd<Ptrn_Disp<0xf>, Ptrn_GenReg, 0, 0, 4>,
                           0x8100, 0xff00, 0, 0>));

    /***************************************************************************
     **
     ** Opcode that moves a general-purpose register into
     ** @(source reg + displacement).
     **
     **************************************************************************/
    // MOV.L Rm, @(disp, Rn)
    // 0001nnnnmmmmdddd
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movl), Ptrn_GenReg,
                           Ptrn_BinaryInd<Ptrn_Disp<0xf>, Ptrn_GenReg, 0, 0, 8>,
                           0x1000, 0xf000, 4, 0>));

    /***************************************************************************
     **
     ** Opcodes that move @(source reg + displacement) into R0
     **
     **************************************************************************/
    // MOV.B @(disp, Rm), R0
    // 10000100mmmmdddd
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movb),
                           Ptrn_BinaryInd<Ptrn_Disp<0xf>, Ptrn_GenReg, 0, 0, 4>,
                           Ptrn_R0Reg, 0x8400, 0xff00, 0, 0>));

    // MOV.W @(disp, Rm), R0
    // 10000101mmmmdddd
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movw),
                           Ptrn_BinaryInd<Ptrn_Disp<0xf>, Ptrn_GenReg, 0, 0, 4>,
                           Ptrn_R0Reg, 0x8500, 0xff00, 0, 0>));

    /***************************************************************************
     **
     ** Opcode that moves @(source reg + displacement) into a general-purpose
     ** register.
     **
     **************************************************************************/
    // MOV.L @(disp, Rm), Rn
    // 0101nnnnmmmmdddd
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movl),
                           Ptrn_BinaryInd<Ptrn_Disp<0xf>, Ptrn_GenReg, 0, 0, 4>,
                           Ptrn_GenReg, 0x5000, 0xf000, 0, 8>));

    /***************************************************************************
     **
     ** Opcodes that move a general purpose register into
     ** @(R0 + another general-purpose register)
     **
     **************************************************************************/
    // MOV.B Rm, @(R0, Rn)
    // 0000nnnnmmmm0100
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movb), Ptrn_GenReg,
                           Ptrn_BinaryInd<Ptrn_R0Reg, Ptrn_GenReg, 0, 0, 8>,
                           0x0004, 0xf00f, 4, 0>));

    // MOV.W Rm, @(R0, Rn)
    // 0000nnnnmmmm0101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movw), Ptrn_GenReg,
                           Ptrn_BinaryInd<Ptrn_R0Reg, Ptrn_GenReg, 0, 0, 8>,
                           0x0005, 0xf00f, 4, 0>));

    // MOV.L Rm, @(R0, Rn)
    // 0000nnnnmmmm0110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movl), Ptrn_GenReg,
                           Ptrn_BinaryInd<Ptrn_R0Reg, Ptrn_GenReg, 0, 0, 8>,
                           0x0006, 0xf00f, 4, 0>));

    /***************************************************************************
     **
     ** Opcodes that move @(R0 + general purpose register) into
     ** another general purpose register
     **
     **************************************************************************/
    // MOV.B @(R0, Rm), Rn
    // 0000nnnnmmmm1100
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movb),
                           Ptrn_BinaryInd<Ptrn_R0Reg, Ptrn_GenReg, 0, 0, 4>,
                           Ptrn_GenReg, 0x000c, 0xf00f, 0, 8>));

    // MOV.W @(R0, Rm), Rn
    // 0000nnnnmmmm1101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movw),
                           Ptrn_BinaryInd<Ptrn_R0Reg, Ptrn_GenReg, 0, 0, 4>,
                           Ptrn_GenReg, 0x000d, 0xf00f, 0, 8>));

    // MOV.L @(R0, Rm), Rn
    // 0000nnnnmmmm1110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movl),
                           Ptrn_BinaryInd<Ptrn_R0Reg, Ptrn_GenReg, 0, 0, 4>,
                           Ptrn_GenReg, 0x000e, 0xf00f, 0, 8>));

    /***************************************************************************
     **
     ** Opcodes that move R0 into @(disp + GBR)
     **
     **************************************************************************/
    // MOV.B R0, @(disp, GBR)
    // 11000000dddddddd
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movb), Ptrn_R0Reg,
                           Ptrn_BinaryInd<Ptrn_Disp<0xff>, Ptrn_GbrReg, 0, 0, 0>,
                           0xc000, 0xff00, 0, 0>));

    // MOV.W R0, @(disp, GBR)
    // 11000001dddddddd
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movw), Ptrn_R0Reg,
                           Ptrn_BinaryInd<Ptrn_Disp<0xff>, Ptrn_GbrReg, 0, 0, 0>,
                           0xc100, 0xff00, 0, 0>));

    // MOV.L R0, @(disp, GBR)
    // 11000010dddddddd
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movl), Ptrn_R0Reg,
                           Ptrn_BinaryInd<Ptrn_Disp<0xff>, Ptrn_GbrReg, 0, 0, 0>,
                           0xc200, 0xff00, 0, 0>));

    /***************************************************************************
     **
     ** Opcodes that move @(disp + GBR) into R0
     **
     **************************************************************************/
    // MOV.B @(disp, GBR), R0
    // 11000100dddddddd
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movb),
                           Ptrn_BinaryInd<Ptrn_Disp<0xff>, Ptrn_GbrReg, 0, 0, 0>,
                           Ptrn_R0Reg, 0xc400, 0xff00, 0, 0>));

    // MOV.W @(disp, GBR), R0
    // 11000101dddddddd
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movw),
                           Ptrn_BinaryInd<Ptrn_Disp<0xff>, Ptrn_GbrReg, 0, 0, 0>,
                           Ptrn_R0Reg, 0xc500, 0xff00, 0, 0>));

    // MOV.L @(disp, GBR), R0
    // 11000110dddddddd
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movl),
                           Ptrn_BinaryInd<Ptrn_Disp<0xff>, Ptrn_GbrReg, 0, 0, 0>,
                           Ptrn_R0Reg, 0xc600, 0xff00, 0, 0>));

    /***************************************************************************
     **
     ** Opcode that does a 4-byte move from @(disp + PC + 1) into R0
     **
     **************************************************************************/
    // MOVA @(disp, PC), R0
    // 11000111dddddddd
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(mova),
                           Ptrn_BinaryInd<Ptrn_Disp<0xff>, Ptrn_PcReg, 0, 0, 0>,
                           Ptrn_R0Reg, 0xc700, 0xff00, 0, 0>));

    /***************************************************************************
     **
     ** Opcode that moves R0 into the address pointed to by a general-purpose
     ** register.  Apparently it doesn't fetch a cache block; IDK if that's
     ** supposed to mean it operates in write-through mode or if it skips the
     ** cache entirely or if it means something completely different from
     ** either hypothesis.
     **
     **************************************************************************/
    // MOVCA.L R0, @Rn
    // 0000nnnn11000011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(movcal), Ptrn_R0Reg,
                           Ptrn_Ind<Ptrn_GenReg>, 0x00c3, 0xf0ff, 0, 8>));

    /***************************************************************************
     **
     ** Floating-point opcodes
     **
     **************************************************************************/
    // FLDI0 FRn - load 0.0 into Frn
    // 1111nnnn10001101
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(fldi0), Ptrn_FrReg,
                           0xf08d, 0xf0ff, 8>));

    // FLDI1 Frn - load 1.0 into Frn
    // 1111nnnn10011101
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(fldi1), Ptrn_FrReg,
                           0xf09d, 0xf0ff, 8>));

    // FMOV FRm, FRn
    //1111nnnnmmmm1100
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov),
                           Ptrn_FrReg, Ptrn_FrReg, 0xf00c, 0xf00f, 4, 8>));

    // FMOV.S @Rm, FRn
    // 1111nnnnmmmm1000
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmovs),
                           Ptrn_Ind<Ptrn_GenReg>, Ptrn_FrReg,
                           0xf008, 0xf00f, 4, 8>));

    // FMOV.S @(R0,Rm), FRn
    // 1111nnnnmmmm0110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmovs),
                           Ptrn_BinaryInd<Ptrn_R0Reg, Ptrn_GenReg, 0, 0, 4>,
                           Ptrn_FrReg, 0xf006, 0xf00f, 0, 8>));

    // FMOV.S @Rm+, FRn
    // 1111nnnnmmmm1001
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmovs),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_FrReg,
                           0xf009, 0xf00f, 4, 8>));

    // FMOV.S FRm, @Rn
    // 1111nnnnmmmm1010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmovs), Ptrn_FrReg,
                           Ptrn_Ind<Ptrn_GenReg>, 0xf00a, 0xf00f, 4, 8>));

    // FMOV.S FRm, @-Rn
    // 1111nnnnmmmm1011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmovs), Ptrn_FrReg,
                           Ptrn_DecInd<Ptrn_GenReg>, 0xf00b, 0xf00f, 4, 8>));

    // FMOV.S FRm, @(R0, Rn)
    // 1111nnnnmmmm0111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmovs), Ptrn_FrReg,
                           Ptrn_BinaryInd<Ptrn_R0Reg, Ptrn_GenReg, 0, 0, 8>,
                           0xf007, 0xf00f, 4, 0>));

    /*
     * Note: Some of the folling FMOV opcodes overlap with with single-precision
     * FMOV.S opcodes.  At runtime the determination of which one to use is
     * made by the SZ flag in the FPSCR register.
     */
    // FMOV DRm, DRn
    // 1111nnn0mmm01100
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov),
                           Ptrn_DrReg, Ptrn_DrReg, 0xf00c, 0xf11f, 5, 9>));

    // FMOV @Rm, DRn
    // 1111nnn0mmmm1000
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov),
                           Ptrn_Ind<Ptrn_GenReg>, Ptrn_DrReg,
                           0xf008, 0xf10f, 4, 9>));

    // FMOV @(R0, Rm), DRn
    // 1111nnn0mmmm0110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov),
                           Ptrn_BinaryInd<Ptrn_R0Reg, Ptrn_GenReg, 0, 0, 4>,
                           Ptrn_DrReg, 0xf006, 0xf10f, 0, 9>));

    // FMOV @Rm+, DRn
    // 1111nnn0mmmm1001
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_DrReg,
                           0xf009, 0xf10f, 4, 9>));

    // FMOV DRm, @Rn
    // 1111nnnnmmm01010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov),
                           Ptrn_DrReg, Ptrn_Ind<Ptrn_GenReg>,
                           0xf00a, 0xf01f, 5, 8>));

    // FMOV DRm, @-Rn
    // 1111nnnnmmm01011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov),
                           Ptrn_DrReg, Ptrn_DecInd<Ptrn_GenReg>,
                           0xf00b, 0xf01f, 5, 8>));

    // FMOV DRm, @(R0,Rn)
    // 1111nnnnmmm00111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov),
                           Ptrn_DrReg,
                           Ptrn_BinaryInd<Ptrn_R0Reg, Ptrn_GenReg, 0, 0, 8>,
                           0xf007, 0xf01f, 5, 0>));

    // FLDS FRm, FPUL
    // 1111mmmm00011101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(flds),
                           Ptrn_FrReg, Ptrn_FpulReg, 0xf01d, 0xf0ff, 8, 0>));

    // FSTS FPUL, FRn
    // 1111nnnn00001101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fsts),
                           Ptrn_FpulReg, Ptrn_FrReg, 0xf00d, 0xf0ff, 0, 8>));

    // FABS FRn
    // 1111nnnn01011101
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(fabs),
                           Ptrn_FrReg, 0xf05d, 0xf0ff, 8>));

    // FADD FRm, FRn
    // 1111nnnnmmmm0000
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fadd),
                           Ptrn_FrReg, Ptrn_FrReg, 0xf000, 0xf00f, 4, 8>));

    // FCMP/EQ FRm, FRn
    // 1111nnnnmmmm0100
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fcmpeq),
                           Ptrn_FrReg, Ptrn_FrReg, 0xf004, 0xf00f, 4, 8>));

    // FCMP/GT FRm, FRn
    // 1111nnnnmmmm0101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fcmpgt),
                           Ptrn_FrReg, Ptrn_FrReg, 0xf005, 0xf00f, 4, 8>));

    // FDIV FRm, FRn
    // 1111nnnnmmmm0011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fdiv),
                           Ptrn_FrReg, Ptrn_FrReg, 0xf003, 0xf00f, 4, 8>));

    // FLOAT FPUL, FRn
    // 1111nnnn00101101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(float),
                           Ptrn_FpulReg, Ptrn_FrReg, 0xf02d, 0xf0ff, 0, 8>));

    // FMAC FR0, FRm, FRn
    // 1111nnnnmmmm1110
    list.push_back(PtrnPtr(new TrinaryOperator<INST_PTRN(fmac),
                           Ptrn_Fr0Reg, Ptrn_FrReg, Ptrn_FrReg,
                           0xf00e, 0xf00f, 0, 4, 8>));

    // FMUL FRm, FRn
    // 1111nnnnmmmm0010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmul),
                           Ptrn_FrReg, Ptrn_FrReg, 0xf002, 0xf00f, 4, 8>));

    // FNEG FRn
    // 1111nnnn01001101
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(fneg),
                           Ptrn_FrReg, 0xf04d, 0xf0ff, 8>));

    // FSQRT FRn
    // 1111nnnn01101101
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(fsqrt),
                           Ptrn_FrReg, 0xf06d, 0xf0ff, 8>));

    // FSUB FRm, FRn
    // 1111nnnnmmmm0001
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fsub),
                           Ptrn_FrReg, Ptrn_FrReg, 0xf001, 0xf00f, 4, 8 >));

    // FTRC FRm, FPUL
    // 1111mmmm00111101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ftrc),
                           Ptrn_FrReg, Ptrn_FpulReg, 0xf03d, 0xf0ff, 8, 0>));

    // FABS DRn
    // 1111nnn001011101
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(fabs),
                           Ptrn_DrReg, 0xf05d, 0xf1ff, 9>));

    // FADD DRm, DRn
    // 1111nnn0mmm00000
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fadd),
                           Ptrn_DrReg, Ptrn_DrReg, 0xf000, 0xf11f, 5, 9>));

    // FCMP/EQ DRm, DRn
    // 1111nnn0mmm00100
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fcmpeq),
                           Ptrn_DrReg, Ptrn_DrReg, 0xf004, 0xf11f, 5, 9>));

    // FCMP/GT DRm, DRn
    // 1111nnn0mmm00101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fcmpgt),
                           Ptrn_DrReg, Ptrn_DrReg, 0xf005, 0xf11f, 5, 9>));

    // FDIV DRm, DRn
    // 1111nnn0mmm00011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fdiv),
                           Ptrn_DrReg, Ptrn_DrReg, 0xf003, 0xf11f, 5, 9>));

    // FCNVDS DRm, FPUL
    // 1111mmm010111101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fcnvds),
                           Ptrn_DrReg, Ptrn_FpulReg, 0xf0bd, 0xf1ff, 9>));

    // FCNVSD FPUL, DRn
    // 1111nnn010101101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fcnvsd),
                           Ptrn_FpulReg, Ptrn_DrReg, 0xf0ad, 0xf1ff, 0, 9>));

    // FLOAT FPUL, DRn
    // 1111nnn000101101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(float),
                           Ptrn_FpulReg, Ptrn_DrReg, 0xf02d, 0xf1ff, 0, 9>));

    // FMUL DRm, DRn
    // 1111nnn0mmm00010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmul),
                           Ptrn_DrReg, Ptrn_DrReg, 0xf002, 0xf11f, 5, 9>));

    // FNEG DRn
    // 1111nnn001001101
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(fneg),
                           Ptrn_DrReg, 0xf04d, 0xf1ff, 9>));

    // FSQRT DRn
    // 1111nnn001101101
    list.push_back(PtrnPtr(new UnaryOperator<INST_PTRN(fsqrt),
                           Ptrn_DrReg, 0xf06d, 0xf1ff, 9>));

    // FSUB DRm, DRn
    // 1111nnn0mmm00001
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fsub),
                           Ptrn_DrReg, Ptrn_DrReg, 0xf001, 0xf11f, 5, 9>));

    // FTRC DRm, FPUL
    // 1111mmm000111101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ftrc),
                           Ptrn_DrReg, Ptrn_FpulReg, 0xf03d, 0xf1ff, 9>));

    // LDS Rm, FPSCR
    // 0100mmmm01101010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(lds),
                           Ptrn_GenReg, Ptrn_FpscrReg, 0x406a, 0xf0ff, 8>));

    // LDS Rm, FPUL
    // 0100mmmm01011010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(lds),
                           Ptrn_GenReg, Ptrn_FpulReg, 0x405a, 0xf0ff, 8, 0>));

    // LDS.L @Rm+, FPSCR
    // 0100mmmm01100110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldsl),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_FpscrReg,
                           0x4066, 0xf0ff, 8, 0>));

    // LDS.L @Rm+, FPUL
    // 0100mmmm01010110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ldsl),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_FpulReg,
                           0x4056, 0xf0ff, 8, 0>));

    // STS FPSCR, Rn
    // 0000nnnn01101010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(sts),
                           Ptrn_FpscrReg, Ptrn_GenReg, 0x006a, 0xf0ff, 0, 8>));

    // STS FPUL, Rn
    // 0000nnnn01011010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(sts),
                           Ptrn_FpulReg, Ptrn_GenReg, 0x005a, 0xf0ff, 0, 8>));

    // STS.L FPSCR, @-Rn
    // 0100nnnn01100010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stsl),
                           Ptrn_FpscrReg, Ptrn_DecInd<Ptrn_GenReg>,
                           0x4062, 0xf0ff, 0, 8>));

    // STS.L FPUL, @-Rn
    // 0100nnnn01010010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(stsl),
                           Ptrn_FpulReg, Ptrn_DecInd<Ptrn_GenReg>,
                           0x4052, 0xf0ff, 0, 8>));

    // FMOV DRm, XDn
    // 1111nnn1mmm01100
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov),
                           Ptrn_DrReg, Ptrn_XdReg, 0xf00c, 0xf11f, 5, 9>));

    // FMOV XDm, DRn
    // 1111nnn0mmm11100
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov),
                           Ptrn_XdReg, Ptrn_DrReg, 0xf01c, 0xf11f, 5, 9>));

    // FMOV XDm, XDn
    // 1111nnn1mmm11100
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov),
                           Ptrn_XdReg, Ptrn_XdReg, 0xf11c, 0xf11f, 5, 9>));

    // FMOV @Rm, XDn
    // 1111nnn1mmmm1000
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov),
                           Ptrn_Ind<Ptrn_GenReg>, Ptrn_XdReg,
                           0xf108, 0xf10f, 4, 9>));

    // FMOV @Rm+, XDn
    // 1111nnn1mmmm1001
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov),
                           Ptrn_IndInc<Ptrn_GenReg>, Ptrn_XdReg,
                           0xf109, 0xf10f, 4, 9>));

    // FMOV @(R0, Rn), XDn
    // 1111nnn1mmmm0110
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov),
                           Ptrn_BinaryInd<Ptrn_R0Reg, Ptrn_GenReg, 0, 0, 4>,
                           Ptrn_XdReg, 0xf106, 0xf10f, 0, 9>));

    // FMOV XDm, @Rn
    // 1111nnnnmmm11010
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov),
                           Ptrn_XdReg, Ptrn_Ind<Ptrn_GenReg>,
                           0xf01a, 0xf01f, 5, 8>));

    // FMOV XDm, @-Rn
    // 1111nnnnmmm11011
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov),
                           Ptrn_XdReg, Ptrn_DecInd<Ptrn_GenReg>,
                           0xf01b, 0xf01f, 5, 8>));

    // FMOV XDm, @(R0, Rn)
    // 1111nnnnmmm10111
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fmov), Ptrn_XdReg,
                           Ptrn_BinaryInd<Ptrn_R0Reg, Ptrn_GenReg, 0, 0, 8>,
                           0xf017, 0xf01f, 5, 0>));

    // FIPR FVm, FVn - vector dot product
    // 1111nnmm11101101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(fipr),
                           Ptrn_FvReg, Ptrn_FvReg, 0xf0ed, 0xf0ff, 8, 10>));

    // FTRV MXTRX, FVn - multiple vector by matrix
    // 1111nn0111111101
    list.push_back(PtrnPtr(new BinaryOperator<INST_PTRN(ftrv),
                           Ptrn_XmtrxReg, Ptrn_FvReg, 0xf1fd, 0xf3ff, 0, 10>));

    // FRCHG
    // 1111101111111101
    list.push_back(PtrnPtr(new NoArgOperator<INST_PTRN(frchg), 0xfbfd>));

    // FSCHG
    // 1111001111111101
    list.push_back(PtrnPtr(new NoArgOperator<INST_PTRN(frchg), 0xf3fd>));

    return list;
}

