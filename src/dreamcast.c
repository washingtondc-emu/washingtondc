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

#ifdef ENABLE_DEBUGGER
#include "gdb_stub.h"
#endif

#ifdef ENABLE_SERIAL_SERVER
#include "serial_server.h"
#endif

#include "dreamcast.h"

static Sh4 cpu;
static BiosFile bios;
static struct Memory mem;

static volatile bool is_running;

#ifdef ENABLE_DEBUGGER
static struct debugger debugger;
static struct gdb_stub gdb_stub;
static bool using_debugger;
#endif

#ifdef ENABLE_SERIAL_SERVER
static struct serial_server serial_server;
bool serial_server_in_use;
#endif

#if defined ENABLE_DEBUGGER || defined ENABLE_SERIAL_SERVER
// Used by the GdbStub and serial_server (if enabled) to do async network I/O
struct event_base *dc_event_base;
#endif

dc_cycle_stamp_t dc_cycle_stamp_priv_;

enum TermReason {
    TERM_REASON_NORM,   // normal program exit
    TERM_REASON_SIGINT, // received SIGINT
    TERM_REASON_ERROR   // usually this means somebody threw a c++ exception
};

// this stores the reason the dreamcast suspended execution
enum TermReason term_reason = TERM_REASON_NORM;

static void dc_sigint_handler(int param);

static void *load_file(char const *path, long *len);

static void dc_single_step(Sh4 *sh4);

void dreamcast_init(char const *bios_path, char const *flash_path) {
    is_running = true;

// #ifdef ENABLE_DEBUGGER
//     debugger = NULL;
// #endif

#if defined(ENABLE_DEBUGGER) || defined(ENABLE_SERIAL_SERVER)
    dc_event_base = event_base_new();
    if (!dc_event_base) {
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    }
#endif

    memory_init(&mem);
    if (flash_path)
        flash_mem_load(flash_path);
    bios_file_init(&bios, bios_path);
    memory_map_init(&bios, &mem);
    sh4_init(&cpu);
    spg_init();

#ifdef ENABLE_SERIAL_SERVER
    if (serial_server_in_use) {
        serial_server_attach(&serial_server);
        sh4_scif_connect_server(&cpu, &serial_server);
    }
#endif

    aica_rtc_init();
}

#ifdef ENABLE_DIRECT_BOOT
void dreamcast_init_direct(char const *path_ip_bin,
                           char const *path_1st_read_bin,
                           char const *bios_path,
                           char const *flash_path,
                           char const *syscalls_path,
                           bool skip_ip_bin) {
    is_running = true;

// #ifdef ENABLE_DEBUGGER
//     debugger = NULL;
// #endif

#if defined(ENABLE_DEBUGGER) || defined(ENABLE_SERIAL_SERVER)
    dc_event_base = event_base_new(); // TODO: check for NULL
#endif

    memory_init(&mem);
    if (flash_path)
        flash_mem_load(flash_path);
    if (bios_path)
        bios_file_init(&bios, bios_path);
    else
        bios_file_init_empty(&bios);
    memory_map_init(&bios, &mem);

    long len_ip_bin;
    void *dat_ip_bin = load_file(path_ip_bin, &len_ip_bin);
    if (!dat_ip_bin) {
        error_set_file_path(path_ip_bin);
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
    }
    memory_map_write(dat_ip_bin, ADDR_IP_BIN & ~0xe0000000, len_ip_bin);
    free(dat_ip_bin);

    long len_1st_read_bin;
    void *dat_1st_read_bin = load_file(path_1st_read_bin, &len_1st_read_bin);
    if (!dat_1st_read_bin) {
        error_set_file_path(path_1st_read_bin);
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
    }
    memory_map_write(dat_1st_read_bin, ADDR_1ST_READ_BIN & ~0xe0000000,
                     len_1st_read_bin);
    free(dat_1st_read_bin);

    if (syscalls_path) {
        long syscalls_len;
        void *dat_syscalls = load_file(syscalls_path, &syscalls_len);

        if (!dat_syscalls) {
            error_set_file_path(syscalls_path);
            error_set_errno_val(errno);
            RAISE_ERROR(ERROR_FILE_IO);
        }

        if (syscalls_len != LEN_SYSCALLS) {
            error_set_length(syscalls_len);
            error_set_expected_length(LEN_SYSCALLS);
            RAISE_ERROR(ERROR_INVALID_FILE_LEN);
        }

        memory_map_write(dat_syscalls, ADDR_SYSCALLS & ~0xe0000000, syscalls_len);
        free(dat_syscalls);
    }

    sh4_init(&cpu);

    spg_init();

    /* set the PC to the booststrap code within IP.BIN */
    if (skip_ip_bin)
        cpu.reg[SH4_REG_PC] = ADDR_1ST_READ_BIN;
    else
        cpu.reg[SH4_REG_PC] = ADDR_BOOTSTRAP;

    /*
     * set the VBR to what it would have been after a BIOS boot.
     * This was obtained empirically on a real Dreamcast.
     *
     * XXX not sure if there should be a different value depending on whether
     * or not we skip IP.BIN.  All I do know is that this value is correct when
     * we do skip IP.BIN because I obtained it by running a homebrew that prints
     * the VBR value when it starts, which would be immediately after IP.BIN is
     * run.  It is possible that there's a different value immediately before
     * IP.BIN runs, and that the value seen by 1ST_READ.BIN is set by IP.BIN.
     */
    cpu.reg[SH4_REG_VBR] = 0x8c00f400;

    aica_rtc_init();
}
#endif

