/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "capstone/capstone.h"

#include "log.h"
#include "dreamcast.h"
#include "io/washdbg_tcp.h"
#include "sh4asm_core/disas.h"
#include "washdc/debugger.h"
#include "hw/arm7/arm7.h"

#include "dbg/washdbg_core.h"

#define BUF_LEN 1024

static char in_buf[BUF_LEN];
static unsigned in_buf_pos;

static csh capstone_handle;
static bool capstone_avail;

struct washdbg_txt_state {
    char const *txt;
    unsigned pos;
};

/*
 * map bp index to address
 */
static struct washdbg_bp_stat {
    uint32_t addr;
    bool enabled;
    bool valid;
} washdbg_bp_stat[NUM_DEBUG_CONTEXTS][DEBUG_N_BREAKPOINTS];

static void washdbg_process_input(void);
static int washdbg_puts(char const *txt);

static unsigned washdbg_print_buffer(struct washdbg_txt_state *state);

static void washdbg_print_banner(void);

static void washdbg_echo(int argc, char **argv);

static void washdbg_state_echo_process(void);

static int
eval_expression(char const *expr, enum dbg_context_id *ctx_id, unsigned *out);

static unsigned washdbg_print_x(void);

static int reg_idx_arm7(char const *reg_name);
static int reg_idx_sh4(char const *reg_name);

static bool is_dec_str(char const *str);
static unsigned parse_dec_str(char const *str);
static bool is_hex_str(char const *str);
static unsigned parse_hex_str(char const *str);

#ifdef ENABLE_DBG_COND
static int parse_int_str(char const *valstr, uint32_t *out);
#endif

static char const *washdbg_disas_single_sh4(uint32_t addr, uint16_t val);
static char const *washdbg_disas_single_arm7(uint32_t addr, uint32_t val);

enum washdbg_byte_count {
    WASHDBG_1_BYTE = 1,
    WASHDBG_2_BYTE = 2,
    WASHDBG_4_BYTE = 4,
    WASHDBG_INST   = 5
};

static int
parse_fmt_string(char const *str, enum washdbg_byte_count *byte_count_out,
                 unsigned *count_out);

enum washdbg_state {
    WASHDBG_STATE_BANNER,
    WASHDBG_STATE_PROMPT,
    WASHDBG_STATE_NORMAL,
    WASHDBG_STATE_BAD_INPUT,
    WASHDBG_STATE_CMD_CONTINUE,
    WASHDBG_STATE_RUNNING,
    WASHDBG_STATE_HELP,
    WASHDBG_STATE_CONTEXT_INFO,
    WASHDBG_STATE_PRINT_ERROR,
    WASHDBG_STATE_ECHO,
    WASHDBG_STATE_X,
    WASHDBG_STATE_CMD_BPSET,
    WASHDBG_STATE_CMD_BPLIST,
    WASHDBG_STATE_CMD_PRINT,

    // permanently stop accepting commands because we're about to disconnect.
    WASHDBG_STATE_CMD_EXIT
} cur_state;

void washdbg_init(void) {
    LOG_INFO("%s called\n", __func__);
    cs_err cs_err_val = cs_open(CS_ARCH_ARM, CS_MODE_ARM,
                                &capstone_handle);
    if (cs_err_val != CS_ERR_OK) {
        // disable disassembly for ARM7
        LOG_ERROR("cs_open returned error code %d\n", (int)cs_err_val);
    } else {
        capstone_avail = true;
    }

    washdbg_print_banner();
    memset(washdbg_bp_stat, 0, sizeof(washdbg_bp_stat));
}

void washdbg_cleanup(void* argp) {
    capstone_avail = false;
    cs_close(&capstone_handle);
}

static struct continue_state {
    struct washdbg_txt_state txt;
} continue_state;

void washdbg_do_continue(int argc, char **argv) {
    continue_state.txt.txt = "Continuing execution\n";
    continue_state.txt.pos = 0;

    cur_state = WASHDBG_STATE_CMD_CONTINUE;
}

static bool washdbg_is_continue_cmd(char const *cmd) {
    return strcmp(cmd, "c") == 0 ||
        strcmp(cmd, "continue") == 0;
}

void washdbg_do_exit(int argc, char **argv) {
    LOG_INFO("User requested exit via WashDbg\n");
    dreamcast_kill();
    cur_state = WASHDBG_STATE_CMD_EXIT;
}

static bool washdbg_is_exit_cmd(char const *cmd) {
    return strcmp(cmd, "exit") == 0;
}

void washdbg_input_ch(char ch) {
    if (ch == '\r')
        return;

    // in_buf[1023] will always be \0
    if (in_buf_pos <= (BUF_LEN - 2))
        in_buf[in_buf_pos++] = ch;
}

static bool washdbg_is_step_cmd(char const *cmd) {
    return strcmp(cmd, "s") == 0 ||
        strcmp(cmd, "step") == 0;
}

static void washdbg_do_step(int argc, char **argv) {
    LOG_INFO("WashDbg single-step requested\n");
    cur_state = WASHDBG_STATE_RUNNING;
    debug_request_single_step();
}

struct print_banner_state {
    struct washdbg_txt_state txt;
} print_banner_state;

