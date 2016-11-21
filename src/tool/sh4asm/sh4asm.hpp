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
#include "Inst.hpp"

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

    typedef std::vector<inst_t> InstList;

    addr32_t lookup_sym(const std::string& sym_name) const;

    inst_t assemble_line(const std::string& inst);
    std::string disassemble_line(inst_t inst) const;

    // assemble all instructions in txt and add it to this program.
    void assemble(const std::string& txt);

    const InstList& get_prog() const;

private:
    typedef std::map<std::string, addr32_t> SymMap;

    SymMap syms;
    InstList prog;

    /* remove any comments from the line. */
    std::string preprocess_line(const std::string& line);

    static TokList tokenize_line(const std::string& line);

    inst_t assemble_tokens(PtrnList toks);
    inst_t assemble_op_noargs(const std::string& inst);

    void add_label(const std::string& lbl);
};
