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

#include <ctime>
#include <csignal>
#include <fstream>
#include <iostream>

#include "common/BaseException.hpp"
#include "flash_memory.hpp"
#include "dc_sched.hpp"
#include "hw/pvr2/spg.hpp"
#include "window.hpp"

#ifdef ENABLE_DEBUGGER
#include "GdbStub.hpp"
#endif

#ifdef ENABLE_SERIAL_SERVER
#include "SerialServer.hpp"
#endif

#include "Dreamcast.hpp"

static const size_t MEM_SZ = 16 * 1024 * 1024;

static Sh4 cpu;
static BiosFile *bios;
static struct Memory mem;

static bool is_running;

#ifdef ENABLE_DEBUGGER
static Debugger *debugger;
#endif

#ifdef ENABLE_SERIAL_SERVER
static SerialServer *serial_server;
#endif

#if defined ENABLE_DEBUGGER || defined ENABLE_SERIAL_SERVER
// Used by the GdbStub and SerialServer (if enabled) to do async network I/O
boost::asio::io_service dc_io_service;
#endif

dc_cycle_stamp_t dc_cycle_stamp_priv_;

enum TermReason {
    TERM_REASON_NORM,   // normal program exit
    TERM_REASON_SIGINT, // received SIGINT
    TERM_REASON_ERROR   // usually this means somebody threw a c++ exception
};

// this stores the reason the dreamcast suspended execution
TermReason term_reason = TERM_REASON_NORM;

static void dc_sigint_handler(int param);

void dreamcast_init(char const *bios_path, char const *flash_path) {
    is_running = true;

#ifdef ENABLE_DEBUGGER
    debugger = NULL;
#endif

    memory_init(&mem, MEM_SZ);
    if (flash_path)
        flash_mem_load(flash_path);
    bios = new BiosFile(bios_path);
    memory_map_init(bios, &mem);
    sh4_init(&cpu);
    spg_init();

#ifdef ENABLE_SERIAL_SERVER
    if (serial_server) {
        serial_server->attach();
        sh4_scif_connect_server(&cpu, serial_server);
    }
#endif
}

#ifdef ENABLE_DIRECT_BOOT
void dreamcast_init_direct(char const *path_ip_bin,
                           char const *path_1st_read_bin,
                           char const *bios_path,
                           char const *flash_path,
                           char const *syscalls_path,
                           bool skip_ip_bin) {
    std::ifstream file_ip_bin(path_ip_bin,
                              std::ifstream::in | std::ifstream::binary);
    std::ifstream file_1st_read_bin(path_1st_read_bin,
                                    std::ifstream::in | std::ifstream::binary);
    is_running = true;

#ifdef ENABLE_DEBUGGER
    debugger = NULL;
#endif

    memory_init(&mem, MEM_SZ);
    if (flash_path)
        flash_mem_load(flash_path);
    if (bios_path)
        bios = new BiosFile(bios_path);
    else
        bios = new BiosFile();
    memory_map_init(bios, &mem);

    file_ip_bin.seekg(0, file_ip_bin.end);
    size_t len_ip_bin = file_ip_bin.tellg();
    file_ip_bin.seekg(0, file_ip_bin.beg);

    file_1st_read_bin.seekg(0, file_1st_read_bin.end);
    size_t len_1st_read_bin = file_1st_read_bin.tellg();
    file_1st_read_bin.seekg(0, file_1st_read_bin.beg);

    uint8_t *dat = new uint8_t[len_ip_bin];
    file_ip_bin.read((char*)dat, sizeof(uint8_t) * len_ip_bin);
    memory_map_write(dat, ADDR_IP_BIN & ~0xe0000000, len_ip_bin);
    delete[] dat;

    dat = new uint8_t[len_1st_read_bin];
    file_1st_read_bin.read((char*)dat, sizeof(uint8_t) * len_1st_read_bin);
    memory_map_write(dat, ADDR_1ST_READ_BIN & ~0xe0000000, len_1st_read_bin);
    delete[] dat;

    if (syscalls_path) {
        size_t syscalls_len;
        std::ifstream file_syscalls(syscalls_path,
                                    std::ifstream::in | std::ifstream::binary);
        file_syscalls.seekg(0, file_syscalls.end);
        syscalls_len = file_syscalls.tellg();
        file_syscalls.seekg(0, file_syscalls.beg);

        if (syscalls_len != LEN_SYSCALLS)
            BOOST_THROW_EXCEPTION(InvalidFileLengthError() <<
                                  errinfo_length(syscalls_len) <<
                                  errinfo_length_expect(LEN_SYSCALLS));
        uint8_t *dat = new uint8_t[syscalls_len];
        file_syscalls.read((char*)dat, sizeof(uint8_t) * syscalls_len);
        memory_map_write(dat, ADDR_SYSCALLS & ~0xe0000000, syscalls_len);
        delete[] dat;
    }

    sh4_init(&cpu);

    spg_init();

    /* set the PC to the booststrap code within IP.BIN */
    if (skip_ip_bin)
        cpu.reg[SH4_REG_PC] = ADDR_1ST_READ_BIN;
    else
        cpu.reg[SH4_REG_PC] = ADDR_BOOTSTRAP;
}
#endif

