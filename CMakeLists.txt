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

set(CMAKE_LEGACY_CYGWIN_WIN32 0) # Remove when CMake >= 2.8.4 is required
cmake_minimum_required(VERSION 2.6)
include(ExternalProject)

project(WashingtonDC C)
set(WashingtonDC_VERSION_MAJOR 0)
set(WashingtonDC_VERSION_MINOR 1)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")

set (LIBEVENT_LIB_PATH ${CMAKE_CURRENT_BINARY_DIR})

if(NOT CMAKE_BUILD_TYPE)
       set(CMAKE_BUILD_TYPE Release)
       message("Setting CMAKE_BUILD_TYPE to Release by default")
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(external/glfw)

add_subdirectory(external/portaudio)

# i need this for strdup and clock_gettime on Debian for some unknown reason
add_definitions(-D_POSIX_C_SOURCE=200809L)

# While I prefer to avoid using GNU-specific stuff when possible, in general
# I don't give a damn about portability to Windows
add_definitions(-D_GNU_SOURCE)

enable_testing()

find_package(OpenGL REQUIRED)

# turn on strict warnings - i have no idea how to do this in windows
if(UNIX)
    # the -Wno-format-trunction is there to prevent gcc from raising frivolous
    # warnings whenever there's a chance that snprintf might do the one thing
    # it exists to do.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wno-format-truncation")
endif()

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
option(JIT_OPTIMIZE "enable optimization passes on the JIT that dont actually work" OFF)
option(ENABLE_TCP_CMD "enable remote command prompt over tcp port 2000" ON)
option(ENABLE_TCP_SERIAL "enable serial server emulator over tcp port 1998" ON)
option(USE_LIBEVENT "use libevent for asynchronous I/O processing" ON)

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

if (ENABLE_TCP_CMD)
    add_definitions(-DENABLE_TCP_CMD)
    if (NOT USE_LIBEVENT)
        message(FATAL_ERROR "-DUSE_LIBEVENT=On is a prerequisite for -DENABLE_TCP_CMD=On")
    endif()
endif()

if (ENABLE_TCP_SERIAL)
    add_definitions(-DENABLE_TCP_SERIAL)
    if (NOT USE_LIBEVENT)
        message(FATAL_ERROR "-DUSE_LIBEVENT=On is a prerequisite for -DENABLE_TCP_SERIAL=On")
    endif()
endif()

