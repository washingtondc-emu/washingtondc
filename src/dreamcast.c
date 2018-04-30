/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2018 snickerbockers
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

#include <errno.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "error.h"
#include "flash_memory.h"
#include "dc_sched.h"
#include "hw/pvr2/spg.h"
#include "MemoryMap.h"
#include "gfx/gfx.h"
#include "hw/aica/aica_rtc.h"
#include "hw/gdrom/gdrom.h"
#include "hw/maple/maple.h"
#include "hw/maple/maple_device.h"
#include "hw/maple/maple_controller.h"
#include "io/io_thread.h"
#include "io/serial_server.h"
#include "io/cmd_tcp.h"
#include "cmd/cons.h"
#include "cmd/cmd_sys.h"
#include "cmd/cmd.h"
#include "glfw/window.h"
#include "hw/pvr2/framebuffer.h"
#include "log.h"
#include "hw/sh4/sh4_read_inst.h"
#include "hw/pvr2/pvr2.h"
#include "hw/sys/sys_block.h"
#include "hw/aica/aica.h"
#include "hw/g1/g1.h"
#include "hw/g2/g2.h"
#include "jit/code_block.h"
#include "jit/jit_intp/code_block_intp.h"
#include "jit/code_cache.h"
#include "jit/jit.h"
#include "gfx/opengl/overlay.h"
#include "BiosFile.h"

#ifdef ENABLE_DEBUGGER
#include "io/gdb_stub.h"
#endif

#ifdef ENABLE_JIT_X86_64
#include "jit/x86_64/native_dispatch.h"
#endif

#include "dreamcast.h"

static Sh4 cpu;
struct Memory dc_mem;

static volatile bool is_running;
static volatile bool signal_exit_threads;

static bool using_debugger;

bool serial_server_in_use;

static struct timespec last_frame_realtime;
static dc_cycle_stamp_t last_frame_virttime;
static bool show_overlay;

enum TermReason {
    TERM_REASON_NORM,   // normal program exit
    TERM_REASON_SIGINT, // received SIGINT
    TERM_REASON_ERROR   // usually this means somebody threw a c++ exception
};

// this stores the reason the dreamcast suspended execution
enum TermReason term_reason = TERM_REASON_NORM;

static enum dc_state dc_state = DC_STATE_NOT_RUNNING;

static void dc_sigint_handler(int param);

static void *load_file(char const *path, long *len);

#ifndef ENABLE_DEBUGGER
// Run until the next scheduled event (in dc_sched) should occur
static void dc_run_to_next_event(Sh4 *sh4);
static void dc_run_to_next_event_jit(Sh4 *sh4);

#ifdef ENABLE_JIT_X86_64
static void dc_run_to_next_event_jit_native(Sh4 *sh4);
#endif

#endif

#ifdef ENABLE_DEBUGGER
// this must be called before run or not at all
static void dreamcast_enable_debugger(void);

static void dreamcast_check_debugger(void);
#endif

// this must be called before run or not at all
static void dreamcast_enable_serial_server(void);

static void dreamcast_enable_cmd_tcp(void);

/*
 * XXX this used to be (SCHED_FREQUENCY / 10).  Now it's (SCHED_FREQUENCY / 100)
 * because programs that use the serial port (like KallistiOS) can timeout if
 * the serial port takes too long to reply.
 *
 * If the serial port is ever removed from the periodic event handler, this
 * should be increased back to (SCHED_FREQUENCY / 10) to save on host CPU
 * cycles.
 */
#define DC_PERIODIC_EVENT_PERIOD (SCHED_FREQUENCY / 100)

static void periodic_event_handler(struct SchedEvent *event);
static struct SchedEvent periodic_event;

/*
 * this counts virtual time.  The frequency of this clock is SCHED_FREQUENCY
 * (see dc_sched.h)
 */
static dc_cycle_stamp_t dc_cycle_stamp_priv_;
static dc_cycle_stamp_t *cycle_stamp_ptr = &dc_cycle_stamp_priv_;

