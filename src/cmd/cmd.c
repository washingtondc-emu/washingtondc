/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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

#include <err.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <png.h>

#include "hw/aica/aica_wave_mem.h"
#include "gfx/gfx_config.h"
#include "cons.h"
#include "dreamcast.h"
#include "config.h"
#include "hw/pvr2/pvr2_core_reg.h"
#include "hw/pvr2/pvr2_tex_cache.h"
#include "gfx/gfx.h"
#include "pix_conv.h"

#include "cmd.h"

#define CMD_BUF_SIZE_SHIFT 10
#define CMD_BUF_SIZE (1 << CMD_BUF_SIZE_SHIFT)

static char cmd_buf[CMD_BUF_SIZE];
unsigned cmd_buf_len;

static int cmd_disable_auto_screenshot(int argc, char **argv);
static int cmd_echo(int argc, char **argv);
static int cmd_enable_auto_screenshot(int argc, char **argv);
static int cmd_help(int argc, char **argv);
static int cmd_render_set_mode(int argc, char **argv);
static int cmd_exit(int argc, char **argv);
static int cmd_resume_execution(int argc, char **argv);
static int cmd_run_frame(int argc, char **argv);
static int cmd_screenshot(int argc, char **argv);
static int cmd_suspend_execution(int argc, char **argv);
static int cmd_begin_execution(int argc, char **argv);
static int cmd_aica_verbose_log(int argc, char **argv);
static int cmd_tex_info(int argc, char **argv);
static int cmd_tex_enum(int argc, char **argv);
static int cmd_tex_dump_all(int argc, char **argv);
static int cmd_tex_dump(int argc, char **argv);

static int save_tex(char const *path, struct pvr2_tex_meta const *meta,
                    void const *dat);

