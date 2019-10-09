/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 ******************************************************************************/

#include "i_hate_windows.h"

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
