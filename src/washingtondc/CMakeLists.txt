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
set(ui_sources "${UI_SOURCE_DIR}/font/digit_0.hpp"
               "${UI_SOURCE_DIR}/font/digit_1.hpp"
               "${UI_SOURCE_DIR}/font/digit_2.hpp"
               "${UI_SOURCE_DIR}/font/digit_3.hpp"
               "${UI_SOURCE_DIR}/font/digit_4.hpp"
               "${UI_SOURCE_DIR}/font/digit_5.hpp"
               "${UI_SOURCE_DIR}/font/digit_6.hpp"
               "${UI_SOURCE_DIR}/font/digit_7.hpp"
               "${UI_SOURCE_DIR}/font/digit_8.hpp"
               "${UI_SOURCE_DIR}/font/digit_9.hpp"
               "${UI_SOURCE_DIR}/font/dot.hpp"
               "${UI_SOURCE_DIR}/font/slash.hpp"
               "${UI_SOURCE_DIR}/font/space.hpp"
               "${UI_SOURCE_DIR}/font/font.hpp"
               "${UI_SOURCE_DIR}/font/font.cpp"
               "${UI_SOURCE_DIR}/overlay.hpp"
               "${UI_SOURCE_DIR}/overlay.cpp")


set(washingtondc_sources "${PROJECT_SOURCE_DIR}/main.cpp"
                         "${PROJECT_SOURCE_DIR}/window.cpp"
                         "${PROJECT_SOURCE_DIR}/window.hpp"
                         "${PROJECT_SOURCE_DIR}/control_bind.cpp"
                         "${PROJECT_SOURCE_DIR}/control_bind.hpp")

add_executable(washingtondc ${washingtondc_sources} ${ui_sources})

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

target_include_directories(washingtondc PRIVATE "${WASHDC_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/src/libwashdc/include" "${PROJECT_SOURCE_DIR}/ui")
target_link_libraries(washingtondc "${washingtondc_libs}")
