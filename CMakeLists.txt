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
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

# turn on strict warnings - i have no idea how to do this in windows
if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

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
set(sh4_sources "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_excp.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_excp.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_inst.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_mem.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_mem.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_mmu.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_mmu.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_reg.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_reg_flags.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_reg.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_scif.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_scif.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_tmu.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_tmu.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/types.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_ocache.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_ocache.c"
                "${PROJECT_SOURCE_DIR}/src/hw/g1/g1_reg.h"
                "${PROJECT_SOURCE_DIR}/src/hw/g1/g1_reg.c"
                "${PROJECT_SOURCE_DIR}/src/hw/g2/g2_reg.h"
                "${PROJECT_SOURCE_DIR}/src/hw/g2/g2_reg.c"
                "${PROJECT_SOURCE_DIR}/src/hw/gdrom/gdrom_reg.h"
                "${PROJECT_SOURCE_DIR}/src/hw/gdrom/gdrom_reg.c"
                "${PROJECT_SOURCE_DIR}/src/hw/g2/modem.h"
                "${PROJECT_SOURCE_DIR}/src/hw/g2/modem.c"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_reg.h"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_reg.c"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_core_reg.h"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_core_reg.c"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_tex_mem.h"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_tex_mem.c"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/spg.c"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/spg.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sys/sys_block.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sys/sys_block.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sys/holly_intc.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sys/holly_intc.h"
                "${PROJECT_SOURCE_DIR}/src/hw/maple/maple_reg.h"
                "${PROJECT_SOURCE_DIR}/src/hw/maple/maple_reg.c"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_reg.h"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_reg.c"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_rtc.h"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_rtc.c"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_wave_mem.h"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_wave_mem.c"
                "${PROJECT_SOURCE_DIR}/src/BiosFile.h"
                "${PROJECT_SOURCE_DIR}/src/BiosFile.c"
                "${PROJECT_SOURCE_DIR}/src/flash_memory.h"
                "${PROJECT_SOURCE_DIR}/src/flash_memory.c"
                "${PROJECT_SOURCE_DIR}/src/mem_code.h"
                "${PROJECT_SOURCE_DIR}/src/memory.h"
                "${PROJECT_SOURCE_DIR}/src/memory.c"
                "${PROJECT_SOURCE_DIR}/src/MemoryMap.h"
                "${PROJECT_SOURCE_DIR}/src/MemoryMap.c"
                "${PROJECT_SOURCE_DIR}/src/dreamcast.h"
                "${PROJECT_SOURCE_DIR}/src/dreamcast.c"
                "${PROJECT_SOURCE_DIR}/src/dc_sched.h"
                "${PROJECT_SOURCE_DIR}/src/dc_sched.c"
                "${PROJECT_SOURCE_DIR}/src/window.c"
                "${PROJECT_SOURCE_DIR}/src/window.h"
                "${PROJECT_SOURCE_DIR}/src/video/opengl/framebuffer.c"
                "${PROJECT_SOURCE_DIR}/src/video/opengl/framebuffer.h"
                "${PROJECT_SOURCE_DIR}/src/video/opengl/shader.c"
                "${PROJECT_SOURCE_DIR}/src/video/opengl/shader.h"
                "${PROJECT_SOURCE_DIR}/src/video/opengl/opengl_backend.h"
                "${PROJECT_SOURCE_DIR}/src/video/opengl/opengl_backend.c"
                "${PROJECT_SOURCE_DIR}/src/gfx_thread.h"
                "${PROJECT_SOURCE_DIR}/src/gfx_thread.c"
                "${PROJECT_SOURCE_DIR}/src/error.c"
                "${PROJECT_SOURCE_DIR}/src/error.h"
                "${PROJECT_SOURCE_DIR}/src/mem_areas.h"
                "${PROJECT_SOURCE_DIR}/src/stringlib.h"
                "${PROJECT_SOURCE_DIR}/src/stringlib.c"
                "${PROJECT_SOURCE_DIR}/src/fifo.h"
                "${PROJECT_SOURCE_DIR}/src/gdi.h"
                "${PROJECT_SOURCE_DIR}/src/gdi.c"
                "${PROJECT_SOURCE_DIR}/src/mount.h"
                "${PROJECT_SOURCE_DIR}/src/mount.c"
                "${PROJECT_SOURCE_DIR}/src/cdrom.h"
                "${PROJECT_SOURCE_DIR}/src/cdrom.c")

if (ENABLE_SH4_MMU)
    add_definitions(-DENABLE_SH4_MMU)
endif()

