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

enable_testing()

configure_file("regression_tests/sh4div_test.pl" "sh4div_test.pl" COPYONLY)
add_test(NAME sh4div_test COMMAND ./sh4div_test.pl)

configure_file("regression_tests/sh4tmu_test.pl" "sh4tmu_test.pl" COPYONLY)
add_test(NAME sh4tmu_test COMMAND ./sh4tmu_test.pl)

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

# libpng version 1.6.34
set(libpng_path "${CMAKE_SOURCE_DIR}/external/libpng")
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
set(zlib_path "${CMAKE_SOURCE_DIR}/external/zlib")
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
set(glew_path "${CMAKE_SOURCE_DIR}/external/glew")
add_library(glew "${glew_path}/src/glew.c"
                 "${glew_path}/include/GL/glew.h"
                 "${glew_path}/include/GL/wglew.h"
                 "${glew_path}/include/GL/glxew.h")

set(include_dirs "${OPENGL_INCLUDE_DIR}"
                 "${CMAKE_SOURCE_DIR}/external/glew/include"
                 "${CMAKE_SOURCE_DIR}/external"
                 "${CMAKE_SOURCE_DIR}/external/glfw/include"
                 "${libpng_path}"
                 "${zlib_path}"
                 "${PROJECT_SOURCE_DIR}/external/portaudio/include")

if (ENABLE_DEBUGGER)
  set(include_dirs "${include_dirs}"
                   "${PROJECT_SOURCE_DIR}/external/sh4asm"
                   "${PROJECT_SOURCE_DIR}/external/capstone/include")
endif()

add_subdirectory(src)

add_subdirectory("${PROJECT_SOURCE_DIR}/external/glfw")
add_subdirectory(external/portaudio)

if (ENABLE_DEBUGGER)
    add_subdirectory("${PROJECT_SOURCE_DIR}/external/capstone")
endif()
