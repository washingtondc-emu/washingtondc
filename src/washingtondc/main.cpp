/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2020 snickerbockers
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
#include "gfxgl3/gfxgl3_renderer.h"
#include "gfxgl4/gfxgl4_renderer.h"
#include "soft_gfx/soft_gfx.h"
#include "stdio_hostfile.hpp"
#include "washdc_getopt.h"
#include "rend_if.hpp"
#include "config_file.h"

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
            "\t-r opengl|soft\tselect renderer (default is opengl))\n"
            "\t-w\t\tcreate a new console (you must also supply -b, -f, and -c)\n");
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

static struct washdc_controller_dev parse_controller(char const *dev) {
    char const *first_space = strpbrk(dev, " \t\n");
    size_t n_chars;
    if (first_space)
        n_chars = first_space - dev;
    else
        n_chars = strlen(dev);

    struct washdc_controller_dev ret;

    if (strncmp(dev, "vmu", n_chars) == 0) {
        ret.tp = WASHDC_CONTROLLER_TP_VMU;

        memset(ret.image_path, 0, sizeof(ret.image_path));
        if (first_space && *first_space) {
            while (isspace(*first_space))
                first_space++;

            if (strncmp(first_space, "file=", 5) == 0) {
                path_string path(path_append(vmu_dir(), first_space + 5));

                strncpy(ret.image_path, path.c_str(),
                        sizeof(ret.image_path));
                ret.image_path[sizeof(ret.image_path) - 1] = '\0';
            }
        }
    } else if (strncmp(dev, "purupuru", n_chars) == 0)
        ret.tp = WASHDC_CONTROLLER_TP_PURUPURU;
    else if (strncmp(dev, "dreamcast_controller", n_chars) == 0)
        ret.tp = WASHDC_CONTROLLER_TP_CONTROLLER;
    else if (strncmp(dev, "dreamcast_keyboard_us", n_chars) == 0)
        ret.tp = WASHDC_CONTROLLER_TP_KEYBOARD_US;
    else
        ret.tp = WASHDC_CONTROLLER_TP_INVALID;

    return ret;
}

static struct washdc_controller_dev get_cfg_controller(char const *node) {
    char const *val = cfg_get_node(node);
    if (val) {
        return parse_controller(val);
    } else {
        struct washdc_controller_dev ret = { WASHDC_CONTROLLER_TP_INVALID };
        return ret;
    }
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
    create_vmu_dir();

