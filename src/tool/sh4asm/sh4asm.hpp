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

#include <map>
#include <string>
#include <vector>
#include <sstream>

#include <boost/shared_ptr.hpp>

#include "types.hpp"

class BadSymbolError : public std::exception {
public:
    BadSymbolError(char const *sym) {
        this->sym = sym;
    }

    char const *what() {
        return sym;
    }
private:
    char const *sym;
};

class ParseError : public std::exception {
public:
    ParseError(char const *sym) {
        this->desc = desc;
    }

    char const *what() {
        return desc;
    }
private:
    char const *desc;
};

class Sh4Prog {
public:
    // completemy arbitrary 16MB limit to keep things from getting too out of
    // hand.  This can be freely changed with no consequence.
    static const int MAX_INST_COUNT = 8 * 1024 * 1024;

    addr32_t lookup_sym(const std::string& sym_name) const;

    inst_t assemble_line(const std::string& inst);
private:
    class Token;

    typedef std::map<std::string, addr32_t> SymMap;
    typedef std::vector<inst_t> InstList;
    typedef boost::shared_ptr<Token> TokPtr;
    typedef std::vector<TokPtr> TokList;
    typedef std::vector<TokPtr> PatternList;

    static PatternList get_patterns();

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

    class Token {
    public:
        virtual ~Token() {
        }

        /*
         * matches returns how far to advance rbegin.
         * If the return is <= 0, then there was no match.
         */
        virtual int matches(TokList::reverse_iterator rbegin,
                            TokList::reverse_iterator rend) = 0;
        virtual std::string text() const = 0;
        virtual inst_t assemble() const {
            return 0;
        }
    };

    class TxtToken : public Token {
    public:
        std::string txt;

        TxtToken(char const *txt) {
            this->txt = std::string(txt);
        }

        TxtToken(const std::string& txt) {
            this->txt = txt;
        }

        virtual int matches(TokList::reverse_iterator rbegin,
                            TokList::reverse_iterator rend) {
            if ((*rbegin)->text() == txt) {
                return 1;
            }

            return 0;
        }

        virtual std::string text() const {
            return txt;
        }
    };

#define TXT_TOK(name) Tok_ ## name
#define INST_TOK(name, inst)                    \
    class TXT_TOK(name) : public TxtToken {     \
    public:                                     \
        TXT_TOK(name)() : TxtToken(inst) {      \
        }                                       \
    }

