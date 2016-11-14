################################################################################
#
#
#    WashingtonDC Dreamcast Emulator
#    Copyright (C) 2016 snickerbockers
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

cmake_minimum_required(VERSION 2.6)
project(WashingtonDC)
set(WashingtonDC_VERSION_MAJOR 0)
set(WashingtonDC_VERSION_MINOR 1)

include_directories("${PROJECT_SOURCE_DIR}/src/"
                    "${PROJECT_SOURCE_DIR}/src/hw/sh4"
                    "${PROJECT_SOURCE_DIR}/src/common"
                    "${PROJECT_SOURCE_DIR}/external")

add_library(sh4asm "${PROJECT_SOURCE_DIR}/src/tool/sh4asm/Inst.cpp"
                   "${PROJECT_SOURCE_DIR}/src/tool/sh4asm/main.cpp"
                   "${PROJECT_SOURCE_DIR}/src/tool/sh4asm/sh4asm.cpp")

add_library(sh4 "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/sh4_inst.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/Icache.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/Ocache.cpp"
                "${PROJECT_SOURCE_DIR}/src/hw/sh4/Memory.cpp")

add_executable(sh4asm_test "${PROJECT_SOURCE_DIR}/unit_tests/sh4asm_test.cpp")
target_link_libraries(sh4asm_test sh4asm)

add_executable(sh4_test "${PROJECT_SOURCE_DIR}/unit_tests/sh4_test.cpp")
target_link_libraries(sh4_test sh4)