struct cmd {
    char const *cmd_name;
    char const *summary;
    char const *help_str;
    int(*cmd_handler)(int, char**);
} cmd_list[] = {
    /*
     * XXX when adding new commands, remember to keep this list in alphabetical
     * order.  The help command is not cognizant enough to sort these, so it
     * will print them in whatever order they are listed.
     */
    {
        .cmd_name = "aica-verbose-log",
        .summary = "log reads-to/writes-from AICA waveform memory to stdout",
        .help_str =
        "aica-verbose-log enable|disable\n"
        "\n"
        "This command can be made to log attempts made by the guest software\n"
        "to access AICA waveform memory.  The logs will be printed to\n"
        "stdout.\n",
        .cmd_handler = cmd_aica_verbose_log
    },
    {
        .cmd_name = "begin-execution",
        .summary = "begin executing with the input settings",
        .help_str =
        "begin-execution\n"
        "\n"
        "start emulator execution\n",
        .cmd_handler = cmd_begin_execution
    },
    {
        .cmd_name = "disable-auto-screenshot",
        .summary = "don't automatically save screenshots as PNG files",
        .help_str =
        "disable-auto-screenshot\n"
        "\n"
        "stop saving framebuffers as PNG files (after you previously started\n"
        "doing this with the enable-auto-screenshot command)\n",
        .cmd_handler = cmd_disable_auto_screenshot
    },
    {
        .cmd_name = "enable-auto-screenshot",
        .summary = "automatically save screenshots as PNG files",
        .help_str =
        "enable-auto-screenshot dir\n"
        "\n"
        "automatically save every framebuffer as a PNG file in the given dir\n"
        "To disable this, use disable-auto-screenshot\n",
        .cmd_handler = cmd_enable_auto_screenshot
    },
    {
        .cmd_name = "echo",
        .summary = "echo text to the console",
        .help_str =
        "echo [text]\n"
        "\n"
        "echo prints all of its arguments to the console\n",
        .cmd_handler = cmd_echo
    },
    {
        .cmd_name = "exit",
        .summary = "exit WashingtonDC immediately",
        .help_str = "exit\n"
        "\n"
        "It does exactly what you think it does.  There's no confirmation\n"
        "prompt, so be careful not to type this in absentmindedly\n",
        .cmd_handler = cmd_exit
    },
    {
        .cmd_name = "help",
        .summary = "online command documentation",
        .help_str =
        "help [cmd]\n"
        "\n"
        "When invoked without any arguments, help will list all commands\n"
        "When invoked with the name of a command, help will display the \n"
        "documentation for that command.\n",
        .cmd_handler = cmd_help
    },
    {
        .cmd_name = "render-set-mode",
        .summary = "set the 3D graphics rendering mode",
        .help_str =
        "render-set-mode default|wireframe\n"
        "\n"
        "change the way that 3D graphics are rendered\n"
        "if you ever feel lost, \'render-set-mode default\' will restore the\n"
        "default rendering settings.\n",
        .cmd_handler = cmd_render_set_mode
    },
    {
        .cmd_name = "resume-execution",
        .summary = "resume execution while the emulator is suspended.",
        .help_str =
        "resume-execution\n"
        "\n"
        "If WashingtonDC is suspended, then resume execution.\n"
#ifdef ENABLE_DEBUGGER
        "This command only works when the remote GDB frontend is not in use.\n"
        "If you've enabled the GDB frontend, you need to use that to control\n"
        "execution instead.\n"
#endif
        ,
        .cmd_handler = cmd_resume_execution
    },
    {
        .cmd_name = "run-frame",
        .summary = "run to the next VBLANK.",
        .help_str =
        "run-frame\n"
        "\n"
        "If WashingtonDC is suspended, then resume execution and suspend\n"
        "again after the next VBLANK.\n"
#ifdef ENABLE_DEBUGGER
        "This command only works when the remote GDB frontend is not in use.\n"
        "If you've enabled the GDB frontend, you need to use that to control\n"
        "execution instead.\n"
#endif
        ,
        .cmd_handler = cmd_run_frame
    },
    {
        .cmd_name = "screenshot",
        .summary = "save a screenshot to a PNG file.",
        .help_str =
        "screenshot path\n"
        "\n"
        "This command saves a screenshot to the given path as a PNG file.\n",
        .cmd_handler = cmd_screenshot
    },
    {
        .cmd_name = "suspend-execution",
        .summary = "suspend execution while the emulator is running.",
        .help_str =
        "suspend-execution\n"
        "\n"
        "If WashingtonDC is running, then suspend execution.\n"
#ifdef ENABLE_DEBUGGER
        "This command only works when the remote GDB frontend is not in use.\n"
        "If you've enabled the GDB frontend, you need to use that to control\n"
        "execution instead.\n"
#endif
        ,
        .cmd_handler = cmd_suspend_execution
    },
    {
        .cmd_name = "tex-dump",
        .summary = "dump a texture in the cache to a .png file",
        .help_str = "tex-dump tex_no file\n"
        "\n"
        "save the texture indicated by tex_no into file.\n"
        "the resulting file will be a .png image.\n",
        .cmd_handler = cmd_tex_dump
    },
    {
        .cmd_name = "tex-dump-all",
        .summary = "dump the entire texture cache into .png files in a directory",
        .help_str = "tex-dump directory\n"
        "\n"
        "Save every texture in the cache into the given directory as PNG images\n",
        .cmd_handler = cmd_tex_dump_all
    },
    {
        .cmd_name = "tex-enum",
        .summary = "list all active texture cache entries",
        .help_str =
        "tex-enum\n"
        "\n"
        "This command prints the index of every active entry in the texture \n"
        "cache\n",
        .cmd_handler = cmd_tex_enum
    },
    {
        .cmd_name = "tex-info",
        .summary = "view metadata for a texture in the texture cache.",
        .help_str =
        "tex-info tex_no|all\n"
        "\n"
        "Look up the given texture in the texture cache and print its \n"
        "metadata.\n",
        .cmd_handler = cmd_tex_info
    },
    { NULL }
};

static struct cmd const* find_cmd_by_name(char const *name);

static void cmd_run_cmd(void);

void cmd_put_char(char ch) {
    // disregard NULL terminators and line-ending bullshit
    if (ch == '\0' || ch == '\r')
        return;

    if (cmd_buf_len < (CMD_BUF_SIZE - 1)) {
        cmd_buf[cmd_buf_len++] = ch;

        if (ch == '\n') {
            cmd_run_cmd();
            cmd_buf_len = 0;
            cmd_print_prompt();
        }
    } else {
        if (ch == '\n') {
            cons_puts("ignoring command due to excessive length\n");
            cmd_buf_len = 0;
            cmd_print_prompt();
        }
    }
}