    INST_TOK(and, "AND");
    INST_TOK(add, "ADD");
    INST_TOK(addc, "ADDC");
    INST_TOK(addv, "ADDV");
    INST_TOK(andb, "AND.B");
    INST_TOK(bf, "BF");
    INST_TOK(bfs, "BF/S");
    INST_TOK(bra, "BRA");
    INST_TOK(braf, "BRAF");
    INST_TOK(bsr, "BSR");
    INST_TOK(bsrf, "BSRF");
    INST_TOK(bt, "BT");
    INST_TOK(bts, "BT/S");
    INST_TOK(clrmac, "CLRMAC");
    INST_TOK(clrs, "CLRS");
    INST_TOK(clrt, "CLRT");
    INST_TOK(cmpeq, "CMP/EQ");
    INST_TOK(cmpge, "CMP/GE");
    INST_TOK(cmpgt, "CMP/GT");
    INST_TOK(cmphi, "CMP/HI");
    INST_TOK(cmphs, "CMP/HS");
    INST_TOK(cmppz, "CMP/PZ");
    INST_TOK(cmppl, "CMP/PL");
    INST_TOK(cmpstr, "CMP/STR");
    INST_TOK(div1, "DIV1");
    INST_TOK(div0s, "DIV0S");
    INST_TOK(divou, "DIVOU");
    INST_TOK(dmulsl, "DMULS.L");
    INST_TOK(dmulul, "DMULU.L");
    INST_TOK(dt, "DT");
    INST_TOK(extsb, "EXTS.B");
    INST_TOK(extsw, "EXTS.W");
    INST_TOK(extub, "EXTU.B");
    INST_TOK(extuw, "EXTU.W");
    INST_TOK(fabs, "FABS");
    INST_TOK(fadd, "FADD");
    INST_TOK(fcmpeq, "FCMP/EQ");
    INST_TOK(fcmpgt, "FCMP/GT");
    INST_TOK(fcnvds, "FCNVDS");
    INST_TOK(fcnvsd, "FCNVSD");
    INST_TOK(fdiv, "FDIV");
    INST_TOK(fipr, "FIPR");
    INST_TOK(fldi0, "FLDI0");
    INST_TOK(fldi1, "FLDI1");
    INST_TOK(flds, "FLDS");
    INST_TOK(float, "FLOAT");
    INST_TOK(fmac, "FMAC");
    INST_TOK(fmov, "FMOV");
    INST_TOK(fmovs, "FMOV.S");
    INST_TOK(fmul, "FMUL");
    INST_TOK(fneg, "FNEG");
    INST_TOK(frchg, "FRCHG");
    INST_TOK(fschg, "FSCHG");
    INST_TOK(fsqrt, "FSQRT");
    INST_TOK(fsts, "FSTS");
    INST_TOK(fsub, "FSUB");
    INST_TOK(ftrc, "FTRC");
    INST_TOK(ftrv, "FTRV");
    INST_TOK(jmp, "JMP");
    INST_TOK(jsr, "JSR");
    INST_TOK(ldc, "LDC");
    INST_TOK(lds, "LDS");
    INST_TOK(ldsl, "LDS.L");
    INST_TOK(ldcl, "LDC.L");
    INST_TOK(ldtlb, "LDTLB");
    INST_TOK(macl, "MAC.L");
    INST_TOK(macw, "MAC.W");
    INST_TOK(mov, "MOV");
    INST_TOK(mova, "MOVA");
    INST_TOK(movb, "MOV.B");
    INST_TOK(movcal, "MOVCA.L");
    INST_TOK(movl, "MOV.L");
    INST_TOK(movw, "MOV.W");
    INST_TOK(movt, "MOVT");
    INST_TOK(mull, "MUL.L");
    INST_TOK(mulsw, "MULS.W");
    INST_TOK(muluw, "MULU.W");
    INST_TOK(neg, "NEG");
    INST_TOK(negc, "NEGC");
    INST_TOK(nop, "NOP");
    INST_TOK(not, "NOT");
    INST_TOK(ocbi, "OCBI");
    INST_TOK(ocbp, "OCBP");
    INST_TOK(ocbwb, "OCBWB");
    INST_TOK(or, "OR");
    INST_TOK(orb, "OR.B");
    INST_TOK(pref, "PREF");
    INST_TOK(rotl, "ROTL");
    INST_TOK(rotr, "ROTR");
    INST_TOK(rotcl, "ROTCL");
    INST_TOK(rotcr, "ROTCR");
    INST_TOK(rte, "RTE");
    INST_TOK(rts, "RTS");
    INST_TOK(sets, "SETS");
    INST_TOK(sett, "SETT");
    INST_TOK(shad, "SHAD");
    INST_TOK(shld, "SHLD");
    INST_TOK(shal, "SHAL");
    INST_TOK(shar, "SHAR");
    INST_TOK(shll, "SHLL");
    INST_TOK(shlr, "SHLR");
    INST_TOK(shll2, "SHLL2");
    INST_TOK(shlr2, "SHLR2");
    INST_TOK(shll8, "SHLL8");
    INST_TOK(shlr8, "SHLR8");
    INST_TOK(shll16, "SHLL16");
    INST_TOK(shlr16, "SHLR16");
    INST_TOK(sleep, "SLEEP");
    INST_TOK(stc, "STC");
    INST_TOK(stcl, "STC.L");
    INST_TOK(sts, "STS");
    INST_TOK(stsl, "STS.L");
    INST_TOK(sub, "SUB");
    INST_TOK(subc, "SUBC");
    INST_TOK(subv, "SUBV");
    INST_TOK(swapb, "SWAP.B");
    INST_TOK(swapw, "SWAP.W");
    INST_TOK(tasb, "TAS.B");
    INST_TOK(tst, "TST");
    INST_TOK(tstb, "TST.B");
    INST_TOK(trapa, "TRAPA");
    INST_TOK(xor, "XOR");
    INST_TOK(xorb, "XOR.B");
    INST_TOK(xtrct, "XTRCT");

    template <class Inst, int BIN>
    struct NoArgOperator : public Token {
        Inst inst;

        virtual int matches(TokList::reverse_iterator rbegin,
                            TokList::reverse_iterator rend) {
            return inst.matches(rbegin, rend);
        }

        virtual std::string text() const {
            return inst.text();
        }

        inst_t assemble() const {
            return (inst_t)BIN;
        }
    };

    template <class Inst, class SrcInput, int BIN, int SRC_SHIFT>
    struct UnaryOperator : public Token {
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

        virtual std::string text() const {
            return inst.text() + " " + src.text();
        }

        inst_t assemble() const {
            return BIN | (src.assemble() << SRC_SHIFT);
        }
    };

    template <class Inst, class SrcInput, class DstInput,
              int BIN, int SRC_SHIFT = 0, int DST_SHIFT = 0>
    struct BinaryOperator : public Token {
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

            if ((*rbegin)->text() != ",")
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

        virtual std::string text() const {
            return inst.text() + " " + src.text() + ", " + dst.text();
        }

