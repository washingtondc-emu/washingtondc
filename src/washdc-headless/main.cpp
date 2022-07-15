/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019, 2020, 2022 snickerbockers
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
#include <string.h>

#include "washdc/hostfile.h"
#include "washdc/washdc.h"
#include "washdc/sound_intf.h"
#include "washdc/buildconfig.h"
#include "washdc/win.h"
#include "washdc_getopt.h"

#include "console_config.hpp"
#include "gfx_null.hpp"

#ifdef USE_LIBEVENT
#include "frontend_io/io_thread.hpp"
#endif

#ifdef ENABLE_DEBUGGER
#include "frontend_io/gdb_stub.hpp"
#include "frontend_io/washdbg_tcp.hpp"
#endif

#ifdef ENABLE_TCP_SERIAL
#include "frontend_io/serial_server.hpp"
#endif

#include "stdio_hostfile.hpp"
#include "paths.hpp"

static void null_sound_init(void);
static void null_sound_cleanup(void);
static void null_sound_submit_samples(washdc_sample_type *samples, unsigned count);

static void print_usage(char const *cmd);

static void
wizard(path_string console_name, path_string dc_bios_path,
       path_string dc_flash_path);

static struct washdc_hostfile_api hostfile_api;
static struct washdc_sound_intf snd_intf;
static struct win_intf null_win_intf;

struct washdc_gameconsole const *console;

static void null_win_init(unsigned width, unsigned height);
static void null_win_check_events(void);
static void null_win_run_once_on_suspend(void);
static void null_win_update(void);
static void null_win_make_context_current(void);
static int null_win_get_width(void);
static int null_win_get_height(void);
static void null_win_update_title(void);

static bool strieq(char const *str1, char const *str2) {
    if (strlen(str1) != strlen(str2))
        return false;
    while (*str1 && *str2) {
        if (toupper(*str1++) != toupper(*str2++))
            return false;
    }
    return true;
}

