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

#include <string>
#include <vector>
#include <sstream>

#include <boost/shared_ptr.hpp>

#include "types.hpp"

class Pattern;

typedef boost::shared_ptr<Pattern> PtrnPtr;
typedef std::vector<PtrnPtr> PtrnList;

typedef std::string Token;
typedef std::vector<Token> TokList;

PtrnList get_patterns();

template <class IteratorType>
static bool safe_to_advance(IteratorType begin, IteratorType end, int adv) {
    /*
     * This is certainly the "wrong" way to do this and I would fail any job
     * interview where I submitted this code, but I'm not at a job interview
     * and I'm not in the mood for algebra either.
     */
    while (adv) {
        if (begin == end)
            return false;
        begin++;
        adv--;
    }
    return true;
}

class Pattern {
public:
    virtual ~Pattern() {
    }

    /*
     * matches returns how far to advance rbegin.
     * If the return is <= 0, then there was no match.
     */
    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) = 0;

    virtual inst_t assemble() const {
        return 0;
    }

    /* returns the pattern in string-form */
    virtual Token disassemble() const = 0;
};

class TxtPattern : public Pattern {
public:
    std::string txt;

    TxtPattern(char const *txt) {
        this->txt = std::string(txt);
    }

    TxtPattern(const std::string& txt) {
        this->txt = txt;
    }

    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        if (*rbegin == txt) {
            return 1;
        }

        return 0;
    }

    virtual Token disassemble() const {
        return Token(txt);
    }
};

#define INST_PTRN(name) Ptrn_ ## name
#define DECL_INST_PTRN(name, inst)              \
    class INST_PTRN(name) : public TxtPattern { \
    public:                                     \
        INST_PTRN(name)() : TxtPattern(inst) {  \
        }                                       \
    }