set(libwashdc_sources "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4.c"
		      "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_excp.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_excp.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_inst.h"
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
                      "${PROJECT_SOURCE_DIR}/src/types.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_ocache.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_ocache.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_icache.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_icache.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/g1/g1.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/g1/g1.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/g1/g1_reg.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/g1/g1_reg.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/g2/g2.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/g2/g2.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/g2/external_dev.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/g2/external_dev.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/g2/g2_reg.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/g2/g2_reg.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/gdrom/gdrom.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/gdrom/gdrom.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/gdrom/gdrom_response.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/gdrom/gdrom_response.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/g2/modem.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/g2/modem.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_reg.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_reg.c"
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
                      "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_rtc.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_rtc.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_wave_mem.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/aica/aica_wave_mem.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/aica/aica.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/aica/aica.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/boot_rom.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/boot_rom.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/flash_mem.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/flash_mem.c"
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
                      "${PROJECT_SOURCE_DIR}/src/glfw/control_bind.c"
                      "${PROJECT_SOURCE_DIR}/src/glfw/control_bind.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/pvr2/framebuffer.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/pvr2/framebuffer.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_gfx_obj.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_gfx_obj.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_yuv.c"
                      "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2_yuv.h"
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
                      "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_jit.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_jit.c"
                      "${PROJECT_SOURCE_DIR}/src/ring.h"
                      "${PROJECT_SOURCE_DIR}/src/config.h"
                      "${PROJECT_SOURCE_DIR}/src/config.c"
                      "${PROJECT_SOURCE_DIR}/src/cmd/cons.h"
                      "${PROJECT_SOURCE_DIR}/src/cmd/cons.c"
                      "${PROJECT_SOURCE_DIR}/src/gfx/gfx_config.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/gfx_config.c"
                      "${PROJECT_SOURCE_DIR}/src/gfx/gfx_tex_cache.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/gfx_tex_cache.c"
                      "${PROJECT_SOURCE_DIR}/src/log.h"
                      "${PROJECT_SOURCE_DIR}/src/log.c"
                      "${PROJECT_SOURCE_DIR}/src/mmio.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/pvr2/pvr2.c"
                      "${PROJECT_SOURCE_DIR}/src/jit/jit_il.h"
                      "${PROJECT_SOURCE_DIR}/src/jit/jit_il.c"
                      "${PROJECT_SOURCE_DIR}/src/jit/code_block.h"
                      "${PROJECT_SOURCE_DIR}/src/jit/code_block.c"
                      "${PROJECT_SOURCE_DIR}/src/jit/code_cache.c"
                      "${PROJECT_SOURCE_DIR}/src/jit/code_cache.h"
                      "${PROJECT_SOURCE_DIR}/src/jit/jit.h"
                      "${PROJECT_SOURCE_DIR}/src/jit/jit.c"
                      "${PROJECT_SOURCE_DIR}/src/jit/jit_mem.h"
                      "${PROJECT_SOURCE_DIR}/src/jit/jit_mem.c"
                      "${PROJECT_SOURCE_DIR}/src/jit/jit_intp/code_block_intp.h"
                      "${PROJECT_SOURCE_DIR}/src/jit/jit_intp/code_block_intp.c"
                      "${PROJECT_SOURCE_DIR}/src/gfx/gfx_il.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/gfx_obj.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/gfx_obj.c"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/font/digit_0.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/font/digit_1.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/font/digit_2.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/font/digit_3.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/font/digit_4.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/font/digit_5.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/font/digit_6.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/font/digit_7.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/font/digit_8.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/font/digit_9.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/font/dot.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/font/slash.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/font/space.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/font/font.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/font/font.c"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/overlay.h"
                      "${PROJECT_SOURCE_DIR}/src/gfx/opengl/overlay.c"
                      "${PROJECT_SOURCE_DIR}/src/avl.h"
                      "${PROJECT_SOURCE_DIR}/src/intmath.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/arm7/arm7.h"
                      "${PROJECT_SOURCE_DIR}/src/hw/arm7/arm7.c"
                      "${PROJECT_SOURCE_DIR}/src/pix_conv.h"
                      "${PROJECT_SOURCE_DIR}/src/pix_conv.c"
                      "${PROJECT_SOURCE_DIR}/src/title.h"
                      "${PROJECT_SOURCE_DIR}/src/title.c"
                      "${PROJECT_SOURCE_DIR}/src/cpu.h"
                      "${PROJECT_SOURCE_DIR}/src/config_file.h"
                      "${PROJECT_SOURCE_DIR}/src/config_file.c"
                      "${PROJECT_SOURCE_DIR}/src/hostfile.h"
                      "${PROJECT_SOURCE_DIR}/src/hostfile.c"
                      "${PROJECT_SOURCE_DIR}/src/screenshot.h"
                      "${PROJECT_SOURCE_DIR}/src/screenshot.c"
		      "${PROJECT_SOURCE_DIR}/src/sound/sound.h"
                      "${PROJECT_SOURCE_DIR}/src/sound/sound.c"
                      "${PROJECT_SOURCE_DIR}/src/washdc.h"
                      "${PROJECT_SOURCE_DIR}/src/washdc.c")

if (ENABLE_TCP_CMD)
    set (libwashdc_sources ${libwashdc_sources} "${PROJECT_SOURCE_DIR}/src/cmd/cmd.h"
                                    "${PROJECT_SOURCE_DIR}/src/cmd/cmd.c"
                                    "${PROJECT_SOURCE_DIR}/src/io/cmd_tcp.h"
                                    "${PROJECT_SOURCE_DIR}/src/io/cmd_tcp.c"
                                    "${PROJECT_SOURCE_DIR}/src/cmd/cmd_sys.h"
                                    "${PROJECT_SOURCE_DIR}/src/cmd/cmd_sys.c"
                                    "${PROJECT_SOURCE_DIR}/src/cmd/cmd_tcp_link.h"
                                    "${PROJECT_SOURCE_DIR}/src/cmd/cmd_tcp_link.c")
