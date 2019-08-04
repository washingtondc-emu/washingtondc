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

#ifndef WASHDC_HOSTFILE_H_
#define WASHDC_HOSTFILE_H_

#include <stddef.h>

struct washdc_hostfile_api {
    char const*(*cfg_dir)(void);
    char const*(*cfg_file)(void);
    char const*(*data_dir)(void);
    char const*(*screenshot_dir)(void);

    /*
     * join two paths together.
     *
     * first parameter: the destination and also the left side of the path
     * second parameter: the right side of the path
     * third parameter: the length of the first parameter.
     */
    void(*path_append)(char *, char const*, size_t);
};

char const *washdc_hostfile_cfg_dir(void);

char const *washdc_hostfile_cfg_file(void);

char const *washdc_hostfile_data_dir(void);

char const *washdc_hostfile_screenshot_dir(void);

void washdc_hostfile_path_append(char *dst, char const *src, size_t dst_sz);

#endif
