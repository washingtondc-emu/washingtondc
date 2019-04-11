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
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wno-format-truncation")
endif()

if (USE_LIBEVENT)
    add_definitions(-DUSE_LIBEVENT)
    ExternalProject_Add(libevent
                        PREFIX washingtondc-libevent
                        GIT_REPOSITORY https://github.com/libevent/libevent
                        GIT_TAG release-2.1.8-stable
                        INSTALL_DIR ${LIBEVENT_LIB_PATH}
                        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBEVENT_LIB_PATH} -DEVENT__DISABLE_TESTS=On -DEVENT__DISABLE_REGRESS=On -DEVENT__DISABLE_BENCHMARK=On -DEVENT__DISABLE_OPENSSL=On -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                        BUILD_IN_SOURCE 1)
endif()

if (USE_LIBEVENT)
    set(include_dirs "${include_dirs}"
                     "${CMAKE_BINARY_DIR}/washingtondc-libevent/src/libevent/include")
endif()

add_subdirectory(libwashdc)
add_subdirectory(washingtondc)

if (USE_LIBEVENT)
    add_dependencies(washingtondc libevent washdc)
    add_dependencies(washdc libevent)
endif()