enum {
    STATE_WHITESPACE,
    STATE_ARG
};

static void cmd_run_cmd(void) {
    unsigned idx = 0;
    int argc = 0;
    char **argv = NULL;
    char const *text_in = cmd_buf;
    int state = STATE_WHITESPACE;
    char const *arg_start;

    // parse out all the whitespace
    while (text_in < (cmd_buf_len + cmd_buf)) {
        if (state == STATE_WHITESPACE) {
            if (!isspace(*text_in)) {
                state = STATE_ARG;
                arg_start = text_in;
            }
        } else {
            // state == STATE_ARG
            if (isspace(*text_in)) {
                state = STATE_WHITESPACE;
                unsigned arg_len = text_in - arg_start;
                argv = (char**)realloc(argv, sizeof(char*) * ++argc);
                if (!argv) {
                    err(1,
                        "failed allocation while parsing cmd arguments in %s",
                        __func__);
                }
                argv[argc - 1] = (char*)malloc(sizeof(char) * (arg_len + 1));
                if (!argv[argc - 1]) {
                    err(1,
                        "failed allocation while parsing cmd arguments in %s",
                        __func__);
                }
                memcpy(argv[argc - 1], arg_start, arg_len);
                argv[argc - 1][arg_len] = '\0';
            }
        }
        text_in++;
    }

    if (!argc)
        return; // nothing to see here

    // now run the command
    char const *cmd_name = argv[0];

    struct cmd const *cmd = find_cmd_by_name(cmd_name);
    if (cmd)
        cmd->cmd_handler(argc, argv); // TODO: check the return value
    else
        cons_puts("ERROR: unable to run command\n");

    // free argv
    for (idx = 0; idx < argc; idx++)
        free(argv[idx]);
    free(argv);
}

static struct cmd const* find_cmd_by_name(char const *name) {
    struct cmd const *cmd = cmd_list;
    while (cmd->cmd_name) {
        if (strcmp(cmd->cmd_name, name) == 0) {
            return cmd;

            break;
        }
        cmd++;
    }
    return NULL;
}

static int cmd_disable_auto_screenshot(int argc, char **argv) {
    config_set_enable_auto_screenshot(false);
    return 0;
}

static int cmd_echo(int argc, char **argv) {
    int idx;
    for (idx = 1; idx < argc; idx++) {
        if (idx > 1)
            cons_puts(" ");
        cons_puts(argv[idx]);
    }
    cons_puts("\n");
    return 0;
}

static int cmd_enable_auto_screenshot(int argc, char **argv) {
    if (argc != 2) {
        cons_puts("usage: enable-auto-screenshot dir\n");
        return 1;
    }

    char const *dir = argv[1];
    config_set_auto_screenshot_dir(dir);
    config_set_enable_auto_screenshot(true);

    cons_printf("screnshots will be saved to the %s directory\n",
                dir);

    return 0;
}

static int cmd_help(int argc, char **argv) {
    struct cmd const *cmd;
    if (argc >= 2) {
        cmd = find_cmd_by_name(argv[1]);
        if (cmd) {
            cons_puts(cmd->help_str);
        } else {
            cons_puts("ERROR: unable to find cmd\n");
            return 1;
        }
    } else {
        cmd = cmd_list;
        while (cmd->cmd_name) {
            cons_puts(cmd->cmd_name);
            cons_puts(" - ");
            cons_puts(cmd->summary);
            cons_puts("\n");
            cmd++;
        }
    }

    return 0;
}

static int cmd_render_set_mode(int argc, char **argv) {
    if (argc != 2) {
        cons_puts("usage: render-set-mode default|wireframe\n");
        return 1;
    }

    char const *mode_str = argv[1];
    if (strcmp(mode_str, "default") == 0) {
        gfx_config_default();
    } else if (strcmp(mode_str, "wireframe") == 0) {
        gfx_config_wireframe();
    } else {
        cons_puts("ERROR: unrecognized graphics rendering mode\n");
        return 1;
    }

    return 0;
}

