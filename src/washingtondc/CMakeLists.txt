################################################################################
#
#    WashingtonDC Dreamcast Emulator
#    Copyright (C) 2019, 2020, 2022 snickerbockers
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

#set(CMAKE_LEGACY_CYGWIN_WIN32 0) # Remove when CMake >= 2.8.4 is required
cmake_minimum_required(VERSION 3.6)

# for std::filesystem in the overlay.  Otherwise, we would not need this
set(CMAKE_CXX_STANDARD 17)

project(WashingtonDC CXX)

set(WashingtonDC_VERSION_MAJOR 0)
set(WashingtonDC_VERSION_MINOR 1)

find_package(OpenGL REQUIRED)
if (WIN32)
    find_library(WS2_32_LIBRARY ws2_32)
endif()

# XXX COMPATIBILITY HACK
# memory dumps rely upon the imgui filebrowser module
# the imgui filebrowser module relies on C++17 filesystem support
# This is literally the only thing in all of WashingtonDC that relies on
# C++17, so we make it optional.  It is disabled by default.
if (ENABLE_MEM_DUMP_UI)
    set(no_imgui_browser "FALSE")
else()
    set(no_imgui_browser "TRUE")
    add_definitions(-DDISABLE_MEM_DUMP_UI)
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

set(COMMON_SOURCE_DIR "${CMAKE_SOURCE_DIR}/src/common")
set(IO_SOURCE_DIR "${COMMON_SOURCE_DIR}/frontend_io")
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

set(gfxgl3_sources "${PROJECT_SOURCE_DIR}/shader_cache.h"
                   "${PROJECT_SOURCE_DIR}/renderdoc_app.h"
                   "${PROJECT_SOURCE_DIR}/gfxgl3/gfxgl3_output.h"
                   "${PROJECT_SOURCE_DIR}/gfxgl3/gfxgl3_output.c"
                   "${PROJECT_SOURCE_DIR}/gfxgl3/gfxgl3_target.h"
                   "${PROJECT_SOURCE_DIR}/gfxgl3/gfxgl3_target.c"
                   "${PROJECT_SOURCE_DIR}/gfxgl3/gfxgl3_renderer.h"
                   "${PROJECT_SOURCE_DIR}/gfxgl3/gfxgl3_renderer.c"
                   "${PROJECT_SOURCE_DIR}/gfxgl3/tex_cache.h"
                   "${PROJECT_SOURCE_DIR}/gfxgl3/tex_cache.c"
                   "gfxgl3_render_vs.h"
                   "gfxgl3_render_fs.h"
                   "gfxgl3_final_vert.h"
                   "gfxgl3_final_frag.h"
                   "gfxgl3_color_transform_enabled_vs.h"
                   "gfxgl3_color_transform_disabled_vs.h"
                   "gfxgl3_tex_transform_enabled_vs.h"
                   "gfxgl3_tex_transform_disabled_vs.h"

                   # gfxgl3 texture instructions
                   "gfxgl3_tex_inst_disabled_fs.h"
                   "gfxgl3_text_inst_decal_fs.h"
                   "gfxgl3_text_inst_mod_fs.h"
                   "gfxgl3_text_inst_decal_alpha_fs.h"
                   "gfxgl3_text_inst_mod_alpha_fs.h"

                   # gfxgl3 punch-through polygons
                   "gfxgl3_punch_through_test_disabled.h"
                   "gfxgl3_punch_through_test_enabled.h"

                   # gfxgl3 user clip test
                   "gfxgl3_user_clip_inverted_fs.h"
                   "gfxgl3_user_clip_enabled_fs.h"
                   "gfxgl3_user_clip_disabled_fs.h")

macro(glsl_to_cstring infile outfile)
  add_custom_command(OUTPUT ${outfile}
    COMMAND perl -pe "s/^(.*)$/\\\"\\1\\\\n\\\"/" < ${infile} > ${outfile}
    MAIN_DEPENDENCY ${infile}
    COMMENT "generating ${outfile} from ${infile}..."
    VERBATIM)
  set(glsl_files "${glsl_files}" "${infile}")
endmacro()

glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/gfxgl3_render.vert" "gfxgl3_render_vs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/color_transform_enabled.vert"
                "gfxgl3_color_transform_enabled_vs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/color_transform_disabled.vert"
                "gfxgl3_color_transform_disabled_vs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/tex_transform_enabled.vert"
                "gfxgl3_tex_transform_enabled_vs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/tex_transform_disabled.vert"
                "gfxgl3_tex_transform_disabled_vs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/gfxgl3_render.frag" "gfxgl3_render_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/gfxgl3_final.vert" "gfxgl3_final_vert.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/gfxgl3_final.frag" "gfxgl3_final_frag.h")

