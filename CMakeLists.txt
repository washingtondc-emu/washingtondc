################################################################################
#
# Copyright 2016-2020 snickerbockers
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

cmake_minimum_required(VERSION 2.8.4)
set(CMAKE_LEGACY_CYGWIN_WIN32 0) # Remove when CMake >= 2.8.4 is required

option(ENABLE_DEBUGGER "Enable the debugger" ON)
option(ENABLE_WATCHPOINTS "Enable debugger watchpoints" OFF)
option(ENABLE_DBG_COND "enable debugger conditions" OFF)
option(DBG_EXIT_ON_UNDEFINED_OPCODE "Bail out if the emulator hits an undefined opcode" OFF)
option(INVARIANTS "runtime sanity checks that should never fail" OFF)
option(SH4_FPU_PEDANTIC "enable FPU error-checking which most games *probably* don't use" OFF)
option(PVR2_LOG_VERBOSE "enable this to make the pvr2 code log mundane events" OFF)
option(DEEP_SYSCALL_TRACE "enable logging to observe the behavior of system calls" OFF)
option(ENABLE_LOG_DEBUG "enable extra debug logs" OFF)
option(ENABLE_JIT_X86_64 "enable native x86_64 JIT backend" ON)
option(ENABLE_TCP_SERIAL "enable serial server emulator over tcp port 1998" ON)
option(USE_LIBEVENT "use libevent for asynchronous I/O processing" ON)
option(JIT_PROFILE "Profile JIT code blocks based on frequency" OFF)
option(BUILD_WASHINGTONDC "Build the washingtondc frontend program" ON)
option(BUILD_WASHDC_HEADLESS "Build the washdc-headless frontend program" ON)
option(ENABLE_TESTS "enable automatic testing" OFF)
option(ENABLE_MMU "enable the SH4's Memory Management Unit (interpreter only)" OFF)

# OKAY.
# this one is sort-of a compatibility hack to get around outdated packages on
# certain Linux distros but it only applies to the frontend so I'm okay with it.
# Basically, the memory dump UI uses an imgui filebrowser which relies upon
# std::filesystem.  This isn't available in gcc until gcc-8.  As of this
# writing, several mainstream linux distros still use older versions of gcc as
# the default CC.  mingw-w64 also suffers from this problem.
#
# This is a debugging feature that's probably not interesting to most users, so
# I don't think it's fair to require people to use a specific compiler just for
# this one feature, so I'm making it an optional feature which is disabled by
# default for now.  This also benefits me because I don't have to put up with
# this bollocks in the github-actions CI.
option(ENABLE_MEM_DUMP_UI "let users dump memory from the washingtondc frontend" OFF)

if (ENABLE_MMU)
    add_definitions(-DENABLE_MMU)
endif()

if (ENABLE_TESTS)
    # TODO tests are disabled by default for now until there's some sort of HLE
    # firmware that will allow us to ship everything they need with WashingtonDC
    enable_testing()
    configure_file("regression_tests/sh4div_test.pl" "sh4div_test.pl" COPYONLY)
    add_test(NAME sh4div_test COMMAND ./sh4div_test.pl)
    configure_file("regression_tests/sh4tmu_test.pl" "sh4tmu_test.pl" COPYONLY)
    add_test(NAME sh4tmu_test COMMAND ./sh4tmu_test.pl)
endif()

# zlib version 1.2.11
set(zlib_path "${CMAKE_SOURCE_DIR}/external/zlib")
add_library(zlib "${zlib_path}/zlib.h"
                 "${zlib_path}/crc32.h"
                 "${zlib_path}/deflate.h"
                 "${zlib_path}/gzguts.h"
                 "${zlib_path}/inffast.h"
                 "${zlib_path}/inffixed.h"
                 "${zlib_path}/inflate.h"
                 "${zlib_path}/inftrees.h"
                 "${zlib_path}/trees.h"
                 "${zlib_path}/zutil.h"
                 "${zlib_path}/adler32.c"
                 "${zlib_path}/compress.c"
                 "${zlib_path}/crc32.c"
                 "${zlib_path}/deflate.c"
                 "${zlib_path}/gzclose.c"
                 "${zlib_path}/gzlib.c"
                 "${zlib_path}/gzlib.c"
                 "${zlib_path}/gzread.c"
                 "${zlib_path}/gzwrite.c"
                 "${zlib_path}/inflate.c"
                 "${zlib_path}/infback.c"
                 "${zlib_path}/inftrees.c"
                 "${zlib_path}/inffast.c"
                 "${zlib_path}/trees.c"
                 "${zlib_path}/uncompr.c"
                 "${zlib_path}/zutil.c")