dc_cycle_stamp_t dc_cycle_stamp() {
    return *cycle_stamp_ptr;
}

void dc_cycle_stamp_set(dc_cycle_stamp_t new_val) {
    *cycle_stamp_ptr = new_val;
}

void dc_set_cycle_stamp_pointer(dc_cycle_stamp_t *ptr) {
    if (!ptr)
        ptr = &dc_cycle_stamp_priv_;

    *ptr = *cycle_stamp_ptr;
    cycle_stamp_ptr = ptr;
}

void dreamcast_init(bool cmd_session) {
    is_running = true;

    memory_init(&dc_mem);
    flash_mem_load(config_get_dc_flash_path());
    bios_file_init(config_get_dc_bios_path());

    int boot_mode = config_get_boot_mode();
    if (boot_mode == (int)DC_BOOT_IP_BIN || boot_mode == (int)DC_BOOT_DIRECT) {
        long len_ip_bin;
        char const *ip_bin_path = config_get_ip_bin_path();
        void *dat_ip_bin = load_file(ip_bin_path, &len_ip_bin);
        if (!dat_ip_bin) {
            error_set_file_path(ip_bin_path);
            error_set_errno_val(errno);
            RAISE_ERROR(ERROR_FILE_IO);
        }

        memory_write(&dc_mem, dat_ip_bin, ADDR_IP_BIN & ADDR_AREA3_MASK,
                     len_ip_bin);
        free(dat_ip_bin);

        long len_1st_read_bin;
        char const *exec_bin_path = config_get_exec_bin_path();
        void *dat_1st_read_bin = load_file(exec_bin_path, &len_1st_read_bin);
        if (!dat_1st_read_bin) {
            error_set_file_path(exec_bin_path);
            error_set_errno_val(errno);
            RAISE_ERROR(ERROR_FILE_IO);
        }
        memory_write(&dc_mem, dat_1st_read_bin,
                     ADDR_1ST_READ_BIN & ADDR_AREA3_MASK, len_1st_read_bin);
        free(dat_1st_read_bin);

        char const *syscall_path = config_get_syscall_path();
        long syscall_len;
        void *dat_syscall = load_file(syscall_path, &syscall_len);

        if (!dat_syscall) {
            error_set_file_path(syscall_path);
            error_set_errno_val(errno);
            RAISE_ERROR(ERROR_FILE_IO);
        }

        if (syscall_len != LEN_SYSCALLS) {
            error_set_length(syscall_len);
            error_set_expected_length(LEN_SYSCALLS);
            RAISE_ERROR(ERROR_INVALID_FILE_LEN);
        }

        memory_write(&dc_mem, dat_syscall,
                     ADDR_SYSCALLS & ADDR_AREA3_MASK, syscall_len);
        free(dat_syscall);
    }

    sh4_init(&cpu);
    jit_init();
    sys_block_init();
    g1_init();
    g2_init();
    aica_init();
    pvr2_init();
    gdrom_init();
    maple_init();

    /* set the PC to the booststrap code within IP.BIN */
    if (boot_mode == (int)DC_BOOT_DIRECT)
        cpu.reg[SH4_REG_PC] = ADDR_1ST_READ_BIN;
    else if (boot_mode == (int)DC_BOOT_IP_BIN)
        cpu.reg[SH4_REG_PC] = ADDR_BOOTSTRAP;

    if (boot_mode == (int)DC_BOOT_IP_BIN || boot_mode == (int)DC_BOOT_DIRECT) {
        /*
         * set the VBR to what it would have been after a BIOS boot.
         * This was obtained empirically on a real Dreamcast.
         *
         * XXX not sure if there should be a different value depending on
         * whether or not we skip IP.BIN.  All I do know is that this value is
         * correct when we do skip IP.BIN because I obtained it by running a
         * homebrew that prints the VBR value when it starts, which would be
         * immediately after IP.BIN is run.  It is possible that there's a
         * different value immediately before IP.BIN runs, and that the value
         * seen by 1ST_READ.BIN is set by IP.BIN.
         */
        cpu.reg[SH4_REG_VBR] = 0x8c00f400;
    }

    aica_rtc_init();

    if (cmd_session) {
#ifdef ENABLE_DEBUGGER
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_NOT_RUNNING);
#endif
        /*
         * if there's no debugging support and we have a remote cmd session
         * attached, then leave the system in DC_STATE_NOT_RUNNING until the
         * user executes the begin-execution command.
         */
    } else {
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_NOT_RUNNING);
    }
}

