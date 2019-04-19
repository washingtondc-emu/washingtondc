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

#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>

#include "dreamcast.h"
#include "washdc/fifo.h"
#include "washdc/MemoryMap.h"
#include "hw/arm7/arm7.h"

#include "washdc/debugger.h"

#ifndef ENABLE_DEBUGGER
#error This file should not be included unless the debugger is enabled
#endif

static DEF_ERROR_INT_ATTR(dbg_state);

struct breakpoint {
    addr32_t addr;
    bool enabled;
};

struct watchpoint {
    addr32_t addr;
    unsigned len;
    bool enabled;
};

struct debug_context {
    enum dbg_context_id id;
    void *cpu;
    struct memory_map *map;

    struct breakpoint breakpoints[DEBUG_N_BREAKPOINTS];
    struct watchpoint w_watchpoints[DEBUG_N_W_WATCHPOINTS];
    struct watchpoint r_watchpoints[DEBUG_N_R_WATCHPOINTS];

    // when a watchpoint gets triggered, at_watchpoint is set to true
    // and the memory address is placed in watchpoint_addr
    addr32_t watchpoint_addr;

    // when this is true and at_watchpoint is true: read-watchpoint
    // when this is false and at_watchpoint is true: write-watchpoint
    bool is_read_watchpoint;

    enum debug_state cur_state;

    /*
     * this gets cleared by debug_request_single_step to request the debugger do a
     * single-step.
     *
     * debug_request_single_step is called from outside of the emu thread
     *
     * The reason why this is per-context is that we want the debugger to skip
     * over other contexts when the user requests a single-step just before a
     * context-switch.
     *
     * That way we can still handle breakpoints and watchpoints from within the
     * other context and return to single-stepping through this context once the
     * context switches back to the original context.
     */
    atomic_flag not_single_step;
};

struct debugger {
    struct debug_frontend const *frontend;

    enum dbg_context_id cur_ctx;
    struct debug_context contexts[NUM_DEBUG_CONTEXTS];

    /*
     * this gets cleared by debug_request_break to request the debugger break
     *
     * debug_request_break is called from outside of the emu thread in response
     * to the user pressing Ctrl+C on his gdb client.
     */
    atomic_flag not_request_break;

    /*
     * this gets cleared by debug_request_continue to request the debugger
     * continue execution from a breakpoint or watchpoin
     *
     * debug_request_continue is called from outside of the emu thread
     */
    atomic_flag not_continue;

    /*
     * this gets cleared by debug_request_detach to request the debugger detach
     *
     * debug_request_detach is called from outside of the emu thread
     */
    atomic_flag not_detach;
};

static struct debugger dbg;

static struct debug_context *get_ctx(void);


// uncomment this line to make the debugger print what it's doing
// #define DEBUGGER_LOG_VERBOSE

#ifdef DEBUGGER_LOG_VERBOSE
#define DBG_TRACE(msg, ...) dbg_do_trace(msg, ##__VA_ARGS__)
#else
#define DBG_TRACE(msg, ...)
#endif

static void frontend_attach(void);
static void frontend_on_break(void);
static void frontend_on_softbreak(cpu_inst_param inst, addr32_t addr);
static void frontend_on_cleanup(void);
static void frontend_run_once(void);

#ifdef ENABLE_WATCHPOINTS
static void frontend_on_read_watchpoint(addr32_t addr);
static void frontend_on_write_watchpoint(addr32_t addr);
#endif

#ifdef DEBUGGER_LOG_VERBOSE
// don't call this directly, use the DBG_TRACE macro instead
static void dbg_do_trace(char const *msg, ...);
#endif

static void dbg_state_transition(enum debug_state new_state);

static char const *cur_ctx_str(void);

static addr32_t dbg_get_pc(enum dbg_context_id id);

void debug_init(void) {
    memset(&dbg, 0, sizeof(dbg));

    dbg.contexts[DEBUG_CONTEXT_SH4].cur_state = DEBUG_STATE_NORM;
    dbg.contexts[DEBUG_CONTEXT_ARM7].cur_state = DEBUG_STATE_NORM;

    atomic_flag_test_and_set(&dbg.not_request_break);
    atomic_flag_test_and_set(&dbg.not_continue);
    atomic_flag_test_and_set(&dbg.not_detach);

    unsigned ctx_no;
    for (ctx_no = 0; ctx_no < NUM_DEBUG_CONTEXTS; ctx_no++)
        atomic_flag_test_and_set(&dbg.contexts[ctx_no].not_single_step);
}

void debug_cleanup(void) {
    frontend_on_cleanup();
}

