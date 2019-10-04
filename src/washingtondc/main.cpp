/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2019 snickerbockers
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
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#include "washdc/washdc.h"
#include "washdc/buildconfig.h"
#include "window.hpp"
#include "overlay.hpp"
#include "sound.hpp"
#include "washdc/hostfile.h"
#include "hostfile.hpp"
#include "console_config.hpp"
#include "opengl/opengl_renderer.h"

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

void path_append(char *dst, char const *src, size_t dst_sz);
static char const *screenshot_dir(void);
static char const *data_dir(void);
static void create_data_dir(void);
char const *cfg_dir(void);
char const *cfg_file(void);
static void create_screenshot_dir(void);
static washdc_hostfile open_screenshot(char const *name,
                                       enum washdc_hostfile_mode mode);
static washdc_hostfile open_cfg_file(enum washdc_hostfile_mode mode);

static washdc_hostfile file_open(char const *path,
                                 enum washdc_hostfile_mode mode);
static void file_close(washdc_hostfile file);
static int file_seek(washdc_hostfile file,
                     long disp,
                     enum washdc_hostfile_seek_origin origin);
static long file_tell(washdc_hostfile file);
static size_t file_read(washdc_hostfile file, void *outp, size_t len);
static size_t file_write(washdc_hostfile file, void const *inp, size_t len);
static int file_flush(washdc_hostfile file);

static struct washdc_sound_intf snd_intf = {
    .init = sound::init,
    .cleanup = sound::cleanup,
    .submit_samples = sound::submit_samples
};

static struct washdc_hostfile_api const hostfile_api = {
    .open = file_open,
    .close = file_close,
    .seek = file_seek,
    .tell = file_tell,
    .read = file_read,
    .write = file_write,
    .flush = file_flush,
    .open_cfg_file = open_cfg_file,
    .open_screenshot = open_screenshot
};

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
            "(default)\n");
}

struct washdc_overlay_intf overlay_intf;

struct washdc_gameconsole const *console;