void dreamcast_cleanup() {
    spg_cleanup();

#ifdef ENABLE_DEBUGGER
    debug_cleanup(&debugger);
#endif

#ifdef ENABLE_SERIAL_SERVER
    if (serial_server_in_use)
        serial_server_cleanup(&serial_server);
#endif

    sh4_cleanup(&cpu);
    bios_file_cleanup(&bios);
    memory_cleanup(&mem);

#if defined(ENABLE_DEBUGGER) || defined(ENABLE_SERIAL_SERVER)
    event_base_free(dc_event_base);
#endif
}

#ifdef ENABLE_DEBUGGER
struct debugger *dreamcast_get_debugger() {
    if (using_debugger)
        return &debugger;
    return NULL;
}
#endif

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

    while (is_running) {
        /*
         * The below logic will run the dc_event_base if the debugger is
         * enabled or if the serial server is enabled.  If only the
         * debugger is enabled *or* both the debugger and the serial server
         * are enabled then it will pick either run_one or poll depending
         * on whether the debugger has suspended execution.  If only the
         * serial server is enabled, then it will unconditionally run
         * dc_ios_service.poll.
         */
#ifdef ENABLE_DEBUGGER
        /*
         * If the debugger is enabled, make sure we have its permission to
         * single-step; if we don't then we call dc_io_service.run_one to
         * block until something interresting happens, and then we skip the
         * rest of the loop.
         *
         * If we do have permission to single-step, then we call
         * dc_io_service.poll instead because we don't want to block.
         */
        if (using_debugger && debug_step(&debugger, sh4_get_pc(&cpu))) {
            if (event_base_loop(dc_event_base, EVLOOP_ONCE) < 0) {
                dreamcast_kill();
                break;
            }

            continue;
        } else {
            if (event_base_loop(dc_event_base, EVLOOP_NONBLOCK) < 0) {
                dreamcast_kill();
                break;
            }
        }
#else
#ifdef ENABLE_SERIAL_SERVER
        if (event_base_loop(dc_event_base, EVLOOP_NONBLOCK) < 0) {
            dreamcast_kill();
            break;
        }
#endif
#endif

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
    gfx_thread_notify_wake_up();
}

Sh4 *dreamcast_get_cpu() {
    return &cpu;
}

#ifdef ENABLE_DEBUGGER
void dreamcast_enable_debugger(void) {
    using_debugger = true;
    debug_init(&debugger);
    gdb_init(&gdb_stub, &debugger);
    debug_attach(&debugger);
}
#endif

#ifdef ENABLE_SERIAL_SERVER
void dreamcast_enable_serial_server(void) {
    serial_server_in_use = true;
    serial_server_init(&serial_server, &cpu);
    serial_server_attach(&serial_server);
    sh4_scif_connect_server(&cpu, &serial_server);
}
#endif

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
    return is_running;
}
