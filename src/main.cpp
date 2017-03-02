/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016. 2017 snickerbockers
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
    char const *bios_path = NULL, *flash_path = NULL;
    char const *cmd = argv[0];
    bool enable_debugger = false;
    bool boot_hle = false, skip_ip_bin = false;
    char const *path_1st_read_bin = NULL, *path_ip_bin = NULL;

    while ((opt = getopt(argc, argv, "b:f:ghu")) != -1) {
        switch (opt) {
        case 'b':
            bios_path = optarg;
            break;
        case 'f':
            flash_path = optarg;
            break;
        case 'g':
            enable_debugger = true;
            break;
        case 'h':
#ifdef ENABLE_HLE_BOOT
            boot_hle = true;
#else
            std::cerr << "ERROR: unable to boot HLE: it's not enabled!" <<
                std::endl <<
                "rebuild with -DENABLE_HLE_BOOT" << std::endl;
            exit(1);
#endif
            break;
        case 'u':
#ifdef ENABLE_HLE_BOOT
            skip_ip_bin = true;
#else
            std::cerr << "ERROR: unable to boot HLE: it's not enabled!" <<
                std::endl <<
                "rebuild with -DENABLE_HLE_BOOT" << std::endl;
            exit(1);
#endif
        }
    }

    argv += optind;
    argc -= optind;

    if (skip_ip_bin && !boot_hle) {
        std::cerr << "Error: -u option is meaningless without -h!" << std::endl;
        exit(1);
    }

    if (boot_hle) {
        if (argc != 2) {
            print_usage(cmd);
            exit(1);
        }

        path_ip_bin = argv[0];
        path_1st_read_bin = argv[1];

        std::cout << "HLE boot enabled, loading IP.BIN from " << path_ip_bin <<
            " and loading 1ST_READ.BIN from " << path_1st_read_bin << std::endl;
    } else if (argc != 0 || !bios_path) {
        print_usage(cmd);
        exit(1);
    }

    try {
#ifdef ENABLE_HLE_BOOT
        if (boot_hle) {
            dreamcast_init_hle(path_ip_bin, path_1st_read_bin,
                               bios_path, flash_path, skip_ip_bin);
        } else {
#endif
            dreamcast_init(bios_path, flash_path);
#ifdef ENABLE_HLE_BOOT
        }
#endif

        if (enable_debugger) {
#ifdef ENABLE_DEBUGGER
            dreamcast_enable_debugger();
#else
            std::cerr << "WARNING: Unable to enable remote gdb stub." <<
                std::endl << "Please rebuild with -DENABLE_DEBUGGER=On" <<
                std::endl;
#endif
        }

        dreamcast_run();
    } catch (BaseException& err) {
        std::cerr << boost::diagnostic_information(err);
        exit(1);
    }

    exit(0);
}
