/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016, 2017 snickerbockers
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
#include <stdatomic.h>
#include <unistd.h>

#include "config.h"
#include "error.h"
#include "flash_memory.h"
#include "dc_sched.h"
#include "hw/pvr2/spg.h"
#include "MemoryMap.h"
#include "gfx/gfx_thread.h"
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

#ifdef ENABLE_DEBUGGER
#include "io/gdb_stub.h"
#endif

#include "dreamcast.h"

static Sh4 cpu;
static BiosFile bios;
static struct Memory mem;

static volatile bool is_running;
static volatile bool signal_exit_threads;

static bool using_debugger;

bool serial_server_in_use;

dc_cycle_stamp_t dc_cycle_stamp_priv_;

enum TermReason {
    TERM_REASON_NORM,   // normal program exit
    TERM_REASON_SIGINT, // received SIGINT
    TERM_REASON_ERROR   // usually this means somebody threw a c++ exception
};

// this stores the reason the dreamcast suspended execution
enum TermReason term_reason = TERM_REASON_NORM;

static atomic_int dc_state = ATOMIC_VAR_INIT(DC_STATE_NOT_RUNNING);

static void dc_sigint_handler(int param);

static void *load_file(char const *path, long *len);

static void dc_single_step(Sh4 *sh4);

// Run until the next scheduled event (in dc_sched) should occur
static void dc_run_to_next_event(Sh4 *sh4);

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

void dreamcast_init(bool cmd_session) {
    is_running = true;

    memory_init(&mem);
    flash_mem_load(config_get_dc_flash_path());
    bios_file_init(&bios, config_get_dc_bios_path());
    memory_map_init(&bios, &mem);

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
        memory_map_write(dat_ip_bin, ADDR_IP_BIN & ~0xe0000000, len_ip_bin);
        free(dat_ip_bin);

        long len_1st_read_bin;
        char const *exec_bin_path = config_get_exec_bin_path();
        void *dat_1st_read_bin = load_file(exec_bin_path, &len_1st_read_bin);
        if (!dat_1st_read_bin) {
            error_set_file_path(exec_bin_path);
            error_set_errno_val(errno);
            RAISE_ERROR(ERROR_FILE_IO);
        }
        memory_map_write(dat_1st_read_bin, ADDR_1ST_READ_BIN & ~0xe0000000,
                         len_1st_read_bin);
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

        memory_map_write(dat_syscall, ADDR_SYSCALLS & ~0xe0000000, syscall_len);
        free(dat_syscall);
    }

    sh4_init(&cpu);
    spg_init();
    gdrom_init();

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
    spg_cleanup();

#ifdef ENABLE_DEBUGGER
    debug_cleanup();
#endif

    sh4_cleanup(&cpu);
    bios_file_cleanup(&bios);
    memory_cleanup(&mem);
}

/*
 * this is used to store the irl timestamp right before execution begins.
 * This exists for performance profiling purposes only.
 */
static struct timespec start_time;

void dreamcast_run() {
    signal(SIGINT, dc_sigint_handler);

    /*
     * hardcode a controller plugged into the first port with no additional
     * maple devices attached.
     * TODO: don't hardcode this
     */
    maple_device_init(maple_addr_pack(0, 0), MAPLE_DEVICE_CONTROLLER);

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

    dc_print_perf_stats();

    // tell the other threads it's time to clean up and exit
    signal_exit_threads = true;

    // kick the gfx_thread and io_thread so they know to check dc_is_running
    gfx_thread_notify_wake_up();
    io_thread_kick();

    switch (term_reason) {
    case TERM_REASON_NORM:
        printf("program execution ended normally\n");
        break;
    case TERM_REASON_ERROR:
        printf("program execution ended due to an unrecoverable error\n");
        break;
    case TERM_REASON_SIGINT:
        printf("program execution ended due to user-initiated interruption\n");
        break;
    default:
        printf("program execution ended for unknown reasons\n");
        break;
    }

    // TODO: don't hardcode this
    maple_device_cleanup(maple_addr_pack(0, 0));

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

static void dc_run_to_next_event(Sh4 *sh4) {
    inst_t inst;
    int exc_pending;
    InstOpcode const *op;
    unsigned inst_cycles;

mulligan:
    while (dc_sched_target_stamp > dc_cycle_stamp()) {
        sh4_fetch_inst(sh4, &inst, &op, &inst_cycles);

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
        if (cycles_after > dc_sched_target_stamp)
            cycles_after = dc_sched_target_stamp;

        sh4_do_exec_inst(sh4, inst, op);

        /*
         * advance the cycles, being careful not to skip over any new events
         * which may have been added
         */
        if (cycles_after > dc_sched_target_stamp)
            cycles_after = dc_sched_target_stamp;
        dc_cycle_advance(cycles_after - dc_cycle_stamp());
    }
}

/* executes a single instruction and maybe ticks the clock. */
static void dc_single_step(Sh4 *sh4) {
    inst_t inst;
    unsigned n_cycles;
    int exc_pending;
    InstOpcode const *op;

    sh4_fetch_inst(sh4, &inst, &op, &n_cycles);

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
    if (cycles_after > dc_sched_target_stamp)
        cycles_after = dc_sched_target_stamp;

    sh4_do_exec_inst(sh4, inst, op);

    // now execute any events which would have happened during that instruction
    SchedEvent *next_event;
    while ((next_event = peek_event()) &&
           (next_event->when <= cycles_after)) {
        pop_event();
        dc_cycle_advance(next_event->when - dc_cycle_stamp());
        next_event->handler(next_event);
    }

    dc_cycle_advance(cycles_after - dc_cycle_stamp());
}

void dc_print_perf_stats(void) {
    struct timespec end_time, delta_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    /* subtract delta_time = end_time - start_time */
    if (end_time.tv_nsec < start_time.tv_nsec) {
        delta_time.tv_nsec = 1000000000 - start_time.tv_nsec + end_time.tv_nsec;
        delta_time.tv_sec = end_time.tv_sec - 1 - start_time.tv_sec;
    } else {
        delta_time.tv_nsec = end_time.tv_nsec - start_time.tv_nsec;
        delta_time.tv_sec = end_time.tv_sec - start_time.tv_sec;
    }

    printf("Total elapsed time: %u seconds and %u nanoseconds\n",
           (unsigned)delta_time.tv_sec, (unsigned)delta_time.tv_nsec);

    printf("%u SH4 CPU cycles executed\n",
           (unsigned)sh4_get_cycles());

    double seconds = delta_time.tv_sec +
        ((double)delta_time.tv_nsec) / 1000000000.0;
    double hz = (double)sh4_get_cycles() / seconds;
    double hz_ratio = hz / (double)(200 * 1000 * 1000);

    printf("Performance is %f MHz (%f%%)\n", hz / 1000000.0, hz_ratio * 100.0);
}

void dreamcast_kill(void) {
    printf("%s called - WashingtonDC will exit soon\n", __func__);
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
    return (enum dc_state)atomic_load(&dc_state);
}

void dc_state_transition(enum dc_state state_new, enum dc_state state_old) {
    int expected = (int)state_old;

    if (!atomic_compare_exchange_strong(&dc_state, &expected, (int)state_new))
        RAISE_ERROR(ERROR_INTEGRITY);
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
    win_check_events();
    cmd_run_once();
}