DECL_INST_PTRN(and, "AND");
DECL_INST_PTRN(add, "ADD");
DECL_INST_PTRN(addc, "ADDC");
DECL_INST_PTRN(addv, "ADDV");
DECL_INST_PTRN(andb, "AND.B");
DECL_INST_PTRN(bf, "BF");
DECL_INST_PTRN(bfs, "BF/S");
DECL_INST_PTRN(bra, "BRA");
DECL_INST_PTRN(braf, "BRAF");
DECL_INST_PTRN(bsr, "BSR");
DECL_INST_PTRN(bsrf, "BSRF");
DECL_INST_PTRN(bt, "BT");
DECL_INST_PTRN(bts, "BT/S");
DECL_INST_PTRN(clrmac, "CLRMAC");
DECL_INST_PTRN(clrs, "CLRS");
DECL_INST_PTRN(clrt, "CLRT");
DECL_INST_PTRN(cmpeq, "CMP/EQ");
DECL_INST_PTRN(cmpge, "CMP/GE");
DECL_INST_PTRN(cmpgt, "CMP/GT");
DECL_INST_PTRN(cmphi, "CMP/HI");
DECL_INST_PTRN(cmphs, "CMP/HS");
DECL_INST_PTRN(cmppz, "CMP/PZ");
DECL_INST_PTRN(cmppl, "CMP/PL");
DECL_INST_PTRN(cmpstr, "CMP/STR");
DECL_INST_PTRN(div1, "DIV1");
DECL_INST_PTRN(div0s, "DIV0S");
DECL_INST_PTRN(divou, "DIVOU");
DECL_INST_PTRN(dmulsl, "DMULS.L");
DECL_INST_PTRN(dmulul, "DMULU.L");
DECL_INST_PTRN(dt, "DT");
DECL_INST_PTRN(extsb, "EXTS.B");
DECL_INST_PTRN(extsw, "EXTS.W");
DECL_INST_PTRN(extub, "EXTU.B");
DECL_INST_PTRN(extuw, "EXTU.W");
DECL_INST_PTRN(fabs, "FABS");
DECL_INST_PTRN(fadd, "FADD");
DECL_INST_PTRN(fcmpeq, "FCMP/EQ");
DECL_INST_PTRN(fcmpgt, "FCMP/GT");
DECL_INST_PTRN(fcnvds, "FCNVDS");
DECL_INST_PTRN(fcnvsd, "FCNVSD");
DECL_INST_PTRN(fdiv, "FDIV");
DECL_INST_PTRN(fipr, "FIPR");
DECL_INST_PTRN(fldi0, "FLDI0");
DECL_INST_PTRN(fldi1, "FLDI1");
DECL_INST_PTRN(flds, "FLDS");
DECL_INST_PTRN(float, "FLOAT");
DECL_INST_PTRN(fmac, "FMAC");
DECL_INST_PTRN(fmov, "FMOV");
DECL_INST_PTRN(fmovs, "FMOV.S");
DECL_INST_PTRN(fmul, "FMUL");
DECL_INST_PTRN(fneg, "FNEG");
DECL_INST_PTRN(frchg, "FRCHG");
DECL_INST_PTRN(fschg, "FSCHG");
DECL_INST_PTRN(fsqrt, "FSQRT");
DECL_INST_PTRN(fsts, "FSTS");
DECL_INST_PTRN(fsub, "FSUB");
DECL_INST_PTRN(ftrc, "FTRC");
DECL_INST_PTRN(ftrv, "FTRV");
DECL_INST_PTRN(jmp, "JMP");
DECL_INST_PTRN(jsr, "JSR");
DECL_INST_PTRN(ldc, "LDC");
DECL_INST_PTRN(lds, "LDS");
DECL_INST_PTRN(ldsl, "LDS.L");
DECL_INST_PTRN(ldcl, "LDC.L");
DECL_INST_PTRN(ldtlb, "LDTLB");
DECL_INST_PTRN(macl, "MAC.L");
DECL_INST_PTRN(macw, "MAC.W");
DECL_INST_PTRN(mov, "MOV");
DECL_INST_PTRN(mova, "MOVA");
DECL_INST_PTRN(movb, "MOV.B");
DECL_INST_PTRN(movcal, "MOVCA.L");
DECL_INST_PTRN(movl, "MOV.L");
DECL_INST_PTRN(movw, "MOV.W");
DECL_INST_PTRN(movt, "MOVT");
DECL_INST_PTRN(mull, "MUL.L");
DECL_INST_PTRN(mulsw, "MULS.W");
DECL_INST_PTRN(muluw, "MULU.W");
DECL_INST_PTRN(neg, "NEG");
DECL_INST_PTRN(negc, "NEGC");
DECL_INST_PTRN(nop, "NOP");
DECL_INST_PTRN(not, "NOT");
DECL_INST_PTRN(ocbi, "OCBI");
DECL_INST_PTRN(ocbp, "OCBP");
DECL_INST_PTRN(ocbwb, "OCBWB");
DECL_INST_PTRN(or, "OR");
DECL_INST_PTRN(orb, "OR.B");
DECL_INST_PTRN(pref, "PREF");
DECL_INST_PTRN(rotl, "ROTL");
DECL_INST_PTRN(rotr, "ROTR");
DECL_INST_PTRN(rotcl, "ROTCL");
DECL_INST_PTRN(rotcr, "ROTCR");
DECL_INST_PTRN(rte, "RTE");
DECL_INST_PTRN(rts, "RTS");
DECL_INST_PTRN(sets, "SETS");
DECL_INST_PTRN(sett, "SETT");
DECL_INST_PTRN(shad, "SHAD");
DECL_INST_PTRN(shld, "SHLD");
DECL_INST_PTRN(shal, "SHAL");
DECL_INST_PTRN(shar, "SHAR");
DECL_INST_PTRN(shll, "SHLL");
DECL_INST_PTRN(shlr, "SHLR");
DECL_INST_PTRN(shll2, "SHLL2");
DECL_INST_PTRN(shlr2, "SHLR2");
DECL_INST_PTRN(shll8, "SHLL8");
DECL_INST_PTRN(shlr8, "SHLR8");
DECL_INST_PTRN(shll16, "SHLL16");
DECL_INST_PTRN(shlr16, "SHLR16");
DECL_INST_PTRN(sleep, "SLEEP");
DECL_INST_PTRN(stc, "STC");
DECL_INST_PTRN(stcl, "STC.L");
DECL_INST_PTRN(sts, "STS");
DECL_INST_PTRN(stsl, "STS.L");
DECL_INST_PTRN(sub, "SUB");
DECL_INST_PTRN(subc, "SUBC");
DECL_INST_PTRN(subv, "SUBV");
DECL_INST_PTRN(swapb, "SWAP.B");
DECL_INST_PTRN(swapw, "SWAP.W");
DECL_INST_PTRN(tasb, "TAS.B");
DECL_INST_PTRN(tst, "TST");
DECL_INST_PTRN(tstb, "TST.B");
DECL_INST_PTRN(trapa, "TRAPA");
DECL_INST_PTRN(xor, "XOR");
DECL_INST_PTRN(xorb, "XOR.B");
DECL_INST_PTRN(xtrct, "XTRCT");

