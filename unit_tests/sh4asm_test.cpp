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
#include <cstdlib>

#include <boost/cstdint.hpp>
#include <boost/tokenizer.hpp>

#include "tool/sh4asm/sh4asm.hpp"
#include "RandGenerator.hpp"
#include "BaseException.hpp"

/*
 * This function tests assembler and disassembler functionality on the given
 * string by first assembling it, then disassessembling it then reassembling it
 * and checking the results of the two assembly operations to see if they are
 * equal (they should be).  The instructions are only compared in binary form
 * because there is not a 1:1 mapping between text-based assembly and binary
 * instructions (whitespace, hex/decimal, double-precision floating point
 * instructions that share opcodes with single-precision floating point
 * instructions, etc.).
 *
 * Of course this doesn't technically test that the assembler is correct, but
 * if it's idempotent then it probably is correct.
 *
 * This function returns true on test-pass, and false on test-fail
 */
bool test_inst(std::string const& inst) {
    Sh4Prog prog;
    inst_t inst1, inst2;
    std::string inst1_as_txt;

    if (inst.at(inst.size() - 1) != '\n') {
        // maybe an exception would be more appropriate here, idgaf
        std::cout << "ERROR: instructions need to end with newlines (this " <<
            "is a problem with the test!)" << std::endl;
        return false;
    }

    std::cout << "Testing \"" << inst.substr(0, inst.size() - 1) << "\"..." << std::endl;

    inst1 = prog.assemble_line(inst);
    inst1_as_txt = prog.disassemble_line(inst1);
    inst2 = prog.assemble_line(inst1_as_txt);

    if (inst1 == inst2) {
        std::cout << "success!" << std::endl;
        return true;
    }

    std::cout << "Failure: expected " << std::hex << inst1 << " but got " <<
        std::hex << inst2 << std::endl;
    return false;
}

/*
 * <N> means to generate a random N-bit integer.
 * Obviously N cannot be greater than 16
 */
