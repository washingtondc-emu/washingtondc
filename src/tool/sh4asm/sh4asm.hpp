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
            if ((*rbegin)->text() == "MOV.W") {
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

    INST_TOK(add, "ADD");
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
    INST_TOK(cmppz, "CMP/PZ");
    INST_TOK(cmppl, "CMP/PL");
    INST_TOK(cmpeq, "CMP/EQ");
    INST_TOK(divou, "DIVOU");
    INST_TOK(dt, "DT");
    INST_TOK(frchg, "FRCHG");
    INST_TOK(fschg, "FSCHG");
    INST_TOK(jmp, "JMP");
    INST_TOK(jsr, "JSR");
    INST_TOK(ldc, "LDC");
    INST_TOK(ldcl, "LDC.L");
    INST_TOK(ldtlb, "LDTLB");
    INST_TOK(mov, "MOV");
    INST_TOK(movl, "MOV.L");
    INST_TOK(movw, "MOV.W");
    INST_TOK(movt, "MOVT");
    INST_TOK(nop, "NOP");
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
    INST_TOK(tasb, "TAS.B");
    INST_TOK(tst, "TST");
    INST_TOK(tstb, "TST.B");
    INST_TOK(trapa, "TRAPA");
    INST_TOK(xor, "XOR");
    INST_TOK(xorb, "XOR.B");

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

    class Tok_GenReg : public Token {
    public:
        virtual int matches(TokList::reverse_iterator rbegin,
                            TokList::reverse_iterator rend) {
            std::string txt = (*rbegin)->text();

            if (txt.size() == 2 || txt.size() == 3) {
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
                txt = txt.substr(0, 2);

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

            if ((*rbegin)->text() != "+") {
                return 0;
            }

            if (safe_to_advance(rbegin, rend, 1)) {
                advance++;
                rbegin++;
            } else {
                return 0;
            }

            if ((more_adv = op.matches(rbegin, rend))) {
                advance += more_adv;
                if (safe_to_advance(rbegin, rend, more_adv))
                    rbegin += more_adv;
                else
                    return 0;

                if ((*rbegin)->text() == "@") {
                    if (safe_to_advance(rbegin, rend, 1)) {
                        advance++;

                        if ((*rbegin)->text() == "-")
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

    SymMap syms;
    InstList prog;

    /* remove any comments from the line. */
    std::string preprocess_line(const std::string& line);

    static TokList tokenize_line(const std::string& line);

    inst_t assemble_tokens(TokList toks);
    inst_t assemble_op_noargs(const std::string& inst);

    void add_label(const std::string& lbl);

    struct OpNoArgs {
        char const *inst;
        inst_t opcode;
    };

    // opcode that takes a single register as its argument
    struct OpReg {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    // opcode that only takes an immediate value as input
    struct OpImm {
        char const  *inst;

        inst_t opcode;

        unsigned imm_shift;
        inst_t imm_mask;
    };

    // opcode that takes a register containing the address of its sole argument
    struct OpIndReg {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpRegSR {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpSRReg {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpGBRReg {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpVBRReg {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpSSRReg {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpSPCReg {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpSGRReg {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpDBRReg {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpRegGBR {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpRegVBR {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpRegSSR {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpRegSPC {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpRegDBR {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpIndIncRegSR {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpIndIncRegGBR {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpIndIncRegVBR {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpIndIncRegSSR {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpIndIncRegSPC {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpIndIncRegDBR {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpSRIndDecReg {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpGBRIndDecReg {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpVBRIndDecReg {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpSSRIndDecReg {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpSPCIndDecReg {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpSGRIndDecReg {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpDBRIndDecReg {
        char const *inst;
        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    // opcode that takes an immediate and a register
    struct OpImmReg {
        char const  *inst;

        inst_t opcode;

        unsigned imm_shift;
        inst_t imm_mask;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    /*
     * Opcode that adds PC + disp (scaled) and moves that
     * into a general-purpose register.
     * format in manual is @(disp:8, PC)
     */
    struct OpPcRelDisp {
        char const *inst;

        inst_t opcode;

        unsigned disp_shift;
        inst_t disp_mask;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    // opcode that takes general-purpose registers as the source and destination
    struct OpRegReg {
        char const *inst;

        inst_t opcode;

        unsigned reg_src_shift;
        inst_t reg_src_mask;

        unsigned reg_dst_shift;
        inst_t reg_dst_mask;
    };

    struct OpRegBank {
        char const *inst;

        inst_t opcode;

        unsigned reg_src_shift;
        inst_t reg_src_mask;

        unsigned bank_no_shift;
        unsigned bank_no_mask;
    };

    struct OpBankReg {
        char const *inst;

        inst_t opcode;

        unsigned reg_src_shift;
        inst_t reg_src_mask;

        unsigned bank_no_shift;
        unsigned bank_no_mask;
    };

    struct OpBankIndDecReg {
        char const *inst;

        inst_t opcode;

        unsigned reg_src_shift;
        inst_t reg_src_mask;

        unsigned bank_no_shift;
        unsigned bank_no_mask;
    };

    struct OpRegMach {
        char const *inst;

        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpRegMacl {
        char const *inst;

        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpMachReg {
        char const *inst;

        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpMaclReg {
        char const *inst;

        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpRegPr {
        char const *inst;

        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpPrReg {
        char const *inst;

        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpIndIncRegMach {
        char const *inst;

        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpIndIncRegMacl {
        char const *inst;

        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpMachIndDecReg {
        char const *inst;

        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpMaclIndDecReg {
        char const *inst;

        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpIndIncRegPr {
        char const *inst;

        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    struct OpPrIndDecReg {
        char const *inst;

        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    /*
     * opcode that takes a general-purpose register as the src and a
     * general-purpose register as the *address* of the dst.
     */
    struct OpRegIndReg {
        char const *inst;

        inst_t opcode;

        unsigned reg_src_shift;
        inst_t reg_src_mask;

        unsigned reg_dst_shift;
        inst_t reg_dst_mask;
    };

    /*
     * opcode that takes a general-purpose register as the *address*
     * of the src and a general-purpose register as the dst.
     */
    struct OpIndRegReg {
        char const *inst;

        inst_t opcode;

        unsigned reg_src_shift;
        inst_t reg_src_mask;

        unsigned reg_dst_shift;
        inst_t reg_dst_mask;
    };

    /*
     * opcode that takes a general purpose register as the source and
     * a general purpose register containing the address of the dest.  The
     * destination-register gets decremented prior to being dereferenced.
     */
    struct OpRegIndDecReg {        
        char const *inst;

        inst_t opcode;

        unsigned reg_src_shift;
        inst_t reg_src_mask;

        unsigned reg_dst_shift;
        inst_t reg_dst_mask;
    };

    /*
     * opcode that takes a general-purpose register containing the address of
     * the source, and a general-purpose register which is the dest.  The
     * source-address is incremented after the operation is complete.
     */
    struct OpIndIncRegReg {
        char const *inst;

        inst_t opcode;

        unsigned reg_src_shift;
        inst_t reg_src_mask;

        unsigned reg_dst_shift;
        inst_t reg_dst_mask;
    };

    /*
     * opcode that takes two general-purpose registers containing the addresses of
     * the source and destination.  Both registers are incremented after the
     * operation is complete
     */
    struct OpIndIncRegIndIncRegReg {
        char const *inst;

        inst_t opcode;

        unsigned reg_src_shift;
        inst_t reg_src_mask;

        unsigned reg_dst_shift;
        inst_t reg_dst_mask;
    };

    /*
     * opcode that takes a register containing an address and a displacement
     * which as added to that address (but not stored in the register like in
     * the inc/dec variants) to be used as the destination.  the source is
     * implied.
     */
    struct OpDispRegDst {
        char const *inst;

        inst_t opcode;

        unsigned disp_shift;
        inst_t disp_mask;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    /*
     * opcode that takes a register to be used as the destination, a register
     * containing an address and a displacement which as added to that address
     * (but not stored in the register like in the inc/dec variants) to be used
     * as the destination.
     */
    struct OpRegDispReg {
        char const *inst;

        inst_t opcode;

        unsigned reg_src_shift;
        inst_t reg_src_mask;

        unsigned disp_shift;
        inst_t disp_mask;

        unsigned reg_dst_shift;
        inst_t reg_dst_mask;
    };

    /*
     * opcode that takes a register containing an address and a displacement
     * which as added to that address (but not stored in the register like in
     * the inc/dec variants) to be used as the source.  the destination is
     * implied.
     */
    struct OpDispRegSrc {
        char const *inst;

        inst_t opcode;

        unsigned disp_shift;
        inst_t disp_mask;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    /*
     * opcode that takes a register to be used as the destination, a register
     * containing an address and a displacement which as added to that address
     * (but not stored in the register like in the inc/dec variants) to be used
     * as the source.
     */
    struct OpDispRegReg {
        char const *inst;

        inst_t opcode;

        unsigned reg_src_shift;
        inst_t reg_src_mask;

        unsigned disp_shift;
        inst_t disp_mask;

        unsigned reg_dst_shift;
        inst_t reg_dst_mask;
    };

    /*
     * @(R0,Rm)
     * opcode that takes a general-purpose register which is added to R0 to get
     * the address of the destination operand.  The source is another
     * general-purpose register
     */
    struct OpRegRegPlusR0 { //coming up with names for these is getting hard...        
        char const *inst;

        inst_t opcode;

        unsigned reg_src_shift;
        inst_t reg_src_mask;

        unsigned reg_dst_shift;
        inst_t reg_dst_mask;
    };

    /*
     * @(R0,Rm)
     * opcode that takes a general-purpose register which is added to R0 to get
     * the address of the source operand.  The destination is another
     * general-purpose register
     */
    struct OpRegPlusR0Reg {
        char const *inst;

        inst_t opcode;

        unsigned reg_src_shift;
        inst_t reg_src_mask;

        unsigned reg_dst_shift;
        inst_t reg_dst_mask;
    };

    /*
     * opcode that takes a displacement as an immediate value and adds that
     * displacement to the GBR register to get the destination.  The source is
     * implied to be R0.
     */
    struct OpR0DispPlusGBR {
        char const *inst;

        inst_t opcode;

        unsigned reg_disp_shift;
        inst_t reg_disp_mask;
    };

    /*
     * opcode that takes a displacement as an immediate value and adds that
     * displacement to the GBR register to get the source.  The destination is
     * implied to be R0.
     */
    struct OpDispPlusGBRR0 {
        char const *inst;

        inst_t opcode;

        unsigned reg_disp_shift;
        inst_t reg_disp_mask;
    };

    // Opcode that uses PC + displacement as a source, and R0 as a destination.
    struct OpDispPlusPCR0 {
        char const *inst;

        inst_t opcode;

        unsigned reg_disp_shift;
        inst_t reg_disp_mask;
    };

    /*
     * opcode that uses r0 as a source and some general-purpose reg as the
     * address of the destination.
     */
    struct OpR0IndReg {
        char const *inst;

        inst_t opcode;

        unsigned reg_shift;
        inst_t reg_mask;
    };

    static const struct OpNoArgs op_noargs[];
    static const struct OpReg op_reg[];
    static const struct OpImm op_imm[];
    static const struct OpIndReg op_indreg[];
    static const struct OpRegSR op_regsr[];
    static const struct OpRegGBR op_reggbr[];
    static const struct OpRegVBR op_regvbr[];
    static const struct OpRegSSR op_regssr[];
    static const struct OpRegSPC op_regspc[];
    static const struct OpRegDBR op_regdbr[];
    static const struct OpSRReg op_srreg[];
    static const struct OpGBRReg op_gbrreg[];
    static const struct OpVBRReg op_vbrreg[];
    static const struct OpSSRReg op_ssrreg[];
    static const struct OpSPCReg op_spcreg[];
    static const struct OpSGRReg op_sgrreg[];
    static const struct OpDBRReg op_dbrreg[];
    static const struct OpIndIncRegSR op_indincregsr[];
    static const struct OpIndIncRegGBR op_indincreggbr[];
    static const struct OpIndIncRegVBR op_indincregvbr[];
    static const struct OpIndIncRegSSR op_indincregssr[];
    static const struct OpIndIncRegSPC op_indincregspc[];
    static const struct OpIndIncRegDBR op_indincregdbr[];
    static const struct OpSRIndDecReg op_srinddecreg[];
    static const struct OpGBRIndDecReg op_gbrinddecreg[];
    static const struct OpVBRIndDecReg op_vbrinddecreg[];
    static const struct OpSSRIndDecReg op_ssrinddecreg[];
    static const struct OpSPCIndDecReg op_spcinddecreg[];
    static const struct OpSGRIndDecReg op_sgrinddecreg[];
    static const struct OpDBRIndDecReg op_dbrinddecreg[];
    static const struct OpImmReg op_immreg[];
    static const struct OpPcRelDisp op_pcreldisp[];
    static const struct OpRegReg op_regreg[];
    static const struct OpRegBank op_regbank[];
    static const struct OpBankReg op_bankreg[];
    static const struct OpBankIndDecReg on_bankinddecreg[];
    static const struct OpRegMach op_regmach[];
    static const struct OpRegMacl op_regmacl[];
    static const struct OpMachReg op_machreg[];
    static const struct OpMaclReg op_maclreg[];
    static const struct OpMachIndDecReg op_machinddecreg[];
    static const struct OpMaclIndDecReg op_maclinddecreg[];
    static const struct OpRegPr op_regpr[];
    static const struct OpPrReg op_prreg[];
    static const struct OpIndIncRegMach op_indincregmach[];
    static const struct OpIndIncRegMacl op_indincregmacl[];
    static const struct OpIndIncRegPr op_indincregpr[];
    static const struct OpPrIndDecReg op_prinddecreg[];
    static const struct OpRegIndReg op_regindreg[];
    static const struct OpIndRegReg op_indregreg[];
    static const struct OpRegIndDecReg op_reginddecreg[];
    static const struct OpIndIncRegReg op_indincregreg[];
    static const struct OpIndIncRegIndIncRegReg op_indincregindincreg[];
    static const struct OpDispRegDst op_dispregdst[];
    static const struct OpRegDispReg op_regdispreg[];
    static const struct OpDispRegSrc op_dispregsrc[];
    static const struct OpDispRegReg op_dispregreg[];
    static const struct OpRegRegPlusR0 op_regregplusr0[];
    static const struct OpRegPlusR0Reg op_regplusr0reg[];
    static const struct OpR0DispPlusGBR op_r0dispplusgbr[];
    static const struct OpDispPlusGBRR0 op_dispplusgbrr0[];
    static const struct OpDispPlusPCR0 op_disppluspcr0[];
    static const struct OpR0IndReg op_r0indreg[];
};