template <class Inst, inst_t BIN>
struct NoArgOperator : public Pattern {
    Inst inst;

    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        return inst.matches(rbegin, rend);
    }

    inst_t assemble() const {
        return (inst_t)BIN;
    }

    Token disassemble() const {
        return inst.disassemble() + "\n";
    }
};

template <class Inst, class SrcInput, int BIN, int SRC_SHIFT>
struct UnaryOperator : public Pattern {
    Inst inst;

    SrcInput src;

    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        int adv;
        int adv_total = 0;

        if ((adv = src.matches(rbegin, rend)) == 0)
            return 0;

        if (safe_to_advance(rbegin, rend, adv)) {
            rbegin += adv;
            adv_total += adv;
        } else
            return 0;

        if ((adv = inst.matches(rbegin, rend)) != 0) {
            adv_total += adv;
            return adv_total;
        }

        return 0;
    }

    inst_t assemble() const {
        return BIN | (src.assemble() << SRC_SHIFT);
    }

    Token disassemble() const {
        return inst.disassemble() + " " + src.disassemble() + "\n";
    }
};

template <class Inst, class SrcInput, class DstInput,
          int BIN, int SRC_SHIFT = 0, int DST_SHIFT = 0>
struct BinaryOperator : public Pattern {
    Inst inst;

    SrcInput src;
    DstInput dst;

    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        int adv;
        int adv_total = 0;

        if ((adv = dst.matches(rbegin, rend)) == 0)
            return 0;

        if (safe_to_advance(rbegin, rend, adv)) {
            rbegin += adv;
            adv_total += adv;
        } else
            return 0;

        if (*rbegin != ",")
            return 0;

        if (safe_to_advance(rbegin, rend, 1)) {
            rbegin++;
            adv_total++;
        }

        if ((adv = src.matches(rbegin, rend)) == 0)
            return 0;

        if (safe_to_advance(rbegin, rend, adv)) {
            rbegin += adv;
            adv_total += adv;
        } else
            return 0;

        if ((adv = inst.matches(rbegin, rend)) != 0) {
            adv_total += adv;
            return adv_total;
        }

        return 0;
    }

    inst_t assemble() const {
        return BIN | (src.assemble() << SRC_SHIFT) |
            (dst.assemble() << DST_SHIFT);
    }

    Token disassemble() const {
        return inst.disassemble() + " " + src.disassemble() + ", " +
            dst.disassemble() + "\n";
    }
};

// Uggh, I have to implement this just to support *one* instruction (FMAC).
template <class Inst, class Src1Input, class Src2Input, class DstInput,
          int BIN, int SRC1_SHIFT = 0, int SRC2_SHIFT = 0,
          int DST_SHIFT = 0>
struct TrinaryOperator : public Pattern {
    Inst inst;

    Src1Input src1;
    Src2Input src2;
    DstInput dst;

    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        int adv;
        int adv_total = 0;

        if ((adv = dst.matches(rbegin, rend)) == 0)
            return 0;

        if (safe_to_advance(rbegin, rend, adv)) {
            rbegin += adv;
            adv_total += adv;
        } else
            return 0;

        if ((*rbegin) != ",")
            return 0;

