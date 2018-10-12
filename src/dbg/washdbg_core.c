/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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

#include <stdio.h>
#include <string.h>

#include "log.h"
#include "dreamcast.h"
#include "io/washdbg_tcp.h"

#include "dbg/washdbg_core.h"

#define BUF_LEN 1024

static char in_buf[BUF_LEN];
unsigned in_buf_pos;

struct washdbg_txt_state {
    char const *txt;
    unsigned pos;
};

static void washdbg_process_input(void);
static int washdbg_puts(char const *txt);

static unsigned washdbg_print_buffer(struct washdbg_txt_state *state);

enum washdbg_state {
    WASHDBG_STATE_BANNER,
    WASHDBG_STATE_PROMPT,
    WASHDBG_STATE_NORMAL,
    WASHDBG_STATE_BAD_INPUT,
    WASHDBG_STATE_CMD_CONTINUE,
    WASHDBG_STATE_RUNNING,
    WASHDBG_STATE_HELP,
    WASHDBG_STATE_CONTEXT_INFO,

    // permanently stop accepting commands because we're about to disconnect.
    WASHDBG_STATE_CMD_EXIT
} cur_state;

void washdbg_init(void) {
    washdbg_print_banner();
}

static struct continue_state {
    struct washdbg_txt_state txt;
} continue_state;

void washdbg_do_continue(void) {
    continue_state.txt.txt = "Continuing execution\n";
    continue_state.txt.pos = 0;

    cur_state = WASHDBG_STATE_CMD_CONTINUE;
}

void washdbg_do_exit(void) {
    LOG_INFO("User requested exit via WashDbg\n");
    dreamcast_kill();
    cur_state = WASHDBG_STATE_CMD_EXIT;
}

void washdbg_input_ch(char ch) {
    if (ch == '\r')
        return;

    // in_buf[1023] will always be \0
    if (in_buf_pos <= (BUF_LEN - 2))
        in_buf[in_buf_pos++] = ch;
}

struct print_banner_state {
    struct washdbg_txt_state txt;
} print_banner_state;

void washdbg_print_banner(void) {
    // this gets printed to the dev console every time somebody connects to the debugger
    static char const *login_banner =
        "Welcome to WashDbg!\n"
        "WashingtonDC Copyright (C) 2016-2018 snickerbockers\n"
        "This program comes with ABSOLUTELY NO WARRANTY;\n"
        "This is free software, and you are welcome to redistribute it\n"
        "under the terms of the GNU GPL version 3.\n\n";

    print_banner_state.txt.txt = login_banner;
    print_banner_state.txt.pos = 0;

    cur_state = WASHDBG_STATE_BANNER;
}

static struct help_state {
    struct washdbg_txt_state txt;
} help_state;

void washdbg_do_help(void) {
    static char const *help_msg =
        "WashDbg command list\n"
        "\n"
        "continue - continue execution when suspended.\n"
        "exit     - exit the debugger and close WashingtonDC\n"
        "help     - display this message\n";

    help_state.txt.txt = help_msg;
    help_state.txt.pos = 0;
    cur_state = WASHDBG_STATE_HELP;
}

struct context_info_state {
    struct washdbg_txt_state txt;
} context_info_state;

/*
 * Display info about the current context before showing a new prompt
 */
void washdbg_print_context_info(void) {
    char const *msg = NULL;
    switch (debug_current_context()) {
    case DEBUG_CONTEXT_SH4:
        msg = "Current debug context is SH4\n";
        break;
    case DEBUG_CONTEXT_ARM7:
        msg = "Current debug context is ARM7\n";
        break;
    default:
        msg = "Current debug context is <unknown/error>\n";
    }
    context_info_state.txt.txt = msg;
    context_info_state.txt.pos = 0;
    cur_state = WASHDBG_STATE_CONTEXT_INFO;
}

struct print_prompt_state {
    struct washdbg_txt_state txt;
} print_prompt_state;

void washdbg_print_prompt(void) {
    static char const *prompt = "(WashDbg): ";

    print_prompt_state.txt.txt = prompt;
    print_prompt_state.txt.pos = 0;
    cur_state = WASHDBG_STATE_PROMPT;
}

static struct bad_input_state {
    struct washdbg_txt_state txt;
    char bad_input_line[BUF_LEN];
} bad_input_state;

static void washdbg_bad_input(char const *bad_cmd) {
    snprintf(bad_input_state.bad_input_line,
             sizeof(bad_input_state.bad_input_line),
             "Unrecognized input \"%s\"\n", bad_cmd);
    bad_input_state.bad_input_line[BUF_LEN - 1] = '\0';

    bad_input_state.txt.txt = bad_input_state.bad_input_line;
    bad_input_state.txt.pos = 0;
    cur_state = WASHDBG_STATE_BAD_INPUT;
}

void washdbg_core_run_once(void) {
    switch (cur_state) {
    case WASHDBG_STATE_BANNER:
        if (washdbg_print_buffer(&print_banner_state.txt) == 0)
            washdbg_print_context_info();
        break;
    case WASHDBG_STATE_PROMPT:
        if (washdbg_print_buffer(&print_prompt_state.txt) == 0)
            cur_state = WASHDBG_STATE_NORMAL;
        break;
    case WASHDBG_STATE_CMD_CONTINUE:
        if (washdbg_print_buffer(&continue_state.txt) == 0) {
            debug_request_continue();
            cur_state = WASHDBG_STATE_RUNNING;
        }
        break;
    case WASHDBG_STATE_NORMAL:
        washdbg_process_input();
        break;
    case WASHDBG_STATE_BAD_INPUT:
        if (washdbg_print_buffer(&bad_input_state.txt) == 0)
            washdbg_print_prompt();
        break;
    case WASHDBG_STATE_HELP:
        if (washdbg_print_buffer(&help_state.txt) == 0)
            washdbg_print_prompt();
        break;
    case WASHDBG_STATE_CONTEXT_INFO:
        if (washdbg_print_buffer(&context_info_state.txt) == 0)
            washdbg_print_prompt();
        break;
    default:
        break;
    }
}

static void washdbg_process_input(void) {
    static char cur_line[BUF_LEN];

    char const *newline_ptr = strchr(in_buf, '\n');
    while (newline_ptr) {
        unsigned newline_idx = newline_ptr - in_buf;

        memset(cur_line, 0, sizeof(cur_line));
        memcpy(cur_line, in_buf, newline_idx);

        if (newline_idx < (BUF_LEN - 1)) {
            size_t chars_to_move = BUF_LEN - newline_idx - 1;
            memmove(in_buf, newline_ptr + 1, chars_to_move);
            in_buf_pos = 0;
        }

        if (strcmp(cur_line, "continue") == 0 ||
            strcmp(cur_line, "c") == 0) {
            washdbg_do_continue();
        } else if (strcmp(cur_line, "exit") == 0) {
            washdbg_do_exit();
        } else if (strcmp(cur_line, "help") == 0) {
            washdbg_do_help();
        } else {
            washdbg_bad_input(cur_line);
        }

        newline_ptr = strchr(in_buf, '\n');
    }
}

static int washdbg_puts(char const *txt) {
    return washdbg_tcp_puts(txt);
}

static unsigned washdbg_print_buffer(struct washdbg_txt_state *state) {
    char const *start = state->txt + state->pos;
    unsigned rem_chars = strlen(state->txt) - state->pos;
    if (rem_chars) {
        unsigned n_chars = washdbg_puts(start);
        if (n_chars == rem_chars)
            return 0;
        else
            state->pos += n_chars;
    } else {
        return 0;
    }
    return strlen(state->txt) - state->pos;
}