static inline bool debug_is_at_watch(void) {
#ifdef ENABLE_WATCHPOINTS
    if (get_ctx()->cur_state == DEBUG_STATE_PRE_WATCH) {
        Sh4 *sh4 = dreamcast_get_cpu();
        printf("DEBUGGER: NOW ENTERING WATCHPOINT BREAK AT PC=0x%08x\n",
               (unsigned)sh4->reg[SH4_REG_PC]);
        if (get_ctx()->is_read_watchpoint)
            frontend_on_read_watchpoint(get_ctx()->watchpoint_addr);
        else
            frontend_on_write_watchpoint(get_ctx()->watchpoint_addr);
        dbg_state_transition(DEBUG_STATE_WATCH);
        dc_state_transition(DC_STATE_DEBUG, DC_STATE_RUNNING);
        return true;
    }
#endif
    return false;
}

static void debug_check_break(enum dbg_context_id id) {
    /*
     * clear the flag now, but don't actually check it until the end of the
     * function.  We want this to be the lowest-priority break-reason and we
     * also don't want it to linger around if we find some higher-priority
     * reason to stop
     */
    struct debug_context *ctx = dbg.contexts + id;
    bool user_break = !atomic_flag_test_and_set(&dbg.not_request_break);

    // hold at a breakpoint for user interaction
    if ((ctx->cur_state == DEBUG_STATE_BREAK) ||
        (ctx->cur_state == DEBUG_STATE_WATCH))
        return;

    if (ctx->cur_state == DEBUG_STATE_STEP) {
        dbg_state_transition(DEBUG_STATE_BREAK);
        frontend_on_break();
        dc_state_transition(DC_STATE_DEBUG, DC_STATE_RUNNING);
        return;
    }

    /*
     * transition out of post-watch state.
     *
     * After a transition out of DC_STATE_DEBUG, the main loop in dreamcast_run
     * will execute an instruction and it will not call this function until the
     * next instruciton after that is about to be called; therefore it is always
     * correct for this function to to transition to DEBUG_STATE_NORM when the
     * cur_state is DEBUG_STATE_POST_WATCH.
     *
     */
    if (ctx->cur_state == DEBUG_STATE_POST_WATCH) {
        /*
         * we intentionally do not return here because we still want to check
         * the breakpoints below.
         */
        dbg_state_transition(DEBUG_STATE_NORM);
    }

    if (debug_is_at_watch())
        return;

    reg32_t pc = dbg_get_pc(id);

    for (unsigned bp_idx = 0; bp_idx < DEBUG_N_BREAKPOINTS; bp_idx++) {
        if (ctx->breakpoints[bp_idx].enabled &&
            pc == ctx->breakpoints[bp_idx].addr) {
            frontend_on_break();
            dbg_state_transition(DEBUG_STATE_BREAK);
            dc_state_transition(DC_STATE_DEBUG, DC_STATE_RUNNING);
            return;
        }
    }

    if (user_break) {
        frontend_on_break();
        dbg_state_transition(DEBUG_STATE_BREAK);
        dc_state_transition(DC_STATE_DEBUG, DC_STATE_RUNNING);
    }
}

void debug_notify_inst(void) {
    debug_check_break(dbg.cur_ctx);
}

void debug_request_detach(void) {
    atomic_flag_clear(&dbg.not_detach);
}

int debug_add_break(enum dbg_context_id id, addr32_t addr) {
    struct debug_context *ctx = dbg.contexts + id;
    DBG_TRACE("request to add hardware breakpoint at 0x%08x\n",
        (unsigned)addr);
    for (unsigned idx = 0; idx < DEBUG_N_BREAKPOINTS; idx++)
        if (!ctx->breakpoints[idx].enabled) {
            ctx->breakpoints[idx].addr = addr;
            ctx->breakpoints[idx].enabled = true;
            return 0;
        }

    DBG_TRACE("unable to add hardware breakpoint at 0x%08x (there are already "
              "%u breakpoints)\n",
              (unsigned)addr, (unsigned)DEBUG_N_BREAKPOINTS);
    return ENOBUFS;
}

int debug_remove_break(enum dbg_context_id id, addr32_t addr) {
    struct debug_context *ctx = dbg.contexts + id;
    DBG_TRACE("request to remove hardware breakpoint at 0x%08x\n",
              (unsigned)addr);
    for (unsigned idx = 0; idx < DEBUG_N_BREAKPOINTS; idx++)
        if (ctx->breakpoints[idx].enabled &&
            ctx->breakpoints[idx].addr == addr) {
            ctx->breakpoints[idx].enabled = false;
            return 0;
        }

    DBG_TRACE("unable to remove hardware breakpoint at 0x%08x (it does not "
              "exist)\n", (unsigned)addr);
    return EINVAL;
}

