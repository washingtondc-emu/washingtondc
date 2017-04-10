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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -g")

    # stupid hack to get around unused static variables in boost/asio/error.hpp
    if (CMAKE_BUILD_TYPE MATCHES "Release")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-variable")
    endif()

endif()

option(ENABLE_SH4_MMU "emulate the sh4's Memory Management Unit (MMU)" ON)
option(ENABLE_DEBUGGER "Enable the debugger" ON)
option(ENABLE_DIRECT_BOOT "Enable direct boot mode (skip boot rom and \
immediately load IP.BIN or 1ST_READ.BIN)" ON)
option(DBG_EXIT_ON_UNDEFINED_OPCODE "Bail out if the emulator hits an undefined opcode" OFF)
option(ENABLE_SERIAL_SERVER "Enable serial server over TCP port 1998" ON)
option(INVARIANTS "runtime sanity checks that should never fail" ON)

if (ENABLE_DIRECT_BOOT)
   add_definitions(-DENABLE_DIRECT_BOOT)
endif()

if (DBG_EXIT_ON_UNDEFINED_OPCODE)
   add_definitions(-DDBG_EXIT_ON_UNDEFINED_OPCODE)
endif()

# TODO: this was originally supposed to be just the sh4-related code,
# but somehow it has swollen to encompass almost everything...
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
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_scif.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_scif.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_tmu.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_tmu.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_ocache.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_ocache.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/g1/g1_reg.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/g1/g1_reg.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/g2/g2_reg.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/g2/g2_reg.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/gdrom/gdrom_reg.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/gdrom/gdrom_reg.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/g2/modem.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/g2/modem.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_reg.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_reg.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_core_reg.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_core_reg.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_tex_mem.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_tex_mem.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/spg.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/spg.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sys/sys_block.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sys/sys_block.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sys/holly_intc.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sys/holly_intc.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/maple/maple_reg.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/maple/maple_reg.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_reg.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_reg.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_rtc.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_rtc.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_wave_mem.hpp"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_wave_mem.cpp"
                "${PROJECT_SOURCE_DIR}/src/BiosFile.hpp"
                "${PROJECT_SOURCE_DIR}/src/BiosFile.cpp"
                "${PROJECT_SOURCE_DIR}/src/flash_memory.hpp"
                "${PROJECT_SOURCE_DIR}/src/flash_memory.cpp"
                "${PROJECT_SOURCE_DIR}/src/Memory.hpp"
                "${PROJECT_SOURCE_DIR}/src/Memory.cpp"
                "${PROJECT_SOURCE_DIR}/src/MemoryMap.hpp"
                "${PROJECT_SOURCE_DIR}/src/MemoryMap.cpp"
                "${PROJECT_SOURCE_DIR}/src/arch/arch_fpu.hpp"
                "${PROJECT_SOURCE_DIR}/src/arch/x86_64/arch_fenv.hpp"
                "${PROJECT_SOURCE_DIR}/src/arch/x86_64/fesetround.cpp"
                "${PROJECT_SOURCE_DIR}/src/arch/x86_64/fegetround.cpp"
                "${PROJECT_SOURCE_DIR}/src/Dreamcast.hpp"
                "${PROJECT_SOURCE_DIR}/src/Dreamcast.cpp"
                "${PROJECT_SOURCE_DIR}/src/dc_sched.hpp"
                "${PROJECT_SOURCE_DIR}/src/dc_sched.cpp"
                "${PROJECT_SOURCE_DIR}/src/BiosFile.hpp"
                "${PROJECT_SOURCE_DIR}/src/BiosFile.cpp"
                "${PROJECT_SOURCE_DIR}/src/window.cpp"
                "${PROJECT_SOURCE_DIR}/src/window.hpp"
                "${PROJECT_SOURCE_DIR}/src/video/opengl/framebuffer.cpp"
                "${PROJECT_SOURCE_DIR}/src/video/opengl/framebuffer.hpp"
                "${PROJECT_SOURCE_DIR}/src/video/opengl/shader.cpp"
                "${PROJECT_SOURCE_DIR}/src/video/opengl/shader.hpp")

