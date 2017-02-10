################################################################################
#
#
#    WashingtonDC Dreamcast Emulator
#    Copyright (C) 2016, 2017 snickerbockers
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#
################################################################################

cmake_minimum_required(VERSION 2.6)
project(WashingtonDC)
set(WashingtonDC_VERSION_MAJOR 0)
set(WashingtonDC_VERSION_MINOR 1)
set(CMAKE_CXX_STANDARD 98)

# turn on strict warnings - i have no idea how to do this in windows
if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
endif()

option(ENABLE_SH4_ICACHE "emulate the sh4's instruction cache" ON)
option(ENABLE_SH4_MMU "emulate the sh4's Memory Management Unit (MMU)" ON)
option(ENABLE_DEBUGGER "Enable the debugger" ON)

set(sh4_sources "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_excp.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_excp.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_inst.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_mem.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_mmu.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_mmu.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_reg.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_reg_flags.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_reg.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_tmu.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_tmu.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_ocache.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_ocache.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/g1/g1.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/g1/g1_reg.cpp"
                "${PROJECT_SOURCE_DIR}/src/BiosFile.hpp"
                "${PROJECT_SOURCE_DIR}/src/BiosFile.cpp"
                "${PROJECT_SOURCE_DIR}/src/Memory.hpp"
                "${PROJECT_SOURCE_DIR}/src/Memory.cpp"
                "${PROJECT_SOURCE_DIR}/src/MemoryMap.hpp"
                "${PROJECT_SOURCE_DIR}/src/MemoryMap.cpp"
                "${PROJECT_SOURCE_DIR}/src/arch/arch_fpu.hpp"
                "${PROJECT_SOURCE_DIR}/src/arch/x86_64/arch_fenv.hpp"
                "${PROJECT_SOURCE_DIR}/src/arch/x86_64/fesetround.cpp"
                "${PROJECT_SOURCE_DIR}/src/arch/x86_64/fegetround.cpp")

if (ENABLE_SH4_ICACHE)
    add_definitions(-DENABLE_SH4_ICACHE)
    set(sh4_sources ${sh4_sources} "${PROJECT_SOURCE_DIR}/src/hw/sh4/Icache.cpp")
    set(sh4_sources ${sh4_sources} "${PROJECT_SOURCE_DIR}/src/hw/sh4/Icache.hpp")
endif()

if (ENABLE_SH4_MMU)
    add_definitions(-DENABLE_SH4_MMU)
endif()

set(washingtondc_sources "${PROJECT_SOURCE_DIR}/src/BiosFile.hpp"
                         "${PROJECT_SOURCE_DIR}/src/BiosFile.cpp"
                         "${PROJECT_SOURCE_DIR}/src/Dreamcast.hpp"
                         "${PROJECT_SOURCE_DIR}/src/Dreamcast.cpp"
                         "${PROJECT_SOURCE_DIR}/src/main.cpp")

if (ENABLE_DEBUGGER)
    add_definitions(-DENABLE_DEBUGGER)
    set(sh4_sources ${sh4_sources} "${PROJECT_SOURCE_DIR}/src/Debugger.hpp"
                                   "${PROJECT_SOURCE_DIR}/src/Debugger.cpp"
                                   "${PROJECT_SOURCE_DIR}/src/GdbStub.hpp"
                                   "${PROJECT_SOURCE_DIR}/src/GdbStub.cpp")
endif()

set(sh4asm_core_sources "${PROJECT_SOURCE_DIR}/src/tool/sh4asm/Inst.cpp"
                        "${PROJECT_SOURCE_DIR}/src/tool/sh4asm/sh4asm.cpp")

include_directories("${PROJECT_SOURCE_DIR}/src/"
                    "${PROJECT_SOURCE_DIR}/src/hw/sh4"
                    "${PROJECT_SOURCE_DIR}/src/common"
                    "${PROJECT_SOURCE_DIR}/external")

add_library(sh4asm_core ${sh4asm_core_sources})
add_library(sh4 ${sh4_sources})

add_executable(sh4asm_test "${PROJECT_SOURCE_DIR}/unit_tests/sh4asm_test.cpp")
target_link_libraries(sh4asm_test sh4asm_core)

add_executable(sh4mem_test "${PROJECT_SOURCE_DIR}/unit_tests/sh4mem_test.cpp")
target_link_libraries(sh4mem_test sh4)

add_executable(sh4inst_test "${PROJECT_SOURCE_DIR}/unit_tests/sh4inst_test.cpp")
target_link_libraries(sh4inst_test sh4 sh4asm_core)

add_executable(sh4tmu_test "${PROJECT_SOURCE_DIR}/unit_tests/sh4tmu_test.cpp")
target_link_libraries(sh4tmu_test sh4 sh4asm_core)

add_executable(sh4asm "${PROJECT_SOURCE_DIR}/src/tool/sh4asm/main.cpp")
target_link_libraries(sh4asm sh4asm_core)

add_executable(washingtondc ${washingtondc_sources})

add_library(boost_system "${PROJECT_SOURCE_DIR}/external/washingtondc-boost/libs/system/src/error_code.cpp")

target_link_libraries(washingtondc sh4 boost_system)
