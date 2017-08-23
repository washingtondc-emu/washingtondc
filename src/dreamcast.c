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
#include "hw/maple/maple.h"
#include "hw/maple/maple_device.h"
#include "hw/maple/maple_controller.h"
#include "io/io_thread.h"
#include "io/serial_server.h"
#include "io/cmd_tcp.h"
#include "cmd/cons.h"
#include "cmd/cmd_thread.h"
#include "cmd/cmd.h"

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

#ifdef ENABLE_DEBUGGER
// this must be called before run or not at all
static void dreamcast_enable_debugger(void);

static void dreamcast_check_debugger(void);
#endif

// this must be called before run or not at all
static void dreamcast_enable_serial_server(void);

static void dreamcast_enable_cmd_tcp(void);

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

    clock_gettime(CLOCK_MONOTONIC, &start_time);

    /*
     * hardcode a controller plugged into the first port with no additional
     * maple devices attached.
     * TODO: don't hardcode this
     */
    struct maple_device *cont = maple_device_get(maple_addr_pack(0, 0));
    cont->sw = &maple_controller_switch_table;
    maple_device_init(cont);

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
    cmd_thread_kick();

    /*
     * if there's a cmd session attached, then hang here until the user enters
     * the begin-execution command.
     */
    while (is_running && (dc_get_state() == DC_STATE_NOT_RUNNING))
        usleep(1000 * 1000 / 10);

    while (is_running) {
#ifdef ENABLE_DEBUGGER
        dreamcast_check_debugger();
#endif

        /*
         * TODO:
         * maybe turn this into a dc_sched event so I don't have to check it as
         * often?  dc_get_state does turn into an atomic read...
         */
        enum dc_state cur_state = dc_get_state();
        if (unlikely(cur_state == DC_STATE_SUSPEND)) {
            cons_puts("Execution suspended.  To resume, enter "
                      "\"resume-execution\" into the CLI prompt.\n");
            cmd_thread_kick();
            do {
                /*
                 * TODO: sleep on a pthread condition or something instead of
                 * polling.
                 */
                usleep(1000 * 1000 / 10);
            } while ((cur_state = dc_get_state()) == DC_STATE_SUSPEND &&
                     is_running);
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
            cmd_thread_kick();

            /*
             * we do a continue here so that we have to check is_running again
             * and potentially exit the loop without executing any more
             * instructions.
             */
            continue;
        }

        /*
         * TODO: reconsider this placement.
         *
         * "Normal" serial port speed is approx 10kbaud, and the Dreamcast's
         * serial port can go up to approx 1mbaud.  Calling sh4_periodic here
         * will force it to (in the worst-case scenario) update on a per-hblank
         * basis, which is less accurate.
         *
         * Moving this to somewhere like sh4_check_interrupts would be way more
         * accurate, but IDK if I want to add an atomic test-and-set there...
         */
        sh4_periodic(&cpu);

#ifdef ENABLE_DEBUGGER
        /*
         * TODO: don't single-step if there's no
         * chance of us hitting a breakpoint
         */
        dc_single_step(&cpu);
#else
        SchedEvent *next_event = peek_event();

        /*
         * if, during the last big chunk of SH4 instructions, there was an
         * event pushed that predated what was originally the next event,
         * then we will have accidentally skipped over it.
         * In this case, we want to run that event immediately without
         * running the CPU
         */
        if (next_event) {
            if (dc_cycle_stamp_priv_ < next_event->when) {
                sh4_run_cycles(&cpu, next_event->when - dc_cycle_stamp_priv_);
            } else {
                pop_event();
                next_event->handler(next_event);
            }
        } else {
            /*
             * Hard to say what to do here.  Constantly checking to see if
             * a new event got pushed would be costly.  Instead I just run
             * the cpu a little, but not so much that I drastically overrun
             * anything that might get scheduled.  The number of cycles to
             * run here is arbitrary, but if it's too low then performance
             * will be negatively impacted and if it's too high then
             * accuracy will be negatively impacted.
             */
            sh4_run_cycles(&cpu, 16);
        }
#endif
    }

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
    maple_device_cleanup(cont);

    dc_print_perf_stats();

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
            debug_run_once();
            usleep(1000 * 1000 / 100);
        } while ((cur_state = dc_get_state()) == DC_STATE_DEBUG);
    }
}
#endif

/* executes a single instruction and maybe ticks the clock. */
static void dc_single_step(Sh4 *sh4) {
    inst_t inst;
    unsigned n_cycles;
    int exc_pending;
    InstOpcode const *op;

    sh4_fetch_inst(sh4, &inst, &op, &n_cycles);

    dc_cycle_stamp_t tgt_stamp = dc_cycle_stamp() + n_cycles;

    SchedEvent *next_event;
    while ((next_event = peek_event()) &&
           (next_event->when <= tgt_stamp)) {
        pop_event();
        dc_cycle_advance(next_event->when - dc_cycle_stamp());
        next_event->handler(next_event);
    }

    sh4_do_exec_inst(sh4, inst, op);

    dc_cycle_advance(tgt_stamp - dc_cycle_stamp());
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

    printf("%u SH4 CPU cycles executed\n", (unsigned)dc_cycle_stamp());

    double seconds = delta_time.tv_sec +
        ((double)delta_time.tv_nsec) / 1000000000.0;
    double hz = ((double)dc_cycle_stamp()) / seconds;
    double hz_ratio = hz / 200000000.0;

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