// these functions return 0 on success, nonzero on failure
int debug_add_r_watch(enum dbg_context_id id, addr32_t addr, unsigned len) {
    struct debug_context *ctx = dbg.contexts + id;
    DBG_TRACE("request to add read-watchpoint at 0x%08x\n", (unsigned)addr);
    for (unsigned idx = 0; idx < DEBUG_N_R_WATCHPOINTS; idx++) {
        struct watchpoint *wp = ctx->r_watchpoints + idx;
        if (!wp->enabled) {
            wp->addr = addr;
            wp->len = len;
            wp->enabled = true;
            return 0;
        }
    }

    DBG_TRACE("unable to add read-watchpoint at 0x%08x (there are already %u "
              "read-watchpoints)\n",
              (unsigned)addr, (unsigned)DEBUG_N_R_WATCHPOINTS);
    return ENOBUFS;
}

int debug_remove_r_watch(enum dbg_context_id id, addr32_t addr, unsigned len) {
    struct debug_context *ctx = dbg.contexts + id;
    DBG_TRACE("request to remove read-watchpoint at 0x%08x\n", (unsigned)addr);
    for (unsigned idx = 0; idx < DEBUG_N_R_WATCHPOINTS; idx++) {
        struct watchpoint *wp = ctx->r_watchpoints + idx;
        if (wp->enabled && wp->addr == addr && wp->len == len) {
            wp->enabled = false;
            return 0;
        }
    }

    DBG_TRACE("unable to remove read-watchpoint at 0x%08x (it does not "
              "exist)\n", (unsigned)addr);
    return EINVAL;
}

// these functions return 0 on success, nonzer on failure
int debug_add_w_watch(enum dbg_context_id id, addr32_t addr, unsigned len) {
    struct debug_context *ctx = dbg.contexts + id;
    DBG_TRACE("request to add write-watchpoint at 0x%08x\n", (unsigned)addr);
    for (unsigned idx = 0; idx < DEBUG_N_W_WATCHPOINTS; idx++) {
        struct watchpoint *wp = ctx->w_watchpoints + idx;
        if (!wp->enabled) {
            wp->addr = addr;
            wp->len = len;
            wp->enabled = true;
            return 0;
        }
    }

    DBG_TRACE("unable to add write-watchpoint at 0x%08x (there are already %u "
              "read-watchpoints)\n",
              (unsigned)addr, (unsigned)DEBUG_N_W_WATCHPOINTS);
    return ENOBUFS;
}

int debug_remove_w_watch(enum dbg_context_id id, addr32_t addr, unsigned len) {
    struct debug_context *ctx = dbg.contexts + id;
    DBG_TRACE("request to remove write-watchpoint at 0x%08x\n", (unsigned)addr);
    for (unsigned idx = 0; idx < DEBUG_N_W_WATCHPOINTS; idx++) {
        struct watchpoint *wp = ctx->w_watchpoints + idx;
        if (wp->enabled && wp->addr == addr && wp->len == len) {
            wp->enabled = false;
            return 0;
        }
    }

    DBG_TRACE("unable to remove write-watchpoint at 0x%08x (it does not "
              "exist)\n", (unsigned)addr);
    return EINVAL;
}

bool debug_is_w_watch(addr32_t addr, unsigned len) {
    struct debug_context *ctx = get_ctx();

    if (ctx->cur_state != DEBUG_STATE_NORM)
        return false;

    addr32_t access_first = addr;
    addr32_t access_last = addr + (len - 1);

    for (unsigned idx = 0; idx < DEBUG_N_W_WATCHPOINTS; idx++) {
        struct watchpoint *wp = ctx->w_watchpoints + idx;
        if (wp->enabled) {
            addr32_t watch_first = wp->addr;
            addr32_t watch_last = watch_first + (wp->len - 1);
            if ((access_first >= watch_first && access_first <= watch_last) ||
                (access_last >= watch_first && access_last <= watch_last) ||
                (watch_first >= access_first && watch_first <= access_last) ||
                (watch_last >= access_first && watch_last <= access_last)) {
                dbg_state_transition(DEBUG_STATE_PRE_WATCH);
                ctx->watchpoint_addr = addr;
                ctx->is_read_watchpoint = false;
                printf("DEBUGGER: write-watchpoint at 0x%08x triggered "
                       "(PC=0x%08x, cur_ctx = %s)!\n",
                       (unsigned)addr, (unsigned)dbg_get_pc(dbg.cur_ctx),
                       cur_ctx_str());
                return true;
            }
        }
    }
    return false;
}

bool debug_is_r_watch(addr32_t addr, unsigned len) {
    struct debug_context *ctx = get_ctx();

    if (ctx->cur_state != DEBUG_STATE_NORM)
        return false;

    addr32_t access_first = addr;
    addr32_t access_last = addr + (len - 1);

    for (unsigned idx = 0; idx < DEBUG_N_R_WATCHPOINTS; idx++) {
        struct watchpoint *wp = ctx->w_watchpoints + idx;
        if (wp->enabled) {
            addr32_t watch_first = wp->addr;
            addr32_t watch_last = watch_first + (wp->len - 1);
            if ((access_first >= watch_first && access_first <= watch_last) ||
                (access_last >= watch_first && access_last <= watch_last) ||
                (watch_first >= access_first && watch_first <= access_last) ||
                (watch_last >= access_first && watch_last <= access_last)) {
                dbg_state_transition(DEBUG_STATE_PRE_WATCH);
                ctx->watchpoint_addr = addr;
                ctx->is_read_watchpoint = true;
                printf("DEBUGGER: read-watchpoint at 0x%08x triggered "
                       "(PC=0x%08x, cur_ctx = %s)!\n",
                       (unsigned)addr, (unsigned)dbg_get_pc(dbg.cur_ctx),
                       cur_ctx_str());
                return true;
            }
        }
    }
    return false;
}

