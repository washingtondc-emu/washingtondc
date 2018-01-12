/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2018 snickerbockers
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
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <libgen.h>

#include "dreamcast.h"
#include "gfx/gfx.h"
#include "glfw/window.h"
#include "io/io_thread.h"
#include "hw/pvr2/framebuffer.h"
#include "gfx/opengl/opengl_output.h"
#include "mount.h"
#include "gdi.h"
#include "config.h"
#include "log.h"

static void print_usage(char const *cmd) {
    fprintf(stderr, "USAGE: %s [options] [IP.BIN 1ST_READ.BIN]\n\n", cmd);

    fprintf(stderr, "WashingtonDC Dreamcast Emulator\n\n");

    fprintf(stderr, "OPTIONS:\n"
            "\t-b <bios_path>\tpath to dreamcast boot ROM\n"
            "\t-f <flash_path>\tpath to dreamcast flash ROM image\n"
            "\t-g\t\tenable remote GDB backend\n"
            "\t-d\t\tenable direct boot (skip BIOS)\n"
            "\t-u\t\tskip IP.BIN and boot straight to 1ST_READ.BIN (only "
            "valid for direct boot)\n"
            "\t-s\t\tpath to dreamcast system call image (only needed for "
            "direct boot)\n"
            "\t-t\t\testablish serial server over TCP port 1998\n"
            "\t-h\t\tdisplay this message and exit\n"
            "\t-m\t\tmount the given image in the GD-ROM drive\n");
}

int main(int argc, char **argv) {
    int opt;
    char const *bios_path = NULL, *flash_path = NULL;
    char const *cmd = argv[0];
    bool enable_debugger = false;
    bool boot_direct = false, skip_ip_bin = false;
    char *path_1st_read_bin = NULL, *path_ip_bin = NULL;
    char *path_syscalls_bin = NULL;
    char *path_gdi = NULL;
    bool enable_serial = false;
    bool enable_cmd_tcp = false;
    char const *title_content = NULL;
    struct mount_meta content_meta; // only valid if path_gdi is non-null

    while ((opt = getopt(argc, argv, "cb:f:s:m:gduht")) != -1) {
        switch (opt) {
        case 'b':
            bios_path = optarg;
            break;
        case 'c':
            enable_cmd_tcp = true;
            break;
        case 'f':
            flash_path = optarg;
            break;
        case 'g':
            enable_debugger = true;
            break;
        case 'd':
            boot_direct = true;
            break;
        case 'u':
            skip_ip_bin = true;
            break;
        case 's':
            path_syscalls_bin = optarg;
            break;
        case 't':
            enable_serial = true;
            break;
        case 'm':
            path_gdi = optarg;
            break;
        case 'h':
            print_usage(cmd);
            exit(0);
        }
    }

    argv += optind;
    argc -= optind;

    if (enable_debugger) {
#ifdef ENABLE_DEBUGGER
        config_set_dbg_enable(true);
#else
        LOG_ERROR("ERROR: Unable to enable remote gdb stub.\n"
                  "Please rebuild with -DENABLE_DEBUGGER=On\n");
        exit(1);
#endif
    } else {
#ifdef ENABLE_DEBUGGER
        config_set_dbg_enable(false);
#endif
    }

    if (path_gdi) {
        mount_gdi(path_gdi);
        if (mount_get_meta(&content_meta) == 0) {
            // dump meta to stdout and set the window title to the game title
            title_content = content_meta.title;

            LOG_INFO("GDI image %s mounted:\n", path_gdi);
            LOG_INFO("\thardware: %s\n", content_meta.hardware);
            LOG_INFO("\tmaker: %s\n", content_meta.maker);
            LOG_INFO("\tdevice info: %s\n", content_meta.dev_info);
            LOG_INFO("\tregion: %s\n", content_meta.region);
            LOG_INFO("\tperipheral support: %s\n", content_meta.periph_support);
            LOG_INFO("\tproduct id: %s\n", content_meta.product_id);
            LOG_INFO("\tproduct version: %s\n", content_meta.product_version);
            LOG_INFO("\trelease date: %s\n", content_meta.rel_date);
            LOG_INFO("\tboot file: %s\n", content_meta.boot_file);
            LOG_INFO("\tcompany: %s\n", content_meta.company);
            LOG_INFO("\ttitle: %s\n", content_meta.title);
        }
    }

    if (boot_direct) {
        if (argc != 2) {
            print_usage(cmd);
            exit(1);
        }

        if (!path_syscalls_bin) {
            LOG_ERROR("Error: cannot direct-boot without a system call "
                      "table (-s flag).\n");
            exit(1);
        }

        path_ip_bin = argv[0];
        path_1st_read_bin = argv[1];

        LOG_INFO("direct boot enbaled, loading IP.BIN from %s and loading "
                 "1ST_READ.BIN from %s\n", path_ip_bin, path_1st_read_bin);
        title_content = basename(path_1st_read_bin);
    } else if (argc != 0 || !bios_path) {
        print_usage(cmd);
        exit(1);
    }

    if (boot_direct) {
        if (skip_ip_bin)
            config_set_boot_mode(DC_BOOT_DIRECT);
        else
            config_set_boot_mode(DC_BOOT_IP_BIN);
        config_set_ip_bin_path(path_ip_bin);
        config_set_exec_bin_path(path_1st_read_bin);
        config_set_syscall_path(path_syscalls_bin);
    } else {
        if (skip_ip_bin) {
            LOG_ERROR("Error: -u option is meaningless with -d!\n");
            exit(1);
        }

        if (path_syscalls_bin) {
            LOG_ERROR("Error: -s option is meaningless when not "
                      "performing a direct boot (-d option)\n");
            exit(1);
        }

        config_set_boot_mode(DC_BOOT_FIRMWARE);
    }

    if (!(boot_direct || path_gdi))
        title_content = "firmware";

    config_set_dc_bios_path(bios_path);
    config_set_dc_flash_path(flash_path);

    dreamcast_init(enable_cmd_tcp);

    framebuffer_init(640, 480);
    win_init(640, 480, title_content);
    gfx_init(640, 480);
    io_thread_launch();

    config_set_enable_cmd_tcp(enable_cmd_tcp);
    config_set_ser_srv_enable(enable_serial);

    dreamcast_run();

    LOG_INFO("killing the window...\n");
    win_cleanup();

    LOG_INFO("Waiting for io_thread to exit...\n");
    io_thread_join();
    LOG_INFO("io_thread has exited.\n");

    if (mount_check())
        mount_eject();

    exit(0);
}