void dreamcast_cleanup() {
    maple_cleanup();
    gdrom_cleanup();
    pvr2_cleanup();
    aica_cleanup();
    g2_cleanup();
    g1_cleanup();
    sys_block_cleanup();

#ifdef ENABLE_DEBUGGER
    debug_cleanup();
#endif

    jit_cleanup();
    sh4_cleanup(&cpu);
    bios_file_cleanup();
    memory_cleanup(&dc_mem);
}

/*
 * this is used to store the irl timestamp right before execution begins.
 * This exists for performance profiling purposes only.
 */
static struct timespec start_time;

static void main_loop_jit(void) {
    while (is_running) {
#ifdef ENABLE_DEBUGGER
        dreamcast_check_debugger();

        /*
         * TODO: don't single-step if there's no
         * chance of us hitting a breakpoint
         */
        dc_single_step(&cpu);
#else
        dc_run_to_next_event_jit(&cpu);
        code_cache_gc();
        struct SchedEvent *next_event = pop_event();
        if (next_event)
            next_event->handler(next_event);
#endif
    }
}

#ifdef ENABLE_JIT_X86_64
static void main_loop_jit_native(void) {
    while (is_running) {
#ifdef ENABLE_DEBUGGER
        dreamcast_check_debugger();

        /*
         * TODO: don't single-step if there's no
         * chance of us hitting a breakpoint
         */
        dc_single_step(&cpu);
#else
        dc_run_to_next_event_jit_native(&cpu);
        code_cache_gc();
        struct SchedEvent *next_event = pop_event();
        if (next_event)
            next_event->handler(next_event);
#endif
    }
}
#endif

static void main_loop_interpreter(void) {
    while (is_running) {
#ifdef ENABLE_DEBUGGER
        dreamcast_check_debugger();

        /*
         * TODO: don't single-step if there's no
         * chance of us hitting a breakpoint
         */
        dc_single_step(&cpu);
#else
        dc_run_to_next_event(&cpu);
        struct SchedEvent *next_event = pop_event();
        if (next_event)
            next_event->handler(next_event);
#endif
    }
}

void dreamcast_run() {
    signal(SIGINT, dc_sigint_handler);

    if (config_get_ser_srv_enable())
        dreamcast_enable_serial_server();

    if (config_get_enable_cmd_tcp())
        dreamcast_enable_cmd_tcp();

#ifdef ENABLE_DEBUGGER
    debug_init();
    if (config_get_dbg_enable())
        dreamcast_enable_debugger();
#endif

    cmd_print_banner();
    cmd_run_once();

    periodic_event.when = dc_cycle_stamp() + DC_PERIODIC_EVENT_PERIOD;
    periodic_event.handler = periodic_event_handler;
    sched_event(&periodic_event);

    /*
     * if there's a cmd session attached, then hang here until the user enters
     * the begin-execution command.
     */
    while (is_running && (dc_get_state() == DC_STATE_NOT_RUNNING)) {
        usleep(1000 * 1000 / 10);
        cmd_run_once();
    }

    clock_gettime(CLOCK_MONOTONIC, &start_time);

#ifndef ENABLE_DEBUGGER

#ifdef ENABLE_JIT_X86_64
    bool const jit = config_get_jit() || config_get_native_jit();
#else
    bool const jit = config_get_jit();
#endif

#else
    bool const jit = !config_get_dbg_enable() &&
        (config_get_jit() || config_get_native_jit());
#endif

#ifdef ENABLE_JIT_X86_64
    bool const native_mode = config_get_native_jit();
#endif

    clock_gettime(CLOCK_MONOTONIC, &last_frame_realtime);
    overlay_show(show_overlay);

    if (jit) {
#ifdef ENABLE_JIT_X86_64
        if (native_mode)
            main_loop_jit_native();
        else
#endif
            main_loop_jit();
    } else {
        main_loop_interpreter();
    }

    dc_print_perf_stats();

    // tell the other threads it's time to clean up and exit
    signal_exit_threads = true;

    // kick the io_thread so it knows to check dc_is_running
    io_thread_kick();

    switch (term_reason) {
    case TERM_REASON_NORM:
        LOG_INFO("program execution ended normally\n");
        break;
    case TERM_REASON_ERROR:
        LOG_INFO("program execution ended due to an unrecoverable error\n");
        break;
    case TERM_REASON_SIGINT:
        LOG_INFO("program execution ended due to user-initiated interruption\n");
        break;
    default:
        LOG_INFO("program execution ended for unknown reasons\n");
        break;
    }

    dreamcast_cleanup();
}