void debug_on_softbreak(cpu_inst_param inst, addr32_t pc) {
    DBG_TRACE("softbreak at 0x%08x\n", (unsigned)pc);
    dbg_state_transition(DEBUG_STATE_BREAK);
    dc_state_transition(DC_STATE_DEBUG, DC_STATE_RUNNING);
    frontend_on_softbreak(inst, pc);
}

void debug_attach(struct debug_frontend const *frontend) {
    LOG_INFO("debugger attached\n");

    dbg.contexts[DEBUG_CONTEXT_SH4].cur_state = DEBUG_STATE_BREAK;

    dbg.frontend = frontend;
    frontend_attach();
    dbg_state_transition(DEBUG_STATE_BREAK);
    dc_state_transition(DC_STATE_DEBUG, DC_STATE_RUNNING);

    atomic_flag_test_and_set(&dbg.not_request_break);
    atomic_flag_test_and_set(&dbg.not_continue);
    atomic_flag_test_and_set(&dbg.not_detach);

    unsigned ctx_no;
    for (ctx_no = 0; ctx_no < NUM_DEBUG_CONTEXTS; ctx_no++)
        atomic_flag_test_and_set(&dbg.contexts[ctx_no].not_single_step);

    LOG_INFO("done attaching debugger\n");
}

static void frontend_attach(void) {
    if (dbg.frontend && dbg.frontend->attach)
        dbg.frontend->attach(dbg.frontend->arg);
}

static void frontend_run_once(void) {
    if (dbg.frontend && dbg.frontend->run_once)
        dbg.frontend->run_once(dbg.frontend->arg);
}

static void frontend_on_break(void) {
    if (dbg.frontend && dbg.frontend->on_break)
        dbg.frontend->on_break(dbg.cur_ctx, dbg.frontend->arg);
}

#ifdef ENABLE_WATCHPOINTS
static void frontend_on_read_watchpoint(addr32_t addr) {
    if (dbg.frontend && dbg.frontend->on_read_watchpoint)
        dbg.frontend->on_read_watchpoint(dbg.cur_ctx, addr, dbg.frontend->arg);
}

static void frontend_on_write_watchpoint(addr32_t addr) {
    if (dbg.frontend->on_write_watchpoint)
        dbg.frontend->on_write_watchpoint(dbg.cur_ctx, addr, dbg.frontend->arg);
}
#endif

static void frontend_on_softbreak(cpu_inst_param inst, addr32_t addr) {
    if (dbg.frontend->on_softbreak)
        dbg.frontend->on_softbreak(dbg.cur_ctx, inst, addr, dbg.frontend->arg);
}

static void frontend_on_cleanup(void) {
    if (dbg.frontend && dbg.frontend->on_cleanup)
        dbg.frontend->on_cleanup(dbg.frontend->arg);
}