int main(int argc, char **argv) {
    int opt;
    char const *cmd = argv[0];
    bool enable_debugger = false;
    bool enable_washdbg = false;
    bool direct_boot = false;
    char *path_game = NULL;
    bool enable_serial = false;
    bool enable_jit = false, enable_native_jit = false,
        enable_interpreter = false, inline_mem = true;
    bool log_stdout = false, log_verbose = false;
    struct washdc_launch_settings settings = { };
    char const *console_name = NULL;
    bool launch_wizard = false;
    char const *dc_bios_path = NULL, *dc_flash_path = NULL;
    bool write_to_flash_mem = false;

    create_cfg_dir();
    create_data_dir();
    create_screenshot_dir();

    while ((opt = washdc_getopt(argc, argv, "w:b:f:c:s:m:d:u:g:htjxpnlv")) != -1) {
        switch (opt) {
        case 'g':
            enable_debugger = true;
            if (strcmp(washdc_optarg, "washdbg") == 0) {
                enable_washdbg = true;
                enable_debugger = false;
            }
            break;
        case 'd':
            fprintf(stderr,
                    "*************************************************************\n"
                    "**\n"
                    "** SUPPORT FOR THE '-d IP.BIN' OPTION HAS BEEN REMOVED\n"
                    "**\n"
                    "*************************************************************\n");
            exit(1);
        case 'u':
            fprintf(stderr,
                    "*************************************************************\n"
                    "**\n"
                    "** DUE TO RECENT CHANGES, THE -u OPTION HAS BEEN MERGED "
                    "INTO THE -m OPTION.\n"
                    "** PLEASE RUN WASHINGTONDC WITH \"-m %s\"\n"
                    "**\n"
                    "*************************************************************\n",
                    washdc_optarg);
            exit(1);
        case 's':
            fprintf(stderr,
                    "*************************************************************\n"
                    "**\n"
                    "** EXCELLENT NEWS!!!!!!!!!!!!!!\n"
                    "** THE OLD '-s syscalls.bin' ARGUMENT IS NO LONGER REQUIRED\n"
                    "** PLEASE REMOVE THE -s ARGUMENT FROM YOUR INVOCATION OF "
                    "WASHINGTONDC\n"
                    "** AND RUN IT AGAIN\n"
                    "**\n"
                    "*************************************************************\n");
            exit(1);
        case 't':
            enable_serial = true;
            break;
        case 'm':
            path_game = washdc_optarg;
            break;
        case 'h':
            print_usage(cmd);
            exit(0);
        case 'j':
            enable_jit = true;
            break;
        case 'x':
            enable_native_jit = true;
            break;
        case 'p':
            enable_interpreter = true;
            break;
        case 'n':
            inline_mem = false;
            break;
        case 'l':
            log_stdout = true;
            break;
        case 'v':
            log_verbose = true;
            break;
        case 'c':
            console_name = washdc_optarg;
            break;
        case 'b':
            dc_bios_path = washdc_optarg;
            break;
        case 'f':
            dc_flash_path = washdc_optarg;
            break;
        case 'w':
            launch_wizard = true;
            break;
        default:
            print_usage(cmd);
            exit(0);
        }
    }

    bool have_console_name = console_name;

    if (!dc_flash_path) {
        /*
         * We only write to flash_mem when console-mode is enabled because that
         * way, WashingtonDC has its own copy of the flash image so we don't
         * need to worry about overwriting something the user wants to preserve.
         */
        write_to_flash_mem = true;
    }

    if (launch_wizard) {
        if (!console_name)
            console_name = "default_dc";
        path_string bios_str, flash_str;
        if (dc_bios_path)
            bios_str = dc_bios_path;
        if (dc_flash_path)
            flash_str = dc_flash_path;
        wizard(console_name, bios_str, flash_str);
    }

    settings.log_to_stdout = log_stdout;
    settings.log_verbose = log_verbose;
    settings.write_to_flash = write_to_flash_mem;

    hostfile_api.open = file_stdio_open;
    hostfile_api.close = file_stdio_close;
    hostfile_api.seek = file_stdio_seek;
    hostfile_api.tell = file_stdio_tell;
    hostfile_api.read = file_stdio_read;
    hostfile_api.write = file_stdio_write;
    hostfile_api.flush = file_stdio_flush;
    hostfile_api.open_cfg_file = open_cfg_file;
    hostfile_api.open_screenshot = open_screenshot;
#ifdef _WIN32
    hostfile_api.pathsep = '\\';
#else
    hostfile_api.pathsep = '/';
#endif

    settings.hostfile_api = &hostfile_api;

    if (enable_debugger && enable_washdbg) {
        fprintf(stderr, "You can't enable WashDbg and GDB at the same time\n");
        exit(1);
    }

    if (enable_debugger || enable_washdbg) {
        if (enable_jit || enable_native_jit) {
            fprintf(stderr, "Debugger enabled - this overrides the jit "
                    "compiler and sets WashingtonDC to interpreter mode\n");
            enable_jit = false;
            enable_native_jit = false;
        }
        enable_interpreter = true;

        if (washdc_have_debugger()) {
            settings.dbg_enable = true;
            settings.washdbg_enable = enable_washdbg;
        } else {
            fprintf(stderr, "ERROR: Unable to enable remote gdb stub.\n"
                    "Please rebuild with -DENABLE_DEBUGGER=On\n");
            exit(1);
        }
    } else {
        settings.dbg_enable = false;
    }

#ifdef ENABLE_DEBUGGER
    if (enable_debugger && !enable_washdbg)
        settings.dbg_intf = &gdb_frontend;
    else if (!enable_debugger && enable_washdbg)
        settings.dbg_intf = &washdbg_frontend;
#else
    enable_debugger = false;
    enable_washdbg = false;
#endif

    if (enable_interpreter && (enable_jit || enable_native_jit)) {
        fprintf(stderr, "ERROR: You can\'t use the interpreter and the JIT at "
                "the same time, silly!\n");
        exit(1);
    }

    if (washdc_have_x86_64_jit()) {
        // enable the jit (with x86_64 backend) by default
        if (!(enable_jit || enable_native_jit || enable_interpreter))
            enable_native_jit = true;
    } else {
        // enable the jit (with jit-interpreter) by default
        if (!(enable_jit || enable_interpreter))
            enable_jit = true;
    }

    settings.inline_mem = inline_mem;
    settings.enable_jit = enable_jit || enable_native_jit;

    if (washdc_have_x86_64_jit()) {
        settings.enable_native_jit = enable_native_jit;
    } else {
        if (enable_native_jit) {
            fprintf(stderr, "ERROR: the native x86_64 jit backend was not enabled "
                    "for this build configuration.\n"
                    "Rebuild WashingtonDC with -DENABLE_JIT_X86_64=On to enable "
                    "the native x86_64 jit backend.\n");
            exit(1);
        }
    }

    direct_boot = false;
    if (path_game) {
        char const *ext = strrchr(path_game, '.');
        if (ext && (strieq(ext, ".bin") || strieq(ext, ".elf"))) {
            direct_boot = true;
            printf(".BIN OR .ELF FILE DETECTED; DIRECT-BOOT MODE ENABLED\n");
        }
    }

    if (direct_boot) {
        settings.boot_mode = WASHDC_BOOT_DIRECT;
        settings.path_1st_read_bin = path_game;
    } else {
        settings.boot_mode = WASHDC_BOOT_FIRMWARE;
    }

    if (dc_bios_path)
        settings.path_dc_bios = dc_bios_path;
    else if (have_console_name)
        settings.path_dc_bios = console_get_firmware_path(console_name);
    if (dc_flash_path)
        settings.path_dc_flash = dc_flash_path;
    else if (have_console_name)
        settings.path_dc_flash = console_get_flashrom_path(console_name);
    if (have_console_name)
        settings.path_rtc = console_get_rtc_path(console_name);
    settings.enable_serial = enable_serial;
    settings.path_gdi = direct_boot ? NULL : path_game;

    null_win_intf.check_events = null_win_check_events;
    null_win_intf.run_once_on_suspend = null_win_run_once_on_suspend;
    null_win_intf.update = null_win_update;
    null_win_intf.make_context_current = null_win_make_context_current;
    null_win_intf.update_title = null_win_update_title;
    null_win_intf.get_width = null_win_get_width;
    null_win_intf.get_height = null_win_get_height;

    settings.win_intf = &null_win_intf;

#ifdef ENABLE_TCP_SERIAL
    settings.sersrv = &sersrv_intf;
#endif

    snd_intf.init = null_sound_init;
    snd_intf.cleanup = null_sound_cleanup;
    snd_intf.submit_samples = null_sound_submit_samples;

    settings.sndsrv = &snd_intf;

    settings.gfx_rend_if = null_rend_if_get();

#ifdef USE_LIBEVENT
    io::init();
#endif

    null_win_init(640, 480); // made up fictional resolution

    console = washdc_init(&settings);

    washdc_run();

#ifdef USE_LIBEVENT
    io::kick();
    io::cleanup();
#endif

    washdc_cleanup();

    exit(0);

    return 0;
}