        inst_t assemble() const {
            return BIN | (src.assemble() << SRC_SHIFT) |
                (dst.assemble() << DST_SHIFT);
        }
    };

    // Uggh, I have to implement this just to support *one* instruction (FMAC).
    template <class Inst, class Src1Input, class Src2Input, class DstInput,
              int BIN, int SRC1_SHIFT = 0, int SRC2_SHIFT = 0,
              int DST_SHIFT = 0>
    struct TrinaryOperator : public Token {
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

            if ((*rbegin)->text() != ",")
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

            if ((*rbegin)->text() != ",")
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

        virtual std::string text() const {
            return inst.text() + " " + src1.text() + ", " +
                src2.text() + ", " + dst.text();
        }

        inst_t assemble() const {
            return BIN | (src1.assemble() << SRC1_SHIFT) |
                (src2.assemble() << SRC2_SHIFT) |
                (dst.assemble() << DST_SHIFT);
        }
    };

    class Tok_GenReg : public Token {
    public:
        virtual int matches(TokList::reverse_iterator rbegin,
                            TokList::reverse_iterator rend) {
            std::string txt = (*rbegin)->text();

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

        std::string text() const {
            std::stringstream ss;
            ss << "R" << reg_no;
            return ss.str();
        }

        inst_t assemble() const {
            return reg_no & 0xff;
        }
    private:
        int reg_no;
    };

    class Tok_BankReg : public Token {
    public:
        virtual int matches(TokList::reverse_iterator rbegin,
                            TokList::reverse_iterator rend) {
            std::string txt = (*rbegin)->text();
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

        std::string text() const {
            std::stringstream ss;
            ss << "R" << reg_no << "_BANK";
            return ss.str();
        }

        inst_t assemble() const {
            return reg_no & 0x7;
        }
    private:
        int reg_no;
    };

    // Special register (i.e., not one of the general-purpose registers)
    class Tok_SpecReg : public Token {
    public:
        Tok_SpecReg(char const *name) {
            this->name = name;
        }

        virtual int matches(TokList::reverse_iterator rbegin,
                            TokList::reverse_iterator rend) {
            std::string txt = (*rbegin)->text();

            if (txt == name)
                return 1;
            return 0;
        }

        inst_t assemble() const {
            // instruction opcode should imply this operand
            return 0;
        }

        std::string text() const {
            return std::string(name);
        }

    private:
        char const *name;
    };

    class Tok_SrReg : public Tok_SpecReg {
    public:
        Tok_SrReg() : Tok_SpecReg("SR") {
        }
    };

    class Tok_GbrReg : public Tok_SpecReg {
    public:
        Tok_GbrReg() : Tok_SpecReg("GBR") {
        }
    };

    class Tok_VbrReg : public Tok_SpecReg {
    public:
        Tok_VbrReg() : Tok_SpecReg("VBR") {
        }
    };

    class Tok_SsrReg : public Tok_SpecReg {
    public:
        Tok_SsrReg() : Tok_SpecReg("SSR") {
        }
    };

    class Tok_SpcReg : public Tok_SpecReg {
    public:
        Tok_SpcReg() : Tok_SpecReg("SPC") {
        }
    };

    class Tok_SgrReg : public Tok_SpecReg {
    public:
        Tok_SgrReg() : Tok_SpecReg("SGR") {
        }
    };

    class Tok_DbrReg : public Tok_SpecReg {
    public:
        Tok_DbrReg() : Tok_SpecReg("DBR") {
        }
    };

    class Tok_PcReg : public Tok_SpecReg {
    public:
        Tok_PcReg() : Tok_SpecReg("PC") {
        }
    };

    class Tok_PrReg : public Tok_SpecReg {
    public:
        Tok_PrReg() : Tok_SpecReg("PR") {
        }
    };

    /*
     * R0 will also be picked up by Tok_GenReg; this token is for the few
     * instructions that only allow R0
     */
    class Tok_R0Reg : public Tok_SpecReg {
    public:
        Tok_R0Reg() : Tok_SpecReg("R0") {
        }
    };

    class Tok_FpulReg : public Tok_SpecReg {
    public:
        Tok_FpulReg() : Tok_SpecReg("FPUL") {
        }
    };

    class Tok_FpscrReg : public Tok_SpecReg {
    public:
        Tok_FpscrReg() : Tok_SpecReg("FPSCR") {
        }
    };

    /*
     * This isn't a special register in the strictest sense since it is not a
     * control register or status register, but there's only one xmtrx so in
     * that sense it is a special register.
     */
    class Tok_XmtrxReg : public Tok_SpecReg {
    public:
        Tok_XmtrxReg() : Tok_SpecReg("XMTRX") {
        }
    };