static int cmd_exit(int argc, char **argv) {
    dreamcast_kill();
    return 0;
}

static int cmd_resume_execution(int argc, char **argv) {
#ifdef ENABLE_DEBUGGER
    if (config_get_dbg_enable()) {
        cons_puts("ERROR: unable to control execution from the cmd prompt when "
                  "gdb is enabled.\n");
        return 1;
    }
#endif

    enum dc_state dc_state = dc_get_state();

    if (dc_state == DC_STATE_SUSPEND) {
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_SUSPEND);
        return 0;
    }

    cons_puts("ERROR: unable to resume execution because WashingtonDC is not "
              "suspended\n");
    return 1;
}

static int cmd_run_frame(int argc, char **argv) {
#ifdef ENABLE_DEBUGGER
    if (config_get_dbg_enable()) {
        cons_puts("ERROR: unable to control execution from the cmd prompt when "
                  "gdb is enabled.\n");
        return 1;
    }
#endif

    enum dc_state dc_state = dc_get_state();

    if (dc_state == DC_STATE_SUSPEND) {
        dc_request_frame_stop();
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_SUSPEND);
        return 0;
    } else if (dc_state == DC_STATE_NOT_RUNNING) {
        dc_request_frame_stop();
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_NOT_RUNNING);
        return 0;
    }

    cons_puts("ERROR: unable to resume execution because WashingtonDC is not "
              "suspended\n");
    return 1;
}

static int cmd_screenshot(int argc, char **argv) {
    if (argc != 2) {
        cons_puts("usage: screenshot path\n");
        return 1;
    }

    if (gfx_save_screenshot(argv[1]) == 0) {
        cons_printf("screenshot saved to \"%s\"\n", argv[1]);
        return 0;
    } else {
        cons_printf("error: failed to save screenshot to \"%s\"\n",
                    argv[1]);
        return 1;
    }
}

static int cmd_suspend_execution(int argc, char **argv) {
#ifdef ENABLE_DEBUGGER
    if (config_get_dbg_enable()) {
        cons_puts("ERROR: unable to control execution from the cmd prompt when "
                  "gdb is enabled.\n");
        return 1;
    }
#endif

    enum dc_state dc_state = dc_get_state();

    if (dc_state == DC_STATE_RUNNING) {
        dc_state_transition(DC_STATE_SUSPEND, DC_STATE_RUNNING);
        return 0;
    }

    cons_puts("ERROR: unable to suspend execution because WashingtonDC is not "
              "running\n");
    return 1;
}

static int cmd_begin_execution(int argc, char **argv) {
#ifdef ENABLE_DEBUGGER
    if (config_get_dbg_enable()) {
        cons_puts("ERROR: unable to control execution from the cmd prompt when "
                  "gdb is enabled.\n");
        return 1;
    }
#endif
    enum dc_state dc_state = dc_get_state();

    if (dc_state == DC_STATE_NOT_RUNNING) {
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_NOT_RUNNING);
        return 0;
    }

    cons_puts("ERROR: unable to begin execution because WashingtonDC is "
              "already running\n");
    return 1;
}

// this gets printed to the dev console every time the emulator starts
static char const *login_banner =
    "WashingtonDC Copyright (C) 2016, 2017 snickerbockers\n"
    "This program comes with ABSOLUTELY NO WARRANTY;\n"
    "This is free software, and you are welcome to redistribute it\n"
    "under the terms of the GNU GPL version 3.\n";

void cmd_print_banner(void) {
    cons_puts(login_banner);
    cmd_print_prompt();
}

void cmd_print_prompt(void) {
    cons_puts("> ");
}

static int cmd_aica_verbose_log(int argc, char **argv) {
    bool do_enable;
    if (argc != 2) {
        cons_puts("Usage: aica-verbose-log enable|disable\n");
        return 1;
    }

    if (strcmp(argv[1], "enable") == 0) {
        do_enable = true;
    } else if (strcmp(argv[1], "disable") == 0) {
        do_enable = false;
    } else {
        cons_puts("Usage: aica-verbose-log enable|disable\n");
        return 1;
    }

    aica_log_verbose(do_enable);

    cons_puts("verbose AICA waveform memory access logging is now ");
    if (do_enable)
        cons_puts("enabled.\n");
    else
        cons_puts("disabled.\n");

    return 0;
}