unsigned debug_gen_reg_idx(enum dbg_context_id id, unsigned idx) {
    switch (dbg.cur_ctx) {
    case DEBUG_CONTEXT_SH4:
        return SH4_REG_R0 + idx;
        /*
         * It's okay to not implement this for ARM7 because only the gdb_stub
         * uses it.
         */
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

unsigned debug_bank0_reg_idx(enum dbg_context_id id, unsigned reg_sr, unsigned idx) {
    switch (id) {
    case DEBUG_CONTEXT_SH4:
        if (reg_sr & SH4_SR_RB_MASK)
            return SH4_REG_R0_BANK + idx;
        return SH4_REG_R0 + idx;
        /*
         * It's okay to not implement this for ARM7 because only the gdb_stub
         * uses it.
         */
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

unsigned debug_bank1_reg_idx(enum dbg_context_id id, unsigned reg_sr, unsigned idx) {
    switch (id) {
    case DEBUG_CONTEXT_SH4:
        if (reg_sr & SH4_SR_RB_MASK)
            return SH4_REG_R0 + idx;
        return SH4_REG_R0_BANK + idx;
        /*
         * It's okay to not implement this for ARM7 because only the gdb_stub
         * uses it.
         */
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

void debug_request_continue(void) {
    atomic_flag_clear(&dbg.not_continue);
}

void debug_request_single_step(void) {
    atomic_flag_clear(&get_ctx()->not_single_step);
}

void debug_request_break() {
    atomic_flag_clear(&dbg.not_request_break);
}

#ifdef DEBUGGER_LOG_VERBOSE
void dbg_do_trace(char const *msg, ...) {
    va_list var_args;
    va_start(var_args, msg);

    // TODO: get this to work with log.h
    printf("DEBUGGER: ");
    vprintf(msg, var_args);

    va_end(var_args);
}
#endif

#ifdef DEBUGGER_LOG_VERBOSE
static char const*
dbg_state_names[DEBUG_STATE_COUNT] = {
    "DEBUG_STATE_NORM",
    "DEBUG_STATE_STEP",
    "DEBUG_STATE_BREAK",
    "DEBUG_STATE_PRE_WATCH",
    "DEBUG_STATE_WATCH",
    "DEBUG_STATE_POST_WATCH"
};
#endif

static void dbg_state_transition(enum debug_state new_state) {
    struct debug_context *ctx = get_ctx();
    DBG_TRACE("state transition from %s to %s\n",
              dbg_state_names[ctx->cur_state], dbg_state_names[new_state]);
    ctx->cur_state = new_state;
}

static pthread_mutex_t debug_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t debug_cond = PTHREAD_COND_INITIALIZER;

void debug_lock(void) {
    if (pthread_mutex_lock(&debug_mutex) < 0)
        abort(); // TODO error handling
}

void debug_unlock(void) {
    if (pthread_mutex_unlock(&debug_mutex) < 0)
        abort(); // TODO error handling
}

void debug_signal(void) {
    if (pthread_cond_signal(&debug_cond) < 0)
        abort();
}

void debug_run_once(void) {
    frontend_run_once();

    struct debug_context *ctx = get_ctx();

    if ((ctx->cur_state != DEBUG_STATE_BREAK) &&
        (ctx->cur_state != DEBUG_STATE_WATCH)) {
        error_set_dbg_state(ctx->cur_state);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    if (!atomic_flag_test_and_set(&ctx->not_single_step)) {
        // gdb frontend requested a single-step via debug_request_single_step
        dbg_state_transition(DEBUG_STATE_STEP);
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_DEBUG);
    }

    if (!atomic_flag_test_and_set(&dbg.not_continue)) {
        if (ctx->cur_state == DEBUG_STATE_WATCH)
            dbg_state_transition(DEBUG_STATE_POST_WATCH);
        else
            dbg_state_transition(DEBUG_STATE_NORM);
        LOG_INFO("Transition to DC_STATE_RUNNING\n");
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_DEBUG);
    }

    if (!atomic_flag_test_and_set(&dbg.not_detach)) {
        DBG_TRACE("detach request\n");

        unsigned ctx_no;
        for (ctx_no = 0; ctx_no < NUM_DEBUG_CONTEXTS; ctx_no++) {
            struct debug_context *ctx = dbg.contexts + ctx_no;
            memset(ctx->breakpoints, 0, sizeof(ctx->breakpoints));
            memset(ctx->r_watchpoints, 0, sizeof(ctx->r_watchpoints));
            memset(ctx->w_watchpoints, 0, sizeof(ctx->w_watchpoints));
        }

        dbg_state_transition(DEBUG_STATE_NORM);
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_DEBUG);
    }
}

union reg_union {
    reg32_t sh4_reg_file[SH4_REGISTER_COUNT];
    reg32_t arm7_reg_file[ARM7_REGISTER_COUNT];
};

void debug_get_all_regs(enum dbg_context_id id,
                        void *reg_file_out, size_t n_bytes) {
    union reg_union reg;

    switch (id) {
    case DEBUG_CONTEXT_SH4:
        if (n_bytes != SH4_REGISTER_COUNT * sizeof(reg32_t))
            RAISE_ERROR(ERROR_INTEGRITY);
        sh4_get_regs(get_ctx()->cpu, reg.sh4_reg_file);
        memcpy(reg_file_out, reg.sh4_reg_file, n_bytes);
        break;
    case DEBUG_CONTEXT_ARM7:
        if (n_bytes != ARM7_REGISTER_COUNT * sizeof(reg32_t))
            RAISE_ERROR(ERROR_INTEGRITY);
        arm7_get_regs(get_ctx()->cpu, reg.arm7_reg_file);
        memcpy(reg_file_out, reg.arm7_reg_file, n_bytes);
        break;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

void debug_set_all_regs(enum dbg_context_id id, void const *reg_file_in,
                        size_t n_bytes) {
    DBG_TRACE("writing to all registers\n");

    reg32_t sh4_reg_file[SH4_REGISTER_COUNT];
    switch (id) {
    case DEBUG_CONTEXT_SH4:
        if (n_bytes != SH4_REGISTER_COUNT * sizeof(reg32_t))
            RAISE_ERROR(ERROR_INTEGRITY);
        memcpy(sh4_reg_file, reg_file_in, sizeof(sh4_reg_file));
        sh4_set_regs(dreamcast_get_cpu(), sh4_reg_file);
        break;
        /*
         * TODO: implement this for ARM7.
         * For now, WashDbg lacks a way to set reigsters and GdbStub only
         * supports SH4, so this doesn't matter.
         */
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

// this one's just a layer on top of get_all_regs
reg32_t debug_get_reg(enum dbg_context_id id, unsigned reg_no) {
    union reg_union reg;
    switch (id) {
    case DEBUG_CONTEXT_SH4:
        debug_get_all_regs(DEBUG_CONTEXT_SH4, reg.sh4_reg_file,
                           sizeof(reg.sh4_reg_file));
        return reg.sh4_reg_file[reg_no];
    case DEBUG_CONTEXT_ARM7:
        debug_get_all_regs(DEBUG_CONTEXT_ARM7, reg.arm7_reg_file,
                           sizeof(reg.arm7_reg_file));
        return reg.arm7_reg_file[reg_no];
        break;
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

void debug_set_reg(enum dbg_context_id id, unsigned reg_no, reg32_t val) {
    DBG_TRACE("setting register index %u to 0x%08x\n",
              (unsigned)reg_no, (unsigned)val);
    switch (id) {
    case DEBUG_CONTEXT_SH4:
        sh4_set_individual_reg(dreamcast_get_cpu(),
                               (unsigned)reg_no, (unsigned)val);
        break;
        /*
         * TODO: implement this for ARM7.
         * For now, WashDbg lacks a way to set reigsters and GdbStub only
         * supports SH4, so this doesn't matter.
         */
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

int debug_read_mem(enum dbg_context_id id, void *out,
                   addr32_t addr, unsigned len) {
    unsigned unit_len, n_units;
    struct debug_context *ctxt = dbg.contexts + id;
    struct memory_map *mmap = ctxt->map;

    DBG_TRACE("request to read %u bytes from %08x\n",
              (unsigned)len, (unsigned)addr);

    if (len % 4 == 0) {
        unit_len = 4;
        n_units = len / 4;
    } else if (len % 2 == 0) {
        unit_len = 2;
        n_units = len / 2;
    } else {
        unit_len = 1;
        n_units = len;
    }

    uint8_t *out_byte_ptr = out;

    int err;
    while (n_units) {
        switch (unit_len) {
        case 4:
            err = memory_map_try_read_32(mmap, addr, (uint32_t*)out_byte_ptr);
            break;
        case 2:
            err = memory_map_try_read_16(mmap, addr, (uint16_t*)out_byte_ptr);
            break;
        case 1:
            err = memory_map_try_read_8(mmap, addr, (uint8_t*)out_byte_ptr);
            break;
        }

        if (err != 0) {
            LOG_ERROR("Failed %u-byte read at 0x%08x\n", unit_len, addr);
            if (len != unit_len)
                LOG_ERROR("Past reads may not have failed.\n");
            return -1;
        }

        out_byte_ptr += unit_len;
        addr += unit_len;
        n_units--;
    }

    return 0;
}

int debug_write_mem(enum dbg_context_id id, void const *input,
                    addr32_t addr, unsigned len) {
    unsigned n_units;
    struct debug_context *ctxt = dbg.contexts + id;
    struct memory_map *mmap = ctxt->map;

    DBG_TRACE("request to write %u bytes to 0x%08x\n",
              (unsigned)len, (unsigned)addr);

    /*
     * Ideally none of the writes would go through if there's a
     * failure at any point down the line, but that's not the way I've
     * implemented this.
     */
    if (len % 4 == 0) {
        n_units = len / 4;
        uint32_t const *input_byte_ptr = input;
        while (n_units) {
            int err = memory_map_try_write_32(mmap, addr, *input_byte_ptr);
            if (err != 0) {
                LOG_ERROR("Failed %u-byte write at 0x%08x\n", len, addr);
                if (len != 4)
                    LOG_ERROR("Past writes may not have failed.\n");
                return -1;
            }
            input_byte_ptr++;
            addr += 4;
            n_units--;
        }
    } else if (len % 2 == 0) {
        n_units = len / 2;
        uint16_t const *input_byte_ptr = input;
        while (n_units) {
            int err = memory_map_try_write_16(mmap, addr, *input_byte_ptr);
            if (err != 0) {
                LOG_ERROR("Failed %u-byte write at 0x%08x\n", len, addr);
                if (len != 2)
                    LOG_ERROR("Past writes may not have failed.\n");
                return -1;
            }
            input_byte_ptr++;
            addr += 2;
            n_units--;
        }
    } else {
        n_units = len;
        uint8_t const *input_byte_ptr = input;
        while (n_units) {
            int err = memory_map_try_write_8(mmap, addr, *input_byte_ptr);
            if (err != 0) {
                LOG_ERROR("Failed %u-byte write at 0x%08x\n", len, addr);
                if (len != 1)
                    LOG_ERROR("Past writes may not have failed.\n");
                return -1;
            }
            input_byte_ptr++;
            addr++;
            n_units--;
        }
    }

    return 0;
}

void debug_init_context(enum dbg_context_id id, void *cpu,
                        struct memory_map *map) {
    memset(dbg.contexts + id, 0, sizeof(dbg.contexts[id]));

    if (id != DEBUG_CONTEXT_SH4 && id != DEBUG_CONTEXT_ARM7)
        RAISE_ERROR(ERROR_INTEGRITY);

    dbg.contexts[id].id = id;
    dbg.contexts[id].cpu = cpu;
    dbg.contexts[id].map = map;
}

void debug_set_context(enum dbg_context_id id) {
    dbg.cur_ctx = id;
}

enum dbg_context_id debug_current_context(void) {
    return dbg.cur_ctx;
}

static addr32_t dbg_get_pc(enum dbg_context_id id) {
    switch (id) {
    case DEBUG_CONTEXT_SH4:
        return ((struct Sh4*)dbg.contexts[id].cpu)->reg[SH4_REG_PC];
    case DEBUG_CONTEXT_ARM7:
        return ((struct arm7*)dbg.contexts[id].cpu)->reg[ARM7_REG_PC];
    default:
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

static char const *cur_ctx_str(void) {
    switch (dbg.cur_ctx) {
    case DEBUG_CONTEXT_SH4:
        return "sh4";
    case DEBUG_CONTEXT_ARM7:
        return "arm7";
    default:
        return "unknown";
    }
}

static struct debug_context *get_ctx(void) {
    return dbg.contexts + dbg.cur_ctx;
}

uint32_t debug_pc_next(enum dbg_context_id id) {
    switch (id) {
    case DEBUG_CONTEXT_SH4:
        return sh4_pc_next(dbg.contexts[DEBUG_CONTEXT_SH4].cpu);
    case DEBUG_CONTEXT_ARM7:
        return arm7_pc_next(dbg.contexts[DEBUG_CONTEXT_ARM7].cpu);
    default:
        RAISE_ERROR(ERROR_INTEGRITY);
    }
}

#ifdef ENABLE_DBG_COND

static struct dbg_condition conditions[N_DEBUG_CONDITIONS];

static bool
debug_eval_cond_mem_val_8(enum dbg_context_id ctx, struct dbg_condition *cond) {
    uint8_t val;
    struct dbg_cond_mem_val *cond_mem_val = &cond->status.cond_mem_val;

    if (debug_read_mem(cond->ctx, &val, cond_mem_val->addr,
                       cond_mem_val->size) != 0) {
        return false;
    }

    if (val != cond_mem_val->prev_val.val8) {
        cond_mem_val->prev_val.val8 = val;

        if (val == cond_mem_val->val.val8) {
            LOG_INFO("memory condition triggered\n");
            LOG_INFO("\tsize is 1 byte.\n");
            LOG_INFO("\taddr 0x%08x: 0x%02x -> 0x%02x\n",
                     (unsigned)cond_mem_val->addr,
                     (unsigned)cond_mem_val->prev_val.val8, (unsigned)val);
            LOG_INFO("\tcurrent ctx is %s\n", cur_ctx_str());
            frontend_on_break();
            dbg_state_transition(DEBUG_STATE_BREAK);
            dc_state_transition(DC_STATE_DEBUG, DC_STATE_RUNNING);
            return true;
        }
    }
    return false;
}

static bool
debug_eval_cond_mem_val_16(enum dbg_context_id ctx, struct dbg_condition *cond) {
    uint16_t val;
    struct dbg_cond_mem_val *cond_mem_val = &cond->status.cond_mem_val;

    if (debug_read_mem(cond->ctx, &val, cond_mem_val->addr,
                       cond_mem_val->size) != 0) {
        return false;
    }

    if (val != cond_mem_val->prev_val.val16) {
        cond_mem_val->prev_val.val16 = val;

        if (val == cond_mem_val->val.val16) {
            LOG_INFO("memory condition triggered\n");
            LOG_INFO("\tsize is 2 bytes.\n");
            LOG_INFO("\taddr 0x%08x: 0x%04x -> 0x%04x\n",
                     (unsigned)cond_mem_val->addr,
                     (unsigned)cond_mem_val->prev_val.val16, (unsigned)val);
            LOG_INFO("\tcurrent ctx is %s\n", cur_ctx_str());
            frontend_on_break();
            dbg_state_transition(DEBUG_STATE_BREAK);
            dc_state_transition(DC_STATE_DEBUG, DC_STATE_RUNNING);
            return true;
        }
    }
    return false;
}

static bool
debug_eval_cond_mem_val_32(enum dbg_context_id ctx, struct dbg_condition *cond) {
    uint32_t val;
    struct dbg_cond_mem_val *cond_mem_val = &cond->status.cond_mem_val;

    if (debug_read_mem(cond->ctx, &val, cond_mem_val->addr,
                       cond_mem_val->size) != 0) {
        return false;
    }

    if (val != cond_mem_val->prev_val.val32) {
        cond_mem_val->prev_val.val32 = val;

        if (val == cond_mem_val->val.val32) {
            LOG_INFO("memory condition triggered\n");
            LOG_INFO("\tsize is 4 bytes.\n");
            LOG_INFO("\taddr 0x%08x: 0x%08x -> 0x%08x\n",
                     (unsigned)cond_mem_val->addr,
                     (unsigned)cond_mem_val->prev_val.val32, (unsigned)val);
            LOG_INFO("\tcurrent ctx is %s\n", cur_ctx_str());
            frontend_on_break();
            dbg_state_transition(DEBUG_STATE_BREAK);
            dc_state_transition(DC_STATE_DEBUG, DC_STATE_RUNNING);
            return true;
        }
    }
    return false;
}

static bool debug_eval_cond(enum dbg_context_id ctx,
                            struct dbg_condition *cond) {
    uint32_t reg_val;
    switch (cond->cond_tp) {
    case DEBUG_CONDITION_REG_VAL:
        if (ctx != cond->ctx)
            return false;
        reg_val = debug_get_reg(ctx, cond->status.cond_reg_val.reg_no);
        if (reg_val == cond->status.cond_reg_val.reg_val &&
            reg_val != cond->status.cond_reg_val.prev_reg_val) {
            frontend_on_break();
            dbg_state_transition(DEBUG_STATE_BREAK);
            dc_state_transition(DC_STATE_DEBUG, DC_STATE_RUNNING);
        }
        cond->status.cond_reg_val.prev_reg_val = reg_val;
        return true;
    case DEBUG_CONDITION_MEM_VAL:
        if (cond->status.cond_mem_val.size == 1)
            return debug_eval_cond_mem_val_8(ctx, cond);
        else if (cond->status.cond_mem_val.size == 2)
            return debug_eval_cond_mem_val_16(ctx, cond);
        else if (cond->status.cond_mem_val.size == 4)
            return debug_eval_cond_mem_val_32(ctx, cond);
        else
            RAISE_ERROR(ERROR_INTEGRITY);
        break;
    case DEBUG_CONDITION_NONE:
        break;
    default:
        RAISE_ERROR(ERROR_INTEGRITY);
    }
    return false;
}

void debug_check_conditions(enum dbg_context_id ctx) {
    int idx;
    for (idx = 0; idx < N_DEBUG_CONDITIONS; idx++)
        if (debug_eval_cond(ctx, conditions + idx))
            return;
}

bool debug_reg_cond(enum dbg_context_id ctx, unsigned reg_no,
                    uint32_t reg_val) {
    int idx;
    for (idx = 0; idx < N_DEBUG_CONDITIONS; idx++) {
        struct dbg_condition *cond = conditions + idx;
        if (cond->cond_tp == DEBUG_CONDITION_NONE) {
            cond->cond_tp = DEBUG_CONDITION_REG_VAL;
            cond->ctx = ctx;
            cond->status.cond_reg_val.reg_no = reg_no;
            cond->status.cond_reg_val.reg_val = reg_val;
            cond->status.cond_reg_val.prev_reg_val = debug_get_reg(ctx, reg_no);
            return true;
        }
    }
    return false;
}

bool debug_mem_cond(enum dbg_context_id ctx, uint32_t addr,
                    uint32_t val, unsigned size) {
    union dbg_val prev_val, tgt_val;

    int err_val;

    switch (size) {
    case 1:
        err_val = debug_read_mem(ctx, &prev_val.val8, addr, 1);
        tgt_val.val8 = (uint8_t)(val & 0xff);
        break;
    case 2:
        err_val = debug_read_mem(ctx, &prev_val.val16, addr, 2);
        tgt_val.val16 = (uint16_t)(val & 0xffff);
        break;
    case 4:
        err_val = debug_read_mem(ctx, &prev_val.val32, addr, 4);
        tgt_val.val32 = val;
        break;
    default:
        return false;
    }

    if (err_val != 0)
        return false;

    int idx;
    for (idx = 0; idx < N_DEBUG_CONDITIONS; idx++) {
        struct dbg_condition *cond = conditions + idx;
        if (cond->cond_tp == DEBUG_CONDITION_NONE) {
            cond->cond_tp = DEBUG_CONDITION_MEM_VAL;
            cond->ctx = ctx;
            cond->status.cond_mem_val.addr = addr;
            cond->status.cond_mem_val.size = size;
            cond->status.cond_mem_val.val = tgt_val;
            cond->status.cond_mem_val.prev_val = prev_val;
            return true;
        }
    }
    return false;
}

#endif