static void print_usage(char const *cmd) {
    fprintf(stderr, "USAGE: %s [options] -b <dc_bios.bin> -f <dc_flash.bin> -m "
            "<path_to_game>\n\n", cmd);

    fprintf(stderr, "WashingtonDC Dreamcast Emulator\n\n");

    fprintf(stderr, "OPTIONS:\n"
            "\t-c <console_name>\tname of console to boot\n"
            "\t-b <bios_path>\tpath to dreamcast boot ROM\n"
            "\t-f <flash_path>\tpath to dreamcast flash ROM image\n"
            "\t-g gdb\t\tenable remote GDB backend\n"
            "\t-g washdbg\tenable remote WashDbg backend\n"
            "\t-u\t\tdirect-boot 1ST_READ.BIN\n"
            "\t-t\t\testablish serial server over TCP port 1998\n"
            "\t-h\t\tdisplay this message and exit\n"
            "\t-l\t\tdump logs to stdout\n"
            "\t-m\t\tmount the given image in the GD-ROM drive\n"
            "\t-n\t\tdon't inline memory reads/writes into the jit\n"
            "\t-p\t\tdisable the dynarec and enable the interpreter instead\n"
            "\t-j\t\tenable dynamic recompiler (as opposed to interpreter)\n"
            "\t-v\t\tenable verbose logging\n"
            "\t-x\t\tenable native x86_64 dynamic recompiler backend "
            "(default)\n");
}

