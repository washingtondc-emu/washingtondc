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
    bool bin_mode = false;

    std::ostream *output = &std::cout;
    std::istream *input = &std::cin;
    std::ofstream *file_out = NULL;
    std::ifstream *file_in = NULL;

    while ((opt = getopt(argc, argv, "bdi:o:")) != -1) {
        switch (opt) {
        case 'b':
            bin_mode = true;
            break;
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
        Sh4Prog::ByteList bin_dat;
        uint8_t dat = 0;
        bool even = true;

        if (bin_mode) {
            while (input->good()) {
                input->read((char*)&dat, 1);
                if (!input->fail())
                    bin_dat.push_back(dat);
            }
        } else {
            while (input->good()) {
                char c = input->get();

                if (c == std::char_traits<char>::eof())
                    break;

                if (isspace(c)) {
                    if (!even)
                        bin_dat.push_back(dat);
                    even = true;
                    continue;
                }

                if (even) {
                    dat = to_hex(c);
                } else {
                    dat = (dat << 4) | to_hex(c);
                    bin_dat.push_back(dat);
                }

                even = !even;
            }
        }

        if (!even)
            bin_dat.push_back(dat);

        prog.add_bin(bin_dat);
        (*output) << prog.get_prog_asm();
    } else {
        while (input->good()) {
            std::string line;
            std::getline(*input, line, '\n');

            if (!only_whitespace(line)) {
                line += "\n";
                prog.add_txt(line);
            }
        }

        Sh4Prog::ByteList bin_dat = prog.get_prog();
        for (Sh4Prog::ByteList::iterator it = bin_dat.begin();
             it != bin_dat.end(); it++) {
            if (bin_mode) {
                char dat = *it;
                output->write(&dat, 1);
            } else {
                (*output) << std::hex << unsigned(*it) << std::endl;
            }
        }
    }

    if (file_in)
        delete file_in;
    if (file_out)
        delete file_out;

    return 0;
}