static char const *tex_fmt_names[TEX_CTRL_PIX_FMT_COUNT] = {
    [TEX_CTRL_PIX_FMT_ARGB_1555] = "ARGB_1555",
    [TEX_CTRL_PIX_FMT_RGB_565] = "RGB_565",
    [TEX_CTRL_PIX_FMT_ARGB_4444] = "ARGB_4444",
    [TEX_CTRL_PIX_FMT_YUV_422] = "YUV_422",
    [TEX_CTRL_PIX_FMT_BUMP_MAP] = "BUMP_MAP",
    [TEX_CTRL_PIX_FMT_4_BPP_PAL] = "PAL_4BPP",
    [TEX_CTRL_PIX_FMT_8_BPP_PAL] = "PAL_8BPP",
    [TEX_CTRL_PIX_FMT_INVALID] = "<invalid format>",
};

static int cmd_tex_info(int argc, char **argv) {
    unsigned first_tex_no, last_tex_no, tex_no;
    bool print_missing = true;
    bool did_print = false;

    if (argc != 2) {
        cons_puts("Usage: tex-info tex_no|all\n");
        return 1;
    }

    /*
     * TODO: one problem with the "all" option is that it fills up the cons
     * text_ring completely and results in dropped characters.
     */
    if (strcmp(argv[1], "all") == 0) {
        first_tex_no = 0;
        last_tex_no = PVR2_TEX_CACHE_SIZE - 1;
        print_missing = false;
    } else {
        first_tex_no = last_tex_no = atoi(argv[1]);
    }

    for (tex_no = first_tex_no; tex_no <= last_tex_no; tex_no++) {
        struct pvr2_tex_meta meta;
        if (pvr2_tex_get_meta(&meta, tex_no) == 0) {
            cons_printf("texture %u:\n", tex_no);
            cons_printf("\tdimensions: (%u, %u)\n",
                        1 << meta.w_shift, 1 << meta.h_shift);
            cons_printf("\tpix_fmt: %s\n",
                        meta.pix_fmt < TEX_CTRL_PIX_FMT_COUNT ?
                        tex_fmt_names[meta.pix_fmt] : "<invalid format>");
            cons_printf("\ttex_fmt: %s\n",
                        meta.tex_fmt < TEX_CTRL_PIX_FMT_COUNT ?
                        tex_fmt_names[meta.tex_fmt] : "<invalid format>");
            cons_printf("\t%s\n", meta.twiddled ? "twiddled" : "not twiddled");
            cons_printf("\tVQ compression: %s\n",
                        meta.vq_compression ? "yes" : "no");
            cons_printf("\tmipmapped: %s\n",
                        meta.mipmap ? "enabled" : "disabled");
            cons_printf("\tstride type: %s\n",
                        meta.stride_sel ? "from texinfo" : "from texture");
            cons_printf("\tfirst address: 0x%08x\n", meta.addr_first);
            cons_printf("\tlast address: 0x%08x\n", meta.addr_last);
            did_print = true;
        } else {
            if (print_missing) {
                cons_printf("Texture %u is not in the texture cache\n", tex_no);
                did_print = true;
            }
        }
    }

    if (!did_print)
        cons_puts("No textures were found\n");

    return 0;
}

static int cmd_tex_enum(int argc, char **argv) {
    unsigned tex_no;
    bool did_print = false;

    for (tex_no = 0; tex_no < PVR2_TEX_CACHE_SIZE; tex_no++) {
        struct pvr2_tex_meta meta;
        if (pvr2_tex_get_meta(&meta, tex_no) == 0) {
            cons_printf("%s%u", did_print ? ", " : "", tex_no);
            did_print = true;
        }
    }

    if (did_print)
        cons_puts("\n");
    else
        cons_puts("the texture cache is currently empty.\n");

    return 0;
}