# gfxgl3 texture instructions
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/gfxgl3_tex_inst_disabled.frag"
                "gfxgl3_tex_inst_disabled_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/gfxgl3_tex_inst_decal.frag"
                "gfxgl3_text_inst_decal_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/gfxgl3_tex_inst_mod.frag"
                "gfxgl3_text_inst_mod_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/gfxgl3_tex_inst_decal_alpha.frag"
                "gfxgl3_text_inst_decal_alpha_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/gfxgl3_tex_inst_mod_alpha.frag"
                "gfxgl3_text_inst_mod_alpha_fs.h")

# gfxgl3 punch-through polygons
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/gfxgl3_punch_through_test_disabled.frag"
                "gfxgl3_punch_through_test_disabled.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/gfxgl3_punch_through_test_enabled.frag"
                "gfxgl3_punch_through_test_enabled.h")

# gfxgl3 user clip test
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/gfxgl3_user_clip_inverted.frag"
                "gfxgl3_user_clip_inverted_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/gfxgl3_user_clip_enabled.frag"
                "gfxgl3_user_clip_enabled_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl3/shaders/gfxgl3_user_clip_disabled.frag"
                "gfxgl3_user_clip_disabled_fs.h")


set(gfxgl4_sources "${PROJECT_SOURCE_DIR}/shader_cache.h"
                   "${PROJECT_SOURCE_DIR}/renderdoc_app.h"
                   "${PROJECT_SOURCE_DIR}/gfxgl4/gfxgl4_output.h"
                   "${PROJECT_SOURCE_DIR}/gfxgl4/gfxgl4_output.c"
                   "${PROJECT_SOURCE_DIR}/gfxgl4/gfxgl4_target.h"
                   "${PROJECT_SOURCE_DIR}/gfxgl4/gfxgl4_target.c"
                   "${PROJECT_SOURCE_DIR}/gfxgl4/gfxgl4_renderer.h"
                   "${PROJECT_SOURCE_DIR}/gfxgl4/gfxgl4_renderer.c"
                   "${PROJECT_SOURCE_DIR}/gfxgl4/tex_cache.h"
                   "${PROJECT_SOURCE_DIR}/gfxgl4/tex_cache.c"
                   "gfxgl4_render_vs.h"
                   "gfxgl4_render_fs.h"
                   "gfxgl4_oit_sort_vs.h"
                   "gfxgl4_oit_sort_fs.h"
                   "gfxgl4_final_frag.h"
                   "gfxgl4_final_vert.h"
                   "gfxgl4_color_transform_enabled_vs.h"
                   "gfxgl4_color_transform_disabled_vs.h"
                   "gfxgl4_tex_transform_enabled_vs.h"
                   "gfxgl4_tex_transform_disabled_vs.h"
                   "gfxgl4_tex_inst_disabled_fs.h"
                   "gfxgl4_tex_inst_decal_fs.h"
                   "gfxgl4_tex_inst_mod_fs.h"
                   "gfxgl4_tex_inst_decal_alpha_fs.h"
                   "gfxgl4_tex_inst_mod_alpha_fs.h"
                   "gfxgl4_user_clip_disabled_fs.h"
                   "gfxgl4_user_clip_enabled_fs.h"
                   "gfxgl4_user_clip_inverted_fs.h"
                   "gfxgl4_oit_disabled_fs.h"
                   "gfxgl4_oit_first_pass_fs.h"
                   "gfxgl4_punch_through_disabled_fs.h"
                   "gfxgl4_punch_through_enabled_fs.h")

glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_render.vert" "gfxgl4_render_vs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_render.frag" "gfxgl4_render_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/oit_sort.vert" "gfxgl4_oit_sort_vs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/oit_sort.frag" "gfxgl4_oit_sort_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/final.vert" "gfxgl4_final_vert.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/final.frag" "gfxgl4_final_frag.h")

# gfxgl4 vertex color transform
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_color_transform_enabled.vert"
                "gfxgl4_color_transform_enabled_vs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_color_transform_disabled.vert"
                "gfxgl4_color_transform_disabled_vs.h")

# gfxgl4 vertex texture transform
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_tex_transform_enabled.vert"
                "gfxgl4_tex_transform_enabled_vs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_tex_transform_disabled.vert"
                "gfxgl4_tex_transform_disabled_vs.h")

# gfxgl4 fragment texture instruction
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_tex_inst_disabled.frag"
                "gfxgl4_tex_inst_disabled_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_tex_inst_decal.frag"
                "gfxgl4_tex_inst_decal_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_tex_inst_mod.frag"
                "gfxgl4_tex_inst_mod_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_tex_inst_decal_alpha.frag"
                "gfxgl4_tex_inst_decal_alpha_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_tex_inst_mod_alpha.frag"
                "gfxgl4_tex_inst_mod_alpha_fs.h")