#ifdef ENABLE_DEBUGGER
static void dreamcast_check_debugger(void) {
    /*
     * If the debugger is enabled, make sure we have its permission to
     * single-step; if we don't then  block until something interresting
     * happens, and then skip the rest of the loop.
     */
    debug_notify_inst(&cpu);

    enum dc_state cur_state = dc_get_state();
    if (unlikely(cur_state == DC_STATE_DEBUG)) {
        do {
            // call debug_run_once 100 times per second
            win_check_events();
            debug_run_once();
            usleep(1000 * 1000 / 100);
        } while ((cur_state = dc_get_state()) == DC_STATE_DEBUG);
    }
}
#endif

#ifndef ENABLE_DEBUGGER
static void dc_run_to_next_event(Sh4 *sh4) {
    inst_t inst;
    InstOpcode const *op;
    unsigned inst_cycles;
    dc_cycle_stamp_t tgt_stamp = sched_target_stamp();

    while (tgt_stamp > dc_cycle_stamp()) {
        inst = sh4_read_inst(sh4);
        op = sh4_decode_inst(inst);
        inst_cycles = sh4_count_inst_cycles(op, &sh4->last_inst_type);

        /*
         * Advance the cycle counter based on how many cycles this instruction
         * will take.  If this would take us past the target stamp, that means
         * the next event should occur while this instruction is executing.
         * Instead of trying to implement that, I execute the instruction
         * without advancing the cycle count beyond dc_sched_target_stamp.  This
         * way, the CPU may appear to be a little faster than it should be from
         * a guest program's perspective, but the passage of time will still be
         * consistent.
         */
        dc_cycle_stamp_t cycles_after = dc_cycle_stamp() +
            inst_cycles * SH4_CLOCK_SCALE;

        tgt_stamp = sched_target_stamp();
        if (cycles_after > tgt_stamp)
            cycles_after = tgt_stamp;

        sh4_do_exec_inst(sh4, inst, op);

        /*
         * advance the cycles, being careful not to skip over any new events
         * which may have been added
         */
        tgt_stamp = sched_target_stamp();
        if (cycles_after > tgt_stamp)
            cycles_after = tgt_stamp;
        dc_cycle_stamp_set(cycles_after);
    }
}

#ifdef ENABLE_JIT_X86_64
static void dc_run_to_next_event_jit_native(Sh4 *sh4) {
    reg32_t newpc = sh4->reg[SH4_REG_PC];

    newpc = native_dispatch_entry(newpc);

    sh4->reg[SH4_REG_PC] = newpc;
}
#endif

