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
#include "window.hpp"

static void print_usage(char const *cmd) {
    std::cerr << "USAGE: " << cmd << " [options] [IP.BIN 1ST_READ.BIN]" <<
        std::endl << std::endl;

    std::cerr << "WashingtonDC Dreamcast Emulator" << std::endl << std::endl;

    std::cerr << "OPTIONS:" << std::endl;
    std::cerr << "\t-b <bios_path>\tpath to dreamcast boot ROM" << std::endl;
    std::cerr << "\t-f <flash_path>\tpath to dreamcast flash ROM "
        "image" << std::endl;
    std::cerr << "\t-g\t\tenable remote GDB backend" << std::endl;
    std::cerr << "\t-d\t\tenable direct boot (skip BIOS)" << std::endl;
    std::cerr << "\t-u\t\tskip IP.BIN and boot straight to 1ST_READ.BIN (only "
        "valid for direct boot)" << std::endl;
    std::cerr << "\t-s\t\tpath to dreamcast system call image (only needed for "
        "direct boot)" << std::endl;
    std::cerr << "\t-t\t\testablish serial server over TCP port 1998" << std::endl;
    std::cerr << "\t-h\t\tdisplay this message and exit" << std::endl;
}

int main(int argc, char **argv) {
    int opt;
    char const *bios_path = NULL, *flash_path = NULL;
    char const *cmd = argv[0];
    bool enable_debugger = false;
    bool boot_direct = false, skip_ip_bin = false;
    char const *path_1st_read_bin = NULL, *path_ip_bin = NULL;
    char const *path_syscalls_bin = NULL;
    bool enable_serial = false;

    while ((opt = getopt(argc, argv, "b:f:s:gduht")) != -1) {
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
        case 'd':
#ifdef ENABLE_DIRECT_BOOT
            boot_direct = true;
#else
            std::cerr << "ERROR: unable to boot in direct-mode: it's "
                "not enabled!" << std::endl <<
                "rebuild with -DENABLE_DIRECT_BOOT" << std::endl;
            exit(1);
#endif
            break;
        case 'u':
#ifdef ENABLE_DIRECT_BOOT
            skip_ip_bin = true;
#else
            std::cerr << "ERROR: unable to boot in direct-mode: it's "
                "not enabled!" << std::endl <<
                "rebuild with -DENABLE_DIRECT_BOOT" << std::endl;
            exit(1);
#endif
            break;
        case 's':
            path_syscalls_bin = optarg;
            break;
        case 't':
            enable_serial = true;
            break;
        case 'h':
            print_usage(cmd);
            exit(0);
        }
    }

    argv += optind;
    argc -= optind;

    if (skip_ip_bin && !boot_direct) {
        std::cerr << "Error: -u option is meaningless without -d!" << std::endl;
        exit(1);
    }

    if (path_syscalls_bin && !boot_direct)
        std::cerr << "Warning: -s option is meaningless when not performing a "
            "direct boot (-d option)" << std::endl;

    if (boot_direct) {
        if (argc != 2) {
            print_usage(cmd);
            exit(1);
        }

        path_ip_bin = argv[0];
        path_1st_read_bin = argv[1];

        std::cout << "direct boot enabled, loading IP.BIN from " <<
            path_ip_bin << " and loading 1ST_READ.BIN from " <<
            path_1st_read_bin << std::endl;
    } else if (argc != 0 || !bios_path) {
        print_usage(cmd);
        exit(1);
    }

    try {
#ifdef ENABLE_DIRECT_BOOT
        if (boot_direct) {
            dreamcast_init_direct(path_ip_bin, path_1st_read_bin,
                                  bios_path, flash_path, path_syscalls_bin,
                                  skip_ip_bin);
        } else {
#endif
            dreamcast_init(bios_path, flash_path);
#ifdef ENABLE_DIRECT_BOOT
        }
#endif

        if (enable_serial) {
#ifdef ENABLE_SERIAL_SERVER
            dreamcast_enable_serial_server();
#else
            std::cerr << "WARNING: Unable to enable TCP serial server." <<
                std::endl << "Please rebuild with -DENABLE_SERIAL_SERVER" <<
                std::endl;
#endif
        }

        if (enable_debugger) {
#ifdef ENABLE_DEBUGGER
            dreamcast_enable_debugger();
#else
            std::cerr << "WARNING: Unable to enable remote gdb stub." <<
                std::endl << "Please rebuild with -DENABLE_DEBUGGER=On" <<
                std::endl;
#endif
        }

        win_init(320, 240);

        dreamcast_run();
    } catch (BaseException& err) {
        std::cerr << boost::diagnostic_information(err);
        exit(1);
    }

    exit(0);
}
