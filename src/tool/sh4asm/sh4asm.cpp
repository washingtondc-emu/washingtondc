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

inst_t Sh4Prog::assemble_line(const std::string& inst) {
    TokList toks = tokenize_line(preprocess_line(inst));
    PtrnList patterns = get_patterns();

    for (PtrnList::iterator it = patterns.begin(); it != patterns.end();
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
