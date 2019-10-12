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

#ifndef PATHS_HPP_
#define PATHS_HPP_

#ifdef _WIN32
#include "i_hate_windows.h"
#else
#include <sys/stat.h>
#endif

#include <string>
#include <iostream>

#include "washdc/hostfile.h"

/*
 * XXX I want to make this work with wchar_t for win32,
 * but it's too much of a clusterfuck
 */
// #ifdef _WIN32
// #include <cwchar>
// typedef wchar_t path_char;
// typedef std::wstring path_string;
// #else
typedef char path_char;
typedef std::string path_string;
// #endif

static path_string path_append(path_string lhs, path_string rhs) {
#ifdef _WIN32
    static char const pathsep = '\\';
#else
    static char const pathsep = '/';
#endif

    if (!rhs.size())
        return lhs; // nothing to append

    if (!lhs.size())
        return rhs; // lhs is empty so return rhs

    /*
     * If there's a trailing / on dst and a leading / on src then get rid of
     * the leading slash on src.
     *
     * If there is not a trailing / on dst and there is not a leading slash on
     * src then give dst a trailing /.
     */
    if (lhs.back() == pathsep && rhs.front() == pathsep) {
        // remove leading / from rhs
        return lhs + rhs.substr(1);
    } else if (lhs.back() != pathsep && rhs.front() != pathsep) {
        // add trailing pathsep to lhs
        return lhs + pathsep + rhs;
    }
    return lhs + rhs;
}

static void create_directory(path_string name) {
#ifdef _WIN32
    if (!CreateDirectoryA(name.c_str(), NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS)
        std::cerr << __func__ << " - failure to create " << name << std::endl;
#else
    if (mkdir(name.c_str(), S_IRUSR | S_IWUSR | S_IXUSR) != 0 &&
        errno != EEXIST)
        std::cerr << __func__ << " - failure to create " << name << std::endl;
#endif
}

static path_string data_dir(void) {
#ifdef _WIN32
    char appdata_dir[MAX_PATH];
    if (SHGetFolderPathA((HWND)0, CSIDL_LOCAL_APPDATA, NULL,
                         SHGFP_TYPE_CURRENT, appdata_dir) != S_OK)
        return path_string();
    return path_append(appdata_dir, "washdc");
#else
    path_string path;
    char const *data_root = getenv("XDG_DATA_HOME");
    if (data_root) {
        path = data_root;
    } else {
        char const *home_dir = getenv("HOME");
        if (home_dir)
            path = path_append(home_dir, ".local/share");
        else
            return path_string();
    }
    return path_append(path, "washdc");
#endif
}

static path_string cfg_dir(void) {
#ifdef _WIN32
    char appdata_dir[MAX_PATH];
    if (SHGetFolderPathA((HWND)0, CSIDL_LOCAL_APPDATA, NULL,
                         SHGFP_TYPE_CURRENT, appdata_dir) != S_OK)
        return path_string();
    return path_append(appdata_dir, "washdc");
#else
    path_string path;
    char const *config_root = getenv("XDG_CONFIG_HOME");
    if (config_root) {
        path = config_root;
    } else {
        char const *home_dir = getenv("HOME");
        if (home_dir) {
            path = path_append(home_dir, ".config");
        } else {
            return path_string();
        }
    }
    return path_append(path, "washdc");
#endif
}

static path_string cfg_file(void) {
    return path_append(cfg_dir(), "wash.cfg");
}

static path_string screenshot_dir(void) {
    return path_append(data_dir(), "screenshots");
}

static void create_data_dir(void) {
    create_directory(data_dir());
}

static void create_screenshot_dir(void) {
    create_data_dir();
    create_directory(screenshot_dir());
}

static void create_cfg_dir(void) {
    create_directory(cfg_dir());
}

static washdc_hostfile open_cfg_file(enum washdc_hostfile_mode mode) {
    path_string the_cfg_file(cfg_file());
    create_directory(cfg_dir());
    return washdc_hostfile_open(the_cfg_file.c_str(), mode);
}

static washdc_hostfile open_screenshot(char const *name,
                                       enum washdc_hostfile_mode mode) {
#ifdef _WIN32
    static char const pathsep = '\\';
#else
    static char const pathsep = '/';
#endif
    path_string path = screenshot_dir() + pathsep + name;
    return washdc_hostfile_open(path.c_str(), mode);
}

#endif
