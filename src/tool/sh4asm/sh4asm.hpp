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

    void assemble_line(const std::string& inst);
private:
    class Token {
    public:
        enum Type {
            TEXT,
            COLON,
            COMMA,
            AT,
            POUND,
            REG,
            END_OF_LINE
        };
        Type tp;

        std::string txt;

        Token(const std::string& tok_txt);
    };

    typedef std::map<std::string, addr32_t> SymMap;
    typedef std::vector<inst_t> InstList;
    typedef std::vector<Token> TokenList;

    SymMap syms;
    InstList prog;

    /* remove any comments from the line. */
    std::string preprocess_line(const std::string& line);

    TokenList tokenize_line(const std::string& line);

    inst_t assemble_tokens(TokenList toks);
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
