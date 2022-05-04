################################################################################
#
#    WashingtonDC Dreamcast Emulator
#    Copyright (C) 2016-2020, 2022  snickerbockers
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
#    along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
################################################################################

if (UNIX)
   include (GNUInstallDirs)
   set(README_DIR "${CMAKE_INSTALL_DOCDIR}")
   set(LICENSE_DIR "${CMAKE_INSTALL_DOCDIR}/licenses")
else()
   set(LICENSE_DIR "licenses")
   set(README_DIR ".")
endif()


set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_NAME WashingtonDC)
set(CPACK_PACKAGE_VENDOR snickerbockers)
set(CPACK_PACKAGE_DESCRIPTION "WashingtonDC Dreamcast Emulator")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://www.washemu.org")
set(CPACK_PACKAGE_VERSION_MAJOR 0)
set(CPACK_PACKAGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 0)

include(CPack)

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
option(WARNINGS_AS_ERRORS "enable compiler warnings as errors (unix only) OFF")

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

# CHD support
add_subdirectory(external/libchdr EXCLUDE_FROM_ALL)

add_subdirectory(src)

configure_file("${CMAKE_SOURCE_DIR}/COPYING" "COPYING" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/external/glfw/LICENSE.md" "LICENSE_glfw.txt" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/external/capstone/LICENSE.TXT" "LICENSE_capstone.txt" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/external/capstone/LICENSE_LLVM.TXT" "LICENSE_llvm.txt" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/external/libevent/LICENSE" "LICENSE_libevent.txt" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/external/glew/LICENSE.txt" "LICENSE_glew.txt" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/external/imgui/LICENSE.txt" "LICENSE_imgui.txt" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/external/portaudio/LICENSE.txt" "LICENSE_portaudio.txt" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/external/libchdr/LICENSE.txt" "LICENSE_libchdr.txt" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/external/libchdr/deps/lzma-19.00/LICENSE" "LICENSE_lzma.txt" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/external/external_licenses/zlib" "LICENSE_zlib.txt" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/external/external_licenses/libchdr_aaron_giles" "LICENSE_aaron_giles.txt" COPYONLY)

set(licenses "${CMAKE_CURRENT_BINARY_DIR}/COPYING"
             "${CMAKE_CURRENT_BINARY_DIR}/LICENSE_glfw.txt"
             "${CMAKE_CURRENT_BINARY_DIR}/LICENSE_capstone.txt"
             "${CMAKE_CURRENT_BINARY_DIR}/LICENSE_llvm.txt"
             "${CMAKE_CURRENT_BINARY_DIR}/LICENSE_libevent.txt"
             "${CMAKE_CURRENT_BINARY_DIR}/LICENSE_glew.txt"
             "${CMAKE_CURRENT_BINARY_DIR}/LICENSE_imgui.txt"
             "${CMAKE_CURRENT_BINARY_DIR}/LICENSE_portaudio.txt"
             "${CMAKE_CURRENT_BINARY_DIR}/LICENSE_libchdr.txt"
             "${CMAKE_CURRENT_BINARY_DIR}/LICENSE_lzma.txt"
             "${CMAKE_CURRENT_BINARY_DIR}/LICENSE_zlib.txt"
             "${CMAKE_CURRENT_BINARY_DIR}/LICENSE_aaron_giles.txt")

install(FILES ${licenses} DESTINATION ${LICENSE_DIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md  COPYING DESTINATION ${README_DIR})
