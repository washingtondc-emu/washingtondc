/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016, 2017 snickerbockers
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

#include <boost/tokenizer.hpp>
#include <boost/exception/exception.hpp>

#include "BaseException.hpp"
#include "types.hpp"
#include "Inst.hpp"

class BadSymbolError : public BaseException {
public:
    BadSymbolError(char const *sym) {
        this->sym = sym;
    }

    char const *what() const throw() {
        return sym;
    }
private:
    char const *sym;
};

class ParseError : public BaseException {
public:
    ParseError(char const *desc) {
        this->desc = desc;
    }

    char const *what() const throw() {
        return desc;
    }
private:
    char const *desc;
};

class Sh4Prog {
public:
    typedef std::vector<uint8_t> ByteList;
    typedef std::map<std::string, addr32_t> SymMap;

    Sh4Prog();
    Sh4Prog(ByteList const& prog);
    Sh4Prog(std::string const& asm_txt);

    addr32_t lookup_sym(const std::string& sym_name) const;

    /*
     * add the given line to the program.  This also adds its hex-string
     * equivalent to the binary data
     */
    void add_txt(const std::string& line);

    /*
     * add all the bytes in bin_data to the program.
     *
     * This regenerates the stored assembly text, which means that any fancy
     * whitespace will get removed if you try to use add_bin after already using
     * add_txt
     */
    void add_bin(const ByteList& bin_data);

    /*
     * assemble or disassemble a single instruction.
     * This does not add the instruction to this Sh4Prog.
     */
    static ByteList assemble_single_line(const std::string& inst,
                                         SymMap *syms = NULL);
    static std::string disassemble_single(const ByteList& bin,
                                          size_t *idx_next,
                                          bool hex_comments = false);
    static inst_t assemble_inst(const std::string& inst, SymMap *syms = NULL);
    static std::string disassemble_inst(inst_t inst);

    // assemble all instructions in txt and add it to this program.
    void assemble(const std::string& txt);

    // get program binary data
    const ByteList& get_prog() const;

    // get program assembly
    const std::string& get_prog_asm() const;

private:
    typedef boost::tokenizer<boost::char_separator<char> > LineTokenizer;

    SymMap syms;
    ByteList prog;
    std::string prog_asm;

    /* remove any comments from the line. */
    static std::string preprocess_line(const std::string& line);

    /*
     * backend implementation of add_txt.  This function requires its argument
     * to be a single line of text.  add_txt breaks up its argument into
     * multiple lines and feeds them to this function.
     */
    void add_single_line(const std::string& line);

    static TokList tokenize_line(const std::string& line);

    // given a hex-character, return its value
    static unsigned hex_to_bin(char ch);

    // given a value, return it as an ascii hexadecimal character
    static char bin_to_hex(uint8_t val);

    inst_t assemble_tokens(PtrnList toks);
    inst_t assemble_op_noargs(const std::string& inst);

    void add_label(const std::string& lbl);

    /*
     * Regenerate prog_asm.  We do this every time a new ByteList is added
     * because  there could be a trailing .byte at the end of the last ByteList
     * that forms a valid instruction when combined with the new ByteList.
     */
    void disassemble();

    static const unsigned COMMENT_COL = 20;
    static std::string hex_comment(uint8_t val);
    static std::string hex_comment(uint16_t val);
};