static void null_sound_init(void) {
}

static void null_sound_cleanup(void) {
}

static void null_sound_submit_samples(washdc_sample_type *samples, unsigned count) {
}

static void wizard(path_string console_name, path_string dc_bios_path,
                   path_string dc_flash_path) {
    path_string firmware_path, flash_path;

    if (dc_bios_path.size()) {
        firmware_path = dc_bios_path;
    } else {
        std::cout << "Please enter the path to your Dreamcast firmware image:" << std::endl;
        std::cin >> firmware_path;
    }

    if (dc_flash_path.size()) {
        flash_path = dc_flash_path;
    } else {
        std::cout << "Please enter the path to your Dreamcast flash image:" << std::endl;
        std::cin >> flash_path;
    }

    path_string firmware_out_path =
        console_get_firmware_path(console_name.c_str());
    path_string flash_out_path =
        console_get_flashrom_path(console_name.c_str());

    create_console_dir(console_name.c_str());

    FILE *firmware_in = fopen(firmware_path.c_str(), "rb");
    if (!firmware_in) {
        std::cerr << "ERROR: unable to read from " << firmware_path << std::endl;
        exit(1);
    }

    FILE *firmware_out = fopen(firmware_out_path.c_str(), "wb");
    if (!firmware_out) {
        std::cerr << "ERROR: unable to write to " << firmware_out_path << std::endl;
        exit(1);
    }

    FILE *flash_in = fopen(flash_path.c_str(), "rb");
    if (!flash_in) {
        std::cerr << "ERROR: unable to read from" << flash_path << std::endl;
        exit(1);
    }

    FILE *flash_out = fopen(flash_out_path.c_str(), "wb");
    if (!flash_out) {
        std::cerr << "ERROR: unable to write to " << flash_out_path << std::endl;
        exit(1);
    }

    int ch;
    while ((ch = fgetc(firmware_in)) != EOF)
        fputc(ch, firmware_out);

    std::cout << firmware_path << " was successfully copied to " <<
        firmware_out_path << std::endl;

    while ((ch = fgetc(flash_in)) != EOF)
        fputc(ch, flash_out);

    std::cout << flash_path << " was successfully copied to " <<
        flash_out_path << std::endl;

    fclose(flash_out);
    fclose(flash_in);
    fclose(firmware_out);
    fclose(firmware_in);

    std::cout << "Press ENTER to continue." << std::endl;
    while (std::cin.get() != '\n')
        ;
}

static int null_win_width, null_win_height;

static void null_win_init(unsigned width, unsigned height) {
    null_win_width = width;
    null_win_height = height;
}

static void null_win_check_events(void) {
}

static void null_win_run_once_on_suspend(void) {
}

static void null_win_update(void) {
}

static void null_win_make_context_current(void) {
}

static int null_win_get_width(void) {
    return null_win_width;
}

static int null_win_get_height(void) {
    return null_win_height;
}

static void null_win_update_title(void) {

}