# libpng version 1.6.34
set(libpng_path "${CMAKE_SOURCE_DIR}/external/libpng")
add_library(png "${libpng_path}/png.h"
                "${libpng_path}/pngconf.h"
                "${libpng_path}/pngdebug.h"
                "${libpng_path}/pnginfo.h"
                "${libpng_path}/pnglibconf.h"
                "${libpng_path}/pngpriv.h"
                "${libpng_path}/pngstruct.h"
                "${libpng_path}/png.c"
                "${libpng_path}/pngerror.c"
                "${libpng_path}/pngget.c"
                "${libpng_path}/pngmem.c"
                "${libpng_path}/pngpread.c"
                "${libpng_path}/pngread.c"
                "${libpng_path}/pngrio.c"
                "${libpng_path}/pngrtran.c"
                "${libpng_path}/pngrutil.c"
                "${libpng_path}/pngset.c"
                "${libpng_path}/pngtrans.c"
                "${libpng_path}/pngwio.c"
                "${libpng_path}/pngwrite.c"
                "${libpng_path}/pngwtran.c"
                "${libpng_path}/pngwutil.c")

target_include_directories(png PRIVATE "${zlib_path}")

if (BUILD_WASHINGTONDC)
    # glew version 2.1.0
    add_definitions(-DGLEW_STATIC)
    set(glew_path "${CMAKE_SOURCE_DIR}/external/glew")
    add_library(glew "${glew_path}/src/glew.c"
                     "${glew_path}/include/GL/glew.h"
                     "${glew_path}/include/GL/wglew.h"
                     "${glew_path}/include/GL/glxew.h")
    target_include_directories(glew PRIVATE "${glew_path}/include")
endif()

set(include_dirs "${OPENGL_INCLUDE_DIR}"
                 "${CMAKE_SOURCE_DIR}/external/glew/include"
                 "${CMAKE_SOURCE_DIR}/external"
                 "${CMAKE_SOURCE_DIR}/external/glfw/include"
                 "${libpng_path}"
                 "${zlib_path}"
                 "${PROJECT_SOURCE_DIR}/external/portaudio/include"
                 "${PROJECT_SOURCE_DIR}/external/sh4asm")

if (ENABLE_DEBUGGER)
  set(include_dirs "${include_dirs}"
                   "${PROJECT_SOURCE_DIR}/external/capstone/include")
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# XXX It's important that glfw comes after libevent because they both define a
# target called "uninstall".  libevent defines its uninstall target
# unconditionally; glfw defines its target only if that target does not exist
# yet.
if (WIN32)
    # don't let portaudio use DirectX Sound as its backend - it will fail to
    # compile on MinGW
    set(PA_USE_DS OFF CACHE BOOL "" FORCE)
endif()
add_subdirectory(external/portaudio EXCLUDE_FROM_ALL)
if (USE_LIBEVENT)
    set(EVENT__DISABLE_TESTS ON CACHE BOOL "" FORCE)
    set(EVENT__DISABLE_REGRESS ON CACHE BOOL "" FORCE)
    set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "" FORCE)
    set(EVENT__DISABLE_OPENSSL ON CACHE BOOL "" FORCE)
    set(EVENT__LIBRARY_TYPE STATIC CACHE STRING "" FORCE)
    set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "" FORCE)
    add_subdirectory(external/libevent EXCLUDE_FROM_ALL)
endif()

if (BUILD_WASHINGTONDC)
    add_subdirectory(external/glfw EXCLUDE_FROM_ALL)
endif()

if (ENABLE_DEBUGGER OR JIT_PROFILE)
    set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${PROJECT_SOURCE_DIR}/external/capstone" EXCLUDE_FROM_ALL)
endif()

add_subdirectory(src)
