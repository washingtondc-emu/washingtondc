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

#include <cctype>
#include <iostream>

#include "types.hpp"
#include "BaseException.hpp"

#include "sh4asm.hpp"

Sh4Prog::Sh4Prog() {
}

Sh4Prog::Sh4Prog(ByteList const& prog) {
    this->prog = prog;
}

Sh4Prog::Sh4Prog(std::string const& asm_txt) {
    add_txt(asm_txt);
}

unsigned Sh4Prog::to_hex(char ch) {
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;

    BOOST_THROW_EXCEPTION(InvalidParamError("character is not hex"));
}

addr32_t Sh4Prog::lookup_sym(const std::string& sym_name) const {
    SymMap::const_iterator it = syms.find(sym_name);

    if (it == syms.end())
        BOOST_THROW_EXCEPTION(BadSymbolError(sym_name.c_str()));

    return it->second;
}

void Sh4Prog::add_txt(const std::string& line) {
    LineTokenizer tok(line, boost::char_separator<char>("\n"));

    for (LineTokenizer::iterator it = tok.begin(); it != tok.end(); it++) {
        add_single_line(*it);
    }
}

void Sh4Prog::add_single_line(const std::string& line) {
    ByteList ret;

    if (line.at(0) == '.') {
        if (line.substr(0, 5) != ".byte")
            BOOST_THROW_EXCEPTION(ParseError("Unrecognized assembler directive"));

        unsigned char_count = 0;
        uint8_t data = 0;
        bool found_space = false;
        std::string hex_str = line.substr(line.find_first_of(" \t") + 1);
        for (std::string::iterator it = hex_str.begin(); it != hex_str.end();
             it++) {
            if (isspace(*it)) {
                found_space = true;
            } else if (found_space) {
                BOOST_THROW_EXCEPTION(ParseError("Garbage data in "
                                                 ".byte directive"));
            } else if (char_count < 2) {
                data = (data << 4) | to_hex(*it);
                char_count++;
            } else {
                BOOST_THROW_EXCEPTION(ParseError("more than a byte of data in "
                                                 "a .byte directive"));
            }
        }

        ret.push_back(data);
    } else {
        inst_t inst = assemble_inst(line);

        ret.push_back(uint8_t(inst & 0xff));
        ret.push_back(uint8_t(inst >> 8));
    }

    prog.insert(prog.end(), ret.begin(), ret.end());
    prog_asm += line;
}

void Sh4Prog::add_bin(const ByteList& bin_data) {
    prog.insert(prog.end(), bin_data.begin(), bin_data.end());
    disassemble();
}

void Sh4Prog::disassemble() {
    size_t idx = 0;
    prog_asm.clear();

    while (idx < prog.size()) {
        uint8_t byte1 = prog.at(idx);
        if ((idx + 1) < prog.size()) {
            uint8_t byte2 = prog.at(idx + 1);

            try {
                // try to assemble the two bytes
                inst_t inst = inst_t(byte1) | (inst_t(byte2) << 8);
                std::string instr_txt = disassemble_inst(inst);

                // if disassemble_inst did not throw a ParseError, then this
                // was a valid instruction
                prog_asm += instr_txt;
                idx += 2;
            } catch (ParseError& err) {
                // unrecognized opcode, go put this in as a .byte
                std::stringstream byte_txt;
                byte_txt << ".byte " << std::hex << unsigned(byte1) << "\n";
                prog_asm += byte_txt.str();
                idx++;
            }
        } else {
            // only one byte in the stream
            std::stringstream byte_txt;
            byte_txt << ".byte " << std::hex << unsigned(byte1) << "\n";
            prog_asm += byte_txt.str();
            idx++;
        }
    }
}

inst_t Sh4Prog::assemble_inst(const std::string& inst) const {
    TokList toks = tokenize_line(preprocess_line(inst));
    PtrnList patterns = get_patterns();

    for (PtrnList::iterator it = patterns.begin(); it != patterns.end();
         ++it) {
        if ((*it)->matches(toks.rbegin(), toks.rend())) {
            return (*it)->assemble();
        }
    }

    BOOST_THROW_EXCEPTION(ParseError("Unrecognized opcode"));
}

std::string Sh4Prog::disassemble_inst(inst_t inst) const {
    PtrnList patterns = get_patterns();

    for (PtrnList::iterator it = patterns.begin(); it != patterns.end();
         ++it) {
        if ((*it)->matches(inst)) {
            return (*it)->disassemble(inst);
        }
    }

    BOOST_THROW_EXCEPTION(ParseError("Unrecognized instruction"));
}

std::string Sh4Prog::preprocess_line(const std::string& line) {
    size_t comment_start = line.find_first_of('!');

    if (comment_start == std::string::npos)
        return line;

    return line.substr(0, comment_start);
}

TokList Sh4Prog::tokenize_line(const std::string& line) {
    Token cur_tok;
    TokList tok_list;

    for (std::string::const_iterator it = line.begin(); it != line.end();
         ++it) {
        char cur_char = *it;

        if (cur_char == ' ' || cur_char == '\t' || cur_char == '\n') {
            if (cur_tok.size()) {
                tok_list.push_back(cur_tok);
                cur_tok.clear();
            }
        } else if (cur_char == ':' || cur_char == ',' ||
                   cur_char == '@' || cur_char == '#' ||
                   cur_char == '(' || cur_char == ')' ||
                   cur_char == '+' || cur_char == '-') {
            if (cur_tok.size()) {
                tok_list.push_back(cur_tok);
            }
            std::string cur_char_as_str(1, cur_char);
            tok_list.push_back(cur_char_as_str);
            cur_tok.clear();
        } else {
            cur_tok.push_back(cur_char);
        }
    }

    if (cur_tok.size()) {
        tok_list.push_back(cur_tok);
        cur_tok.clear();
    }

    return tok_list;
}

void Sh4Prog::add_label(const std::string& lbl) {
    syms[lbl] = prog.size() - 1;
}

const Sh4Prog::ByteList& Sh4Prog::get_prog() const {
    return prog;
}

const std::string& Sh4Prog::get_prog_asm() const {
    return prog_asm;
}

void Sh4Prog::assemble(const std::string& txt) {
    LineTokenizer tok(txt, boost::char_separator<char>("\n"));

    for (LineTokenizer::iterator it = tok.begin(); it != tok.end(); it++) {
        prog.push_back(assemble_inst(*it));
    }
}