void dreamcast_cleanup() {
    spg_cleanup();

#ifdef ENABLE_DEBUGGER
    if (debugger)
        delete debugger;
#endif

#ifdef ENABLE_SERIAL_SERVER
    if (serial_server)
        delete serial_server;
#endif

    sh4_cleanup(&cpu);
    delete bios;
    memory_cleanup(&mem);
}

#ifdef ENABLE_DEBUGGER
Debugger *dreamcast_get_debugger() {
    return debugger;
}
#endif

void dreamcast_run() {
    signal(SIGINT, dc_sigint_handler);

    /*
     * TODO: later when I'm emulating more than just the CPU,
     * I'll need to remember to call this every time I re-enter
     * the CPU's context.
     */
    sh4_enter(&cpu);

    /*
     * store the irl timestamp right before execution begins.
     * This exists for performance profiling purposes only.
     */
    struct timespec start_time, end_time, delta_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    try {
        while (is_running) {
            is_running = is_running && win_check_events();

            /*
             * The below logic will run the dc_io_service if the debugger is
             * enabled or if the serial server is enabled.  If only the
             * debugger is enabled *or* both the debugger and the serial server
             * are enabled then it will pick either run_one or poll depending
             * on whether the debugger has suspended execution.  If only the
             * serial server is enabled, then it will unconditionally run
             * dc_ios_service.poll.
             */
#ifdef ENABLE_DEBUGGER
            /*
             * If the debugger is enabled, make sure we have its permission to single-step;
             * if we don't then we call dc_io_service.run_one to block until
             * something interresting happens, and then we skip the rest of the loop.
             *
             * If we do have permission to single-step, then we call
             * dc_io_service.poll instead because we don't want to block.
             */
            if (debugger && debugger->step(sh4_get_pc(&cpu))) {
                dc_io_service.run_one();

                continue;
            } else {
                dc_io_service.poll();
            }
#else
#ifdef ENABLE_SERIAL_SERVER
            dc_io_service.poll();
#endif
#endif

#ifdef ENABLE_DEBUGGER
            /*
             * TODO: don't single-step if there's no
             * chance of us hitting a breakpoint
             */
            sh4_single_step(&cpu);
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
    } catch(const BaseException& exc) {
        std::cerr << boost::diagnostic_information(exc);
        term_reason = TERM_REASON_ERROR;
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    /* subtract delta_time = end_time - start_time */
    if (end_time.tv_nsec < start_time.tv_nsec) {
        delta_time.tv_nsec = 1000000000 - start_time.tv_nsec + end_time.tv_nsec;
        delta_time.tv_sec = end_time.tv_sec - 1 - start_time.tv_sec;
    } else {
        delta_time.tv_nsec = end_time.tv_nsec - start_time.tv_nsec;
        delta_time.tv_sec = end_time.tv_sec - start_time.tv_sec;
    }

    switch (term_reason) {
    case TERM_REASON_NORM:
        std::cout << "program execution ended normally" << std::endl;
        break;
    case TERM_REASON_ERROR:
        std::cout << "program execution ended due to an unrecoverable " <<
            "error" << std::endl;
        break;
    case TERM_REASON_SIGINT:
        std::cout << "program execution ended due to user-initiated " <<
            "interruption" << std::endl;
        break;
    default:
        std::cout << "program execution ended for unknown reasons" << std::endl;
        break;
    }

    std::cout << "Total elapsed time: " << std::dec << delta_time.tv_sec <<
        " seconds and " << delta_time.tv_nsec << " nanoseconds." << std::endl;

    std::cout << dc_cycle_stamp() << " SH4 CPU cycles executed." << std::endl;

    double seconds = delta_time.tv_sec +
        double(delta_time.tv_nsec) / 1000000000.0;
    double hz = double(dc_cycle_stamp()) / seconds;
    double hz_ratio = hz / 200000000.0;

    std::cout << "Performance is " << (hz / 1000000.0) << " MHz (" <<
        (hz_ratio * 100.0) << "%)" << std::endl;
}

void dreamcast_kill() {
    is_running = false;
}

Sh4 *dreamcast_get_cpu() {
    return &cpu;
}

#ifdef ENABLE_DEBUGGER
void dreamcast_enable_debugger(void) {
    debugger = new GdbStub();
    debugger->attach();
}
#endif

#ifdef ENABLE_SERIAL_SERVER
void dreamcast_enable_serial_server(void) {
    serial_server = new SerialServer(&cpu);
    serial_server->attach();
    sh4_scif_connect_server(&cpu, serial_server);
}
#endif

static void dc_sigint_handler(int param) {
    is_running = false;
    term_reason = TERM_REASON_SIGINT;
}