static int cmd_tex_dump(int argc, char **argv) {
    unsigned tex_no;

    if (argc != 3) {
        cons_puts("Usage: tex-dump tex_no file\n");
        return 1;
    }

    tex_no = atoi(argv[1]);

    char const *file = argv[2];

    struct pvr2_tex_meta meta;

    if (pvr2_tex_get_meta(&meta, tex_no) == 0) {
        void *tex_dat;
        size_t n_bytes;
        pvr2_tex_cache_read(&tex_dat, &n_bytes, &meta);
        if (tex_dat) {
            if (save_tex(file, &meta, tex_dat) < 0)
                cons_printf("Failed to save texture\n");

            free(tex_dat);
        } else {
            cons_printf("Failed to retrieve texture %u from the texture "
                        "cache\n", tex_no);
        }
    } else {
        cons_printf("Texture %u is not in the texture cache\n", tex_no);
    }

    return 0;
}

#define TEX_DUMP_ALL_PATH_LEN 512
static int cmd_tex_dump_all(int argc, char **argv) {
    unsigned tex_no;
    char const *dir_path;
    char const *path_last_char;
    char total_path[TEX_DUMP_ALL_PATH_LEN];
    bool all_success = true;

    if (argc != 2) {
        cons_puts("Usage: tex-dump-all directory\n");
        return 1;
    }

    dir_path = argv[1];
    path_last_char = dir_path + strlen(dir_path);

    for (tex_no = 0; tex_no < PVR2_TEX_CACHE_SIZE; tex_no++) {
        struct pvr2_tex_meta meta;
        if (pvr2_tex_get_meta(&meta, tex_no) == 0) {
            void *tex_dat;
            size_t n_bytes;
            pvr2_tex_cache_read(&tex_dat, &n_bytes, &meta);
            if (*path_last_char == '/') {
                snprintf(total_path, TEX_DUMP_ALL_PATH_LEN, "%stex_%03u.png",
                         dir_path, tex_no);
            } else {
                snprintf(total_path, TEX_DUMP_ALL_PATH_LEN, "%s/tex_%03u.png",
                         dir_path, tex_no);
            }
            total_path[TEX_DUMP_ALL_PATH_LEN - 1] = '\0';

            if (save_tex(total_path, &meta, tex_dat) < 0)
                all_success = false;
            free(tex_dat);
        }
    }

    if (!all_success)
        cons_printf("Some textures could not be saved\n");

    return 0;
}