        if (safe_to_advance(rbegin, rend, 1)) {
            rbegin++;
            adv_total++;
        }

        if ((adv = src2.matches(rbegin, rend)) == 0)
            return 0;

        if (safe_to_advance(rbegin, rend, adv)) {
            rbegin += adv;
            adv_total += adv;
        } else
            return 0;

        if (*rbegin != ",")
            return 0;

        if (safe_to_advance(rbegin, rend, 1)) {
            rbegin++;
            adv_total++;
        }

        if ((adv = src1.matches(rbegin, rend)) == 0)
            return 0;

        if (safe_to_advance(rbegin, rend, adv)) {
            rbegin += adv;
            adv_total += adv;
        } else
            return 0;

        if ((adv = inst.matches(rbegin, rend)) != 0) {
            adv_total += adv;
            return adv_total;
        }

        return 0;
    }

    inst_t assemble() const {
        return BIN | (src1.assemble() << SRC1_SHIFT) |
            (src2.assemble() << SRC2_SHIFT) |
            (dst.assemble() << DST_SHIFT);
    }

    Token disassemble() const {
        return inst.disassemble() + " " + src1.disassemble() + ", " +
            src2.disassemble() + ", " + dst.disassemble() + "\n";
    }
};

class Ptrn_GenReg : public Pattern {
public:
    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        std::string txt = *rbegin;

        if ((txt[0] == 'R') && (txt.size() == 2 || txt.size() == 3)) {
            int reg_no;
            std::stringstream(txt.substr(1)) >> reg_no;
            if (reg_no >= 0 && reg_no <= 15) {
                this->reg_no = reg_no;
                return 1;
            }
        }

        return 0;
    }

    inst_t assemble() const {
        return reg_no & 0xff;
    }

    Token disassemble() const {
        std::stringstream ss;
        ss << "R" << reg_no;
        return ss.str();
    }
private:
    int reg_no;
};

class Ptrn_BankReg : public Pattern {
public:
    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        std::string txt = *rbegin;
        if (txt.size() == 7 || txt.size() == 8) {
            if (txt.at(0) != 'R')
                return 0;
            size_t underscore_pos = txt.find_first_of("_BANK");
            if (underscore_pos == std::string::npos)
                return 0;
            int reg_no;
            std::stringstream(txt.substr(1, underscore_pos)) >> reg_no;
            if (reg_no >= 0 && reg_no <= 7) {
                this->reg_no = reg_no;
                return 1;
            } else {
                return 0;
            }
        }

        return 0;
    }

    inst_t assemble() const {
        return reg_no & 0x7;
    }

    Token disassemble() const {
        std::stringstream ss;
        ss << "R" << reg_no << "_BANK";
        return ss.str();
    }
private:
    int reg_no;
};

// Special register (i.e., not one of the general-purpose registers)
class Ptrn_SpecReg : public Pattern {
public:
    Ptrn_SpecReg(char const *name) {
        this->name = name;
    }

    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        std::string txt = *rbegin;

        if (txt == name)
            return 1;
        return 0;
    }

    inst_t assemble() const {
        // instruction opcode should imply this operand
        return 0;
    }

    Token disassemble() const {
        return Token(name);
    }

private:
    char const *name;
};

class Ptrn_SrReg : public Ptrn_SpecReg {
public:
    Ptrn_SrReg() : Ptrn_SpecReg("SR") {
    }
};

class Ptrn_GbrReg : public Ptrn_SpecReg {
public:
    Ptrn_GbrReg() : Ptrn_SpecReg("GBR") {
    }
};

class Ptrn_VbrReg : public Ptrn_SpecReg {
public:
    Ptrn_VbrReg() : Ptrn_SpecReg("VBR") {
    }
};

class Ptrn_SsrReg : public Ptrn_SpecReg {
public:
    Ptrn_SsrReg() : Ptrn_SpecReg("SSR") {
    }
};

class Ptrn_SpcReg : public Ptrn_SpecReg {
public:
    Ptrn_SpcReg() : Ptrn_SpecReg("SPC") {
    }
};