endif()

if (ENABLE_TCP_SERIAL)
    set (libwashdc_sources ${libwashdc_sources} "${PROJECT_SOURCE_DIR}/src/io/serial_server.h"
                                    "${PROJECT_SOURCE_DIR}/src/io/serial_server.c")
endif()

set(washingtondc_sources "${PROJECT_SOURCE_DIR}/src/main.c")

if (JIT_OPTIMIZE)
   add_definitions(-DJIT_OPTIMIZE)
   set (libwashdc_sources ${libwashdc_sources} "${PROJECT_SOURCE_DIR}/src/jit/jit_determ.h"
                                   "${PROJECT_SOURCE_DIR}/src/jit/jit_determ.c")

endif()

if (ENABLE_JIT_X86_64)
   add_definitions(-DENABLE_JIT_X86_64)
   set(libwashdc_sources ${libwashdc_sources} "${PROJECT_SOURCE_DIR}/src/jit/x86_64/code_block_x86_64.h"
                                  "${PROJECT_SOURCE_DIR}/src/jit/x86_64/code_block_x86_64.c"
                                  "${PROJECT_SOURCE_DIR}/src/jit/x86_64/emit_x86_64.h"
                                  "${PROJECT_SOURCE_DIR}/src/jit/x86_64/emit_x86_64.c"
                                  "${PROJECT_SOURCE_DIR}/src/jit/x86_64/exec_mem.h"
                                  "${PROJECT_SOURCE_DIR}/src/jit/x86_64/exec_mem.c"
                                  "${PROJECT_SOURCE_DIR}/src/jit/x86_64/native_dispatch.h"
                                  "${PROJECT_SOURCE_DIR}/src/jit/x86_64/native_dispatch.c"
                                  "${PROJECT_SOURCE_DIR}/src/jit/x86_64/native_mem.h"
                                  "${PROJECT_SOURCE_DIR}/src/jit/x86_64/native_mem.c"
                                  "${PROJECT_SOURCE_DIR}/src/jit/x86_64/abi.h")
endif()

if (USE_LIBEVENT)
    set(libwashdc_sources ${libwashdc_sources}
        "${PROJECT_SOURCE_DIR}/src/io/io_thread.h"
        "${PROJECT_SOURCE_DIR}/src/io/io_thread.c")
endif()


if (ENABLE_DEBUGGER)

    if (NOT USE_LIBEVENT)
        message(FATAL_ERROR "-DUSE_LIBEVENT=On is a prerequisite for -DENABLE_DEBUGGER=On")
    endif()

    set(CAPSTONE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)

    set(CAPSTONE_ARM_SUPPORT ON CACHE BOOL "" FORCE)
    set(CAPSTONE_ARM64_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_M68K_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_MIPS_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_PPC_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_SPARC_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_SYSZ_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_XCORE_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_X86_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_TMS320C64X_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_M680X_SUPPORT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_EVM_SUPPORT OFF CACHE BOOL "" FORCE)

    add_subdirectory(external/capstone)

    add_definitions(-DENABLE_DEBUGGER)
    set(libwashdc_sources ${libwashdc_sources} "${PROJECT_SOURCE_DIR}/src/dbg/debugger.h"
                                   "${PROJECT_SOURCE_DIR}/src/dbg/debugger.c"
                                   "${PROJECT_SOURCE_DIR}/src/io/gdb_stub.h"
                                   "${PROJECT_SOURCE_DIR}/src/io/gdb_stub.c"
                                   "${PROJECT_SOURCE_DIR}/src/io/washdbg_tcp.h"
                                   "${PROJECT_SOURCE_DIR}/src/io/washdbg_tcp.c"
                                   "${PROJECT_SOURCE_DIR}/src/dbg/washdbg_core.h"
                                   "${PROJECT_SOURCE_DIR}/src/dbg/washdbg_core.c"
                                   "${PROJECT_SOURCE_DIR}/external/sh4asm/sh4asm_core/disas.h"
                                   "${PROJECT_SOURCE_DIR}/external/sh4asm/sh4asm_core/disas.c"
                                   "${PROJECT_SOURCE_DIR}/external/sh4asm/sh4asm_core/sh4_asm_emit.h"
                                   "${PROJECT_SOURCE_DIR}/external/sh4asm/sh4asm_core/sh4_asm_emit.c")

    if (ENABLE_WATCHPOINTS)
        add_definitions(-DENABLE_WATCHPOINTS)
    endif()

    if (ENABLE_DBG_COND)
        add_definitions(-DENABLE_DBG_COND)
    endif()
