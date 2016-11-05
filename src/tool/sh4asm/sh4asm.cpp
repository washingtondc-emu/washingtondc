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

#include <iostream>

#include "types.hpp"

#include "sh4asm.hpp"

addr32_t Sh4Prog::lookup_sym(const std::string& sym_name) const {
    SymMap::const_iterator it = syms.find(sym_name);

    if (it == syms.end())
        throw BadSymbolError(sym_name.c_str());

    return it->second;
}

Sh4Prog::PatternList Sh4Prog::get_patterns() {
    PatternList list;

    /***************************************************************************
     **
     ** operators which take no arguments
     **
     **************************************************************************/
    list.push_back(TokPtr(new NoArgOperator<TXT_TOK(divou), 0x0019>));
    list.push_back(TokPtr(new NoArgOperator<TXT_TOK(rts), 0x000b>));
    list.push_back(TokPtr(new NoArgOperator<TXT_TOK(clrmac), 0x0028>));
    list.push_back(TokPtr(new NoArgOperator<TXT_TOK(clrs), 0x0048>));
    list.push_back(TokPtr(new NoArgOperator<TXT_TOK(clrt), 0x0008>));
    list.push_back(TokPtr(new NoArgOperator<TXT_TOK(ldtlb), 0x0038>));
    list.push_back(TokPtr(new NoArgOperator<TXT_TOK(nop), 0x0009>));
    list.push_back(TokPtr(new NoArgOperator<TXT_TOK(rte), 0x002b>));
    list.push_back(TokPtr(new NoArgOperator<TXT_TOK(sets), 0x0058>));
    list.push_back(TokPtr(new NoArgOperator<TXT_TOK(sett), 0x0018>));
    list.push_back(TokPtr(new NoArgOperator<TXT_TOK(sleep), 0x001b>));
    list.push_back(TokPtr(new NoArgOperator<TXT_TOK(frchg), 0xfbfd>));
    list.push_back(TokPtr(new NoArgOperator<TXT_TOK(fschg), 0xf3fd>));

    /***************************************************************************
     **
     ** operators which take 1 argument (general-purpose register):
     **
     **************************************************************************/
    // 0000nnnn00101001
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(movt), Tok_GenReg,
                          0x0029, 8>));

    // 0100nnnn00010001
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(cmppz), Tok_GenReg,
                          0x4011, 8>));

    // 0100nnnn00010101
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(cmppl), Tok_GenReg,
                          0x4015, 8>));

    // 0100nnnn00010000
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(dt), Tok_GenReg,
                          0x4010, 8>));

    // 0100nnnn00000100
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(rotl), Tok_GenReg,
                          0x4004, 8>));

    // 0100nnnn00000101
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(rotr), Tok_GenReg,
                          0x4005, 8>));

    // 0100nnnn00100100
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(rotcl), Tok_GenReg,
                          0x4024, 8>));

    // 0100nnnn00100101
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(rotcr), Tok_GenReg,
                          0x4025, 8>));

    // 0100nnnn00200000
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(shal), Tok_GenReg,
                          0x4020, 8>));

    // 0100nnnn00100001
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(shar), Tok_GenReg,
                          0x4021, 8>));

    // 0100nnnn00000000
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(shll), Tok_GenReg,
                          0x4000, 8>));

    // 0100nnnn00000001
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(shlr), Tok_GenReg,
                          0x4001, 8>));

    // 0100nnnn00001000
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(shll2), Tok_GenReg,
                          0x4008, 8>));

    // 0100nnnn00001001
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(shlr2), Tok_GenReg,
                          0x4009, 8>));

    // 0100nnnn00011000
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(shll8), Tok_GenReg,
                          0x4018, 8>));

    // 0100nnnn00011001
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(shlr8), Tok_GenReg,
                          0x4019, 8>));

    // 0100nnnn00101000
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(shll16), Tok_GenReg,
                          0x4028, 8>));

    // 0100nnnn00101001
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(shlr16), Tok_GenReg,
                          0x4029, 8>));

    // 0000nnnn00100011
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(braf), Tok_GenReg,
                          0x0023, 8>));

    // 0000nnnn00000011
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(bsrf), Tok_GenReg,
                          0x0003, 8>));

    // 0100nnnn00101011
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(jmp), Tok_GenReg,
                          0x402b, 8>));

    // 0100nnnn00001011
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(jsr), Tok_GenReg,
                          0x400b, 8>));


    /***************************************************************************
     **
     ** opcode that only takes an immediate value as input
     **
     **************************************************************************/
    // 10001000iiiiiiii
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(cmpeq), Tok_immed<0xff>,
                          0x8800, 0>));

    // 11001101iiiiiiii
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(andb), Tok_immed<0xff>,
                          0xcd00, 0>));

    // 11001111iiiiiiii
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(orb), Tok_immed<0xff>,
                          0xcf00, 0>));

    // 11001011iiiiiiii
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(or), Tok_immed<0xff>,
                          0xcb00, 0>));

    // 11001000iiiiiiii
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(tst), Tok_immed<0xff>,
                          0xc800, 0>));

    // 11001100iiiiiiii
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(tstb), Tok_immed<0xff>,
                          0xcc00, 0>));

    // 11001010iiiiiiii
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(xor), Tok_immed<0xff>,
                          0xca00, 0>));

    // 11001110iiiiiiii
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(xorb), Tok_immed<0xff>,
                          0xce00, 0>));

    // 10001011dddddddd
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(bf), Tok_Disp<0xff>,
                          0x8b00, 0>));

    // 10001111dddddddd
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(bfs), Tok_Disp<0xff>,
                          0x8f00, 0>));

    // 10001001dddddddd
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(bt), Tok_Disp<0xff>,
                          0x8900, 0>));

    // 10001101dddddddd
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(bts), Tok_Disp<0xff>,
                          0x8d00, 0>));

    // 1010dddddddddddd
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(bts), Tok_Disp<0xfff>,
                          0xa000, 0>));

    // 1011dddddddddddd
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(bsr), Tok_Disp<0xfff>,
                          0xb000, 0>));

    // 11000011iiiiiiii
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(trapa), Tok_immed<0x0ff>,
                          0xc300, 0>));

    /***************************************************************************
     **
     ** opcode that takes a general-purpose register containing the
     ** address of its sole argument.
     **
     **************************************************************************/
    // 0100nnnn00011011
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(tasb), Tok_Ind<Tok_GenReg>,
                          0x401b, 8>));

    // 0000nnnn10100011
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(ocbi), Tok_Ind<Tok_GenReg>,
                          0x00a3, 8>));

    // 0000nnnn10100011
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(ocbp), Tok_Ind<Tok_GenReg>,
                          0x00b3, 8>));

    // 0000nnnn10000011
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(pref), Tok_Ind<Tok_GenReg>,
                          0x0083, 8>));


    /***************************************************************************
     **
     ** LDC/STC instructions
     **
     **************************************************************************/
    // 0100mmmm00001110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldc), Tok_GenReg,
                          Tok_SrReg, 0x400e, 8, 0>));

    // 0100mmmm00011110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldc), Tok_GenReg,
                          Tok_GbrReg, 0x401e, 8, 0>));

    // 0100mmmm00101110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldc), Tok_GenReg,
                          Tok_VbrReg, 0x402e, 8, 0>));

    // 0100mmmm00111110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldc), Tok_GenReg,
                          Tok_SsrReg, 0x403e, 8, 0>));

    // 0100mmmm01001110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldc), Tok_GenReg,
                          Tok_SpcReg, 0x404e, 8, 0>));

    // 0100mmmm11111010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldc), Tok_GenReg,
                          Tok_DbrReg, 0x40fa, 8, 0>));

    // 0000mmmm00000010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stc), Tok_SrReg,
                          Tok_GenReg, 0x0002, 0, 8>));

    // 0000mmmm00010010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stc), Tok_GbrReg,
                          Tok_GenReg, 0x0012, 0, 8>));

    // 0000mmmm00100010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stc), Tok_VbrReg,
                          Tok_GenReg, 0x0022, 0, 8>));

    // 0000mmmm01000010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stc), Tok_SsrReg,
                          Tok_GenReg, 0x0032, 0, 8>));

    // 0000mmmm01000010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stc), Tok_SpcReg,
                          Tok_GenReg, 0x0042, 0, 8>));

    // 0000mmmm00111010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stc), Tok_SgrReg,
                          Tok_GenReg, 0x003a, 0, 8>));

    // 0000mmmm11111010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stc), Tok_DbrReg,
                          Tok_GenReg, 0x00fa, 0, 8>));

    // 0100mmmm00000111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldcl),
                          Tok_IndInc<Tok_GenReg>, Tok_SrReg, 0x4007, 8, 0>));

    // 0100mmmm00010111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldcl),
                          Tok_IndInc<Tok_GenReg>, Tok_GbrReg, 0x4017, 8, 0>));

    // 0100mmmm00100111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldcl),
                          Tok_IndInc<Tok_GenReg>, Tok_VbrReg, 0x4027, 8, 0>));

    // 0100mmmm00110111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldcl),
                          Tok_IndInc<Tok_GenReg>, Tok_SsrReg, 0x4037, 8, 0>));

    // 0100mmmm01000111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldcl),
                          Tok_IndInc<Tok_GenReg>, Tok_SpcReg, 0x4047, 8, 0>));

    // 0100mmmm11110110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldcl),
                          Tok_IndInc<Tok_GenReg>, Tok_DbrReg, 0x40f6, 8, 0>));

    // 0100nnnn00000011
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stcl),
                          Tok_SrReg, Tok_DecInd<Tok_GenReg>, 0x4003, 0, 8>));

    // 0100nnnn00010011
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stcl),
                          Tok_GbrReg, Tok_DecInd<Tok_GenReg>, 0x4013, 0, 8>));

    // 0100nnnn00100011
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stcl),
                          Tok_VbrReg, Tok_DecInd<Tok_GenReg>, 0x4023, 0, 8>));

    // 0100nnnn00110011
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stcl),
                          Tok_SsrReg, Tok_DecInd<Tok_GenReg>, 0x4033, 0, 8>));

    // 0100nnnn01000011
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stcl),
                          Tok_SpcReg, Tok_DecInd<Tok_GenReg>, 0x4043, 0, 8>));

    // 0100nnnn00110010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stcl),
                          Tok_SgrReg, Tok_DecInd<Tok_GenReg>, 0x4032, 0, 8>));

    // 0100nnnn11110010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stcl),
                          Tok_DbrReg, Tok_DecInd<Tok_GenReg>, 0x40f2, 0, 8>));

    /***************************************************************************
     **
     ** Opcodes that take an immediate as input and a general-purpose
     ** register as output
     **
     **************************************************************************/
    // 1110nnnniiiiiiii
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(mov), Tok_immed<0x00ff>,
                          Tok_GenReg, 0xe000, 0, 8>));

    // 0111nnnniiiiiiii
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(add), Tok_immed<0x00ff>,
                          Tok_GenReg, 0x7000, 0, 8>));

    /***************************************************************************
     **
     ** Opcodes that add an immediate value (scaled by either 2 or 4) to the
     ** PC and then use *that* address as the source to move a value into a
     ** given general-purpose register (the destination).
     **
     **************************************************************************/

    // 1001nnnndddddddd
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movw), Tok_BinaryInd<
                          Tok_Disp<0x00ff>, Tok_PcReg, 0x0000, 0, 0>,
                          Tok_GenReg, 0x9000, 0, 8>));

    // 1001nnnndddddddd
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movw), Tok_BinaryInd<
                          Tok_Disp<0x00ff>, Tok_PcReg, 0x0000, 0, 0>,
                          Tok_GenReg, 0xd000, 0, 8>));

    /***************************************************************************
     **
     ** Opcodes that take a general-purpose register as a source and a
     ** general-purpose register as a destination
     **
     **************************************************************************/
    // 0110nnnnmmmm0011
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movw), Tok_GenReg,
                          Tok_GenReg, 0x6003, 4, 8>));

    // 0110nnnnmmmm1000
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(swapb), Tok_GenReg,
                          Tok_GenReg, 0x6008, 4, 8>));

    // 0110nnnnmmmm1001
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(swapw), Tok_GenReg,
                          Tok_GenReg, 0x6009, 4, 8>));

    // 0110nnnnmmmm1101
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(xtrct), Tok_GenReg,
                          Tok_GenReg, 0x200d, 4, 8>));

    // 0111nnnnmmmm1100
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(add), Tok_GenReg,
                          Tok_GenReg, 0x300c, 4, 8>));

    // 0111nnnnmmmm1110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(addc), Tok_GenReg,
                          Tok_GenReg, 0x300e, 4, 8>));

    // 0111nnnnmmmm1111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(addv), Tok_GenReg,
                          Tok_GenReg, 0x300f, 4, 8>));

    // 0011nnnnmmmm0000
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(cmpeq), Tok_GenReg,
                          Tok_GenReg, 0x3000, 4, 8>));

    // 0011nnnnmmmm0010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(cmphs), Tok_GenReg,
                          Tok_GenReg, 0x3002, 4, 8>));

    // 0011nnnnmmmm0011
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(cmpge), Tok_GenReg,
                          Tok_GenReg, 0x3003, 4, 8>));

    // 0011nnnnmmmm0110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(cmphi), Tok_GenReg,
                          Tok_GenReg, 0x3006, 4, 8>));

    // 0011nnnnmmmm0111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(cmpgt), Tok_GenReg,
                          Tok_GenReg, 0x3007, 4, 8>));

    // 0010nnnnmmmm1100
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(cmpstr), Tok_GenReg,
                          Tok_GenReg, 0x200c, 4, 8>));

    // 0011nnnnmmmm0100
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(div1), Tok_GenReg,
                          Tok_GenReg, 0x3004, 4, 8>));

    // 0010nnnnmmmm0111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(div0s), Tok_GenReg,
                          Tok_GenReg, 0x2007, 4, 8>));

    // 0011nnnnmmmm1101
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(dmulsl), Tok_GenReg,
                          Tok_GenReg, 0x300d, 4, 8>));

    // 0011nnnnmmmm0101
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(dmulul), Tok_GenReg,
                          Tok_GenReg, 0x3005, 4, 8>));

    // 0110nnnnmmmm1110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(extsb), Tok_GenReg,
                          Tok_GenReg, 0x600e, 4, 8>));

    // 0110nnnnmmmm1111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(extsw), Tok_GenReg,
                          Tok_GenReg, 0x600f, 4, 8>));

    // 0110nnnnmmmm1100
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(extub), Tok_GenReg,
                          Tok_GenReg, 0x600c, 4, 8>));

    // 0110nnnnmmmm1101
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(extuw), Tok_GenReg,
                          Tok_GenReg, 0x600d, 4, 8>));

    // 0000nnnnmmmm0111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(mull), Tok_GenReg,
                          Tok_GenReg, 0x0007, 4, 8>));

    // 0010nnnnmmmm1111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(mulsw), Tok_GenReg,
                          Tok_GenReg, 0x200f, 4, 8>));

    // 0010nnnnmmmm1110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(muluw), Tok_GenReg,
                          Tok_GenReg, 0x200e, 4, 8>));

    // 0110nnnnmmmm1011
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(neg), Tok_GenReg,
                          Tok_GenReg, 0x600b, 4, 8>));

    // 0110nnnnmmmm1010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(negc), Tok_GenReg,
                          Tok_GenReg, 0x600a, 4, 8>));

    // 0011nnnnmmmm1000
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(sub), Tok_GenReg,
                          Tok_GenReg, 0x3008, 4, 8>));

    // 0011nnnnmmmm1010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(subc), Tok_GenReg,
                          Tok_GenReg, 0x300a, 4, 8>));

    // 0011nnnnmmmm1011
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(subv), Tok_GenReg,
                          Tok_GenReg, 0x300b, 4, 8>));

    // 0010nnnnmmmm1001
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(and), Tok_GenReg,
                          Tok_GenReg, 0x2009, 4, 8>));

    // 0110nnnnmmmm0111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(not), Tok_GenReg,
                          Tok_GenReg, 0x6007, 4, 8>));

    // 0010nnnnmmmm1011
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(or), Tok_GenReg,
                          Tok_GenReg, 0x200b, 4, 8>));

    // 0010nnnnmmmm1000
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(tst), Tok_GenReg,
                          Tok_GenReg, 0x2008, 4, 8>));

    // 0010nnnnmmmm1010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(xor), Tok_GenReg,
                          Tok_GenReg, 0x200a, 4, 8>));

    // 0100nnnnmmmm1100
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(shad), Tok_GenReg,
                          Tok_GenReg, 0x400c, 4, 8>));

    // 0100nnnnmmmm1101
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(shld), Tok_GenReg,
                          Tok_GenReg, 0x400c, 4, 8>));

    /***************************************************************************
     **
     ** Opcodes that use bank-switched registers as the source or destination
     **
     **************************************************************************/
    // LDC Rm, Rn_BANK
    // 0100mmmm1nnn1110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldc), Tok_GenReg,
                          Tok_BankReg, 0x408e, 8, 4>));

    // LDC.L @Rm+, Rn_BANK
    // 0100mmmm1nnn0111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldcl),
                          Tok_IndInc<Tok_GenReg>, Tok_BankReg, 0x4087, 8, 4>));

    // STC Rm_BANK, Rn
    // 0000nnnn1mmm0010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stc), Tok_BankReg,
                          Tok_GenReg, 0x0082, 4, 8>));

    // STC.L Rm_BANK, @-Rn
    // 0100nnnn1mmm0011
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stcl), Tok_BankReg,
                          Tok_DecInd<Tok_GenReg>, 0x4083, 4, 8>));

    /***************************************************************************
     **
     ** Some assorted LDS/STS instructions
     **
     **************************************************************************/
    // LDS Rm,MACH
    // 0100mmmm00001010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(lds), Tok_GenReg, Tok_Mach,
                          0x400a, 8, 0>));

    // LDS Rm, MACL
    // 0100mmmm00011010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(lds), Tok_GenReg, Tok_Macl,
                          0x401a, 8, 0>));

    // STS MACH, Rn
    // 0000nnnn00001010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(sts), Tok_Mach, Tok_GenReg,
                          0x000a, 0, 8>));

    // STS MACL, Rn
    // 0000nnnn00011010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(sts), Tok_Macl, Tok_GenReg,
                          0x001a, 0, 8>));

    // LDS Rm, PR
    // 0100mmmm00101010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(lds), Tok_GenReg, Tok_PrReg,
                          0x402a, 8, 0>));

    // STS PR, Rn
    // 0000nnnn00101010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(sts), Tok_PrReg, Tok_GenReg,
                          0x002a, 0, 8>));

    // LDS.L @Rm+, MACH
    // 0100mmmm00000110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldsl),
                          Tok_IndInc<Tok_GenReg>, Tok_Mach, 0x4006, 8, 0>));

    // LDS.L @Rm+, MACL
    // 0100mmmm00010110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldsl),
                          Tok_IndInc<Tok_GenReg>, Tok_Macl, 0x4016, 8, 0>));

    // STS.L MACH, @-Rn
    // 0100mmmm00000010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stsl),
                          Tok_Mach, Tok_DecInd<Tok_GenReg>, 0x4002, 0, 8>));

    // STS.L MACL, @-Rn
    // 0100mmmm00010010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stsl),
                          Tok_Macl, Tok_DecInd<Tok_GenReg>, 0x4012, 0, 8>));

    // LDS.L @Rm+, PR
    // 0100mmmm00100110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ldsl),
                          Tok_IndInc<Tok_GenReg>, Tok_PrReg, 0x4026, 8, 0>));

    // STS.L PR, @-Rn
    // 0100nnnn00100010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(stsl), Tok_PrReg,
                          Tok_DecInd<Tok_GenReg>, 0x4022, 0, 8>));

    /***************************************************************************
     **
     ** Opcodes that move a general-purpose register into the address pointed
     ** to by another general-purpose register
     **
     **************************************************************************/
    // MOV.B Rm, @Rn
    // 0010nnnnmmmm0000
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movb), Tok_GenReg,
                          Tok_Ind<Tok_GenReg>, 0x2000, 4, 8>));

    // MOV.W Rm, @Rn
    // 0010nnnnmmmm0001
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movw), Tok_GenReg,
                          Tok_Ind<Tok_GenReg>, 0x2001, 4, 8>));

    // MOV.L Rm, @Rn
    // 0010nnnnmmmm0010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movl), Tok_GenReg,
                          Tok_Ind<Tok_GenReg>, 0x2002, 4, 8>));


    /***************************************************************************
     **
     ** Opcodes that move the contents of the address pointed to by a
     ** general-purpose register into a general-purpose register
     **
     **************************************************************************/
    // MOV.B @Rm, Rn
    // 0110nnnnmmmm0000
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movb), Tok_Ind<Tok_GenReg>,
                          Tok_GenReg, 0x6000, 4, 8>));

    // MOV.W @Rm, Rn
    // 0110nnnnmmmm0001
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movw), Tok_Ind<Tok_GenReg>,
                          Tok_GenReg, 0x6001, 4, 8>));

    // MOV.L @Rm, Rn
    // 0110nnnnmmmm0010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movl), Tok_Ind<Tok_GenReg>,
                          Tok_GenReg, 0x6002, 4, 8>));

    /***************************************************************************
     **
     ** Opcodes that move the contents of a general-purpose register into the
     ** memory pointed to by another general purpose register after first
     ** decrementing the destination register
     **
     **************************************************************************/
    // MOV.B Rm, @-Rn
    // 0010nnnnmmmm0100
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movb), Tok_GenReg,
                          Tok_DecInd<Tok_GenReg>, 0x2004, 4, 8>));

    // MOV.W Rm, @-Rn
    // 0010nnnnmmmm0101
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movw), Tok_GenReg,
                          Tok_DecInd<Tok_GenReg>, 0x2005, 4, 8>));

    // MOV.L Rm, @-Rn
    // 0010nnnnmmmm0110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movl), Tok_GenReg,
                          Tok_DecInd<Tok_GenReg>, 0x2006, 4, 8>));

    /***************************************************************************
     **
     ** Opcodes that move the contents of the memory pointed to by the source
     ** register into the destination register and then increment the source
     ** register
     **
     **************************************************************************/
    // MOV.B @Rm+, Rn
    // 0110nnnnmmmm0100
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movb),
                          Tok_IndInc<Tok_GenReg>, Tok_GenReg, 0x6004, 4, 8>));

    // MOV.W @Rm+, Rn
    // 0110nnnnmmmm0101
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movw),
                          Tok_IndInc<Tok_GenReg>, Tok_GenReg, 0x6005, 4, 8>));

    // MOV.L @Rm+, Rn
    // 0110nnnnmmmm0110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movl),
                          Tok_IndInc<Tok_GenReg>, Tok_GenReg, 0x6006, 4, 8>));

    /***************************************************************************
     **
     ** Opcodes that multiply the contents of the memory pointed to by the
     ** source register into the second source register and add that to MAC.
     ** Then both source registers are incremented
     **
     **************************************************************************/
    // MAC.L @Rm+, @Rn+
    // 0000nnnnmmmm1111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(macl), Tok_IndInc<Tok_GenReg>,
                          Tok_IndInc<Tok_GenReg>, 0x000f, 4, 8>));

    // MAC.W @Rm+, @Rn+
    // 0100nnnnmmmm1111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(macw),
                          Tok_IndInc<Tok_GenReg>, Tok_IndInc<Tok_GenReg>,
                          0x400f, 4, 8>));

    /***************************************************************************
     **
     ** Opcodes that move R0 into @(source reg + displacement).
     **
     **************************************************************************/
    // MOV.B R0, @(disp, Rn)
    // 10000000nnnndddd
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movb), Tok_R0Reg,
                          Tok_BinaryInd<Tok_Disp<0xf>, Tok_GenReg, 0, 0, 4>,
                          0x8000, 0, 0>));

    // MOV.W R0, @(disp, Rn)
    // 10000001nnnndddd
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movw), Tok_R0Reg,
                          Tok_BinaryInd<Tok_Disp<0xf>, Tok_GenReg, 0, 0, 4>,
                          0x8100, 0, 0>));

    /***************************************************************************
     **
     ** Opcode that moves a general-purpose register into
     ** @(source reg + displacement).
     **
     **************************************************************************/
    // MOV.L Rm, @(disp, Rn)
    // 0001nnnnmmmmdddd
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movl), Tok_GenReg,
                          Tok_BinaryInd<Tok_Disp<0xf>, Tok_GenReg, 0, 0, 8>,
                          0x1000, 4, 0>));

    /***************************************************************************
     **
     ** Opcodes that move @(source reg + displacement) into R0
     **
     **************************************************************************/
    // MOV.B @(disp, Rm), R0
    // 10000100mmmmdddd
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movb),
                          Tok_BinaryInd<Tok_Disp<0xf>, Tok_GenReg, 0, 0, 4>,
                          Tok_R0Reg, 0x8400, 0, 0>));

    // MOV.W @(disp, Rm), R0
    // 10000101mmmmdddd
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movb),
                          Tok_BinaryInd<Tok_Disp<0xf>, Tok_GenReg, 0, 0, 4>,
                          Tok_R0Reg, 0x8500, 0, 0>));

    /***************************************************************************
     **
     ** Opcode that moves @(source reg + displacement) into a general-purpose
     ** register.
     **
     **************************************************************************/
    // MOV.L @(disp, Rm), Rn
    // 0101nnnnmmmmdddd
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movl),
                          Tok_BinaryInd<Tok_Disp<0xf>, Tok_GenReg, 0, 0, 4>,
                          Tok_GenReg, 0x5000, 0, 8>));

    /***************************************************************************
     **
     ** Opcodes that move a general purpose register into
     ** @(R0 + another general-purpose register)
     **
     **************************************************************************/
    // MOV.B Rm, @(R0, Rn)
    // 0000nnnnmmmm0100
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movb), Tok_GenReg,
                          Tok_BinaryInd<Tok_R0Reg, Tok_GenReg, 0, 0, 8>,
                          0x0004, 4, 0>));

    // MOV.W Rm, @(R0, Rn)
    // 0000nnnnmmmm0101
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movw), Tok_GenReg,
                          Tok_BinaryInd<Tok_R0Reg, Tok_GenReg, 0, 0, 8>,
                          0x0005, 4, 0>));

    // MOV.L Rm, @(R0, Rn)
    // 0000nnnnmmmm0110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movl), Tok_GenReg,
                          Tok_BinaryInd<Tok_R0Reg, Tok_GenReg, 0, 0, 8>,
                          0x0006, 4, 0>));

    /***************************************************************************
     **
     ** Opcodes that move @(R0 + general purpose register) into
     ** another general purpose register
     **
     **************************************************************************/
    // MOV.B @(R0, Rm), Rn
    // 0000nnnnmmmm1100
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movb),
                          Tok_BinaryInd<Tok_R0Reg, Tok_GenReg, 0, 0, 4>,
                          Tok_GenReg, 0x000c, 0, 8>));

    // MOV.W @(R0, Rm), Rn
    // 0000nnnnmmmm1101
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movw),
                          Tok_BinaryInd<Tok_R0Reg, Tok_GenReg, 0, 0, 4>,
                          Tok_GenReg, 0x000d, 0, 8>));

    // MOV.L @(R0, Rm), Rn
    // 0000nnnnmmmm1110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movl),
                          Tok_BinaryInd<Tok_R0Reg, Tok_GenReg, 0, 0, 4>,
                          Tok_GenReg, 0x000e, 0, 8>));

    /***************************************************************************
     **
     ** Opcodes that move R0 into @(disp + GBR)
     **
     **************************************************************************/
    // MOV.B R0, @(disp, GBR)
    // 11000000dddddddd
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movb), Tok_R0Reg,
                          Tok_BinaryInd<Tok_Disp<0xff>, Tok_GbrReg, 0, 0, 0>,
                          0xc000, 0, 0>));

    // MOV.W R0, @(disp, GBR)
    // 11000001dddddddd
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movw), Tok_R0Reg,
                          Tok_BinaryInd<Tok_Disp<0xff>, Tok_GbrReg, 0, 0, 0>,
                          0xc100, 0, 0>));

    // MOV.L R0, @(disp, GBR)
    // 11000010dddddddd
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movl), Tok_R0Reg,
                          Tok_BinaryInd<Tok_Disp<0xff>, Tok_GbrReg, 0, 0, 0>,
                          0xc200, 0, 0>));

    /***************************************************************************
     **
     ** Opcodes that move @(disp + GBR) into R0
     **
     **************************************************************************/
    // MOV.B R0, @(disp, GBR)
    // 11000100dddddddd
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movb),
                          Tok_BinaryInd<Tok_Disp<0xff>, Tok_GbrReg, 0, 0, 0>,
                          Tok_R0Reg, 0xc400, 0, 0>));

    // MOV.W R0, @(disp, GBR)
    // 11000101dddddddd
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movw),
                          Tok_BinaryInd<Tok_Disp<0xff>, Tok_GbrReg, 0, 0, 0>,
                          Tok_R0Reg, 0xc500, 0, 0>));

    // MOV.L R0, @(disp, GBR)
    // 11000110dddddddd
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movl),
                          Tok_BinaryInd<Tok_Disp<0xff>, Tok_GbrReg, 0, 0, 0>,
                          Tok_R0Reg, 0xc600, 0, 0>));

    /***************************************************************************
     **
     ** Opcode that does a 4-byte move from @(disp + PC + 1) into R0
     **
     **************************************************************************/
    // MOVA @(disp, PC), R0
    // 11000111dddddddd
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(mova),
                          Tok_BinaryInd<Tok_Disp<0xff>, Tok_PcReg, 0, 0, 0>,
                          Tok_R0Reg, 0xc700, 0, 0>));

    /***************************************************************************
     **
     ** Opcode that moves R0 into the address pointed to by a general-purpose
     ** register.  Apparently it doesn't fetch a cache block; IDK if that's
     ** supposed to mean it operates in write-through mode or if it skips the
     ** cache entirely or if it means something completely different from
     ** either hypothesis.
     **
     **************************************************************************/
    // MOVA.L R0, @Rn
    // 0000nnnn11000011
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movcal), Tok_R0Reg,
                          Tok_Ind<Tok_GenReg>, 0x00c3, 0, 8>));

    /***************************************************************************
     **
     ** Floating-point opcodes
     **
     **************************************************************************/
    // FLDI0 FRn - load 0.0 into Frn
    // 1111nnnn10001101
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(fldi0), Tok_FrReg,
                          0xf08d, 8>));

    // FLDI1 Frn - load 1.0 into Frn
    // 1111nnnn10011101
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(fldi1), Tok_FrReg,
                          0xf09d, 8>));

    // FMOV FRm, FRn
    //1111nnnnmmmm1100
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fmov),
                          Tok_FrReg, Tok_FrReg, 0xf00c, 4, 8>));

    // FMOV.S @Rm, FRn
    // 1111nnnnmmmm1000
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fmovs),
                          Tok_Ind<Tok_GenReg>, Tok_FrReg, 0xf008, 4, 8>));

    // FMOV.S @(R0,Rm), FRn
    // 1111nnnnmmmm0110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fmovs),
                          Tok_BinaryInd<Tok_R0Reg, Tok_GenReg, 0, 0, 4>,
                          Tok_FrReg, 0xf006,0, 8>));

    // FMOV.S @Rm+, FRn
    // 1111nnnnmmmm1001
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fmovs),
                          Tok_IndInc<Tok_GenReg>, Tok_FrReg, 0xf009, 4, 8>));

    // FMOV.S FRm, @Rn
    // 1111nnnnmmmm1010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fmovs), Tok_FrReg,
                          Tok_Ind<Tok_GenReg>, 0xf00a, 4, 8>));

    // FMOV.S FRm, @-Rn
    // 1111nnnnmmmm1011
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fmovs), Tok_FrReg,
                          Tok_DecInd<Tok_GenReg>, 0xf00b, 4, 8>));

    // FMOV.S FRm, @(R0, Rn)
    // 1111nnnnmmmm0111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fmovs), Tok_FrReg,
                          Tok_BinaryInd<Tok_R0Reg, Tok_GenReg, 0, 0, 8>,
                          0xf007, 4, 0>));

    /*
     * Note: Some of the folling FMOV opcodes overlap with with single-precision
     * FMOV.S opcodes.  At runtime the determination of which one to use is
     * made by the SZ flag in the FPSCR register.
     */
    // FMOV DRm, DRn
    // 1111nnn0mmm01100
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fmov),
                          Tok_DrReg, Tok_DrReg, 0xf00c, 5, 9>));

    // FMOV @Rm, DRn
    // 1111nnn0mmmm1000
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fmov),
                          Tok_Ind<Tok_GenReg>, Tok_DrReg, 0xf008, 4, 9>));

    // FMOV @(R0, Rm), DRn
    // 1111nnn0mmmm0110
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fmov),
                          Tok_BinaryInd<Tok_R0Reg, Tok_GenReg, 0, 0, 4>,
                          Tok_DrReg, 0xf006, 0, 9>));

    // FMOV @Rm+, DRn
    // 1111nnn0mmmm1001
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fmov),
                          Tok_IndInc<Tok_GenReg>, Tok_DrReg, 0xf009, 4, 9>));

    // FMOV DRm, @Rn
    // 1111nnnnmmm01010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fmov),
                          Tok_DrReg, Tok_Ind<Tok_GenReg>, 0xf00a, 5, 8>));

    // FMOV DRm, @-Rn
    // 1111nnnnmmm01011
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fmov),
                          Tok_DrReg, Tok_DecInd<Tok_GenReg>, 0xf00b, 5, 8>));

    // FMOV DRm, @(R0,Rn)
    // 1111nnnnmmm00111
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fmov),
                          Tok_DrReg,
                          Tok_BinaryInd<Tok_R0Reg, Tok_GenReg, 0, 0, 8>,
                          0xf007, 5, 0>));

    // FLDS FRm, FPUL
    // 1111mmmm00011101
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(flds),
                          Tok_FrReg, Tok_FpulReg, 0xf01d, 8, 0>));

    // FSTS FPUL, FRn
    // 1111nnnn00001101
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fsts),
                          Tok_FpulReg, Tok_FrReg, 0xf00d, 0, 8>));

    // FABS FRn
    // 1111nnnn01011101
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(fabs),
                          Tok_FrReg, 0xf05d, 8>));

    // FADD FRm, FRn
    // 1111nnnnmmmm0000
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fadd),
                          Tok_FrReg, Tok_FrReg, 0xf000, 4, 8>));

    // FCMP/EQ FRm, FRn
    // 1111nnnnmmmm0100
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fcmpeq),
                          Tok_FrReg, Tok_FrReg, 0xf004, 4, 8>));

    // FCMP/GT FRm, FRn
    // 1111nnnnmmmm0101
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fcmpgt),
                          Tok_FrReg, Tok_FrReg, 0xf005, 4, 8>));

    // FDIV FRm, FRn
    // 1111nnnnmmmm0011
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fdiv),
                          Tok_FrReg, Tok_FrReg, 0xf003, 4, 8>));

    // FLOAT FPUL, FRn
    // 1111nnnn00101101
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(float),
                          Tok_FpulReg, Tok_FrReg, 0xf02d, 0, 8>));

    // FMAC FR0, FRm, FRn
    // 1111nnnnmmmm1110
    list.push_back(TokPtr(new TrinaryOperator<TXT_TOK(fmac),
                          Tok_Fr0Reg, Tok_FrReg, Tok_FrReg,
                          0xf00e, 0, 4, 8>));

    // FMUL FRm, FRn
    // 1111nnnnmmmm0010
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fmul),
                          Tok_FrReg, Tok_FrReg, 0xf002, 4, 8>));

    // FNEG FRn
    // 1111nnnn01001101
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(fneg),
                          Tok_FrReg, 0xf04d, 8>));

    // FSQRT FRn
    // 1111nnnn01101101
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(fsqrt),
                          Tok_FrReg, 0xf06d, 8>));

    // FSUB FRm, FRn
    // 1111nnnnmmmm0001
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(fsub),
                          Tok_FrReg, Tok_FrReg, 0xf001, 4, 8 >));

    // FTRC FRm, FPUL
    // 1111mmmm00111101
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(ftrc),
                          Tok_FrReg, Tok_FpulReg, 0xf03d, 8, 0>));

    return list;
}

