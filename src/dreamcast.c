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
#include "hw/gdrom/gdrom_reg.h"
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
#include "hw/pvr2/pvr2_tex_mem.h"
#include "hw/pvr2/pvr2_ta.h"
#include "log.h"
#include "hw/sh4/sh4_read_inst.h"
#include "hw/pvr2/pvr2.h"
#include "hw/pvr2/pvr2_reg.h"
#include "hw/sys/sys_block.h"
#include "hw/aica/aica.h"
#include "hw/aica/aica_common.h"
#include "hw/aica/aica_rtc.h"
#include "hw/g1/g1.h"
#include "hw/g1/g1_reg.h"
#include "hw/g2/g2.h"
#include "hw/g2/g2_reg.h"
#include "hw/g2/modem.h"
#include "hw/g2/external_dev.h"
#include "hw/maple/maple_reg.h"
#include "jit/code_block.h"
#include "jit/jit_intp/code_block_intp.h"
#include "jit/code_cache.h"
#include "jit/jit.h"
#include "gfx/opengl/overlay.h"
#include "hw/boot_rom.h"

#ifdef ENABLE_DEBUGGER
#include "io/gdb_stub.h"
#endif

#ifdef ENABLE_JIT_X86_64
#include "jit/x86_64/native_dispatch.h"
#include "jit/x86_64/native_mem.h"
#endif

#include "dreamcast.h"

static Sh4 cpu;
static struct Memory dc_mem;
static struct memory_map mem_map;
static struct boot_rom firmware;

static struct aica aica;

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

static struct dc_clock sh4_clock;

static void dc_sigint_handler(int param);

static void *load_file(char const *path, long *len);

static void construct_sh4_mem_map(struct Sh4 *sh4, struct memory_map *map);

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

void dreamcast_init(bool cmd_session) {
    is_running = true;

    memory_init(&dc_mem);
    flash_mem_load(config_get_dc_flash_path());
    boot_rom_init(&firmware, config_get_dc_bios_path());

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

    dc_clock_init(&sh4_clock);
    sh4_init(&cpu, &sh4_clock);
    jit_init(&sh4_clock);
    sys_block_init();
    g1_init();
    g2_init();
    aica_init(&aica);
    pvr2_init(&sh4_clock);
    gdrom_init(&sh4_clock);
    maple_init(&sh4_clock);

    memory_map_init(&mem_map);
    construct_sh4_mem_map(&cpu, &mem_map);
    sh4_set_mem_map(&cpu, &mem_map);

#ifdef ENABLE_JIT_X86_64
    native_mem_register(cpu.mem.map);
#endif

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

    aica_rtc_init(&sh4_clock);

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
    memory_map_cleanup(cpu.mem.map);

    maple_cleanup();
    gdrom_cleanup();
    pvr2_cleanup();
    memory_map_cleanup(&mem_map);
    aica_cleanup(&aica);
    g2_cleanup();
    g1_cleanup();
    sys_block_cleanup();

#ifdef ENABLE_DEBUGGER
    debug_cleanup();
#endif

    jit_cleanup();
    sh4_cleanup(&cpu);
    dc_clock_cleanup(&sh4_clock);
    boot_rom_cleanup(&firmware);
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
        struct SchedEvent *next_event = pop_event(&sh4_clock);
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
        struct SchedEvent *next_event = pop_event(&sh4_clock);
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
        struct SchedEvent *next_event = pop_event(&sh4_clock);
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

    periodic_event.when = clock_cycle_stamp(&sh4_clock) + DC_PERIODIC_EVENT_PERIOD;
    periodic_event.handler = periodic_event_handler;
    sched_event(&sh4_clock, &periodic_event);

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
    dc_cycle_stamp_t tgt_stamp = clock_target_stamp(&sh4_clock);

    while (tgt_stamp > clock_cycle_stamp(&sh4_clock)) {
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
        dc_cycle_stamp_t cycles_after = clock_cycle_stamp(&sh4_clock) +
            inst_cycles * SH4_CLOCK_SCALE;

        tgt_stamp = clock_target_stamp(&sh4_clock);
        if (cycles_after > tgt_stamp)
            cycles_after = tgt_stamp;

        sh4_do_exec_inst(sh4, inst, op);

        /*
         * advance the cycles, being careful not to skip over any new events
         * which may have been added
         */
        tgt_stamp = clock_target_stamp(&sh4_clock);
        if (cycles_after > tgt_stamp)
            cycles_after = tgt_stamp;
        clock_set_cycle_stamp(&sh4_clock, cycles_after);
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
    dc_cycle_stamp_t tgt_stamp = clock_target_stamp(&sh4_clock);

    while (tgt_stamp > clock_cycle_stamp(&sh4_clock)) {
        addr32_t blk_addr = newpc;
        struct cache_entry *ent = code_cache_find(blk_addr);

        struct code_block_intp *blk = &ent->blk.intp;
        if (!ent->valid) {
            jit_compile_intp(sh4, blk, blk_addr);
            ent->valid = true;
        }

        newpc = code_block_intp_exec(blk);

        dc_cycle_stamp_t cycles_after = clock_cycle_stamp(&sh4_clock) +
            blk->cycle_count;
        clock_set_cycle_stamp(&sh4_clock, cycles_after);
        tgt_stamp = clock_target_stamp(&sh4_clock);
    }
    if (clock_cycle_stamp(&sh4_clock) > tgt_stamp)
        clock_set_cycle_stamp(&sh4_clock, tgt_stamp);

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
    dc_cycle_stamp_t cycles_after = clock_cycle_stamp(&sh4_clock) +
        n_cycles * SH4_CLOCK_SCALE;
    dc_cycle_stamp_t tgt_stamp = sched_target_stamp();
    if (cycles_after > tgt_stamp)
        cycles_after = tgt_stamp;

    sh4_do_exec_inst(sh4, inst, op);

    // now execute any events which would have happened during that instruction
    SchedEvent *next_event;
    while ((next_event = peek_event(&sh4_clock)) &&
           (next_event->when <= cycles_after)) {
        pop_event(&sh4_clock);
        clock_set_cycle_stamp(&sh4_clock, next_event->when);
        next_event->handler(next_event);
    }

    clock_set_cycle_stamp(&sh4_clock, cycles_after);
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
             (unsigned)sh4_get_cycles(&cpu));

    double seconds = delta_time.tv_sec +
        ((double)delta_time.tv_nsec) / 1000000000.0;
    double hz = (double)sh4_get_cycles(&cpu) / seconds;
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

    periodic_event.when = clock_cycle_stamp(&sh4_clock) + DC_PERIODIC_EVENT_PERIOD;
    sched_event(&sh4_clock, &periodic_event);
}

