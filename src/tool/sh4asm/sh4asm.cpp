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
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(bf), Tok_immed<0xff>,
                          0x8b00, 0>));

    // 10001111dddddddd
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(bfs), Tok_immed<0xff>,
                          0x8f00, 0>));

    // 10001001dddddddd
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(bt), Tok_immed<0xff>,
                          0x8900, 0>));

    // 10001101dddddddd
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(bts), Tok_immed<0xff>,
                          0x8d00, 0>));

    // 1010dddddddddddd
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(bts), Tok_immed<0xfff>,
                          0xa000, 0>));

    // 1011dddddddddddd
    list.push_back(TokPtr(new UnaryOperator<TXT_TOK(bsr), Tok_immed<0xfff>,
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


    // 0010nnnnmmmm0001
    list.push_back(TokPtr(new BinaryOperator<TXT_TOK(movw), Tok_GenReg,
                          Tok_Ind<Tok_GenReg>, 0x2001, 4, 8>));

    // 0110nnnnmmmm0001
    list.push_back(TokPtr(new BinaryOperator<Tok_movw, Tok_Ind<Tok_GenReg>,
                          Tok_GenReg, 0x6001, 4, 8>));
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
                   cur_char == '@' || cur_char == '#') {
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

const struct Sh4Prog::OpNoArgs Sh4Prog::op_noargs[] = {
    { "DIV0U",  0x0019 },
    { "RTS",    0x000b },
    { "CLRMAC", 0x0028 },
    { "CLRS",   0x0048 },
    { "CLRT",   0x0008 },
    { "LDTLB",  0x0038 },
    { "NOP",    0x0009 },
    { "RTE",    0x002b },
    { "SETS",   0x0058 },
    { "SETT",   0x0018 },
    { "SLEEP",  0x001b },
    { "FRCHG",  0xfbfd },
    { "FSCHG",  0xf3fd },
    { NULL }
};

const struct Sh4Prog::OpReg Sh4Prog::op_reg[] = {
    { "MOVT",   0x0029, 8, 0x0f00 }, // 0000nnnn00101001
    { "CMP/PZ", 0x4011, 8, 0x0f00 }, // 0100nnnn00010001
    { "CMP/PL", 0x4015, 8, 0x0f00 }, // 0100nnnn00010101
    { "DT",     0x4010, 8, 0x0f00 }, // 0100nnnn00010000
    { "ROTL",   0x4004, 8, 0x0f00 }, // 0100nnnn00000100
    { "ROTR",   0x4005, 8, 0x0f00 }, // 0100nnnn00000101
    { "ROTCL",  0x4024, 8, 0x0f00 }, // 0100nnnn00100100
    { "ROTCR",  0x4025, 8, 0x0f00 }, // 0100nnnn00100101
    { "SHAL",   0x4020, 8, 0x0f00 }, // 0100nnnn00200000
    { "SHAR",   0x4021, 8, 0x0f00 }, // 0100nnnn00100001
    { "SHLL",   0x4000, 8, 0x0f00 }, // 0100nnnn00000000
    { "SHLR",   0x4001, 8, 0x0f00 }, // 0100nnnn00000001
    { "SHLL2",  0x4008, 8, 0x0f00 }, // 0100nnnn00001000
    { "SHLR2",  0x4009, 8, 0x0f00 }, // 0100nnnn00001001
    { "SHLL8",  0x4018, 8, 0x0f00 }, // 0100nnnn00011000
    { "SHLR8",  0x4019, 8, 0x0f00 }, // 0100nnnn00011001
    { "SHLL16", 0x4028, 8, 0x0f00 }, // 0100nnnn00101000
    { "SHLR16", 0x4029, 8, 0x0f00 }, // 0100nnnn00101001
    { "BRAF",   0x0023, 8, 0x0f00 }, // 0000nnnn00100011
    { "BSRF",   0x0003, 8, 0x0f00 }, // 0000nnnn00000011
    { "JMP",    0x402b, 8, 0x0f00 }, // 0100nnnn00101011
    { "JSR",    0x400b, 8, 0x0f00 }, // 0100nnnn00001011
    { NULL }
};

const struct Sh4Prog::OpImm Sh4Prog::op_imm[] = {
    { "CMP/EQ",   0x8800, 0, 0x00ff }, // 10001000iiiiiiii
    { "AND.B",    0xcd00, 0, 0x00ff }, // 11001101iiiiiiii
    { "OR",       0xcb00, 0, 0x00ff }, // 11001011iiiiiiii
    { "OR.B",     0xcf00, 0, 0x00ff }, // 11001111iiiiiiii
    { "TST",      0xc800, 0, 0x00ff }, // 11001000iiiiiiii
    { "TST.B",    0xcc00, 0, 0x00ff }, // 11001100iiiiiiii
    { "XOR",      0xca00, 0, 0x00ff }, // 11001010iiiiiiii
    { "XOR.B",    0xce00, 0, 0x00ff }, // 11001110iiiiiiii
    { "BF",       0x8b00, 0, 0x00ff }, // 10001011dddddddd
    { "BF/S",     0x8f00, 0, 0x00ff }, // 10001111dddddddd
    { "BT",       0x8900, 0, 0x00ff }, // 10001001dddddddd
    { "BT/S",     0x8d00, 0, 0x00ff }, // 10001101dddddddd
    { "BRA",      0xa000, 0, 0x0fff }, // 1010dddddddddddd
    { "BSR",      0xb000, 0, 0x0fff }, // 1011dddddddddddd
    { "TRAPA",    0xc300, 0, 0x00ff }, // 11000011iiiiiiii
    { NULL }
};

const struct Sh4Prog::OpIndReg Sh4Prog::op_indreg[] = {
    { "TAS.B", 0x401b, 0, 0x0f00 }, // 0100nnnn00011011
    { "OCBI",  0x00a3, 8, 0x0f00 }, // 0000nnnn10100011
    { "OCBP",  0x00a3, 8, 0x0f00 }, // 0000nnnn10100011
    { "OCBWB", 0x00b3, 8, 0x0f00 }, // 0000nnnn10110011
    { "PREF",  0x0083, 8, 0x0f00 }, // 0000nnnn10000011
    { NULL }
};

const struct Sh4Prog::OpRegSR Sh4Prog::op_regsr[] = {
    { "LDC", 0x400e, 8, 0x0f00 }, // 0100mmmm00001110
    { NULL }
};

const struct Sh4Prog::OpRegGBR Sh4Prog::op_reggbr[] = {
    { "LDC", 0x401e, 8, 0x0f00 }, // 0100mmmm00011110
    { NULL }
};

const struct Sh4Prog::OpRegVBR Sh4Prog::op_regvbr[] = {
    { "LDC", 0x402e, 8, 0x0f00 }, // 0100mmmm00101110
    { NULL }
};

const struct Sh4Prog::OpRegSSR Sh4Prog::op_regssr[] = {
    { "LDC", 0x403e, 8, 0x0f00 }, // 0100mmmm00111110
    { NULL }
};

const struct Sh4Prog::OpRegSPC Sh4Prog::op_regspc[] = {
    { "LDC", 0x404e, 8, 0x0f00 }, // 0100mmmm01001110
    { NULL }
};

const struct Sh4Prog::OpRegDBR Sh4Prog::op_regdbr[] = {
    { "LDC", 0x40fa, 8, 0x0f00 }, // 0100mmmm11111010
    { NULL }
};

const struct Sh4Prog::OpSRReg Sh4Prog::op_srreg[] = {
    { "STC", 0x0002, 8, 0x0f00 }, // 0000mmmm00000010
    { NULL }
};

const struct Sh4Prog::OpGBRReg Sh4Prog::op_gbrreg[] = {
    { "STC", 0x0012, 8, 0x0f00 }, // 0000mmmm00010010
    { NULL }
};

const struct Sh4Prog::OpVBRReg Sh4Prog::op_vbrreg[] = {
    { "STC", 0x0022, 8, 0x0f00 }, // 0000mmmm00100010
    { NULL }
};

const struct Sh4Prog::OpSSRReg Sh4Prog::op_ssrreg[] = {
    { "STC", 0x0032, 8, 0x0f00 }, // 0000mmmm01000010
    { NULL }
};

const struct Sh4Prog::OpSPCReg Sh4Prog::op_spcreg[] = {
    { "STC", 0x0042, 8, 0x0f00 }, // 0000mmmm01000010
    { NULL }
};

const struct Sh4Prog::OpSGRReg Sh4Prog::op_sgrreg[] = {
    { "STC", 0x003a, 8, 0x0f00 }, // 0000mmmm00111010
    { NULL }
};

const struct Sh4Prog::OpDBRReg Sh4Prog::op_dbrreg[] = {
    { "STC", 0x00fa, 8, 0x0f00 }, // 0000mmmm11111010
    { NULL }
};

const struct Sh4Prog::OpIndIncRegSR Sh4Prog::op_indincregsr[] = {
    { "LDC.L", 0x4007, 8, 0x0f00 }, // 0100mmmm00000111
    { NULL }
};

const struct Sh4Prog::OpIndIncRegGBR Sh4Prog::op_indincreggbr[] = {
    { "LDC.L", 0x4017, 8, 0x0f00 }, // 0100mmmm00010111
    { NULL }
};

const struct Sh4Prog::OpIndIncRegVBR Sh4Prog::op_indincregvbr[] = {
    { "LDC.L", 0x4027, 8, 0x0f00 }, // 0100mmmm00100111
    { NULL }
};

const struct Sh4Prog::OpIndIncRegSSR Sh4Prog::op_indincregssr[] = {
    { "LDC.L", 0x4037, 8, 0x0f00 }, // 0100mmmm00110111
    { NULL }
};

const struct Sh4Prog::OpIndIncRegSPC Sh4Prog::op_indincregspc[] = {
    { "LDC.L", 0x4047, 8, 0x0f00 }, // 0100mmmm01000111
    { NULL }
};

const struct Sh4Prog::OpIndIncRegDBR Sh4Prog::op_indincregdbr[] = {
    { "LDC.L", 0x40f6, 8, 0x0f00 }, // 0100mmmm11110110
    { NULL }
};

const struct Sh4Prog::OpSRIndDecReg Sh4Prog::op_srinddecreg[] = {
    { "STC.L", 0x4003, 8, 0x0f00 }, // 0100nnnn00000011
    { NULL }
};

const struct Sh4Prog::OpGBRIndDecReg Sh4Prog::op_gbrinddecreg[] = {
    { "STC.L", 0x4013, 8, 0x0f00 }, // 0100nnnn00010011
    { NULL }
};

const struct Sh4Prog::OpVBRIndDecReg Sh4Prog::op_vbrinddecreg[] = {
    { "STC.L", 0x4023, 8, 0x0f00 }, // 0100nnnn00100011
    { NULL }
};

const struct Sh4Prog::OpSSRIndDecReg Sh4Prog::op_ssrinddecreg[] = {
    { "STC.L", 0x4033, 8, 0x0f00 }, // 0100nnnn00110011
    { NULL }
};

const struct Sh4Prog::OpSPCIndDecReg Sh4Prog::op_spcinddecreg[] = {
    { "STC.L", 0x4043, 8, 0x0f00 }, // 0100nnnn01000011
    { NULL }
};

const struct Sh4Prog::OpSGRIndDecReg Sh4Prog::op_sgrinddecreg[] = {
    { "STC.L", 0x4032, 8, 0x0f00 }, // 0100nnnn00110010
    { NULL }
};

const struct Sh4Prog::OpDBRIndDecReg Sh4Prog::op_dbrinddecreg[] = {
    { "STC.L", 0x40f2, 8, 0x0f00 }, // 0100nnnn11110010
    { NULL }
};

// INST #imm,Rn
const struct Sh4Prog::OpImmReg Sh4Prog::op_immreg[] = {
    { "MOV", 0xe000, 0, 0x00ff, 8, 0x0f00 }, // 1110nnnniiiiiiii
    { "ADD", 0x7000, 0, 0x00ff, 8, 0x0f00 }, // 0111nnnniiiiiiii
    { NULL }
};

const struct Sh4Prog::OpPcRelDisp Sh4Prog::op_pcreldisp[] = {
    { "MOV.W", 0x9000, 0, 0xff, 8, 0x0f00 },    // 1001nnnndddddddd
    { "MOV.L", 0xd000, 0, 0xff, 8, 0x0f00 },    // 1101nnnndddddddd
    { NULL }
};

const struct Sh4Prog::OpRegReg Sh4Prog::op_regreg[] = {
    { "MOV",     0x6003, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm0011
    { "SWAP.B",  0x6008, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm1000
    { "SWAP.W",  0x6009, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm1001
    { "XTRCT",   0x200d, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm1101
    { "ADD",     0x300c, 4, 0x00f0, 8, 0x0f00 }, // 0111nnnnmmmm1100
    { "ADDC",    0x300e, 4, 0x00f0, 8, 0x0f00 }, // 0111nnnnmmmm1110
    { "ADDV",    0x300f, 4, 0x00f0, 8, 0x0f00 }, // 0111nnnnmmmm1111
    { "CMP/EQ",  0x3000, 4, 0x00f0, 8, 0x0f00 }, // 0011nnnnmmmm0000
    { "CMP/HS",  0x3002, 4, 0x00f0, 8, 0x0f00 }, // 0011nnnnmmmm0010
    { "CMP/GE",  0x3003, 4, 0x00f0, 8, 0x0f00 }, // 0011nnnnmmmm0011
    { "CMP/HI",  0x3006, 4, 0x00f0, 8, 0x0f00 }, // 0011nnnnmmmm0110
    { "CMP/GT",  0x3007, 4, 0x00f0, 8, 0x0f00 }, // 0011nnnnmmmm0111
    { "CMP/STR", 0x200c, 4, 0x00f0, 8, 0x0f00 }, // 0010nnnnmmmm1100
    { "DIV1",    0x3004, 4, 0x00f0, 8, 0x0f00 }, // 0011nnnnmmmm0100
    { "DIV0S",   0x2007, 4, 0x00f0, 8, 0x0f00 }, // 0010nnnnmmmm0111
    { "DMULS.L", 0x300d, 4, 0x00f0, 8, 0x0f00 }, // 0011nnnnmmmm1101
    { "DMULU.L", 0x3005, 4, 0x00f0, 8, 0x0f00 }, // 0011nnnnmmmm0101
    { "EXTS.B",  0x600e, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm1110
    { "EXTS.W",  0x600f, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm1111
    { "EXTU.B",  0x600c, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm1100
    { "EXTU.W",  0x600d, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm1101
    { "MUL.L",   0x0007, 4, 0x00f0, 8, 0x0f00 }, // 0000nnnnmmmm0111
    { "MULS.W",  0x200f, 4, 0x00f0, 8, 0x0f00 }, // 0010nnnnmmmm1111
    { "MULU.W",  0x200e, 4, 0x00f0, 8, 0x0f00 }, // 0010nnnnmmmm1110
    { "NEG",     0x600b, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm1011
    { "NEGC",    0x600a, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm1010
    { "SUB",     0x3008, 4, 0x00f0, 8, 0x0f00 }, // 0011nnnnmmmm1000
    { "SUBC",    0x300a, 4, 0x00f0, 8, 0x0f00 }, // 0011nnnnmmmm1010
    { "SUBV",    0x300b, 4, 0x00f0, 8, 0x0f00 }, // 0011nnnnmmmm1011
    { "AND",     0x2009, 4, 0x00f0, 8, 0x0f00 }, // 0010nnnnmmmm1001
    { "NOT",     0x6007, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm0111
    { "OR",      0x200b, 4, 0x00f0, 8, 0x0f00 }, // 0010nnnnmmmm1011
    { "TST",     0x2008, 4, 0x00f0, 8, 0x0f00 }, // 0010nnnnmmmm1000
    { "XOR",     0x200a, 4, 0x00f0, 8, 0x0f00 }, // 0010nnnnmmmm1010
    { "SHAD",    0x400c, 4, 0x00f0, 8, 0x0f00 }, // 0100nnnnmmmm1100
    { "SHLD",    0x400d, 4, 0x00f0, 8, 0x0f00 }, // 0100nnnnmmmm1101
    { NULL }
};

const struct Sh4Prog::OpRegBank Sh4Prog::op_regbank[] = {
    { "LDC", 0x408e, 8, 0x0f00, 4, 0x0070 }, // 0100mmmm1nnn1110
    { NULL }
};

const struct Sh4Prog::OpBankReg Sh4Prog::op_bankreg[] = {
    { "LDC.L", 0x4087, 8, 0x0f00, 4, 0x0070 }, // 0100mmmm1nnn0111
    { "STC",   0x0082, 8, 0x0f00, 4, 0x0070 }, // 0000nnnn1mmm0010
    { NULL }
};

const struct Sh4Prog::OpBankIndDecReg Sh4Prog::on_bankinddecreg[] = {
    { "STC.L", 0x4083, 4, 0x0070, 8, 0x0f00 }, // 0100nnnn1mmm0011
    { NULL }
};

const struct Sh4Prog::OpRegMach Sh4Prog::op_regmach[] = {
    { "LDS", 0x400a, 8, 0x0f00 }, // 0100mmmm00001010
    { NULL }
};

const struct Sh4Prog::OpRegMacl Sh4Prog::op_regmacl[] = {
    { "LDS", 0x401a, 8, 0x0f00 }, // 0100mmmm00011010
    { NULL }
};

const struct Sh4Prog::OpMachReg Sh4Prog::op_machreg[] = {
    { "STS", 0x000a, 8, 0x0f00 }, // 0000nnnn00001010
    { NULL }
};

const struct Sh4Prog::OpMaclReg Sh4Prog::op_maclreg[] = {
    { "STS", 0x001a, 8, 0x0f00 }, // 0000nnnn00011010
    { NULL }
};

const struct Sh4Prog::OpRegPr Sh4Prog::op_regpr[] = {
    { "LDS", 0x402a, 8, 0x0f00 }, // 0100mmmm00011010
    { NULL }
};

const struct Sh4Prog::OpPrReg Sh4Prog::op_prreg[] = {
    { "STS", 0x002a, 8, 0x0f00 }, // 0000nnnn00101010
    { NULL }
};

const struct Sh4Prog::OpIndIncRegMach Sh4Prog::op_indincregmach[] = {
    { "LDS.L", 0x4006, 8, 0x0f00 }, // 0100mmmm00000110
    { NULL }
};

const struct Sh4Prog::OpIndIncRegMacl Sh4Prog::op_indincregmacl[] = {
    { "LDS.L", 0x4016, 8, 0x0f00 }, // 0100mmmm00010110
    { NULL }
};

const struct Sh4Prog::OpMachIndDecReg Sh4Prog::op_machinddecreg[] = {
    { "STS.L", 0x4002, 8, 0x0f00 },
    { NULL }
};

const struct Sh4Prog::OpMaclIndDecReg Sh4Prog::op_maclinddecreg[] = {
    { "STS.L", 0x4012, 8, 0x0f00 },
    { NULL }
};

const struct Sh4Prog::OpIndIncRegPr Sh4Prog::op_indincregpr[] = {
    { "LDS.L", 0x4026, 8, 0x0f00 }, // 0100mmmm00100110
    { NULL }
};

const struct Sh4Prog::OpPrIndDecReg Sh4Prog::op_prinddecreg[] = {
    { "STS.L", 0x4022, 8, 0x0f00 }, // 0100nnnn00100010
    { NULL }
};

const struct Sh4Prog::OpRegIndReg Sh4Prog::op_regindreg[] = {
    { "MOV.B", 0x2000, 4, 0x00f0, 8, 0x0f00 }, // 0010nnnnmmmm0000
    { "MOV.W", 0x2001, 4, 0x00f0, 8, 0x0f00 }, // 0010nnnnmmmm0001
    { "MOV.L", 0x2002, 4, 0x00f0, 8, 0x0f00 }, // 0010nnnnmmmm0010
    { NULL }
};

const struct Sh4Prog::OpIndRegReg Sh4Prog::op_indregreg[] = {
    { "MOV.B", 0x6000, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm0000
    { "MOV.W", 0x6001, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm0001
    { "MOV.L", 0x6002, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm0010
    { NULL }
};

const struct Sh4Prog::OpRegIndDecReg Sh4Prog::op_reginddecreg[] = {
    { "MOV.B", 0x2004, 4, 0x00f0, 8, 0x0f00 }, // 0010nnnnmmmm0100
    { "MOV.W", 0x2005, 4, 0x00f0, 8, 0x0f00 }, // 0010nnnnmmmm0101
    { "MOV.L", 0x2006, 4, 0x00f0, 8, 0x0f00 }, // 0010nnnnmmmm0110
    { NULL }
};

const struct Sh4Prog::OpIndIncRegReg Sh4Prog::op_indincregreg[] = {
    { "MOV.B", 0x6004, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm0100
    { "MOV.W", 0x6005, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm0101
    { "MOV.L", 0x6006, 4, 0x00f0, 8, 0x0f00 }, // 0110nnnnmmmm0110
    { NULL }
};

const struct Sh4Prog::OpIndIncRegIndIncRegReg
Sh4Prog::op_indincregindincreg[] = {
    { "MAC.L", 0x000f, 4, 0x00f0, 8, 0x0f00 }, // 0000nnnnmmmm1111
    { "MAC.W", 0x400f, 4, 0x00f0, 8, 0x0f00 }, // 0100nnnnmmmm1111
    { NULL }
};

const struct Sh4Prog::OpDispRegDst Sh4Prog::op_dispregdst[] = {
    { "MOV.B", 0x8000, 0, 0x000f, 4, 0x00f0 }, // 10000000nnnndddd
    { "MOV.W", 0x8100, 0, 0x000f, 4, 0x00f0 }, // 10000001nnnndddd
    { NULL }
};

const struct Sh4Prog::OpRegDispReg Sh4Prog::op_regdispreg[] = {
    { "MOV.L", 0x1000, 4, 0x00f0, 0, 0x000f, 8, 0x0f00 }, // 0001nnnnmmmmdddd
    { NULL }
};

const struct Sh4Prog::OpDispRegSrc Sh4Prog::op_dispregsrc[] = {
    { "MOV.B", 0x8400, 0, 0x000f, 4, 0x00f0 }, // 10000100mmmmdddd
    { "MOV.W", 0x8500, 0, 0x000f, 4, 0x00f0 }, // 10000101mmmmdddd
    { NULL }
};

const struct Sh4Prog::OpDispRegReg Sh4Prog::op_dispregreg[] = {
    { "MOV.L", 0x5000, 4, 0x00f0, 0, 0x000f, 8, 0x0f00 }, // 0101nnnnmmmmdddd
    { NULL }
};

const struct Sh4Prog::OpRegRegPlusR0 Sh4Prog::op_regregplusr0[] = {
    { "MOV.B", 0x0004, 4, 0x00f0, 8, 0x0f00 }, // 0000nnnnmmmm0100
    { "MOV.W", 0x0005, 4, 0x00f0, 8, 0x0f00 }, // 0000nnnnmmmm0101
    { "MOV.L", 0x0006, 4, 0x00f0, 8, 0x0f00 }, // 0000nnnnmmmm0110
    { NULL }
};

const struct Sh4Prog::OpRegPlusR0Reg Sh4Prog::op_regplusr0reg[] = {
    { "MOV.B", 0x000c, 4, 0x00f0, 8, 0x0f00 }, // 0000nnnnmmmm1100
    { "MOV.W", 0x000d, 4, 0x00f0, 8, 0x0f00 }, // 0000nnnnmmmm1101
    { "MOV.L", 0x000e, 4, 0x00f0, 8, 0x0f00 }, // 0000nnnnmmmm1110
    { NULL }
};

const struct Sh4Prog::OpR0DispPlusGBR Sh4Prog::op_r0dispplusgbr[] = {
    { "MOV.B", 0xc000, 0, 0x00ff }, // 11000000dddddddd
    { "MOV.W", 0xc100, 0, 0x00ff }, // 11000001dddddddd
    { "MOV.L", 0xc200, 0, 0x00ff }, // 11000010dddddddd
    { NULL }
};

const struct Sh4Prog::OpDispPlusGBRR0 Sh4Prog::op_dispplusgbrr0[] = {
    { "MOV.B", 0xc400, 0, 0x00ff }, // 11000100dddddddd
    { "MOV.W", 0xc500, 0, 0x00ff }, // 11000101dddddddd
    { "MOV.L", 0xc600, 0, 0x00ff }, // 11000110dddddddd
    { NULL }
};

const struct Sh4Prog::OpDispPlusPCR0 Sh4Prog::op_disppluspcr0[] = {
    { "MOVA", 0xc700, 0, 0x00ff }, // 11000111dddddddd
    { NULL }
};

const struct Sh4Prog::OpR0IndReg Sh4Prog::op_r0indreg[] = {
    { "MOVCA.L", 0x00c3, 8, 0x0f00 },
    { NULL }
};

inst_t Sh4Prog::assemble_op_noargs(const std::string& inst) {
    OpNoArgs const *op_tbl = op_noargs;

    while (op_tbl->inst) {
        if (std::string(op_tbl->inst) == inst)
            return op_tbl->opcode;

        op_tbl++;
    }

    throw ParseError(("Unrecognized instruction " + inst).c_str());
}

void Sh4Prog::add_label(const std::string& lbl) {
    syms[lbl] = prog.size() - 1;
}