inst_t Sh4Prog::assemble_line(const std::string& inst) {
    TokList toks = tokenize_line(preprocess_line(inst));
    PatternList patterns = get_patterns();

    for (PatternList::iterator it = patterns.begin(); it != patterns.end();
         ++it) {
        if ((*it)->matches(toks.rbegin(), toks.rend())) {
            return (*it)->assemble();
        }
    }

    throw ParseError("Unrecognized opcode");
}

std::string Sh4Prog::preprocess_line(const std::string& line) {
    size_t comment_start = line.find_first_of('!');

    if (comment_start == std::string::npos)
        return line;

    return line.substr(0, comment_start);
}

Sh4Prog::TokList Sh4Prog::tokenize_line(const std::string& line) {
    std::string cur_tok;
    TokList tok_list;

    for (std::string::const_iterator it = line.begin(); it != line.end();
         ++it) {
        char cur_char = *it;

        if (cur_char == ' ' || cur_char == '\t' || cur_char == '\n') {
            if (cur_tok.size()) {
                tok_list.push_back(TokPtr(new TxtToken(cur_tok)));
                cur_tok.clear();
            }
        } else if (cur_char == ':' || cur_char == ',' ||
                   cur_char == '@' || cur_char == '#' ||
                   cur_char == '(' || cur_char == ')' ||
                   cur_char == '+' || cur_char == '-') {
            if (cur_tok.size()) {
                tok_list.push_back(TokPtr(new TxtToken(cur_tok)));
            }
            std::string cur_char_as_str(1, cur_char);
            tok_list.push_back(TokPtr(new TxtToken(cur_char_as_str)));
            cur_tok.clear();
        } else {
            cur_tok.push_back(cur_char);
        }
    }

    if (cur_tok.size()) {
        tok_list.push_back(TokPtr(new TxtToken(cur_tok)));
        cur_tok.clear();
    }

    return tok_list;
}

void Sh4Prog::add_label(const std::string& lbl) {
    syms[lbl] = prog.size() - 1;
}
