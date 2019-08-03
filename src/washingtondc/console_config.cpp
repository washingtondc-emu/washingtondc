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

#include <string>
#include <cstring>
#include <sys/stat.h>
#include <cerrno>
#include <cstdio>

#include "hostfile.hpp"

static char const* consoles_dir(void) {
    static char dir_path[HOSTFILE_PATH_LEN];
    char const *the_cfg_dir = cfg_dir();

    strncpy(dir_path, the_cfg_dir, sizeof(dir_path));
    dir_path[HOSTFILE_PATH_LEN - 1] = '\0';
    path_append(dir_path, "consoles", HOSTFILE_PATH_LEN);

    return dir_path;
}

static void create_consoles_dir(void) {
    create_cfg_dir();

    char const *the_consoles_dir = consoles_dir();
    if (mkdir(the_consoles_dir, S_IRUSR | S_IWUSR | S_IXUSR) != 0 &&
        errno != EEXIST)
        fprintf(stderr, "%s - failure to create %s\n", __func__, the_consoles_dir);
}

char const *console_get_dir(char const *console_name) {
    char const *the_consoles_dir = consoles_dir();
    static char dir_path[HOSTFILE_PATH_LEN];

    strncpy(dir_path, the_consoles_dir, sizeof(dir_path));
    dir_path[HOSTFILE_PATH_LEN - 1] = '\0';
    path_append(dir_path, console_name, HOSTFILE_PATH_LEN);

    return dir_path;
}

void create_console_dir(char const *console_name) {
    create_consoles_dir();

    char const *dir_path = console_get_dir(console_name);

    if (mkdir(dir_path, S_IRUSR | S_IWUSR | S_IXUSR) != 0 && errno != EEXIST)
        fprintf(stderr, "%s - failure to create %s\n", __func__, dir_path);
}

char const *console_get_rtc_path(char const *console_name) {
    char const *dir_path = console_get_dir(console_name);

    static char console_path[HOSTFILE_PATH_LEN];

    strncpy(console_path, dir_path, sizeof(console_path));
    console_path[HOSTFILE_PATH_LEN - 1] = '\0';
    path_append(console_path, "rtc.txt", sizeof(console_path));

    return console_path;
}

char const *console_get_firmware_path(char const *console_name) {
    char const *dir_path = console_get_dir(console_name);

    static char console_path[HOSTFILE_PATH_LEN];

    strncpy(console_path, dir_path, sizeof(console_path));
    console_path[HOSTFILE_PATH_LEN - 1] = '\0';
    path_append(console_path, "dc_bios.bin", sizeof(console_path));

    return console_path;
}

char const *console_get_flashrom_path(char const *console_name) {
    char const *dir_path = console_get_dir(console_name);

    static char console_path[HOSTFILE_PATH_LEN];

    strncpy(console_path, dir_path, sizeof(console_path));
    console_path[HOSTFILE_PATH_LEN - 1] = '\0';
    path_append(console_path, "dc_flash.bin", sizeof(console_path));

    return console_path;
}