class Ptrn_SgrReg : public Ptrn_SpecReg {
public:
    Ptrn_SgrReg() : Ptrn_SpecReg("SGR") {
    }
};

class Ptrn_DbrReg : public Ptrn_SpecReg {
public:
    Ptrn_DbrReg() : Ptrn_SpecReg("DBR") {
    }
};

class Ptrn_PcReg : public Ptrn_SpecReg {
public:
    Ptrn_PcReg() : Ptrn_SpecReg("PC") {
    }
};

class Ptrn_PrReg : public Ptrn_SpecReg {
public:
    Ptrn_PrReg() : Ptrn_SpecReg("PR") {
    }
};

/*
 * R0 will also be picked up by Ptrn_GenReg; this token is for the few
 * instructions that only allow R0
 */
class Ptrn_R0Reg : public Ptrn_SpecReg {
public:
    Ptrn_R0Reg() : Ptrn_SpecReg("R0") {
    }
};

class Ptrn_FpulReg : public Ptrn_SpecReg {
public:
    Ptrn_FpulReg() : Ptrn_SpecReg("FPUL") {
    }
};

class Ptrn_FpscrReg : public Ptrn_SpecReg {
public:
    Ptrn_FpscrReg() : Ptrn_SpecReg("FPSCR") {
    }
};

/*
 * This isn't a special register in the strictest sense since it is not a
 * control register or status register, but there's only one xmtrx so in
 * that sense it is a special register.
 */
class Ptrn_XmtrxReg : public Ptrn_SpecReg {
public:
    Ptrn_XmtrxReg() : Ptrn_SpecReg("XMTRX") {
    }
};

class Ptrn_FrReg : public Pattern {
public:
    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        std::string txt = *rbegin;

        if ((txt.substr(0, 2) == "FR") &&
            (txt.size() == 3 || txt.size() == 4)) {
            int reg_no;
            std::stringstream(txt.substr(2)) >> reg_no;
            if (reg_no >= 0 && reg_no <= 15) {
                this->reg_no = reg_no;
                return 1;
            }
        }

        return 0;
    }

    Token disassemble() const {
        std::stringstream ss;
        ss << "FR" << reg_no;
        return ss.str();
    }

    inst_t assemble() const {
        return reg_no & 0xf;
    }
private:
    int reg_no;
};

class Ptrn_Fr0Reg : public Ptrn_SpecReg {
public:
    Ptrn_Fr0Reg() : Ptrn_SpecReg("FR0") {
    }
};

// Double-precision floating point registers
class Ptrn_DrReg : public Pattern {
public:
    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        std::string txt = *rbegin;

        if ((txt.substr(0, 2) == "DR") &&
            (txt.size() == 3 || txt.size() == 4)) {
            int reg_no;
            std::stringstream(txt.substr(2)) >> reg_no;
            if (reg_no == 0  || reg_no == 2  ||
                reg_no == 4  || reg_no == 6  ||
                reg_no == 8  || reg_no == 10 ||
                reg_no == 12 || reg_no == 14) {
                this->reg_no = reg_no;
                return 1;
            }
        }

        return 0;
    }

    std::string text() const {
        std::stringstream ss;
        ss << "DR" << reg_no;
        return ss.str();
    }

    Token disassemble() const {
        std::stringstream ss;
        ss << "DR" << reg_no;
        return ss.str();
    }

    inst_t assemble() const {
        return (reg_no >> 1) & 0x7;
    }
private:
    int reg_no;
};

// Double-precision floating point registers
class Ptrn_XdReg : public Pattern {
public:
    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        std::string txt = *rbegin;

        if ((txt.substr(0, 2) == "XD") &&
            (txt.size() == 3 || txt.size() == 4)) {
            int reg_no;
            std::stringstream(txt.substr(2)) >> reg_no;
            if (reg_no == 0  || reg_no == 2  ||
                reg_no == 4  || reg_no == 6  ||
                reg_no == 8  || reg_no == 10 ||
                reg_no == 12 || reg_no == 14) {
                this->reg_no = reg_no;
                return 1;
            }
        }

