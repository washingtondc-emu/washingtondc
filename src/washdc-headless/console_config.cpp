/*******************************************************************************
 *
 * Copyright 2019 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 ******************************************************************************/

#include <string>
#include <cstring>
#include <sys/stat.h>
#include <cerrno>
#include <cstdio>

#include "paths.hpp"

#define HOSTFILE_PATH_LEN 4096

static path_string consoles_dir(void) {
    return path_append(cfg_dir(), "consoles");
}

static void create_consoles_dir(void) {
    create_cfg_dir();
    create_directory(consoles_dir());
}

static path_string console_get_dir(char const *console_name) {
    return path_append(consoles_dir(), console_name);
}

void create_console_dir(char const *console_name) {
    create_consoles_dir();
    create_directory(console_get_dir(console_name));
}

char const* console_get_rtc_path(char const *console_name) {
    path_string path(path_append(console_get_dir(console_name), "rtc.txt"));

    static char c_path[HOSTFILE_PATH_LEN];
    strncpy(c_path, path.c_str(), sizeof(c_path));
    c_path[sizeof(c_path) - 1] = '\0';
    return c_path;
}

char const *console_get_firmware_path(char const *console_name) {
    path_string path(path_append(console_get_dir(console_name), "dc_bios.bin"));

    static char c_path[HOSTFILE_PATH_LEN];
    strncpy(c_path, path.c_str(), sizeof(c_path));
    c_path[sizeof(c_path) - 1] = '\0';
    return c_path;
}

char const *console_get_flashrom_path(char const *console_name) {
    path_string path(path_append(console_get_dir(console_name), "dc_flash.bin"));

    static char c_path[HOSTFILE_PATH_LEN];
    strncpy(c_path, path.c_str(), sizeof(c_path));
    c_path[sizeof(c_path) - 1] = '\0';
    return c_path;
}