    class Tok_FrReg : public Token {
    public:
        virtual int matches(TokList::reverse_iterator rbegin,
                            TokList::reverse_iterator rend) {
            std::string txt = (*rbegin)->text();

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

        std::string text() const {
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

    class Tok_Fr0Reg : public Tok_SpecReg {
    public:
        Tok_Fr0Reg() : Tok_SpecReg("FR0") {
        }
    };

    // Double-precision floating point registers
    class Tok_DrReg : public Token {
    public:
        virtual int matches(TokList::reverse_iterator rbegin,
                            TokList::reverse_iterator rend) {
            std::string txt = (*rbegin)->text();

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

        inst_t assemble() const {
            return (reg_no >> 1) & 0x7;
        }
    private:
        int reg_no;
    };

    // Double-precision floating point registers
    class Tok_XdReg : public Token {
    public:
        virtual int matches(TokList::reverse_iterator rbegin,
                            TokList::reverse_iterator rend) {
            std::string txt = (*rbegin)->text();

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

        std::string text() const {
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
    class Tok_FvReg : public Token {
    public:
        virtual int matches(TokList::reverse_iterator rbegin,
                            TokList::reverse_iterator rend) {
            std::string txt = (*rbegin)->text();

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

        std::string text() const {
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
    class Tok_immed : public Token {
    public:
        virtual int matches(TokList::reverse_iterator rbegin,
                            TokList::reverse_iterator rend) {
            std::string txt = (*rbegin)->text();
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

            if ((*rbegin)->text() == "#") {
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

        std::string text() const {
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
    class Tok_Disp : public Token {
    public:
        virtual int matches(TokList::reverse_iterator rbegin,
                            TokList::reverse_iterator rend) {
            std::string txt = (*rbegin)->text();
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

        std::string text() const {
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

    template <class InnerOperand>
    class Tok_Ind : public Token {
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

                if ((*rbegin)->text() == "@") {
                    return advance + 1;
                }
            }

            return 0;
        }

        std::string text() const {
            return std::string("@") + op.text();
        }

        inst_t assemble() const {
            return op.assemble();
        }
    };

    /*
     * this is a token for representing operands which are indirections of sums
     * They are expressed in assembler in the form @(LeftOperand, RightOperand)
     */
    template <class LeftOperand, class RightOperand, int BIN, int SRC_SHIFT = 0,
              int DST_SHIFT = 0>
    class Tok_BinaryInd : public Token {
    public:
        virtual int matches(TokList::reverse_iterator rbegin,
                            TokList::reverse_iterator rend) {
            std::string txt = (*rbegin)->text();
            int adv = 0, adv_extra;

            if ((*rbegin)->text() != ")")
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

            if ((*rbegin)->text() != ",")
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

            if ((*rbegin)->text() != "(")
                return 0;
            if (safe_to_advance(rbegin, rend, 1)) {
                adv++;
                rbegin++;
            } else {
                return 0;
            }

            if ((*rbegin)->text() != "@")
                return 0;

            return adv + 1;
        }

        std::string text() const {
            return std::string("@(") + op_left.text() + std::string(", ") +
                op_right.text() + std::string(")");
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
    class Tok_IndInc : public Token {
    public:
        InnerOperand op;

        virtual int matches(TokList::reverse_iterator rbegin,
                            TokList::reverse_iterator rend) {
            int advance = 0;
            if (rbegin == rend)
                return 0;

            if ((*rbegin)->text() != "+") {
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

                if ((*rbegin)->text() == "@") {
                    return advance + 1;
                }
            }

            return 0;
        }

        std::string text() const {
            return std::string("@") + op.text() + std::string("+");
        }

        inst_t assemble() const {
            return op.assemble();
        }
    };

    template <class InnerOperand>
    class Tok_DecInd : public Token {
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

                if ((*rbegin)->text() == "-") {
                    if (safe_to_advance(rbegin, rend, 1)) {
                        advance++;
                        rbegin++;

                        if ((*rbegin)->text() == "@")
                            return advance + 1;
                    }
                }
            }

            return 0;
        }

        std::string text() const {
            return std::string("@") + op.text() + std::string("+");
        }

        inst_t assemble() const {
            return op.assemble();
        }
    };

    class Tok_Mach : public TxtToken {
    public:
        Tok_Mach() : TxtToken("MACH") {
        }
    };

    class Tok_Macl : public TxtToken {
    public:
        Tok_Macl() : TxtToken("MACL") {
        }
    };

    SymMap syms;
    InstList prog;

    /* remove any comments from the line. */
    std::string preprocess_line(const std::string& line);

    static TokList tokenize_line(const std::string& line);

    inst_t assemble_tokens(TokList toks);
    inst_t assemble_op_noargs(const std::string& inst);

    void add_label(const std::string& lbl);
};