#gfxgl4 user clip test
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_user_clip_disabled.frag"
                "gfxgl4_user_clip_disabled_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_user_clip_enabled.frag"
                "gfxgl4_user_clip_enabled_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_user_clip_inverted.frag"
                "gfxgl4_user_clip_inverted_fs.h")

# gfxgl4 OIT first pass
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_oit_disabled.frag"
                "gfxgl4_oit_disabled_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_oit_first_pass.frag"
                "gfxgl4_oit_first_pass_fs.h")

# gfxgl4 punch-through test
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_punch_through_disabled.frag"
                "gfxgl4_punch_through_disabled_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/gfxgl4/shaders/gfxgl4_punch_through_enabled.frag"
                "gfxgl4_punch_through_enabled_fs.h")


set(soft_gfx_sources "${PROJECT_SOURCE_DIR}/soft_gfx/soft_gfx.c"
                     "${PROJECT_SOURCE_DIR}/soft_gfx/soft_gfx.h"
                     "soft_gfx_final_fs.h"
                     "soft_gfx_final_vs.h")

glsl_to_cstring("${PROJECT_SOURCE_DIR}/soft_gfx/shaders/final.frag"
                "soft_gfx_final_fs.h")
glsl_to_cstring("${PROJECT_SOURCE_DIR}/soft_gfx/shaders/final.vert"
                "soft_gfx_final_vs.h")

add_custom_target(validate-glsl COMMAND ${CMAKE_COMMAND} -E env -- glslangValidator ${glsl_files})

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
                         "${PROJECT_SOURCE_DIR}/rend_if.hpp"
                         "${PROJECT_SOURCE_DIR}/gfx_obj.h"
                         "${PROJECT_SOURCE_DIR}/gfx_obj.c"
                         "${PROJECT_SOURCE_DIR}/shader.h"
                         "${PROJECT_SOURCE_DIR}/shader.c"
                         "${PROJECT_SOURCE_DIR}/renderer.h"
                         "${PROJECT_SOURCE_DIR}/config_file.h"
                         "${PROJECT_SOURCE_DIR}/config_file.c"
                         "${PROJECT_SOURCE_DIR}/sdl_gamecontrollerdb.h"
                         "${COMMON_SOURCE_DIR}/washdc_getopt.c"
                         "${gfxgl3_sources}"
                         "${gfxgl4_sources}"
                         "${soft_gfx_sources}")

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
install(TARGETS washingtondc RUNTIME DESTINATION bin)

add_definitions(-DGLEW_STATIC)

set(vid_libs "glfw" "glew" "${OPENGL_gl_LIBRARY}")

if (NOT WIN32)
    set(vid_libs "rt" "${vid_libs}")
endif()

set(washingtondc_libs "washdc"
                      "${vid_libs}"
                      "portaudio_static"
                      "${CMAKE_DL_LIBS}")

if (NOT WIN32)
   set (washingtondc_libs "${washingtondc_libs}" "m")

   if (no_imgui_browser STREQUAL "FALSE")
       set(washingtondc_libs "${washingtondc_libs}" "stdc++fs")
   endif()
endif()

if (USE_LIBEVENT)
    set(washingtondc_libs "${washingtondc_libs}" "event_core")
    if (WIN32)
        target_link_libraries(event_core INTERFACE ws2_32)
    else()
        set(washingtondc_libs "${washingtondc_libs}" "pthread" "event_pthreads")
    endif()
endif()

if (need_capstone STREQUAL "TRUE")
    set(washingtondc_libs "${washingtondc_libs}" capstone-static)
endif()

set(washingtondc_libs "${washingtondc_libs}" chdr-static)

target_include_directories(washingtondc PRIVATE "${WASHDC_SOURCE_DIR}/include"
                                                "${CMAKE_SOURCE_DIR}/src/common"
                                                "${CMAKE_SOURCE_DIR}/src/libwashdc/include"
                                                "${PROJECT_SOURCE_DIR}/ui"
                                                "${IMGUI_SOURCE_DIR}"
                                                "${CMAKE_SOURCE_DIR}/external/sh4asm"
                                                "${CMAKE_SOURCE_DIR}/external/capstone/include"
                                                "${CMAKE_SOURCE_DIR}/external/glew/include"
                                                "${CMAKE_SOURCE_DIR}/external/libevent/include"
                                                "${CMAKE_BINARY_DIR}/external/libevent/include"
                                                "${CMAKE_SOURCE_DIR}/external/imgui-filebrowser"
                                                "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(washingtondc "${washingtondc_libs}")