char const *insts_to_test[] = {
    "DIVOU",
    "RTS",
    "CLRMAC",
    "CLRS",
    "CLRT",
    "LDTLB",
    "NOP",
    "RTE",
    "SETS",
    "SETT",
    "SLEEP",
    "FRCHG",
    "FSCHG",
    "MOVT R<4>",
    "CMP/PZ R<4>",
    "CMP/PL R<4>",
    "DT R<4>",
    "ROTL R<4>",
    "ROTR R<4>",
    "ROTCL R<4>",
    "ROTCR R<4>",
    "SHAL R<4>",
    "SHAR R<4>",
    "SHLL R<4>",
    "SHLR R<4>",
    "SHLL2 R<4>",
    "SHLR2 R<4>",
    "SHLL8 R<4>",
    "SHLR8 R<4>",
    "SHLL16 R<4>",
    "SHLR16 R<4>",
    "BRAF R<4>",
    "BSRF R<4>",
    "CMP/EQ #<8>, R0",
    "AND.B #<8>, @(R0, GBR)",
    "AND #<8>, R0",
    "OR.B #<8>, @(R0, GBR)",
    "OR #<8>, R0",
    "TST #<8>, R0",
    "TST.B #<8>, @(R0, GBR)",
    "XOR #<8>, R0",
    "XOR.B #<8>, @(R0, GBR)",
    "BF <8>",
    "BF/S <8>",
    "BT <8>",
    "BT/S <8>",
    "BRA <12>",
    "BSR <12>",
    "TRAPA #<8>",
    "TAS.B @R<4>",
    "OCBI @R<4>",
    "OCBP @R<4>",
    "PREF @R<4>",
    "JMP @R<4>",
    "JSR @R<4>",
    "LDC R<4>, SR",
    "LDC R<4>, GBR",
    "LDC R<4>, VBR",
    "LDC R<4>, SSR",
    "LDC R<4>, SPC",
    "LDC R<4>, DBR",
    "STC SR, R<4>",
    "STC GBR, R<4>",
    "STC VBR, R<4>",
    "STC SSR, R<4>",
    "STC SPC, R<4>",
    "STC SGR, R<4>",
    "STC DBR, R<4>",
    "LDC.L @R<4>+, SR",
    "LDC.L @R<4>+, GBR",
    "LDC.L @R<4>+, VBR",
    "LDC.L @R<4>+, SSR",
    "LDC.L @R<4>+, SPC",
    "LDC.L @R<4>+, DBR",
    "STC.L SR, @-R<4>",
    "STC.L GBR, @-R<4>",
    "STC.L VBR, @-R<4>",
    "STC.L SSR, @-R<4>",
    "STC.L SPC, @-R<4>",
    "STC.L SGR, @-R<4>",
    "STC.L DBR, @-R<4>",
    "MOV #<8>, R<4>",
    "ADD #<8>, R<4>",
    "MOV.W @(<8>, PC), R<4>",
    "MOV.L @(<8>, PC), R<4>",
    "MOV R<4>, R<4>",
    "SWAP.B R<4>, R<4>",
    "SWAP.W R<4>, R<4>",
    "XTRCT R<4>, R<4>",
    "ADD R<4>, R<4>",
    "ADDC R<4>, R<4>",
    "ADDV R<4>, R<4>",
    "CMP/EQ R<4>, R<4>",
    "CMP/HS R<4>, R<4>",
    "CMP/GE R<4>, R<4>",
    "CMP/HI R<4>, R<4>",
    "CMP/GT R<4>, R<4>",
    "CMP/STR R<4>, R<4>",
    "DIV1 R<4>, R<4>",
    "DIV0S R<4>, R<4>",
    "DMULS.L R<4>, R<4>",
    "DMULU.L R<4>, R<4>",
    "EXTS.B R<4>, R<4>",
    "EXTS.W R<4>, R<4>",
    "EXTU.B R<4>, R<4>",
    "EXTU.W R<4>, R<4>",
    "MUL.L R<4>, R<4>",
    "MULS.W R<4>, R<4>",
    "MULU.W R<4>, R<4>",
    "NEG R<4>, R<4>",
    "NEGC R<4>, R<4>",
    "SUB R<4>, R<4>",
    "SUBC R<4>, R<4>",
    "SUBV R<4>, R<4>",
    "AND R<4>, R<4>",
    "NOT R<4>, R<4>",
    "OR R<4>, R<4>",
    "TST R<4>, R<4>",
    "XOR R<4>, R<4>",
    "SHAD R<4>, R<4>",
    "SHLD R<4>, R<4>",
    "LDC R<4>, R<3>_BANK",
    "LDC.L @R<4>+, R<3>_BANK",
    "STC R<3>_BANK, R<4>",
    "STC.L R<3>_BANK, @-R<4>",
    "LDS R<4>, MACH",
    "LDS R<4>, MACL",
    "STS MACH, R<4>",
    "STS MACL, R<4>",
    "LDS R<4>, PR",
    "STS PR, R<4>",
    "LDS.L @R<4>+, MACH",
    "LDS.L @R<4>+, MACL",
    "STS.L MACH, @-R<4>",
    "STS.L MACL, @-R<4>",
    "LDS.L @R<4>+, PR",
    "STS.L PR, @-R<4>",
    "MOV.B R<4>, @R<4>",
    "MOV.W R<4>, @R<4>",
    "MOV.L R<4>, @R<4>",
    "MOV.B @R<4>, R<4>",
    "MOV.W @R<4>, R<4>",
    "MOV.L @R<4>, R<4>",
    "MOV.B R<4>, @-R<4>",
    "MOV.W R<4>, @-R<4>",
    "MOV.L R<4>, @-R<4>",
    "MOV.B @R<4>+, R<4>",
    "MOV.W @R<4>+, R<4>",
    "MOV.L @R<4>+, R<4>",
    "MAC.L @R<4>+, @R<4>+",
    "MAC.W @R<4>+, @R<4>+",
    "MOV.B R0, @(<4>, R<4>)",
    "MOV.W R0, @(<4>, R<4>)",
    "MOV.L R<4>, @(<4>, R<4>)",
    "MOV.B @(<4>, R<4>), R0",
    "MOV.W @(<4>, R<4>), R0",
    "MOV.L @(<4>, R<4>), R<4>",
    "MOV.B R<4>, @(R0, R<4>)",
    "MOV.W R<4>, @(R0, R<4>)",
    "MOV.L R<4>, @(R0, R<4>)",
    "MOV.B @(R0, R<4>), R<4>",
    "MOV.W @(R0, R<4>), R<4>",
    "MOV.L @(R0, R<4>), R<4>",
    "MOV.B R0, @(<8>, GBR)",
    "MOV.W R0, @(<8>, GBR)",
    "MOV.L R0, @(<8>, GBR)",
    "MOV.B @(<8>, GBR), R0",
    "MOV.W @(<8>, GBR), R0",
    "MOV.L @(<8>, GBR), R0",
    "MOVA @(<8>, PC), R0",
    "MOVCA.L R0, @R<4>",
    "FLDI0 FR<4>",
    "FLDI1 FR<4>",
    "FMOV FR<4>, FR<4>",
    "FMOV.S @R<4>, FR<4>",
    "FMOV.S @(R0, R<4>), FR<4>",
    "FMOV.S @R<4>+, FR<4>",
    "FMOV.S FR<4>, @R<4>",
    "FMOV.S FR<4>, @-R<4>",
    "FMOV.S FR<4>, @(R0, R<4>)",
    "FMOV DR<3,2>, DR<3,2>",
    "FMOV @R<4>, DR<3,2>",
    "FMOV @(R0, R<4>), DR<3,2>",
    "FMOV @R<4>+, DR<3,2>",
    "FMOV DR<3,2>, @R<4>",
    "FMOV DR<3,2>, @-R<4>",
    "FMOV DR<3,2>, @(R0, R<4>)",
    "FLDS FR<4>, FPUL",
    "FSTS FPUL, FR<4>",
    "FABS FR<4>",
    "FADD FR<4>, FR<4>",
    "FCMP/EQ FR<4>, FR<4>",
    "FCMP/GT FR<4>, FR<4>",
    "FDIV FR<4>, FR<4>",
    "FLOAT FPUL, FR<4>",
    "FMAC FR0, FR<4>, FR<4>",
    "FMUL FR<4>, FR<4>",
    "FNEG FR<4>",
    "FSQRT FR<4>",
    "FSUB FR<4>, FR<4>",
    "FTRC FR<4>, FPUL",
    "FABS DR<3,2>",
    "FADD DR<3,2>, DR<3,2>",
    "FCMP/EQ DR<3,2>, DR<3,2>",
    "FCMP/GT DR<3,2>, DR<3,2>",
    "FDIV DR<3,2>, DR<3,2>",
    "FCNVDS DR<3,2>, FPUL",
    "FCNVSD FPUL, DR<3,2>",
    "FLOAT FPUL, DR<3,2>",
    "FMUL DR<3,2>, DR<3,2>",
    "FNEG DR<3,2>",
    "FSQRT DR<3,2>",
    "FSUB DR<3,2>, DR<3,2>",
    "FTRC DR<3,2>, FPUL",
    "LDS R<4>, FPSCR",
    "LDS R<4>, FPUL",
    "LDS.L @R<4>+, FPSCR",
    "LDS.L @R<4>+, FPUL",
    "STS FPSCR, R<4>",
    "STS FPUL, R<4>",
    "STS.L FPSCR, @-R<4>",
    "STS.L FPUL, @-R<4>",
    "FMOV DR<3,2>, XD<3,2>",
    "FMOV XD<3,2>, DR<3,2>",
    "FMOV XD<3,2>, XD<3,2>",
    "FMOV @R<4>, XD<3,2>",
    "FMOV @R<4>+, XD<3,2>",
    "FMOV @(R0, R<4>), XD<3,2>",
    "FMOV XD<3,2>, @R<4>",
    "FMOV XD<3,2>, @-R<4>",
    "FMOV XD<3,2>, @(R0, R<4>)",
    "FIPR FV<2,4>, FV<2,4>",
    "FTRV XMTRX, FV<2,4>",
    "FRCHG",
    "FSCHG",
    NULL
};