static void washdbg_print_banner(void) {
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

void washdbg_do_help(int argc, char **argv) {
    static char const *help_msg =
        "WashDbg command list\n"
        "\n"
        "bpdel        - delete a breakpoint\n"
        "bpdis        - disable a breakpoint\n"
        "bpen         - enable a breakpoint\n"
        "bplist       - list all breakpoints\n"
        "bpset <addr> - set a breakpoint\n"
        "continue     - continue execution when suspended.\n"
        "echo         - echo back text\n"
        "exit         - exit the debugger and close WashingtonDC\n"
        "help         - display this message\n"
#ifdef ENABLE_DBG_COND
        "memwatch     - watch a specific memory address for a specific value\n"
#endif
        "print        - print a value\n"
#ifdef ENABLE_DBG_COND
        "regwatch     - watch for a register to be set to a given value\n"
#endif
        "step         - single-step\n"
        "x            - eXamine memory address\n";

    help_state.txt.txt = help_msg;
    help_state.txt.pos = 0;
    cur_state = WASHDBG_STATE_HELP;
}

static bool washdbg_is_help_cmd(char const *cmd) {
    return strcmp(cmd, "help") == 0;
}

struct context_info_state {
    char msg[128];
    struct washdbg_txt_state txt;
} context_info_state;

/*
 * Display info about the current context before showing a new prompt
 */
void washdbg_print_context_info(void) {
    uint32_t pc_next;
    uint32_t inst32;
    uint16_t inst16;
    char const *disas = "";

    switch (debug_current_context()) {
    case DEBUG_CONTEXT_SH4:
        pc_next = debug_pc_next(DEBUG_CONTEXT_SH4);
        if (debug_read_mem(DEBUG_CONTEXT_SH4, &inst16,
                           pc_next, sizeof(inst16)) == 0) {
            disas = washdbg_disas_single_sh4(pc_next, inst16);
        }
        snprintf(context_info_state.msg, sizeof(context_info_state.msg),
                 "Current debug context is SH4\n"
                 "PC is 0x%08x\n"
                 "next_inst:\n\t0x%08x: %s\n",
                 (unsigned)debug_get_reg(DEBUG_CONTEXT_SH4, SH4_REG_PC),
                 (unsigned)pc_next, disas);
        break;
    case DEBUG_CONTEXT_ARM7:
        pc_next = debug_pc_next(DEBUG_CONTEXT_ARM7);
        if (debug_read_mem(DEBUG_CONTEXT_ARM7, &inst32,
                           pc_next, sizeof(inst32)) == 0) {
            disas = washdbg_disas_single_arm7(pc_next, inst32);
        }
        snprintf(context_info_state.msg, sizeof(context_info_state.msg),
                 "Current debug context is ARM7\n"
                 "PC is 0x%08x\n"
                 "next_inst:\n\t0x%08x: %s\n",
                 (unsigned)debug_get_reg(DEBUG_CONTEXT_ARM7, ARM7_REG_PC),
                 (unsigned)pc_next, disas);
        break;
    default:
        snprintf(context_info_state.msg, sizeof(context_info_state.msg),
                 "Current debug context is <unknown/error>\n");
    }
    context_info_state.msg[sizeof(context_info_state.msg) - 1] = '\0';
    context_info_state.txt.txt = context_info_state.msg;
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

static struct print_error_state {
    struct washdbg_txt_state txt;
} print_error_state;

static void washdbg_print_error(char const *error) {
    print_error_state.txt.txt = error;
    print_error_state.txt.pos = 0;
    cur_state = WASHDBG_STATE_PRINT_ERROR;
}

static struct echo_state {
    int argc;
    char **argv;
    int cur_arg;
    unsigned cur_arg_pos;
    bool print_space;
} echo_state;

static void washdbg_echo(int argc, char **argv) {
    int arg_no;

    if (argc <= 1) {
        washdbg_print_prompt();
        return;
    }

    echo_state.cur_arg = 1;
    echo_state.cur_arg_pos = 0;
    echo_state.print_space = false;
    echo_state.argc = argc;
    echo_state.argv = (char**)calloc(sizeof(char*), argc);
    if (!echo_state.argv) {
        washdbg_print_error("Failed allocation.\n");
        goto cleanup_args;
    }

    for (arg_no = 0; arg_no < argc; arg_no++) {
        echo_state.argv[arg_no] = strdup(argv[arg_no]);
        if (!echo_state.argv[arg_no]) {
            washdbg_print_error("Failed allocation.\n");
            goto cleanup_args;
        }
    }

    cur_state = WASHDBG_STATE_ECHO;

    return;

cleanup_args:
    for (arg_no = 0; arg_no < argc; arg_no++)
        free(echo_state.argv[arg_no]);
    free(echo_state.argv);
}

static bool washdbg_is_echo_cmd(char const *cmd) {
    return strcmp(cmd, "echo") == 0;
}

#define WASHDBG_X_STATE_STR_LEN 128

enum x_state_disas_mode {
    X_STATE_DISAS_DISABLED,
    X_STATE_DISAS_SH4,
    X_STATE_DISAS_ARM7
};

static struct x_state {
    char str[WASHDBG_X_STATE_STR_LEN];
    size_t str_pos;

    void *dat;
    unsigned byte_count;
    unsigned count;
    unsigned idx;
    enum x_state_disas_mode disas_mode;

    uint32_t next_addr;
} x_state;

static void washdbg_x_set_string(void) {
    uint32_t val32;
    uint16_t val16;
    uint8_t val8;

    if (x_state.disas_mode == X_STATE_DISAS_SH4) {
        val16 = ((uint16_t*)x_state.dat)[x_state.idx];

        snprintf(x_state.str, sizeof(x_state.str), "0x%08x: %s\n",
                 x_state.next_addr,
                 washdbg_disas_single_sh4(x_state.next_addr, val16));
        x_state.str[sizeof(x_state.str) - 1] = '\0';
        return;
    } else if (x_state.disas_mode == X_STATE_DISAS_ARM7) {
        val32 = ((uint32_t*)x_state.dat)[x_state.idx];

        snprintf(x_state.str, sizeof(x_state.str), "0x%08x: %s\n",
                 x_state.next_addr,
                 washdbg_disas_single_arm7(x_state.next_addr, val32));
        x_state.str[sizeof(x_state.str) - 1] = '\0';
        return;
    }
    switch (x_state.byte_count) {
    case 4:
        val32 = ((uint32_t*)x_state.dat)[x_state.idx];
        snprintf(x_state.str, sizeof(x_state.str), "0x%08x: 0x%08x\n",
                 (unsigned)x_state.next_addr, (unsigned)val32);
        break;
    case 2:
        val16 = ((uint16_t*)x_state.dat)[x_state.idx];
        snprintf(x_state.str, sizeof(x_state.str), "0x%08x: 0x%04x\n",
                 (unsigned)x_state.next_addr, (unsigned)val16);
        break;
    case 1:
        val8 = ((uint8_t*)x_state.dat)[x_state.idx];
        snprintf(x_state.str, sizeof(x_state.str), "0x%08x: 0x%02x\n",
                 (unsigned)x_state.next_addr, (unsigned)val8);
        break;
    default:
        strncpy(x_state.str, "<ERROR>\n", sizeof(x_state.str));
        x_state.str[WASHDBG_X_STATE_STR_LEN - 1] = '\0';
    }
}

static void washdbg_x(int argc, char **argv) {
    unsigned addr;
    enum dbg_context_id ctx;
    char *fmt_str;

    if (argc != 2) {
        washdbg_print_error("only a single argument is supported for the x "
                            "command.\n");
        return;
    }

    fmt_str = strchr(argv[0], '/');
    if (fmt_str) {
        fmt_str++;
        printf("The format string is \"%s\"\n", fmt_str);
    }

    memset(&x_state, 0, sizeof(x_state));

    if (parse_fmt_string(fmt_str, &x_state.byte_count, &x_state.count) < 0) {
        washdbg_print_error("failed to parse x-command format string.\n");
        return;
    }

    if (eval_expression(argv[1], &ctx, &addr) != 0)
        return;

    if (x_state.byte_count == WASHDBG_INST) {
        switch (ctx) {
        case DEBUG_CONTEXT_SH4:
            x_state.byte_count = 2;
            x_state.disas_mode = X_STATE_DISAS_SH4;
            break;
        case DEBUG_CONTEXT_ARM7:
            x_state.byte_count = 4;
            if (capstone_avail) {
                x_state.disas_mode = X_STATE_DISAS_ARM7;
            } else {
                LOG_ERROR("capstone_avail is false\n");
                x_state.disas_mode = X_STATE_DISAS_DISABLED;
            }
            break;
        default:
            washdbg_print_error("unknown context ???\n");
            return;
        }
    }

    x_state.next_addr = addr;

    if (x_state.count > 1024 * 32) {
        washdbg_print_error("too much data\n");
        return;
    }

    x_state.dat = calloc(x_state.byte_count, x_state.count);
    if (!x_state.dat) {
        washdbg_print_error("failed allocation\n");
        return;
    }

    // now do the memory lookup here
    if (debug_read_mem(ctx, x_state.dat, addr,
                       x_state.byte_count * x_state.count) < 0) {
        washdbg_print_error("only a single argument is supported for the x "
                            "command.\n");
        return;
    }

    washdbg_x_set_string();
    x_state.idx = 1;
    x_state.next_addr += x_state.byte_count;

    cur_state = WASHDBG_STATE_X;
}

static unsigned washdbg_print_x(void) {
    char const *start = x_state.str + x_state.str_pos;
    size_t len = strlen(x_state.str);
    unsigned rem_chars = len - x_state.str_pos;
    if (rem_chars) {
        unsigned n_chars = washdbg_puts(start);
        if (n_chars == rem_chars)
            goto reload;
        else
            x_state.str_pos += n_chars;
    } else {
        goto reload;
    }
    return 1;

reload:
    if (x_state.idx == x_state.count)
        return 0;
    washdbg_x_set_string();
    x_state.idx++;
    x_state.next_addr += x_state.byte_count;
    return 1;
}

static bool washdbg_is_x_cmd(char const *cmd) {
    // TODO: implement formatted versions, like x/w
    return strcmp(cmd, "x") == 0 ||
        (strlen(cmd) && cmd[0] == 'x' && cmd[1] == '/');
}

#define WASHDBG_BPSET_STR_LEN 64

static struct bpset_state {
    struct washdbg_txt_state txt;
    char str[WASHDBG_BPSET_STR_LEN];
} bpset_state;

static void washdbg_bpset(int argc, char **argv) {
    if (argc != 2) {
        washdbg_print_error("only a single argument is supported for the bpset "
                            "command.\n");
        return;
    }

    unsigned addr;
    enum dbg_context_id ctx;
    if (eval_expression(argv[1], &ctx, &addr) != 0)
        return;

    unsigned bp_idx;
    for (bp_idx = 0; bp_idx < DEBUG_N_BREAKPOINTS; bp_idx++)
        if (!washdbg_bp_stat[ctx][bp_idx].valid) {
            washdbg_bp_stat[ctx][bp_idx].addr = addr;
            washdbg_bp_stat[ctx][bp_idx].valid = true;
            washdbg_bp_stat[ctx][bp_idx].enabled = true;
            break;
        }

    if (bp_idx >= DEBUG_N_BREAKPOINTS ||
        debug_add_break(ctx, addr) != 0) {
        washdbg_print_error("failed to add breakpoint\n");
        return;
    }

    memset(bpset_state.str, 0, sizeof(bpset_state.str));

    snprintf(bpset_state.str, sizeof(bpset_state.str),
             "breakpoint %u added successfully.\n", bp_idx);

    bpset_state.txt.txt = bpset_state.str;
    bpset_state.txt.pos = 0;

    cur_state = WASHDBG_STATE_CMD_BPSET;
}

static bool washdbg_is_bpset_cmd(char const *cmd) {
    return strcmp(cmd, "bpset") == 0;
}

#define WASHDBG_BPLIST_STR_LEN 64

static struct bplist_state {
    char str[WASHDBG_BPLIST_STR_LEN];
    unsigned str_idx;
    unsigned bp_next;
    enum dbg_context_id ctx_next;
    struct washdbg_txt_state txt;
} bplist_state;

static int washdbg_bplist_load_bp(void) {
    if (bplist_state.ctx_next >= NUM_DEBUG_CONTEXTS)
        return -1;

    struct washdbg_bp_stat *chosen = NULL;
    unsigned idx_cur;
    enum dbg_context_id ctx_cur;
    for (ctx_cur = bplist_state.ctx_next, idx_cur = bplist_state.bp_next;
         ctx_cur < NUM_DEBUG_CONTEXTS; ctx_cur++) {
        for (; idx_cur < DEBUG_N_BREAKPOINTS;
             idx_cur++) {
            struct washdbg_bp_stat *bp = &washdbg_bp_stat[ctx_cur][idx_cur];
            if (bp->valid) {
                chosen = bp;

                bplist_state.bp_next = idx_cur + 1;
                bplist_state.ctx_next = ctx_cur;
                if (bplist_state.bp_next >= DEBUG_N_BREAKPOINTS) {
                    bplist_state.bp_next = 0;
                    bplist_state.ctx_next++;
                }
                goto have_chosen;
            }
        }
        idx_cur = 0;
    }

have_chosen:

    if (!chosen)
        return -1;

    memset(bplist_state.str, 0, sizeof(bplist_state.str));

    char const *ctx_name = ctx_cur == DEBUG_CONTEXT_SH4 ? "sh4" :
        (ctx_cur == DEBUG_CONTEXT_ARM7 ? "arm7" : "unknown");
    snprintf(bplist_state.str, sizeof(bplist_state.str),
             "%s breakpoint %u: 0x%08x (%s)\n",
             ctx_name, idx_cur, (unsigned)chosen->addr,
             chosen->enabled ? "enabled" : "disabled");
    bplist_state.str[WASHDBG_BPLIST_STR_LEN - 1] = '\0';
    bplist_state.txt.pos = 0;
    return 0;
}

static void washdbg_bplist_run(void) {
    if (washdbg_print_buffer(&bplist_state.txt) != 0)
        return;
    if (washdbg_bplist_load_bp() != 0)
        washdbg_print_prompt();
}

static void washdbg_do_bplist(int argc, char **argv) {
    if (argc != 1) {
        washdbg_print_error("bplist takes no arguments!\n");
        return;
    }
    memset(&bplist_state, 0, sizeof(bplist_state));
    bplist_state.txt.txt = bplist_state.str;
    cur_state = WASHDBG_STATE_CMD_BPLIST;
}

static bool washdbg_is_bplist_cmd(char const *cmd) {
    return strcmp(cmd, "bplist") == 0;
}

static void washdbg_do_bpdis(int argc, char **argv) {
    if (argc != 2) {
        washdbg_print_error("need to provide breakpoint id\n");
        return;
    }

    enum dbg_context_id ctx;
    unsigned idx;
    if (eval_expression(argv[1], &ctx, &idx) != 0)
        return;

    if ((ctx != DEBUG_CONTEXT_SH4 && ctx != DEBUG_CONTEXT_ARM7) ||
        (idx >= DEBUG_N_BREAKPOINTS)) {
        washdbg_print_error("bad breakpoint idx\n");
        return;
    }

    struct washdbg_bp_stat *bp = &washdbg_bp_stat[ctx][idx];
    if (!bp->valid) {
        washdbg_print_error("breakpoint is not set\n");
        return;
    }

    if (debug_remove_break(ctx, bp->addr) != 0) {
        washdbg_print_error("failed to remove breakpoint\n");
        return;
    }
    bp->enabled = false;
    washdbg_print_prompt();
}


static bool washdbg_is_bpdis_cmd(char const *str) {
    return strcmp(str, "bpdis") == 0;
}

static void washdbg_do_bpen(int argc, char **argv) {
    if (argc != 2) {
        washdbg_print_error("need to provide breakpoint id\n");
        return;
    }

    enum dbg_context_id ctx;
    unsigned idx;
    if (eval_expression(argv[1], &ctx, &idx) != 0)
        return;

    if ((ctx != DEBUG_CONTEXT_SH4 && ctx != DEBUG_CONTEXT_ARM7) ||
        (idx >= DEBUG_N_BREAKPOINTS)) {
        washdbg_print_error("bad breakpoint idx\n");
        return;
    }

    struct washdbg_bp_stat *bp = &washdbg_bp_stat[ctx][idx];
    if (!bp->valid) {
        washdbg_print_error("breakpoint is not set\n");
        return;
    }

    if (debug_add_break(ctx, bp->addr) != 0) {
        washdbg_print_error("failed to re-add breakpoint\n");
        return;
    }
    bp->enabled = true;
    washdbg_print_prompt();
}

static bool washdbg_is_bpen_cmd(char const *str) {
    return strcmp(str, "bpen") == 0;
}

static void washdbg_do_bpdel(int argc, char **argv) {
    if (argc != 2) {
        washdbg_print_error("need to provide breakpoint id\n");
        return;
    }

    enum dbg_context_id ctx;
    unsigned idx;
    if (eval_expression(argv[1], &ctx, &idx) != 0)
        return;

    if ((ctx != DEBUG_CONTEXT_SH4 && ctx != DEBUG_CONTEXT_ARM7) ||
        (idx >= DEBUG_N_BREAKPOINTS)) {
        washdbg_print_error("bad breakpoint idx\n");
        return;
    }

    struct washdbg_bp_stat *bp = &washdbg_bp_stat[ctx][idx];
    if (!bp->valid) {
        washdbg_print_error("breakpoint is not set\n");
        return;
    }

    if (debug_remove_break(ctx, bp->addr) != 0) {
        washdbg_print_error("failed to remove breakpoint\n");
        return;
    }

    memset(bp, 0, sizeof(*bp));
    washdbg_print_prompt();
}

static bool washdbg_is_bpdel_cmd(char const *str) {
    return strcmp(str, "bpdel") == 0;
}

#define WASHDBG_PRINT_STATE_STR_LEN 128

static struct washdbg_print_state {
    char str[WASHDBG_PRINT_STATE_STR_LEN];
    struct washdbg_txt_state txt;
} print_state;

static void washdbg_print(int argc, char **argv) {
    unsigned val;
    enum dbg_context_id ctx;

    if (argc != 2) {
        washdbg_print_error("only a single argument is supported for the print "
                            "command.\n");
        return;
    }

    memset(&print_state, 0, sizeof(print_state));
    print_state.txt.txt = print_state.str;

    if (eval_expression(argv[1], &ctx, &val) != 0)
        return;

    snprintf(print_state.str, sizeof(print_state.str), "0x%08x\n", val);
    print_state.str[WASHDBG_PRINT_STATE_STR_LEN - 1] = '\0';

    cur_state = WASHDBG_STATE_CMD_PRINT;
}

static bool washdbg_is_print_cmd(char const *str) {
    return strcmp(str, "print") == 0 ||
        strcmp(str, "p") == 0;
}

static bool washdbg_is_regwatch_cmd(char const *str) {
    return strcmp(str, "regwatch") == 0;
}

static void washdbg_regwatch(int argc, char **argv) {
#ifdef ENABLE_DBG_COND
    if (argc != 4) {
        washdbg_print_error("usage: regwatch context register value\n");
        return;
    }

    enum dbg_context_id ctx;
    if (strcmp(argv[1], "arm7") == 0) {
        ctx = DEBUG_CONTEXT_ARM7;
    } else if (strcmp(argv[1], "sh4") == 0) {
        ctx = DEBUG_CONTEXT_SH4;
    } else {
        washdbg_print_error("unrecognized context string.\n");
        return;
    }

    int reg_idx;
    if (ctx == DEBUG_CONTEXT_ARM7) {
        reg_idx = reg_idx_arm7(argv[2]);
    } else {
        reg_idx = reg_idx_sh4(argv[2]);
    }

    if (reg_idx < 0) {
        washdbg_print_error("unrecognized register.\n");
        return;
    }

    uint32_t value;
    if (parse_int_str(argv[3], &value) != 0)
        return;

    if (!debug_reg_cond(ctx, reg_idx, value))
        washdbg_print_error("failed to insert condition\n");
    else
        washdbg_print_prompt();

#else
    washdbg_print_error("regwatch command not available; rebuild WashingtonDC "
                        "with -DENABLE_DBG_COND=On.\n");
#endif
}

static bool washdbg_is_memwatch_cmd(char const *str) {
    return strcmp(str, "memwatch") == 0;
}

static void washdbg_memwatch(int argc, char **argv) {
#ifdef ENABLE_DBG_COND
    uint32_t size, addr, val;

    if (argc != 5) {
        washdbg_print_error("usage: memwatch context size addr value\n");
        return;
    }

    enum dbg_context_id ctx;
    if (strcmp(argv[1], "arm7") == 0) {
        ctx = DEBUG_CONTEXT_ARM7;
    } else if (strcmp(argv[1], "sh4") == 0) {
        ctx = DEBUG_CONTEXT_SH4;
    } else {
        washdbg_print_error("unrecognized context string.\n");
        return;
    }

    if (parse_int_str(argv[2], &size) != 0)
        return;
    if (parse_int_str(argv[3], &addr) != 0)
        return;
    if (parse_int_str(argv[4], &val) != 0)
        return;

    if (!debug_mem_cond(ctx, addr, val, size))
        washdbg_print_error("failed to insert condition\n");
    else
        washdbg_print_prompt();

#else
    washdbg_print_error("memwatch command not available; rebuild WashingtonDC "
                        "with -DENABLE_DBG_COND=On.\n");
#endif
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
    case WASHDBG_STATE_PRINT_ERROR:
        if (washdbg_print_buffer(&print_error_state.txt) == 0)
            washdbg_print_prompt();
        break;
    case WASHDBG_STATE_ECHO:
        washdbg_state_echo_process();
        break;
    case WASHDBG_STATE_X:
        if (washdbg_print_x() == 0) {
            free(x_state.dat);
            x_state.dat = NULL;
            washdbg_print_prompt();
        }
        break;
    case WASHDBG_STATE_CMD_BPSET:
        if (washdbg_print_buffer(&bpset_state.txt) == 0)
            washdbg_print_prompt();
        break;
    case WASHDBG_STATE_CMD_BPLIST:
        washdbg_bplist_run();
        break;
    case WASHDBG_STATE_CMD_PRINT:
        if (washdbg_print_buffer(&print_state.txt) == 0)
            washdbg_print_prompt();
        break;
    default:
        break;
    }
}

void washdbg_core_on_break(enum dbg_context_id id, void *argptr) {
    if (cur_state != WASHDBG_STATE_RUNNING)
        RAISE_ERROR(ERROR_INTEGRITY);
    washdbg_print_context_info();
}

// maximum length of a single argument
#define SINGLE_ARG_MAX 128

// maximum number of arguments
#define MAX_ARG_COUNT 256

static void washdbg_process_input(void) {
    static char cur_line[BUF_LEN];
    int argc = 0;
    char **argv = NULL;
    int arg_no;

    char const *newline_ptr = strchr(in_buf, '\n');
    if (newline_ptr) {
        unsigned newline_idx = newline_ptr - in_buf;

        memset(cur_line, 0, sizeof(cur_line));
        memcpy(cur_line, in_buf, newline_idx);

        if (newline_idx < (BUF_LEN - 1)) {
            size_t chars_to_move = BUF_LEN - newline_idx - 1;
            memmove(in_buf, newline_ptr + 1, chars_to_move);
            in_buf_pos = 0;
        }

        // Now separate the current line out into arguments
        char *token = strtok(cur_line, " \t");
        while (token) {
            if (argc + 1 > MAX_ARG_COUNT) {
                washdbg_print_error("too many arguments\n");
                goto cleanup_args;
            }

            // the + 1 is to add in space for the \0
            size_t tok_len = strlen(token) + 1;

            if (tok_len > SINGLE_ARG_MAX) {
                washdbg_print_error("argument exceeded maximum length.\n");
                goto cleanup_args;
            }

            char *new_arg = (char*)malloc(tok_len * sizeof(char));
            if (!new_arg) {
                washdbg_print_error("Failed allocation.\n");
                goto cleanup_args;
            }

            memcpy(new_arg, token, tok_len * sizeof(char));

            char **new_argv = (char**)realloc(argv, sizeof(char*) * (argc + 1));
            if (!new_argv) {
                washdbg_print_error("Failed allocation.\n");
                goto cleanup_args;
            }

            argv = new_argv;
            argv[argc] = new_arg;
            argc++;

            token = strtok(NULL, " \t");
        }

        char const *cmd;
        if (argc)
            cmd = argv[0];
        else
            cmd = "";

        if (strlen(cmd)) {
            if (washdbg_is_continue_cmd(cmd)) {
                washdbg_do_continue(argc, argv);
            } else if (washdbg_is_exit_cmd(cmd)) {
                washdbg_do_exit(argc, argv);
            } else if (washdbg_is_help_cmd(cmd)) {
                washdbg_do_help(argc, argv);
            } else if (washdbg_is_echo_cmd(cmd)) {
                washdbg_echo(argc, argv);
            } else if (washdbg_is_x_cmd(cmd)) {
                washdbg_x(argc, argv);
            } else if (washdbg_is_step_cmd(cmd)) {
                washdbg_do_step(argc, argv);
            } else if (washdbg_is_bpset_cmd(cmd)) {
                washdbg_bpset(argc, argv);
            } else if (washdbg_is_bplist_cmd(cmd)) {
                washdbg_do_bplist(argc, argv);
            } else if (washdbg_is_bpdis_cmd(cmd)) {
                washdbg_do_bpdis(argc, argv);
            } else if (washdbg_is_bpen_cmd(cmd)) {
                washdbg_do_bpen(argc, argv);
            } else if (washdbg_is_bpdel_cmd(cmd)) {
                washdbg_do_bpdel(argc, argv);
            } else if (washdbg_is_print_cmd(cmd)) {
                washdbg_print(argc, argv);
            } else if (washdbg_is_regwatch_cmd(cmd)) {
                washdbg_regwatch(argc, argv);
            } else if (washdbg_is_memwatch_cmd(cmd)) {
                washdbg_memwatch(argc, argv);
            } else {
                washdbg_bad_input(cmd);
            }
        } else {
            washdbg_print_prompt();
        }
    }

cleanup_args:
    for (arg_no = 0; arg_no < argc; arg_no++)
        free(argv[arg_no]);
    free(argv);
}

static int washdbg_puts(char const *txt) {
    return washdbg_tcp_puts(txt);
}

static void washdbg_state_echo_process(void) {
    if (echo_state.cur_arg >= echo_state.argc) {
        if (echo_state.print_space) {
            if (washdbg_puts("\n"))
                echo_state.print_space = false;
            else
                return;
        }
        washdbg_print_prompt();
        int arg_no;
        for (arg_no = 0; arg_no < echo_state.argc; arg_no++)
            free(echo_state.argv[arg_no]);
        free(echo_state.argv);
        memset(&echo_state, 0, sizeof(echo_state));
        return;
    }

    for (;;) {
        if (echo_state.print_space == true) {
            if (washdbg_puts(" "))
                echo_state.print_space = false;
            else
                return;
        }

        char *arg = echo_state.argv[echo_state.cur_arg];
        unsigned arg_len = strlen(arg);
        unsigned arg_pos = echo_state.cur_arg_pos;
        unsigned rem_chars = arg_len - arg_pos;

        if (rem_chars) {
            unsigned n_chars = washdbg_puts(arg + arg_pos);
            if (n_chars == rem_chars) {
                echo_state.cur_arg_pos = 0;
                echo_state.cur_arg++;
                echo_state.print_space = true;
                if (echo_state.cur_arg >= echo_state.argc)
                    return;
            } else {
                echo_state.cur_arg_pos += n_chars;
                return;
            }
        }
    }
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

static bool is_hex_str(char const *str) {
    if (*str == 0)
        return false; // empty string
    while (*str)
        switch (*str++) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case 'a':
        case 'A':
        case 'b':
        case 'B':
        case 'c':
        case 'C':
        case 'd':
        case 'D':
        case 'e':
        case 'E':
        case 'f':
        case 'F':
            break;
        default:
            return false;
        }
    return true;
}

static bool is_dec_str(char const *str) {
    if (*str == 0)
        return false; // empty string
    while (*str)
        if (!isdigit(*str++))
            return false;
    return true;
}

static unsigned parse_dec_str(char const *str) {
    size_t len = strlen(str);
    if (!len)
        return 0; // error condition; just ignore it for now
    size_t idx = len - 1;
    unsigned total = 0;
    unsigned scale = 1;
    do {
        unsigned weight = str[idx] - '0';
        total += scale * weight;
        scale *= 10;
    } while (idx--);
    return total;
}

static unsigned parse_hex_str(char const *str) {
    size_t len = strlen(str);
    if (!len)
        return 0; // error condition; just ignore it for now
    size_t idx = len - 1;
    unsigned total = 0;
    unsigned scale = 1;
    do {
        /* unsigned weight = str[idx] - '0'; */
        unsigned weight;
        if (str[idx] >= '0' && str[idx] <= '9')
            weight = str[idx] - '0';
        else if (str[idx] >= 'a' && str[idx] <= 'f')
            weight = str[idx] - 'a' + 10;
        else if (str[idx] >= 'A' && str[idx] <= 'F')
            weight = str[idx] - 'A' + 10;
        else
            weight = 0; // error condition; just ignore it for now
        total += scale * weight;
        scale *= 16;
    } while (idx--);
    printf("%s is %u\n", str, total);
    return total;
}

static struct name_map {
    char const *str;
    int idx;
} const sh4_reg_map[] = {
    { "r0", SH4_REG_R0 },
    { "r1", SH4_REG_R1 },
    { "r2", SH4_REG_R2 },
    { "r3", SH4_REG_R3 },
    { "r4", SH4_REG_R4 },
    { "r5", SH4_REG_R5 },
    { "r6", SH4_REG_R6 },
    { "r7", SH4_REG_R7 },
    { "r8", SH4_REG_R8 },
    { "r9", SH4_REG_R9 },
    { "r10", SH4_REG_R10 },
    { "r11", SH4_REG_R11 },
    { "r12", SH4_REG_R12 },
    { "r13", SH4_REG_R13 },
    { "r14", SH4_REG_R14 },
    { "r15", SH4_REG_R15 },

    { "r0b", SH4_REG_R0_BANK },
    { "r1b", SH4_REG_R1_BANK },
    { "r2b", SH4_REG_R2_BANK },
    { "r3b", SH4_REG_R3_BANK },
    { "r4b", SH4_REG_R4_BANK },
    { "r5b", SH4_REG_R5_BANK },
    { "r6b", SH4_REG_R6_BANK },
    { "r7b", SH4_REG_R7_BANK },

    { "fr0", SH4_REG_FR0 },
    { "fr1", SH4_REG_FR1 },
    { "fr2", SH4_REG_FR2 },
    { "fr3", SH4_REG_FR3 },
    { "fr4", SH4_REG_FR4 },
    { "fr5", SH4_REG_FR5 },
    { "fr6", SH4_REG_FR6 },
    { "fr7", SH4_REG_FR7 },
    { "fr8", SH4_REG_FR8 },
    { "fr9", SH4_REG_FR9 },
    { "fr10", SH4_REG_FR10 },
    { "fr11", SH4_REG_FR11 },
    { "fr12", SH4_REG_FR12 },
    { "fr13", SH4_REG_FR13 },
    { "fr14", SH4_REG_FR14 },
    { "fr15", SH4_REG_FR15 },

    // TODO: double-precision registers, vector registers, XMTRX

    { "xf0", SH4_REG_XF0 },
    { "xf1", SH4_REG_XF1 },
    { "xf2", SH4_REG_XF2 },
    { "xf3", SH4_REG_XF3 },
    { "xf4", SH4_REG_XF4 },
    { "xf5", SH4_REG_XF5 },
    { "xf6", SH4_REG_XF6 },
    { "xf7", SH4_REG_XF7 },
    { "xf8", SH4_REG_XF8 },
    { "xf9", SH4_REG_XF9 },
    { "xf10", SH4_REG_XF10 },
    { "xf11", SH4_REG_XF11 },
    { "xf12", SH4_REG_XF12 },
    { "xf13", SH4_REG_XF13 },
    { "xf14", SH4_REG_XF14 },
    { "xf15", SH4_REG_XF15 },

    { "fpscr", SH4_REG_FPSCR },
    { "fpul", SH4_REG_FPUL },
    { "sr", SH4_REG_SR },
    { "ssr", SH4_REG_SSR },
    { "spc", SH4_REG_SPC },
    { "gbr", SH4_REG_GBR },
    { "vbr", SH4_REG_VBR },
    { "sgr", SH4_REG_SGR },
    { "dbr", SH4_REG_DBR },
    { "mach", SH4_REG_MACH },
    { "macl", SH4_REG_MACL },
    { "pr", SH4_REG_PR },
    { "pc", SH4_REG_PC },

    { NULL }
};

__attribute__((unused))
static struct name_map const arm7_reg_map[] = {
    { "r0", ARM7_REG_R0 },
    { "r1", ARM7_REG_R1 },
    { "r2", ARM7_REG_R2 },
    { "r3", ARM7_REG_R3 },
    { "r4", ARM7_REG_R4 },
    { "r5", ARM7_REG_R5 },
    { "r6", ARM7_REG_R6 },
    { "r7", ARM7_REG_R7 },
    { "r8", ARM7_REG_R8 },
    { "r9", ARM7_REG_R9 },
    { "r10", ARM7_REG_R10 },
    { "r11", ARM7_REG_R11 },
    { "r12", ARM7_REG_R12 },
    { "r13", ARM7_REG_R13 },
    { "r14", ARM7_REG_R14 },
    { "r15", ARM7_REG_R15 },

    { "sb", ARM7_REG_R9 },
    { "sl", ARM7_REG_R10 },
    { "fp", ARM7_REG_R11 },
    { "ip", ARM7_REG_R12 },
    { "sp", ARM7_REG_R13 },
    { "lr", ARM7_REG_R14 },
    { "pc", ARM7_REG_PC },

    { "r8_fiq", ARM7_REG_R8_FIQ },
    { "r9_fiq", ARM7_REG_R9_FIQ },
    { "r10_fiq", ARM7_REG_R10_FIQ },
    { "r11_fiq", ARM7_REG_R11_FIQ },
    { "r12_fiq", ARM7_REG_R12_FIQ },
    { "r13_fiq", ARM7_REG_R13_FIQ },
    { "r14_fiq", ARM7_REG_R14_FIQ },
    { "r13_svc", ARM7_REG_R13_SVC },
    { "r14_svc", ARM7_REG_R14_SVC },
    { "r13_abt", ARM7_REG_R13_ABT },
    { "r14_abt", ARM7_REG_R14_ABT },
    { "r13_irq", ARM7_REG_R13_IRQ },
    { "r14_irq", ARM7_REG_R14_IRQ },
    { "r13_und", ARM7_REG_R13_UND },
    { "r14_und", ARM7_REG_R14_UND },

    { "cpsr", ARM7_REG_CPSR },

    { "spsr_fiq", ARM7_REG_SPSR_FIQ },
    { "spsr_svc", ARM7_REG_SPSR_SVC },
    { "spsr_abt", ARM7_REG_SPSR_ABT },
    { "spsr_irq", ARM7_REG_SPSR_IRQ },
    { "spsr_und", ARM7_REG_SPSR_UND },

    { NULL }
};

static int reg_idx_sh4(char const *reg_name) {
    struct name_map const *cursor = sh4_reg_map;

    while (cursor->str) {
        if (strcmp(reg_name, cursor->str) == 0)
            return cursor->idx;
        cursor++;
    }
    return -1;
}

static int reg_idx_arm7(char const *reg_name) {
    struct name_map const *cursor = arm7_reg_map;

    while (cursor->str) {
        if (strcmp(reg_name, cursor->str) == 0)
            return cursor->idx;
        cursor++;
    }
    return -1;
}

/*
 * expression format:
 * <ctx>:0xhex_val
 * OR
 * <ctx>:dec_val
 * OR
 * <ctx>:$reg_name
 *
 * ctx can be arm7 or sh4.  If it is not provided, it defaults to the current
 * context.  If the command interprets the value as being a pointer, then ctx
 * indicates whether it points to arm7's memory space or sh4's memory space.
 *
 * If the command does not interpret the value as a pointer, then ctx only
 * matters for the $reg_name form.  However, ctx can still be speciified even
 * though it is useless.
 */
static int eval_expression(char const *expr, enum dbg_context_id *ctx_id, unsigned *out) {
    enum dbg_context_id ctx = debug_current_context();

    char *first_colon = strchr(expr, ':');
    if (first_colon != NULL) {
        unsigned n_chars = first_colon - expr;
        if (n_chars == 3 && toupper(expr[0]) == 'S' &&
            toupper(expr[1]) == 'H' && toupper(expr[2]) == '4') {
            ctx = DEBUG_CONTEXT_SH4;
        } else if (n_chars == 4 && toupper(expr[0]) == 'A' &&
                   toupper(expr[1]) == 'R' && toupper(expr[2]) == 'M' &&
                   toupper(expr[3]) == '7') {
            ctx = DEBUG_CONTEXT_ARM7;
        } else {
            // unrecognized context
            washdbg_print_error("Unknown context\n");
            return -1;
        }

        expr = first_colon + 1;
    }

    *ctx_id = ctx;

    if (strlen(expr) == 0) {
        washdbg_print_error("empty expression\n");
        return -1;
    }

    if (expr[0] == '$') {
        // register
        if (ctx == DEBUG_CONTEXT_SH4) {
            int reg_idx = reg_idx_sh4(expr + 1);
            if (reg_idx >= 0) {
                *out = debug_get_reg(DEBUG_CONTEXT_SH4, reg_idx);
                return 0;
            }
            washdbg_print_error("unknown sh4 register\n");
            return -1;
        } else if (ctx == DEBUG_CONTEXT_ARM7) {
            int reg_idx = reg_idx_arm7(expr + 1);
            if (reg_idx >= 0) {
                *out = debug_get_reg(DEBUG_CONTEXT_ARM7, reg_idx);
                return 0;
            }
            washdbg_print_error("unknown arm7 register\n");
            return -1;
        } else {
            washdbg_print_error("register expressions are not implemented yet\n");
            return -1;
        }
    } else if (expr[0] == '0' && toupper(expr[1]) == 'X' &&
               is_hex_str(expr + 2)) {
        // hex
        *out = parse_hex_str(expr);
        return 0;
    } else if (is_dec_str(expr)) {
        // decimal
        *out = parse_dec_str(expr);
        return 0;
    } else {
        // error
        washdbg_print_error("unknown expression class\n");
        return -1;
    }
}

static int
parse_fmt_string(char const *str, enum washdbg_byte_count *byte_count_out,
                 unsigned *count_out) {
    bool have_count = false;
    bool have_byte_count = false;
    unsigned byte_count = 4;
    unsigned count = 1;

    bool parsing_digits = false;

    char const *digit_start = NULL;

    if (!str)
        goto the_end;

    while (*str || parsing_digits) {
        if (parsing_digits) {
            if (*str < '0' || *str > '9') {
                parsing_digits = false;
                unsigned n_chars = str - digit_start + 1;
                if (n_chars >= 32)
                    return -1;
                char tmp[32];
                strncpy(tmp, digit_start, sizeof(tmp));
                tmp[31] = '\0';
                if (have_count)
                    return -1;
                have_count = true;
                count = atoi(tmp);

                continue;
            }
        } else {
            switch (*str) {
            case 'w':
                if (have_byte_count)
                    return -1;
                byte_count = WASHDBG_4_BYTE;
                have_byte_count = true;
                break;
            case 'h':
                if (have_byte_count)
                    return -1;
                byte_count = WASHDBG_2_BYTE;
                have_byte_count = true;
                break;
            case 'b':
                if (have_byte_count)
                    return -1;
                byte_count = WASHDBG_1_BYTE;
                have_byte_count = true;
                break;
            case 'i':
                if (have_byte_count)
                    return -1;
                byte_count = WASHDBG_INST;
                have_byte_count = true;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                parsing_digits = true;
                digit_start = str;
                continue;
            default:
                return -1;
            }
        }
        str++;
    }

    /*
     * This limit is arbitrary, you can increase or decrease it as you'd like.
     * I just put this in there to keep things sane.
     */
    if (count >= 2048) {
        washdbg_print_error("too much data\n");
        return -1;
    }

the_end:

    *count_out = count;
    *byte_count_out = byte_count;

    return 0;
}

#define DISAS_LINE_LEN 128
static char sh4_disas_line[DISAS_LINE_LEN];

static void washdbg_disas_single_emit(char ch) {
    size_t len = strlen(sh4_disas_line);
    if (len >= DISAS_LINE_LEN - 1)
        return; // no more space
    sh4_disas_line[len] = ch;
}

static char const *washdbg_disas_single_sh4(uint32_t addr, uint16_t val) {
    memset(sh4_disas_line, 0, sizeof(sh4_disas_line));
    disas_inst(val, washdbg_disas_single_emit);

    return sh4_disas_line;
}

static char const *washdbg_disas_single_arm7(uint32_t addr, uint32_t val) {
    cs_insn *insn;
    static char buf[DISAS_LINE_LEN];

    size_t count = cs_disasm(capstone_handle, (uint8_t*)&val,
                             sizeof(val), addr, 1, &insn);

    if (count == 1) {
        snprintf(buf, sizeof(buf), "%s %s", insn->mnemonic, insn->op_str);
    } else {
        LOG_ERROR("cs_disasm returned %u; cs_errno is %d\n",
                  (unsigned)count, (int)cs_errno(capstone_handle));
        snprintf(buf, sizeof(buf), "0x%08x", (unsigned)val);
    }

    if (count)
        cs_free(insn, count);

    buf[sizeof(buf) - 1] = '\0';

    return buf;
}

#ifdef ENABLE_DBG_COND
static int parse_int_str(char const *valstr, uint32_t *out) {
    if (is_dec_str(valstr)) {
        *out = parse_dec_str(valstr);
        return 0;
    } else if (strlen(valstr) > 2 && valstr[0] == '0' && valstr[1] == 'x' &&
               is_hex_str(valstr + 2)) {
        *out = parse_hex_str(valstr + 2);
        return 0;
    } else {
        LOG_ERROR("valstr is \"%s\"\n", valstr);
        washdbg_print_error("unable to parse value.\n");
    }
    return - 1;
}
#endif