        return 0;
    }

    Token disassemble() const {
        std::stringstream ss;
        ss << "XD" << reg_no;
        return ss.str();
    }

    inst_t assemble() const {
        return (reg_no >> 1) & 0x7;
    }
private:
    int reg_no;
};

// Floating-point vector registers
class Ptrn_FvReg : public Pattern {
public:
    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        std::string txt = *rbegin;

        if ((txt.substr(0, 2) == "FV") && (txt.size() == 3)) {
            int reg_no;
            std::stringstream(txt.substr(2)) >> reg_no;
            if (reg_no == 0 || reg_no == 4 || reg_no == 8 || reg_no == 12) {
                this->reg_no = reg_no;
                return 1;
            }
        }

        return 0;
    }

    Token disassemble() const {
        std::stringstream ss;
        ss << "FV" << reg_no;
        return ss.str();
    }

    inst_t assemble() const {
        return (reg_no >> 2) & 0x3;
    }
private:
    int reg_no;
};

template <unsigned MASK>
class Ptrn_immed : public Pattern {
public:
    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        std::string txt = *rbegin;
        bool is_hex;

        if (txt.size() < 1)
            return 0;

        if (txt.size() > 2 && txt.substr(0, 2) == "0x") {
            // hex string
            is_hex = true;
            txt = txt.substr(2);

            for (std::string::iterator it = txt.begin();
                 it != txt.end(); it++) {
                if ((*it < '0' || *it > '9') &&
                    (*it < 'a' || *it > 'f') &&
                    (*it < 'A' || *it > 'F'))
                    return 0;
            }
        } else {
            is_hex = false;
            for (std::string::iterator it = txt.begin();
                 it != txt.end(); it++) {
                if (*it < '0' || *it > '9')
                    return 0;
            }
        }

        if (safe_to_advance(rbegin, rend, 1))
            rbegin += 1;
        else
            return 0;

        if (*rbegin == "#") {
            std::stringstream ss(txt);
            if (is_hex) {
                ss >> std::hex >> imm;
            } else {
                ss >> imm;
            }

            return 2;
        }

        return 0;
    }

    Token disassemble() const {
        std::stringstream ss;
        ss << "#0x" << std::hex << imm;
        return ss.str();
    }

    inst_t assemble() const {
        return imm & MASK;
    }
private:
    unsigned imm;
};

/*
 * Displacement values, which are like immediates but they don't
 * begin with #-symbols.
 *
 * TODO: add support for labels here (wherein you reference an address by a
 *       string of ascii letters).
 */
template <unsigned MASK>
class Ptrn_Disp : public Pattern {
public:
    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        std::string txt = *rbegin;
        bool is_hex;

        if (txt.size() < 1)
            return 0;

        if (txt.size() > 2 && txt.substr(0, 2) == "0x") {
            // hex string
            is_hex = true;
            txt = txt.substr(2);

            for (std::string::iterator it = txt.begin();
                 it != txt.end(); it++) {
                if ((*it < '0' || *it > '9') &&
                    (*it < 'a' || *it > 'f') &&
                    (*it < 'A' || *it > 'F'))
                    return 0;
            }
        } else {
            is_hex = false;
            for (std::string::iterator it = txt.begin();
                 it != txt.end(); it++) {
                if (*it < '0' || *it > '9')
                    return 0;
            }
        }

        std::stringstream ss(txt);
        if (is_hex) {
            ss >> std::hex >> imm;
        } else {
            ss >> imm;
        }

        return 1;
    }

    inst_t assemble() const {
        return imm & MASK;
    }

    Token disassemble() const {
        std::stringstream ss;
        ss << "#0x" << std::hex << imm;
        return ss.str();
    }
private:
    unsigned imm;
};

template <class InnerOperand>
class Ptrn_Ind : public Pattern {
public:
    InnerOperand op;

    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        int advance = 0;
        if (rbegin == rend)
            return 0;

        if ((advance = op.matches(rbegin, rend))) {
            if (safe_to_advance(rbegin, rend, advance))
                rbegin += advance;
            else
                return 0;

            if (*rbegin == "@") {
                return advance + 1;
            }
        }

