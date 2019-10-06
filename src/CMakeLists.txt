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

cmake_minimum_required(VERSION 2.8.4)
include(ExternalProject)

set (LIBEVENT_LIB_PATH ${CMAKE_CURRENT_BINARY_DIR})

if(NOT CMAKE_BUILD_TYPE)
       set(CMAKE_BUILD_TYPE Release)
       message("Setting CMAKE_BUILD_TYPE to Release by default")
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# i need this for strdup and clock_gettime on Debian for some unknown reason
add_definitions(-D_POSIX_C_SOURCE=200809L)

# While I prefer to avoid using GNU-specific stuff when possible, in general
# I don't give a damn about portability to Windows
add_definitions(-D_GNU_SOURCE)

find_package(OpenGL REQUIRED)

# turn on strict warnings - i have no idea how to do this in windows
if(UNIX)
    # the -Wno-format-trunction is there to prevent gcc from raising frivolous
    # warnings whenever there's a chance that snprintf might do the one thing
    # it exists to do.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-format-truncation")
endif()

if (USE_LIBEVENT)
    add_definitions(-DUSE_LIBEVENT)
endif()

add_subdirectory(libwashdc)

if (BUILD_WASHINGTONDC)
    add_subdirectory(washingtondc)
    add_dependencies(washingtondc washdc)
endif()

if (BUILD_WASHDC_HEADLESS)
    add_subdirectory(washdc-headless)
    add_dependencies(washdc-headless washdc)
endif()