if (ENABLE_SH4_MMU)
    add_definitions(-DENABLE_SH4_MMU)
endif()

set(washingtondc_sources "${PROJECT_SOURCE_DIR}/src/BiosFile.hpp"
                         "${PROJECT_SOURCE_DIR}/src/BiosFile.cpp"
                         "${PROJECT_SOURCE_DIR}/src/main.cpp")

if (ENABLE_DEBUGGER)
    add_definitions(-DENABLE_DEBUGGER)
    set(sh4_sources ${sh4_sources} "${PROJECT_SOURCE_DIR}/src/Debugger.hpp"
                                   "${PROJECT_SOURCE_DIR}/src/Debugger.cpp"
                                   "${PROJECT_SOURCE_DIR}/src/GdbStub.hpp"
                                   "${PROJECT_SOURCE_DIR}/src/GdbStub.cpp")
endif()

if (ENABLE_SERIAL_SERVER)
  add_definitions(-DENABLE_SERIAL_SERVER)
  set(sh4_sources ${sh4_sources} "${PROJECT_SOURCE_DIR}/src/SerialServer.hpp"
                                 "${PROJECT_SOURCE_DIR}/src/SerialServer.cpp")
endif()

set(sh4asm_core_sources "${PROJECT_SOURCE_DIR}/src/tool/sh4asm/Inst.cpp"
                        "${PROJECT_SOURCE_DIR}/src/tool/sh4asm/sh4asm.cpp")

include_directories("${PROJECT_SOURCE_DIR}/src/"
                    "${PROJECT_SOURCE_DIR}/src/hw/sh4"
                    "${PROJECT_SOURCE_DIR}/src/common"
                    "${PROJECT_SOURCE_DIR}/external")

add_library(sh4asm_core ${sh4asm_core_sources})
add_library(sh4 ${sh4_sources})
add_library(boost_system "${PROJECT_SOURCE_DIR}/external/washingtondc-boost/libs/system/src/error_code.cpp")

# TODO: I wish there was an easy way to do this as a dependency like in make...
configure_file("src/video/opengl/final_frag.glsl" "final_frag.glsl")
configure_file("src/video/opengl/final_vert.glsl" "final_vert.glsl")

# TODO: These shouldn't all need to be linked against GLFW, but there are some
# GLFW function calls in Dreamcast.cpp, and since that's linked into libsh4
# (which it shouldn't be, but it is) everything that uses libsh4 also needs to
# link against GLFW.
#
# Ideally, only washingtondc should need GLFW

add_executable(sh4asm_test "${PROJECT_SOURCE_DIR}/unit_tests/sh4asm_test.cpp")
target_link_libraries(sh4asm_test sh4asm_core boost_system rt GL glfw GLEW)

add_executable(sh4mem_test "${PROJECT_SOURCE_DIR}/unit_tests/sh4mem_test.cpp")
target_link_libraries(sh4mem_test sh4 boost_system rt GL glfw GLEW)

add_executable(sh4inst_test "${PROJECT_SOURCE_DIR}/unit_tests/sh4inst_test.cpp")
target_link_libraries(sh4inst_test sh4 sh4asm_core boost_system rt GL glfw GLEW)

add_executable(sh4tmu_test "${PROJECT_SOURCE_DIR}/unit_tests/sh4tmu_test.cpp")
target_link_libraries(sh4tmu_test sh4 sh4asm_core boost_system rt GL glfw GLEW)

add_executable(sh4div_test "${PROJECT_SOURCE_DIR}/unit_tests/sh4div_test.cpp")
target_link_libraries(sh4div_test sh4 sh4asm_core boost_system rt GL glfw GLEW)

add_executable(sh4asm "${PROJECT_SOURCE_DIR}/src/tool/sh4asm/main.cpp")
target_link_libraries(sh4asm sh4asm_core rt)

add_executable(washingtondc ${washingtondc_sources})

target_link_libraries(washingtondc sh4 boost_system rt GL glfw GLEW)