static void dc_run_to_next_event_jit(Sh4 *sh4) {
    reg32_t newpc = sh4->reg[SH4_REG_PC];
    dc_cycle_stamp_t tgt_stamp = sched_target_stamp();

    while (tgt_stamp > dc_cycle_stamp()) {
        addr32_t blk_addr = newpc;
        struct cache_entry *ent = code_cache_find(blk_addr);

        struct code_block_intp *blk = &ent->blk.intp;
        if (!ent->valid) {
            jit_compile_intp(blk, blk_addr);
            ent->valid = true;
        }

        newpc = code_block_intp_exec(blk);

        dc_cycle_stamp_t cycles_after = dc_cycle_stamp() +
            blk->cycle_count;
        dc_cycle_stamp_set(cycles_after);
        tgt_stamp = sched_target_stamp();
    }
    if (dc_cycle_stamp() > tgt_stamp)
        dc_cycle_stamp_set(tgt_stamp);

    sh4->reg[SH4_REG_PC] = newpc;
}

#else

/* executes a single instruction and maybe ticks the clock. */
void dc_single_step(Sh4 *sh4) {
    inst_t inst = sh4_read_inst(sh4);
    InstOpcode const *op = sh4_decode_inst(inst);
    unsigned n_cycles = sh4_count_inst_cycles(op, &sh4->last_inst_type);

    /*
     * Advance the cycle counter based on how many cycles this instruction
     * will take.  If this would take us past the target stamp, that means
     * the next event should occur while this instruction is executing.
     * Instead of trying to implement that, I execute the instruction
     * without advancing the cycle count beyond dc_sched_target_stamp.  This
     * way, the CPU may appear to be a little faster than it should be from
     * a guest program's perspective, but the passage of time will still be
     * consistent.
     */
    dc_cycle_stamp_t cycles_after = dc_cycle_stamp() +
        n_cycles * SH4_CLOCK_SCALE;
    dc_cycle_stamp_t tgt_stamp = sched_target_stamp();
    if (cycles_after > tgt_stamp)
        cycles_after = tgt_stamp;

    sh4_do_exec_inst(sh4, inst, op);

    // now execute any events which would have happened during that instruction
    SchedEvent *next_event;
    while ((next_event = peek_event()) &&
           (next_event->when <= cycles_after)) {
        pop_event();
        dc_cycle_stamp_set(next_event->when);
        next_event->handler(next_event);
    }

    dc_cycle_stamp_set(cycles_after);
}

#endif

static void time_diff(struct timespec *delta,
                      struct timespec const *end,
                      struct timespec const *start) {
    /* subtract delta_time = end_time - start_time */
    if (end->tv_nsec < start->tv_nsec) {
        delta->tv_nsec = 1000000000 - start->tv_nsec + end->tv_nsec;
        delta->tv_sec = end->tv_sec - 1 - start->tv_sec;
    } else {
        delta->tv_nsec = end->tv_nsec - start->tv_nsec;
        delta->tv_sec = end->tv_sec - start->tv_sec;
    }
}

void dc_print_perf_stats(void) {
    struct timespec end_time, delta_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    time_diff(&delta_time, &end_time, &start_time);

    LOG_INFO("Total elapsed time: %u seconds and %u nanoseconds\n",
             (unsigned)delta_time.tv_sec, (unsigned)delta_time.tv_nsec);

    LOG_INFO("%u SH4 CPU cycles executed\n",
             (unsigned)sh4_get_cycles());

    double seconds = delta_time.tv_sec +
        ((double)delta_time.tv_nsec) / 1000000000.0;
    double hz = (double)sh4_get_cycles() / seconds;
    double hz_ratio = hz / (double)(200 * 1000 * 1000);

    LOG_INFO("Performance is %f MHz (%f%%)\n", hz / 1000000.0, hz_ratio * 100.0);
}

void dreamcast_kill(void) {
    LOG_INFO("%s called - WashingtonDC will exit soon\n", __func__);
    is_running = false;
}

Sh4 *dreamcast_get_cpu() {
    return &cpu;
}

