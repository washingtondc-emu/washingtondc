################################################################################
#
#
#    WashingtonDC Dreamcast Emulator
#    Copyright (C) 2016-2018 snickerbockers
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
include(ExternalProject)

project(WashingtonDC)
set(WashingtonDC_VERSION_MAJOR 0)
set(WashingtonDC_VERSION_MINOR 1)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")

set (LIBEVENT_LIB_PATH ${CMAKE_CURRENT_BINARY_DIR})

ExternalProject_Add(
        libevent
        PREFIX washingtondc-libevent
        URL https://github.com/libevent/libevent/releases/download/release-2.1.8-stable/libevent-2.1.8-stable.tar.gz
        URL_MD5 f3eeaed018542963b7d2416ef1135ecc
        CONFIGURE_COMMAND ./configure
        BUILD_COMMAND make
        INSTALL_DIR ${LIBEVENT_LIB_PATH}
        INSTALL_COMMAND make prefix=${LIBEVENT_LIB_PATH} install
        BUILD_IN_SOURCE 1)

# i need this for strdup and clock_gettime on Debian for some unknown reason
add_definitions(-D_POSIX_C_SOURCE=200809L)

# While I prefer to avoid using GNU-specific stuff when possible, in general
# I don't give a damn about portability to Windows
add_definitions(-D_GNU_SOURCE)

enable_testing()

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW3 REQURED glfw3)
pkg_search_module(PNG REQUIRED libpng)

