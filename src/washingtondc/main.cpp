/*******************************************************************************
 *
 * Copyright 2016-2020 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#include "i_hate_windows.h"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>

#ifndef _WIN32
#include <sys/stat.h>
#endif

#include "washdc/washdc.h"
#include "washdc/buildconfig.h"
#include "window.hpp"
#include "overlay.hpp"
#include "sound.hpp"
#include "washdc/hostfile.h"
#include "console_config.hpp"
#include "opengl/opengl_renderer.h"
#include "soft_gfx/soft_gfx.h"
#include "stdio_hostfile.hpp"
#include "washdc_getopt.h"
#include "rend_if.hpp"

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

#define CFG_FILE_NAME "wash.cfg"

#include "stdio_hostfile.hpp"
#include "paths.hpp"

struct renderer const *renderer;
static std::string rend_string;

static struct washdc_sound_intf snd_intf;
static struct washdc_hostfile_api hostfile_api;

static void wizard(path_string console_name, path_string dc_bios_path,
                   path_string dc_flash_path);

static void print_usage(char const *cmd) {
    fprintf(stderr, "USAGE: %s [options] [-d IP.BIN] [-u 1ST_READ.BIN]\n\n", cmd);

    fprintf(stderr, "WashingtonDC Dreamcast Emulator\n\n");

    fprintf(stderr, "OPTIONS:\n"
            "\t-c <console_name>\tname of console to boot\n"
            "\t-b <bios_path>\tpath to dreamcast boot ROM\n"
            "\t-f <flash_path>\tpath to dreamcast flash ROM image\n"
            "\t-g gdb\t\tenable remote GDB backend\n"
            "\t-g washdbg\tenable remote WashDbg backend\n"
            "\t-d\t\tenable direct boot (skip BIOS)\n"
            "\t-u\t\tskip IP.BIN and boot straight to 1ST_READ.BIN\n"
            "\t-s\t\tpath to dreamcast system call image (only needed for "
            "direct boot)\n"
            "\t-t\t\testablish serial server over TCP port 1998\n"
            "\t-h\t\tdisplay this message and exit\n"
            "\t-l\t\tdump logs to stdout\n"
            "\t-m\t\tmount the given image in the GD-ROM drive\n"
            "\t-n\t\tdon't inline memory reads/writes into the jit\n"
            "\t-p\t\tdisable the dynarec and enable the interpreter instead\n"
            "\t-j\t\tenable dynamic recompiler (as opposed to interpreter)\n"
            "\t-v\t\tenable verbose logging\n"
            "\t-x\t\tenable native x86_64 dynamic recompiler backend "
            "(default)\n"
            "\t-r opengl|soft\tselect renderer (default is opengl))\n");
}

struct washdc_gameconsole const *console;

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

// for washdc_getopt
char* washdc_optarg;
int washdc_optind = 1, washdc_opterr, washdc_optopt;

int main(int argc, char **argv) {
    int opt;
    char const *cmd = argv[0];
    bool enable_debugger = false;
    bool enable_washdbg = false;
    bool boot_direct = false, skip_ip_bin = false;
    char *path_1st_read_bin = NULL, *path_ip_bin = NULL;
    char *path_syscalls_bin = NULL;
    char *path_gdi = NULL;
    bool enable_serial = false;
    bool enable_jit = false, enable_native_jit = false,
        enable_interpreter = false, inline_mem = true;
    bool log_stdout = false, log_verbose = false;
    struct washdc_launch_settings settings = { };
    char const *console_name = NULL;
    bool launch_wizard = false;
    char const *dc_bios_path = NULL, *dc_flash_path = NULL;
    bool write_to_flash_mem = false;
    char const *gfx_backend = "opengl";

    create_cfg_dir();
    create_data_dir();
    create_screenshot_dir();

    while ((opt = washdc_getopt(argc, argv, "w:b:f:c:s:m:d:u:g:r:htjxpnlv")) != -1) {
        switch (opt) {
        case 'g':
            enable_debugger = true;
            if (strcmp(washdc_optarg, "washdbg") == 0) {
                enable_washdbg = true;
                enable_debugger = false;
            }
            break;
        case 'd':
            boot_direct = true;
            path_ip_bin = washdc_optarg;
            break;
        case 'u':
            skip_ip_bin = true;
            path_1st_read_bin = washdc_optarg;
            break;
        case 's':
            path_syscalls_bin = washdc_optarg;
            break;
        case 't':
            enable_serial = true;
            break;
        case 'm':
            path_gdi = washdc_optarg;
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
        case 'r':
            gfx_backend = washdc_optarg;
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

    settings.hostfile_api = &hostfile_api;

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

    if (skip_ip_bin) {
        if (!path_syscalls_bin) {
            fprintf(stderr, "Error: cannot direct-boot without a system call "
                    "table (-s flag).\n");
            exit(1);
        }

        if (!path_1st_read_bin) {
            fprintf(stderr, "Error: cannot direct-boot without a "
                    "1ST-READ.BIN\n");
            exit(1);
        }

        settings.boot_mode = WASHDC_BOOT_DIRECT;
        settings.path_ip_bin = path_ip_bin;
        settings.path_1st_read_bin = path_1st_read_bin;
        settings.path_syscalls_bin = path_syscalls_bin;
    } else if (boot_direct) {
        if (!path_syscalls_bin) {
            fprintf(stderr, "Error: cannot direct-boot without a system call "
                    "table (-s flag).\n");
            exit(1);
        }

        settings.boot_mode = WASHDC_BOOT_IP_BIN;
        settings.path_ip_bin = path_ip_bin;
        settings.path_syscalls_bin = path_syscalls_bin;
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
    settings.path_gdi = path_gdi;
    settings.win_intf = get_win_intf_glfw();

#ifdef ENABLE_TCP_SERIAL
    settings.sersrv = &sersrv_intf;
#endif

    snd_intf.init = sound::init;
    snd_intf.cleanup = sound::cleanup;
    snd_intf.submit_samples = sound::submit_samples;

    settings.sndsrv = &snd_intf;

    if (strcmp(gfx_backend, "opengl") == 0) {
        renderer = &opengl_renderer;
    } else if (strcmp(gfx_backend, "soft") == 0) {
        renderer = &soft_gfx_renderer;
    } else {
        fprintf(stderr, "ERROR: unknown rendering backend \"%s\"\n", gfx_backend);
        exit(1);
    }

    settings.gfx_rend_if = renderer->rend_if;
    rend_string = gfx_backend;

#ifdef USE_LIBEVENT
    io::init();
#endif

    static struct renderer_callbacks callbacks = { };

    if (renderer == &opengl_renderer && overlay_enabled())
        callbacks.overlay_draw = overlay::draw;
    callbacks.win_update = win_glfw_update;
    renderer->set_callbacks(&callbacks);

    console = washdc_init(&settings);

    if (overlay_enabled())
        overlay::init(enable_debugger || enable_washdbg);

    washdc_run();

    renderer->set_callbacks(NULL);

    if (overlay_enabled())
        overlay::cleanup();

#ifdef USE_LIBEVENT
    io::kick();
    io::cleanup();
#endif

    washdc_cleanup();

    exit(0);
}

void do_resume(void) {
    washdc_resume();
}

void do_run_one_frame(void) {
    washdc_run_one_frame();
}

void do_pause(void) {
    washdc_pause();
}

std::string const& rend_name(void) {
    return rend_string;
}

bool overlay_enabled(void) {
    return renderer == &opengl_renderer;
}
