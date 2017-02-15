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
#include <iostream>

#include "common/BaseException.hpp"

#ifdef ENABLE_DEBUGGER
#include "GdbStub.hpp"
#endif

#include "Dreamcast.hpp"

Dreamcast::Dreamcast(char const *bios_path) {
    is_running = true;

#ifdef ENABLE_DEBUGGER
    debugger = NULL;
#endif

    memory_init(&mem, MEM_SZ);
    bios = new BiosFile(bios_path);
    memory_map_init(bios, &mem);
    sh4_init(&cpu);
}

Dreamcast::~Dreamcast() {
#ifdef ENABLE_DEBUGGER
    if (debugger)
        delete debugger;
#endif

    sh4_cleanup(&cpu);
    delete bios;
    memory_cleanup(&mem);
}

void Dreamcast::run() {
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

void Dreamcast::kill() {
    is_running = false;
}

Sh4 *Dreamcast::get_cpu() {
    return &cpu;
}

Memory *Dreamcast::gem_mem() {
    return &mem;
}

#ifdef ENABLE_DEBUGGER
void Dreamcast::enable_debugger(void) {
    debugger = new GdbStub(this);
    debugger->attach();
}
#endif
