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

#include "cons.h"

#include "cmd.h"

#define CMD_BUF_SIZE_SHIFT 10
#define CMD_BUF_SIZE (1 << CMD_BUF_SIZE_SHIFT)

static char cmd_buf[CMD_BUF_SIZE];
unsigned cmd_buf_len;

static int cmd_echo(int argc, char **argv);

struct cmd {
    char const *cmd_name;
    int(*cmd_handler)(int, char**);
} cmd_list[] = {
    { "echo", cmd_echo },
    { NULL, NULL }
};

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

    struct cmd const *cmd = cmd_list;
    while (cmd->cmd_name) {
        if (strcmp(cmd->cmd_name, cmd_name) == 0) {
            cmd->cmd_handler(argc, argv); // TODO: check the return value
            break;
        }
        cmd++;
    }

    if (!cmd->cmd_name)
        cons_puts("ERROR: unable to run command\n");

    // free argv
    for (idx = 0; idx < argc; idx++)
        free(argv[idx]);
    free(argv);
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
