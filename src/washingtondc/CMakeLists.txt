################################################################################
#
#
#    WashingtonDC Dreamcast Emulator
#    Copyright (C) 2016-2019 snickerbockers
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

#set(CMAKE_LEGACY_CYGWIN_WIN32 0) # Remove when CMake >= 2.8.4 is required
cmake_minimum_required(VERSION 2.6)

project(WashingtonDC CXX)
set(WashingtonDC_VERSION_MAJOR 0)
set(WashingtonDC_VERSION_MINOR 1)

find_package(OpenGL REQUIRED)
if (WIN32)
    find_library(WS2_32_LIBRARY ws2_32)
endif()

set(UI_SOURCE_DIR "${PROJECT_SOURCE_DIR}/ui")
set(ui_sources "${UI_SOURCE_DIR}/overlay.hpp"
               "${UI_SOURCE_DIR}/overlay.cpp"
               "${UI_SOURCE_DIR}/renderer.hpp"
               "${UI_SOURCE_DIR}/renderer.cpp")

set(IMGUI_SOURCE_DIR "${CMAKE_SOURCE_DIR}/external/imgui")
set(imgui_sources "${IMGUI_SOURCE_DIR}/imgui.cpp"
                  "${IMGUI_SOURCE_DIR}/imgui.h"
                  "${IMGUI_SOURCE_DIR}/imgui_demo.cpp"
                  "${IMGUI_SOURCE_DIR}/imgui_draw.cpp"
                  "${IMGUI_SOURCE_DIR}/imgui_widgets.cpp"
                  "${IMGUI_SOURCE_DIR}/imgui_internal.h"
                  "${IMGUI_SOURCE_DIR}/imconfig.h"
                  "${IMGUI_SOURCE_DIR}/imstb_rectpack.h"
                  "${IMGUI_SOURCE_DIR}/imstb_textedit.h"
                  "${IMGUI_SOURCE_DIR}/imstb_truetype.h")

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

set(serial_sources "${IO_SOURCE_DIR}/serial_server.hpp"
                   "${IO_SOURCE_DIR}/serial_server.cpp")

set(gfxgl_sources "${PROJECT_SOURCE_DIR}/opengl/shader.h"
                  "${PROJECT_SOURCE_DIR}/opengl/shader.c"
                  "${PROJECT_SOURCE_DIR}/opengl/shader_cache.h"
                  "${PROJECT_SOURCE_DIR}/opengl/shader_cache.c"
                  "${PROJECT_SOURCE_DIR}/opengl/opengl_output.h"
                  "${PROJECT_SOURCE_DIR}/opengl/opengl_output.c"
                  "${PROJECT_SOURCE_DIR}/opengl/opengl_target.h"
                  "${PROJECT_SOURCE_DIR}/opengl/opengl_target.c"
                  "${PROJECT_SOURCE_DIR}/opengl/opengl_renderer.h"
                  "${PROJECT_SOURCE_DIR}/opengl/opengl_renderer.c")


set(washingtondc_sources "${PROJECT_SOURCE_DIR}/main.cpp"
                         "${PROJECT_SOURCE_DIR}/washingtondc.hpp"
                         "${PROJECT_SOURCE_DIR}/window.cpp"
                         "${PROJECT_SOURCE_DIR}/window.hpp"
                         "${PROJECT_SOURCE_DIR}/control_bind.cpp"
                         "${PROJECT_SOURCE_DIR}/control_bind.hpp"
                         "${PROJECT_SOURCE_DIR}/sound.hpp"
                         "${PROJECT_SOURCE_DIR}/sound.cpp"
                         "${PROJECT_SOURCE_DIR}/console_config.hpp"
                         "${PROJECT_SOURCE_DIR}/console_config.cpp"
                         "${gfxgl_sources}")

if (ENABLE_TCP_SERIAL)
    add_definitions(-DENABLE_TCP_SERIAL)
    if (NOT USE_LIBEVENT)
        message(FATAL_ERROR "-DUSE_LIBEVENT=On is a prerequisite for -DENABLE_TCP_SERIAL=On")
    endif()
    set (washingtondc_sources ${washingtondc_sources} ${serial_sources})
endif()

set(need_capstone "FALSE")
set(need_sh4asm "FALSE")

if (JIT_PROFILE)
    set(need_capstone "TRUE")
    set(need_sh4asm "TRUE")
endif()

if (ENABLE_DEBUGGER)
    if (NOT USE_LIBEVENT)
        message(FATAL_ERROR "-DUSE_LIBEVENT=On is a prerequisite for -DENABLE_DEBUGGER=On")
    endif()

    set(need_capstone "TRUE")
    set(need_sh4asm "TRUE")

    set(washingtondc_sources ${washingtondc_sources} ${debug_sources})

    add_definitions(-DENABLE_DEBUGGER)
    if (ENABLE_WATCHPOINTS)
        add_definitions(-DENABLE_WATCHPOINTS)
    endif()

    if (ENABLE_DBG_COND)
        add_definitions(-DENABLE_DBG_COND)
    endif()
endif()

if (need_sh4asm STREQUAL "TRUE")
    set(washingtondc_sources ${washingtondc_sources} ${sh4asm_sources})
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
endif()

if (USE_LIBEVENT)
    set(washingtondc_sources "${washingtondc_sources}" "${io_sources}")
endif()

add_executable(washingtondc ${washingtondc_sources} ${ui_sources}
                            ${imgui_sources})

add_definitions(-DGLEW_STATIC)

set(vid_libs "glfw" "glew" "${OPENGL_gl_LIBRARY}")

if (NOT WIN32)
    set(vid_libs "rt" "${vid_libs}")
endif()

set(washingtondc_libs "m"
                      "washdc"
                      "png"
                      "${vid_libs}"
                      "zlib"
                      "portaudio_static")

if (USE_LIBEVENT)
    set(washingtondc_libs "${washingtondc_libs}" "event_core")
    if (WIN32)
         set(washingtondc_libs "${washingtondc_libs}" "${WS2_32_LIBRARY}")
    else()
        set(washingtondc_libs "${washingtondc_libs}" "pthread" "event_pthreads")
    endif()
endif()

if (need_capstone STREQUAL "TRUE")
    set(washingtondc_libs "${washingtondc_libs}" capstone-static)
endif()

target_include_directories(washingtondc PRIVATE "${WASHDC_SOURCE_DIR}/include"
                                                "${CMAKE_SOURCE_DIR}/src/common"
                                                "${CMAKE_SOURCE_DIR}/src/libwashdc/include"
                                                "${PROJECT_SOURCE_DIR}/ui"
                                                "${IMGUI_SOURCE_DIR}"
                                                "${CMAKE_SOURCE_DIR}/external/sh4asm"
                                                "${CMAKE_SOURCE_DIR}/external/capstone/include"
                                                "${CMAKE_SOURCE_DIR}/external/glew/include"
                                                "${CMAKE_SOURCE_DIR}/external/libevent/include"
                                                "${CMAKE_BINARY_DIR}/external/libevent/include")
target_link_libraries(washingtondc "${washingtondc_libs}")