#ifdef ENABLE_DEBUGGER
static void dreamcast_enable_debugger(void) {
    using_debugger = true;
    debug_attach(&gdb_frontend);
}
#endif

static void dreamcast_enable_serial_server(void) {
    serial_server_in_use = true;
    serial_server_attach();
    sh4_scif_connect_server(&cpu);
}

void dreamcast_enable_cmd_tcp(void) {
    cmd_tcp_attach();
}

static void dc_sigint_handler(int param) {
    is_running = false;
    term_reason = TERM_REASON_SIGINT;
}

static void *load_file(char const *path, long *len) {
    FILE *fp = fopen(path, "rb");
    long file_sz;
    void *dat = NULL;

    if (!fp)
        return NULL;

    if (fseek(fp, 0, SEEK_END) < 0)
        goto close_fp;
    if ((file_sz = ftell(fp)) < 0)
        goto close_fp;
    if (fseek(fp, 0, SEEK_SET) < 0)
        goto close_fp;

    dat = malloc(file_sz);
    if (fread(dat, file_sz, 1, fp) != 1)
        goto free_dat;

    *len = file_sz;

    // success
    goto close_fp;

free_dat:
    free(dat);
    dat = NULL;
close_fp:
    fclose(fp);
    return dat;
}

bool dc_is_running(void) {
    return !signal_exit_threads;
}

bool dc_emu_thread_is_running(void) {
    return is_running;
}

enum dc_state dc_get_state(void) {
    return dc_state;
}

void dc_state_transition(enum dc_state state_new, enum dc_state state_old) {
    if (state_old != dc_state)
        RAISE_ERROR(ERROR_INTEGRITY);
    dc_state = state_new;
}

bool dc_debugger_enabled(void) {
    return using_debugger;
}

/*
 * the purpose of this handler is to perform processing that needs to happen
 * occasionally but has no hard timing requirements.  The timing of this event
 * is *technically* deterministic, but users should not assume any determinism
 * because the frequency of this event is subject to change.
 */
static void periodic_event_handler(struct SchedEvent *event) {
    enum dc_state cur_state = dc_get_state();
    if (unlikely(cur_state == DC_STATE_SUSPEND)) {
        cons_puts("Execution suspended.  To resume, enter "
                  "\"resume-execution\" into the CLI prompt.\n");
        do {
            win_check_events();
            cmd_run_once();
            /*
             * TODO: sleep on a pthread condition or something instead of
             * polling.
             */
            usleep(1000 * 1000 / 60);
        } while (is_running &&
                 ((cur_state = dc_get_state()) == DC_STATE_SUSPEND));

        if (dc_is_running()) {
            cons_puts("execution resumed\n");
        } else {
            /*
             * TODO: this message doesn't actually get printed.  The likely
             * cause is that the cmd thread does not have time to print it.
             * it may be worthwile to drain all output before the cmd
             * thread exits, but I'd also have to be careful not to spend
             * too long waiting on an ack from an external system...
             */
            cons_puts("responding to request to exit\n");
        }
    }

    sh4_periodic(&cpu);

    periodic_event.when = dc_cycle_stamp() + DC_PERIODIC_EVENT_PERIOD;
    sched_event(&periodic_event);
}

void dc_end_frame(void) {
    struct timespec timestamp, delta;
    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    dc_cycle_stamp_t virt_timestamp = dc_cycle_stamp();

    time_diff(&delta, &timestamp, &last_frame_realtime);

    double framerate = 1.0 / (delta.tv_sec + delta.tv_nsec / 1000000000.0);
    double virt_framerate = (double)SCHED_FREQUENCY /
        (double)(virt_timestamp - last_frame_virttime);
    last_frame_realtime = timestamp;
    last_frame_virttime = virt_timestamp;
    overlay_set_fps(framerate);
    overlay_set_virt_fps(virt_framerate);

    framebuffer_render();
    win_check_events();
    cmd_run_once();
}

void dc_toggle_overlay(void) {
    show_overlay = !show_overlay;
    overlay_show(show_overlay);
}