set(washingtondc_sources "${PROJECT_SOURCE_DIR}/src/BiosFile.h"
                         "${PROJECT_SOURCE_DIR}/src/BiosFile.c"
                         "${PROJECT_SOURCE_DIR}/src/main.c")

if (ENABLE_DEBUGGER)
    add_definitions(-DENABLE_DEBUGGER)
    set(sh4_sources ${sh4_sources} "${PROJECT_SOURCE_DIR}/src/debugger.h"
                                   "${PROJECT_SOURCE_DIR}/src/debugger.c"
                                   "${PROJECT_SOURCE_DIR}/src/gdb_stub.h"
                                   "${PROJECT_SOURCE_DIR}/src/gdb_stub.c")
endif()

if (ENABLE_SERIAL_SERVER)
  add_definitions(-DENABLE_SERIAL_SERVER)
  set(sh4_sources ${sh4_sources} "${PROJECT_SOURCE_DIR}/src/serial_server.h"
                                 "${PROJECT_SOURCE_DIR}/src/serial_server.c")
endif()

set(sh4asm_core_sources "${PROJECT_SOURCE_DIR}/tool/sh4asm/Inst.cpp"
                        "${PROJECT_SOURCE_DIR}/tool/sh4asm/sh4asm.cpp")

include_directories("${PROJECT_SOURCE_DIR}/src/"
                    "${PROJECT_SOURCE_DIR}/src/hw/sh4"
                    "${PROJECT_SOURCE_DIR}/common"
                    "${PROJECT_SOURCE_DIR}/external")

add_library(sh4asm_core ${sh4asm_core_sources})
add_library(sh4 ${sh4_sources})

# TODO: I wish there was an easy way to do this as a dependency like in make...
configure_file("src/video/opengl/final_frag.glsl" "final_frag.glsl")
configure_file("src/video/opengl/final_vert.glsl" "final_vert.glsl")

# TODO: These shouldn't all need to be linked against GLFW, but there are some
# GLFW function calls in Dreamcast.cpp, and since that's linked into libsh4
# (which it shouldn't be, but it is) everything that uses libsh4 also needs to
# link against GLFW.
#
# Ideally, only washingtondc should need GLFW

set(common_headers "${PROJECT_SOURCE_DIR}/common/BaseException.hpp")

add_executable(sh4asm_test "${PROJECT_SOURCE_DIR}/unit_tests/sh4asm_test.cpp" ${common_headers})
target_link_libraries(sh4asm_test m sh4asm_core rt GL glfw GLEW pthread)
target_include_directories(sh4asm_test PUBLIC "${PROJECT_SOURCE_DIR}/tool")
add_test(NAME sh4asm_test COMMAND ./sh4asm_test)

add_executable(sh4mem_test "${PROJECT_SOURCE_DIR}/unit_tests/sh4mem_test.cpp" ${common_headers})
target_link_libraries(sh4mem_test m sh4 rt GL glfw GLEW pthread event)
add_test(NAME sh4mem_test COMMAND ./sh4mem_test)

add_executable(sh4inst_test "${PROJECT_SOURCE_DIR}/unit_tests/sh4inst_test.cpp" ${common_headers})
target_link_libraries(sh4inst_test m sh4 sh4asm_core rt GL glfw GLEW pthread event)
target_include_directories(sh4inst_test PUBLIC "${PROJECT_SOURCE_DIR}/tool")
add_test(NAME sh4inst_test COMMAND ./sh4inst_test)

add_executable(sh4tmu_test "${PROJECT_SOURCE_DIR}/unit_tests/sh4tmu_test.cpp" ${common_headers})
target_link_libraries(sh4tmu_test m sh4 sh4asm_core rt GL glfw GLEW pthread event)
target_include_directories(sh4tmu_test PUBLIC "${PROJECT_SOURCE_DIR}/tool")
add_test(NAME sh4tmu_test COMMAND ./sh4tmu_test)

add_executable(sh4div_test "${PROJECT_SOURCE_DIR}/unit_tests/sh4div_test.cpp" ${common_headers})
target_link_libraries(sh4div_test m sh4 sh4asm_core rt GL glfw GLEW pthread event)
target_include_directories(sh4div_test PUBLIC "${PROJECT_SOURCE_DIR}/tool")
add_test(NAME sh4div_test COMMAND ./sh4div_test)

add_executable(sh4asm "${PROJECT_SOURCE_DIR}/tool/sh4asm/main.cpp" ${common_headers})
target_link_libraries(sh4asm m sh4asm_core rt)

add_executable(washingtondc ${washingtondc_sources})

target_link_libraries(washingtondc m sh4 rt GL glfw GLEW pthread event)
