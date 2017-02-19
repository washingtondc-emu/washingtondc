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

#include <time.h>
#include <fstream>
#include <iostream>

#include "common/BaseException.hpp"
#include "flash_memory.hpp"

#ifdef ENABLE_DEBUGGER
#include "GdbStub.hpp"
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
}

#ifdef ENABLE_HLE_BOOT
void dreamcast_init_hle(char const *path_ip_bin,
                        char const *path_1st_read_bin,
                        char const *bios_path,
                        char const *flash_path) {
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

    sh4_init(&cpu);

    /* set the PC to the booststrap code within IP.BIN */
    cpu.reg[SH4_REG_PC] = ADDR_BOOTSTRAP;
}
#endif

void dreamcast_cleanup() {
#ifdef ENABLE_DEBUGGER
    if (debugger)
        delete debugger;
#endif

    sh4_cleanup(&cpu);
    delete bios;
    memory_cleanup(&mem);
}

void dreamcast_run() {
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
#ifdef ENABLE_DEBUGGER
            if (debugger && debugger->step(sh4_get_pc(&cpu)))
                continue;

            /*
             * TODO: don't single-step if there's no
             * chance of us hitting a breakpoint
             */
            sh4_single_step(&cpu);
#else
            sh4_run_cycles(&cpu, 4);
            sh4_tmu_tick(&cpu);
#endif
        }
    } catch(const BaseException& exc) {
        std::cerr << boost::diagnostic_information(exc);
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

    if (!is_running)
        std::cout << "program execution ended normally" << std::endl;

    std::cout << "Total elapsed time: " << delta_time.tv_sec <<
        " seconds and " << delta_time.tv_nsec << " nanoseconds." << std::endl;

    std::cout << cpu.cycle_stamp << " SH4 CPU cycles executed." << std::endl;

    double seconds = delta_time.tv_sec +
        double(delta_time.tv_nsec) / 1000000000.0;
    double hz = double(cpu.cycle_stamp) / seconds;
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