static int save_tex(char const *path, struct pvr2_tex_meta const *meta,
                    void const *dat) {
    /*
     * TODO: come up with a way to do the writing asynchronously from the io
     * thread.
     */
    FILE *stream = fopen(path, "wb");

    if (!stream)
        return -1;

    int err_val = 0;
    void *dat_conv = NULL;
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                  NULL, NULL, NULL);
    if (!png_ptr) {
        err_val = -1;
        goto finish;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);

    if (!info_ptr) {
        err_val = -1;
        goto cleanup_png;
    }

    jmp_buf error_point;
    if (setjmp(error_point)) {
        err_val = -1;
        goto cleanup_png;
    }

    png_init_io(png_ptr, stream);

    if (meta->pix_fmt != TEX_CTRL_PIX_FMT_ARGB_1555 &&
        meta->pix_fmt != TEX_CTRL_PIX_FMT_RGB_565 &&
        meta->pix_fmt != TEX_CTRL_PIX_FMT_ARGB_4444 &&
        meta->pix_fmt != TEX_CTRL_PIX_FMT_YUV_422) {
        err_val = -1;
        goto cleanup_png;
    }

    /*
     * prevent integer overflows when we call malloc below.
     *
     * Also, the max texture-side-log2 on pvr2 is 10 anyways.
     */
    if (meta->w_shift > 10 || meta->h_shift > 10)
        RAISE_ERROR(ERROR_INTEGRITY);
    unsigned tex_w = 1 << meta->w_shift, tex_h = 1 << meta->h_shift;

    int color_tp_png;
    unsigned n_colors;
    unsigned pvr2_pix_size;
    switch (meta->pix_fmt) {
    case TEX_CTRL_PIX_FMT_ARGB_1555:
    case TEX_CTRL_PIX_FMT_ARGB_4444:
        color_tp_png = PNG_COLOR_TYPE_RGB_ALPHA;
        n_colors = 4;
        pvr2_pix_size = 2;
        break;
    case TEX_CTRL_PIX_FMT_RGB_565:
        color_tp_png = PNG_COLOR_TYPE_RGB;
        n_colors = 3;
        pvr2_pix_size = 2;
        break;
    case TEX_CTRL_PIX_FMT_YUV_422:
        color_tp_png = PNG_COLOR_TYPE_RGB;
        n_colors = 3;
        pvr2_pix_size = 3;
        dat_conv = (uint8_t*)malloc(sizeof(uint8_t) * n_colors * tex_w * tex_h);
        conv_yuv422_rgb888(dat_conv, dat, tex_w, tex_h);
        dat = dat_conv;
        break;
    default:
        err_val = -1;
        goto cleanup_png;
    }

    // this should not be possible, but scan-build thinks it is...?
    if (!tex_w || !tex_h)
        RAISE_ERROR(ERROR_INTEGRITY);

    png_bytepp row_pointers = (png_bytepp)calloc(tex_h, sizeof(png_bytep));
    if (!row_pointers)
        goto cleanup_png;

    unsigned row, col;
    for (row = 0; row < tex_h; row++) {
        png_bytep cur_row = row_pointers[row] =
            (png_bytep)malloc(sizeof(png_byte) * tex_w * n_colors);

        for (col = 0; col < tex_w; col++) {
            unsigned pix_idx = row * tex_w + col;
            uint8_t src_pix[4];
            unsigned red, green, blue, alpha;

            memcpy(src_pix, dat + pix_idx * pvr2_pix_size, sizeof(src_pix));

            switch (meta->pix_fmt) {
            case TEX_CTRL_PIX_FMT_ARGB_1555:
                alpha = src_pix[1] & 0x80 ? 255 : 0;
                red = (src_pix[1] & 0x7c) >> 2;
                green = ((src_pix[1] & 0x03) << 3) | ((src_pix[0] & 0xe0) >> 5);
                blue = src_pix[0] & 0x1f;

                red <<= 3;
                green <<= 3;
                blue <<= 3;
                break;
            case TEX_CTRL_PIX_FMT_ARGB_4444:
                blue = src_pix[0] & 0x0f;
                green = (src_pix[0] & 0xf0) >> 4;
                red = src_pix[1] & 0x0f;
                alpha = (src_pix[1] & 0xf0) >> 4;

                alpha <<= 4;
                red <<= 4;
                green <<= 4;
                blue <<= 4;
                break;
            case TEX_CTRL_PIX_FMT_RGB_565:
                blue = src_pix[0] & 0x1f;
                green = ((src_pix[0] & 0xe0) >> 5) | ((src_pix[1] & 0x7) << 3);
                red = (src_pix[1] & 0xf1) >> 3;

                red <<= 3;
                green <<= 2;
                blue <<= 3;
                break;
            case TEX_CTRL_PIX_FMT_YUV_422:
                red = src_pix[0];
                green = src_pix[1];
                blue = src_pix[2];
                break;
            default:
                err_val = -1;
                goto cleanup_rows;
            }

            cur_row[n_colors * col] = (png_byte)red;
            cur_row[n_colors * col + 1] = (png_byte)green;
            cur_row[n_colors * col + 2] = (png_byte)blue;
            if (n_colors == 4)
                cur_row[n_colors * col + 3] = (png_byte)alpha;
        }
    }

    png_set_IHDR(png_ptr, info_ptr, tex_w, tex_h, 8,
                 color_tp_png, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);
    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, info_ptr);

cleanup_rows:
    for (row = 0; row < tex_h; row++)
        if (row_pointers[row])
            free(row_pointers[row]);
/* cleanup_row_pointers: */
    free(row_pointers);

cleanup_png:
    png_destroy_write_struct(&png_ptr, &info_ptr);

finish:
    fclose(stream);
    if (dat_conv)
        free(dat_conv);
    return err_val;
}
