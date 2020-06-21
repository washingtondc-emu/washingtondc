################################################################################
#
# Copyright 2019, 2020 snickerbockers
# snickerbockers@washemu.org
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors
# may be used to endorse or promote products derived from this software without
# specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
# WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
################################################################################

set(CMAKE_LEGACY_CYGWIN_WIN32 0) # Remove when CMake >= 2.8.4 is required
cmake_minimum_required(VERSION 2.6)

project(washdc_headless CXX)
set(washdc_headless_VERSION_MAJOR 0)
set(washdc_headless_VERSION_MINOR 1)

set(IO_SOURCE_DIR "${CMAKE_SOURCE_DIR}/src/common/frontend_io")
set(io_sources "${IO_SOURCE_DIR}/io_thread.hpp"
               "${IO_SOURCE_DIR}/io_thread.cpp")

set(SH4ASM_SOURCE_DIR "${CMAKE_SOURCE_DIR}/external/sh4asm/sh4asm_core")
set(sh4asm_sources "${SH4ASM_SOURCE_DIR}/disas.h"
                   "${SH4ASM_SOURCE_DIR}/sh4asm_txt_emit.h")

set(debug_sources "${IO_SOURCE_DIR}/washdbg_tcp.hpp"
                  "${IO_SOURCE_DIR}/washdbg_tcp.cpp"
                  "${IO_SOURCE_DIR}/washdbg_core.hpp"
                  "${IO_SOURCE_DIR}/washdbg_core.cpp"
                  "${IO_SOURCE_DIR}/gdb_stub.hpp"
                  "${IO_SOURCE_DIR}/gdb_stub.cpp")

set(debug_sources "${IO_SOURCE_DIR}/washdbg_tcp.hpp"
                  "${IO_SOURCE_DIR}/washdbg_tcp.cpp"
                  "${IO_SOURCE_DIR}/washdbg_core.hpp"
                  "${IO_SOURCE_DIR}/washdbg_core.cpp"
                  "${IO_SOURCE_DIR}/gdb_stub.hpp"
                  "${IO_SOURCE_DIR}/gdb_stub.cpp")

set(serial_sources "${IO_SOURCE_DIR}/serial_server.hpp"
                   "${IO_SOURCE_DIR}/serial_server.cpp")

set(serial_sources "${IO_SOURCE_DIR}/serial_server.hpp"
                   "${IO_SOURCE_DIR}/serial_server.cpp")

set(washdc_headless_sources "main.cpp"
                            "console_config.hpp"
                            "console_config.cpp"
			    "gfx_null.hpp"
			    "gfx_null.cpp"
			    "gfx_obj.h"
			    "gfx_obj.c")

if (ENABLE_TCP_SERIAL)
    add_definitions(-DENABLE_TCP_SERIAL)
    if (NOT USE_LIBEVENT)
        message(FATAL_ERROR "-DUSE_LIBEVENT=On is a prerequisite for -DENABLE_TCP_SERIAL=On")
    endif()
    set (washdc_headless_sources ${washdc_headless_sources} ${serial_sources})
endif()

set(need_capstone "FALSE")

if (JIT_PROFILE)
    set(need_capstone "TRUE")
endif()

# TODO: I don't want to link against libpng and zlib for the headless client,
# but I have to because the code that saves screenshots is part of the core
# library.
set(washdc_headless_libs washdc png zlib)

if (ENABLE_DEBUGGER)
    if (NOT USE_LIBEVENT)
        message(FATAL_ERROR "-DUSE_LIBEVENT=On is a prerequisite for -DENABLE_DEBUGGER=On")
    endif()

    set(need_capstone "TRUE")

    set(washdc_headless_sources ${washdc_headless_sources} ${sh4asm_sources} ${debug_sources})

    add_definitions(-DENABLE_DEBUGGER)
    if (ENABLE_WATCHPOINTS)
        add_definitions(-DENABLE_WATCHPOINTS)
    endif()

    if (ENABLE_DBG_COND)
        add_definitions(-DENABLE_DBG_COND)
    endif()
endif()

if (need_capstone STREQUAL "TRUE")
    if (JIT_PROFILE)
        set(CAPSTONE_X86_SUPPORT ON CACHE BOOL "" FORCE)
    else()
        set(CAPSTONE_X86_SUPPORT OFF CACHE BOOL "" FORCE)
    endif()

    set(CAPSTONE_ARM_SUPPORT ON CACHE BOOL "" FORCE)

    set(CAPSTONE_ARM64_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_M68K_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_MIPS_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_PPC_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_SPARC_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_SYSZ_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_XCORE_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_TMS320C64X_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_M680X_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_EVM_SUPPORT OFF CACHE BOOL "" FORCE)

    set(washdc_headless_libs "${washdc_headless_libs}" capstone-static)
endif()

if (USE_LIBEVENT)
    set(washdc_headless_sources "${washdc_headless_sources}" "${io_sources}")
    set(washdc_headless_libs "${washdc_headless_libs}" "event_core")
    if (WIN32)
        target_link_libraries(event_core INTERFACE ws2_32)
    else()
        set(washdc_headless_libs "${washdc_headless_libs}" "pthread" "event_pthreads")
    endif()
endif()

add_executable(washdc-headless "${washdc_headless_sources}")

target_include_directories(washdc-headless PRIVATE
                                           "${CMAKE_SOURCE_DIR}/src/libwashdc/include"
                                           "${CMAKE_SOURCE_DIR}/src/common"
                                           "${CMAKE_SOURCE_DIR}/external/capstone/include"
                                           "${CMAKE_SOURCE_DIR}/external/sh4asm"
                                           "${CMAKE_SOURCE_DIR}/external/libevent/include"
                                           "${CMAKE_BINARY_DIR}/external/libevent/include")
target_link_libraries(washdc-headless "${washdc_headless_libs}")