// lookup table for n-bit integer masks
static const unsigned MASK_MAX = 16;
static unsigned masks[1 + MASK_MAX] = {
    0,
    0x1,
    0x3,
    0x7,
    0xf,
    0x1f,
    0x3f,
    0x7f,
    0xff,
    0x1ff,
    0x3ff,
    0x7ff,
    0xfff,
    0x1fff,
    0x3fff,
    0x7fff,
    0xffff
};

typedef boost::char_separator<char> CharSeparator;
typedef boost::tokenizer<CharSeparator> Tokenizer ;
template <class RandGen>
std::string process_inst_str(RandGen *gen, std::string inst) {
    std::stringstream actual;
    CharSeparator sep("<>");
    Tokenizer tok(inst, sep);
    bool pick_val = false;

    /*
     * instruction template format:
     *     <N> - random N-bit integer
     *     <N,M> - random N-bit interger multiplied by M
     */
    for (Tokenizer::iterator it = tok.begin(); it != tok.end();
         it++, pick_val = !pick_val) {
        if (pick_val) {
            size_t comma_idx = it->find_first_of(',');
            std::string val_str, scale_str;
            if (comma_idx == std::string::npos) {
                val_str = it->c_str();
                scale_str = "1";
            } else {
                val_str = it->substr(0, comma_idx);
                scale_str = it->substr(comma_idx);
            }
            unsigned n_bits = atoi(val_str.c_str());
            unsigned scale = atoi(scale_str.c_str());
            if (n_bits > MASK_MAX)
                throw InvalidParamError("Too many bits in instruction mask!");
            boost::uint32_t val_mask = masks[n_bits];
            boost::uint32_t rand_val = gen->pick_val(0) & val_mask;

            actual << (rand_val * scale);
        } else {
            actual << *it;
        }
    }

    return actual.str();
}

int test_all_insts(unsigned seed) {
    unsigned n_tests = 0;
    unsigned n_success = 0;
    char const **inst = insts_to_test;

    RandGenerator<boost::uint32_t> gen(seed);
    gen.reset();

    while (*inst) {
        std::string processed(process_inst_str(&gen, std::string(*inst)));

        if (test_inst(processed + "\n"))
            n_success++;
        n_tests++;

        inst++;
    }

    double percent = 100.0 * double(n_success) / double(n_tests);
    std::cout << n_tests << " tests run - " << n_success <<
        " successes " << "(" << percent << "%)" << std::endl;

    if (n_success == n_tests)
        return 0;
    return 1;
}

int main(int argc, char **argv) {
    unsigned int seed = time(NULL);
    int opt;

    while ((opt = getopt(argc, argv, "s:")) > 0) {
        if (opt == 's')
            seed = atoi(optarg);
    }

    return test_all_insts(seed);
}
