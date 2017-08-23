/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "gfx/gfx_config.h"
#include "cons.h"
#include "dreamcast.h"

#include "cmd.h"

#define CMD_BUF_SIZE_SHIFT 10
#define CMD_BUF_SIZE (1 << CMD_BUF_SIZE_SHIFT)

static char cmd_buf[CMD_BUF_SIZE];
unsigned cmd_buf_len;

static int cmd_echo(int argc, char **argv);
static int cmd_help(int argc, char **argv);
static int cmd_render_set_mode(int argc, char **argv);
static int cmd_exit(int argc, char **argv);
static int cmd_resume_execution(int argc, char **argv);
static int cmd_suspend_execution(int argc, char **argv);
static int cmd_begin_execution(int argc, char **argv);

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
        .cmd_name = "begin-execution",
        .summary = "begin executing with the input settings",
        .help_str =
        "begin-execution\n"
        "\n"
        "start emulator execution\n",
        .cmd_handler = cmd_begin_execution
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
        "This command does not work on builds with the remote GDB frontend\n"
        "enabled.  Use the gdb frontend to control execution instead.\n",
#else
        "This command is enabled because the remote GDB frontend is not\n"
        "enabled.  If WashingtonDC had been built with -DENABLE_DEBUGGER=On,\n"
        "then this command would not be available.\n",
#endif
        .cmd_handler = cmd_resume_execution
    },
    {
        .cmd_name = "suspend-execution",
        .summary = "suspend execution while the emulator is running.",
        .help_str =
        "suspend-execution\n"
        "\n"
        "If WashingtonDC is running, then suspend execution.\n"
#ifdef ENABLE_DEBUGGER
        "This command does not work on builds with the remote GDB frontend\n"
        "enabled.  Use the gdb frontend to control execution instead.\n",
#else
        "This command is enabled because the remote GDB frontend is not\n"
        "enabled.  If WashingtonDC had been built with -DENABLE_DEBUGGER=On,\n"
        "then this command would not be available.\n",
#endif
        .cmd_handler = cmd_suspend_execution
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
    cons_puts("ERROR: unable to control execution from the cmd prompt in gdb "
              "builds\n");
    return 1;
#else
    enum dc_state dc_state = dc_get_state();

    if (dc_state == DC_STATE_SUSPEND) {
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_SUSPEND);
        return 0;
    }

    cons_puts("ERROR: unable to resume execution because WashingtonDC is not "
              "suspended\n");
    return 1;
#endif
}

static int cmd_suspend_execution(int argc, char **argv) {
#ifdef ENABLE_DEBUGGER
    cons_puts("ERROR: unable to control execution from the cmd prompt in gdb "
              "builds\n");
    return 1;
#else
    enum dc_state dc_state = dc_get_state();

    if (dc_state == DC_STATE_RUNNING) {
        dc_state_transition(DC_STATE_SUSPEND, DC_STATE_RUNNING);
        return 0;
    }

    cons_puts("ERROR: unable to suspend execution because WashingtonDC is not "
              "running\n");
    return 1;
#endif
}

static int cmd_begin_execution(int argc, char **argv) {
#ifdef ENABLE_DEBUGGER
    cons_puts("ERROR: unable to control execution from the cmd prompt in gdb "
              "builds\n");
    return 1;
#else
    enum dc_state dc_state = dc_get_state();

    if (dc_state == DC_STATE_NOT_RUNNING) {
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_NOT_RUNNING);
        return 0;
    }

    cons_puts("ERROR: unable to begin execution because WashingtonDC is "
              "already running\n");
    return 1;
#endif
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