    while ((opt = washdc_getopt(argc, argv, "wb:f:c:s:m:d:u:g:r:htjxpnlv")) != -1) {
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

    // INIT CONFIGURATION SYSTEM
    path_string the_cfg_file(cfg_file());
    FILE *cfg_file = fopen(the_cfg_file.c_str(), "r");
    if (!cfg_file) {
        cfg_file = fopen(the_cfg_file.c_str(), "w");
        cfg_create_default_config(cfg_file);
        fclose(cfg_file);

        cfg_file = fopen(the_cfg_file.c_str(), "r");
    }
    if (cfg_file) {
        cfg_init(cfg_file);
    } else {
        fprintf(stderr, "Unable to open wash.cfg; does it even exist?\n");
        exit(1);
    }
    fclose(cfg_file);

    // configure controllers
    settings.controllers[0][0] = get_cfg_controller("wash.dc.port.0.0");
    settings.controllers[0][1] = get_cfg_controller("wash.dc.port.0.1");
    settings.controllers[0][2] = get_cfg_controller("wash.dc.port.0.2");
    settings.controllers[1][0] = get_cfg_controller("wash.dc.port.1.0");
    settings.controllers[1][1] = get_cfg_controller("wash.dc.port.1.1");
    settings.controllers[1][2] = get_cfg_controller("wash.dc.port.1.2");
    settings.controllers[2][0] = get_cfg_controller("wash.dc.port.2.0");
    settings.controllers[2][1] = get_cfg_controller("wash.dc.port.2.1");
    settings.controllers[2][2] = get_cfg_controller("wash.dc.port.2.2");
    settings.controllers[3][0] = get_cfg_controller("wash.dc.port.3.0");
    settings.controllers[3][1] = get_cfg_controller("wash.dc.port.3.1");
    settings.controllers[3][2] = get_cfg_controller("wash.dc.port.3.2");

    cfg_get_bool("wash.dbg.dump_mem_on_error", &settings.dump_mem_on_error);

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

    if (dc_bios_path) {
        settings.path_dc_bios = dc_bios_path;
    } else if (have_console_name) {
        settings.path_dc_bios = console_get_firmware_path(console_name);
    } else {
        fprintf(stderr,
                "========================================================\n");
        fprintf(stderr,
                "==\n");
        fprintf(stderr,
                "== ERROR - SUPPLY DREAMCAST FIRMWARE IMAGE FILE (-b option)\n");
        fprintf(stderr,
                "==\n");
        fprintf(stderr,
                "========================================================\n");
        print_usage(cmd);
        exit(1);
    }

    if (dc_flash_path) {
        settings.path_dc_flash = dc_flash_path;
    } else if (have_console_name) {
        settings.path_dc_flash = console_get_flashrom_path(console_name);
    } else {
        fprintf(stderr,
                "========================================================\n");
        fprintf(stderr,
                "==\n");
        fprintf(stderr,
                "== ERROR - SUPPLY DREAMCAST FLASH IMAGE FILE (-f option)\n");
        fprintf(stderr,
                "==\n");
        fprintf(stderr,
                "========================================================\n");
        print_usage(cmd);
        exit(1);
    }

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

    /***************************************************************************
     *
     * CREATE WINDOW
     *
     **************************************************************************/
    int win_width, win_height;
    if (cfg_get_int("win.external-res.x", &win_width) != 0 || win_width <= 0)
        win_width = 640;
    if (cfg_get_int("win.external-res.y", &win_height) != 0 || win_height <= 0)
        win_height = 480;

    if (strcmp(gfx_backend, "soft") == 0) {
        // create OpenGL 3.3 context for soft_gfx
        if (win_glfw_init(win_width, win_height, 3, 3) != 0) {
            fprintf(stderr, "ERROR: unable to create OpenGL 3.3 "
                    "window/context\n");
            exit(1);
        }
        renderer = &soft_gfx_renderer;
    } else if (strcmp(gfx_backend, "gl4") == 0) {
        // create OpenGL 4.5 context for gfxgl4
        if (win_glfw_init(win_width, win_height, 4, 5) != 0) {
            fprintf(stderr, "ERROR: unable to create OpenGL 4.5 "
                    "window/context\n");
            exit(1);
        }
        renderer = &gfxgl4_renderer;
    } else if (strcmp(gfx_backend, "gl3") == 0) {
        // create OpenGL 3.3 context for gfxgl3
        if (win_glfw_init(win_width, win_height, 3, 3) != 0) {
            fprintf(stderr, "ERROR: unable to create OpenGL 3.3 "
                    "window/context\n");
            exit(1);
        }
        renderer = &gfxgl3_renderer;
    } else if (strcmp(gfx_backend, "opengl") == 0) {
        /*
         * try to create a gl4.5 backend for gfxgl4, and if that
         * fails we'll settle for a gl3.3 backend for gfxgl3
         */
        if (win_glfw_init(win_width, win_height, 4, 5) == 0) {
            renderer = &gfxgl4_renderer;
        } else if (win_glfw_init(win_width, win_height, 3, 3) == 0) {
            renderer = &gfxgl3_renderer;
        } else {
            fprintf(stderr, "ERROR: unable to create OpenGL 3.3 "
                    "window/context\n");
            exit(1);
        }
    } else {
        fprintf(stderr, "ERROR: unknown rendering backend \"%s\"\n", gfx_backend);
        exit(1);
    }

    settings.gfx_rend_if = renderer->rend_if;

    if (renderer == &gfxgl4_renderer)
        rend_string = "gfxgl4";
    else if (renderer == &gfxgl3_renderer)
        rend_string = "gfxgl3";
    else if (renderer == &soft_gfx_renderer)
        rend_string = "soft_gfx";
    else
        rend_string = gfx_backend;

    printf("\"%s\" renderer selected\n", rend_string.c_str());

#ifdef USE_LIBEVENT
    io::init();
#endif

    static struct renderer_callbacks callbacks = { };

    if (overlay_enabled())
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

    win_glfw_cleanup();

    cfg_cleanup();

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
    return renderer == &gfxgl3_renderer ||
        renderer == &gfxgl4_renderer;
}
