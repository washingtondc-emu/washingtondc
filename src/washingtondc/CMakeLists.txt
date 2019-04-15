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

project(WashingtonDC CXX)
set(WashingtonDC_VERSION_MAJOR 0)
set(WashingtonDC_VERSION_MINOR 1)

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

set(washingtondc_sources "${PROJECT_SOURCE_DIR}/main.cpp"
                         "${PROJECT_SOURCE_DIR}/window.cpp"
                         "${PROJECT_SOURCE_DIR}/window.hpp"
                         "${PROJECT_SOURCE_DIR}/control_bind.cpp"
                         "${PROJECT_SOURCE_DIR}/control_bind.hpp")

add_executable(washingtondc ${washingtondc_sources} ${ui_sources} ${imgui_sources})

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
    set(washingtondc_libs "${washingtondc_libs}" "${LIBEVENT_LIB_PATH}/lib/libevent.a")
endif()

if (ENABLE_DEBUGGER)
    set(washingtondc_libs "${washingtondc_libs}" capstone-static)
endif()

target_include_directories(washingtondc PRIVATE "${WASHDC_SOURCE_DIR}/include"
                                                "${CMAKE_SOURCE_DIR}/src/libwashdc/include"
                                                "${PROJECT_SOURCE_DIR}/ui"
                                                "${IMGUI_SOURCE_DIR}")
target_link_libraries(washingtondc "${washingtondc_libs}")
