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

#include <unistd.h>

#include <cctype>
#include <iostream>
#include <fstream>
#include <sstream>

#include "BaseException.hpp"

#include "sh4asm.hpp"

static bool only_whitespace(std::string const& str) {
    for (std::string::const_iterator it = str.begin(); it != str.end(); it++) {
        if (!isspace(*it))
            return false;
    }

    return true;
}

static unsigned to_hex(char ch) {
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;

    throw InvalidParamError("character is not hex");
}

static void print_usage(char const *cmd) {
    std::cerr << "Usage: " << cmd << " [-i input] [-o output] " <<
        "instruction" << std::endl;
}

int main(int argc, char **argv) {
    Sh4Prog prog;
    int opt;
    bool disas = false;
    char const *cmd = argv[0];
    char const *filename_in = NULL, *filename_out = NULL;

    std::ostream *output = &std::cout;
    std::istream *input = &std::cin;
    std::ofstream *file_out = NULL;
    std::ifstream *file_in = NULL;

    while ((opt = getopt(argc, argv, "di:o:")) != -1) {
        switch (opt) {
        case 'd':
            disas = true;
            break;
        case 'i':
            filename_in = optarg;
            break;
        case 'o':
            filename_out = optarg;
            break;
        default:
            print_usage(cmd);
            return 1;
        }
    }

    argv += optind;
    argc -= optind;

    if (argc != 0) {
        print_usage(cmd);
        return 1;
    }

    if (filename_in)
        input = file_in = new std::ifstream(filename_in);

    if (filename_out) {
        output = file_out = new std::ofstream(filename_out);
    }

    if (disas) {
        inst_t instr = 0;
        unsigned count = 0;
        while (input->good()) {
            char c = input->get();
            if (c == std::char_traits<char>::eof())
                break;

            if (isspace(c))
                continue;

            instr = (instr << 4) | inst_t(to_hex(c));

            count++;
            if (count == 4) {
                (*output) << prog.disassemble_line(instr) << std::endl;
                count = 0;
                instr = 0;
            }
        }

        if (count)
            (*output) << prog.disassemble_line(instr) << std::endl;
    } else {
        while (input->good()) {
            std::string line;
            std::getline(*input, line, '\n');

            if (!only_whitespace(line)) {
                inst_t instr = prog.assemble_line((line + "\n").c_str());
                (*output) << std::hex << instr << std::endl;
            }
        }
    }

    if (file_in)
        delete file_in;
    if (file_out)
        delete file_out;

    return 0;
}
