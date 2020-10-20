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
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
################################################################################

project(washdc C)
set(washdc_VERSION_MAJOR 0)
set(washdc_VERSION_MINOR 1)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")


if(NOT CMAKE_BUILD_TYPE)
       set(CMAKE_BUILD_TYPE Release)
       message("Setting CMAKE_BUILD_TYPE to Release by default")
endif()

# i need this for strdup and clock_gettime on Debian for some unknown reason
add_definitions(-D_POSIX_C_SOURCE=200809L)

# need this for MAP_ANONYMOUS on unix
if (NOT WIN32)
    add_definitions(-D_GNU_SOURCE)
endif()

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

if (ENABLE_TCP_SERIAL)
    add_definitions(-DENABLE_TCP_SERIAL)
endif()

set(WASHDC_SOURCE_DIR "${PROJECT_SOURCE_DIR}")

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
                      "${WASHDC_SOURCE_DIR}/include/washdc/hw/sh4/sh4_reg_idx.h"
                      "${WASHDC_SOURCE_DIR}/include/washdc/hw/arm7/arm7_reg_idx.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_reg_flags.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_reg.c"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_scif.c"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_scif.h"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_tmu.c"
                      "${WASHDC_SOURCE_DIR}/hw/sh4/sh4_tmu.h"
                      "${WASHDC_SOURCE_DIR}/include/washdc/types.h"
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
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_core.h"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_core.c"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_def.h"
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
                      "${WASHDC_SOURCE_DIR}/hw/maple/maple_keyboard.h"
                      "${WASHDC_SOURCE_DIR}/hw/maple/maple_keyboard.c"
                      "${WASHDC_SOURCE_DIR}/hw/maple/maple_reg.h"
                      "${WASHDC_SOURCE_DIR}/hw/maple/maple_reg.c"
                      "${WASHDC_SOURCE_DIR}/hw/aica/aica_rtc.h"
                      "${WASHDC_SOURCE_DIR}/hw/aica/aica_rtc.c"
                      "${WASHDC_SOURCE_DIR}/hw/aica/aica_wave_mem.h"
                      "${WASHDC_SOURCE_DIR}/hw/aica/aica_wave_mem.c"
                      "${WASHDC_SOURCE_DIR}/hw/aica/aica.h"
                      "${WASHDC_SOURCE_DIR}/hw/aica/aica.c"
                      "${WASHDC_SOURCE_DIR}/hw/aica/adpcm.h"
                      "${WASHDC_SOURCE_DIR}/hw/boot_rom.h"
                      "${WASHDC_SOURCE_DIR}/hw/boot_rom.c"
                      "${WASHDC_SOURCE_DIR}/hw/flash_mem.h"
                      "${WASHDC_SOURCE_DIR}/hw/flash_mem.c"
                      "${WASHDC_SOURCE_DIR}/mem_code.h"
                      "${WASHDC_SOURCE_DIR}/memory.h"
                      "${WASHDC_SOURCE_DIR}/memory.c"
                      "${WASHDC_SOURCE_DIR}/include/washdc/MemoryMap.h"
                      "${WASHDC_SOURCE_DIR}/MemoryMap.c"
                      "${WASHDC_SOURCE_DIR}/dreamcast.h"
                      "${WASHDC_SOURCE_DIR}/dreamcast.c"
                      "${WASHDC_SOURCE_DIR}/dc_sched.h"
                      "${WASHDC_SOURCE_DIR}/dc_sched.c"
                      "${WASHDC_SOURCE_DIR}/win/win.c"
                      "${WASHDC_SOURCE_DIR}/include/washdc/win.h"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/framebuffer.c"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/framebuffer.h"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_gfx_obj.c"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_gfx_obj.h"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_yuv.c"
                      "${WASHDC_SOURCE_DIR}/hw/pvr2/pvr2_yuv.h"
                      "${WASHDC_SOURCE_DIR}/include/washdc/gfx/gfx_all.h"
                      "${WASHDC_SOURCE_DIR}/include/washdc/gfx/config.h"
                      "${WASHDC_SOURCE_DIR}/include/washdc/gfx/def.h"
                      "${WASHDC_SOURCE_DIR}/include/washdc/gfx/tex_cache.h"
                      "${WASHDC_SOURCE_DIR}/include/washdc/gfx/obj.h"
                      "${WASHDC_SOURCE_DIR}/gfx/rend_common.h"
                      "${WASHDC_SOURCE_DIR}/gfx/rend_common.c"
                      "${WASHDC_SOURCE_DIR}/gfx/gfx.h"
                      "${WASHDC_SOURCE_DIR}/gfx/gfx.c"
                      "${WASHDC_SOURCE_DIR}/error.c"
                      "${WASHDC_SOURCE_DIR}/include/washdc/error.h"
                      "${WASHDC_SOURCE_DIR}/mem_areas.h"
                      "${WASHDC_SOURCE_DIR}/include/washdc/stringlib.h"
                      "${WASHDC_SOURCE_DIR}/stringlib.c"
                      "${WASHDC_SOURCE_DIR}/include/washdc/fifo.h"
                      "${WASHDC_SOURCE_DIR}/gdi.h"
                      "${WASHDC_SOURCE_DIR}/gdi.c"
                      "${WASHDC_SOURCE_DIR}/cdi.h"
                      "${WASHDC_SOURCE_DIR}/cdi.c"
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
                      "${WASHDC_SOURCE_DIR}/include/washdc/ring.h"
                      "${WASHDC_SOURCE_DIR}/config.h"
                      "${WASHDC_SOURCE_DIR}/config.c"
                      "${WASHDC_SOURCE_DIR}/gfx/gfx_config.h"
                      "${WASHDC_SOURCE_DIR}/gfx/gfx_config.c"
                      "${WASHDC_SOURCE_DIR}/log.h"
                      "${WASHDC_SOURCE_DIR}/include/washdc/log.h"
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
                      "${WASHDC_SOURCE_DIR}/jit/defs.h"
                      "${WASHDC_SOURCE_DIR}/jit/jit.h"
                      "${WASHDC_SOURCE_DIR}/jit/jit.c"
                      "${WASHDC_SOURCE_DIR}/jit/jit_mem.h"
                      "${WASHDC_SOURCE_DIR}/jit/jit_mem.c"
                      "${WASHDC_SOURCE_DIR}/jit/jit_intp/code_block_intp.h"
                      "${WASHDC_SOURCE_DIR}/jit/jit_intp/code_block_intp.c"
                      "${WASHDC_SOURCE_DIR}/jit/jit_disas.h"
                      "${WASHDC_SOURCE_DIR}/jit/jit_disas.c"
                      "${WASHDC_SOURCE_DIR}/jit/optimize.h"
                      "${WASHDC_SOURCE_DIR}/jit/optimize.c"
                      "${WASHDC_SOURCE_DIR}/include/washdc/gfx/gfx_il.h"
                      "${WASHDC_SOURCE_DIR}/avl.h"
                      "${WASHDC_SOURCE_DIR}/hw/arm7/arm7.h"
                      "${WASHDC_SOURCE_DIR}/hw/arm7/arm7.c"
                      "${WASHDC_SOURCE_DIR}/include/washdc/pix_conv.h"
                      "${WASHDC_SOURCE_DIR}/pix_conv.c"
                      "${WASHDC_SOURCE_DIR}/title.h"
                      "${WASHDC_SOURCE_DIR}/title.c"
                      "${WASHDC_SOURCE_DIR}/include/washdc/cpu.h"
                      "${WASHDC_SOURCE_DIR}/include/washdc/hostfile.h"
                      "${WASHDC_SOURCE_DIR}/screenshot.h"
                      "${WASHDC_SOURCE_DIR}/screenshot.c"
                      "${WASHDC_SOURCE_DIR}/washdc.c"
                      "${WASHDC_SOURCE_DIR}/include/washdc/washdc.h"
                      "${WASHDC_SOURCE_DIR}/include/washdc/gameconsole.h"
                      "${WASHDC_SOURCE_DIR}/gameconsole.c"
                      "${WASHDC_SOURCE_DIR}/include/washdc/buildconfig.h"
                      "${WASHDC_SOURCE_DIR}/buildconfig.c"
                      "${WASHDC_SOURCE_DIR}/include/washdc/debugger.h"
                      "${WASHDC_SOURCE_DIR}/include/washdc/serial_server.h"
                      "${WASHDC_SOURCE_DIR}/serial_server.h"
                      "${WASHDC_SOURCE_DIR}/serial_server.c"
                      "${WASHDC_SOURCE_DIR}/include/washdc/sound_intf.h"
                      "${WASHDC_SOURCE_DIR}/sound.h"
                      "${WASHDC_SOURCE_DIR}/sound.c")

if (JIT_PROFILE)
    add_definitions(-DJIT_PROFILE)
    set(libwashdc_sources ${libwashdc_sources} "${WASHDC_SOURCE_DIR}/jit/jit_profile.h"
                                               "${WASHDC_SOURCE_DIR}/jit/jit_profile.c")
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
                                              "${WASHDC_SOURCE_DIR}/jit/x86_64/abi.h"
                                              "${WASHDC_SOURCE_DIR}/jit/x86_64/register_set.h"
                                              "${WASHDC_SOURCE_DIR}/jit/x86_64/register_set.c")
endif()

if (ENABLE_DEBUGGER)
    add_definitions(-DENABLE_DEBUGGER)
    set(libwashdc_sources ${libwashdc_sources} "${WASHDC_SOURCE_DIR}/dbg/debugger.c")

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

target_include_directories(washdc PRIVATE "${include_dirs}" "${WASHDC_SOURCE_DIR}/" "${WASHDC_SOURCE_DIR}/hw/sh4" "${WASHDC_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/src/common")