# turn on strict warnings - i have no idea how to do this in windows
if(UNIX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")
endif()

option(ENABLE_DEBUGGER "Enable the debugger" OFF)
option(DBG_EXIT_ON_UNDEFINED_OPCODE "Bail out if the emulator hits an undefined opcode" OFF)
option(INVARIANTS "runtime sanity checks that should never fail" ON)
option(SH4_FPU_PEDANTIC "enable FPU error-checking which most games *probably* don't use" OFF)
option(PVR2_LOG_VERBOSE "enable this to make the pvr2 code log mundane events" OFF)
option(DEEP_SYSCALL_TRACE "enable logging to observe the behavior of system calls" OFF)
option(ENABLE_LOG_DEBUG "enable extra debug logs" OFF)
option(ENABLE_JIT_X86_64 "enable native x86_64 JIT backend" ON)

if (DBG_EXIT_ON_UNDEFINED_OPCODE)
   add_definitions(-DDBG_EXIT_ON_UNDEFINED_OPCODE)
endif()

if (SH4_FPU_FAST)
   add_definitions(-DSH4_FPU_FAST)
endif()

if (INVARIANTS)
   add_definitions(-DINVARIANTS)
endif()

if (PVR2_LOG_VERBOSE)
  add_definitions(-DPVR2_LOG_VERBOSE)
endif()

if  (DEEP_SYSCALL_TRACE)
  add_definitions(-DDEEP_SYSCALL_TRACE)
endif()

if (ENABLE_LOG_DEBUG)
   add_definitions(-DENABLE_LOG_DEBUG)
endif()

# TODO: this was originally supposed to be just the sh4-related code,
# but somehow it has swollen to encompass almost everything...
set(sh4_sources "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_excp.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_excp.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_inst.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_read_inst.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_mem.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_mem.c"
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
                "${PROJECT_SOURCE_DIR}/src/hw/g1/g1.h"
                "${PROJECT_SOURCE_DIR}/src/hw/g1/g1.c"
                "${PROJECT_SOURCE_DIR}/src/hw/g1/g1_reg.h"
                "${PROJECT_SOURCE_DIR}/src/hw/g1/g1_reg.c"
                "${PROJECT_SOURCE_DIR}/src/hw/g2/g2.h"
                "${PROJECT_SOURCE_DIR}/src/hw/g2/g2.c"
                "${PROJECT_SOURCE_DIR}/src/hw/g2/g2_reg.h"
                "${PROJECT_SOURCE_DIR}/src/hw/g2/g2_reg.c"
                "${PROJECT_SOURCE_DIR}/src/hw/gdrom/gdrom.h"
                "${PROJECT_SOURCE_DIR}/src/hw/gdrom/gdrom.c"
                "${PROJECT_SOURCE_DIR}/src/hw/gdrom/gdrom_reg.h"
                "${PROJECT_SOURCE_DIR}/src/hw/gdrom/gdrom_reg.c"
                "${PROJECT_SOURCE_DIR}/src/hw/gdrom/gdrom_response.h"
                "${PROJECT_SOURCE_DIR}/src/hw/gdrom/gdrom_response.c"
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
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_ta.c"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_ta.h"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_tex_cache.c"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_tex_cache.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sys/sys_block.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sys/sys_block.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sys/holly_intc.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sys/holly_intc.h"
                "${PROJECT_SOURCE_DIR}/src/hw/maple/maple.h"
                "${PROJECT_SOURCE_DIR}/src/hw/maple/maple.c"
                "${PROJECT_SOURCE_DIR}/src/hw/maple/maple_device.h"
                "${PROJECT_SOURCE_DIR}/src/hw/maple/maple_device.c"
                "${PROJECT_SOURCE_DIR}/src/hw/maple/maple_controller.h"
                "${PROJECT_SOURCE_DIR}/src/hw/maple/maple_controller.c"
                "${PROJECT_SOURCE_DIR}/src/hw/maple/maple_reg.h"
                "${PROJECT_SOURCE_DIR}/src/hw/maple/maple_reg.c"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_reg.h"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_reg.c"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_rtc.h"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_rtc.c"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_wave_mem.h"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_wave_mem.c"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica.h"
                "${PROJECT_SOURCE_DIR}/src/hw/aica/aica.c"
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
                "${PROJECT_SOURCE_DIR}/src/glfw/window.c"
                "${PROJECT_SOURCE_DIR}/src/glfw/window.h"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/framebuffer.c"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/framebuffer.h"
                "${PROJECT_SOURCE_DIR}/src/gfx/opengl/shader.c"
                "${PROJECT_SOURCE_DIR}/src/gfx/opengl/shader.h"
                "${PROJECT_SOURCE_DIR}/src/gfx/opengl/opengl_output.h"
                "${PROJECT_SOURCE_DIR}/src/gfx/opengl/opengl_output.c"
                "${PROJECT_SOURCE_DIR}/src/gfx/opengl/opengl_target.h"
                "${PROJECT_SOURCE_DIR}/src/gfx/opengl/opengl_target.c"
                "${PROJECT_SOURCE_DIR}/src/gfx/opengl/opengl_renderer.h"
                "${PROJECT_SOURCE_DIR}/src/gfx/opengl/opengl_renderer.c"
                "${PROJECT_SOURCE_DIR}/src/gfx/rend_common.h"
                "${PROJECT_SOURCE_DIR}/src/gfx/rend_common.c"
                "${PROJECT_SOURCE_DIR}/src/gfx/gfx.h"
                "${PROJECT_SOURCE_DIR}/src/gfx/gfx.c"
                "${PROJECT_SOURCE_DIR}/src/gfx/geo_buf.c"
                "${PROJECT_SOURCE_DIR}/src/gfx/geo_buf.h"
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
                "${PROJECT_SOURCE_DIR}/src/cdrom.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_dmac.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_dmac.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_tbl.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_tbl.c"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_disas.h"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_disas.c"
                "${PROJECT_SOURCE_DIR}/src/host_branch_pred.h"
                "${PROJECT_SOURCE_DIR}/src/io/io_thread.h"
                "${PROJECT_SOURCE_DIR}/src/io/io_thread.c"
                "${PROJECT_SOURCE_DIR}/src/text_ring.h"
                "${PROJECT_SOURCE_DIR}/src/text_ring.c"
                "${PROJECT_SOURCE_DIR}/src/io/serial_server.h"
                "${PROJECT_SOURCE_DIR}/src/io/serial_server.c"
                "${PROJECT_SOURCE_DIR}/src/io/cmd_tcp.h"
                "${PROJECT_SOURCE_DIR}/src/io/cmd_tcp.c"
                "${PROJECT_SOURCE_DIR}/src/config.h"
                "${PROJECT_SOURCE_DIR}/src/config.c"
                "${PROJECT_SOURCE_DIR}/src/cmd/cmd_sys.h"
                "${PROJECT_SOURCE_DIR}/src/cmd/cmd_sys.c"
                "${PROJECT_SOURCE_DIR}/src/cmd/cmd_tcp_link.h"
                "${PROJECT_SOURCE_DIR}/src/cmd/cmd_tcp_link.c"
                "${PROJECT_SOURCE_DIR}/src/cmd/cons.h"
                "${PROJECT_SOURCE_DIR}/src/cmd/cons.c"
                "${PROJECT_SOURCE_DIR}/src/cmd/cmd.h"
                "${PROJECT_SOURCE_DIR}/src/cmd/cmd.c"
                "${PROJECT_SOURCE_DIR}/src/gfx/gfx_config.h"
                "${PROJECT_SOURCE_DIR}/src/gfx/gfx_config.c"
                "${PROJECT_SOURCE_DIR}/src/gfx/gfx_tex_cache.h"
                "${PROJECT_SOURCE_DIR}/src/gfx/gfx_tex_cache.c"
                "${PROJECT_SOURCE_DIR}/src/log.h"
                "${PROJECT_SOURCE_DIR}/src/mmio.h"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2.h"
                "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2.c"
                "${PROJECT_SOURCE_DIR}/src/gfx/geo_buf_serial.h"
                "${PROJECT_SOURCE_DIR}/src/gfx/geo_buf_serial.c"
                "${PROJECT_SOURCE_DIR}/src/jit/jit_il.h"
                "${PROJECT_SOURCE_DIR}/src/jit/jit_il.c"
                "${PROJECT_SOURCE_DIR}/src/jit/code_block.h"
                "${PROJECT_SOURCE_DIR}/src/jit/code_block.c"
                "${PROJECT_SOURCE_DIR}/src/jit/code_cache.c"
                "${PROJECT_SOURCE_DIR}/src/jit/code_cache.h"
                "${PROJECT_SOURCE_DIR}/src/jit/jit.h"
                "${PROJECT_SOURCE_DIR}/src/jit/jit.c"
                "${PROJECT_SOURCE_DIR}/src/jit/jit_intp/code_block_intp.h"
                "${PROJECT_SOURCE_DIR}/src/jit/jit_intp/code_block_intp.c")

set(washingtondc_sources "${PROJECT_SOURCE_DIR}/src/BiosFile.h"
                         "${PROJECT_SOURCE_DIR}/src/BiosFile.c"
                         "${PROJECT_SOURCE_DIR}/src/main.c")

if (ENABLE_JIT_X86_64)
   add_definitions(-DENABLE_JIT_X86_64)
   set(sh4_sources ${sh4_sources} "${PROJECT_SOURCE_DIR}/src/jit/x86_64/code_block_x86_64.h"
                                  "${PROJECT_SOURCE_DIR}/src/jit/x86_64/code_block_x86_64.c"
                                  "${PROJECT_SOURCE_DIR}/src/jit/x86_64/emit_x86_64.h"
                                  "${PROJECT_SOURCE_DIR}/src/jit/x86_64/emit_x86_64.c"
                                  "${PROJECT_SOURCE_DIR}/src/jit/x86_64/exec_mem.h"
                                  "${PROJECT_SOURCE_DIR}/src/jit/x86_64/exec_mem.c")
endif()

if (ENABLE_DEBUGGER)
    add_definitions(-DENABLE_DEBUGGER)
    set(sh4_sources ${sh4_sources} "${PROJECT_SOURCE_DIR}/src/debugger.h"
                                   "${PROJECT_SOURCE_DIR}/src/debugger.c"
                                   "${PROJECT_SOURCE_DIR}/src/io/gdb_stub.h"
                                   "${PROJECT_SOURCE_DIR}/src/io/gdb_stub.c")
endif()

if (DEEP_SYSCALL_TRACE)
   set(sh4_sources ${sh4_sources}
                   "${PROJECT_SOURCE_DIR}/src/deep_syscall_trace.h"
                   "${PROJECT_SOURCE_DIR}/src/deep_syscall_trace.c")
endif()

include_directories("${PROJECT_SOURCE_DIR}/src/"
                    "${PROJECT_SOURCE_DIR}/src/hw/sh4"
                    "${PROJECT_SOURCE_DIR}/common"
                    "${PROJECT_SOURCE_DIR}/external"
                    "${CMAKE_BINARY_DIR}/washingtondc-libevent/src/libevent/include")

add_library(sh4 ${sh4_sources})

# TODO: I wish there was an easy way to do this as a dependency like in make...
configure_file("src/gfx/opengl/final_frag.glsl" "final_frag.glsl" COPYONLY)
configure_file("src/gfx/opengl/final_vert.glsl" "final_vert.glsl" COPYONLY)
configure_file("src/gfx/opengl/pvr2_ta_vert.glsl" "pvr2_ta_vert.glsl" COPYONLY)
configure_file("src/gfx/opengl/pvr2_ta_frag.glsl" "pvr2_ta_frag.glsl" COPYONLY)

configure_file("regression_tests/sh4div_test.pl" "sh4div_test.pl" COPYONLY)
add_test(NAME sh4div_test COMMAND ./sh4div_test.pl)

configure_file("regression_tests/sh4tmu_test.pl" "sh4tmu_test.pl" COPYONLY)
add_test(NAME sh4tmu_test COMMAND ./sh4tmu_test.pl)

add_executable(washingtondc ${washingtondc_sources})

target_link_libraries(washingtondc m sh4 rt GL ${GLFW3_STATIC_LIBRARIES} GLEW pthread ${LIBEVENT_LIB_PATH}/lib/libevent.a ${LIBEVENT_LIB_PATH}/lib/libevent_pthreads.a ${PNG_STATIC_LIBRARIES})
