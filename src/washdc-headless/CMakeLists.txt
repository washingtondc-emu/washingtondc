################################################################################
#
#
#    WashingtonDC Dreamcast Emulator
#    Copyright (C) 2019 snickerbockers
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

set(CMAKE_LEGACY_CYGWIN_WIN32 0) # Remove when CMake >= 2.8.4 is required
cmake_minimum_required(VERSION 2.6)

project(washdc_headless CXX)
set(washdc_headless_VERSION_MAJOR 0)
set(washdc_headless_VERSION_MINOR 1)

set(IO_SOURCE_DIR "${PROJECT_SOURCE_DIR}/frontend_io")
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
			    "hostfile.hpp"
			    "gfx_null.hpp"
			    "gfx_null.cpp")

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
  set(washdc_headless_libs "${washdc_headless_libs}" "event_pthreads" "event" "pthread")
endif()

add_executable(washdc-headless "${washdc_headless_sources}")

target_include_directories(washdc-headless PRIVATE
                                           "${CMAKE_SOURCE_DIR}/src/libwashdc/include"
                                           "${CMAKE_SOURCE_DIR}/external/capstone/include"
                                           "${CMAKE_SOURCE_DIR}/external/sh4asm"
                                           "${CMAKE_SOURCE_DIR}/external/libevent/include")

target_link_libraries(washdc-headless "${washdc_headless_libs}")