endif()

if (DEEP_SYSCALL_TRACE)
   set(libwashdc_sources ${libwashdc_sources}
                   "${PROJECT_SOURCE_DIR}/src/deep_syscall_trace.h"
                   "${PROJECT_SOURCE_DIR}/src/deep_syscall_trace.c")
endif()

add_library(washdc ${libwashdc_sources})

# libpng version 1.6.34
set(libpng_path "${PROJECT_SOURCE_DIR}/external/libpng")
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

# zlib version 1.2.11
set(zlib_path "${PROJECT_SOURCE_DIR}/external/zlib")
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

# glew version 2.1.0
set(glew_path "${PROJECT_SOURCE_DIR}/external/glew")
add_library(glew "${glew_path}/src/glew.c"
                 "${glew_path}/include/GL/glew.h"
                 "${glew_path}/include/GL/wglew.h"
                 "${glew_path}/include/GL/glxew.h")

set(include_dirs "${OPENGL_INCLUDE_DIR}"
                 "${PROJECT_SOURCE_DIR}/external/glew/include"
                 "${PROJECT_SOURCE_DIR}/src/"
                 "${PROJECT_SOURCE_DIR}/src/hw/sh4"
                 "${PROJECT_SOURCE_DIR}/common"
                 "${PROJECT_SOURCE_DIR}/external"
                 "${PROJECT_SOURCE_DIR}/external/glfw/include"
                 "${libpng_path}"
                 "${zlib_path}"
                 "${PROJECT_SOURCE_DIR}/external/portaudio/include")

if (ENABLE_DEBUGGER)
  set(include_dirs "${include_dirs}"
                   "${PROJECT_SOURCE_DIR}/external/sh4asm"
                   "${PROJECT_SOURCE_DIR}/external/capstone/include")
endif()

configure_file("regression_tests/sh4div_test.pl" "sh4div_test.pl" COPYONLY)
add_test(NAME sh4div_test COMMAND ./sh4div_test.pl)

configure_file("regression_tests/sh4tmu_test.pl" "sh4tmu_test.pl" COPYONLY)
add_test(NAME sh4tmu_test COMMAND ./sh4tmu_test.pl)

add_executable(washingtondc ${washingtondc_sources})

set(washingtondc_libs "m"
                      "washdc"
                      "rt"
                      "png"
                      "zlib"
                      "glfw"
                      "glew"
                      "${OPENGL_gl_LIBRARY}"
                      "pthread"
                      "portaudio")

if (USE_LIBEVENT)
    add_definitions(-DUSE_LIBEVENT)
    ExternalProject_Add(libevent
                        PREFIX washingtondc-libevent
                        GIT_REPOSITORY https://github.com/libevent/libevent
                        GIT_TAG release-2.1.8-stable
                        INSTALL_DIR ${LIBEVENT_LIB_PATH}
                        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBEVENT_LIB_PATH} -DEVENT__DISABLE_TESTS=On -DEVENT__DISABLE_REGRESS=On -DEVENT__DISABLE_BENCHMARK=On -DEVENT__DISABLE_OPENSSL=On -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                        BUILD_IN_SOURCE 1)

    set(washingtondc_libs "${washingtondc_libs}" "${LIBEVENT_LIB_PATH}/lib/libevent.a")
    set(include_dirs "${include_dirs}"
                     "${CMAKE_BINARY_DIR}/washingtondc-libevent/src/libevent/include")
    add_dependencies(washingtondc libevent sh4)
    add_dependencies(washdc libevent)
endif()

include_directories("${include_dirs}")

if (ENABLE_DEBUGGER)
    set(washingtondc_libs "${washingtondc_libs}" capstone-static)
endif()

target_link_libraries(washingtondc "${washingtondc_libs}")
