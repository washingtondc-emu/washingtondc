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

set(WASHDC_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src/washdc")

set(libwashdc_sources "${WASHDC_SOURCE_DIR}/hw/sh4/sh4.c"
		      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_excp.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_excp.c"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_inst.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_inst.c"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_read_inst.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_mem.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_mem.c"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_reg.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_reg_flags.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_reg.c"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_scif.c"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_scif.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_tmu.c"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_tmu.h"
                      "${WASHDC_SOURCE_DIR}/types.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_ocache.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_ocache.c"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_icache.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_icache.c"
                      "${WASHDC_SOURCE_DIR}/hw/g1/g1.h"
                      "${WASHDC_SOURCE_DIR}/hw/g1/g1.c"
                      "${WASHDC_SOURCE_DIR}/hw/g1/g1_reg.h"
                      "${WASHDC_SOURCE_DIR}/hw/g1/g1_reg.c"
                      "${WASHDC_SOURCE_DIR}/hw/g2/g2.h"
                      "${WASHDC_SOURCE_DIR}/hw/g2/g2.c"
                      "${WASHDC_SOURCE_DIR}/hw/g2/external_dev.h"
                      "${WASHDC_SOURCE_DIR}/hw/g2/external_dev.c"
                      "${WASHDC_SOURCE_DIR}/hw/g2/g2_reg.h"
                      "${WASHDC_SOURCE_DIR}/hw/g2/g2_reg.c"
                      "${WASHDC_SOURCE_DIR}/hw/gdrom/gdrom.h"
                      "${WASHDC_SOURCE_DIR}/hw/gdrom/gdrom.c"
                      "${WASHDC_SOURCE_DIR}/hw/gdrom/gdrom_response.h"
                      "${WASHDC_SOURCE_DIR}/hw/gdrom/gdrom_response.c"
                      "${WASHDC_SOURCE_DIR}/hw/g2/modem.h"
                      "${WASHDC_SOURCE_DIR}/hw/g2/modem.c"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_reg.h"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_reg.c"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_tex_mem.h"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_tex_mem.c"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/spg.c"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/spg.h"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_ta.c"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_ta.h"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_tex_cache.c"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_tex_cache.h"
                      "${WASHDC_SOURCE_DIR}/hw/sys/sys_block.c"
                      "${WASHDC_SOURCE_DIR}/hw/sys/sys_block.h"
                      "${WASHDC_SOURCE_DIR}/hw/sys/holly_intc.c"
                      "${WASHDC_SOURCE_DIR}/hw/sys/holly_intc.h"
                      "${WASHDC_SOURCE_DIR}/hw/maple/maple.h"
                      "${WASHDC_SOURCE_DIR}/hw/maple/maple.c"
                      "${WASHDC_SOURCE_DIR}/hw/maple/maple_device.h"
                      "${WASHDC_SOURCE_DIR}/hw/maple/maple_device.c"
                      "${WASHDC_SOURCE_DIR}/hw/maple/maple_controller.h"
                      "${WASHDC_SOURCE_DIR}/hw/maple/maple_controller.c"
                      "${WASHDC_SOURCE_DIR}/hw/maple/maple_reg.h"
                      "${WASHDC_SOURCE_DIR}/hw/maple/maple_reg.c"
                      "${WASHDC_SOURCE_DIR}/hw/aica/aica_rtc.h"
                      "${WASHDC_SOURCE_DIR}/hw/aica/aica_rtc.c"
                      "${WASHDC_SOURCE_DIR}/hw/aica/aica_wave_mem.h"
                      "${WASHDC_SOURCE_DIR}/hw/aica/aica_wave_mem.c"
                      "${WASHDC_SOURCE_DIR}/hw/aica/aica.h"
                      "${WASHDC_SOURCE_DIR}/hw/aica/aica.c"
                      "${WASHDC_SOURCE_DIR}/hw/boot_rom.h"
                      "${WASHDC_SOURCE_DIR}/hw/boot_rom.c"
                      "${WASHDC_SOURCE_DIR}/hw/flash_mem.h"
                      "${WASHDC_SOURCE_DIR}/hw/flash_mem.c"
                      "${WASHDC_SOURCE_DIR}/mem_code.h"
                      "${WASHDC_SOURCE_DIR}/memory.h"
                      "${WASHDC_SOURCE_DIR}/memory.c"
                      "${WASHDC_SOURCE_DIR}/MemoryMap.h"
                      "${WASHDC_SOURCE_DIR}/MemoryMap.c"
                      "${WASHDC_SOURCE_DIR}/dreamcast.h"
                      "${WASHDC_SOURCE_DIR}/dreamcast.c"
                      "${WASHDC_SOURCE_DIR}/dc_sched.h"
                      "${WASHDC_SOURCE_DIR}/dc_sched.c"
                      "${WASHDC_SOURCE_DIR}/glfw/window.c"
                      "${WASHDC_SOURCE_DIR}/glfw/window.h"
                      "${WASHDC_SOURCE_DIR}/glfw/control_bind.c"
                      "${WASHDC_SOURCE_DIR}/glfw/control_bind.h"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/framebuffer.c"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/framebuffer.h"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_gfx_obj.c"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_gfx_obj.h"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_yuv.c"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_yuv.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/shader.c"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/shader.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/opengl_output.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/opengl_output.c"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/opengl_target.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/opengl_target.c"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/opengl_renderer.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/opengl_renderer.c"
                      "${WASHDC_SOURCE_DIR}/gfx/rend_common.h"
                      "${WASHDC_SOURCE_DIR}/gfx/rend_common.c"
                      "${WASHDC_SOURCE_DIR}/gfx/gfx.h"
                      "${WASHDC_SOURCE_DIR}/gfx/gfx.c"
                      "${WASHDC_SOURCE_DIR}/error.c"
                      "${WASHDC_SOURCE_DIR}/error.h"
                      "${WASHDC_SOURCE_DIR}/mem_areas.h"
                      "${WASHDC_SOURCE_DIR}/stringlib.h"
                      "${WASHDC_SOURCE_DIR}/stringlib.c"
                      "${WASHDC_SOURCE_DIR}/fifo.h"
                      "${WASHDC_SOURCE_DIR}/gdi.h"
                      "${WASHDC_SOURCE_DIR}/gdi.c"
                      "${WASHDC_SOURCE_DIR}/mount.h"
                      "${WASHDC_SOURCE_DIR}/mount.c"
                      "${WASHDC_SOURCE_DIR}/cdrom.h"
                      "${WASHDC_SOURCE_DIR}/cdrom.c"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_dmac.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_dmac.c"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_tbl.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_tbl.c"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_jit.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_jit.c"
                      "${WASHDC_SOURCE_DIR}/ring.h"
                      "${WASHDC_SOURCE_DIR}/config.h"
                      "${WASHDC_SOURCE_DIR}/config.c"
                      "${WASHDC_SOURCE_DIR}/cmd/cons.h"
                      "${WASHDC_SOURCE_DIR}/cmd/cons.c"
                      "${WASHDC_SOURCE_DIR}/gfx/gfx_config.h"
                      "${WASHDC_SOURCE_DIR}/gfx/gfx_config.c"
                      "${WASHDC_SOURCE_DIR}/gfx/gfx_tex_cache.h"
                      "${WASHDC_SOURCE_DIR}/gfx/gfx_tex_cache.c"
                      "${WASHDC_SOURCE_DIR}/log.h"
                      "${WASHDC_SOURCE_DIR}/log.c"
                      "${WASHDC_SOURCE_DIR}/mmio.h"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2.h"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2.c"
                      "${WASHDC_SOURCE_DIR}/jit/jit_il.h"
                      "${WASHDC_SOURCE_DIR}/jit/jit_il.c"
                      "${WASHDC_SOURCE_DIR}/jit/code_block.h"
                      "${WASHDC_SOURCE_DIR}/jit/code_block.c"
                      "${WASHDC_SOURCE_DIR}/jit/code_cache.c"
                      "${WASHDC_SOURCE_DIR}/jit/code_cache.h"
                      "${WASHDC_SOURCE_DIR}/jit/jit.h"
                      "${WASHDC_SOURCE_DIR}/jit/jit.c"
                      "${WASHDC_SOURCE_DIR}/jit/jit_mem.h"
                      "${WASHDC_SOURCE_DIR}/jit/jit_mem.c"
                      "${WASHDC_SOURCE_DIR}/jit/jit_intp/code_block_intp.h"
                      "${WASHDC_SOURCE_DIR}/jit/jit_intp/code_block_intp.c"
                      "${WASHDC_SOURCE_DIR}/gfx/gfx_il.h"
                      "${WASHDC_SOURCE_DIR}/gfx/gfx_obj.h"
                      "${WASHDC_SOURCE_DIR}/gfx/gfx_obj.c"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/font/digit_0.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/font/digit_1.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/font/digit_2.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/font/digit_3.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/font/digit_4.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/font/digit_5.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/font/digit_6.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/font/digit_7.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/font/digit_8.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/font/digit_9.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/font/dot.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/font/slash.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/font/space.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/font/font.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/font/font.c"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/overlay.h"
                      "${WASHDC_SOURCE_DIR}/gfx/opengl/overlay.c"
                      "${WASHDC_SOURCE_DIR}/avl.h"
                      "${WASHDC_SOURCE_DIR}/intmath.h"
                      "${WASHDC_SOURCE_DIR}/hw/arm7/arm7.h"
                      "${WASHDC_SOURCE_DIR}/hw/arm7/arm7.c"
                      "${WASHDC_SOURCE_DIR}/pix_conv.h"
                      "${WASHDC_SOURCE_DIR}/pix_conv.c"
                      "${WASHDC_SOURCE_DIR}/title.h"
                      "${WASHDC_SOURCE_DIR}/title.c"
                      "${WASHDC_SOURCE_DIR}/cpu.h"
                      "${WASHDC_SOURCE_DIR}/config_file.h"
                      "${WASHDC_SOURCE_DIR}/config_file.c"
                      "${WASHDC_SOURCE_DIR}/hostfile.h"
                      "${WASHDC_SOURCE_DIR}/hostfile.c"
                      "${WASHDC_SOURCE_DIR}/screenshot.h"
                      "${WASHDC_SOURCE_DIR}/screenshot.c"
                      "${WASHDC_SOURCE_DIR}/sound/sound.h"
                      "${WASHDC_SOURCE_DIR}/sound/sound.c"
                      "${WASHDC_SOURCE_DIR}/washdc.c"
                      "${WASHDC_SOURCE_DIR}/include/washdc/washdc.h")

if (ENABLE_TCP_CMD)
    set (libwashdc_sources ${libwashdc_sources} "${WASHDC_SOURCE_DIR}/cmd/cmd.h"
                                                "${WASHDC_SOURCE_DIR}/cmd/cmd.c"
                                                "${WASHDC_SOURCE_DIR}/io/cmd_tcp.h"
                                                "${WASHDC_SOURCE_DIR}/io/cmd_tcp.c"
                                                "${WASHDC_SOURCE_DIR}/cmd/cmd_sys.h"
                                                "${WASHDC_SOURCE_DIR}/cmd/cmd_sys.c"
                                                "${WASHDC_SOURCE_DIR}/cmd/cmd_tcp_link.h"
                                                "${WASHDC_SOURCE_DIR}/cmd/cmd_tcp_link.c")
endif()

if (ENABLE_TCP_SERIAL)
    set (libwashdc_sources ${libwashdc_sources} "${WASHDC_SOURCE_DIR}/io/serial_server.h"
                                                "${WASHDC_SOURCE_DIR}/io/serial_server.c")
endif()

set(washingtondc_sources "${PROJECT_SOURCE_DIR}/src/main.c")

if (JIT_OPTIMIZE)
   add_definitions(-DJIT_OPTIMIZE)
   set (libwashdc_sources ${libwashdc_sources} "${WASHDC_SOURCE_DIR}/jit/jit_determ.h"
                                               "${WASHDC_SOURCE_DIR}/jit/jit_determ.c")

endif()

if (ENABLE_JIT_X86_64)
   add_definitions(-DENABLE_JIT_X86_64)
   set(libwashdc_sources ${libwashdc_sources} "${WASHDC_SOURCE_DIR}/jit/x86_64/code_block_x86_64.h"
                                              "${WASHDC_SOURCE_DIR}/jit/x86_64/code_block_x86_64.c"
                                              "${WASHDC_SOURCE_DIR}/jit/x86_64/emit_x86_64.h"
                                              "${WASHDC_SOURCE_DIR}/jit/x86_64/emit_x86_64.c"
                                              "${WASHDC_SOURCE_DIR}/jit/x86_64/exec_mem.h"
                                              "${WASHDC_SOURCE_DIR}/jit/x86_64/exec_mem.c"
                                              "${WASHDC_SOURCE_DIR}/jit/x86_64/native_dispatch.h"
                                              "${WASHDC_SOURCE_DIR}/jit/x86_64/native_dispatch.c"
                                              "${WASHDC_SOURCE_DIR}/jit/x86_64/native_mem.h"
                                              "${WASHDC_SOURCE_DIR}/jit/x86_64/native_mem.c"
                                              "${WASHDC_SOURCE_DIR}/jit/x86_64/abi.h")
endif()

if (USE_LIBEVENT)
    set(libwashdc_sources ${libwashdc_sources}
        "${WASHDC_SOURCE_DIR}/io/io_thread.h"
        "${WASHDC_SOURCE_DIR}/io/io_thread.c")
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
    set(libwashdc_sources ${libwashdc_sources} "${WASHDC_SOURCE_DIR}/dbg/debugger.h"
                                               "${WASHDC_SOURCE_DIR}/dbg/debugger.c"
                                               "${WASHDC_SOURCE_DIR}/io/gdb_stub.h"
                                               "${WASHDC_SOURCE_DIR}/io/gdb_stub.c"
                                               "${WASHDC_SOURCE_DIR}/io/washdbg_tcp.h"
                                               "${WASHDC_SOURCE_DIR}/io/washdbg_tcp.c"
                                               "${WASHDC_SOURCE_DIR}/dbg/washdbg_core.h"
                                               "${WASHDC_SOURCE_DIR}/dbg/washdbg_core.c"
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
                         "${WASHDC_SOURCE_DIR}/deep_syscall_trace.h"
                         "${WASHDC_SOURCE_DIR}/deep_syscall_trace.c")
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
                 "${WASHDC_SOURCE_DIR}/"
                 "${WASHDC_SOURCE_DIR}/hw/sh4"
                 "${PROJECT_SOURCE_DIR}/common"
                 "${PROJECT_SOURCE_DIR}/external"
                 "${PROJECT_SOURCE_DIR}/external/glfw/include"
		 "${WASHDC_SOURCE_DIR}/include"
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
    add_dependencies(washingtondc libevent washdc)
    add_dependencies(washdc libevent)
endif()

target_include_directories(washdc PRIVATE "${include_dirs}")
target_include_directories(washingtondc PRIVATE "${WASHDC_SOURCE_DIR}/include")

if (ENABLE_DEBUGGER)
    set(washingtondc_libs "${washingtondc_libs}" capstone-static)
endif()

target_link_libraries(washingtondc "${washingtondc_libs}")