static void wizard(std::string const& console_name, char const *dc_bios_path, char const *dc_flash_path) {
    std::string firmware_path, flash_path;

    if (dc_bios_path) {
        firmware_path = dc_bios_path;
    } else {
        std::cout << "Please enter the path to your Dreamcast firmware image:" << std::endl;
        std::cin >> firmware_path;
    }

    if (dc_flash_path) {
        flash_path = dc_flash_path;
    } else {
        std::cout << "Please enter the path to your Dreamcast flash image:" << std::endl;
        std::cin >> flash_path;
    }

    std::string firmware_out_path = console_get_firmware_path(console_name.c_str());
    std::string flash_out_path = console_get_flashrom_path(console_name.c_str());

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

    create_cfg_dir();
    create_data_dir();
    create_screenshot_dir();

    while ((opt = getopt(argc, argv, "w:b:f:c:s:m:d:u:g:htjxpnlv")) != -1) {
        switch (opt) {
        case 'g':
            enable_debugger = true;
            if (strcmp(optarg, "washdbg") == 0) {
                enable_washdbg = true;
                enable_debugger = false;
            }
            break;
        case 'd':
            boot_direct = true;
            path_ip_bin = optarg;
            break;
        case 'u':
            skip_ip_bin = true;
            path_1st_read_bin = optarg;
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
            console_name = optarg;
            break;
        case 'b':
            dc_bios_path = optarg;
            break;
        case 'f':
            dc_flash_path = optarg;
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
        wizard(console_name, dc_bios_path, dc_flash_path);
    }

    argv += optind;
    argc -= optind;

    settings.log_to_stdout = log_stdout;
    settings.log_verbose = log_verbose;
    settings.write_to_flash = write_to_flash_mem;

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

    settings.sndsrv = &snd_intf;

    settings.gfx_rend_if = &opengl_rend_if;

    overlay_intf.overlay_draw = overlay::draw;
    overlay_intf.overlay_set_fps = overlay::set_fps;
    overlay_intf.overlay_set_virt_fps = overlay::set_virt_fps;

    settings.overlay_intf = &overlay_intf;

#ifdef USE_LIBEVENT
    io::init();
#endif

    console = washdc_init(&settings);

    overlay::init(enable_debugger || enable_washdbg);

    washdc_run();

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

void path_append(char *dst, char const *src, size_t dst_sz) {
    if (!src[0])
        return; // nothing to append

    // get the index of the null terminator
    unsigned zero_idx = 0;
    while (dst[zero_idx])
        zero_idx++;

    if (!zero_idx) {
        // special case - dst is empty so copy src over
        strncpy(dst, src, dst_sz);
        dst[dst_sz - 1] = '\0';
        return;
    }

    /*
     * If there's a trailing / on dst and a leading / on src then get rid of
     * the leading slash on src.
     *
     * If there is not a trailing / on dst and there is not a leading slash on
     * src then give dst a trailing /.
     */
    if (dst[zero_idx - 1] == '/' && src[0] == '/') {
        // remove leading / from src
        src = src + 1;
        if (!src[0])
            return; // nothing to append
    } else if (dst[zero_idx - 1] != '/' && src[0] != '/') {
        // add trailing / to dst
        if (zero_idx < dst_sz - 1) {
            dst[zero_idx++] = '/';
            dst[zero_idx] = '\0';
        } else {
            return; // out of space
        }
    }

    // there's no more space
    if (zero_idx >= dst_sz -1 )
        return;

    strncpy(dst + zero_idx, src, dst_sz - zero_idx);
    dst[dst_sz - 1] = '\0';
}

static char const *screenshot_dir(void) {
    static char path[HOSTFILE_PATH_LEN];
    char const *the_data_dir = data_dir();
    if (!the_data_dir)
        return NULL;
    strncpy(path, the_data_dir, HOSTFILE_PATH_LEN);
    path[HOSTFILE_PATH_LEN - 1] = '\0';
    path_append(path, "/screenshots", HOSTFILE_PATH_LEN);
    return path;
}

static char const *data_dir(void) {
    static char path[HOSTFILE_PATH_LEN];
    char const *data_root = getenv("XDG_DATA_HOME");
    if (data_root) {
        strncpy(path, data_root, HOSTFILE_PATH_LEN);
        path[HOSTFILE_PATH_LEN - 1] = '\0';
    } else {
        char const *home_dir = getenv("HOME");
        if (home_dir) {
            strncpy(path, home_dir, HOSTFILE_PATH_LEN);
            path[HOSTFILE_PATH_LEN - 1] = '\0';
        } else {
            return NULL;
        }
        path_append(path, "/.local/share", HOSTFILE_PATH_LEN);
    }
    path_append(path, "washdc", HOSTFILE_PATH_LEN);
    return path;
}

char const *cfg_dir(void) {
    static char path[HOSTFILE_PATH_LEN];
    char const *config_root = getenv("XDG_CONFIG_HOME");
    if (config_root) {
        strncpy(path, config_root, HOSTFILE_PATH_LEN);
        path[HOSTFILE_PATH_LEN - 1] = '\0';
    } else {
        char const *home_dir = getenv("HOME");
        if (home_dir) {
            strncpy(path, home_dir, HOSTFILE_PATH_LEN);
            path[HOSTFILE_PATH_LEN - 1] = '\0';
        } else {
            return NULL;
        }
        path_append(path, "/.config", HOSTFILE_PATH_LEN);
    }
    path_append(path, "washdc", HOSTFILE_PATH_LEN);
    return path;
}

char const *cfg_file(void) {
    static char path[HOSTFILE_PATH_LEN];
    char const *the_cfg_dir = cfg_dir();
    if (!the_cfg_dir)
        return NULL;
    strncpy(path, the_cfg_dir, HOSTFILE_PATH_LEN);
    path[HOSTFILE_PATH_LEN - 1] = '\0';
    path_append(path, "wash.cfg", HOSTFILE_PATH_LEN);
    return path;
}

void create_directory(char const *name) {
#ifdef _WIN32
    if (!CreateDirectoryA(name, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        fprintf(stderr, "%s - failure to create %s\n", __func__, name);
#else
    if (mkdir(name, S_IRUSR | S_IWUSR | S_IXUSR) != 0 && errno != EEXIST)
        fprintf(stderr, "%s - failure to create %s\n", __func__, name);
#endif
}

static void create_screenshot_dir(void) {
    create_data_dir();
    create_directory(screenshot_dir());
}

static washdc_hostfile open_screenshot(char const *name,
                                       enum washdc_hostfile_mode mode) {
    std::string path = std::string(screenshot_dir()) + "/" + name;
    return file_open(path.c_str(), mode);
}

void create_cfg_dir(void) {
    create_directory(cfg_dir());
}

static washdc_hostfile open_cfg_file(enum washdc_hostfile_mode mode) {
    return file_open(cfg_file(), mode);
}

static void create_data_dir(void) {
    create_directory(data_dir());
}

static washdc_hostfile file_open(char const *path,
                                 enum washdc_hostfile_mode mode) {
    char modestr[4] = { 0 };
    int top = 0;
    if (mode & WASHDC_HOSTFILE_WRITE)
        modestr[top++] = 'w';
    else if (mode & WASHDC_HOSTFILE_READ)
        modestr[top++] = 'r';
    else
        return WASHDC_HOSTFILE_INVALID;

    if (mode & WASHDC_HOSTFILE_BINARY)
        modestr[top++] = 'b';
    if (mode & WASHDC_HOSTFILE_DONT_OVERWRITE)
        modestr[top++] = 'x';

    return fopen(path, modestr);
}

static void file_close(washdc_hostfile file) {
    fclose((FILE*)file);
}

static int file_seek(washdc_hostfile file, long disp,
                     enum washdc_hostfile_seek_origin origin) {
    int whence;
    switch (origin) {
    case WASHDC_HOSTFILE_SEEK_BEG:
        whence = SEEK_SET;
        break;
    case WASHDC_HOSTFILE_SEEK_CUR:
        whence = SEEK_CUR;
        break;
    case WASHDC_HOSTFILE_SEEK_END:
        whence = SEEK_END;
        break;
    default:
        return -1;
    }
    return fseek((FILE*)file, disp, whence);
}

static long file_tell(washdc_hostfile file) {
    return ftell((FILE*)file);
}

static size_t file_read(washdc_hostfile file, void *outp, size_t len) {
    return fread(outp, 1, len, (FILE*)file);
}

static size_t file_write(washdc_hostfile file, void const *inp, size_t len) {
    return fwrite(inp, 1, len, (FILE*)file);
}

static int file_flush(washdc_hostfile file) {
    return fflush((FILE*)file);
}
