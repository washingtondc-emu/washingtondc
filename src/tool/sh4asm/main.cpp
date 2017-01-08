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

    BOOST_THROW_EXCEPTION(InvalidParamError("character is not hex"));
}

struct options {
    char const *filename_in, *filename_out;
    bool bin_mode;
    bool print_addrs;
    bool disas;
};

static void print_usage(char const *cmd) {
    std::cerr << "Usage: " << cmd << " [-i input] [-o output] " <<
        "instruction" << std::endl;
}

// used to print the program address of a given instruction (if enabled)
static void print_addr(std::ostream *output, addr32_t addr) {
    (*output) << std::hex << std::setfill('0') <<
        std::setw(8) << addr << ":    ";
}

void do_disasm(std::istream *input, std::ostream *output,
               struct options const *options) {
    Sh4Prog::ByteList bin_dat;
    uint8_t dat = 0;
    bool even = true;
    addr32_t pc = 0;

    if (options->bin_mode) {
        while (input->good()) {
            input->read((char*)&dat, 1);
            if (!input->fail())
                bin_dat.push_back(dat);

            if (bin_dat.size() >= 2) {
                size_t idx = 0;
                do {
                    if (options->print_addrs)
                        print_addr(output, pc);

                    size_t old_idx = idx;
                    (*output) << Sh4Prog::disassemble_single(bin_dat, &idx);
                    size_t adv = idx - old_idx;
                    pc += adv;
                } while (idx < bin_dat.size());

                bin_dat.clear();
            }
        }

        if (bin_dat.size()) {
            size_t idx = 0;
            do {
                if (options->print_addrs)
                    print_addr(output, pc);

                size_t old_idx = idx;
                (*output) << Sh4Prog::disassemble_single(bin_dat, &idx);
                size_t adv = idx - old_idx;
                pc += adv;
            } while (idx < bin_dat.size());
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

            if (bin_dat.size() >= 2) {
                size_t idx = 0;
                do {
                    if (options->print_addrs)
                        print_addr(output, pc);

                    size_t old_idx = idx;
                    (*output) << Sh4Prog::disassemble_single(bin_dat, &idx);
                    size_t adv = idx - old_idx;
                    pc += adv;
                } while (idx < bin_dat.size());

                bin_dat.clear();
            }
        }

        if (!even)
            bin_dat.push_back(dat);

        if (bin_dat.size()) {
            size_t idx = 0;
            do {
                if (options->print_addrs)
                    print_addr(output, pc);

                size_t old_idx = idx;
                (*output) << Sh4Prog::disassemble_single(bin_dat, &idx);
                size_t adv = idx - old_idx;
                pc += adv;
            } while (idx < bin_dat.size());
        }
    }
}

void do_asm(std::istream *input, std::ostream *output,
            struct options const *options) {
    while (input->good()) {
        std::string line;
        std::getline(*input, line, '\n');

        /*
         * filter out addresses left by the -l option in disassembler mode
         * The way this is implemented effectively turns everything before
         * the colon into a comment (because we ignore it), but users
         * should not actually use rely on this behavior because eventually
         * it may change.  Colons will probably be used as a suffix for
         * labels at some point since that seems to be the standard way to
         * implement labels across most assemblers.
         */
        size_t colon_idx = line.find_first_of(':');
        if (colon_idx != std::string::npos)
            line = line.substr(colon_idx + 1);

        // trim leading whitespace
        size_t first_non_whitespace_idx = line.find_first_not_of(" \t");
        if (first_non_whitespace_idx != std::string::npos)
            line = line.substr(first_non_whitespace_idx);

        if (!only_whitespace(line))
            line += "\n";

        if (line.size()) {
            Sh4Prog::ByteList bin_dat = Sh4Prog::assemble_single_line(line);
            for (Sh4Prog::ByteList::iterator it = bin_dat.begin();
                 it != bin_dat.end(); it++) {
                if (options->bin_mode) {
                    char dat = *it;
                    output->write(&dat, 1);
                } else {
                    (*output) << std::hex << unsigned(*it) << std::endl;
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    int err_code = 0;
    int opt;
    char const *cmd = argv[0];
    struct options options;

    memset(&options, 0, sizeof(options));

    std::ostream *output = &std::cout;
    std::istream *input = &std::cin;
    std::ofstream *file_out = NULL;
    std::ifstream *file_in = NULL;

    while ((opt = getopt(argc, argv, "bdli:o:")) != -1) {
        switch (opt) {
        case 'b':
            options.bin_mode = true;
            break;
        case 'd':
            options.disas = true;
            break;
        case 'l':
            options.print_addrs = true;
            break;
        case 'i':
            options.filename_in = optarg;
            break;
        case 'o':
            options.filename_out = optarg;
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

    if (options.filename_in)
        input = file_in = new std::ifstream(options.filename_in);

    if (options.filename_out) {
        output = file_out = new std::ofstream(options.filename_out);
    }

    try {
        if (options.disas) {
            do_disasm(input, output, &options);
        } else {
            do_asm(input, output, &options);
        }
    } catch (BaseException& exc) {
        std::cerr << boost::diagnostic_information(exc);
        err_code = 1;
    }

    if (file_in)
        delete file_in;
    if (file_out)
        delete file_out;

    return err_code;
}