        return 0;
    }

    inst_t assemble() const {
        return op.assemble();
    }

    Token disassemble() const {
        return std::string("@") + op.disassemble();
    }
};

/*
 * this is a token for representing operands which are indirections of sums
 * They are expressed in assembler in the form @(LeftOperand, RightOperand)
 */
template <class LeftOperand, class RightOperand, int BIN, int SRC_SHIFT = 0,
          int DST_SHIFT = 0>
class Ptrn_BinaryInd : public Pattern {
public:
    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        std::string txt = *rbegin;
        int adv = 0, adv_extra;

        if (*rbegin != ")")
            return 0;
        if (safe_to_advance(rbegin, rend, 1)) {
            adv++;
            rbegin++;
        } else {
            return 0;
        }

        if (!(adv_extra = op_right.matches(rbegin, rend)))
            return 0;

        if (safe_to_advance(rbegin, rend, adv_extra)) {
            adv += adv_extra;
            rbegin += adv_extra;
        } else {
            return 0;
        }

        if (*rbegin != ",")
            return 0;
        if (safe_to_advance(rbegin, rend, 1)) {
            adv++;
            rbegin++;
        } else {
            return 0;
        }

        if (!(adv_extra = op_left.matches(rbegin, rend)))
            return 0;

        if (safe_to_advance(rbegin, rend, adv_extra)) {
            adv += adv_extra;
            rbegin += adv_extra;
        } else {
            return 0;
        }

        if (*rbegin != "(")
            return 0;
        if (safe_to_advance(rbegin, rend, 1)) {
            adv++;
            rbegin++;
        } else {
            return 0;
        }

        if (*rbegin != "@")
            return 0;

        return adv + 1;
    }

    Token disassemble() const {
        return std::string("@(") + op_left.disassemble() + std::string(", ") +
            op_right.disassemble() + std::string(")");
    }

    inst_t assemble() const {
        return (op_left.assemble() << SRC_SHIFT) |
            (op_right.assemble() << DST_SHIFT) | BIN;
    }

private:
    LeftOperand op_left;
    RightOperand op_right;
};

template <class InnerOperand>
class Ptrn_IndInc : public Pattern {
public:
    InnerOperand op;

    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        int advance = 0;
        if (rbegin == rend)
            return 0;

        if (*rbegin != "+") {
            return 0;
        }

        if (safe_to_advance(rbegin, rend, 1)) {
            advance++;
            rbegin++;
        } else {
            return 0;
        }

        if ((advance = op.matches(rbegin, rend))) {
            if (safe_to_advance(rbegin, rend, advance))
                rbegin += advance;
            else
                return 0;

            if (*rbegin == "@") {
                return advance + 1;
            }
        }

        return 0;
    }

    Token disassemble() const {
        return std::string("@") + op.disassemble() + std::string("+");
    }

    inst_t assemble() const {
        return op.assemble();
    }
};

template <class InnerOperand>
class Ptrn_DecInd : public Pattern {
public:
    InnerOperand op;

    virtual int matches(TokList::reverse_iterator rbegin,
                        TokList::reverse_iterator rend) {
        int advance = 0, more_adv;
        if (rbegin == rend)
            return 0;

        if ((more_adv = op.matches(rbegin, rend))) {
            advance += more_adv;
            if (safe_to_advance(rbegin, rend, more_adv))
                rbegin += more_adv;
            else
                return 0;

            if (*rbegin == "-") {
                if (safe_to_advance(rbegin, rend, 1)) {
                    advance++;
                    rbegin++;

                    if (*rbegin == "@")
                        return advance + 1;
                }
            }
        }

        return 0;
    }

    inst_t assemble() const {
        return op.assemble();
    }

    Token disassemble() const {
        return std::string("@") + op.disassemble() + std::string("+");
    }
};

class Ptrn_Mach : public TxtPattern {
public:
    Ptrn_Mach() : TxtPattern("MACH") {
    }
};

class Ptrn_Macl : public TxtPattern {
public:
    Ptrn_Macl() : TxtPattern("MACL") {
    }
};
