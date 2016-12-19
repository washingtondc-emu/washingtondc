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
#include <iostream>

#include "BaseException.hpp"
#include "Dreamcast.hpp"

static void print_usage(char const *cmd) {
    std::cerr << "USAGE: " << cmd << " -b bios [-g]" << std::endl;
}

int main(int argc, char **argv) {
    int opt;
    bool is_running = true;
    char const *bios_path = NULL;
    char const *cmd = argv[0];
    bool enable_debugger = false;

    while ((opt = getopt(argc, argv, "b:g")) != -1) {
        switch (opt) {
        case 'b':
            bios_path = optarg;
            break;
        case 'g':
            enable_debugger = true;
            break;
        }
    }

    argv += optind;
    argc -= optind;

    if (argc != 0 || !bios_path) {
        print_usage(cmd);
        exit(1);
    }

    try {
        Dreamcast dc(bios_path);

        if (enable_debugger) {
#ifdef ENABLE_DEBUGGER
            dc.enable_debugger();
#else
            std::cerr << "WARNING: Unable to enable remote gdb stub." <<
                std::endl << "Please rebuild with -DENABLE_DEBUGGER=On" <<
                std::endl;
#endif
        }

        dc.run();
    } catch (BaseException& err) {
        std::cerr << boost::diagnostic_information(err);
        exit(1);
    }

    exit(0);
}
