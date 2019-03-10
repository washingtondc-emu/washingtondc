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
#include <sys/stat.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

#include "hostfile.h"

#define CFG_FILE_NAME "wash.cfg"

#define CFG_PATH_LEN 4096

static void path_append(char *dst, char const *src, size_t dst_sz) {
    if (!src[0])
        return; // nothing to append

    // get the index of the null terminator
    unsigned zero_idx = 0;
    while (dst[zero_idx])
        zero_idx++;

    if (!zero_idx) {
        // special case - dst is empty so copy src over
        strncpy(dst, src, dst_sz);
        dst[dst_sz - 1] = '\0';
        return;
    }

    /*
     * If there's a trailing / on dst and a leading / on src then get rid of
     * the leading slash on src.
     *
     * If there is not a trailing / on dst and there is not a leading slash on
     * src then give dst a trailing /.
     */
    if (dst[zero_idx - 1] == '/' && src[0] == '/') {
        // remove leading / from src
        src = src + 1;
        if (!src[0])
            return; // nothing to append
    } else if (dst[zero_idx - 1] != '/' && src[0] != '/') {
        // add trailing / to dst
        if (zero_idx < dst_sz - 1) {
            dst[zero_idx++] = '/';
            dst[zero_idx] = '\0';
        } else {
            return; // out of space
        }
    }

    // there's no more space
    if (zero_idx >= dst_sz -1 )
        return;

    strncpy(dst + zero_idx, src, dst_sz - zero_idx);
    dst[dst_sz - 1] = '\0';
}

char const *hostfile_cfg_dir(void) {
    static char path[CFG_PATH_LEN];
    char const *config_root = getenv("XDG_CONFIG_HOME");
    if (config_root) {
        strncpy(path, config_root, CFG_PATH_LEN);
        path[CFG_PATH_LEN - 1] = '\0';
    } else {
        char const *home_dir = getenv("HOME");
        if (home_dir) {
            strncpy(path, home_dir, CFG_PATH_LEN);
            path[CFG_PATH_LEN - 1] = '\0';
        } else {
            return NULL;
        }
        path_append(path, "/.config", CFG_PATH_LEN);
    }
    path_append(path, "washdc", CFG_PATH_LEN);
    return path;
}

char const *hostfile_cfg_file(void) {
    static char path[CFG_PATH_LEN];
    char const *cfg_dir = hostfile_cfg_dir();
    if (!cfg_dir)
        return NULL;
    strncpy(path, cfg_dir, CFG_PATH_LEN);
    path[CFG_PATH_LEN - 1] = '\0';
    path_append(path, "wash.cfg", CFG_PATH_LEN);
    return path;
}