void dc_end_frame(void) {
    struct timespec timestamp, delta;
    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    dc_cycle_stamp_t virt_timestamp = clock_cycle_stamp(&sh4_clock);

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

static void construct_sh4_mem_map(struct Sh4 *sh4, struct memory_map *map) {
    /*
     * I don't like the idea of putting SH4_AREA_P4 ahead of AREA3 (memory),
     * but this absolutely needs to be at the front of the list because the
     * only distinction between this and the other memory regions is that the
     * upper three bits of the address are all 1, and for the other regions the
     * upper three bits can be anything as long as they are not all 1.
     *
     * SH4_OC_RAM_AREA is also an SH4 on-chip component but as far as I know
     * nothing else in the dreamcast's memory map overlaps with it; this is why
     * have not also put it at the begging of the regions array.
     */
    memory_map_add(map, SH4_AREA_P4_FIRST, SH4_AREA_P4_LAST,
                   0xffffffff, 0xffffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &sh4_p4_intf, sh4);
    memory_map_add(map, ADDR_AREA3_FIRST, ADDR_AREA3_LAST,
                   0x1fffffff, ADDR_AREA3_MASK, MEMORY_MAP_REGION_RAM,
                   &ram_intf, &dc_mem);
    memory_map_add(map, ADDR_TEX32_FIRST, ADDR_TEX32_LAST,
                   0x1fffffff, 0x1fffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_tex_mem_area32_intf, NULL);
    memory_map_add(map, ADDR_TEX64_FIRST, ADDR_TEX64_LAST,
                   0x1fffffff, 0x1fffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_tex_mem_area64_intf, NULL);
    memory_map_add(map, ADDR_TA_FIFO_POLY_FIRST, ADDR_TA_FIFO_POLY_LAST,
                   0x1fffffff, 0x1fffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_ta_fifo_intf, NULL);
    memory_map_add(map, SH4_OC_RAM_AREA_FIRST, SH4_OC_RAM_AREA_LAST,
                   0xffffffff, 0xffffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &sh4_ora_intf, sh4);

    /*
     * TODO: everything below here needs to stay at the end so that the
     * masking/mirroring doesn't make it pick up addresses that should
     * belong to other parts of the map.  I need to come up with a better
     * way to implement mirroring.
     */
    memory_map_add(map, ADDR_BIOS_FIRST, ADDR_BIOS_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &boot_rom_intf, &firmware);
    memory_map_add(map, ADDR_FLASH_FIRST, ADDR_FLASH_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &flash_mem_intf, NULL);
    memory_map_add(map, ADDR_G1_FIRST, ADDR_G1_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &g1_intf, NULL);
    memory_map_add(map, ADDR_SYS_FIRST, ADDR_SYS_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &sys_block_intf, NULL);
    memory_map_add(map, ADDR_MAPLE_FIRST, ADDR_MAPLE_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &maple_intf, NULL);
    memory_map_add(map, ADDR_G2_FIRST, ADDR_G2_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &g2_intf, NULL);
    memory_map_add(map, ADDR_PVR2_FIRST, ADDR_PVR2_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_reg_intf, NULL);
    memory_map_add(map, ADDR_MODEM_FIRST, ADDR_MODEM_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &modem_intf, NULL);
    memory_map_add(map, ADDR_PVR2_CORE_FIRST, ADDR_PVR2_CORE_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_core_reg_intf, NULL);
    memory_map_add(map, ADDR_AICA_CHANNEL_FIRST, ADDR_AICA_CHANNEL_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_channel_intf, &aica.channel);
    memory_map_add(map, ADDR_AICA_COMMON_FIRST, ADDR_AICA_COMMON_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_common_intf, &aica.common);
    memory_map_add(map, ADDR_AICA_WAVE_FIRST, ADDR_AICA_WAVE_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_wave_mem_intf, &aica.mem);
    memory_map_add(map, ADDR_AICA_DSP_FIRST, ADDR_AICA_DSP_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_dsp_intf, &aica.dsp);
    memory_map_add(map, ADDR_AICA_RTC_FIRST, ADDR_AICA_RTC_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_rtc_intf, NULL);
    memory_map_add(map, ADDR_GDROM_FIRST, ADDR_GDROM_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &gdrom_reg_intf, NULL);
    memory_map_add(map, ADDR_EXT_DEV_FIRST, ADDR_EXT_DEV_LAST,
                   ADDR_AREA0_MASK, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &ext_dev_intf, NULL);
}
